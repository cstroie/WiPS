/**
  ntp.cpp - Network Time Protocol Client
         
  Implementation of NTP client functionality for time synchronization,
  time zone handling, daylight saving time detection, and time formatting.

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
#include "ntp.h"

/**
  Constructor - Initialize NTP object with default values
*/
NTP::NTP() {
}

/**
  Initialize NTP client with server and port
  
  Sets up the NTP server configuration and performs initial time synchronization.
  
  @param ntpServer NTP server hostname
  @param ntpPort NTP server port (default 123)
  @return Current Unix timestamp
*/
void NTP::init(const char *ntpServer, int ntpPort) {
  setServer(ntpServer, ntpPort);
  return getSeconds(true);
}

/**
  Set NTP server hostname and port
  
  @param ntpServer NTP server hostname
  @param ntpPort NTP server port (default 123)
*/
void NTP::setServer(const char *ntpServer, int ntpPort) {
  port = ntpPort;
  strncpy(server, (char*)ntpServer, sizeof(server));
}

/**
  Set time zone offset from UTC
  
  @param tz Time zone offset in hours (-12.0 to +12.0)
*/
void NTP::setTZ(float tz) {
  TZ = tz;
}

/**
  Report time in NMEA-style format to serial port
  
  Sends a proprietary NMEA sentence with timestamp and formatted date/time.
  
  Format: $PNTPC,0x%08X,%d.%02d.%02d,%02d.%02d.%02d\r\n
  
  @param utm Unix timestamp to report
*/
void NTP::report(unsigned long utm) {
  datetime_t dt = getDateTime(utm);
  Serial.printf_P(PSTR("$PNTPC,0x%08X,%d.%02d.%02d,%02d.%02d.%02d\r\n"),
                  utm, dt.yy + 2000, dt.ll, dt.dd, dt.hh, dt.mm, dt.ss);
}

/**
  Format time as HH:MM:SS string
  
  @param buf Buffer to store formatted time string
  @param len Buffer length
  @param utm Unix timestamp
  @return Length of formatted string
*/
uint8_t NTP::getClock(char *buf, size_t len, unsigned long utm) {
  datetime_t dt = getDateTime(utm);
  snprintf_P(buf, len, PSTR("%02d:%02d:%02d"), dt.hh, dt.mm, dt.ss);
  return strlen(buf);
}

/**
  Get current time in seconds since Unix epoch
  
  Returns the current time, synchronizing with NTP server if needed.
  Time is tracked using millis() for sub-second precision between syncs.
  
  @param sync Whether to attempt time synchronization if needed (default true)
  @return Unix timestamp
*/
unsigned long NTP::getSeconds(bool sync) {
  // Check if we need to sync with NTP server
  if (millis() >= nextSync and sync) {
    // Try to get the time from Internet
    unsigned long utm = getNTP();
    if (utm == 0) {
      // Time sync has failed, try again in one minute
      nextSync = millis() + 60000UL;
      valid = false;
    }
    else {
      // Successfully got time from NTP server
      // Compute the new time delta between real time and internal clock
      delta = utm - (millis() / 1000);
      // Schedule next sync in 8 hours (28800000 ms)
      nextSync = millis() + 28800000UL;
      valid = true;
      report(utm);
    }
  }
  // Get current time based on uptime and time delta,
  // or just uptime for no time sync ever
  // Add time zone offset in seconds
  return (millis() / 1000) + delta + (long)(TZ * 3600);
}

/**
  © Francesco Potortì 2013 - GPLv3 - Revision: 1.13

  Send an NTP packet and wait for the response, return the Unix time

  To lower the memory footprint, no buffers are allocated for sending
  and receiving the NTP packets.  Four bytes of memory are allocated
  for transmision, the rest is random garbage collected from the data
  memory segment, and the received packet is read one byte at a time.
  The Unix time is returned, that is, seconds from 1970-01-01T00:00.
  
  @return Unix timestamp or 0 on failure
*/
unsigned long NTP::getNTP() {
  // NTP UDP client
  WiFiUDP client;
  // Open socket on arbitrary port
  bool ok = client.begin(12321);
  // NTP request header: Only the first four bytes of an outgoing
  // packet need to be set appropriately, the rest can be whatever.
  const long ntpFirstFourBytes = 0xEC0600E3;
  // Fail if UDP could not init a socket
  if (!ok) return 0UL;
  // Clear received data from possible stray received packets
  client.flush();
  // Send an NTP request
  char ntpServerBuf[strlen_P((char*)server) + 1];
  strncpy(ntpServerBuf, (char*)server, sizeof(ntpServerBuf));
  if (!(client.beginPacket(ntpServerBuf, port) &&
        client.write((byte *)&ntpFirstFourBytes, 48) == 48 &&
        client.endPacket()))
    return 0UL;                             // sending request failed
  // Wait for response; check every pollIntv ms up to maxPoll times
  const int pollIntv = 150;                 // poll every this many ms
  const byte maxPoll = 15;                  // poll up to this many times
  int pktLen;                               // received packet length
  for (byte i = 0; i < maxPoll; i++) {
    if ((pktLen = client.parsePacket()) == 48) break;
    delay(pollIntv);
  }
  if (pktLen != 48) return 0UL;             // no correct packet received
  // Read and discard the first useless bytes (32 for speed, 40 for accuracy)
  for (byte i = 0; i < 40; ++i) client.read();
  // Read the integer part of sending time
  unsigned long ntpTime = client.read(); // NTP time
  for (byte i = 1; i < 4; i++)
    ntpTime = ntpTime << 8 | client.read();
  // Round to the nearest second if we want accuracy
  // The fractionary part is the next byte divided by 256: if it is
  // greater than 500ms we round to the next second; we also account
  // for an assumed network delay of 50ms, and (0.5-0.05)*256=115;
  // additionally, we account for how much we delayed reading the packet
  // since its arrival, which we assume on average to be pollIntv/2.
  ntpTime += (client.read() > 115 - pollIntv / 8);
  // Discard the rest of the packet and stop
  client.flush();
  client.stop();
  return ntpTime - 2208988800UL;            // convert NTP time to Unix time
}

