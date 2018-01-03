/**
  aprs.h - Automated Position Reporting System

  Copyright (c) 2017 Costin STROIE <costinstroie@eridu.eu.org>

  This file is part of WiFi_APRS_Tracker.
*/

#ifndef APRS_H
#define APRS_H

#define DEBUG

#define PORT  14580

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include "ntp.h"
#include "version.h"

// APRS constants
const char aprsPath[]     PROGMEM = ">CBAPRS,TCPIP*:";
const char aprsTlmPARM[]  PROGMEM = "PARM.Vcc,RSSI,Heap,Acc,Spd,PROBE,Fix,Fst,Slw,VCC,HT,RB,TM";
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
    void setNTP(NTP &n);
    bool connect(const char *server, int port);
    bool connect();
    void stop();
    void setCallSign(const char *callsign);
    void setPassCode(const char *passcode);
    int  doPassCode(char *callsign);
    void setObjectName();
    bool send(const char *pkt);
    bool send();
    void time(char *buf, size_t len);
    bool authenticate(const char *callsign, const char *passcode);
    bool authenticate();
    void setSymbol(const char table, const char symbol);
    bool sendStatus(const char *message);
    bool sendMessage(const char *dest, const char *title, const char *message);
    void coordinates(char *buf, float lat, float lng, char table, char symbol);
    void coordinates(char *buf, float lat, float lng);
    void setLocation(float lat, float lng);
    bool sendPosition(float lat, float lng, int cse = 0, int spd = 0, float alt = -1, const char *comment = NULL, const char *object = NULL);
    bool sendObjectPosition(float lat, float lng, int cse = 0, int spd = 0, float alt = -1, const char *comment = NULL);
    bool sendWeather(int temp, int hmdt, int pres, int srad);
    bool sendTelemetry(int p1, int p2, int p3, int p4, int p5, byte bits, const char *object = NULL);
    bool sendTelemetrySetup(const char *object = NULL);
    char aprsCallSign[10];
    char aprsPassCode[10];
    char aprsObjectNm[10];
    char aprsTlmBits = B00000000;     // Telemetry bits

  private:
    WiFiClient aprsClient;
    NTP   ntp;
    char  aprsPkt[250];
    char  aprsServer[50];             // CWOP APRS-IS server address to connect to
    int   aprsPort;                   // CWOP APRS-IS port
    char  aprsLocation[20];
    char  aprsTable;
    char  aprsSymbol;
    int   aprsTlmSeq        = 999;    // Telemetry sequence mumber
    char  DEVICEID[6];                // t_hing A_rduino E_SP8266 W_iFi 4_
};

#endif /* APRS_H */
