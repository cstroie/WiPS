/**
  server.h - Simple TCP server

  Copyright (c) 2017-2018 Costin STROIE <costinstroie@eridu.eu.org>

  This file is part of WiFi_APRS_Tracker.
*/

#ifndef SERVER_H
#define SERVER_H

#define MAX_CLIENTS 2

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#include "version.h"



class TCPServer: public WiFiServer {
  public:
    TCPServer(uint16_t serverPort);
    void init(const char *serverName);
    void check(const char *welcome);
    void sendAll(char *buf, size_t len);
  private:
    int  port;
    WiFiClient TCPClient[MAX_CLIENTS];
};

#endif /* SERVER_H */
