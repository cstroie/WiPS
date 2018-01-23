/**
  ntp.cpp - Network Time Protocol

  Copyright (c) 2017-2018 Costin STROIE <costinstroie@eridu.eu.org>

  This file is part of WiFi_APRS_Tracker.
*/

#include "Arduino.h"
#include "ntp.h"

NTP::NTP() {
}

void NTP::init(const char *ntpServer, int ntpPort) {
  setServer(ntpServer, ntpPort);
  getSeconds();
}

void NTP::setServer(const char *ntpServer, int ntpPort) {
  port = ntpPort;
  strncpy(server, (char*)ntpServer, sizeof(server));
}

void NTP::setTZ(float tz) {
  TZ = tz;
}


/**
  Get current time as UNIX time (1970 epoch)

  @param sync flag to show whether network sync is to be performed
  @return current UNIX time
*/
unsigned long NTP::getSeconds(bool sync) {
  // Check if we need to sync
  if (millis() >= nextSync and sync) {
    // Try to get the time from Internet
    unsigned long utm = getNTP();
    if (utm == 0) {
      // Time sync has failed, sync again over one minute
      nextSync = millis() + 60000UL;
      valid = false;
    }
    else {
      // Compute the new time delta
      delta = utm - (millis() / 1000);
      // Time sync has succeeded, sync again in 8 hours
      nextSync = millis() + 28800000UL;
      valid = true;
      Serial.print(F("Network UNIX Time: 0x"));
      Serial.println(utm, 16);
    }
  }
  // Get current time based on uptime and time delta,
  // or just uptime for no time sync ever
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
  return ntpTime - 2208988800UL;            // convert to Unix time
}

/**
  Get the uptime

  @param buf character array to return the text to
  @param len the maximum length of the character array
  @return uptime in seconds
*/
unsigned long NTP::getUptime(char *buf, size_t len) {
  // Get the uptime in seconds
  unsigned long upt = millis() / 1000;
  // Compute days, hours, minutes and seconds
  int ss =  upt % 60;
  int mm = (upt % 3600) / 60;
  int hh = (upt % 86400L) / 3600;
  int dd =  upt / 86400L;
  // Create the formatted time
  if (dd == 1) snprintf_P(buf, len, PSTR("%d day, %02d:%02d:%02d"),  dd, hh, mm, ss);
  else         snprintf_P(buf, len, PSTR("%d days, %02d:%02d:%02d"), dd, hh, mm, ss);
  // Return the uptime in seconds
  return upt;
}
