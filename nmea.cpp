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

void NMEA::sendGGA(char *buf, size_t len, unsigned long utm, float lat, float lng, int fix, int sat) {
  // Local buffers
  char ckbuf[8] = "";
  // Compute hour, minute and second
  int hh = (utm % 86400L) / 3600;
  int mm = (utm % 3600) / 60;
  int ss =  utm % 60;
  // Compute integer and fractional coordinates
  int latDD = (int)(abs(lat));
  int latMM = (int)((fabs(lat) - latDD) * 600000);
  int lngDD = (int)(abs(lng));
  int lngMM = (int)((fabs(lng) - lngDD) * 600000);

  // GGA
  snprintf_P(buf, len, PSTR("$GPGGA,%02d%02d%02d.0,%02d%02d.%04d,%c,%03d%02d.%04d,%c,%d,%d,1,0,M,0,M,,"),
             hh, mm, ss,
             latDD, latMM / 10000, latMM % 10000, lat >= 0 ? 'N' : 'S',
             lngDD, lngMM / 10000, lngMM % 10000, lng >= 0 ? 'E' : 'W',
             fix, sat);
  sprintf(ckbuf, "*%02X\r\n", checksum(buf));
  strcat(buf, ckbuf);
}
void NMEA::sendRMC(char *buf, size_t len, unsigned long utm, float lat, float lng, int spd, int crs) {
  // Local buffers
  char ckbuf[8] = "";
  // Compute hour, minute and second
  int hh = (utm % 86400L) / 3600;
  int mm = (utm % 3600) / 60;
  int ss =  utm % 60;
  // Compute integer and fractional coordinates
  int latDD = (int)(abs(lat));
  int latMM = (int)((fabs(lat) - latDD) * 600000);
  int lngDD = (int)(abs(lng));
  int lngMM = (int)((fabs(lng) - lngDD) * 600000);

  // RMC
  snprintf_P(buf, len, PSTR("$GPRMC,%02d%02d%02d.0,A,%02d%02d.%04d,%c,%03d%02d.%04d,%c,%03d.0,%03d.0,,,"),
             hh, mm, ss,
             latDD, latMM / 10000, latMM % 10000, lat >= 0 ? 'N' : 'S',
             lngDD, lngMM / 10000, lngMM % 10000, lng >= 0 ? 'E' : 'W',
             spd, crs);
  sprintf(ckbuf, "*%02X\r\n", checksum(buf));
  strcat(buf, ckbuf);
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

