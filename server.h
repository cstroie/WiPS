/**
  server.h - Simple TCP server

  Copyright (c) 2017-2018 Costin STROIE <costinstroie@eridu.eu.org>

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
