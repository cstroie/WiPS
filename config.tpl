/**
  config.h - User settings

  Copyright (c) 2017-2018 Costin STROIE <costinstroie@eridu.eu.org>

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

#ifndef CONFIG_H
#define CONFIG_H

// Static auth
#define WIFI_RS       ";"
#define WIFI_FS       ","
#define WIFI_SSIDPASS "ssid1,psk1;ssid2,psk2"

// Geolocation
#define GEO_APIKEY    "USE_YOUR_GOOGLE_KEY"
#define GEO_MAXACC    250
#define GEO_MINACC    50

// APRS settings
#define APRS_SERVER   "cbaprs.de"
#define APRS_PORT     27235

// NTP
#define NTP_SERVER    "europe.pool.ntp.org"

// OTA
// IMPORTANT: Set a strong password (minimum 8 characters) for OTA updates
// #define OTA_PASS      "YOUR_STRONG_PASSWORD_HERE"

#endif /* CONFIG_H */
