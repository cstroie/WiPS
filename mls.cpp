/**
  mls.cpp - Mozilla Location Services

  Copyright (c) 2017-2018 Costin STROIE <costinstroie@eridu.eu.org>

  This file is part of WiFi_APRS_Tracker.
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
  // Keep only the BSSID and RSSI
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
  int   acc = -1;
  float lat = 0.0;
  float lng = 0.0;

  // Create the secure WiFi
  WiFiClientSecure geoClient;
  geoClient.setTimeout(5000);

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
    }
    else {
      // No current valid coordinates
      current.valid     = false;
    }
  }

  // Return the geolocation accuracy
  return acc;
}

/**
  Get the navigation distance, bearing and speed using the equirectangular approximation
*/
// Haversine
// var φ1 = lat1.toRadians(), φ2 = lat2.toRadians(), Δλ = (lon2-lon1).toRadians(), R = 6371e3; // gives d in metres
// var d = Math.acos( Math.sin(φ1)*Math.sin(φ2) + Math.cos(φ1)*Math.cos(φ2) * Math.cos(Δλ) ) * R;
long MLS::getMovement() {
  // Check if the geolocation seems valid
  if (current.valid and previous.valid) {
    // Earth radius in meters
    float R = 6371000;
    // Equirectangular approximation
    float x = DEG_TO_RAD * (current.longitude - previous.longitude) * cos(DEG_TO_RAD * (current.latitude + previous.latitude) / 2);
    float y = DEG_TO_RAD * (current.latitude  - previous.latitude);
    // Compute the distance and speed
    distance = sqrt(x * x + y * y) * R;
    speed = 1000 * distance / (current.uptm - previous.uptm);
    knots = lround(speed * 1.94384449);
    // If moving, get the bearing
    if (knots > 0) {
      float course = RAD_TO_DEG * atan2(sin(current.longitude - previous.longitude) * cos(current.latitude),
                                        cos(previous.latitude) * sin(current.latitude) - sin(previous.latitude) * cos(current.latitude) * cos(current.longitude - previous.longitude));
      bearing = (int)(course + 360) % 360;
    }
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
