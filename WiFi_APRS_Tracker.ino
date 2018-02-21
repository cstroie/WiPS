/**
  WiFi APRS Tracker - Automated Position Reporting System based on Wifi
                      geolocation, using Mozilla Location Services

  Copyright (c) 2017-2018 Costin STROIE <costinstroie@eridu.eu.org>

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

// NMEA
#include "nmea.h"
NMEA nmea;

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

// Smooth accuracy and course
int sAcc = -1;
int sCrs = -1;

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
    charIP(WiFi.localIP(),   ipbuf, sizeof(ipbuf), false);
    charIP(WiFi.gatewayIP(), gwbuf, sizeof(gwbuf), false);
    charIP(WiFi.dnsIP(),     nsbuf, sizeof(nsbuf), false);

    // Print
    Serial.printf("$PWIFI,CFG,%s,%d,%d,%s,%s,%s\r\n",
                  WiFi.SSID().c_str(), WiFi.channel(), WiFi.RSSI(),
                  ipbuf, gwbuf, nsbuf);
  }
  else {
    Serial.print("$PWIFI,ERR\r\n");
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
  Serial.printf("$PWIFI,SRV,%s\r\n", wifiMgr->getConfigPortalSSID().c_str());
  setLED(10);
}

/**
  Try to connect to WiFi
*/
void wifiConnect(int timeout = 300) {
  // Set the host name
  WiFi.hostname(NODENAME);
  // Led ON
  setLED(10);
  // Try to connect to WiFi
#ifdef WIFI_SSID
  Serial.printf("$PWIFI,BGN,%s\r\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (!WiFi.isConnected()) {
    Serial.printf("$PWIFI,TRY,%s\r\n", WIFI_SSID);
    delay(1000);
  };
  Serial.printf("$PWIFI,CON,%s\r\n", WIFI_SSID);
#else
  setLED(4);
  WiFiManager wifiManager;
  wifiManager.setTimeout(timeout);
  wifiManager.setAPCallback(wifiCallback);
  while (!wifiManager.autoConnect(NODENAME)) {
    setLED(2);
    delay(1000);
  }
#endif
  // Led OFF
  setLED(0);
}

