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

import QtQuick 2.11
import PIA.NativeAcc 1.0 as NativeAcc
import "../core"
import "qrc:/javascript/util.js" as Util

// StaticHtml is just a Text that contains basic HTML markup, which is stripped
// from the accessibility annotation.
//
// Like ValueHtml, it is separate from StaticText because the HTML stripping is
// probably pretty expensive.
Text {
  NativeAcc.Text.name: Util.stripHtml(text)
}
