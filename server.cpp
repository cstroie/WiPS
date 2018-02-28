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

/**
  Initialize the server

  @param serverName the name the server is known as
*/
void TCPServer::init(const char *serverName, const char *welcome) {
  // Keep the server name
  strncpy(name, serverName, sizeof(name));
  name[sizeof(name) - 1] = '\0';
  // Keep the welcome message
  strncpy(wlcm, serverName, sizeof(wlcm));
  wlcm[sizeof(wlcm) - 1] = '\0';
  // Configure mDNS
  MDNS.addService((const char*)name, "tcp", (int)port);
  Serial.printf("$PMDNS,%s,%u,TCP,%u\r\n", name, MAX_CLIENTS, port);
  // Start the TCP server
  begin();
  setNoDelay(true);
}

/**
  Check the connected clients

  @param welcome the welcome message to send to the new clients
*/
int TCPServer::check() {
  int i;
  // Check if there are any new clients
  if (hasClient()) {
    for (i = 0; i < MAX_CLIENTS; i++) {
      // Find free or disconnected servers
      if ((not TCPClient[i]) or (not TCPClient[i].connected())) {
        if (TCPClient[i]) {
          // Force disconnecting the stalled server
          TCPClient[i].stop();
          clients--;
          Serial.printf("$PSRVD,%s,%u,%u\r\n", name, clients, i);
        }
        // Create a new server connection
        TCPClient[i] = available();
        clients++;
        // Report the new connection
        IPAddress ip = TCPClient[i].remoteIP();
        Serial.printf("$PSRVC,%s,%u,%u,%d.%d.%d.%d\r\n", name, clients, i, ip[0], ip[1], ip[2], ip[3]);
        // Send the welcome message
        TCPClient[i].print(wlcm);
        break;
      }
    }
    // No free or disconnected servers, so reject
    if (i == MAX_CLIENTS) {
      WiFiClient rejected = available();
      // Report connection
      IPAddress ip = rejected.remoteIP();
      Serial.printf("$PSRVR,%s,%u,%u,%d.%d.%d.%d\r\n", name, clients, i, ip[0], ip[1], ip[2], ip[3]);
      rejected.stop();
    }
  }

  // Flush client data while counting them
  clients = 0;
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
  Send the message to all connected clients

  @param buf the message to send to all clients
*/
void TCPServer::sendAll(char *buf) {
  // Send the data to all connected clients
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (TCPClient[i] && TCPClient[i].connected()) {
      TCPClient[i].print(buf);
      yield();
    }
    else {
      // Stop the server if the client is disconnected
      if (TCPClient[i]) TCPClient[i].stop();
    }
  }
}

