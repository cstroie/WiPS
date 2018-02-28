/**
  mls.h - Mozilla Location Services

  Copyright (c) 2017-2018 Costin STROIE <costinstroie@eridu.eu.org>

  This file is part of WiFi_APRS_Tracker.
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
    geo_t current;
    geo_t previous;
    float distance;
    float speed;
    int   knots;
    int   bearing;
  private:
    struct  BSSID_RSSI {
      uint8_t bssid[WL_MAC_ADDR_LENGTH];
      int8_t  rssi;
    } nets[MAXNETS];
    int           netCount;
};

#endif /* MLS_H */
