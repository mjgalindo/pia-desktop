// Copyright (c) 2022 Private Internet Access, Inc.
//
// This file is part of the Private Internet Access Desktop Client.
//
// The Private Internet Access Desktop Client is free software: you can
// redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.
//
// The Private Internet Access Desktop Client is distributed in the hope that
// it will be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the Private Internet Access Desktop Client.  If not, see
// <https://www.gnu.org/licenses/>.

#include "common.h"
#line SOURCE_FILE("posix_ping.cpp")

#include "posix_ping.h"
#include <QRandomGenerator>
#include <QTimer>
#include <QHostAddress>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <fcntl.h>

PosixPing::PosixPing()
    : _identifier{static_cast<quint16>(QRandomGenerator::global()->bounded(std::numeric_limits<quint16>::max()))},
      _nextSequence{0}
{
    // Unit tests don't run as root, so we can't actually do the ICMP pings.  We
    // still want to test the bulk of LatencyTracker, so mimic the pings just by
    // triggering phony measurements in unit tests.
#ifndef UNIT_TEST
    _icmpSocket = PosixFd{::socket(PF_INET, SOCK_RAW, IPPROTO_ICMP)};
    if(!_icmpSocket)
    {
        qWarning() << "Failed to open ICMP socket:" << errno;
    }

    int val = 1;
    if (setsockopt(_icmpSocket.get(), IPPROTO_IP,
                   IP_HDRINCL, &val, sizeof(val)) < 0) {
      qWarning() << "Failed to set IP_HDRINCL flag on ICMP socket";
    }

    // apply NONBLOCK flag
    int oldFlags = ::fcntl(_icmpSocket.get(), F_GETFL);
    ::fcntl(_icmpSocket.get(), F_SETFL, oldFlags | O_NONBLOCK);

    _pReadNotifier.emplace(_icmpSocket.get(), QSocketNotifier::Type::Read);
    connect(_pReadNotifier.ptr(), &QSocketNotifier::activated, this,
            &PosixPing::onReadyRead);
#endif
}

quint16 PosixPing::calcChecksum(const quint8 *data, std::size_t len) const
{
    const quint16 *words = reinterpret_cast<const quint16*>(data);
    const quint16 *wordsEnd = words + (len/2);
    // Add into a 32-bit accumulator, then fold the carries in.
    quint32 accum{0};
    while(words != wordsEnd)
    {
        accum += *words;
        ++words;
    }
    // Handle trailing odd byte
    if(len % 2)
        accum += data[len-1];

    // Fold in carry
    accum = (accum & 0xFFFF) + (accum >> 16);
    // Do it again in case that carried also
    accum = (accum & 0xFFFF) + (accum >> 16);
    // Take one's complement of low 16 bits
    return ~static_cast<quint16>(accum);
}

