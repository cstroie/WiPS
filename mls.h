/**
  mls.h - Mozilla Location Services

  Copyright (c) 2017 Costin STROIE <costinstroie@eridu.eu.org>

  This file is part of WiFi_APRS_Tracker.
*/

#ifndef MLS_H
#define MLS_H

#define MAXNETS 64

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

const char geoServer[]        = "location.services.mozilla.com";
const int  geoPort            = 443;
const char geoPOST[] PROGMEM  = "POST /v1/geolocate?key=test HTTP/1.1";
const char eol[]     PROGMEM  = "\r\n";


class MLS {
  public:
    MLS();
    void  init();
    int   wifiScan();
    int   geoLocation();
    void  getVector();
    size_t netCount;
    float latitude, longitude;
    float distance, speed;
    int bearing;
  private:
    struct BSSID_RSSI {
      uint8_t bssid[6];
      int8_t  rssi;
    } nets[MAXNETS];
    bool validCoords      = false;
    bool validPrevCoords  = false;
    float prevLatitude, prevLongitude;
    time_t prevTime, currTime;
};

#endif /* MLS_H */
