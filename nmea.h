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
    void          send();
};

#endif /* NMEA_H */
