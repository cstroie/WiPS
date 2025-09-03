/**
  aprs.h - Automated Position Reporting System
         
  This library provides functionality for connecting to APRS-IS servers,
  authenticating with callsign and passcode, and sending various types
  of APRS packets including position reports, weather data, telemetry,
  status messages, and object positions.

  Features:
  - Secure and insecure APRS-IS connections
  - Automatic passcode generation from callsign
  - Position reporting with timestamp, coordinates, course, speed, altitude
  - Telemetry data transmission with sequence numbering
  - Weather data reporting
  - Status and message transmission
  - Custom object naming and symbol selection

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

#ifndef APRS_H
#define APRS_H

#define DEBUG

#include "Arduino.h"
#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif
#include "version.h"

// APRS constants
static const char aprsPath[]     PROGMEM = ">WIDE1-1,TCPIP*:";
static const char aprsTlmPARM[]  PROGMEM = "PARM.Vcc,RSSI,Heap,Acc,Spd,PROBE,FIX,FST,SLW,VCC,HT,RB,TM";
static const char aprsTlmEQNS[]  PROGMEM = "EQNS.0,0.004,2.5,0,-1,0,0,256,0,0,1,0,0.0008,0,0";
static const char aprsTlmUNIT[]  PROGMEM = "UNIT.V,dBm,Bytes,m,m/s,prb,on,fst,slw,bad,ht,rb,er";
static const char aprsTlmBITS[]  PROGMEM = "BITS.11111111, ";

// Various constants
static const char pstrD[]  PROGMEM = "%d";
static const char pstrDD[] PROGMEM = "%d.%d";
static const char pstrSP[] PROGMEM = " ";
static const char pstrCL[] PROGMEM = ":";
static const char pstrSL[] PROGMEM = "/";
static const char eol[]    PROGMEM = "\r\n";

class APRS {
  public:
    APRS();
    void init(const char *server, int port);
    void setServer(const char *server);
    void setServer(const char *server, int port);
    bool connect(const char *server, int port);
    bool connect();
    void stop();
    void setCallSign(const char *callsign);
    void setPassCode(const char *passcode);
    int  doPassCode(char *callsign);
    void setObjectName(const char *object);
    bool send(const char *pkt);
    bool send();
    void time(unsigned long utm, char *buf, size_t len);
    bool authenticate(const char *callsign, const char *passcode);
    bool authenticate();
    void setSymbol(const char table, const char symbol);
    bool sendStatus(const char *message);
    bool sendMessage(const char *dest, const char *title, const char *message);
    void coordinates(char *buf, float lat, float lng, char table, char symbol);
    void coordinates(char *buf, float lat, float lng);
    void setLocation(float lat, float lng);
    bool sendPosition(unsigned long utm, float lat, float lng, int cse, int spd, float alt, const char *comment, const char *object);
    bool sendObjectPosition(unsigned long utm, float lat, float lng, int cse, int spd, float alt, const char *comment);
    bool sendWeather(unsigned long utm, int temp, int hmdt, int pres, int srad);
    bool sendTelemetry(int p1, int p2, int p3, int p4, int p5, byte bits);
    bool sendTelemetrySetup();
    char aprsCallSign[10];
    char aprsPassCode[10];
    char aprsObjectNm[10];
    char aprsTlmBits        = B00000000;  ///< Telemetry status bits
    int  aprsTlmSeq         = 999;        ///< Telemetry sequence number (0-999)
    bool error;                          ///< Error flag for connection/packet issues

  private:
    WiFiClient aprsClient;               ///< TCP client for APRS-IS connection
    char  aprsPkt[250];                  ///< Buffer for composing APRS packets
    char  aprsServer[50];                ///< APRS-IS server address
    int   aprsPort;                      ///< APRS-IS server port
    char  aprsLocation[20];              ///< Formatted APRS coordinates
    char  aprsTable;                     ///< APRS symbol table identifier
    char  aprsSymbol;                    ///< APRS symbol code
    char  DEVICEID[6];                   ///< Device identifier for weather reports
};

#endif /* APRS_H */
