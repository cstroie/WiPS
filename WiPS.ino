/**
  WiPS - Wireless Positioning System and Automated Position Reporting System
         based on Wifi geolocation, using Mozilla Location Services.

  Copyright (c) 2017-2018 Costin STROIE <costinstroie@eridu.eu.org>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, orl
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// True if the tracker is being probed
bool PROBE = true;

// Led
#define LED 2

// User settings
#include "config.h"

// Software version
#include "version.h"

// WiFi
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>

#ifdef WIFI_SSIDPASS
static const char wifiSP[] PROGMEM = WIFI_SSIDPASS;
const char *wifiRS          = WIFI_RS;
const char *wifiFS          = WIFI_FS;
#endif

// OTA
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
int otaPort     = 8266;

// TCP server
#include "server.h"
// NMEA-0183 Navigational Data Server
TCPServer nmeaServer(10110);

// UDP Broadcast
WiFiUDP   bcastUDP;
IPAddress bcastIP(0, 0, 0, 0);
const int bcastPort = 10110;

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
struct nmeaReports {
  bool gga: 1;
  bool rmc: 1;
  bool gll: 1;
  bool vtg: 1;
  bool zda: 1;
};
nmeaReports nmeaReport = {1, 1, 0, 0, 0};

// OLED
#include <U8x8lib.h>
U8X8_SSD1306_128X32_UNIVISION_HW_I2C u8x8(16);

// Set ADC to Voltage
ADC_MODE(ADC_VCC);

// Timings
unsigned long geoNextTime = 0;    // Next time to geolocate
unsigned long geoDelay    = 20;   // Delay between geolocating
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
    yield();

    // Print
    Serial.printf_P(PSTR("$PWIFI,CON,%s,%d,%ddBm,%s,%s,%s\r\n"),
                    WiFi.SSID().c_str(), WiFi.channel(), WiFi.RSSI(),
                    ipbuf, gwbuf, nsbuf);
    // Display
    u8x8.clear();
    u8x8.draw1x2String(0, 0, WiFi.SSID().c_str());
    u8x8.setCursor(0, 2); u8x8.print("IP "); u8x8.print(ipbuf);
    //u8x8.setCursor(0, 3); u8x8.print("GW "); u8x8.print(gwbuf);
    //u8x8.setCursor(0, 4); u8x8.print("NS "); u8x8.print(nsbuf);
    delay(1000);
  }
  else
    Serial.print(F("$PWIFI,ERR\r\n"));
  yield();
}

/**
  Turn the built-in led on, analog
*/
void setLED(int load) {
  int level = (1 << load) - 1;
  analogWrite(LED, PWMRANGE - level);
}

/**
  Try WPS PBC
  TODO
*/
bool tryWPSPBC() {
  Serial.print(F("$PWIFI,WPS,START\r\n"));
  bool wpsSuccess = WiFi.beginWPSConfig();
  if (wpsSuccess) {
    // Well this means not always success :-/ in case of a timeout we have an empty ssid
    String newSSID = WiFi.SSID();
    if (newSSID.length() > 0) {
      // WPSConfig has already connected in STA mode successfully to the new station.
      Serial.printf_P(PSTR("$PWIFI,WPS,%s\r\n"), newSSID.c_str());
    }
    else
      wpsSuccess = false;
  }
  return wpsSuccess;
}

/**
  Try to connect to a HTTPS server

  @param timeout connection timeout
*/
bool wifiCheckHTTP(char* server, int port, int timeout = 10000) {
  bool result = false;
  WiFiClientSecure testClient;
  testClient.setTimeout(timeout);
  testClient.setInsecure();
  char buf[64] = "";
  if (testClient.connect(server, port)) {
    Serial.printf_P(PSTR("$PHTTP,CON,%s,%d\r\n"), server, port);
    // Send a request
    testClient.print("HEAD / HTTP/1.1\r\n");
    testClient.print("Host: "); testClient.print(server); testClient.print("\r\n");
    testClient.print("Connection: close\r\n\r\n");
    // Check the response
    int rlen = testClient.readBytesUntil('\r', buf, 64);
    if (rlen > 0) {
      buf[rlen] = '\0';
      result = true;
      Serial.printf_P(PSTR("$PHTTP,RSP,%s\r\n"), buf);
    }
    else
      Serial.printf_P(PSTR("$PHTTP,DIS,%s,%d\r\n"), server, port);
  }
  else
    Serial.printf_P(PSTR("$PHTTP,ERR,%s,%d\r\n"), server, port);
  // Stop the test
  testClient.stop();
  // Return the result
  return result;
}

