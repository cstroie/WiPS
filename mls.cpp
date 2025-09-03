/**
  mls.cpp - Mozilla Location Services

  Copyright (c) 2017-2023 Costin STROIE <costinstroie@eridu.eu.org>

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
#include "mls.h"

MLS::MLS() {
}

void MLS::init() {
}

/**
  Scan the WiFi networks and store them in an array of structs

  @return the number of networks found
*/
int MLS::wifiScan(bool sort) {
  // Keep the AP BSSID
  uint8_t apBSSID[WL_MAC_ADDR_LENGTH];
  memcpy(apBSSID, WiFi.BSSID(), WL_MAC_ADDR_LENGTH);
  // Scan
  netCount = WiFi.scanNetworks();
  // Keep only BSSID and RSSI
  int scanCount = 0, storeCount = 0;
  // Only if there are any networks found
  if (netCount > 0) {
    // Limit to a maximum
    while (storeCount < MAXNETS and scanCount < netCount) {
      // Exclude the AP BSSID from the list
      if (memcmp(WiFi.BSSID(scanCount), apBSSID, WL_MAC_ADDR_LENGTH) != 0) {
        memcpy(nets[storeCount].bssid, WiFi.BSSID(scanCount), WL_MAC_ADDR_LENGTH);
        nets[storeCount].rssi = (int8_t)(WiFi.RSSI(scanCount));
        storeCount++;
      }
      scanCount++;
    }
  }
  // Clear the scan results
  WiFi.scanDelete();
  // Keep the number of networks found
  netCount = storeCount;
  if (sort) {
    // Sort the networks by RSSI, descending
    BSSID_RSSI tmp;
    for (size_t i = 1; i < netCount; i++) {
      for (size_t j = i; j > 0 && (nets[j - 1].rssi < nets[j].rssi); j--) {
        memcpy(tmp.bssid, nets[j - 1].bssid, WL_MAC_ADDR_LENGTH);
        tmp.rssi = nets[j - 1].rssi;
        memcpy(nets[j - 1].bssid, nets[j].bssid, WL_MAC_ADDR_LENGTH);
        nets[j - 1].rssi = nets[j].rssi;
        memcpy(nets[j].bssid, tmp.bssid, WL_MAC_ADDR_LENGTH);
        nets[j].rssi = tmp.rssi;
      }
    }
  }
  // Return the number of networks found
  return netCount;
}

/**
  Geolocation. Store the coordinates in private variables

  @return the geolocation accuracy
*/
int MLS::geoLocation() {
  int   err = -1;
  int   acc = -1;
  float lat = 0.0;
  float lng = 0.0;

  // Create the secure WiFi
  WiFiClientSecure geoClient;
  geoClient.setTimeout(5000);
  geoClient.setInsecure();

  // Try to connect
  if (geoClient.connect(geoServer, geoPort)) {
    // Local buffer
    const int bufSize = 250;
    char buf[bufSize] = "";
    // Keep the internal time
    unsigned long now = millis();

    // The geolocation request header
    strcpy_P(buf, geoPOST);
    strcat_P(buf, eol);
    geoClient.print(buf);
    //Serial.print(buf);
    yield();
    // Host
    strcpy_P(buf, PSTR("Host: "));
    strcat(buf, geoServer);
    strcat_P(buf, eol);
    geoClient.print(buf);
    //Serial.print(buf);
    yield();
    // User agent
    strcpy_P(buf, PSTR("User-Agent: Arduino-MLS/0.1"));
    strcat_P(buf, eol);
    geoClient.print(buf);
    //Serial.print(buf);
    yield();
    // Content type
    strcpy_P(buf, PSTR("Content-Type: application/json"));
    strcat_P(buf, eol);
    geoClient.print(buf);
    //Serial.print(buf);
    yield();
    // Content length: each line has 60 chars  LEN = COUNT * 60 + 24
    const int sbufSize = 8;
    char sbuf[sbufSize] = "";
    strcpy_P(buf, PSTR("Content-Length: "));
    itoa(24 + 60 * netCount, sbuf, 10);
    strncat(buf, sbuf, sbufSize);
    strcat_P(buf, eol);
    geoClient.print(buf);
    //Serial.print(buf);
    yield();
    // Connection
    strcpy_P(buf, PSTR("Connection: close"));
    strcat_P(buf, eol);
    strcat_P(buf, eol);
    geoClient.print(buf);
    //Serial.print(buf);
    yield();

    // The geolocation request payload
    // First line in json
    strcpy_P(buf, PSTR("{\"wifiAccessPoints\": [\n"));
    geoClient.print(buf);
    //Serial.print(buf);
    // One line per network
    for (size_t i = 0; i < netCount; ++i) {
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
      if (i < netCount - 1) strcat(buf, ",\n");
      // Send the line
      geoClient.print(buf);
      //Serial.print(buf);
      yield();
    }
    // Last line in json
    strcpy_P(buf, PSTR("]}\n"));
    geoClient.print(buf);
    //Serial.print(buf);

    // Get the geolocation response
    //Serial.println();
    while (geoClient.connected()) {
      int rlen = geoClient.readBytesUntil('\r', buf, bufSize);
      buf[rlen] = '\0';
      //Serial.print(buf);
      if (rlen == 1) break;
    }

    // Parse the result
    while (geoClient.connected()) {
      int rlen = geoClient.readBytesUntil(':', buf, bufSize);
      buf[rlen] = '\0';
      if      (strstr_P(buf, PSTR("\"lat\"")))      lat = geoClient.parseFloat();
      else if (strstr_P(buf, PSTR("\"lng\"")))      lng = geoClient.parseFloat();
      else if (strstr_P(buf, PSTR("\"accuracy\""))) acc = geoClient.parseInt();
      else if (strstr_P(buf, PSTR("\"code\"")))     err = geoClient.parseInt();
    }
    //Serial.println();

    // Close the connection
    geoClient.stop();

    // Check if previous valid coordinates are too old (over one hour) and invalidate them
    if (now - previous.uptm > 3600000UL) previous.valid = false;

    // Check the data
    if (acc >= 0 and acc <= GEO_MAXACC) {
      if (current.valid) {
        // Store previous coordinates
        previous.valid      = current.valid;
        previous.latitude   = current.latitude;
        previous.longitude  = current.longitude;
        previous.uptm       = current.uptm;
      }
      // Store new coordinates
      current.valid     = true;
      current.latitude  = lat;
      current.longitude = lng;
      current.uptm      = now;
      // Get the locator
      getLocator(current.latitude, current.longitude);
    }
    else {
      // No current valid coordinates
      current.valid     = false;
    }
  }

  // Check the error and return it as negative accuracy
  if (err > 0) acc = -err;

  // Return the geolocation accuracy
  return acc;
}

