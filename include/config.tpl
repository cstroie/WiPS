/**
  config.h - User configuration template

  This file contains all user-configurable settings for the WiFi APRS Tracker.
  To use this file, copy it to "config.h" and modify the settings as needed.

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

#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
// Uncomment and set your WiFi credentials for static configuration
// Format: "ssid1,pass1;ssid2,pass2;..."
//#define WIFI_SSIDPASS "your_ssid,your_password"
#define WIFI_RS       ";"  // Record separator for WiFi credentials
#define WIFI_FS       ","  // Field separator for WiFi credentials

// Geolocation Settings
// Get your WiGLE API key from https://wigle.net/
// Format: "base64encoded_username:password"
//#define GEO_APIKEY    "USE_YOUR_WIGLE_KEY"
#define GEO_MAXACC    250  // Maximum acceptable accuracy in meters
#define GEO_MINACC    50   // Minimum acceptable accuracy in meters

// APRS-IS Configuration
#define APRS_SERVER   "cbaprs.de"  // APRS-IS server
#define APRS_PORT     27235        // APRS-IS port (27235 for filtered, 14580 for full feed)

// NTP Configuration
#define NTP_SERVER    "europe.pool.ntp.org"  // NTP server for time synchronization

// Over-The-Air (OTA) Updates
// IMPORTANT: Set a strong password (minimum 8 characters) for secure OTA updates
// Uncomment the line below and set your password
//#define OTA_PASS      "YOUR_STRONG_PASSWORD_HERE"

// ESP32 Specific Configuration
#ifdef ESP32
  // Uncomment to enable ESP32 specific features
  //#define ESP32_BLE_SCANNER  // Enable BLE scanning for additional location sources
  //#define ESP32_WIFI_POWER_MANAGEMENT  // Enable WiFi power management
#endif

#endif /* CONFIG_H */
