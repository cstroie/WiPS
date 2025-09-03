/**
  geo-wiggle.h - WiGLE Geolocation
         
  This library provides WiFi-based geolocation functionality using
  WiGLE.net API.
  
  The geolocation process involves:
  1. Scanning nearby WiFi access points and collecting their BSSID and RSSI
  2. Sending this data to WiGLE.net geolocation service
  3. Receiving latitude, longitude, and accuracy information
  4. Calculating movement, distance, speed, and bearing between locations
  5. Converting coordinates to Maidenhead locator and cardinal directions

  Features:
  - WiFi AP scanning with BSSID and RSSI collection
  - HTTPS geolocation using WiGLE.net API
  - Distance and bearing calculations using spherical geometry
  - Maidenhead grid locator conversion
  - Cardinal direction mapping
  - Movement detection and speed calculation

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

#ifndef WIGGLE_H
#define WIGGLE_H

#include "Arduino.h"
#ifdef ESP32
  #include <WiFi.h>
  #include <HTTPClient.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>
#endif
#include <WiFiClientSecure.h>
#include "geo.h"
#include "config.h"

// Define GeoLocation server
#define GEO_SERVER    "api.wigle.net"
#define GEO_PORT      443
#define GEO_PATH      "GET /api/v2/network/search HTTP/1.1"

const char geoServer[]        = GEO_SERVER;
const int  geoPort            = GEO_PORT;
#ifdef ESP32
  const char geoPATH[] = GEO_PATH;
#else
  const char geoPATH[] PROGMEM  = GEO_PATH;
#endif

/**
  Geolocation class
  
  Provides methods for WiFi scanning, geolocation, movement tracking,
  and coordinate conversions.
*/
class WIGGLE {
  public:
    /**
      Constructor - Initialize WIGGLE object with default values
    */
    WIGGLE();
    
    /**
      Initialize WIGGLE - currently empty but reserved for future use
    */
    void  init();
    
    /**
      Perform geolocation using collected WiFi data
      
      Sends BSSID/RSSI data to geolocation service and parses response
      to extract latitude, longitude, and accuracy.
      
      @return Accuracy in meters, or -1 on error
    */
    int   geoLocation(geo_t* loc, nets_t* nets, int netCount);
    
  private:
};

#endif /* WIGGLE_H */
