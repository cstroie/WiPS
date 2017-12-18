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

// Device specific constants
const char NODENAME[] = "WiFiTrk";
const char nodename[] = "wifitrk";
const char VERSION[]  = "0.2.2";


// APRS constants
const char aprsPath[]     PROGMEM = ">CBAPRS,TCPIP*:";
const char aprsTlmPARM[]  PROGMEM = "PARM.Vcc,RSSI,Heap,IRed,Visb,PROBE,ATMO,LUX,SAT,VCC,HT,RB,TM";
const char aprsTlmEQNS[]  PROGMEM = "EQNS.0,0.004,2.5,0,-1,0,0,256,0,0,256,0,0,256,0";
const char aprsTlmUNIT[]  PROGMEM = "UNIT.V,dBm,Bytes,units,units,prb,on,on,sat,bad,ht,rb,er";
const char aprsTlmBITS[]  PROGMEM = "BITS.10011111, ";

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
    bool send(const char *pkt);
    bool send();
    void time(char *buf, size_t len);
    bool authenticate(const char *callsign, const char *passcode);
    void setSymbol(const char table, const char symbol);
    void sendStatus(const char *message);
    void sendMessage(const char *dest, const char *title, const char *message);
    void coordinates(char *buf, float lat, float lng, char table, char symbol);
    void coordinates(char *buf, float lat, float lng);
    void setLocation(float lat, float lng);
    void sendPosition(float lat, float lng, int cse = 0, int spd = 0, float alt = -1, const char *comment = NULL);
    void sendWeather(int temp, int hmdt, int pres, int srad);
    void sendTelemetry(int p1, int p2, int p3, int p4, int p5, byte bits);
    void sendTelemetrySetup();
  private:
    NTP  ntp;
    WiFiClient aprsClient;
    char  aprsPkt[100];
    char  aprsServer[50];             // CWOP APRS-IS server address to connect to
    int   aprsPort;                   // CWOP APRS-IS port
    char  aprsLocation[20];
    char  aprsCallSign[10];
    char  aprsPassCode[10];
    char  aprsTable;
    char  aprsSymbol;
    int   aprsTlmSeq        = 0;      // Telemetry sequence mumber
    bool  PROBE             = false;  // True if the station is being probed
    char  DEVICEID[6];                // t_hing A_rduino E_SP8266 W_iFi 4_

};

#endif /* APRS_H */
