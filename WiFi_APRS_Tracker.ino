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

// Timings
unsigned long geoNextTime = 0;    // Next time to geolocate
unsigned long geoDelay    = 30;   // Delay between geolocating
unsigned long rpNextTime  = 0;    // Next time to report
unsigned long rpDelay     = 30;   // Delay between reporting
unsigned long rpDelayStep = 30;   // Step to increase the delay between reporting with
unsigned long rpDelayMin  = 30;   // Minimum delay between reporting
unsigned long rpDelayMax  = 1800; // Maximum delay between reporting

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
  Turn the built-in led off
*/
void ledOff() {
  digitalWrite(LED_BUILTIN, HIGH);
}

/**
  Turn the built-in led on
*/
void ledOn() {
  digitalWrite(LED_BUILTIN, LOW);
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

  // Initialize the LED_BUILTIN pin as an output
  pinMode(LED_BUILTIN, OUTPUT);

  // Set the host name
  WiFi.hostname(NODENAME);
#ifdef WIFI_SSID
  Serial.print("WiFi connecting ");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (!WiFi.isConnected()) {
    Serial.print(".");
    delay(1000);
  };
  Serial.println(F(" done."));
#else
  // Try to connect to WiFi
  WiFiManager wifiManager;
  wifiManager.setTimeout(300);
  wifiManager.setAPCallback(wifiCallback);
  wifiManager.autoConnect(NODENAME);
  while (!wifiManager.autoConnect(NODENAME))
    delay(1000);
#endif

  // Led OFF
  ledOff();

  // Connected
  showWiFi();

  // Configure NTP
  ntp.init(NTP_SERVER);

  // Configure APRS
  aprs.init(APRS_SERVER, APRS_PORT);
  aprs.setNTP(ntp);
  aprs.setCallSign(APRS_CALLSIGN);
  Serial.print(F("APRS callsign: ")); Serial.print(aprs.aprsCallSign);
  Serial.print(F(", passcode: ")); Serial.println(aprs.aprsPassCode);
}

/**
  Main Arduino loop
*/
void loop() {
  // Uptime
  unsigned long now = millis() / 1000;
  // Check if we should geolocate
  if (now >= geoNextTime) {
    // Repeat the geolocation after a delay
    geoNextTime = now + geoDelay;
    // Log the APRS time
    char aprsTime[10] = "";
    aprs.time(aprsTime, sizeof(aprsTime));
    Serial.print("["); Serial.print(aprsTime); Serial.print("] ");

    // Scan the WiFi access points
    Serial.print(F("WiFi networks... "));
    int found = mls.wifiScan(false);
    Serial.print(found);
    Serial.print(" found, ");

    // Get the coordinates
    if (found > 0) {
      Serial.print(F("geolocating... "));
      int acc = mls.geoLocation();
      if (mls.validCoords) {
        // Local comment
        const int commentSize = 64;
        char comment[commentSize] = "";

        Serial.print(mls.latitude, 6); Serial.print(","); Serial.print(mls.longitude, 6);
        Serial.print(F(" acc ")); Serial.print(acc); Serial.println("m.");

        bool moved = mls.getMovement() >= (acc / 2);
        if (moved) {
          Serial.print(F("          "));
          Serial.print(F("Dst: ")); Serial.print(mls.distance, 2); Serial.print("m  ");
          Serial.print(F("Spd: ")); Serial.print(mls.speed, 2); Serial.print("m/s  ");
          Serial.print(F("Crs: ")); Serial.print(mls.bearing);
          Serial.println();
        }

        // Prepare the comment
        int dst = 100 * mls.distance;
        snprintf_P(comment, commentSize, PSTR("Acc: %dm, Dst: %d.%dm, Spd: %dkm/h"), acc, dst / 100, dst % 100, (int)(3.6 * mls.speed));

        // APRS if moving or time expired
        if ((moved or (now >= rpNextTime)) and acc >= 0) {
          // Led ON
          ledOn();
          // Adjust the delay
          if (moved) {
            // Reset the delay to minimum
            rpDelay = rpDelayMin;
          }
          else {
            // Not moving, increase the delay to a maximum
            rpDelay += rpDelayStep;
            if (rpDelay > rpDelayMax) rpDelay = rpDelayMax;
          }
          // Repeat the report after a the delay
          rpNextTime = now + rpDelay;

          // Connect to the server
          if (aprs.connect()) {
            // Authenticate
            aprs.authenticate();
            // Report course and speed if the geolocation accuracy better than moving distance
            if (moved)  aprs.sendPosition(mls.latitude, mls.longitude, mls.bearing, lround(mls.speed * 1.94384449), -1, comment);
            else        aprs.sendPosition(mls.latitude, mls.longitude, mls.bearing, 0, -1, comment);
            // Close the connection
            aprs.stop();
          }
          // Led OFF
          ledOff();
        }
      }
      else {
        Serial.print(F("failed, acc "));
        Serial.print(acc);
        Serial.println("m.");
      }
    }
  };
}

