/**
  aprs.cpp - Automated Position Reporting System

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

bool APRS::connect(const char *server, int port) {
  setServer(server, port);
  return connect();
}

bool APRS::connect() {
  bool result = aprsClient.connect(aprsServer, aprsPort);
  if (!result) error = true;
  return result;
}

void APRS::stop() {
  aprsClient.stop();
}

/**
  Set the APRS callsign and compute the passcode
*/
void APRS::setCallSign(const char *callsign) {
  if (callsign == NULL and callsign[0] != '\0')
    // Create an automatic callsign, using the ChipID
    snprintf_P(aprsCallSign, sizeof(aprsCallSign), PSTR("TK%04X"), ESP.getChipId() & 0xFFFF);
  else
    // Keep the callsign
    strncpy(aprsCallSign, (char*)callsign, sizeof(aprsCallSign));
  // Compute the passcode
  snprintf_P(aprsPassCode, sizeof(aprsPassCode), PSTR("%3d"), doPassCode((char*)aprsCallSign));
}

/**
  Set your own passcode (not necessary)
*/
void APRS::setPassCode(const char *passcode) {
  // Keep the passcode
  strncpy(aprsPassCode, (char*)passcode, sizeof(aprsPassCode));
}

/**
  Compute the passcode from the callsign
*/
int APRS::doPassCode(char *callsign) {
  char rootCall[10]; // need to copy call to remove ssid from parse
  char *p1 = rootCall;
  int hash;
  int i, len;
  char *ptr = rootCall;

  // Uppercase and find the bare callsign
  while ((*callsign != '-') && (*callsign != '\0'))
    *p1++ = toupper((int)(*callsign++));
  *p1 = '\0';

  // Initialize with the key value
  hash = 0x73e2;
  i = 0;
  len = (int)strlen(rootCall);

  // Loop through the string two bytes at a time
  while (i < len) {
    hash ^= (unsigned char)(*ptr++) << 8; // xor high byte with accumulated hash
    hash ^= (*ptr++);                     // xor low byte with accumulated hash
    i += 2;
  }
  // Mask off the high bit so number is always positive
  return (int)(hash & 0x7fff);
}

/**
  Set the APRS object name
*/
void APRS::setObjectName(const char *object) {
  if (object == NULL) {
    // Create an automatic object name, using the ChipID
    snprintf_P(aprsObjectNm, sizeof(aprsObjectNm), PSTR("WAT%06X"), ESP.getChipId());
  }
  else {
    // Keep the object name
    strncpy(aprsObjectNm, (char*)object, sizeof(aprsObjectNm));
  }
  // Pad with spaces
  for (int i = strlen(aprsObjectNm); i < sizeof(aprsObjectNm) - 1; i++)
    aprsObjectNm[i] = '_';
  // Make sure it ends with null
  aprsObjectNm[sizeof(aprsObjectNm) - 1] = '\0';
}

/**
  Send an APRS packet and, eventually, print it to serial line

  @param *pkt the packet to send
*/
bool APRS::send(const char *pkt) {
  bool result;
  if (result = aprsClient.connected()) {
    int plen = strlen(pkt);
#ifndef DEVEL
    // Write the packet and check the number of bytes written
    if (aprsClient.write(pkt) != plen) error = true;
    yield();
#endif
#ifdef DEBUG
    Serial.printf_P(PSTR("$PAPRS,%03d,%s"), plen, pkt);
#endif
  }
  else
    error = true;
  return result & (~error);
}

bool APRS::send() {
  return send(aprsPkt);
}