/**
  Main Arduino setup function
*/
void setup() {
  // Init the serial communication
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
  Serial.printf("$PVERS,%s,%s,%s\r\n", NODENAME, VERSION, __DATE__);

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
    Serial.print("$POTA,START\r\n");
  });

  ArduinoOTA.onEnd([]() {
    Serial.print("\r\n$POTA,FINISHED\r\n");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int otaPrg = progress / (total / 100);
    if (otaProgress != otaPrg) {
      otaProgress = otaPrg;
      Serial.printf("$POTA,PROGRESS,%u%%\r\n", otaProgress * 100);
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("$POTA,ERROR,%u,", error);
    if (error == OTA_AUTH_ERROR)
      Serial.print(F("Auth Failed\r\n"));
    else if (error == OTA_BEGIN_ERROR)
      Serial.print(F("Begin Failed\r\n"));
    else if (error == OTA_CONNECT_ERROR)
      Serial.print(F("Connect Failed\r\n"));
    else if (error == OTA_RECEIVE_ERROR)
      Serial.print(F("Receive Failed\r\n"));
    else if (error == OTA_END_ERROR)
      Serial.print(F("End Failed\r\n"));
  });

  ArduinoOTA.begin();
  Serial.print("$POTA,READY\r\n");

  // Configure NTP
  ntp.init(NTP_SERVER);

  // Configure APRS
  aprs.init(APRS_SERVER, APRS_PORT);
  // Use an automatic callsign
  aprs.setCallSign();
  Serial.print(F("$PAPRS,AUTH,")); Serial.print(aprs.aprsCallSign);
  Serial.print(","); Serial.print(aprs.aprsPassCode);
  //Serial.print(","); Serial.print(aprs.aprsObjectNm);
  Serial.print("\r\n");

  // Hardware data
  int hwVcc  = ESP.getVcc();
  Serial.print("$PHWMN,VCC,");
  Serial.print((float)hwVcc / 1000, 3);
  Serial.print("\r\n");

  // Initialize the random number generator and set the APRS telemetry start sequence
  randomSeed(ntp.getSeconds(false) + hwVcc + millis());
  aprs.aprsTlmSeq = random(1000);
  Serial.printf("$PHWMN,TLM,%d\r\n", aprs.aprsTlmSeq);
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
    // Make sure we are connected, shorter timeout
    if (!WiFi.isConnected()) wifiConnect(60);

    // Set the telemetry bit 7 if the tracker is being probed
    if (PROBE) aprs.aprsTlmBits = B10000000;
    else       aprs.aprsTlmBits = B00000000;

    // Check the time and set the telemetry bit 0 if time is not accurate
    if (!ntp.valid) aprs.aprsTlmBits |= B00000001;
    // Set the telemetry bit 1 if the uptime is less than one day (recent reboot)
    if (millis() < 86400000UL) aprs.aprsTlmBits |= B00000010;

    // Led on
    setLED(4);

    // Scan the WiFi access points
    Serial.print("$PSCAN,WIFI,");
    int found = mls.wifiScan(false);

    // Get the coordinates
    if (found > 0) {
      // Led on
      setLED(6);
      Serial.print(found);
      int acc = mls.geoLocation();
      // Led off
      setLED(4);

      // Exponential smooth the accuracy
      if (sAcc < 0) sAcc = acc;
      else          sAcc = (((sAcc << 2) - sAcc + acc) + 2) >> 2;

      if (mls.current.valid) {
        // Get the time of the fix
        unsigned long utm = ntp.getSeconds();
        // Report
        Serial.print(F(",LAT,"));
        Serial.print(mls.current.latitude, 6);
        Serial.print(",LNG,");
        Serial.print(mls.current.longitude, 6);
        Serial.print(F(",ACC,"));
        Serial.print(acc);

        // Check if moving
        bool moving = mls.getMovement() >= (sAcc >> 2);
        if (moving) {
          // Exponential smooth the bearing
          if (sCrs < 0) sCrs = mls.bearing;
          else          sCrs = (((sCrs << 2) - sCrs + mls.bearing) + 2) >> 2;
          // Report
          Serial.print(F(",DST,")); Serial.print(mls.distance, 2);
          Serial.print(F(",SPD,")); Serial.print(mls.speed, 2);
          Serial.print(F(",CRS,")); Serial.print(mls.bearing);
        }
        Serial.print("\r\n");

        // NMEA
        nmea.send(utm, mls.current.latitude, mls.current.longitude, mls.knots, mls.bearing, 1, found);

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
              // Report course and speed
              aprs.sendPosition(utm, mls.current.latitude, mls.current.longitude, sCrs, mls.knots, -1, buf);
              // Send the telemetry
              //   mls.speed / 0.0008 = mls.speed * 1250
              aprs.sendTelemetry((vcc - 2500) / 4, -rssi, heap / 256, acc, (int)(sqrt(mls.speed * 1250)), aprs.aprsTlmBits);
              // Send the status
              //snprintf_P(buf, sizeof(buf), PSTR("%s/%s, Vcc: %d.%3dV, RSSI: %ddBm"), NODENAME, VERSION, vcc / 1000, vcc % 1000, rssi);
              //aprs.sendStatus(buf);
              // Adjust the delay (aka SmartBeaconing)
              if (moving) {
                // Reset the delay to minimum
                rpDelay = rpDelayMin;
                // Set the telemetry bits 4 and 5 if moving, according to the speed
                if (mls.speed > 10) aprs.aprsTlmBits |= B00100000;
                else                aprs.aprsTlmBits |= B00010000;
              }
              else {
                // Not moving, increase the delay up to a maximum
                rpDelay += rpDelayStep;
                if (rpDelay > rpDelayMax) rpDelay = rpDelayMax;
              }
            }
            // Close the connection
            aprs.stop();
          }

          // On error, reset the delay to the minimum
          if (aprs.error) {
            rpDelay = rpDelayMin;
            aprs.error = false;
          }

          // Repeat the report after the delay
          rpNextTime = now + rpDelay;

          // Led OFF
          setLED(0);
        }
      }
      else {
        Serial.print(F(",ACC,"));
        Serial.print(acc);
        Serial.print("\r\n");
      }
    }
    else {
      // No WiFi networks
      Serial.print(F("NONE\r\n"));
    }

    // Led off
    setLED(0);

    // Repeat the geolocation after a delay
    geoNextTime = now + geoDelay;
  };
}
// vim: set ft=arduino ai ts=2 sts=2 et sw=2 sta nowrap nu :
