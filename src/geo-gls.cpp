/**
  geo-gls.cpp - Google Location Services Geolocation

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
#include "geo-gls.h"
#include "platform.h"

// End of line string
static const char eol[]    PROGMEM = "\r\n";


GLS::GLS() {
}

void GLS::init() {
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
int GLS::geoLocation(geo_t* loc, nets_t* nets, int netCount) {
  int   err = -1;      // Error code
  int   acc = -1;      // Accuracy in meters
  float lat = 0.0;     // Latitude
  float lng = 0.0;     // Longitude

  // Create the secure WiFi client for HTTPS communication
  WiFiClientSecure geoClient;
  geoClient.setTimeout(5000);
  
  // Only use setInsecure() if GEO_APIKEY is not properly configured (for testing only)
  #ifdef GEO_INSECURE
  geoClient.setInsecure();
  Serial.println(F("$PSEC,WARN,Using insecure HTTPS connection"));
  #endif

  // Try to connect to the geolocation server
  if (geoClient.connect(geoServer, geoPort)) {
    // Local buffer for HTTP request construction
    const int bufSize = 250;
    char buf[bufSize] = "";
  
    // Record the current time for location timestamp
    unsigned long now = millis();

  #ifdef ESP32
    // Send HTTP request headers for ESP32
    // Request line
    strcpy(buf, geoPOST);
    strcat(buf, eol);
    geoClient.print(buf);
    yield();
  
    // Host header
    strcpy(buf, "Host: ");
    strcat(buf, geoServer);
    strcat(buf, eol);
    geoClient.print(buf);
    yield();
  
    // User agent header
    strcpy(buf, "User-Agent: Arduino-MLS/0.1");
    strcat(buf, eol);
    geoClient.print(buf);
    yield();
  
    // Content type header
    strcpy(buf, "Content-Type: application/json");
    strcat(buf, eol);
    geoClient.print(buf);
    yield();
  
    // Content length header: each network entry is ~60 chars, plus 24 for JSON structure
    const int sbufSize = 8;
    char sbuf[sbufSize] = "";
    strcpy(buf, "Content-Length: ");
    itoa(24 + 60 * netCount, sbuf, 10);
    strncat(buf, sbuf, sbufSize);
    strcat(buf, eol);
    geoClient.print(buf);
    yield();
  
    // Connection header
    strcpy(buf, "Connection: close");
    strcat(buf, eol);
    strcat(buf, eol);
    geoClient.print(buf);
    yield();
  #else
    // Send HTTP request headers for ESP8266
    // Request line
    strcpy_P(buf, geoPOST);
    strcat_P(buf, eol);
    geoClient.print(buf);
    yield();
  
    // Host header
    strcpy_P(buf, PSTR("Host: "));
    strcat(buf, geoServer);
    strcat_P(buf, eol);
    geoClient.print(buf);
    yield();
  
    // User agent header
    strcpy_P(buf, PSTR("User-Agent: Arduino-MLS/0.1"));
    strcat_P(buf, eol);
    geoClient.print(buf);
    yield();
  
    // Content type header
    strcpy_P(buf, PSTR("Content-Type: application/json"));
    strcat_P(buf, eol);
    geoClient.print(buf);
    yield();
  
    // Content length header: each network entry is ~60 chars, plus 24 for JSON structure
    const int sbufSize = 8;
    char sbuf[sbufSize] = "";
    strcpy_P(buf, PSTR("Content-Length: "));
    itoa(24 + 60 * netCount, sbuf, 10);
    strncat(buf, sbuf, sbufSize);
    strcat_P(buf, eol);
    geoClient.print(buf);
    yield();
  
    // Connection header
    strcpy_P(buf, PSTR("Connection: close"));
    strcat_P(buf, eol);
    strcat_P(buf, eol);
    geoClient.print(buf);
    yield();
  #endif

    // Send JSON payload with WiFi access point data
    // Start of JSON object
    strcpy_P(buf, PSTR("{\"considerIp\": false, \"wifiAccessPoints\": [\n"));
    geoClient.print(buf);
    
    // Add each detected WiFi network's data
    for (int i = 0; i < netCount; ++i) {
      char sbuf[4] = "";
      
      // Start network object with MAC address
      strcpy_P(buf, PSTR("{\"macAddress\": \""));
      
      // Convert BSSID bytes to hex string with colons
      uint8_t* bss = nets[i].bssid;
      for (size_t b = 0; b < 6; b++) {
        itoa(bss[b], sbuf, 16);
        if (bss[b] < 16) strcat(buf, "0");  // Pad single digit hex values
        strncat(buf, sbuf, 4);
        if (b < 5) strcat(buf, ":");        // Add colon separators
      }
      
      // Add signal strength (RSSI)
      strcat_P(buf, PSTR("\", \"signalStrength\": "));
      itoa(nets[i].rssi, sbuf, 10);
      strncat(buf, sbuf, 4);
      
      // Add optional fields with default values
      strcat_P(buf, PSTR(", \"age\": 0"));                    // Age in milliseconds
      strcat_P(buf, PSTR(", \"channel\": 0"));                // Channel number
      strcat_P(buf, PSTR(", \"signalToNoiseRatio\": 0"));     // SNR in dB
      
      // Close network object
      strcat(buf, "}");
      if (i < netCount - 1) strcat(buf, ",\n");  // Add comma if not last item
      
      // Send the network data
      geoClient.print(buf);
      yield();
    }
    
    // Close JSON array and object
    strcpy_P(buf, PSTR("]}\n"));
    geoClient.print(buf);

    // Read and process HTTP response headers
    while (geoClient.connected()) {
      int rlen = geoClient.readBytesUntil('\r', buf, bufSize);
      buf[rlen] = '\0';
      if (rlen == 1) break;  // Empty line indicates end of headers
    }

    // Parse the JSON response to extract location data
    while (geoClient.connected()) {
      int rlen = geoClient.readBytesUntil(':', buf, bufSize);
      buf[rlen] = '\0';
      
      // Check for different JSON fields in the response
      if      (strstr_P(buf, PSTR("\"lat\"")))      lat = geoClient.parseFloat();      // Latitude
      else if (strstr_P(buf, PSTR("\"lng\"")))      lng = geoClient.parseFloat();      // Longitude
      else if (strstr_P(buf, PSTR("\"accuracy\""))) acc = geoClient.parseInt();       // Accuracy
      else if (strstr_P(buf, PSTR("\"error\""))) {
        // Error response - extract error code
        geoClient.find("\"code\":");
        err = geoClient.parseInt();
      }
    }

    // Close the HTTPS connection
    geoClient.stop();

    // Validate the received location data
    if (acc >= 0 and acc <= GEO_MAXACC) {
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
    }
  }

  // Handle error codes - return negative error code if present
  if (err > 0) acc = -err;

  // Return the accuracy in meters (positive) or error code (negative)
  return acc;
}
