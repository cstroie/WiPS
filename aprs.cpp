/**
  aprs.cpp - Automated Position Reporting System

  Copyright (c) 2017 Costin STROIE <costinstroie@eridu.eu.org>

  This file is part of WiFi_APRS_Tracker.
*/

#include "Arduino.h"
#include "aprs.h"

const char eol[]    PROGMEM = "\r\n";

APRS::APRS() {
}

void APRS::init(const char *server, int port) {
  setServer(server, port);
}

void APRS::setServer(const char *server) {
  strncpy(aprsServer, (char*)server, sizeof(aprsServer));
}

void APRS::setServer(const char *server, int port) {
  aprsPort = port;
  setServer(server);
}

void APRS::setNTP(NTP &n) {
  ntp = n;
}

bool APRS::connect(const char *server, int port) {
  setServer(server, port);
  return connect();
}

bool APRS::connect() {
  return aprsClient.connect(aprsServer, aprsPort);
}

void APRS::stop() {
  aprsClient.stop();
}

/**
  Send an APRS packet and, eventually, print it to serial line

  @param *pkt the packet to send
*/
bool APRS::send(const char *pkt) {
  bool result;
  if (result = aprsClient.connected()) {
#ifndef DEVEL
    aprsClient.print(pkt);
    yield();
#endif
#ifdef DEBUG
    Serial.print(F("[APRS  >] "));
    Serial.print(pkt);
#endif
  }
  return result;
}

bool APRS::send() {
  return send(aprsPkt);
}

/**
  Return time in zulu APRS format: HHMMSSh

  @param *buf the buffer to return the time to
  @param len the buffer length
*/
void APRS::time(char *buf, size_t len) {
  // Get the time, but do not open a connection to server
  unsigned long utm = ntp.timeUNIX(false);
  // Compute hour, minute and second
  int hh = (utm % 86400L) / 3600;
  int mm = (utm % 3600) / 60;
  int ss =  utm % 60;
  // Return the formatted time
  snprintf_P(buf, len, PSTR("%02d%02d%02dh"), hh, mm, ss);
}

/**
  Send APRS authentication data
  user FW0690 pass -1 vers WxSta 0.2"
*/
bool APRS::authenticate(const char *callsign, const char *passcode) {
  bool result = false;
  // Store the APRS callsign and passkey
  strncpy(aprsCallSign, (char*)callsign, sizeof(aprsCallSign));
  strncpy(aprsPassCode, (char*)passcode, sizeof(aprsPassCode));
  // Only authenticate if connected
  if (aprsClient.connected()) {
    // Send the credentials
    strcpy_P(aprsPkt, PSTR("user "));
    strcat_P(aprsPkt, aprsCallSign);
    strcat_P(aprsPkt, PSTR(" pass "));
    strcat_P(aprsPkt, aprsPassCode);
    strcat_P(aprsPkt, PSTR(" vers "));
    strcat  (aprsPkt, NODENAME);
    strcat_P(aprsPkt, pstrSP);
    strcat  (aprsPkt, VERSION);
    strcat_P(aprsPkt, eol);
    // Flush the welcome message
    aprsClient.flush();
    // Send the credentials
    send(aprsPkt);
    // Check the one-line response
    result = aprsClient.findUntil("verified", "\r");
    /*
      int rlen = aprsClient.readBytesUntil('\n', aprsPkt, sizeof(aprsPkt));
      aprsPkt[rlen] = '\0';
      Serial.print("[APRS<] "); Serial.print(aprsPkt);
      // Tokenize the response
      char* pch;
      pch = strtok(aprsPkt, " ");
      while (pch != NULL) {
      if (strcmp(pch, "verified") == 0) {
        result = true;
        break;
      }
      pch = strtok(NULL, " ");
      }
    */
  }
  return result;
}

void APRS::setSymbol(const char table, const char symbol) {
  aprsTable  = table;
  aprsSymbol = symbol;
}

/**
  Send APRS status
  FW0690>APRS,TCPIP*:>Fine weather

  @param message the status message to send
*/
void APRS::sendStatus(const char *message) {
  // Send only if the message is not empty
  if (message[0] != '\0') {
    // Send the APRS packet
    strcpy_P(aprsPkt, aprsCallSign);
    strcat_P(aprsPkt, aprsPath);
    strcat_P(aprsPkt, PSTR(">"));
    strcat  (aprsPkt, message);
    strcat_P(aprsPkt, eol);
    send(aprsPkt);
  }
}