/**
  Get system uptime formatted as days, hours, minutes, seconds
  
  @param buf Buffer to store formatted uptime string
  @param len Buffer length
  @return Uptime in seconds
*/
unsigned long NTP::getUptime(char *buf, size_t len) {
  // Get the uptime in seconds
  unsigned long upt = millis() / 1000;
  
  // Compute days, hours, minutes and seconds
  int ss =  upt % 60;
  int mm = (upt % 3600) / 60;
  int hh = (upt % 86400L) / 3600;
  int dd =  upt / 86400L;
  
  // Create the formatted time string with proper pluralization
  if (dd == 1) snprintf_P(buf, len, PSTR("%d day, %02d:%02d:%02d"),  dd, hh, mm, ss);
  else         snprintf_P(buf, len, PSTR("%d days, %02d:%02d:%02d"), dd, hh, mm, ss);
  
  // Return the uptime in seconds
  return upt;
}

/**
  Convert Unix timestamp to date/time components
  
  Breaks down a Unix timestamp into year, month, day, hour, minute, second
  components and applies daylight saving time adjustment if applicable.
  
  @param utm Unix timestamp
  @return datetime_t structure with date/time components
*/
datetime_t NTP::getDateTime(unsigned long utm) {
  datetime_t dt;
  static const uint8_t daysInMonth[] PROGMEM = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  
  // Convert to year 2000 epoch (Unix epoch is 1970, we use 2000)
  utm -= 946684800UL;
  
  // Extract time components
  dt.ss = utm % 60;        // Seconds
  utm /= 60;
  dt.mm = utm % 60;        // Minutes
  utm /= 60;
  dt.hh = utm % 24;        // Hours
  uint16_t days = utm / 24; // Days since 2000-01-01
  
  // Compute the date by counting years and months
  dt.yy = 0;  // Years since 2000
  dt.ll = 1;  // Month (1-based)
  dt.dd = 1;  // Day (1-based)
  
  // Account for leap years when counting years
  uint8_t leap;
  for (dt.yy = 0; ; ++dt.yy) {
    leap = dt.yy % 4 == 0;
    if (days < 365 + leap)
      break;
    days -= 365 + leap;
  }
  
  // Count months
  for (dt.ll = 1; ; ++dt.ll) {
    uint8_t daysPerMonth = pgm_read_byte(daysInMonth + dt.ll - 1);
    if (leap && dt.ll == 2)
      ++daysPerMonth;
    if (days < daysPerMonth)
      break;
    days -= daysPerMonth;
  }
  
  dt.dd = days + 1; // Convert to 1-based day
  
  return dt;
}

/**
  Determine the day of the week using the Tomohiko Sakamoto's method

  @param year Year (e.g., 2023)
  @param month Month (1-12)
  @param day Day (1-31)
  @return Day of week, 0..6 (Sun..Sat)
*/
uint8_t NTP::getDOW(uint16_t year, uint8_t month, uint8_t day) {
  // Tomohiko Sakamoto's algorithm lookup table
  uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  
  // Adjust year for January and February
  year -= month < 3;
  
  // Apply Tomohiko Sakamoto's formula
  return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
}

/**
  Check if a specified date observes DST, according to
  the time changing rules in Europe:

    start: last Sunday in March,   0300 -> 0400
    end:   last Sunday in October, 0400 -> 0300

  @param year Year (e.g., 2023)
  @param month Month (1-12)
  @param day Day (1-31)
  @param hour Hour (0-23)
  @return bool DST yes or no
*/
bool NTP::dstCheck(uint16_t year, uint8_t month, uint8_t day, uint8_t hour) {
  // Get the last Sunday in March (DST start)
  uint8_t dayBegin = 31 - getDOW(year, 3, 31);
  
  // Get the last Sunday in October (DST end)
  uint8_t dayEnd = 31 - getDOW(year, 10, 31);
  
  // Compute the day where DST changes, since we are checking only
  // at 3 and 4 o'clock, this is enough
  return (month > 3   and month < 10) or                      // Summer months (April-September)
         (month == 3  and day >  dayBegin) or                 // After last Sunday in March
         (month == 3  and day == dayBegin and hour >= 3) or   // On last Sunday in March after 3AM
         (month == 10 and day <  dayEnd) or                   // Before last Sunday in October
         (month == 10 and day == dayEnd and hour < 4);        // On last Sunday in October before 4AM
}
