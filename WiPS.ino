/**
  WiPS - Wireless Positioning System and Automated Position Reporting System
         based on WiFi geolocation, using Mozilla Location Services and Google Geolocation API.

  This project provides:
  - WiFi-based geolocation using nearby access points
  - NMEA-0183 sentence generation for GPS compatibility
  - APRS position reporting with telemetry data
  - TCP NMEA server and UDP broadcast capabilities
  - Over-The-Air (OTA) firmware updates
  - Network Time Protocol (NTP) synchronization

  Main operational flow:
  1. Connect to WiFi using multiple fallback methods
  2. Synchronize time with NTP server
  3. Continuously scan for WiFi networks and perform geolocation
  4. Calculate movement, speed, and bearing between locations
  5. Generate NMEA sentences for GPS compatibility
  6. Send APRS position reports with telemetry data when moving or on schedule
  7. Implement SmartBeaconing algorithm for adaptive reporting intervals

  Copyright (c) 2017-2025 Costin STROIE <costinstroie@eridu.eu.org>

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

// True if the tracker is being probed (used for telemetry bit setting)
bool PROBE = true;

// Led
#ifdef ESP32
  #define LED 2  // ESP32 default LED pin
#else
  #define LED 2  // ESP8266 default LED pin
#endif

// User settings
#include "config.h"

// Platform compatibility
#include "platform.h"

// Software version
#include "version.h"

// WiFi
#ifdef ESP32
  #include <WiFi.h>
  #include <WiFiUdp.h>
  #include <WiFiManager.h>
#else
  #include <ESP8266WiFi.h>
  #include <WiFiUdp.h>
  #include <WiFiManager.h>
#endif

#ifdef WIFI_SSIDPASS
#ifdef ESP32
const char wifiSP[] = WIFI_SSIDPASS;
#else
const char wifiSP[] PROGMEM = WIFI_SSIDPASS;
#endif
const char *wifiRS          = WIFI_RS;
const char *wifiFS          = WIFI_FS;
#endif

// OTA
#ifdef ESP32
  #include <ESPmDNS.h>
#else
  #include <ESP8266mDNS.h>
#endif
#include <ArduinoOTA.h>
int otaPort     = 8266;
bool otaSecure  = false;  // Flag to indicate if OTA password is set

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

#ifdef HAVE_OLED
// OLED
#include <U8x8lib.h>
#ifdef ESP32
U8X8_SSD1306_128X32_UNIVISION_HW_I2C u8x8(/* clock=*/ 16, /* data=*/ 17, /* reset=*/ U8X8_PIN_NONE);
#else
U8X8_SSD128X32_UNIVISION_HW_I2C u8x8(16);
#endif
#endif

// Set ADC to Voltage
#ifdef ESP8266
  ADC_MODE(ADC_VCC);
#endif

// Timings for geolocation and reporting cycles
unsigned long geoNextTime = 0;    ///< Next time to perform geolocation
unsigned long geoDelay    = 20;   ///< Delay between geolocation attempts (seconds)
unsigned long rpNextTime  = 0;    ///< Next time to send APRS report
unsigned long rpDelay     = 60;   ///< Current delay between APRS reports (seconds)
unsigned long rpDelayStep = 30;   ///< Step to increase delay when stationary (seconds)
unsigned long rpDelayMin  = 60;   ///< Minimum delay between APRS reports (seconds)
unsigned long rpDelayMax  = 1800; ///< Maximum delay between APRS reports (seconds)
unsigned long nmeaNextTime = 0;   ///< Next time to send NMEA sentences

// Exponentially smoothed values for better data quality
int sAcc = -1;  ///< Smoothed accuracy value
int sCrs = -1;  ///< Smoothed course/bearing value


