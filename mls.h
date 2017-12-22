/**
  mls.h - Mozilla Location Services

  Copyright (c) 2017 Costin STROIE <costinstroie@eridu.eu.org>

  This file is part of WiFi_APRS_Tracker.
*/

#ifndef MLS_H
#define MLS_H

#define MAXNETS 32

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include "config.h"

const char geoServer[]        = "location.services.mozilla.com";
const int  geoPort            = 443;
const char geoPOST[] PROGMEM  = "POST /v1/geolocate?key=" GEO_APIKEY " HTTP/1.1";
const char eol[]     PROGMEM  = "\r\n";


class MLS {
  public:
    MLS();
    void  init();
    int   wifiScan(bool sort = false);
    int   geoLocation();
    long  getMovement();
    float latitude;
    float longitude;
    float distance;
    float speed;
    int   bearing;
    bool  validCoords     = false;
    bool  validPrevCoords = false;
  private:
    struct  BSSID_RSSI {
      uint8_t bssid[6];
      int8_t  rssi;
    } nets[MAXNETS];
    int           netCount;
    float         prevLatitude;
    float         prevLongitude;
    unsigned long prevTime;
    unsigned long currTime;
};

#endif /* MLS_H */
