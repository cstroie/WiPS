/**
  ntp.h - Network Time Protocol Client
         
  This library provides functionality for synchronizing time with NTP servers
  and handling time zone conversions, daylight saving time, and time reporting.
  
  Features:
  - NTP time synchronization with configurable servers
  - Automatic time synchronization at regular intervals
  - Time zone handling with floating point offsets
  - Daylight saving time detection for European rules
  - NMEA-style time reporting format
  - Uptime calculation and formatting

  Copyright (c) 2017-2025 Costin STROIE <costinstroie@eridu.eu.org>

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
#ifdef ESP32
  #include <WiFi.h>
  #include <WiFiUdp.h>
#else
  #include <ESP8266WiFi.h>
  #include <WiFiUdp.h>
#endif

/**
  Date and time structure for breaking down Unix timestamps
  
  Stores date and time components extracted from Unix timestamps for
  easy formatting and display purposes.
*/
struct datetime_t {
  uint8_t yy;  ///< Year (0-99, representing 2000-2099)
  uint8_t ll;  ///< Month (1-12)
  uint8_t dd;  ///< Day (1-31)
  uint8_t hh;  ///< Hour (0-23)
  uint8_t mm;  ///< Minute (0-59)
  uint8_t ss;  ///< Second (0-59)
};

/**
  Network Time Protocol client class
  
  Provides methods for time synchronization, time zone handling,
  daylight saving time detection, and time formatting.
*/
class NTP {
  public:
    /**
      Constructor - Initialize NTP object with default values
    */
    NTP();
    
    /**
      Initialize NTP client with server and port
      
      @param ntpServer NTP server hostname
      @param ntpPort NTP server port (default 123)
      @return Current Unix timestamp or 0 on failure
    */
    unsigned long init(const char *ntpServer, int ntpPort = 123);
    
    /**
      Set NTP server hostname and port
      
      @param ntpServer NTP server hostname
      @param ntpPort NTP server port (default 123)
    */
    void          setServer(const char *ntpServer, int ntpPort = 123);
    
    /**
      Set time zone offset from UTC
      
      @param tz Time zone offset in hours (-12.0 to +12.0)
    */
    void          setTZ(float tz);
    
    /**
      Report time in NMEA-style format to serial port
      
      Format: $PNTPC,0x%08X,%d.%02d.%02d,%02d.%02d.%02d\r\n
      
      @param utm Unix timestamp to report
    */
    void          report(unsigned long utm);
    
    /**
      Get current time in seconds since Unix epoch
      
      @param sync Whether to attempt time synchronization if needed (default true)
      @return Unix timestamp
    */
    unsigned long getSeconds(bool sync = true);
    
    /**
      Get system uptime formatted as days, hours, minutes, seconds
      
      @param buf Buffer to store formatted uptime string
      @param len Buffer length
      @return Uptime in seconds
    */
    unsigned long getUptime(char *buf, size_t len);
    
    /**
      Convert Unix timestamp to date/time components
      
      @param utm Unix timestamp
      @return datetime_t structure with date/time components
    */
    datetime_t    getDateTime(unsigned long utm);
    
    /**
      Format time as HH:MM:SS string
      
      @param buf Buffer to store formatted time string
      @param len Buffer length
      @param utm Unix timestamp
      @return Length of formatted string
    */
    uint8_t       getClock(char *buf, size_t len, unsigned long utm);
    
    /**
      Calculate day of week for given date
      
      Uses Tomohiko Sakamoto's algorithm.
      
      @param year Year (e.g., 2023)
      @param month Month (1-12)
      @param day Day (1-31)
      @return Day of week (0=Sunday, 1=Monday, ..., 6=Saturday)
    */
    uint8_t       getDOW(uint16_t year, uint8_t month, uint8_t day);
    
    /**
      Check if given date/time is in daylight saving time (European rules)
      
      European DST: Last Sunday in March to last Sunday in October
      
      @param year Year (e.g., 2023)
      @param month Month (1-12)
      @param day Day (1-31)
      @param hour Hour (0-23)
      @return true if in DST, false otherwise
    */
    bool          dstCheck(uint16_t year, uint8_t month, uint8_t day, uint8_t hour);
    
    bool          valid = false;  ///< Flag indicating if time is accurate/synchronized

  private:
    /**
      Get time from NTP server using RFC5905 protocol
      
      @return Unix timestamp or 0 on failure
    */
    unsigned long getNTP();
    
    char          server[50];     ///< NTP server address (RFC5905)
    int           port = 123;     ///< NTP port
    unsigned long nextSync = 0UL; ///< Next time to synchronize
    unsigned long delta = 0UL;    ///< Difference between real time and internal clock
    float         TZ = 0;         ///< Time zone offset in hours
};

#endif /* NTP_H */