/**
  Convert IPAddress to char array
  
  Formats an IP address as a string with optional padding for display alignment.
  
  @param ip IP address to convert
  @param buf Buffer to store the formatted IP string
  @param len Buffer length
  @param pad Whether to pad each octet to 3 characters (default false)
*/
void charIP(const IPAddress ip, char *buf, size_t len, boolean pad = false) {
#ifdef ESP32
  if (pad) snprintf(buf, len, "%3d.%3d.%3d.%3d", ip[0], ip[1], ip[2], ip[3]);
  else     snprintf(buf, len, "%d.%d.%d.%d",     ip[0], ip[1], ip[2], ip[3]);
#else
  if (pad) snprintf_P(buf, len, PSTR("%3d.%3d.%3d.%3d"), ip[0], ip[1], ip[2], ip[3]);
  else     snprintf_P(buf, len, PSTR("%d.%d.%d.%d"),     ip[0], ip[1], ip[2], ip[3]);
#endif
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

#ifdef HAVE_OLED
    // Display
    u8x8.clear();
    u8x8.draw1x2String(0, 0, WiFi.SSID().c_str());
    u8x8.setCursor(0, 2); u8x8.print("IP "); u8x8.print(ipbuf);
    //u8x8.setCursor(0, 3); u8x8.print("GW "); u8x8.print(gwbuf);
    //u8x8.setCursor(0, 4); u8x8.print("NS "); u8x8.print(nsbuf);
    delay(1000);
#endif
  }
  else
    Serial.print(F("$PWIFI,ERR\r\n"));
  yield();
}