/**
  Try to connect to WiFi network

  @param ssid the WiFi SSID
  @param pass the WiFi psk
  @param timeout connection timeout
  @return connection result
*/
bool wifiTryConnect(const char* ssid = NULL, const char* pass = NULL, int timeout = 15) {
  bool result = false;

  // Need a name for default SSID
  char _ssid[WL_SSID_MAX_LENGTH] = "";

  if (ssid == NULL) {
    // Connect to stored SSID
    if (WiFi.SSID() != "") {
      strncpy(_ssid, WiFi.SSID().c_str(), WL_SSID_MAX_LENGTH);
    }
    else {
      // No stored and no specified SSID
      return result;
    }
  }
  else {
    // Keep the specified SSID, for convenience
    strncpy(_ssid, ssid, WL_SSID_MAX_LENGTH);
  }

  // Try to connect
  Serial.printf_P(PSTR("$PWIFI,BGN,%s\r\n"), _ssid);
  u8x8.clear();
  u8x8.draw1x2String(0, 0, "WiFi");
  // Different calls for different configurations
  if (ssid == NULL) WiFi.begin();
  else              WiFi.begin(_ssid, pass);
  // Display
  u8x8.setCursor(0, 2);
  u8x8.print(_ssid);
  u8x8.setCursor(0, 3);
  u8x8.print("---------------");
  u8x8.setCursor(0, 3);
  // Check the status
  int tries = 0;
  while (!WiFi.isConnected() and tries < timeout) {
    tries++;
    Serial.printf_P(PSTR("$PWIFI,TRY,%d/%d,%s\r\n"), tries, timeout, _ssid);
    u8x8.print("|");
    delay(1000);
  };
  // Check the internet connection
  if (WiFi.isConnected()) {
    showWiFi();
    result = wifiCheckHTTP(GEO_SERVER, GEO_PORT);
    //result = (mls.geoLocation() >= 0);
    if (!result)
      Serial.printf_P(PSTR("$PWIFI,ERR,%s\r\n"), _ssid);
  }
  else
    // Timed out
    Serial.printf_P(PSTR("$PWIFI,END,%s\r\n"), _ssid);
  return result;
}

