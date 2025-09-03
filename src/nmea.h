/**
  nmea.h - NMEA-0183 sentence generator
         
  This library provides functionality for generating NMEA-0183 compliant
  sentences commonly used in GPS and navigation systems. It supports
  GGA, RMC, GLL, VTG, and ZDA sentence formats with proper checksums.
  
  The implementation handles:
  - Time conversion from Unix timestamp to NMEA format
  - Coordinate conversion from decimal degrees to NMEA format
  - Checksum calculation for NMEA sentence validation
  - Proper formatting according to NMEA-0183 standards

  Features:
  - GGA (Global Positioning System Fix Data) sentence generation
  - RMC (Recommended Minimum Specific GPS/Transit Data) sentence generation
  - GLL (Geographic Position - Latitude/Longitude) sentence generation
  - VTG (Track Made Good and Ground Speed) sentence generation
  - ZDA (Time and Date) sentence generation
  - Automatic checksum calculation and formatting

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

#ifndef NMEA_H
#define NMEA_H

#include "Arduino.h"
#ifdef ESP32
  #include <WiFi.h>
#endif

/**
  NMEA-0183 sentence generator class
  
  Provides methods for generating various NMEA sentence types with
  proper formatting, checksums, and time/coordinate conversions.
*/
class NMEA {
  public:
    /**
      Constructor - Initialize NMEA object with default values
    */
    NMEA();
    
    /**
      Initialize NMEA - currently empty but reserved for future use
    */
    void          init();
    
    /**
      Calculate NMEA checksum for a sentence
      
      Computes XOR of all characters between '$' and '*' delimiters.
      
      @param s NMEA sentence string (starting with '$')
      @return Checksum value (0-255)
    */
    int           checksum(const char *s);
    
    /**
      Generate GGA (Global Positioning System Fix Data) sentence
      
      Format: $GPGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,x.x,xxxx*hh
      
      @param buf Buffer to store generated sentence
      @param len Buffer length
      @param utm Unix timestamp for time reference
      @param lat Latitude in decimal degrees
      @param lng Longitude in decimal degrees
      @param fix GPS fix quality (0=invalid, 1=GPS, 2=DGPS, etc.)
      @param sat Number of satellites in view
      @return Length of generated sentence
    */
    int           getGGA(char *buf, size_t len, unsigned long utm, float lat, float lng, int fix, int sat);
    
    /**
      Generate RMC (Recommended Minimum Specific GPS/Transit Data) sentence
      
      Format: $GPRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,x.x,a*hh
      
      @param buf Buffer to store generated sentence
      @param len Buffer length
      @param utm Unix timestamp for time reference
      @param lat Latitude in decimal degrees
      @param lng Longitude in decimal degrees
      @param spd Speed over ground in knots
      @param crs Course over ground in degrees
      @return Length of generated sentence
    */
    int           getRMC(char *buf, size_t len, unsigned long utm, float lat, float lng, int spd, int crs);
    
    /**
      Generate GLL (Geographic Position - Latitude/Longitude) sentence
      
      Format: $GPGLL,llll.ll,a,yyyyy.yy,a,hhmmss.ss,A,E*hh
      
      @param buf Buffer to store generated sentence
      @param len Buffer length
      @param utm Unix timestamp for time reference
      @param lat Latitude in decimal degrees
      @param lng Longitude in decimal degrees
      @return Length of generated sentence
    */
    int           getGLL(char *buf, size_t len, unsigned long utm, float lat, float lng);
    
    /**
      Generate VTG (Track Made Good and Ground Speed) sentence
      
      Format: $GPVTG,x.x,T,,M,x.x,N,x.x,K,E*hh
      
      @param buf Buffer to store generated sentence
      @param len Buffer length
      @param crs Course over ground in degrees
      @param knots Speed over ground in knots
      @param kmh Speed over ground in kilometers per hour
      @return Length of generated sentence
    */
    int           getVTG(char *buf, size_t len, int crs, int knots, int kmh);
    
    /**
      Generate ZDA (Time and Date) sentence
      
      Format: $GPZDA,hhmmss.ss,dd,mm,yyyy,xx,xx*hh
      
      @param buf Buffer to store generated sentence
      @param len Buffer length
      @param utm Unix timestamp for time reference
      @return Length of generated sentence
    */
    int           getZDA(char *buf, size_t len, unsigned long utm);
    
    /**
      Generate welcome/version identification sentence
      
      Format: $PVERS,name,version,date*hh
      
      @param name Device name
      @param vers Version string
      @return Length of generated sentence
    */
    int           getWelcome(const char* name, const char* vers);
    
    char          welcome[80];  ///< Welcome/version sentence buffer

  private:
    /**
      Convert decimal coordinates to NMEA format (DDMM.MMMM)
      
      Extracts degrees, minutes, and fractional minutes from decimal degrees
      and stores them in internal variables for sentence generation.
      
      @param lat Latitude in decimal degrees
      @param lng Longitude in decimal degrees
    */
    void          getCoords(float lat, float lng);
    
    /**
      Convert Unix timestamp to NMEA time and date format
      
      Extracts year, month, day, hour, minute, and second from Unix timestamp
      and stores them in internal variables for sentence generation.
      
      @param utm Unix timestamp
    */
    void          getTime(unsigned long utm);
    
    // Time components for NMEA sentences
    int           yy;     ///< Year (00-99)
    int           ll;     ///< Month (01-12)
    int           dd;     ///< Day (01-31)
    int           hh;     ///< Hour (00-23)
    int           mm;     ///< Minute (00-59)
    int           ss;     ///< Second (00-59)
    unsigned long utmOLD; ///< Previous Unix timestamp for optimization
    
    // Coordinate components for NMEA sentences
    int           latDD;  ///< Latitude degrees
    int           latMM;  ///< Latitude minutes
    int           latFF;  ///< Latitude fractional minutes (4 digits)
    int           lngDD;  ///< Longitude degrees
    int           lngMM;  ///< Longitude minutes
    int           lngFF;  ///< Longitude fractional minutes (4 digits)
    float         latOLD; ///< Previous latitude for optimization
    float         lngOLD; ///< Previous longitude for optimization
};

#endif /* NMEA_H */
