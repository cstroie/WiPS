/**
  nmea.cpp - Simple GPS data export in NMEA format

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
  sprintf_P(ckbuf, PSTR("*%02X\r\n"), checksum(welcome));
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
  static const uint8_t daysInMonth [] PROGMEM = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (utm != utmOLD) {
    // Bring to year 2000 epoch
    utm -= 946684800UL;
    ss = utm % 60;
    utm /= 60;
    mm = utm % 60;
    utm /= 60;
    hh = utm % 24;
    uint16_t days = utm / 24;
    uint8_t leap;
    for (yy = 0; ; ++yy) {
      leap = yy % 4 == 0;
      if (days < 365 + leap)
        break;
      days -= 365 + leap;
    }
    for (ll = 1; ; ++ll) {
      uint8_t daysPerMonth = pgm_read_byte(daysInMonth + ll - 1);
      if (leap && ll == 2)
        ++daysPerMonth;
      if (days < daysPerMonth)
        break;
      days -= daysPerMonth;
    }
    dd = days + 1;
    utmOLD = utm;
  }
}

/*
  Compose the GGA sentence
  $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
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
  sprintf_P(ckbuf, PSTR("*%02X\r\n"), checksum(buf));
  strcat(buf, ckbuf);
  return strlen(buf);
}

/*
  Compose the RMC sentence
  $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
*/
int NMEA::getRMC(char *buf, size_t len, unsigned long utm, float lat, float lng, int spd, int crs) {
  // Get integer and fractional coordinates
  getCoords(lat, lng);
  // Get time
  getTime(utm);

  // RMC
  snprintf_P(buf, len, PSTR("$GPRMC,%02d%02d%02d.0,A,%02d%02d.%04d,%c,%03d%02d.%04d,%c,%03d.0,%03d.0,%02d%02d%02d,,,E"),
             hh, mm, ss,
             latDD, latMM, latFF, lat >= 0 ? 'N' : 'S',
             lngDD, lngMM, lngFF, lng >= 0 ? 'E' : 'W',
             spd, crs > 0 ? crs : 0,
             dd, ll, yy);
  // Checksum
  char ckbuf[8] = "";
  sprintf_P(ckbuf, PSTR("*%02X\r\n"), checksum(buf));
  strcat(buf, ckbuf);
  return strlen(buf);
}

/*
  Compose the GLL sentence
  $GPGLL,4916.45,N,12311.12,W,225444,A,*1D
*/
int NMEA::getGLL(char *buf, size_t len, unsigned long utm, float lat, float lng) {
  // Get integer and fractional coordinates
  getCoords(lat, lng);
  // Get time
  getTime(utm);

  // GLL
  snprintf_P(buf, len, PSTR("$GPGLL,%02d%02d.%04d,%c,%03d%02d.%04d,%c,%02d%02d%02d.0,A,E"),
             latDD, latMM, latFF, lat >= 0 ? 'N' : 'S',
             lngDD, lngMM, lngFF, lng >= 0 ? 'E' : 'W',
             hh, mm, ss);
  // Checksum
  char ckbuf[8] = "";
  sprintf_P(ckbuf, PSTR("*%02X\r\n"), checksum(buf));
  strcat(buf, ckbuf);
  return strlen(buf);
}

/*
  Compose the VTG sentence
  $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
*/
int NMEA::getVTG(char *buf, size_t len, int crs, int knots, int kmh) {
  // VTG
  snprintf_P(buf, len, PSTR("$GPVTG,%03d.0,T,,M,%03d.0,N,%03d.0,K,E"),
             crs > 0 ? crs : 0, knots, kmh);
  // Checksum
  char ckbuf[8] = "";
  sprintf_P(ckbuf, PSTR("*%02X\r\n"), checksum(buf));
  strcat(buf, ckbuf);
  return strlen(buf);
}

/*
  Compose the ZDA sentence
  $GPZDA,201530.00,04,07,2002,00,00*60
*/
int NMEA::getZDA(char *buf, size_t len, unsigned long utm) {
  // Get time
  getTime(utm);

  // ZDA
  snprintf_P(buf, len, PSTR("$GPZDA,%02d%02d%02d.0,%02d,%02d,%04d,,"),
             hh, mm, ss, dd, ll, yy + 2000);
  // Checksum
  char ckbuf[8] = "";
  sprintf_P(ckbuf, PSTR("*%02X\r\n"), checksum(buf));
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