/**
  Try to connect to a list of known wifi networks

  @result connection result to a known WiFi
*/
bool wifiTryKnownNetworks() {
  bool result = false;
  if (strlen_P(wifiSP) > 0) {
    // Scan the networks
    int netCount = WiFi.scanNetworks();
    if (netCount > 0) {
      Serial.printf_P(PSTR("$SWIFI,CNT,%d\r\n"), netCount);
      for (size_t i = 0; i < netCount; i++)
        Serial.printf_P(PSTR("$SWIFI,%d,%d,%d,%s,%s\r\n"),
                        i + 1,
                        WiFi.channel(i),
                        WiFi.RSSI(i),
                        WiFi.encryptionType(i) == ENC_TYPE_NONE ? "open" : "",
                        WiFi.SSID(i).c_str());
      // Temporary buffers for SSID, password and credentials list
      char ssid[WL_SSID_MAX_LENGTH]    = "";
      char pass[WL_WPA_KEY_MAX_LENGTH] = "";
      char sspa[250] = "";
      // Copy the credentials to RAM
      strncpy_P(sspa, wifiSP, 250);
      // The substrings/fields
      char *f1, *f2;
      char *fs, *rs;

      // Start from beginning
      f1 = sspa;
      // Find the record separator
      rs = strstr(f1, wifiRS);
      // While valid...
      while (rs != NULL) {
        // Find the field separator
        fs = strstr(f1, wifiFS);
        if (fs != NULL) {
          f2 = fs + strlen(wifiFS);
          // Check for valid lenghts
          if ((fs - f1 <= WL_SSID_MAX_LENGTH) and
              (rs - f2 <= WL_WPA_KEY_MAX_LENGTH)) {
            // Make a copy of SSID and password and make sure
            // they are null terminated
            strncpy(ssid, f1, fs - f1); ssid[fs - f1] = 0;
            strncpy(pass, f2, rs - f2); pass[rs - f2] = 0;
            // Check if we know any network
            for (size_t i = 0; i < netCount; i++) {
              // Check if we the SSID match
              if ((strncmp(ssid, WiFi.SSID(i).c_str(), WL_SSID_MAX_LENGTH) == 0) and
                  (strlen(ssid) == strlen(WiFi.SSID(i).c_str()))) {
                // Try to connect to wifi
                if (wifiTryConnect(ssid, pass)) {
                  result = true;
                  break;
                }
              }
              yield();
            }
          }
        }
        if (result) break;
        // Find the next record separator
        f1 = rs + strlen(wifiRS);
        rs = strstr(f1, wifiRS);
        // If null, maybe it's because the list ends with no RS
        if (rs == NULL and f1 < sspa + strlen(sspa))
          rs = sspa + strlen(sspa);
        yield();
      }
      // Return if we have a connection
      if (result) return result;

#ifdef WIFI_GREYHAT
      // Start again, this time trying all
      f1 = sspa;
      // Find the record separator
      rs = strstr(f1, wifiRS);
      // While valid...
      while (rs != NULL) {
        // Find the field separator
        fs = strstr(f1, wifiFS);
        if (fs != NULL) {
          f2 = fs + strlen(wifiFS);
          // Check for valid lenghts
          if ((fs - f1 <= WL_SSID_MAX_LENGTH) and
              (rs - f2 <= WL_WPA_KEY_MAX_LENGTH)) {
            // Make a copy of SSID and password and make sure
            // they are null terminated
            strncpy(ssid, f1, fs - f1); ssid[fs - f1] = 0;
            strncpy(pass, f2, rs - f2); pass[rs - f2] = 0;
            // Try all the networks
            for (size_t i = 0; i < netCount; i++) {
              // Try to connect to wifi
              if (wifiTryConnect(WiFi.SSID(i).c_str(), pass)) {
                result = true;
                break;
              }
              yield();
            }
          }
        }
        if (result) break;
        // Find the next record separator
        f1 = rs + strlen(wifiRS);
        rs = strstr(f1, wifiRS);
        // If null, maybe it's because the list ends with no RS
        if (rs == NULL and f1 < sspa + strlen(sspa))
          rs = sspa + strlen(sspa);
        yield();
      }
#endif
    }
  }
  // Clear the scan results
  WiFi.scanDelete();
  // Return the result
  return result;
}

/**
  Try to connect to open wifi networks

  @result connection result to an open WiFi
*/
bool wifiTryOpenNetworks() {
  bool result = false;
  // Scan
  int netCount = WiFi.scanNetworks();
  if (netCount > 0) {
    char ssid[WL_SSID_MAX_LENGTH] = "";
    for (size_t i = 1; i < netCount; i++) {
      // Find the open networks
      if (WiFi.encryptionType(i) == ENC_TYPE_NONE) {
        // Keep the SSID
        strncpy(ssid, WiFi.SSID(i).c_str(), WL_SSID_MAX_LENGTH);
        Serial.printf_P(PSTR("$PWIFI,OPN,%s\r\n"), ssid);
        // Try to connect to wifi
        if (wifiTryConnect(ssid)) {
          result = true;
          break;
        }
      }
      yield();
    }
  }
  // Clear the scan results
  WiFi.scanDelete();
  // Return the result
  return result;
}

/**
  Feedback notification when SoftAP is started
*/
void wifiCallback(WiFiManager * wifiMgr) {
  Serial.printf_P(PSTR("$PWIFI,SRV,%s\r\n"),
                  wifiMgr->getConfigPortalSSID().c_str());
  u8x8.clear();
  u8x8.draw1x2String(0, 0, wifiMgr->getConfigPortalSSID().c_str());
  setLED(10);
}

