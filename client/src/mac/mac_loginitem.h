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
#line HEADER_FILE("mac/mac_loginitem.h")

#ifndef MAC_LOGINITEM_H
#define MAC_LOGINITEM_H

// These functions control whether the client launches at login on OS X.
// If the operation can't be completed, these will throw an Error.

// Test if the client is currently configured to launch at login.
bool macLaunchAtLogin();

// Enable or disable launch at startup.
void macSetLaunchAtLogin(bool enabled);

#endif
