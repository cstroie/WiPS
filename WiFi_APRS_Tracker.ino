/**
  WiFi APRS Tracker - Automated Position Reporting System based on Wifi
                      geolocation, using Mozilla Location Services

  Copyright (c) 2017 Costin STROIE <costinstroie@eridu.eu.org>

  This file is part of WiFi_APRS_Tracker.
*/

// True if the tracker is being probed
bool PROBE = true;

// User settings
#include "config.h"

// Software version
#include "version.h"

// WiFi
#include <ESP8266WiFi.h>
#include <WiFiManager.h>

// OTA
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
int otaProgress       = 0;
int otaPort           = 8266;

// Mozilla Location Services
#include "mls.h"
MLS mls;

// Network Time Protocol
#include "ntp.h"
NTP ntp;

// APRS
#include "aprs.h"
APRS aprs;

// Set ADC to Voltage
ADC_MODE(ADC_VCC);

// Timings
unsigned long geoNextTime = 0;    // Next time to geolocate
unsigned long geoDelay    = 30;   // Delay between geolocating
unsigned long rpNextTime  = 0;    // Next time to report
unsigned long rpDelay     = 30;   // Delay between reporting
unsigned long rpDelayStep = 30;   // Step to increase the delay between reporting
unsigned long rpDelayMin  = 30;   // Minimum delay between reporting
unsigned long rpDelayMax  = 1800; // Maximum delay between reporting

// Smoothen accuracy
int sacc;

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
  Serial.println();
  if (WiFi.isConnected()) {
    char ipbuf[16] = "";
    char gwbuf[16] = "";
    char nsbuf[16] = "";

    // Get the IPs as char arrays
    charIP(WiFi.localIP(),   ipbuf, sizeof(ipbuf), true);
    charIP(WiFi.gatewayIP(), gwbuf, sizeof(gwbuf), true);
    charIP(WiFi.dnsIP(),     nsbuf, sizeof(nsbuf), true);

    // Print
    Serial.print(F("WiFi connected to ")); Serial.print(WiFi.SSID());
    Serial.print(F(" on channel "));       Serial.print(WiFi.channel());
    Serial.print(F(", RSSI "));            Serial.print(WiFi.RSSI());    Serial.println(F(" dBm."));
    Serial.print(F(" IP : "));             Serial.println(ipbuf);
    Serial.print(F(" GW : "));             Serial.println(gwbuf);
    Serial.print(F(" DNS: "));             Serial.println(nsbuf);
    Serial.println();
  }
  else {
    Serial.print(F("WiFi not connected"));
  }
}

/**
  Turn the built-in led on, analog
*/
void setLED(int load) {
  int level = (1 << load) - 1;
  analogWrite(LED_BUILTIN, PWMRANGE - level);
}

/**
  Feedback notification when SoftAP is started
*/
void wifiCallback(WiFiManager *wifiMgr) {
  Serial.print(F("Connect to "));
  Serial.println(wifiMgr->getConfigPortalSSID());
}

/**
  Try to connect to WiFi
*/
void wifiConnect() {
  // Set the host name
  WiFi.hostname(NODENAME);
  // Store credentials only if changed
  //WiFi.persistent(false);
  // Led ON
  setLED(10);
  // Try to connect to WiFi
#ifdef WIFI_SSID
  Serial.print("WiFi connecting ");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (!WiFi.isConnected()) {
    Serial.print(".");
    delay(1000);
  };
  Serial.println(F(" done."));
#else
  WiFiManager wifiManager;
  wifiManager.setTimeout(300);
  wifiManager.setAPCallback(wifiCallback);
  while (!wifiManager.autoConnect(NODENAME))
    delay(1000);
#endif
  // Led OFF
  setLED(0);
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
  setLED(0);

  // Try to connect
  wifiConnect();
  // Connected
  showWiFi();

  // OTA Update
  ArduinoOTA.setPort(otaPort);
  ArduinoOTA.setHostname(NODENAME);
#ifdef OTA_PASS
  ArduinoOTA.setPassword((const char *)OTA_PASS);
#endif

  ArduinoOTA.onStart([]() {
    Serial.println(F("OTA Start"));
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Finished");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int otaPrg = progress / (total / 100);
    if (otaProgress != otaPrg) {
      otaProgress = otaPrg;
      Serial.printf("Progress: %u%%\r", otaProgress * 100);
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR)
      Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR)
      Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR)
      Serial.println(F("End Failed"));
  });

  ArduinoOTA.begin();
  Serial.println(F("OTA Ready"));

  // Configure NTP
  ntp.init(NTP_SERVER);

  // Configure APRS
  aprs.init(APRS_SERVER, APRS_PORT);
  aprs.setNTP(ntp);
  // Use an automatic callsign
  aprs.setCallSign();
  Serial.print(F("APRS callsign: ")); Serial.print(aprs.aprsCallSign);
  Serial.print(F(", passcode: ")); Serial.print(aprs.aprsPassCode);
  //Serial.print(F(", object: ")); Serial.print(aprs.aprsObjectNm);
  Serial.println();

  // Hardware data
  int hwVcc  = ESP.getVcc();
  Serial.print(F("Vcc : "));
  Serial.println((float)hwVcc / 1000, 3);

  // Initialize the random number generator and set the APRS telemetry start sequence
  randomSeed(ntp.getSeconds(false) + hwVcc + millis());
  aprs.aprsTlmSeq = random(1000);
  Serial.print(F("TLM : "));
  Serial.println(aprs.aprsTlmSeq);
}

