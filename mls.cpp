/**
  mls.cpp - Mozilla Location Services

  Copyright (c) 2017 Costin STROIE <costinstroie@eridu.eu.org>

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
int MLS::wifiScan() {
  // Scan
  netCount = WiFi.scanNetworks();
  // Keep the significant date
  if (netCount > 0) {
    if (netCount > MAXNETS) netCount = MAXNETS;
    for (size_t i = 0; i < netCount; ++i) {
      // TODO exclude the network we are connected to
      memcpy(nets[i].bssid, WiFi.BSSID(i), 6);
      nets[i].rssi = (int8_t)(WiFi.RSSI(i));
    }
  }
  // Clear the scan results
  WiFi.scanDelete();
  // Return the number of networks found
  return netCount;
}

/**
  Geolocation. Store the coordinates in private variables

  @return the geolocation accuracy
*/
int MLS::geoLocation() {
  int acc = -1;
  float lat = 0.0;
  float lng = 0.0;

  /* WiFi client */
  WiFiClientSecure geoClient;
  geoClient.setTimeout(5000);

  if (geoClient.connect(geoServer, geoPort)) {
    // Local buffer
    const int bufSize = 250;
    char buf[bufSize] = "";

    /* Geolocation request header */
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

    /* Geolocation request payload */
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

    /* Get the geolocation response */
    //Serial.println();
    while (geoClient.connected()) {
      int rlen = geoClient.readBytesUntil('\r', buf, bufSize);
      buf[rlen] = '\0';
      //Serial.print(buf);
      if (rlen == 1) break;
    }

    /* Parse the result */
    while (geoClient.connected()) {
      int rlen = geoClient.readBytesUntil(':', buf, bufSize);
      buf[rlen] = '\0';
      if (strstr_P(buf, PSTR("\"lat\"")))           lat = geoClient.parseFloat();
      else if (strstr_P(buf, PSTR("\"lng\"")))      lng = geoClient.parseFloat();
      else if (strstr_P(buf, PSTR("\"accuracy\""))) acc = geoClient.parseInt();
    }
    //Serial.println();

    /* Close the connection */
    geoClient.stop();

    /* Check the data */
    if (acc >= 0 and acc <= GEO_MAXACC) {
      if (validCoords) {
        /* Store old data */
        validPrevCoords = validCoords;
        prevLatitude    = latitude;
        prevLongitude   = longitude;
        prevTime        = currTime;
      }
      /* Store new data */
      validCoords     = true;
      latitude        = lat;
      longitude       = lng;
      currTime        = millis();
    }
    else {
      validCoords     = false;
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
bool MLS::getVector() {
  // Check if the geolocation seems valid
  if (validCoords and validPrevCoords) {
    // Earth radius in meters
    float R = 6371000;
    // Equirectangular approximation
    float x = DEG_TO_RAD * (longitude - prevLongitude) * cos(DEG_TO_RAD * (latitude + prevLatitude) / 2);
    float y = DEG_TO_RAD * (latitude - prevLatitude);
    // Compute the distance and speed
    distance = sqrt(x * x + y * y) * R;
    speed = 1000 * distance / (currTime - prevTime);
    // Get the bearing
    float crs = RAD_TO_DEG * atan2(sin(longitude - prevLongitude) * cos(latitude),
                                   cos(prevLatitude) * sin(latitude) - sin(prevLatitude) * cos(latitude) * cos(longitude - prevLongitude));
    bearing = (int)(crs + 360) % 360;
    return true;
  }
  else
  {
    // Store an invalid distance
    distance = -1;
    return false;
  }
}
