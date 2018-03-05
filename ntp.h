/**
  ntp.h - Network Time Protocol

  Copyright (c) 2017-2018 Costin STROIE <costinstroie@eridu.eu.org>

  This file is part of WiPS.
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
