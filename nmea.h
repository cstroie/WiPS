/**
  nmea.h - Simple GPS data export in NMEA format

  Copyright (c) 2017-2023 Costin STROIE <costinstroie@eridu.eu.org>

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

#ifndef NMEA_H
#define NMEA_H

#include "Arduino.h"

class NMEA {
  public:
    NMEA();
    void          init();
    int           checksum(const char *s);
    int           getGGA(char *buf, size_t len, unsigned long utm, float lat, float lng, int fix, int sat);
    int           getRMC(char *buf, size_t len, unsigned long utm, float lat, float lng, int spd, int crs);
    int           getGLL(char *buf, size_t len, unsigned long utm, float lat, float lng);
    int           getVTG(char *buf, size_t len, int crs, int knots, int kmh);
    int           getZDA(char *buf, size_t len, unsigned long utm);
    int           getWelcome(const char* name, const char* vers);
    char          welcome[80];
  private:
    void          getCoords(float lat, float lng);
    void          getTime(unsigned long utm);
    int           latDD, latMM, latFF, lngDD, lngMM, lngFF;
    int           yy, ll, dd, hh, mm, ss;
    float         latOLD, lngOLD;
    unsigned long utmOLD;
};

#endif /* NMEA_H */
