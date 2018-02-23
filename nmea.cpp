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

/**
  Compose the welcome message
*/
int NMEA::getWelcome(const char* name, const char* vers) {
  strcat(welcome, "$PVERS");
  strcat(welcome, ",");
  strcat(welcome, name);
  strcat(welcome, ",");
  strcat(welcome, vers);
  strcat(welcome, ",");
  strcat(welcome, __DATE__);
  // Checksum
  char ckbuf[8] = "";
  sprintf(ckbuf, "*%02X\r\n", checksum(welcome));
  strcat(welcome, ckbuf);
  return strlen(welcome);
}

/**
  Set the coordinates to work with

  @param lat latitude
  @param lng longitude
*/
void NMEA::getCoords(float lat, float lng) {
  // Compute integer and fractional coordinates
  if (lat != latOLD) {
    latDD = (int)(abs(lat));
    latMM = (int)((fabs(lat) - latDD) * 60 * 10000);
    latFF = (int)(latMM % 10000);
    latMM = (int)(latMM / 10000);
    latOLD = lat;
  }
  if (lng != lngOLD) {
    lngDD = (int)(abs(lng));
    lngMM = (int)((fabs(lng) - lngDD) * 60 * 10000);
    lngFF = (int)(lngMM % 10000);
    lngMM = (int)(lngMM / 10000);
    lngOLD = lng;
  }
}

/**
  Compute hour, minute and second

  @param utm UNIX time
*/
void NMEA::getTime(unsigned long utm) {
  if (utm != utmOLD) {
    hh = (utm % 86400L) / 3600;
    mm = (utm % 3600) / 60;
    ss =  utm % 60;
    utmOLD = utm;
  }
}

/*
  Compose the GGA sentence
*/
int NMEA::getGGA(char *buf, size_t len, unsigned long utm, float lat, float lng, int fix, int sat) {
  // Get integer and fractional coordinates
  getCoords(lat, lng);
  // Get time
  getTime(utm);

  // GGA
  snprintf_P(buf, len, PSTR("$GPGGA,%02d%02d%02d.0,%02d%02d.%04d,%c,%03d%02d.%04d,%c,%d,%d,1,0,M,0,M,,"),
             hh, mm, ss,
             latDD, latMM, latFF, lat >= 0 ? 'N' : 'S',
             lngDD, lngMM, lngFF, lng >= 0 ? 'E' : 'W',
             fix, sat);
  // Checksum
  char ckbuf[8] = "";
  sprintf(ckbuf, "*%02X\r\n", checksum(buf));
  strcat(buf, ckbuf);
  return strlen(buf);
}

/*
  Compose the RMC sentence
*/
int NMEA::getRMC(char *buf, size_t len, unsigned long utm, float lat, float lng, int spd, int crs) {
  // Get integer and fractional coordinates
  getCoords(lat, lng);
  // Get time
  getTime(utm);

  // RMC
  snprintf_P(buf, len, PSTR("$GPRMC,%02d%02d%02d.0,A,%02d%02d.%04d,%c,%03d%02d.%04d,%c,%03d.0,%03d.0,,,"),
             hh, mm, ss,
             latDD, latMM, latFF, lat >= 0 ? 'N' : 'S',
             lngDD, lngMM, lngFF, lng >= 0 ? 'E' : 'W',
             spd, crs);
  // Checksum
  char ckbuf[8] = "";
  sprintf(ckbuf, "*%02X\r\n", checksum(buf));
  strcat(buf, ckbuf);
  return strlen(buf);
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

