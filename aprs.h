/**
  aprs.h - Automated Position Reporting System

  Copyright (c) 2017-2023 Costin STROIE <costinstroie@eridu.eu.org>

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
#include <ESP8266WiFi.h>
#include "version.h"

// APRS constants
const char aprsPath[]     PROGMEM = ">APEWPS,TCPIP*:";
const char aprsTlmPARM[]  PROGMEM = "PARM.Vcc,RSSI,Heap,Acc,Spd,PROBE,FIX,FST,SLW,VCC,HT,RB,TM";
const char aprsTlmEQNS[]  PROGMEM = "EQNS.0,0.004,2.5,0,-1,0,0,256,0,0,1,0,0.0008,0,0";
const char aprsTlmUNIT[]  PROGMEM = "UNIT.V,dBm,Bytes,m,m/s,prb,on,fst,slw,bad,ht,rb,er";
const char aprsTlmBITS[]  PROGMEM = "BITS.11111111, ";

// Various constants
const char pstrD[]  PROGMEM = "%d";
const char pstrDD[] PROGMEM = "%d.%d";
const char pstrSP[] PROGMEM = " ";
const char pstrCL[] PROGMEM = ":";
const char pstrSL[] PROGMEM = "/";
//const char eol[]    PROGMEM = "\r\n";

class APRS {
  public:
    APRS();
    void init(const char *server, int port);
    void setServer(const char *server);
    void setServer(const char *server, int port);
    bool connect(const char *server, int port);
    bool connect();
    void stop();
    void setCallSign(const char *callsign = NULL);
    void setPassCode(const char *passcode);
    int  doPassCode(char *callsign);
    void setObjectName(const char *callsign = NULL);
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
    bool sendPosition(unsigned long utm, float lat, float lng, int cse = 0, int spd = 0, float alt = -1, const char *comment = NULL, const char *object = NULL);
    bool sendObjectPosition(unsigned long utm, float lat, float lng, int cse = 0, int spd = 0, float alt = -1, const char *comment = NULL);
    bool sendWeather(unsigned long utm, int temp, int hmdt, int pres, int srad);
    bool sendTelemetry(int p1, int p2, int p3, int p4, int p5, byte bits);
    bool sendTelemetrySetup();
    char aprsCallSign[10];
    char aprsPassCode[10];
    char aprsObjectNm[10];
    char aprsTlmBits        = B00000000;  // Telemetry bits
    int  aprsTlmSeq         = 999;        // Telemetry sequence mumber
    bool error;

  private:
    WiFiClient aprsClient;
    char  aprsPkt[250];
    char  aprsServer[50];             // CWOP APRS-IS server address to connect to
    int   aprsPort;                   // CWOP APRS-IS port
    char  aprsLocation[20];
    char  aprsTable;
    char  aprsSymbol;
    char  DEVICEID[6];                // t_hing A_rduino E_SP8266 W_iFi 4_
};

#endif /* APRS_H */
