/**
  ntp.h - Network Time Protocol

  Copyright (c) 2017-2020 Costin STROIE <costinstroie@eridu.eu.org>

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

#ifndef NTP_H
#define NTP_H

#include "Arduino.h"
#include <WiFiUdp.h>

struct datetime_t {
  uint8_t yy;
  uint8_t ll;
  uint8_t dd;
  uint8_t hh;
  uint8_t mm;
  uint8_t ss;
};

class NTP {
  public:
    NTP();
    unsigned long init(const char *ntpServer, int ntpPort = 123);
    void          setServer(const char *ntpServer, int ntpPort = 123);
    void          setTZ(float tz);
    void          report(unsigned long utm);
    unsigned long getSeconds(bool sync = true);
    unsigned long getUptime(char *buf, size_t len);
    datetime_t    getDateTime(unsigned long utm);
    uint8_t       getClock(char *buf, size_t len, unsigned long utm);
    uint8_t       getDOW(uint16_t year, uint8_t month, uint8_t day);
    bool          dstCheck(uint16_t year, uint8_t month, uint8_t day, uint8_t hour);
    bool          valid       = false;               // Flag to know the time is accurate
  private:
    unsigned long getNTP();
    char          server[50];                        // NTP server to connect to (RFC5905)
    int           port     = 123;                    // NTP port
    unsigned long nextSync = 0UL;                    // Next time to syncronize
    unsigned long delta    = 0UL;                    // Difference between real time and internal clock
    float         TZ       = 0;                      // Time zone
};

#endif /* NTP_H */
