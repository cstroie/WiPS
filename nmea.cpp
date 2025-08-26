/**
  nmea.cpp - NMEA-0183 sentence generator
         
  Implementation of NMEA-0183 sentence generation for GPS and navigation
  applications. Handles time conversion, coordinate formatting, checksum
  calculation, and proper NMEA sentence structure.

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

#include "Arduino.h"
#include "nmea.h"

/**
  Constructor - Initialize NMEA object with default values
*/
NMEA::NMEA() {
  // Initialize time and coordinate tracking variables
  yy = ll = dd = hh = mm = ss = 0;
  utmOLD = 0;
  latDD = latMM = latFF = lngDD = lngMM = lngFF = 0;
  latOLD = lngOLD = 0.0;
}

/**
  Initialize NMEA - currently empty but reserved for future use
*/
void NMEA::init() {
}

/**
  Generate welcome/version identification sentence
  
  Creates a proprietary NMEA sentence containing device name, version,
  and compilation date with proper checksum.
  
  Format: $PVERS,name,version,date*hh
  
  @param name Device name
  @param vers Version string
  @return Length of generated sentence
*/
int NMEA::getWelcome(const char* name, const char* vers) {
  // Start with the sentence header
  strcat(welcome, "$PVERS");
  strcat(welcome, ",");
  strcat(welcome, name);
  strcat(welcome, ",");
  strcat(welcome, vers);
  strcat(welcome, ",");
  strcat(welcome, __DATE__);
  
  // Calculate and append checksum
  char ckbuf[8] = "";
  sprintf_P(ckbuf, PSTR("*%02X\r\n"), checksum(welcome));
  strcat(welcome, ckbuf);
  
  return strlen(welcome);
}

/**
  Convert decimal coordinates to NMEA format (DDMM.MMMM)
  
  Extracts degrees, minutes, and fractional minutes from decimal degrees
  and stores them in internal variables for sentence generation.
  Uses optimization to avoid recalculation if coordinates haven't changed.
  
  @param lat Latitude in decimal degrees
  @param lng Longitude in decimal degrees
*/
void NMEA::getCoords(float lat, float lng) {
  // Compute integer and fractional coordinates for latitude
  if (lat != latOLD) {
    // Extract degrees
    latDD = (int)(abs(lat));
    // Convert fractional degrees to minutes with 4 decimal places
    latMM = (int)((fabs(lat) - latDD) * 60 * 10000);
    // Extract fractional minutes (last 4 digits)
    latFF = (int)(latMM % 10000);
    // Extract whole minutes
    latMM = (int)(latMM / 10000);
    // Update tracking variable
    latOLD = lat;
  }
  
  // Compute integer and fractional coordinates for longitude
  if (lng != lngOLD) {
    // Extract degrees
    lngDD = (int)(abs(lng));
    // Convert fractional degrees to minutes with 4 decimal places
    lngMM = (int)((fabs(lng) - lngDD) * 60 * 10000);
    // Extract fractional minutes (last 4 digits)
    lngFF = (int)(lngMM % 10000);
    // Extract whole minutes
    lngMM = (int)(lngMM / 10000);
    // Update tracking variable
    lngOLD = lng;
  }
}