/**
  Send an APRS message

  @param dest the message destination, own call sign if empty
  @param title the message title, if not empty
  @param message the message body
*/
void APRS::sendMessage(const char *dest, const char *title, const char *message) {
  // The object's call sign has to be padded with spaces until 9 chars long
  const int padSize = 9;
  char padCallSign[padSize] = " ";
  // Check if the destination is specified
  if (dest == NULL) strcpy_P(padCallSign, aprsCallSign);  // Copy the own call sign from PROGMEM
  else              strncpy(padCallSign, dest, padSize);  // Use the specified destination
  // Pad with spaces, then make sure it ends with '\0'
  for (int i = strlen(padCallSign); i < padSize; i++)
    padCallSign[i] = ' ';
  padCallSign[padSize] = '\0';
  // Create the header of the packet
  strcpy_P(aprsPkt, aprsCallSign);
  strcat_P(aprsPkt, aprsPath);
  strcat_P(aprsPkt, pstrCL);
  // Message destination
  strncat(aprsPkt, padCallSign, padSize);
  strcat_P(aprsPkt, pstrCL);
  // Message title
  if (title != NULL) strncat(aprsPkt, title, 8);
  // The body of the message, maximum size is 45, including the title
  strncat(aprsPkt, message, 40);
  strcat_P(aprsPkt, eol);
  send(aprsPkt);
}

/**
  Create the coordinates in APRS format, also setting the symbol
*/
void APRS::coordinates(char *buf, float lat, float lng) {
  // Compute integer and fractional coordinates
  int latDD = abs((int)lat);
  int latMM = (int)((abs(lat) - latDD) * 6000);
  int lngDD = abs((int)lng);
  int lngMM = (int)((abs(lng) - lngDD) * 6000);
  // Return the formatted coordinates
  sprintf_P(buf, PSTR("%02d%02d.%02d%c%c%03d%02d.%02d%c%c"),
            latDD, latMM / 100, latMM % 100, lat >= 0 ? 'N' : 'S', aprsTable,
            lngDD, lngMM / 100, lngMM % 100, lng >= 0 ? 'E' : 'W', aprsSymbol);
}

void APRS::coordinates(char *buf, float lat, float lng, char table, char symbol) {
  // Set the symbol
  setSymbol(table, symbol);
  // Compute the coordinates
  coordinates(buf, lat, lng);
}

void APRS::setLocation(float lat, float lng) {
  coordinates(aprsLocation, lat, lng);
}

/**
  Send APRS position and altitude
  FW0690>APRS,TCPIP*:!DDMM.hhN/DDDMM.hhW$/A=000000 comment

  @param comment the comment to append
*/
void APRS::sendPosition(float lat, float lng, int cse, int spd, float alt, const char *comment) {
  // Local buffer
  const int bufSize = 20;
  char buf[bufSize] = "";

  // Compose the APRS packet
  strcpy_P(aprsPkt, aprsCallSign);
  strcat_P(aprsPkt, aprsPath);
  strcat_P(aprsPkt, PSTR("!"));
  // Coordinates in APRS format
  setSymbol('/', '>');
  setLocation(lat, lng);
  strcat_P(aprsPkt, aprsLocation);
  // Course and speed
  if (spd > 0) {
    snprintf_P(buf, bufSize, PSTR("%03d/%03d"), cse, spd);
    strncat(aprsPkt, buf, bufSize);
  }
  // Altitude
  if (alt >= 0) {
    strcat_P(aprsPkt, PSTR("/A="));
    sprintf_P(buf, PSTR("%06d"), (long)(alt * 3.28084));
    strncat(aprsPkt, buf, bufSize);
  }
  //strcat_P(aprsPkt, pstrSP);
  // Comment
  if (comment != NULL)
    strcat(aprsPkt, comment);
  else {
    strcat(aprsPkt, NODENAME);
    strcat_P(aprsPkt, pstrSL);
    strcat(aprsPkt, VERSION);
    if (PROBE) strcat_P(aprsPkt, PSTR(" [PROBE]"));
  }
  strcat_P(aprsPkt, eol);
  send(aprsPkt);
}

