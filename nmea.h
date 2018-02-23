/**
  nmea.h - Simple GPS data export in NMEA format

  Copyright (c) 2017-2018 Costin STROIE <costinstroie@eridu.eu.org>

  This file is part of WiFi_APRS_Tracker.
*/

#ifndef NMEA_H
#define NMEA_H

#include "Arduino.h"

class NMEA {
  public:
    NMEA();
    void          init();
    int           getGGA(char *buf, size_t len, unsigned long utm, float lat, float lng, int fix, int sat);
    int           getRMC(char *buf, size_t len, unsigned long utm, float lat, float lng, int spd, int crs);
    int           getGLL(char *buf, size_t len, unsigned long utm, float lat, float lng);
    int           getVTG(char *buf, size_t len, int crs, int knots, int kmh);
    int           getZDA(char *buf, size_t len, unsigned long utm);
    int           getWelcome(const char* name, const char* vers);
    char          welcome[80];
  private:
    int           checksum(const char *s);
    void          getCoords(float lat, float lng);
    void          getTime(unsigned long utm);
    int           latDD, latMM, latFF, lngDD, lngMM, lngFF;
    int           yy, ll, dd, hh, mm, ss;
    float         latOLD, lngOLD;
    unsigned long utmOLD;
};

#endif /* NMEA_H */
