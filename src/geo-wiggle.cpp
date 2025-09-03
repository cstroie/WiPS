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
#ifdef ESP32
  #include <HTTPClient.h>
#else
  #include <ESP8266HTTPClient.h>
#endif

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

  // Create the secure HTTPS client
  HTTPClient https;
  WiFiClientSecure geoClient;
  
  // Set up authentication - assuming GEO_APIKEY is defined in config
  #ifdef GEO_INSECURE
  geoClient.setInsecure();
  Serial.println(F("$PSEC,WARNING,Using insecure HTTPS connection for geolocation testing"));
  #endif

  // Choose a random BSSID
  int bestIndex = 0;
  if (netCount > 0) {
    bestIndex = random(netCount);
  }

  // Build the URL with the best BSSID
  char url[200];
  if (netCount > 0) {
    char bssidStr[18];
    uint8_t* bss = nets[bestIndex].bssid;
    snprintf(bssidStr, sizeof(bssidStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             bss[0], bss[1], bss[2], bss[3], bss[4], bss[5]);
    snprintf_P(url, sizeof(url), PSTR("https://%s/api/v2/network/search?netid=%s"), geoServer, bssidStr);
  } else {
    snprintf_P(url, sizeof(url), PSTR("https://%s/api/v2/network/search"), geoServer);
  }

  // Begin the HTTPS request
  https.begin(geoClient, url);
  
  // Set headers
  https.addHeader("Authorization", "Basic " + String(GEO_WIGGLE_KEY));
  https.addHeader("User-Agent", "Arduino-Wigle/0.1");
  
  // Record the current time for location timestamp
  unsigned long now = millis();

  // Send HTTP GET request
  int httpResponseCode = https.GET();
  
  if (httpResponseCode == HTTP_CODE_OK) {
    // Get the response payload
    String response = https.getString();

    Serial.println(response); // For debugging
    
    // Parse the JSON response to extract location data
    bool success = false;
    bool foundResults = false;
    
    // Look for "success" field
    int successIndex = response.indexOf("\"success\"");
    if (successIndex != -1) {
      int colonIndex = response.indexOf(":", successIndex);
      if (colonIndex != -1) {
        // Check if the value is "true"
        success = (response.substring(colonIndex + 1, colonIndex + 5) == "true");
      }
    }
    
    // Look for "totalResults" field
    int totalResultsIndex = response.indexOf("\"totalResults\"");
    if (totalResultsIndex != -1) {
      int colonIndex = response.indexOf(":", totalResultsIndex);
      if (colonIndex != -1) {
        int commaIndex = response.indexOf(",", colonIndex);
        int endIndex = (commaIndex != -1) ? commaIndex : response.length();
        int totalResults = response.substring(colonIndex + 1, endIndex).toInt();
        foundResults = (totalResults > 0);
      }
    }
    
    // Look for latitude and longitude in the first result
    if (foundResults) {
      int trilatIndex = response.indexOf("\"trilat\"");
      if (trilatIndex != -1) {
        int colonIndex = response.indexOf(":", trilatIndex);
        if (colonIndex != -1) {
          int commaIndex = response.indexOf(",", colonIndex);
          int endIndex = (commaIndex != -1) ? commaIndex : response.length();
          lat = response.substring(colonIndex + 1, endIndex).toFloat();
        }
      }
      
      int trilongIndex = response.indexOf("\"trilong\"");
      if (trilongIndex != -1) {
        int colonIndex = response.indexOf(":", trilongIndex);
        if (colonIndex != -1) {
          int commaIndex = response.indexOf(",", colonIndex);
          int endIndex = (commaIndex != -1) ? commaIndex : response.length();
          lng = response.substring(colonIndex + 1, endIndex).toFloat();
        }
      }
      
      // Look for accuracy (using range as proxy for accuracy)
      int rangeIndex = response.indexOf("\"range\"");
      if (rangeIndex != -1) {
        int colonIndex = response.indexOf(":", rangeIndex);
        if (colonIndex != -1) {
          int commaIndex = response.indexOf(",", colonIndex);
          int endIndex = (commaIndex != -1) ? commaIndex : response.length();
          acc = response.substring(colonIndex + 1, endIndex).toInt();
        }
      }
    }

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
  } else {
    // HTTP request failed
    loc->valid = false;
    err = 4;
  }

  // Close the connection
  https.end();

  // Handle error codes - return negative error code if present
  if (err > 0) acc = -err;

  // Return the accuracy in meters (positive) or error code (negative)
  return acc;
}