/**
  Convert Unix timestamp to NMEA time and date format
  
  Extracts year, month, day, hour, minute, and second from Unix timestamp
  and stores them in internal variables for sentence generation.
  Uses optimization to avoid recalculation if time hasn't changed.
  
  @param utm Unix timestamp
*/
void NMEA::getTime(unsigned long utm) {
  // Days in each month for non-leap years (PROGMEM for memory efficiency)
  static const uint8_t daysInMonth [] PROGMEM = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  
  // Only recalculate if time has changed
  if (utm != utmOLD) {
    // Convert to year 2000 epoch (Unix epoch is 1970, NMEA uses 2000)
    utm -= 946684800UL;
    
    // Extract time components
    ss = utm % 60;        // Seconds
    utm /= 60;
    mm = utm % 60;        // Minutes
    utm /= 60;
    hh = utm % 24;        // Hours
    uint16_t days = utm / 24;  // Days since 2000-01-01
    
    // Compute the date by counting years and months
    yy = 0;  // Years since 2000
    ll = 1;  // Month (1-based)
    dd = 1;  // Day (1-based)
    
    // Account for leap years when counting years
    uint8_t leap;
    for (yy = 0; ; ++yy) {
      leap = yy % 4 == 0;
      if (days < 365 + leap)
        break;
      days -= 365 + leap;
    }
    
    // Count months
    for (ll = 1; ; ++ll) {
      uint8_t daysPerMonth = pgm_read_byte(daysInMonth + ll - 1);
      if (leap && ll == 2)
        ++daysPerMonth;
      if (days < daysPerMonth)
        break;
      days -= daysPerMonth;
    }
    
    dd = days + 1;  // Convert to 1-based day
    
    // Keep the time for optimization
    utmOLD = utm;
  }
}

/**
  Generate GGA (Global Positioning System Fix Data) sentence
  
  Provides essential fix data including time, position, fix quality,
  number of satellites, and dilution of precision.
  
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
int NMEA::getGGA(char *buf, size_t len, unsigned long utm, float lat, float lng, int fix, int sat) {
  // Get integer and fractional coordinates
  getCoords(lat, lng);
  
  // Get time components
  getTime(utm);

  // Generate GGA sentence with position, time, and fix data
  snprintf_P(buf, len, PSTR("$GPGGA,%02d%02d%02d.0,%02d%02d.%04d,%c,%03d%02d.%04d,%c,%d,%d,1,0,M,0,M,,"),
             hh, mm, ss,                           // Time (hhmmss.ss)
             latDD, latMM, latFF, lat >= 0 ? 'N' : 'S',  // Latitude (DDMM.MMMM,N/S)
             lngDD, lngMM, lngFF, lng >= 0 ? 'E' : 'W',  // Longitude (DDDMM.MMMM,E/W)
             fix, sat);                            // Fix quality and satellite count
  
  // Calculate and append checksum
  char ckbuf[8] = "";
  sprintf_P(ckbuf, PSTR("*%02X\r\n"), checksum(buf));
  strcat(buf, ckbuf);
  
  return strlen(buf);
}

/**
  Generate RMC (Recommended Minimum Specific GPS/Transit Data) sentence
  
  Provides the recommended minimum data including time, position,
  speed, course, and date.
  
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
int NMEA::getRMC(char *buf, size_t len, unsigned long utm, float lat, float lng, int spd, int crs) {
  // Get integer and fractional coordinates
  getCoords(lat, lng);
  
  // Get time components
  getTime(utm);

  // Generate RMC sentence with position, speed, course, and date
  snprintf_P(buf, len, PSTR("$GPRMC,%02d%02d%02d.0,A,%02d%02d.%04d,%c,%03d%02d.%04d,%c,%03d.0,%03d.0,%02d%02d%02d,,,E"),
             hh, mm, ss,                           // Time (hhmmss.ss)
             latDD, latMM, latFF, lat >= 0 ? 'N' : 'S',  // Latitude (DDMM.MMMM,N/S)
             lngDD, lngMM, lngFF, lng >= 0 ? 'E' : 'W',  // Longitude (DDDMM.MMMM,E/W)
             spd, crs > 0 ? crs : 0,               // Speed and course
             dd, ll, yy);                          // Date (ddmmyy)
  
  // Calculate and append checksum
  char ckbuf[8] = "";
  sprintf_P(ckbuf, PSTR("*%02X\r\n"), checksum(buf));
  strcat(buf, ckbuf);
  
  return strlen(buf);
}

/**
  Generate GLL (Geographic Position - Latitude/Longitude) sentence
  
  Provides current position and time of position fix.
  
  Format: $GPGLL,llll.ll,a,yyyyy.yy,a,hhmmss.ss,A,E*hh
  
  @param buf Buffer to store generated sentence
  @param len Buffer length
  @param utm Unix timestamp for time reference
  @param lat Latitude in decimal degrees
  @param lng Longitude in decimal degrees
  @return Length of generated sentence
*/
int NMEA::getGLL(char *buf, size_t len, unsigned long utm, float lat, float lng) {
  // Get integer and fractional coordinates
  getCoords(lat, lng);
  
  // Get time components
  getTime(utm);

  // Generate GLL sentence with position and time
  snprintf_P(buf, len, PSTR("$GPGLL,%02d%02d.%04d,%c,%03d%02d.%04d,%c,%02d%02d%02d.0,A,E"),
             latDD, latMM, latFF, lat >= 0 ? 'N' : 'S',    // Latitude (DDMM.MMMM,N/S)
             lngDD, lngMM, lngFF, lng >= 0 ? 'E' : 'W',    // Longitude (DDDMM.MMMM,E/W)
             hh, mm, ss);                                  // Time (hhmmss.ss)
  
  // Calculate and append checksum
  char ckbuf[8] = "";
  sprintf_P(ckbuf, PSTR("*%02X\r\n"), checksum(buf));
  strcat(buf, ckbuf);
  
  return strlen(buf);
}

