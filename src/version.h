/**
  version.h - Software version and device identification constants
         
  This header file defines the software version and device identification
  constants used throughout the WiPS project.

  Copyright (c) 2017-2025 Costin STROIE <costinstroie@eridu.eu.org>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef VERSION_H
#define VERSION_H

#include "Arduino.h"

// Device identification constants
const char NODENAME[] = "WiPS";     ///< Device name for network services
const char nodename[] = "wips";     ///< Lowercase device name
const char VERSION[]  = "0.4.1";   ///< Software version string

#endif /* VERSION_H */
