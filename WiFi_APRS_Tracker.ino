
// User settings
#include "UserSettings.h"

// WiFi
#include <ESP8266WiFi.h>

// Mozilla Location Services
#include "mls.h"
MLS mls;

// Network Time Protocol
#include "ntp.h"
NTP ntp;

// APRS
#include "aprs.h"
APRS aprs;


/**
  Main Arduino setup function
*/
void setup() {
  // Init the serial com
  Serial.begin(115200);
  Serial.println();

  // Set WiFi to station mode and disconnect from AP
  WiFi.mode(WIFI_STA);
  //WiFi.disconnect();
  //delay(100);

  // Configure NTP
  ntp.init("europe.pool.ntp.org");
  // Check the time and
  time_t utm = ntp.timeUNIX();

  // APRS
  aprs.init("cbaprs.de", 27235);
  aprs.setNTP(ntp);
}

/**
  Main Arduino loop
*/
void loop() {
  // Scan the WiFi access points
  Serial.print(F("Scanning WiFi networks...  "));
  int found = mls.wifiScan();
  Serial.print(found);
  Serial.println(" found");

  // Connect to WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print(F("Connecting to WiFi "));
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(1000);
    }
    Serial.println(" done");
  }

  // Get the coordinates
  if (found > 0) {
    Serial.println(F("Geolocating...  "));
    int acc = mls.geoLocation();
    Serial.print(F("  Lat: ")); Serial.println(mls.latitude, 6);
    Serial.print(F("  Lng: ")); Serial.println(mls.longitude, 6);
    Serial.print(F("  Acc: ")); Serial.println(acc);

    Serial.println(F("Vector...  "));
    mls.getVector();
    Serial.print(F("  Dst: ")); Serial.println(mls.distance, 6);
    Serial.print(F("  Spd: ")); Serial.println(mls.speed, 6);
    Serial.print(F("  Hed: ")); Serial.println(mls.bearing);
  }

  // APRS
  if (mls.netCount > 0) {
    aprs.connect();
    aprs.authenticate("IDDQD", "15095");
    aprs.sendPosition(mls.latitude, mls.longitude);
    aprs.stop();
  }
  Serial.println();
  delay(60000);
}