/**
  Try to connect to WiFi
*/
bool wifiConnect(int timeout = 300) {
  // Set the host name
  WiFi.hostname(NODENAME);
  // Set the mode
  WiFi.mode(WIFI_STA);
  // Do not try to auto-connect on power on
  //WiFi.setAutoConnect(false);
  // Led ON
  setLED(1);

  // Keep the connection result
  bool result = true;
  // Try to connect to WiFi
#ifdef WIFI_SSID
  wifiTryConnect(WIFI_SSID, WIFI_PASS);
#else
  // Check if already connected, then try to connect to the last known AP
  if (!WiFi.isConnected()) {
    // Keep the saved credentials
    char savedSSID[WL_SSID_MAX_LENGTH];
    char savedPSK[WL_WPA_KEY_MAX_LENGTH];
    strncpy(savedSSID, WiFi.SSID().c_str(), WL_SSID_MAX_LENGTH);
    strncpy(savedPSK, WiFi.psk().c_str(), WL_WPA_KEY_MAX_LENGTH);
    // Try to connect with saved credentials
    if (not wifiTryConnect()) {
      // Try the known networks
      if (not wifiTryKnownNetworks()) {
        // Try the open networks
        if (not wifiTryOpenNetworks()) {
          // Use the WiFi Manager
          WiFiManager wifiManager;
          wifiManager.setTimeout(timeout);
          wifiManager.setAPCallback(wifiCallback);
          setLED(10);
          if (not wifiManager.startConfigPortal(NODENAME)) {
            setLED(2);
            result = false;
          }
        }
      }
    }
  }
#endif
  // Led OFF
  setLED(0);

  return result;
}

/**
  UDP broadcast
*/
void broadcast(char *buf, size_t len) {
  // Find the broadcast IP
  bcastIP = IPAddress((~ (uint32_t)WiFi.subnetMask()) | ((uint32_t)WiFi.gatewayIP()));

  //Serial.printf_P(PSTR("$PBCST,%u,%d.%d.%d.%d\r\n"),
  //                bcastPort, bcastIP[0], bcastIP[1], bcastIP[2], bcastIP[3]);
  // Send the packet
  bcastUDP.beginPacket(bcastIP, bcastPort);
  bcastUDP.write(buf, len);
  bcastUDP.endPacket();
}