/**
  Return time in zulu APRS format: HHMMSSh

  @param utm UNIX time
  @param *buf the buffer to return the time to
  @param len the buffer length
*/
void APRS::time(unsigned long utm, char *buf, size_t len) {
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
  // Store the APRS callsign and passkey
  strncpy(aprsCallSign, (char*)callsign, sizeof(aprsCallSign));
  strncpy(aprsPassCode, (char*)passcode, sizeof(aprsPassCode));
  return authenticate();
}
/**
  Send APRS authentication data
  user FW0690 pass -1 vers WxSta 0.2"
*/
bool APRS::authenticate() {
  bool result = false;
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
    // Send the credentials
    if (send(aprsPkt)) {
      while (aprsClient.connected() and (not result))
        // Check the response
        result = aprsClient.findUntil("verified", "\r");
      /*
        int rlen = aprsClient.readBytesUntil('\n', aprsPkt, sizeof(aprsPkt));
        aprsPkt[rlen] = '\0';
        Serial.printf_P(PSTR("$PAPRS,%03d,%s\r\n"), rlen, aprsPkt);
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
  }
  return result;
}

/**
  Set the symbols table and the symbol
*/
void APRS::setSymbol(const char table, const char symbol) {
  aprsTable  = table;
  aprsSymbol = symbol;
}

/**
  Send APRS status
  FW0690>APRS,TCPIP*:>Fine weather

  @param message the status message to send
*/
bool APRS::sendStatus(const char *message) {
  // Send only if the message is not empty
  if (message[0] != '\0') {
    // Send the APRS packet
    strcpy_P(aprsPkt, aprsCallSign);
    strcat_P(aprsPkt, aprsPath);
    strcat_P(aprsPkt, PSTR(">"));
    strcat  (aprsPkt, message);
    strcat_P(aprsPkt, eol);
    return send(aprsPkt);
  }
}

/**
  Send an APRS message

  @param dest the message destination, own call sign if empty
  @param title the message title, if not empty
  @param message the message body
*/
bool APRS::sendMessage(const char *dest, const char *title, const char *message) {
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
  return send(aprsPkt);
}

/**
  Create the coordinates in APRS format, also setting the symbol
*/
void APRS::coordinates(char *buf, float lat, float lng) {
  // Compute integer and fractional coordinates
  int latDD = (int)(abs(lat));
  int latMM = (int)((fabs(lat) - latDD) * 6000);
  int lngDD = (int)(abs(lng));
  int lngMM = (int)((fabs(lng) - lngDD) * 6000);
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
bool APRS::sendPosition(unsigned long utm, float lat, float lng, int cse, int spd, float alt, const char *comment, const char *object) {
  // Local buffer
  const int bufSize = 20;
  char buf[bufSize] = "";

  // Compose the APRS packet
  strcpy_P(aprsPkt, aprsCallSign);
  strcat_P(aprsPkt, aprsPath);

  // Object
  if (object != NULL) {
    strcat_P(aprsPkt, PSTR(";"));
    strcat(aprsPkt, object);
    strcat_P(aprsPkt, PSTR("*"));
    time(utm, buf, bufSize);
    strncat(aprsPkt, buf, bufSize);
  }
  else {
    strcat_P(aprsPkt, PSTR("!"));
  }

  // Coordinates in APRS format
  setSymbol('/', '>');
  setLocation(lat, lng);
  strcat_P(aprsPkt, aprsLocation);
  // Course and speed
  if (spd >= 0 and cse >= 0) {
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
    //if (PROBE) strcat_P(aprsPkt, PSTR(" [PROBE]"));
  }
  strcat_P(aprsPkt, eol);
  return send(aprsPkt);
}

/**
  Send APRS object position and altitude
  FW0690>APRS,TCPIP*:!DDMM.hhN/DDDMM.hhW$/A=000000 comment

  @param comment the comment to append
*/
bool APRS::sendObjectPosition(unsigned long utm, float lat, float lng, int cse, int spd, float alt, const char *comment) {
  return sendPosition(utm, lat, lng, cse, spd, alt, comment, aprsObjectNm);
}

/**
  Send APRS weather data, then try to get the forecast
  FW0690>APRS,TCPIP*:@152457h4427.67N/02608.03E_.../...g...t044h86b10201L001WxSta

  @param temp temperature in Fahrenheit
  @param hmdt relative humidity in percents
  @param pres athmospheric pressure (QNH) in dPa
  @param srad solar radiation in W/m^2
*/
bool APRS::sendWeather(unsigned long utm, int temp, int hmdt, int pres, int srad) {
  const int bufSize = 10;
  char buf[bufSize] = "";
  // Weather report
  strcpy_P(aprsPkt, aprsCallSign);
  strcat_P(aprsPkt, aprsPath);
  strcat_P(aprsPkt, PSTR("@"));
  time(utm, buf, bufSize);
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
  return send(aprsPkt);
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
bool APRS::sendTelemetry(int p1, int p2, int p3, int p4, int p5, byte bits) {
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
  return send(aprsPkt);
}

/**
  Send APRS telemetry setup
*/
bool APRS::sendTelemetrySetup() {
  bool result = true;
  // The object's call sign has to be padded with spaces until 9 chars long
  const int padSize = 9;
  char padCallSign[padSize] = " ";
  // Copy the call sign or object name
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
  result &= send(aprsPkt);
  // Trim the packet
  aprsPkt[lenHeader] = '\0';
  // Equations
  strcat_P(aprsPkt, aprsTlmEQNS);
  strcat_P(aprsPkt, eol);
  result &= send(aprsPkt);
  // Trim the packet
  aprsPkt[lenHeader] = '\0';
  // Units
  strcat_P(aprsPkt, aprsTlmUNIT);
  strcat_P(aprsPkt, eol);
  result &= send(aprsPkt);
  // Trim the packet
  aprsPkt[lenHeader] = '\0';
  // Bit sense and project name
  strcat_P(aprsPkt, aprsTlmBITS);
  strcat(aprsPkt, NODENAME);
  strcat_P(aprsPkt, pstrSL);
  strcat(aprsPkt, VERSION);
  strcat_P(aprsPkt, eol);
  result &= send(aprsPkt);
  return result;
}
