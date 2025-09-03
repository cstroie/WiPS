/**
  geo-wiggle.cpp - Google Location Services Geolocation

  Implementation of WiFi-based geolocation using Mozilla Location Services
  and Google Geolocation API. Handles WiFi scanning, HTTPS communication,
  JSON parsing, and coordinate calculations.

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

#include "Arduino.h"
#include "geo-wiggle.h"
#include "platform.h"

// End of line string
static const char eol[]    PROGMEM = "\r\n";


WIGGLE::WIGGLE() {
}

void WIGGLE::init() {
}

/**
  Perform geolocation using collected WiFi data via Mozilla Location Services or Google Geolocation API
  
  This function sends the collected BSSID/RSSI data to a geolocation service over HTTPS
  and parses the JSON response to extract latitude, longitude, and accuracy information.
  
  The process involves:
  1. Creating a secure HTTPS connection to the geolocation service
  2. Building a JSON request with WiFi access point data
  3. Sending an HTTP POST request with the JSON payload
  4. Reading and parsing the JSON response
  5. Extracting location coordinates and accuracy
  6. Updating internal location state and Maidenhead locator
  
  @return Accuracy in meters if successful, negative error code on failure
*/
int WIGGLE::geoLocation(geo_t* loc, nets_t* nets, int netCount) {
  int   err = -1;      // Error code
  int   acc = -1;      // Accuracy in meters
  float lat = 0.0;     // Latitude
  float lng = 0.0;     // Longitude

  // Create the secure WiFi client for HTTPS communication
  WiFiClientSecure geoClient;
  geoClient.setTimeout(5000);
  
  // Set up authentication - assuming GEO_APIKEY is defined in config
  #ifdef GEO_INSECURE
  geoClient.setInsecure();
  Serial.println(F("$PSEC,WARNING,Using insecure HTTPS connection for geolocation testing"));
  #endif

  // Try to connect to the geolocation server
  if (geoClient.connect(geoServer, geoPort)) {
    // Local buffer for HTTP request construction
    const int bufSize = 250;
    char buf[bufSize] = "";
  
    // Record the current time for location timestamp
    unsigned long now = millis();

    // Send HTTP request headers for ESP8266 and ESP32
    // Request line with query parameters
    strcpy_P(buf, PSTR("GET /api/v2/network/search?"));
    // Build the query parameters for the BSSID with highest RSSI (Wigle API searches by netid/BSSID)
    if (netCount > 0) {
      // Find the BSSID with the highest RSSI
      int bestIndex = 0;
      for (int i = 1; i < netCount; i++) {
        if (nets[i].rssi > nets[bestIndex].rssi) {
          bestIndex = i;
        }
      }
      
      char bssidParam[50];
      strcpy_P(bssidParam, PSTR("netid="));
      // Convert best BSSID to hex string format
      char bssidStr[18];
      uint8_t* bss = nets[bestIndex].bssid;
      snprintf(bssidStr, sizeof(bssidStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               bss[0], bss[1], bss[2], bss[3], bss[4], bss[5]);
      strcat(bssidParam, bssidStr);
      strcat(buf, bssidParam);
    }
    strcat_P(buf, PSTR(" HTTP/1.1"));
    strcat_P(buf, eol);
    geoClient.print(buf);
    yield();
  
    // Host header
    strcpy_P(buf, PSTR("Host: "));
    strcat(buf, geoServer);
    strcat_P(buf, eol);
    geoClient.print(buf);
    yield();
  
    // Authorization header - Basic auth with API key
    strcpy_P(buf, PSTR("Authorization: Basic "));
    strcat(buf, GEO_WIGGLE_KEY);
    strcat_P(buf, eol);
    geoClient.print(buf);
    yield();
  
    // User agent header
    strcpy_P(buf, PSTR("User-Agent: Arduino-Wigle/0.1"));
    strcat_P(buf, eol);
    geoClient.print(buf);
    yield();
  
    // Connection header
    strcpy_P(buf, PSTR("Connection: close"));
    strcat_P(buf, eol);
    strcat_P(buf, eol);
    geoClient.print(buf);
    yield();

    Serial.println(buf);

    // Read and process HTTP response headers
    while (geoClient.connected()) {
      size_t rlen = geoClient.readBytesUntil('\r', buf, bufSize);
      buf[rlen] = '\0';
      Serial.println(buf);
      if (rlen == 1) break;  // Empty line indicates end of headers
    }

    // Parse the JSON response to extract location data
    bool success = false;
    bool foundResults = false;
    
    while (geoClient.connected() && geoClient.available()) {
      // Look for "success" field
      if (geoClient.find("\"success\"")) {
        geoClient.find(":");
        success = geoClient.parseInt() == 1;
      }
      
      // Look for "totalResults" field
      if (geoClient.find("\"totalResults\"")) {
        geoClient.find(":");
        int totalResults = geoClient.parseInt();
        foundResults = (totalResults > 0);
      }
      
      // Look for latitude and longitude in the first result
      if (foundResults && geoClient.find("\"trilat\"")) {
        geoClient.find(":");
        lat = geoClient.parseFloat();
      }
      
      if (foundResults && geoClient.find("\"trilong\"")) {
        geoClient.find(":");
        lng = geoClient.parseFloat();
      }
      
      // Look for accuracy (using range as proxy for accuracy)
      if (foundResults && geoClient.find("\"range\"")) {
        geoClient.find(":");
        acc = geoClient.parseInt();
      }
      
      break;
    }

    // Close the HTTPS connection
    geoClient.stop();

    // Validate the received location data
    if (success && foundResults && acc >= 0 && acc <= GEO_MAXACC) {
      // Valid location with acceptable accuracy
      
      // Store the new location data
      loc->valid     = true;
      loc->latitude  = lat;
      loc->longitude = lng;
      loc->uptm      = now;
    }
    else {
      // Invalid or inaccurate location data
      loc->valid = false;
      if (!success) err = 1;
      else if (!foundResults) err = 2;
      else err = 3;
    }
  }

  // Handle error codes - return negative error code if present
  if (err > 0) acc = -err;

  // Return the accuracy in meters (positive) or error code (negative)
  return acc;
}