/**
  Main Arduino setup function
*/
void setup() {
  // Do not save the last WiFi settings
  WiFi.persistent(false);
  // Init the serial communication
  Serial.begin(9600, SERIAL_8N1, SERIAL_TX_ONLY);
  Serial.print("\r\n");
  // Init the display
  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.setContrast(2);
  // Compose the NMEA welcome message
  nmea.getWelcome(NODENAME, VERSION);
  Serial.print(nmea.welcome);
  Serial.print(F("$PGPL3,This program comes with ABSOLUTELY NO WARRANTY.\r\n"));
  Serial.print(F("$PGPL3,This is free software, and you are welcome \r\n"));
  Serial.print(F("$PGPL3,to redistribute it under certain conditions.\r\n"));
  // Display
  u8x8.draw2x2String(4, 0, NODENAME);
  u8x8.drawString(5, 3, VERSION);
  delay(1000);

  // Initialize the LED pin as an output
  pinMode(LED, OUTPUT);
  setLED(0);

  // Try to connect, for ever
  while (not wifiConnect(300));

  // OTA Update
  ArduinoOTA.setPort(otaPort);
  ArduinoOTA.setHostname(NODENAME);
#ifdef OTA_PASS
  ArduinoOTA.setPassword((const char *)OTA_PASS);
#endif

  ArduinoOTA.onStart([]() {
    Serial.print(F("$POTA,STA\r\n"));
    u8x8.clear();
    u8x8.draw1x2String(3, 0, "OTA Update");
    u8x8.drawString(2, 3, "[----------]");
  });

  ArduinoOTA.onEnd([]() {
    Serial.print(F("\r\n$POTA,FIN\r\n"));
    u8x8.clear();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static int otaProgress = 0;
    int otaPrg = progress / (total / 100);
    if (otaProgress != otaPrg) {
      otaProgress = otaPrg;
      Serial.printf_P(PSTR("$POTA,PRG,%u%%\r\n"), otaProgress);
      if (otaProgress < 100)
        u8x8.drawString(3 + otaProgress / 10, 3, "|");
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf_P(PSTR("$POTA,ERR,%u,"), error);
    if      (error == OTA_AUTH_ERROR)     Serial.print(F("Auth Failed\r\n"));
    else if (error == OTA_BEGIN_ERROR)    Serial.print(F("Begin Failed\r\n"));
    else if (error == OTA_CONNECT_ERROR)  Serial.print(F("Connect Failed\r\n"));
    else if (error == OTA_RECEIVE_ERROR)  Serial.print(F("Receive Failed\r\n"));
    else if (error == OTA_END_ERROR)      Serial.print(F("End Failed\r\n"));
  });

  ArduinoOTA.begin();
  Serial.print(F("$POTA,RDY\r\n"));

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
  Serial.print(F("$PHWMN,VCC,"));
  Serial.print((float)hwVcc / 1000, 3);
  Serial.print("\r\n");

  // Initialize the random number generator and set the APRS telemetry start sequence
  randomSeed(ntp.getSeconds(false) + hwVcc + millis());
  aprs.aprsTlmSeq = random(1000);
  Serial.printf_P(PSTR("$PHWMN,TLM,%d\r\n"), aprs.aprsTlmSeq);

  // Start NMEA TCP server
  nmeaServer.init("nmea-0183", nmea.welcome);
}

/**
  Main Arduino loop
*/
void loop() {
  // Handle OTA
  ArduinoOTA.handle();
  yield();

  // Handle NMEA clients
  nmeaServer.check();

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

    // Get the time of the fix
    unsigned long utm = ntp.getSeconds();

    // Scan the WiFi access points
    Serial.print(F("$PSCAN,WIFI,"));
    int found = mls.wifiScan(false);

    // Get the coordinates
    if (found > 0) {
      // Led on
      setLED(6);
      Serial.print(found);
      Serial.print(","); Serial.print(ntp.getSeconds() - utm);
      Serial.print("s\r\n");

      // Geolocate
      int acc = mls.geoLocation();
      // Led off
      setLED(4);

      // Exponential smooth the accuracy
      if (sAcc < 0) sAcc = acc;
      else          sAcc = (((sAcc << 2) - sAcc + acc) + 2) >> 2;

      // Display
      u8x8.clear();
      char bufClock[20];
      ntp.getClock(bufClock, 20, utm);
      u8x8.setCursor(0, 3); u8x8.print("UTC "); u8x8.print(bufClock);

      if (mls.current.valid) {
        // Report
        Serial.print(F("$PSCAN,FIX,"));
        Serial.print(mls.current.latitude, 6);  Serial.print(",");
        Serial.print(mls.current.longitude, 6); Serial.print(",");
        Serial.print(mls.locator);              Serial.print(",");
        Serial.print(acc);                      Serial.print("m,");
        Serial.print(ntp.getSeconds() - utm);   Serial.print("s");
        // Display
        u8x8.print(" FIX");
        u8x8.setCursor(0, 0);
        u8x8.print("Lat ");
        u8x8.print(mls.current.latitude  >= 0 ? "N " : "S ");
        u8x8.print(fabs(mls.current.latitude),  6);
        u8x8.setCursor(0, 1);
        u8x8.print("Lng ");
        u8x8.print(mls.current.longitude >= 0 ? "E" : "W");
        if (abs(mls.current.longitude) < 100) u8x8.print(" ");
        u8x8.print(fabs(mls.current.longitude), 6);

        // Check if moving
        bool moving = mls.getMovement() >= (sAcc >> 2);
        if (moving) {
          // Exponential smooth the bearing (75%)
          if (sCrs < 0) sCrs = mls.bearing;
          else          sCrs = ((sCrs + (mls.bearing << 2) - mls.bearing) + 2) >> 2;
          // Report
          Serial.print(",");
          Serial.print(mls.distance, 2);  Serial.print("m,");
          Serial.print(mls.speed, 2);     Serial.print("m/s,");
          Serial.print(mls.bearing);      Serial.print("'");
          // Display
          u8x8.setCursor(0, 2); u8x8.print("Spd "); u8x8.print(mls.speed, 2);
          u8x8.setCursor(9, 2); u8x8.print("Crs "); u8x8.print(sCrs);
        }
        else {
          // Display the locator
          u8x8.setCursor(0, 2); u8x8.print("Loc "); u8x8.print(mls.locator);
        }
        Serial.print("\r\n");

        // Compose and send the NMEA sentences
        char bufServer[200];
        int lenServer;
        // GGA
        if (nmeaReport.gga) {
          lenServer = nmea.getGGA(bufServer, 200, utm, mls.current.latitude, mls.current.longitude, 1, found);
          Serial.print(bufServer);
          if (nmeaServer.clients) nmeaServer.sendAll(bufServer);
          broadcast(bufServer, lenServer);
        }
        // RMC
        if (nmeaReport.rmc) {
          lenServer = nmea.getRMC(bufServer, 200, utm, mls.current.latitude, mls.current.longitude, mls.knots, sCrs);
          Serial.print(bufServer);
          if (nmeaServer.clients) nmeaServer.sendAll(bufServer);
          broadcast(bufServer, lenServer);
        }
        // GLL
        if (nmeaReport.gll) {
          lenServer = nmea.getGLL(bufServer, 200, utm, mls.current.latitude, mls.current.longitude);
          Serial.print(bufServer);
          if (nmeaServer.clients) nmeaServer.sendAll(bufServer);
          broadcast(bufServer, lenServer);
        }
        // VTG
        if (nmeaReport.vtg) {
          lenServer = nmea.getVTG(bufServer, 200, sCrs, mls.knots, (int)(mls.speed * 3.6));
          Serial.print(bufServer);
          if (nmeaServer.clients) nmeaServer.sendAll(bufServer);
          broadcast(bufServer, lenServer);
        }
        // ZDA
        if (nmeaReport.zda) {
          lenServer = nmea.getZDA(bufServer, 200, utm);
          Serial.print(bufServer);
          if (nmeaServer.clients) nmeaServer.sendAll(bufServer);
          broadcast(bufServer, lenServer);
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
              snprintf_P(buf, sizeof(buf), PSTR("Acc:%d Dst:%d Spd:%d Crs:%s Vcc:%d.%d RSSI:%d"),
                         acc, (int)(mls.distance), (int)(3.6 * mls.speed), mls.getCardinal(sCrs),
                         vcc / 1000, (vcc % 1000) / 100, rssi);
              // Report course and speed
              aprs.sendPosition(utm, mls.current.latitude, mls.current.longitude, sCrs, mls.knots, acc, buf);
              // Send the telemetry
              //   mls.speed / 0.0008 = mls.speed * 1250
              aprs.sendTelemetry((vcc - 2500) / 4, -rssi, heap / 256, acc, (int)(sqrt(mls.speed * 1250)), aprs.aprsTlmBits);
              // Send the status
              //snprintf_P(buf, sizeof(buf), PSTR("%s/%s, Vcc: %d.%3dV, RSSI: %ddBm"),
              //           NODENAME, VERSION, vcc / 1000, vcc % 1000, rssi);
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
        Serial.printf_P(PSTR("$PSCAN,NOFIX,%dm,%ds\r\n"), acc, ntp.getSeconds() - utm);
        u8x8.print(" NFX");
      }
      // Repeat the geolocation after a delay
      geoNextTime = now + geoDelay;
    }
    else {
      // No WiFi networks
      Serial.print(F("0"));
      Serial.print(",");
      Serial.print(ntp.getSeconds() - utm);
      Serial.print("s\r\n");
      // Repeat the geolocation now
      geoNextTime = now;
    }

    // Led off
    setLED(0);
  };
}
// vim: set ft=arduino ai ts=2 sts=2 et sw=2 sta nowrap nu :