/**
  Main Arduino loop
*/
void loop() {
  // Handle OTA
  ArduinoOTA.handle();
  yield();

  // Uptime
  unsigned long now = millis() / 1000;

  // Check if we should geolocate
  if (now >= geoNextTime) {
    // Make sure we are connected
    if (!WiFi.isConnected()) wifiConnect();

    // Set the telemetry bit 7 if the tracker is being probed
    if (PROBE) aprs.aprsTlmBits = B10000000;
    else       aprs.aprsTlmBits = B00000000;

    // Log the APRS time
    char aprsTime[10] = "";
    aprs.time(aprsTime, sizeof(aprsTime));
    Serial.print("["); Serial.print(aprsTime); Serial.print("] ");

    // Check the time and set the telemetry bit 0 if time is not accurate
    if (!ntp.valid) aprs.aprsTlmBits |= B00000001;
    // Set the telemetry bit 1 if the uptime is less than one day (recent reboot)
    if (millis() < 86400000UL) aprs.aprsTlmBits |= B00000010;

    // Led on
    setLED(4);

    // Scan the WiFi access points
    Serial.print(F("WiFi networks... "));
    int found = mls.wifiScan(false);

    // Get the coordinates
    if (found > 0) {
      // Led on
      setLED(6);
      Serial.print(found); Serial.print(F(" found, geolocating... "));
      int acc = mls.geoLocation();
      // Led off
      setLED(4);

      // Exponential smoothing the accuracy
      if (sacc < 0) sacc = acc;
      else          sacc = (((sacc << 2) - sacc + acc) + 2) >> 2;

      if (mls.current.valid) {
        Serial.print(mls.current.latitude, 6); Serial.print(","); Serial.print(mls.current.longitude, 6);
        Serial.print(F(" acc "));      Serial.print(acc); Serial.println("m.");

        // Check if moving
        bool moving = mls.getMovement() >= (sacc >> 2);
        if (moving) {
          Serial.print(F("          "));
          Serial.print(F("Dst: ")); Serial.print(mls.distance, 2); Serial.print("m  ");
          Serial.print(F("Spd: ")); Serial.print(mls.speed, 2);    Serial.print("m/s  ");
          Serial.print(F("Crs: ")); Serial.print(mls.bearing);
          Serial.println();
        }

        // Read the Vcc (mV)
        int vcc  = ESP.getVcc();
        // Set the bit 3 to show whether the battery is wrong (3.3V +/- 10%)
        if (vcc < 3000 or vcc > 3600) aprs.aprsTlmBits |= B00001000;
        // Get RSSI
        int rssi = WiFi.RSSI();
        // Get free heap
        int heap = ESP.getFreeHeap();

        // APRS if moving or time expired
        if ((moving or (now >= rpNextTime)) and acc >= 0) {
          // Led ON
          setLED(8);

          // Connect to the server
          if (aprs.connect()) {
            // Authenticate
            if (aprs.authenticate()) {
              // Local buffer, max comment length is 43 bytes
              char buf[45] = "";
              // Prepare the comment
              snprintf_P(buf, sizeof(buf), PSTR("Acc:%d Dst:%d Spd:%d Vcc:%d.%d RSSI:%d"), acc, (int)(mls.distance), (int)(3.6 * mls.speed), vcc / 1000, (vcc % 1000) / 100, rssi);
              // Report course and speed if the geolocation accuracy is better than moving distance
              if (moving) {
                // Report
                aprs.sendPosition(mls.current.latitude, mls.current.longitude, mls.bearing, lround(mls.speed * 1.94384449), -1, buf);
                // Reset the delay to minimum
                rpDelay = rpDelayMin;
                // Set the telemetry bits 4 and 5 if moving, according to the speed
                if (mls.speed > 10) aprs.aprsTlmBits |= B00100000;
                else                aprs.aprsTlmBits |= B00010000;
              }
              else {
                // Report
                aprs.sendPosition(mls.current.latitude, mls.current.longitude, mls.bearing, 0, -1, buf);
                // Not moving, increase the delay to a maximum
                rpDelay += rpDelayStep;
                if (rpDelay > rpDelayMax) rpDelay = rpDelayMax;
              }
              // Send the telemetry
              aprs.sendTelemetry((vcc - 2500) / 4, -rssi, heap / 256, acc, (int)(sqrt(mls.speed / 0.0008)), aprs.aprsTlmBits);
              // Send the status
              //snprintf_P(buf, sizeof(buf), PSTR("%s/%s, Vcc: %d.%3dV, RSSI: %ddBm"), NODENAME, VERSION, vcc / 1000, vcc % 1000, rssi);
              //aprs.sendStatus(buf);
            }
            else {
              // Reset the delay to minimum
              rpDelay = rpDelayMin;
            }
            // Close the connection
            aprs.stop();
          }
          // Led OFF
          setLED(0);

          // Repeat the report after the delay
          rpNextTime = now + rpDelay;
        }
      }
      else {
        Serial.print(F("failed, acc "));
        Serial.print(acc);
        Serial.println("m.");
      }
    }
    else {
      // No WiFi networks
      Serial.println(F("none."));
    }

    // Led off
    setLED(0);

    // Repeat the geolocation after a delay
    geoNextTime = now + geoDelay;
  };
}
// vim: set ft=arduino ai ts=2 sts=2 et sw=2 sta nowrap nu :
