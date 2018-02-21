/**
  nmea.cpp - Simple GPS data export in NMEA format

  Copyright (c) 2017-2018 Costin STROIE <costinstroie@eridu.eu.org>

  This file is part of WiFi_APRS_Tracker.
*/

#include "Arduino.h"
#include "nmea.h"

NMEA::NMEA() {
}

void NMEA::init() {
}

void NMEA::send(unsigned long utm, float lat, float lng, int spd, int crs, int fix, int sat) {
  // Local buffers
  char buf[100] = "";
  char ckbuf[4] = "";
  // Compute hour, minute and second
  int hh = (utm % 86400L) / 3600;
  int mm = (utm % 3600) / 60;
  int ss =  utm % 60;
  // Compute integer and fractional coordinates
  int latDD = (int)(abs(lat));
  int latMM = (int)((fabs(lat) - latDD) * 6000);
  int lngDD = (int)(abs(lng));
  int lngMM = (int)((fabs(lng) - lngDD) * 6000);
  // GGA
  sprintf_P(buf, PSTR("$GPGGA,%02d%02d%02d.0,%02d%02d.%03d,%c,%03d%02d.%03d,%c,%d,%d,1,0,M,0,M,,"),
            hh, mm, ss,
            latDD, latMM / 100, latMM % 100, lat >= 0 ? 'N' : 'S',
            lngDD, lngMM / 100, lngMM % 100, lng >= 0 ? 'E' : 'W',
            fix, sat);
  sprintf(ckbuf, "*%02X", checksum(buf));
  strcat(buf, ckbuf);
  Serial.print(buf);
  Serial.print("\r\n");
  // RMC
  sprintf_P(buf, PSTR("$GPRMC,%02d%02d%02d.0,A,%02d%02d.%03d,%c,%03d%02d.%03d,%c,%03d.0,%03d.0,,,"),
            hh, mm, ss,
            latDD, latMM / 100, latMM % 100, lat >= 0 ? 'N' : 'S',
            lngDD, lngMM / 100, lngMM % 100, lng >= 0 ? 'E' : 'W',
            spd, crs);
  sprintf(ckbuf, "*%02X", checksum(buf));
  strcat(buf, ckbuf);
  Serial.print(buf);
  Serial.print("\r\n");
}

/**
  Compute the checksum
*/
int NMEA::checksum(const char *s) {
  int c = 0;
  s++;
  while (*s)
    c ^= *s++;
  return c;
}