/**
  Send APRS weather data, then try to get the forecast
  FW0690>APRS,TCPIP*:@152457h4427.67N/02608.03E_.../...g...t044h86b10201L001WxSta

  @param temp temperature in Fahrenheit
  @param hmdt relative humidity in percents
  @param pres athmospheric pressure (QNH) in dPa
  @param srad solar radiation in W/m^2
*/
void APRS::sendWeather(int temp, int hmdt, int pres, int srad) {
  const int bufSize = 10;
  char buf[bufSize] = "";
  // Weather report
  strcpy_P(aprsPkt, aprsCallSign);
  strcat_P(aprsPkt, aprsPath);
  strcat_P(aprsPkt, PSTR("@"));
  time(buf, bufSize);
  strncat(aprsPkt, buf, bufSize);
  setSymbol('/', '_');
  strcat_P(aprsPkt, aprsLocation);
  strcat_P(aprsPkt, PSTR("_"));
  // Wind (unavailable)
  strcat_P(aprsPkt, PSTR(".../...g..."));
  // Temperature
  if (temp >= -460) { // 0K in F
    snprintf_P(buf, bufSize, PSTR("t%03d"), temp);
    strncat(aprsPkt, buf, bufSize);
  }
  else
    strcat_P(aprsPkt, PSTR("t..."));
  // Humidity
  if (hmdt >= 0) {
    if (hmdt == 100)
      strcat_P(aprsPkt, PSTR("h00"));
    else {
      snprintf_P(buf, bufSize, PSTR("h%02d"), hmdt);
      strncat(aprsPkt, buf, bufSize);
    }
  }
  // Athmospheric pressure
  if (pres >= 0) {
    snprintf_P(buf, bufSize, PSTR("b%05d"), pres);
    strncat(aprsPkt, buf, bufSize);
  }
  // Solar radiation, if valid
  if (srad >= 0) {
    if (srad < 1000) snprintf_P(buf, bufSize, PSTR("L%03d"), srad);
    else             snprintf_P(buf, bufSize, PSTR("l%03d"), srad - 1000);
    strncat(aprsPkt, buf, bufSize);
  }
  // Comment (device name)
  strcat_P(aprsPkt, DEVICEID);
  strcat_P(aprsPkt, eol);
  send(aprsPkt);
}


/**
  Send APRS telemetry and, periodically, send the telemetry setup
  FW0690>APRS,TCPIP*:T#517,173,062,213,002,000,00000000

  @param vcc voltage
  @param rssi wifi level
  @param heap free memory
  @param luxVis raw visible illuminance
  @param luxIrd raw infrared illuminance
  @bits digital inputs
*/
void APRS::sendTelemetry(int p1, int p2, int p3, int p4, int p5, byte bits) {
  // Increment the telemetry sequence number, reset it if exceeds 999
  if (++aprsTlmSeq > 999) aprsTlmSeq = 0;
  // Send the telemetry setup if the sequence number is 0
  if (aprsTlmSeq == 0) sendTelemetrySetup();
  // Compose the APRS packet
  const int bufSize = 40;
  char buf[bufSize] = "";
  strcpy_P(aprsPkt, aprsCallSign);
  strcat_P(aprsPkt, aprsPath);
  snprintf_P(buf, bufSize, PSTR("T#%03d,%03d,%03d,%03d,%03d,%03d,"), aprsTlmSeq, p1, p2, p3, p4, p5);
  strncat(aprsPkt, buf, bufSize);
  itoa(bits, buf, 2);
  strncat(aprsPkt, buf, bufSize);
  strcat_P(aprsPkt, eol);
  send(aprsPkt);
}

/**
  Send APRS telemetry setup
*/
void APRS::sendTelemetrySetup() {
  // The object's call sign has to be padded with spaces until 9 chars long
  const int padSize = 9;
  char padCallSign[padSize] = " ";
  // Copy the call sign from PROGMEM
  strcpy_P(padCallSign, aprsCallSign);
  // Pad with spaces, then make sure it ends with '\0'
  for (int i = strlen(padCallSign); i < padSize; i++)
    padCallSign[i] = ' ';
  padCallSign[padSize] = '\0';
  // Create the common header of the packet
  strcpy_P(aprsPkt, aprsCallSign);
  strcat_P(aprsPkt, aprsPath);
  strcat_P(aprsPkt, pstrCL);
  strncat(aprsPkt, padCallSign, padSize);
  strcat_P(aprsPkt, pstrCL);
  // At this point, keep the size of the packet header,
  // so we can trim the packet and append to it again
  int lenHeader = strlen(aprsPkt);
  // Parameter names
  strcat_P(aprsPkt, aprsTlmPARM);
  strcat_P(aprsPkt, eol);
  send(aprsPkt);
  // Trim the packet
  aprsPkt[lenHeader] = '\0';
  // Equations
  strcat_P(aprsPkt, aprsTlmEQNS);
  strcat_P(aprsPkt, eol);
  send(aprsPkt);
  // Trim the packet
  aprsPkt[lenHeader] = '\0';
  // Units
  strcat_P(aprsPkt, aprsTlmUNIT);
  strcat_P(aprsPkt, eol);
  send(aprsPkt);
  // Trim the packet
  aprsPkt[lenHeader] = '\0';
  // Bit sense and project name
  strcat_P(aprsPkt, aprsTlmBITS);
  strcat(aprsPkt, NODENAME);
  strcat_P(aprsPkt, pstrSL);
  strcat(aprsPkt, VERSION);
  strcat_P(aprsPkt, eol);
  send(aprsPkt);
}