/**
  Get the navigation distance, bearing and speed using the equirectangular approximation
*/
long MLS::getMovement() {
  // Check if the geolocation seems valid
  if (current.valid and previous.valid) {
    // Compute the distance and speed
    distance = getDistance(previous.latitude, previous.longitude, current.latitude, current.longitude);
    speed = 1000 * distance / (current.uptm - previous.uptm);
    knots = lround(speed * 1.94384449);
    // If moving, get the bearing
    if (knots > 0)
      bearing = getBearing(previous.latitude, previous.longitude, current.latitude, current.longitude);
  }
  else {
    // Invalid coordinates, store zero distance and speed
    distance  = 0;
    speed     = 0;
    knots     = 0;
  }
  // Return the distance
  return (long)distance;
}

/**
  Returns distance in meters between two positions, both specified
  as signed decimal-degrees latitude and longitude. Uses great-circle
  distance computation for hypothetical sphere of radius 6372795 meters.
  Because Earth is no exact sphere, rounding errors may be up to 0.5%.
  Courtesy of Maarten Lamers
*/
float MLS::getDistance(float lat1, float long1, float lat2, float long2) {
  float delta = radians(long1 - long2);
  float sdlong = sin(delta);
  float cdlong = cos(delta);
  lat1 = radians(lat1);
  lat2 = radians(lat2);
  float slat1 = sin(lat1);
  float clat1 = cos(lat1);
  float slat2 = sin(lat2);
  float clat2 = cos(lat2);
  delta = (clat1 * slat2) - (slat1 * clat2 * cdlong);
  delta = sq(delta);
  delta += sq(clat2 * sdlong);
  delta = sqrt(delta);
  float denom = (slat1 * slat2) + (clat1 * clat2 * cdlong);
  delta = atan2(delta, denom);
  return delta * 6372795;
}

/**
  Returns course in degrees (North=0, West=270) from position 1 to position 2,
  both specified as signed decimal-degrees latitude and longitude.
  Because Earth is no exact sphere, calculated course may be off by a tiny fraction.
  Courtesy of Maarten Lamers
*/
int MLS::getBearing(float lat1, float long1, float lat2, float long2) {
  float dlon = radians(long2 - long1);
  lat1 = radians(lat1);
  lat2 = radians(lat2);
  float a1 = sin(dlon) * cos(lat2);
  float a2 = sin(lat1) * cos(lat2) * cos(dlon);
  a2 = cos(lat1) * sin(lat2) - a2;
  a2 = atan2(a1, a2);
  return (int)(degrees(a2) + 360) % 360;
}

/**
  Get the direction as cardinal point
*/
const char* MLS::getCardinal(int course) {
  static const char* directions[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                     "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
                                    };
  int direction = (int)(((float)course + 11.25f) / 22.5f);
  return directions[direction % 16];
}

/**
  Get the maidenhead locator
*/
void MLS::getLocator(float lat, float lng) {
  int o1, o2, o3;
  int a1, a2, a3;
  float rem;

  // Longitude
  rem = lng + 180.0;
  o1 = (int)(rem / 20.0);
  rem = rem - (float)o1 * 20.0;
  o2 = (int)(rem / 2.0);
  rem = rem - 2.0 * (float)o2;
  o3 = (int)(12.0 * rem);

  // Latitude
  rem = lat + 90.0;
  a1 = (int)(rem / 10.0);
  rem = rem - (float)a1 * 10.0;
  a2 = (int)(rem);
  rem = rem - (float)a2;
  a3 = (int)(24.0 * rem);

  locator[0] = (char)o1 + 'A';
  locator[1] = (char)a1 + 'A';
  locator[2] = (char)o2 + '0';
  locator[3] = (char)a2 + '0';
  locator[4] = (char)o3 + 'a';
  locator[5] = (char)a3 + 'a';
  locator[6] = (char)0;
}
