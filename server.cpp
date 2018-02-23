/**
  server.cpp - Simple TCP server

  Copyright (c) 2017-2018 Costin STROIE <costinstroie@eridu.eu.org>

  This file is part of WiFi_APRS_Tracker.
*/

#include <Arduino.h>
#include "server.h"

TCPServer::TCPServer(uint16_t serverPort) : WiFiServer(serverPort) {
  port = serverPort;
}

void TCPServer::init(const char *serverName) {
  // Configure mDNS
  MDNS.addService(serverName, "tcp", port);
  Serial.printf("$PMDNS,%s,%u,TCP,%u\r\n", serverName, MAX_CLIENTS, port);
  // Start the TCP server
  begin();
  setNoDelay(true);
}

/**
  Check the connected clients to NMEA server
*/
int TCPServer::check(const char *welcome) {
  int i;
  // Check if there are any new clients
  if (hasClient()) {
    for (i = 0; i < MAX_CLIENTS; i++) {
      // Find free or disconnected slots
      if ((not TCPClient[i]) or
          (not TCPClient[i].connected())) {
        if (TCPClient[i]) {
          TCPClient[i].stop();
          Serial.printf("$PSRVD,%u,%u,%u\r\n", port, clients, i);
        }
        TCPClient[i] = available();
        // Report connection
        IPAddress ip = TCPClient[i].remoteIP();
        Serial.printf("$PSRVC,%u,%u,%u,%d.%d.%d.%d\r\n", port, clients + 1, i, ip[0], ip[1], ip[2], ip[3]);
        // Welcome
        TCPClient[i].print(welcome);
        break;
      }
    }
    // No free or disconnected splots, so reject
    if (i == MAX_CLIENTS) {
      WiFiClient rejected = available();
      // Report connection
      IPAddress ip = rejected.remoteIP();
      Serial.printf("$PSRVR,%u,%u,%u,%d.%d.%d.%d\r\n", port, clients, i, ip[0], ip[1], ip[2], ip[3]);
      rejected.stop();
    }
  }

  clients = 0;
  // Flush client data while counting them
  for (i = 0; i < MAX_CLIENTS; i++) {
    if (TCPClient[i] and TCPClient[i].connected()) {
      clients++;
      if (TCPClient[i].available()) {
        // Flush the data
        TCPClient[i].flush();
      }
    }
  }
  return clients;
}

/**
  Send the NMEA sentence to all connected clients

  @param sentence the NMEA sentence
*/
void TCPServer::sendAll(char *buf, size_t len) {
  // Send the data to all connected clients
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (TCPClient[i] && TCPClient[i].connected()) {
      TCPClient[i].write(buf, len);
      yield();
    }
    else {
      // Stop the disconnected client
      if (TCPClient[i]) TCPClient[i].stop();
    }
  }
}

