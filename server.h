/**
  server.h - Simple TCP server

  Copyright (c) 2017-2018 Costin STROIE <costinstroie@eridu.eu.org>

  This file is part of WiPS.
*/

#ifndef SERVER_H
#define SERVER_H

#define MAX_CLIENTS 4

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include "version.h"

class TCPServer: public WiFiServer {
  public:
    TCPServer(uint16_t serverPort);
    void init(const char *serverName, const char *welcome);
    int  check();
    void sendAll(char *buf);
    int  clients;
  private:
    int  port;
    char name[16];
    char wlcm[100];
    WiFiClient TCPClient[MAX_CLIENTS];
};

#endif /* SERVER_H */