/**
  Generate VTG (Track Made Good and Ground Speed) sentence
  
  Provides course and speed information relative to the ground.
  
  Format: $GPVTG,x.x,T,,M,x.x,N,x.x,K,E*hh
  
  @param buf Buffer to store generated sentence
  @param len Buffer length
  @param crs Course over ground in degrees
  @param knots Speed over ground in knots
  @param kmh Speed over ground in kilometers per hour
  @return Length of generated sentence
*/
int NMEA::getVTG(char *buf, size_t len, int crs, int knots, int kmh) {
  // Generate VTG sentence with course and speed data
  snprintf_P(buf, len, PSTR("$GPVTG,%03d.0,T,,M,%03d.0,N,%03d.0,K,E"),
             crs > 0 ? crs : 0,  // Course over ground (degrees)
             knots,              // Speed over ground (knots)
             kmh);               // Speed over ground (km/h)
  
  // Calculate and append checksum
  char ckbuf[8] = "";
  sprintf_P(ckbuf, PSTR("*%02X\r\n"), checksum(buf));
  strcat(buf, ckbuf);
  
  return strlen(buf);
}

/**
  Generate ZDA (Time and Date) sentence
  
  Provides complete time and date information with local time zone.
  
  Format: $GPZDA,hhmmss.ss,dd,mm,yyyy,xx,xx*hh
  
  @param buf Buffer to store generated sentence
  @param len Buffer length
  @param utm Unix timestamp for time reference
  @return Length of generated sentence
*/
int NMEA::getZDA(char *buf, size_t len, unsigned long utm) {
  // Get time components
  getTime(utm);

  // Generate ZDA sentence with complete time and date
  snprintf_P(buf, len, PSTR("$GPZDA,%02d%02d%02d.0,%02d,%02d,%04d,,"),
             hh, mm, ss,      // Time (hhmmss.ss)
             dd, ll, yy + 2000); // Date (dd,mm,yyyy)
  
  // Calculate and append checksum
  char ckbuf[8] = "";
  sprintf_P(ckbuf, PSTR("*%02X\r\n"), checksum(buf));
  strcat(buf, ckbuf);
  
  return strlen(buf);
}

/**
  Calculate NMEA checksum for a sentence
  
  Computes the XOR of all characters between '$' and '*' delimiters.
  The checksum is used to verify data integrity in NMEA sentences.
  
  @param s NMEA sentence string (starting with '$')
  @return Checksum value (0-255)
*/
int NMEA::checksum(const char *s) {
  int c = 0;
  s++;  // Skip the '$' character
  // XOR all characters until end of string
  while (*s)
    c ^= *s++;
  return c;
}
