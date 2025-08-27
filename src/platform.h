/**
  platform.h - Platform-specific definitions and compatibility layer
         
  This header provides compatibility between ESP8266 and ESP32 platforms
  by defining platform-specific constants and functions.

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

#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef ESP32
  // ESP32 specific definitions
  #define PWMRANGE 255
  #define WL_MAC_ADDR_LENGTH 6
  #define WL_SSID_MAX_LENGTH 32
  #define WL_WPA_KEY_MAX_LENGTH 64
  #define WL_NETWORKS_LIST_MAXNUM 16
  
  // ESP32 doesn't have getVcc(), return a default 3.3V value
  #define ESP_getVcc() (3300)
  
  // Compatibility macros for PROGMEM
  #define PROGMEM
  #define PSTR(s) (s)
  #define PROGMEM_read_byte(addr) (*(const unsigned char *)(addr))
  #define PROGMEM_read_word(addr) (*(const unsigned short *)(addr))
  #define PROGMEM_read_dword(addr) (*(const unsigned long *)(addr))
  
#else
  // ESP8266 specific definitions
  #include <ESP8266WiFi.h>
  #define ESP_getVcc() ESP.getVcc()
  
#endif

#endif /* PLATFORM_H */
