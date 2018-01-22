/**
  ntp.h - Network Time Protocol

  Copyright (c) 2017 Costin STROIE <costinstroie@eridu.eu.org>

  This file is part of WiFi_APRS_Tracker.
*/

#ifndef NTP_H
#define NTP_H

#include "Arduino.h"
#include <WiFiUdp.h>

class NTP {
  public:
    NTP();
    void          init(const char *ntpServer, int ntpPort = 123);
    void          setServer(const char *ntpServer, int ntpPort = 123);
    void          setTZ(float tz);
    unsigned long getSeconds(bool sync = true);
    unsigned long getUptime(char *buf, size_t len);
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
