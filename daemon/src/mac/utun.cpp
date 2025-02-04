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
#line SOURCE_FILE("mac/utun.cpp")

#include <net/if_utun.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sys_domain.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <libproc.h>

#include "mac/utun.h"

UTun::UTun(int unitNumber)
{
    sockaddr_ctl sc{};
    ctl_info ctlInfo{};

    // Request a utun device
    ::strlcpy(ctlInfo.ctl_name, UTUN_CONTROL_NAME, sizeof(ctlInfo.ctl_name));

    _sock = PosixFd{::socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL)};
    if(!_sock)
    {
        qWarning() << "Unable to open system socket for utun device:" << ErrnoTracer{errno};
        return;
    }

    // Prevent the socket being inherited by child processes
    if(::fcntl(_sock.get(), F_SETFD, FD_CLOEXEC))
        qWarning() << "fcntl failed setting FD_CLOEXEC:" << ErrnoTracer{errno};

    // Convert Kernel control name to id
    // This updates the ctlInfo.ctl_id field
    if(::ioctl(_sock.get(), CTLIOCGINFO, &ctlInfo) == -1)
    {
        qWarning() << "Unable to get system socket info for utun device:" << ErrnoTracer{errno};
        return;
    }

    // Configure the tunnel device
    sc.sc_len = sizeof(sc);
    sc.sc_id = ctlInfo.ctl_id;
    sc.sc_family = AF_SYSTEM;
    sc.ss_sysaddr = AF_SYS_CONTROL;
    sc.sc_unit = unitNumber;         // utun<unitNumber>

    // Setup && configure the utun
    if(::connect(_sock.get(), reinterpret_cast<sockaddr*>(&sc), sizeof(sc)) < 0)
        return;

    _unitNumber = unitNumber;
    _name = QStringLiteral("utun%1").arg(_unitNumber - 1);
}

int UTun::mtu() const
{
    ifreq ifr{};
    ::strlcpy(ifr.ifr_name, qPrintable(name()), sizeof(ifr.ifr_name));

    // SIOCSIFMTU == Socket IO Control Get InterFace MTU
    if(!::ioctl(_sock.get(), SIOCGIFMTU, &ifr))
        return ifr.ifr_mtu;

    return 0;
}

int UTun::setMtu(int mtu)
{
    ifreq ifr{};
    ifr.ifr_mtu = mtu;
    ::strlcpy(ifr.ifr_name, qPrintable(name()), sizeof(ifr.ifr_name));

    // SIOCSIFMTU == Socket IO Control Set InterFace MTU
    if(!::ioctl(_sock.get(), SIOCSIFMTU, &ifr))
        return mtu;

    return 0;
}

nullable_t<UTun> UTun::create()
{
    // Start at utun7 and go up to utun33 (25 + 8)
    enum : int { BaseNumber = 8, MaxUnits = 25 };

    // Attempt to find a usable unit number (i.e utunN)
    for(int i = 0; i < MaxUnits; ++i)
    {
        int unitNumber = BaseNumber + i;

        UTun utun{unitNumber};
        if(utun.valid())
            return utun;
    }

    qWarning() << "Could not open utun device. Tried up to unit number:" << BaseNumber + MaxUnits - 1;
    return {};
}
