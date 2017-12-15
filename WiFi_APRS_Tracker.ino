/**
  WiFi APRS Tracker - Automated Position Reporting System based on Wifi
                      geolocation, using Mozilla Location Services

  Copyright (c) 2017 Costin STROIE <costinstroie@eridu.eu.org>

  This file is part of WiFi_APRS_Tracker.
*/


// User settings
#include "config.h"

// WiFi
#include <ESP8266WiFi.h>
#include <WiFiManager.h>

// Mozilla Location Services
#include "mls.h"
MLS mls;

// Network Time Protocol
#include "ntp.h"
NTP ntp;

// APRS
#include "aprs.h"
APRS aprs;


unsigned long rpNextTime  = 0;        // Next time to report
unsigned long rpDelay     = 30000UL;  // Delay between reporting
unsigned long rpLastTime  = 0;        // Last report time

/**
  Convert IPAddress to char array
*/
char charIP(const IPAddress ip, char *buf, size_t len, boolean pad = false) {
  if (pad) snprintf_P(buf, len, PSTR("%3d.%3d.%3d.%3d"), ip[0], ip[1], ip[2], ip[3]);
  else     snprintf_P(buf, len, PSTR("%d.%d.%d.%d"),     ip[0], ip[1], ip[2], ip[3]);
}

/**
  Display the WiFi parameters
*/
void showWiFi() {
  if (WiFi.isConnected()) {
    char ipbuf[16] = "";
    char gwbuf[16] = "";
    char nsbuf[16] = "";

    // Get the IPs as char arrays
    charIP(WiFi.localIP(),   ipbuf, sizeof(ipbuf), true);
    charIP(WiFi.gatewayIP(), gwbuf, sizeof(gwbuf), true);
    charIP(WiFi.dnsIP(),     nsbuf, sizeof(nsbuf), true);

    // Print
    Serial.println();
    Serial.print(F("WiFi connected to "));
    Serial.print(WiFi.SSID());
    Serial.print(F(" on channel "));
    Serial.print(WiFi.channel());
    Serial.print(F(", RSSI "));
    Serial.print(WiFi.RSSI());
    Serial.println(F(" dBm."));
    Serial.print(F(" IP : "));
    Serial.println(ipbuf);
    Serial.print(F(" GW : "));
    Serial.println(gwbuf);
    Serial.print(F(" DNS: "));
    Serial.println(nsbuf);
    Serial.println();
  }
  else {
    Serial.println();
    Serial.print(F("WiFi not connected"));
  }
}

/**
  Feedback notification when SoftAP is started
*/
void wifiCallback(WiFiManager *wifiMgr) {
  Serial.print(F("Connect to "));
  Serial.println(wifiMgr->getConfigPortalSSID());
}

/**
  Main Arduino setup function
*/
void setup() {
  // Init the serial communication
  Serial.begin(115200);
  Serial.println();
  Serial.print(NODENAME);
  Serial.print(" ");
  Serial.print(VERSION);
  Serial.print(" ");
  Serial.println(__DATE__);

  // Set the host name
  WiFi.hostname(NODENAME);
  // Try to connect to WiFi
  WiFiManager wifiManager;
  wifiManager.setTimeout(300);
  wifiManager.setAPCallback(wifiCallback);
  wifiManager.autoConnect(NODENAME);
  while (!wifiManager.autoConnect(NODENAME))
    delay(1000);

  // Connected
  showWiFi();

  // Configure NTP
  ntp.init(NTP_SERVER);

  // Configure APRS
  aprs.init(APRS_SERVER, APRS_PORT);
  aprs.setNTP(ntp);
}

/**
  Main Arduino loop
*/
void loop() {
  unsigned long now = millis();
  // Check if we should report
  if (now >= rpNextTime) {
    // Repeat after a delay
    rpNextTime = now + rpDelay;
    // Log the uptime
    Serial.print("["); Serial.print(now / 1000); Serial.println("]");

    // Scan the WiFi access points
    Serial.print(F("Scanning WiFi networks... "));
    int found = mls.wifiScan();
    Serial.print(found);
    Serial.println(" found");

    // Get the coordinates
    if (found > 0) {
      Serial.print(F("Geolocating... "));
      int acc = mls.geoLocation();
      if (acc >= 0 and acc <= GEO_MAXACC) {
        Serial.print(mls.latitude, 6); Serial.print(","); Serial.print(mls.longitude, 6);
        Serial.print(F(" acc ")); Serial.print(acc); Serial.println("m.");

        if (mls.getVector()) {
          Serial.print(F("Dst: ")); Serial.print(mls.distance, 2); Serial.print("m  ");
          Serial.print(F("Spd: ")); Serial.print(mls.speed, 2); Serial.print("m/s  ");
          Serial.print(F("Crs: ")); Serial.print(mls.bearing);
          Serial.println();
        }

        // APRS
        if (aprs.connect()) {
          aprs.authenticate(APRS_CALLSIGN, APRS_PASSCODE);
          if (acc < GEO_MINACC and mls.speed >= 1)
            aprs.sendPosition(mls.latitude, mls.longitude, mls.bearing, (int)(mls.speed * 1.94384449));
          else if (now - rpLastTime > 300000UL) {
            rpLastTime = now;
            aprs.sendPosition(mls.latitude, mls.longitude);
          }
          aprs.stop();
        }
      }
      else {
        Serial.print(F("failed, acc "));
        Serial.print(acc);
        Serial.println("m.");
      }
    }
    Serial.println();
  };
}