bool PosixPing::sendEchoRequest(quint32 address, int payloadSize, bool allowFragment)
{
#ifdef UNIT_TEST
    // Fake this in unit tests since we can't send real ICMP pings when not run
    // as root.
    // Unit tests use the IPv4 documentation range to test a lack of response,
    // so check for that too (but act like a request was sent with no reply).
    if((address & 0xFFFFFF00) != 0xC0000200)    // 192.0.2.0/24
    {
        qInfo() << "Mocking ping to" << QHostAddress{address};
        QTimer::singleShot(30, this, [this, address]{emit receivedReply(address);});
    }
    return true;
#endif
    if(!_icmpSocket)
        return false; // Can't do anything, failed to open raw socket - traced earlier

    // Build an ICMP echo request packet.
    int rawPacketSize = sizeof(IcmpEcho) + payloadSize + sizeof(struct ip);
    int packetSize = sizeof(IcmpEcho) + payloadSize;
    std::vector<std::uint8_t> rawPacket;
    rawPacket.resize(rawPacketSize);
    quint8* pRawPacket = rawPacket.data();
    quint8* packet = pRawPacket + sizeof(struct ip);
    IcmpEcho *pEcho = reinterpret_cast<IcmpEcho*>(packet);
    struct ip *ip = reinterpret_cast<struct ip *>(pRawPacket);

    ip->ip_src.s_addr = 0;
    ip->ip_v = 4;
    ip->ip_hl = sizeof *ip >> 2;
    ip->ip_tos = 0;
    ip->ip_len = rawPacketSize;
    // The IPv4 ID is only used for tracking fragmented datagrams.  We disable
    // fragmentation most of the time, but since we've already picked a random
    // identifier anyway for the ICMP header, use that.
    ip->ip_id = htons(_identifier);
    ip->ip_off = htons(0);
    ip->ip_ttl = 255;
    ip->ip_p = 1;
    ip->ip_sum = 0;
    ip->ip_dst.s_addr = htonl(address);

    pEcho->type = 8;
    pEcho->code = 0;
    pEcho->checksum = 0;
    pEcho->identifier = htons(_identifier);
    pEcho->sequence = htons(_nextSequence);

    ++_nextSequence;
    // The default payload on Mac/Linux is 56 bytes from 0x00 - 0x37.  The first
    // few bytes are replaced with a timestamp.
    for(int i = 0; i < payloadSize; ++i)
    {
        packet[sizeof(IcmpEcho)+i] = i;
    }

    timeval now{};
    gettimeofday(&now, nullptr);

    // The timestamp is written with 32-bit fields on Mac / 64-bit fields on Linux
#ifdef Q_OS_MAC
    using TimestampField = quint32;
#else
    using TimestampField = quint64;
#endif

    TimestampField *pWritePos = reinterpret_cast<TimestampField*>(&packet[sizeof(IcmpEcho)]);
    *pWritePos = static_cast<TimestampField>(now.tv_sec);
    ++pWritePos;
    *pWritePos = static_cast<TimestampField>(now.tv_usec);

    // Compute the checksum.  Add into a 32-bit accumulator, then fold the
    // carries in.
    pEcho->checksum = calcChecksum(packet, packetSize);

    if (!allowFragment) {
#if defined(Q_OS_MAC)
        ip->ip_off = IP_DF;
#else
        ip->ip_off = htons(IP_DF);
        int val = IP_PMTUDISC_DO;
        int err = setsockopt(_icmpSocket.get(), IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
        if (err) {
            qWarning() << "Failed to set DF flag on ICMP socket";
            return false;
        }
#endif
    }

    // Write the packet
    sockaddr_in to;
    to.sin_family = AF_INET;
    to.sin_port = 0;    // Not used for ICMP raw socket
    to.sin_addr.s_addr = htonl(address);
    auto sent = ::sendto(_icmpSocket.get(), pRawPacket, rawPacketSize, 0,
                         reinterpret_cast<sockaddr*>(&to), sizeof(to));
    if(sent < 0)
    {
        if(errno == EWOULDBLOCK)
        {
            qWarning() << "Failed to ping" << QHostAddress{address}.toString()
                << "- would have blocked";
        }
        else
        {
            qWarning() << "Failed to ping" << QHostAddress{address}.toString()
                << "-" << errno;
        }
        return false;
    }
    else if(sent != rawPacketSize)
    {
        qWarning() << "Only sent" << sent << "/" << rawPacketSize
            << "bytes in ping to" << QHostAddress{address}.toString();
        return false;
    }

    return true;
}

void PosixPing::onReadyRead()
{
    struct Ipv4
    {
        quint8 version_ihl;    // Version and IP header length
        quint8 dscp_ecn; // DSCP and ECN
        quint16 len;
        quint16 id;
        quint16 flags_foffset; // Flags and fragment offset
        quint8 ttl;
        quint8 protocol;
        quint16 checksum;
        quint32 src;
        quint32 dest;
    };

    alignas(Ipv4) std::array<quint8, 2048> packet;
    auto read = recv(_icmpSocket.get(), packet.data(), packet.size(), 0);
    if(read < 0)
    {
        // Shouldn't happen, socket said it was ready
        qWarning() << "Failed to read from ICMP socket -" << read << "- err:"
            << errno;
        return;
    }

    if(static_cast<std::size_t>(read) < sizeof(Ipv4))
    {
        qWarning() << "Read incomplete packet of" << read << "bytes, expected"
            << sizeof(Ipv4) << "bytes";
        return;
    }

    const Ipv4 *pIpHdr = reinterpret_cast<const Ipv4*>(packet.data());

    // Ignore the packet length from the IP header - the kernel has already
    // manipulated it (converted to host byte order and subtracted header
    // length).  'read' tells us how long the packet is.

    if((pIpHdr->version_ihl >> 4) != 4)
    {
        qWarning() << "Invalid IPv4 version:" << (pIpHdr->version_ihl >> 4);
        return;
    }

    std::size_t headerBytes = (pIpHdr->version_ihl & 0x0F) * 4;
    if(headerBytes < 20 || static_cast<std::size_t>(read) < headerBytes ||
       read - headerBytes < sizeof(IcmpEcho))
    {
        qWarning() << "Invalid IP header length:" << headerBytes
            << "bytes (read" << read << "bytes)";
        return;
    }

    // Should be ICMP - this is an ICMP socket
    if(pIpHdr->protocol != 1)
    {
        qWarning() << "Received non-ICMP packet with protocol" << pIpHdr->protocol;
        return;
    }

    // Check ICMP checksum
    if(calcChecksum(packet.data() + headerBytes, read - headerBytes))
    {
        qWarning() << "Received corrupt ICMP packet from"
            << QHostAddress{ntohl(pIpHdr->src)}.toString();
    }

    // Find the ICMP header
    const IcmpEcho *pEchoReply = reinterpret_cast<const IcmpEcho*>(packet.data() + headerBytes);
    // If it's not an echo reply, not ours, etc., just ignore it.
    if(pEchoReply->type != 0 || pEchoReply->code != 0 ||
       ntohs(pEchoReply->identifier) != _identifier)
    {
        // Don't trace, this will probably happen a lot.
        return;
    }

    // It's our reply - emit the response.
    emit receivedReply(ntohl(pIpHdr->src));
}