/**
  Control the built-in LED with PWM for status indication
  
  Uses PWM to control LED brightness for different status levels.
  Higher load values produce brighter LED output.
  
  @param load LED brightness level (0-10, where 0 is off, 10 is brightest)
*/
void setLED(int load) {
  // Calculate PWM level (0-1023) based on load
  int level = (1 << load) - 1;
#ifdef ESP32
  // Set LED brightness (ESP32 uses 0-255 for analogWrite)
  analogWrite(LED, 255 - (level >> 2));  // Scale down to 0-255 range
#else
  // Set LED brightness (inverted because LED is active low on ESP8266)
  analogWrite(LED, PWMRANGE - level);
#endif
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
  Test HTTPS server connectivity to verify internet access
  
  Attempts to establish a secure HTTPS connection to the specified server
  and sends a HEAD request to verify server availability.
  
  @param server Server hostname to test
  @param port Server port to connect to
  @param timeout Connection timeout in milliseconds
  @return true if connection and request successful, false otherwise
*/
bool wifiCheckHTTP(char* server, int port, int timeout = 10000) {
  bool result = false;
  WiFiClientSecure testClient;
  testClient.setTimeout(timeout);
  
  // Only use insecure connection when explicitly configured for testing
  #ifdef GEO_INSECURE
  if (strcmp(GEO_SERVER, "www.googleapis.com") == 0 && 
      strcmp(GEO_APIKEY, "USE_YOUR_GOOGLE_KEY") == 0) {
    testClient.setInsecure();
    Serial.println(F("$PSEC,WARNING,Using insecure HTTPS connection for geolocation testing"));
  }
  #endif
  
  char buf[64] = "";
  if (testClient.connect(server, port)) {
    Serial.printf_P(PSTR("$PHTTP,CON,%s,%d\r\n"), server, port);
    // Send a simple HEAD request to test server availability
    testClient.print("HEAD / HTTP/1.1\r\n");
    testClient.print("Host: "); testClient.print(server); testClient.print("\r\n");
    testClient.print("Connection: close\r\n\r\n");
    // Check the response
    int rlen = testClient.readBytesUntil('\r', buf, sizeof(buf) - 1);
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
  // Stop the test connection
  testClient.stop();
  // Return the result
  return result;
}

/**
  Attempt to connect to a WiFi network

  @param ssid WiFi SSID (NULL to use stored credentials)
  @param pass WiFi password
  @param timeout Connection timeout in seconds
  @return true if connection successful, false otherwise
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
#ifdef HAVE_OLED
  u8x8.clear();
  u8x8.draw1x2String(0, 0, "WiFi");
#endif
  // Different calls for different configurations
  if (ssid == NULL) WiFi.begin();
  else              WiFi.begin(_ssid, pass);
#ifdef HAVE_OLED
  // Display
  u8x8.setCursor(0, 2);
  u8x8.print(_ssid);
  u8x8.setCursor(0, 3);
  u8x8.print("---------------");
  u8x8.setCursor(0, 3);
#endif
  // Check the status
  int tries = 0;
  while (!WiFi.isConnected() and tries < timeout) {
    tries++;
    Serial.printf_P(PSTR("$PWIFI,TRY,%d/%d,%s\r\n"), tries, timeout, _ssid);
#ifdef HAVE_OLED
    u8x8.print("|");
#endif
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
  Attempt to connect to a list of pre-configured WiFi networks

  @return true if connection to any known network successful, false otherwise
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
  Attempt to connect to open (unsecured) WiFi networks

  @return true if connection to any open network successful, false otherwise
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
#ifdef HAVE_OLED
  u8x8.clear();
  u8x8.draw1x2String(0, 0, wifiMgr->getConfigPortalSSID().c_str());
#endif
  setLED(10);
}

/**
  Main WiFi connection handler with multiple fallback methods

  Connection attempts in order:
  1. Pre-configured credentials (if WIFI_SSID defined)
  2. Stored WiFi credentials
  3. Known networks list
  4. Open networks
  5. WiFi Manager captive portal

  Implements a comprehensive connection strategy with multiple fallbacks
  to ensure device can connect to WiFi in various environments.

  @param timeout WiFi Manager timeout in seconds
  @return true if connection successful, false otherwise
*/
bool wifiConnect(int timeout = 300) {
  // Set the host name for network identification
  WiFi.hostname(NODENAME);
  // Set WiFi mode to station only
  WiFi.mode(WIFI_STA);
  // Disable auto-connect to prevent conflicts with our connection logic
  //WiFi.setAutoConnect(false);
  // Turn on LED to indicate connection attempt
  setLED(1);

  // Keep the connection result
  bool result = true;
  // Try to connect to WiFi using various methods
#ifdef WIFI_SSID
  // Use pre-configured credentials if defined
  wifiTryConnect(WIFI_SSID, WIFI_PASS);
#else
  // Check if already connected, then try multiple fallback methods
  if (!WiFi.isConnected()) {
    // Keep the saved credentials for reference
    char savedSSID[WL_SSID_MAX_LENGTH];
    char savedPSK[WL_WPA_KEY_MAX_LENGTH];
    strncpy(savedSSID, WiFi.SSID().c_str(), WL_SSID_MAX_LENGTH);
    strncpy(savedPSK, WiFi.psk().c_str(), WL_WPA_KEY_MAX_LENGTH);
    // Try connection methods in order of preference:
    // 1. Saved credentials
    if (not wifiTryConnect()) {
      // 2. Known networks from configuration
      if (not wifiTryKnownNetworks()) {
        // 3. Open networks
        if (not wifiTryOpenNetworks()) {
          // 4. WiFi Manager captive portal as last resort
          WiFiManager wifiManager;
          wifiManager.setTimeout(timeout);
          wifiManager.setAPCallback(wifiCallback);
          setLED(10);  // Bright LED to indicate captive portal mode
          if (not wifiManager.startConfigPortal(NODENAME)) {
            setLED(2);  // Dim LED to indicate failure
            result = false;
          }
        }
      }
    }
  }
#endif
  // Turn off LED when connection process completes
  setLED(0);

  return result;
}

/**
  Broadcast data via UDP to the local network
  
  Sends NMEA sentences and other data to all devices on the local network
  using UDP broadcast. Calculates the broadcast address based on the
  subnet mask and gateway IP.
  
  @param buf Data buffer to broadcast
  @param len Length of data to broadcast
*/
void broadcast(char *buf, size_t len) {
  // Calculate broadcast IP: invert subnet mask and OR with gateway IP
  bcastIP = IPAddress((~ (uint32_t)WiFi.subnetMask()) | ((uint32_t)WiFi.gatewayIP()));

  // Send the UDP packet
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
#ifdef HAVE_OLED
  // Init the display
  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.setContrast(2);
#endif
  // Compose the NMEA welcome message
  nmea.getWelcome(NODENAME, VERSION);
  Serial.print(nmea.welcome);
  Serial.print(F("$PGPL3,This program comes with ABSOLUTELY NO WARRANTY.\r\n"));
  Serial.print(F("$PGPL3,This is free software, and you are welcome \r\n"));
  Serial.print(F("$PGPL3,to redistribute it under certain conditions.\r\n"));
#ifdef HAVE_OLED
  // Display
  u8x8.draw2x2String(4, 0, NODENAME);
#ifdef CALLSIGN
  u8x8.drawString((16 - strlen(CALLSIGN)) / 2, 3, CALLSIGN);
#else
  u8x8.drawString((16 - strlen(VERSION)) / 2, 3, VERSION);
#endif
#endif
  delay(1000);

  // Initialize the LED pin as an output
  pinMode(LED, OUTPUT);
  setLED(0);

  // Try to connect, for ever
  unsigned int connectionAttempts = 0;
  while (not wifiConnect(300)) {
    connectionAttempts++;
    Serial.printf_P(PSTR("$PERR,WIFI,Connection attempt %d failed, retrying...\r\n"), connectionAttempts);
    delay(5000); // Wait 5 seconds before retrying
  }

  // OTA Update
  ArduinoOTA.setPort(otaPort);
  ArduinoOTA.setHostname(NODENAME);
#ifdef OTA_PASS
  if (strlen(OTA_PASS) >= 8) {  // Require minimum password length for security
    ArduinoOTA.setPassword((const char *)OTA_PASS);
    otaSecure = true;
    Serial.println(F("$POTA,SEC,Password protection enabled"));
  } else if (strlen(OTA_PASS) > 0) {
    Serial.println(F("$PSEC,WARNING,OTA password too short, minimum 8 characters required"));
  }
#endif

  ArduinoOTA.onStart([]() {
    Serial.print(F("$POTA,STA\r\n"));
#ifdef HAVE_OLED
    u8x8.clear();
    u8x8.draw1x2String(3, 0, "OTA Update");
    u8x8.drawString(2, 3, "[----------]");
#endif
  });

  ArduinoOTA.onEnd([]() {
    Serial.print(F("\r\n$POTA,FIN\r\n"));
#ifdef HAVE_OLED
    u8x8.clear();
#endif
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static int otaProgress = 0;
    int otaPrg = progress / (total / 100);
    if (otaProgress != otaPrg) {
      otaProgress = otaPrg;
      Serial.printf_P(PSTR("$POTA,PRG,%u%%\r\n"), otaProgress);
#ifdef HAVE_OLED
      if (otaProgress < 100)
        u8x8.drawString(3 + otaProgress / 10, 3, "|");
#endif
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
  aprs.setCallSign(CALLSIGN);
  Serial.print(F("$PAPRS,AUTH,")); Serial.print(aprs.aprsCallSign);
  Serial.print(","); Serial.print(aprs.aprsPassCode);
  //Serial.print(","); Serial.print(aprs.aprsObjectNm);
  Serial.print("\r\n");

  // Hardware data
#ifdef ESP8266
  int hwVcc  = ESP.getVcc();
  Serial.print(F("$PHWMN,VCC,"));
  Serial.print((float)hwVcc / 1000, 3);
  Serial.print("\r\n");
#endif

  // Initialize the random number generator and set the APRS telemetry start sequence
  // Use a more secure seed combining multiple entropy sources
  uint32_t secureSeed = (uint32_t)(ntp.getSeconds(false) + hwVcc + millis() + ESP.getChipId());
  // Apply bit mixing to improve randomness
  secureSeed ^= (secureSeed << 13);
  secureSeed ^= (secureSeed >> 17);
  secureSeed ^= (secureSeed << 5);
  randomSeed(secureSeed);
  // Set initial telemetry sequence number to random value to avoid conflicts
  aprs.aprsTlmSeq = random(1000);
  Serial.printf_P(PSTR("$PHWMN,TLM,%d\r\n"), aprs.aprsTlmSeq);

  // Start NMEA TCP server
  nmeaServer.init("nmea-0183", nmea.welcome);
}

/**
  Main Arduino loop - Implements the core operational logic
  
  This function implements the main operational cycle:
  1. Handle OTA updates
  2. Manage NMEA TCP clients
  3. Send NMEA sentences every second
  4. Perform geolocation at regular intervals
  5. Send APRS reports based on movement and SmartBeaconing algorithm
  6. Update telemetry data and status indicators
  
  The loop implements a state machine that balances:
  - Frequent position updates for accuracy
  - Adaptive reporting intervals (SmartBeaconing)
  - Resource conservation during stationary periods
  - Robust error handling and recovery
*/
void loop() {
  // Handle OTA firmware updates
  ArduinoOTA.handle();
  yield();

  // Handle NMEA TCP client connections
  nmeaServer.check();

  // Get current uptime in seconds
  unsigned long now = millis() / 1000;

  // Send NMEA sentences every second for GPS compatibility
  if (now >= nmeaNextTime) {
    // Get current time from NTP
    unsigned long utm = ntp.getSeconds();
    
    // Send NMEA sentences to all outputs (serial, TCP, UDP)
    sendNMEASentences(utm, false);
    
    // Schedule next NMEA output for 1 second later
    nmeaNextTime = now + 1;
  }

  // Perform geolocation when it's time
  if (now >= geoNextTime) {
    // Ensure WiFi connectivity with shorter timeout
    if (!WiFi.isConnected()) wifiConnect(60);

    // Set APRS telemetry status bits for current conditions
    // Bit 7: Tracker probe status
    if (PROBE) aprs.aprsTlmBits = B10000000;
    else       aprs.aprsTlmBits = B00000000;
    // Bit 0: Time accuracy (set if NTP time is invalid)
    if (!ntp.valid) aprs.aprsTlmBits |= B00000001;
    // Bit 1: Recent reboot (set if uptime < 1 day)
    if (millis() < 86400000UL) aprs.aprsTlmBits |= B00000010;

    // Turn on LED to indicate geolocation in progress
    setLED(4);

    // Get the time of the geolocation attempt
    unsigned long utm = ntp.getSeconds();

    // Scan for nearby WiFi access points
    Serial.print(F("$PSCAN,WIFI,"));
    int found = mls.wifiScan(false);

    // Process geolocation if WiFi networks were found
    if (found > 0) {
      // Increase LED brightness during geolocation
      setLED(6);
      Serial.print(found);
      Serial.print(","); Serial.print(ntp.getSeconds() - utm);
      Serial.print("s\r\n");

      // Perform geolocation using WiFi data
      int acc = mls.geoLocation();
      // Return LED to normal brightness
      setLED(4);

      // Exponentially smooth the accuracy for better data quality
      // This applies a 75% smoothing factor to reduce jitter
      if (sAcc < 0) sAcc = acc;  // Initialize on first reading
      else          sAcc = (((sAcc << 2) - sAcc + acc) + 2) >> 2;  // Smoothed average

#ifdef HAVE_OLED
      // Update OLED display with current time
      u8x8.clear();
      char bufClock[20];
      ntp.getClock(bufClock, sizeof(bufClock), utm);
      u8x8.setCursor(0, 3); u8x8.print("UTC "); u8x8.print(bufClock);
#endif

      // Process location data if geolocation was successful
      if (mls.current.valid) {
        // Report successful geolocation
        Serial.print(F("$PSCAN,FIX,"));
        Serial.print(mls.current.latitude, 6);  Serial.print(",");
        Serial.print(mls.current.longitude, 6); Serial.print(",");
        Serial.print(mls.locator);              Serial.print(",");
        Serial.print(acc);                      Serial.print("m,");
        Serial.print(ntp.getSeconds() - utm);   Serial.print("s");
#ifdef HAVE_OLED
        // Update OLED display with location information
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
#endif

        // Check if the device is moving by comparing distance to accuracy threshold
        bool moving = mls.getMovement() >= (sAcc >> 2);  // Movement threshold = 25% of accuracy
        if (moving) {
          // Exponentially smooth the bearing for better directional data
          // This applies a 75% smoothing factor to reduce bearing jitter
          if (sCrs < 0) sCrs = mls.bearing;  // Initialize on first reading
          else          sCrs = ((sCrs + (mls.bearing << 2) - mls.bearing) + 2) >> 2;  // Smoothed average
          // Report movement data
          Serial.print(",");
          Serial.print(mls.distance, 2);  Serial.print("m,");
          Serial.print(mls.speed, 2);     Serial.print("m/s,");
          Serial.print(mls.bearing);      Serial.print("'");
#ifdef HAVE_OLED
          // Update OLED display with movement information
          u8x8.setCursor(0, 2); u8x8.print("Spd "); u8x8.print(mls.speed, 2);
          u8x8.setCursor(9, 2); u8x8.print("Crs "); u8x8.print(sCrs);
#endif
        }
#ifdef HAVE_OLED
        else {
          // Display the locator when stationary
          u8x8.setCursor(0, 2); u8x8.print("Loc "); u8x8.print((char*)mls.locator);
        }
#endif
        Serial.print("\r\n");

        // Send updated NMEA sentences with current location
        sendNMEASentences(utm, true);

        // Collect system telemetry data
        int vcc  = ESP.getVcc();    // Battery voltage in mV
        int rssi = WiFi.RSSI();     // WiFi signal strength in dBm
        int heap = ESP.getFreeHeap(); // Free memory in bytes

        // Set telemetry bit 3 if battery voltage is outside normal range (3.3V ±10%)
        if (vcc < 3000 or vcc > 3600) aprs.aprsTlmBits |= B00001000;
        
        // Send APRS report if moving or scheduled reporting time has arrived
        if ((moving or (now >= rpNextTime)) and acc >= 0) {
          // Turn on LED to indicate APRS transmission
          setLED(8);

          // Connect to APRS-IS server and send position report
          if (aprs.connect()) {
            // Authenticate with APRS-IS server
            if (aprs.authenticate()) {
              // Local buffer for APRS comment (max 43 bytes)
              char buf[45] = "";
              // Prepare the comment with telemetry data
              snprintf_P(buf, sizeof(buf), PSTR("Acc:%d Dst:%d Spd:%d Crs:%s Vcc:%d.%d RSSI:%d"),
                         acc, (int)(mls.distance), (int)(3.6 * mls.speed), mls.getCardinal(sCrs),
                         vcc / 1000, (vcc % 1000) / 100, rssi);
              // Send position report with course, speed, and comment
              aprs.sendPosition(utm, mls.current.latitude, mls.current.longitude, sCrs, mls.knots, acc, buf);
              // Send telemetry data with system metrics
              // Speed conversion: mls.speed / 0.0008 = mls.speed * 1250
              aprs.sendTelemetry((vcc - 2500) / 4, -rssi, heap / 256, acc, (int)(sqrt(mls.speed * 1250)), aprs.aprsTlmBits);
              
              // Adjust reporting delay using SmartBeaconing algorithm
              if (moving) {
                // Reset delay to minimum when moving for frequent updates
                rpDelay = rpDelayMin;
                // Set telemetry bits 4 and 5 based on speed for status indication
                if (mls.speed > 10) aprs.aprsTlmBits |= B00100000;  // High speed
                else                aprs.aprsTlmBits |= B00010000;  // Low speed
              }
              else {
                // Increase delay when stationary to conserve bandwidth and battery
                rpDelay += rpDelayStep;
                if (rpDelay > rpDelayMax) rpDelay = rpDelayMax;  // Cap at maximum delay
              }
            }
            // Close APRS-IS connection
            aprs.stop();
          }

          // Handle APRS transmission errors by resetting to minimum delay
          if (aprs.error) {
            rpDelay = rpDelayMin;
            aprs.error = false;
          }

          // Schedule next APRS report based on current delay setting
          rpNextTime = now + rpDelay;

          // Turn off LED after APRS transmission
          setLED(0);
        }
      }
      else {
        // Report failed geolocation attempt
        Serial.printf_P(PSTR("$PSCAN,NOFIX,%dm,%ds\r\n"), acc, ntp.getSeconds() - utm);
#ifdef HAVE_OLED
        u8x8.print(" NFX");
#endif
      }
      // Schedule next geolocation attempt
      geoNextTime = now + geoDelay;
    }
    else {
      // No WiFi networks found - report and retry immediately
      Serial.print(F("0"));
      Serial.print(",");
      Serial.print(ntp.getSeconds() - utm);
      Serial.print("s\r\n");
      // Retry geolocation immediately
      geoNextTime = now;
    }

    // Turn off LED when geolocation cycle completes
    setLED(0);
  };
}

/**
  Send NMEA sentences to all configured outputs
  
  Generates and broadcasts NMEA-0183 sentences to:
  - Serial port for direct connection
  - TCP NMEA server for network clients
  - UDP broadcast for local network devices
  
  Implements conditional sentence generation based on:
  - Location validity
  - Movement status
  - Configuration flags
  
  @param utm Unix timestamp for time reference
  @param useCurrentLocation Whether to use current location data (not used in current implementation)
*/
void sendNMEASentences(unsigned long utm, bool useCurrentLocation) {
  char bufServer[200];
  int lenServer;
  
  // Determine location data to use
  float lat = 0.0;
  float lng = 0.0;
  int found = 0;
  
  // Use current valid location if available
  if (mls.current.valid) {
    lat = mls.current.latitude;
    lng = mls.current.longitude;
    found = 1;
  }
  // If no valid location, use default values (0.0, 0.0) and found = 0
  
  // Generate and send GGA sentence (GPS fix data)
  if (nmeaReport.gga) {
    lenServer = nmea.getGGA(bufServer, sizeof(bufServer), utm, lat, lng, found, found);
    Serial.print(bufServer);
    if (nmeaServer.clients) nmeaServer.sendAll(bufServer);
    broadcast(bufServer, lenServer);
  }
  // Generate and send RMC sentence (recommended minimum data) - only if valid location
  if (nmeaReport.rmc && found) {
    lenServer = nmea.getRMC(bufServer, sizeof(bufServer), utm, lat, lng, mls.knots, sCrs);
    Serial.print(bufServer);
    if (nmeaServer.clients) nmeaServer.sendAll(bufServer);
    broadcast(bufServer, lenServer);
  }
  // Generate and send GLL sentence (geographic position) - only if valid location
  if (nmeaReport.gll && found) {
    lenServer = nmea.getGLL(bufServer, sizeof(bufServer), utm, lat, lng);
    Serial.print(bufServer);
    if (nmeaServer.clients) nmeaServer.sendAll(bufServer);
    broadcast(bufServer, lenServer);
  }
  // Generate and send VTG sentence (track made good) - only if valid location and moving
  if (nmeaReport.vtg && found && mls.knots > 0) {
    lenServer = nmea.getVTG(bufServer, sizeof(bufServer), sCrs, mls.knots, (int)(mls.speed * 3.6));
    Serial.print(bufServer);
    if (nmeaServer.clients) nmeaServer.sendAll(bufServer);
    broadcast(bufServer, lenServer);
  }
  // Generate and send ZDA sentence (time and date) - always sent as it only requires time
  if (nmeaReport.zda) {
    lenServer = nmea.getZDA(bufServer, sizeof(bufServer), utm);
    Serial.print(bufServer);
    if (nmeaServer.clients) nmeaServer.sendAll(bufServer);
    broadcast(bufServer, lenServer);
  }
}

// vim: set ft=arduino ai ts=2 sts=2 et sw=2 sta nowrap nu :
