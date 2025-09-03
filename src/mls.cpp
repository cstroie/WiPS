/**
  geo.cpp - Mozilla Location Services Geolocation
         
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
#include "geo.h"
#include "platform.h"

GEO::GEO() {
}

void GEO::init() {
}

/**
  Scan for WiFi networks and collect BSSID/RSSI data from surrounding access points
  
  This function performs a WiFi network scan and stores the BSSID (MAC address)
  and RSSI (signal strength) of each detected network, excluding the currently
  connected access point to avoid biasing the geolocation.
  
  @param sort Whether to sort networks by RSSI strength (strongest first)
  @return Number of networks stored (excluding current AP)
*/
int GEO::wifiScan(bool sort) {
  // Keep the AP BSSID to exclude it from results
  uint8_t apBSSID[WL_MAC_ADDR_LENGTH];
  memcpy(apBSSID, WiFi.BSSID(), WL_MAC_ADDR_LENGTH);
  
  // Perform the WiFi scan
  netCount = WiFi.scanNetworks();
  
  // Keep only BSSID and RSSI, up to MAXNETS limit
  int scanCount = 0, storeCount = 0;
  
  // Only if there are any networks found
  if (netCount > 0) {
    // Limit to a maximum to prevent buffer overflow
    while (storeCount < MAXNETS and scanCount < netCount) {
      // Exclude the current AP BSSID from the list to avoid biasing geolocation
      if (memcmp(WiFi.BSSID(scanCount), apBSSID, WL_MAC_ADDR_LENGTH) != 0) {
        // Store the BSSID and RSSI
        memcpy(nets[storeCount].bssid, WiFi.BSSID(scanCount), WL_MAC_ADDR_LENGTH);
        nets[storeCount].rssi = (int8_t)(WiFi.RSSI(scanCount));
        storeCount++;
      }
      scanCount++;
    }
  }
  
  // Clear the scan results to free memory
  WiFi.scanDelete();
  
  // Update the network count to reflect stored networks
  netCount = storeCount;
  
  // Sort networks by RSSI strength if requested (strongest first)
  if (sort) {
    // Bubble sort implementation for RSSI descending order
    BSSID_RSSI tmp;
    for (size_t i = 1; i < netCount; i++) {
      for (size_t j = i; j > 0 && (nets[j - 1].rssi < nets[j].rssi); j--) {
        // Swap network entries
        memcpy(tmp.bssid, nets[j - 1].bssid, WL_MAC_ADDR_LENGTH);
        tmp.rssi = nets[j - 1].rssi;
        memcpy(nets[j - 1].bssid, nets[j].bssid, WL_MAC_ADDR_LENGTH);
        nets[j - 1].rssi = nets[j].rssi;
        memcpy(nets[j].bssid, tmp.bssid, WL_MAC_ADDR_LENGTH);
        nets[j].rssi = tmp.rssi;
      }
    }
  }
  
  // Return the number of networks stored
  return netCount;
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
int GEO::geoLocation() {
  int   err = -1;      // Error code
  int   acc = -1;      // Accuracy in meters
  float lat = 0.0;     // Latitude
  float lng = 0.0;     // Longitude

  // Create the secure WiFi client for HTTPS communication
  WiFiClientSecure geoClient;
  geoClient.setTimeout(5000);
  
  // Only use setInsecure() if GEO_APIKEY is not properly configured (for testing only)
  #ifdef GEO_INSECURE
  if (strcmp(GEO_SERVER, "www.googleapis.com") == 0 && 
      strcmp(GEO_APIKEY, "USE_YOUR_GOOGLE_KEY") == 0) {
    geoClient.setInsecure();
    Serial.println(F("$PSEC,WARNING,Using insecure HTTPS connection for geolocation testing"));
  }
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
    for (size_t i = 0; i < netCount; ++i) {
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

    // Invalidate previous coordinates if they're too old (over 1 hour)
    if (now - previous.uptm > 3600000UL) previous.valid = false;

    // Validate the received location data
    if (acc >= 0 and acc <= GEO_MAXACC) {
      // Valid location with acceptable accuracy
      
      // If we already had a valid location, save it as previous
      if (current.valid) {
        previous.valid      = current.valid;
        previous.latitude   = current.latitude;
        previous.longitude  = current.longitude;
        previous.uptm       = current.uptm;
      }
      
      // Store the new location data
      current.valid     = true;
      current.latitude  = lat;
      current.longitude = lng;
      current.uptm      = now;
      
      // Calculate and store the Maidenhead locator for this location
      getLocator(current.latitude, current.longitude);
    }
    else {
      // Invalid or inaccurate location data
      current.valid = false;
    }
  }

  // Handle error codes - return negative error code if present
  if (err > 0) acc = -err;

  // Return the accuracy in meters (positive) or error code (negative)
  return acc;
}

/**
  Calculate movement metrics between current and previous locations
  
  Computes the distance traveled, speed, and bearing between the previous
  and current location fixes. Updates internal member variables for:
  - distance (meters)
  - speed (meters/second)
  - knots (knots, rounded)
  - bearing (degrees from previous to current location)
  
  This function should be called after a successful geolocation to determine
  if the device has moved and in what direction/speed.
  
  @return Distance traveled in meters
*/
long GEO::getMovement() {
  // Check if both current and previous geolocations are valid
  if (current.valid and previous.valid) {
    // Compute the great-circle distance between locations using haversine formula
    distance = getDistance(previous.latitude, previous.longitude, current.latitude, current.longitude);
    
    // Calculate speed in meters/second
    // Time difference is in milliseconds, so multiply by 1000 to convert to seconds
    speed = 1000.0 * distance / (current.uptm - previous.uptm);
    
    // Convert speed to knots (1 knot = 0.514444 m/s)
    knots = lround(speed * 1.94384449);
    
    // If moving (non-zero speed), calculate the bearing from previous to current location
    if (knots > 0) {
      bearing = getBearing(previous.latitude, previous.longitude, current.latitude, current.longitude);
    }
  }
  else {
    // Invalid coordinates - set default values for stationary state
    distance = 0;
    speed    = 0.0;
    knots    = 0;
    bearing  = -1;  // Invalid bearing indicator
  }
  
  // Return the distance traveled
  return (long)distance;
}

/**
  Calculate great-circle distance between two coordinates using haversine formula
  
  Uses the haversine formula to compute the shortest distance between two points
  on a sphere (Earth). The Earth's radius is assumed to be 6372795 meters.
  
  Formula: a = sin²(Δφ/2) + cos φ1 ⋅ cos φ2 ⋅ sin²(Δλ/2)
           c = 2 ⋅ atan2( √a, √(1−a) )
           d = R ⋅ c
  
  @param lat1 Latitude of first point in decimal degrees
  @param long1 Longitude of first point in decimal degrees
  @param lat2 Latitude of second point in decimal degrees
  @param long2 Longitude of second point in decimal degrees
  @return Distance in meters
*/
float GEO::getDistance(float lat1, float long1, float lat2, float long2) {
  // Convert longitude difference to radians
  float delta = radians(long1 - long2);
  float sdlong = sin(delta);
  float cdlong = cos(delta);
  
  // Convert latitudes to radians
  lat1 = radians(lat1);
  lat2 = radians(lat2);
  float slat1 = sin(lat1);
  float clat1 = cos(lat1);
  float slat2 = sin(lat2);
  float clat2 = cos(lat2);
  
  // Haversine formula calculations for great-circle distance
  delta = (clat1 * slat2) - (slat1 * clat2 * cdlong);
  delta = sq(delta);
  delta += sq(clat2 * sdlong);
  delta = sqrt(delta);
  float denom = (slat1 * slat2) + (clat1 * clat2 * cdlong);
  delta = atan2(delta, denom);
  
  // Multiply by Earth's radius in meters (6372795m for WGS84 sphere approximation)
  return delta * 6372795;
}

/**
  Calculate initial bearing (forward azimuth) between two coordinates
  
  Computes the initial bearing (forward azimuth) from point 1 to point 2
  using spherical trigonometry. The result is normalized to 0-359 degrees.
  
  Formula: θ = atan2( sin(Δλ) ⋅ cos(φ2), cos(φ1) ⋅ sin(φ2) − sin(φ1) ⋅ cos(φ2) ⋅ cos(Δλ) )
  
  @param lat1 Latitude of first point in decimal degrees
  @param long1 Longitude of first point in decimal degrees
  @param lat2 Latitude of second point in decimal degrees
  @param long2 Longitude of second point in decimal degrees
  @return Bearing in degrees (0-359)
*/
int GEO::getBearing(float lat1, float long1, float lat2, float long2) {
  // Convert coordinate differences to radians
  float dlon = radians(long2 - long1);
  lat1 = radians(lat1);
  lat2 = radians(lat2);
  
  // Spherical trigonometry calculations for bearing
  float a1 = sin(dlon) * cos(lat2);
  float a2 = sin(lat1) * cos(lat2) * cos(dlon);
  a2 = cos(lat1) * sin(lat2) - a2;
  a2 = atan2(a1, a2);
  
  // Convert radians to degrees and normalize to 0-359 range
  return (int)(degrees(a2) + 360) % 360;
}

/**
  Convert bearing in degrees to 16-point compass direction abbreviation
  
  Maps a bearing angle (0-359 degrees) to the corresponding 16-point
  compass direction (N, NNE, NE, ENE, E, etc.).
  
  The 16 directions are spaced every 22.5 degrees (360/16), with
  the first sector (N) centered at 0/360 degrees.
  
  @param course Bearing in degrees (0-359)
  @return Pointer to cardinal direction string
*/
const char* GEO::getCardinal(int course) {
  // 16-point compass directions in order
  static const char* directions[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                     "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
                                    };
  
  // Map course to direction index (each sector is 22.5 degrees)
  // Add 11.25 (half sector) to center the ranges properly
  int direction = (int)(((float)course + 11.25f) / 22.5f);
  
  // Return the direction string (wrap around for 360 degrees)
  return directions[direction % 16];
}

/**
  Convert latitude/longitude coordinates to 6-character Maidenhead grid locator
  
  Calculates the Maidenhead locator (QRA locator) for given coordinates.
  The locator system divides the world into nested grids:
  - First pair (A-Z): 20° longitude × 10° latitude fields
  - Second pair (0-9): 2° longitude × 1° latitude squares
  - Third pair (a-x): 5' longitude × 2.5' latitude subsquares
  
  @param lat Latitude in decimal degrees (-90 to +90)
  @param lng Longitude in decimal degrees (-180 to +180)
*/
void GEO::getLocator(float lat, float lng) {
  int o1, o2, o3;  // Longitude field, square, subsquare
  int a1, a2, a3;  // Latitude field, square, subsquare
  float rem;       // Remainder for calculations

  // Calculate longitude components
  rem = lng + 180.0;           // Normalize to 0-360 range
  o1 = (int)(rem / 20.0);      // Field (20° divisions: A-Z)
  rem = rem - (float)o1 * 20.0;
  o2 = (int)(rem / 2.0);       // Square (2° divisions: 0-9)
  rem = rem - 2.0 * (float)o2;
  o3 = (int)(12.0 * rem);      // Subsquare (5' divisions: a-x)

  // Calculate latitude components
  rem = lat + 90.0;            // Normalize to 0-180 range
  a1 = (int)(rem / 10.0);      // Field (10° divisions: A-Z)
  rem = rem - (float)a1 * 10.0;
  a2 = (int)(rem);             // Square (1° divisions: 0-9)
  rem = rem - (float)a2;
  a3 = (int)(24.0 * rem);      // Subsquare (2.5' divisions: a-x)

  // Create the 6-character locator string
  locator[0] = (char)o1 + 'A';  // Longitude field (A-Z)
  locator[1] = (char)a1 + 'A';  // Latitude field (A-Z)
  locator[2] = (char)o2 + '0';  // Longitude square (0-9)
  locator[3] = (char)a2 + '0';  // Latitude square (0-9)
  locator[4] = (char)o3 + 'a';  // Longitude subsquare (a-x)
  locator[5] = (char)a3 + 'a';  // Latitude subsquare (a-x)
  locator[6] = (char)0;         // Null terminator
}
