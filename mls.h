/**
  mls.h - Mozilla Location Services

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

#ifndef MLS_H
#define MLS_H

#define MAXNETS 32

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include "config.h"

// Define GeoLocation server
#define GEO_SERVER    "location.services.mozilla.com"
#define GEO_PORT      443
#define GEO_POST      "POST /v1/geolocate?key=" GEO_APIKEY " HTTP/1.1"

const char geoServer[]        = GEO_SERVER;
const int  geoPort            = GEO_PORT;
const char geoPOST[] PROGMEM  = GEO_POST;
const char eol[]     PROGMEM  = "\r\n";

struct geo_t {
  float         latitude;
  float         longitude;
  bool          valid;
  unsigned long uptm;
};

class MLS {
  public:
    MLS();
    void  init();
    int   wifiScan(bool sort = false);
    int   geoLocation();
    long  getMovement();
    float getDistance(float lat1, float long1, float lat2, float long2);
    int   getBearing(float lat1, float long1, float lat2, float long2);
    const char* getCardinal(int course);
    void  getLocator(float lat, float lng);
    geo_t current;
    geo_t previous;
    float distance;
    float speed;
    int   knots;
    int   bearing;
    char  locator[7];
  private:
    struct  BSSID_RSSI {
      uint8_t bssid[WL_MAC_ADDR_LENGTH];
      int8_t  rssi;
    } nets[MAXNETS];
    int           netCount;
};

#endif /* MLS_H */
