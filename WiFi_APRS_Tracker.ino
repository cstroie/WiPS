
// User settings
#include "UserSettings.h"

// WiFi
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

// NTP
#include "ntp.h"

NTP ntp;

// Firefox geolocation services
const char geoServer[]        = "location.services.mozilla.com";
const int  geoPort            = 443;
const char geoPOST[] PROGMEM  = "POST /v1/geolocate?key=" GEO_APIKEY " HTTP/1.1";
const char eol[]     PROGMEM  = "\r\n";

struct BSSID_RSSI {
  uint8_t bssid[6];
  int8_t  rssi;
};

int geoLocation(float* lat, float* lng, struct BSSID_RSSI* nets, size_t found) {
  int acc = -1;
  *lat = 0.0;
  *lng = 0.0;


  WiFiClientSecure geoClient;
  geoClient.setTimeout(5000);

  if (geoClient.connect(geoServer, geoPort)) {
    const int bufSize = 250;
    char buf[bufSize] = "";

    /* Compose the geolocation request */
    // POST request
    strcpy_P(buf, geoPOST);
    strcat_P(buf, eol);
    geoClient.print(buf);
    Serial.print(buf);
    yield();
    // Host
    strcpy_P(buf, PSTR("Host: "));
    strcat(buf, geoServer);
    strcat_P(buf, eol);
    geoClient.print(buf);
    Serial.print(buf);
    yield();
    // User agent
    strcpy_P(buf, PSTR("User-Agent: Arduino-MLS/0.1"));
    strcat_P(buf, eol);
    geoClient.print(buf);
    Serial.print(buf);
    yield();
    // Content type
    strcpy_P(buf, PSTR("Content-Type: application/json"));
    strcat_P(buf, eol);
    geoClient.print(buf);
    Serial.print(buf);
    yield();
    // Content length: each line has 60 chars  LEN = COUNT * 60 + 24
    const int sbufSize = 8;
    char sbuf[sbufSize] = "";
    strcpy_P(buf, PSTR("Content-Length: "));
    itoa(24 + 60 * found, sbuf, 10);
    strncat(buf, sbuf, sbufSize);
    strcat_P(buf, eol);
    geoClient.print(buf);
    Serial.print(buf);
    yield();
    // Connection
    strcpy_P(buf, PSTR("Connection: close"));
    strcat_P(buf, eol);
    strcat_P(buf, eol);
    geoClient.print(buf);
    Serial.print(buf);
    yield();

    /* Payload */
    // First line in json
    strcpy_P(buf, PSTR("{\"wifiAccessPoints\": [\n"));
    geoClient.print(buf);
    Serial.print(buf);
    // One line per network
    for (size_t i = 0; i < found; ++i) {
      char sbuf[4] = "";
      // Open line
      strcpy_P(buf, PSTR("{\"macAddress\": \""));
      // BSSID from bytes to char array
      uint8_t* bss = nets[i].bssid;
      for (size_t b = 0; b < 6; b++) {
        itoa(bss[b], sbuf, 16);
        if (bss[b] < 16) strcat(buf, "0");
        strncat(buf, sbuf, 4);
        if (b < 5) strcat(buf, ":");
      }
      // RSSI
      strcat_P(buf, PSTR("\", \"signalStrength\": "));
      itoa(nets[i].rssi, sbuf, 10);
      strncat(buf, sbuf, 4);
      // Close line
      strcat(buf, "}");
      if (i < found - 1) strcat(buf, ",\n");
      // Send the line
      geoClient.print(buf);
      Serial.print(buf);
      yield();
    }
    // Last line in json
    strcpy_P(buf, PSTR("]}\n"));
    geoClient.print(buf);
    Serial.print(buf);

    /* Get the response */
    Serial.println();
    while (geoClient.connected()) {
      int rlen = geoClient.readBytesUntil('\r', buf, bufSize);
      buf[rlen] = '\0';
      Serial.print(buf);
      if (rlen == 1) break;
    }

    while (geoClient.connected()) {
      int rlen = geoClient.readBytesUntil(':', buf, bufSize);
      buf[rlen] = '\0';

      if (strstr_P(buf, PSTR("\"lat\"")))
        *lat = geoClient.parseFloat();
      else if (strstr_P(buf, PSTR("\"lng\"")))
        *lng = geoClient.parseFloat();
      else if (strstr_P(buf, PSTR("\"accuracy\"")))
        acc = geoClient.parseInt();
    }

    Serial.println();

    /* Close the connection */
    geoClient.stop();
  }

  return acc;
}

int geoWifiScan(struct BSSID_RSSI* nets, size_t maxnets) {
  size_t found = WiFi.scanNetworks();
  if (found > 0) {
    if (found > maxnets) found = maxnets;
    for (size_t i = 0; i < found; ++i) {
      memcpy(nets[i].bssid, WiFi.BSSID(i), 6);
      nets[i].rssi = (int8_t)(WiFi.RSSI(i));
    }
  }
  WiFi.scanDelete();
  return found;
}

/**
  Main Arduino setup function
*/
void setup() {
  // Init the serial com
  Serial.begin(115200);
  Serial.println();

  // Set WiFi to station mode and disconnect from AP
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Scan the WiFi access points
  const size_t maxnets = 64;
  struct BSSID_RSSI nets[maxnets];
  Serial.print(F("Scanning WiFi networks...  "));
  int found = geoWifiScan(nets, maxnets);
  Serial.print(found);
  Serial.println(" found");

  // Connect to WiFi
  Serial.print(F("Connecting to WiFi "));
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println(".");

  // Configure NTP
  ntp.init("europe.pool.ntp.org");
  // Check the time and set the telemetry bit 0 if time is not accurate
  unsigned long utm = ntp.timeUNIX();

  // Get the coordinates
  float lat, lng, acc;
  if (found > 0) {
    Serial.println(F("Geolocating...  "));
    int acc = geoLocation(&lat, &lng, nets, found);

    Serial.print(F("Lat: ")); Serial.println(lat, 6);
    Serial.print(F("Lng: ")); Serial.println(lng, 6);
    Serial.print(F("Acc: ")); Serial.println(acc);
  }

  // APRS
  if (found > 0) {
    char aprsLAT[8] = "";
    char aprsLNG[8] = "";

    int latDD = abs((int)lat);
    int latMM = (int)((abs(lat) - latDD) * 6000);
    int lngDD = abs((int)lng);
    int lngMM = (int)((abs(lng) - lngDD) * 6000);
    Serial.printf("APRS: %02d%02d.%02d%c/%03d%02d.%02d%c>\n", latDD, latMM / 100, latMM % 100, lat >= 0 ? 'N' : 'S', lngDD, lngMM / 100, lngMM % 100, lng >= 0 ? 'E' : 'W');
  }
}

/**
  Main Arduino loop
*/
void loop() {

}
