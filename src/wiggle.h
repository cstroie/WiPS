/**
  wiggle.h - WiGLE.net Geolocation API Client
         
  This library provides functionality for geolocation using the WiGLE.net API.
  It handles WiFi network scanning, API authentication, and location data parsing.

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

#ifndef WIGGLE_H
#define WIGGLE_H

#define MAXNETS 32

#include "Arduino.h"
#ifdef ESP32
  #include <WiFi.h>
  #include <HTTPClient.h>
#else
  #include <ESP8266WiFi.h>
#endif
#include <WiFiClientSecure.h>
#include "config.h"

// Define WiGLE API server
#define WIGLE_SERVER    "api.wigle.net"
#define WIGLE_PORT      443
#ifdef ESP32
  #define WIGLE_POST      "POST /api/v2/network/search HTTP/1.1"
#else
  #define WIGLE_POST      "POST /api/v2/network/search HTTP/1.1"
#endif

const char wigleServer[]      = WIGLE_SERVER;
const int  wiglePort          = WIGLE_PORT;
#ifdef ESP32
  const char wiglePOST[] = WIGLE_POST;
#else
  const char wiglePOST[] PROGMEM  = WIGLE_POST;
#endif

/**
  Geolocation data structure
  
  Stores latitude, longitude, validity flag, and timestamp for a location fix.
*/
struct geo_t {
  float         latitude;   ///< Latitude in decimal degrees
  float         longitude;  ///< Longitude in decimal degrees
  bool          valid;      ///< Validity flag indicating successful geolocation
  unsigned long uptm;       ///< Uptime timestamp when location was acquired
};

/**
  WiGLE.net API client class
  
  Provides methods for WiFi scanning, geolocation via WiGLE API, movement tracking,
  and coordinate conversions.
*/
class WIGGLE {
  public:
    /**
      Constructor - Initialize WIGGLE object with default values
    */
    WIGGLE();
    
    /**
      Initialize WIGGLE - currently empty but reserved for future use
    */
    void  init();
    
    /**
      Scan for WiFi networks and collect BSSID/RSSI data
      
      @param sort Whether to sort networks by RSSI strength
      @return Number of networks found and stored
    */
    int   wifiScan(bool sort);
    
    /**
      Perform geolocation using collected WiFi data via WiGLE.net API
      
      @return Accuracy in meters, or -1 on error
    */
    int   geoLocation();
    
    /**
      Calculate movement between current and previous locations
      
      @return Distance in meters, or -1 if locations invalid
    */
    long  getMovement();
    
    /**
      Calculate great-circle distance between two coordinates
      
      @param lat1 Latitude of first point in decimal degrees
      @param long1 Longitude of first point in decimal degrees
      @param lat2 Latitude of second point in decimal degrees
      @param long2 Longitude of second point in decimal degrees
      @return Distance in meters
    */
    float getDistance(float lat1, float long1, float lat2, float long2);
    
    /**
      Calculate initial bearing between two coordinates
      
      @param lat1 Latitude of first point in decimal degrees
      @param long1 Longitude of first point in decimal degrees
      @param lat2 Latitude of second point in decimal degrees
      @param long2 Longitude of second point in decimal degrees
      @return Bearing in degrees (0-359)
    */
    int   getBearing(float lat1, float long1, float lat2, float long2);
    
    /**
      Convert bearing to cardinal direction abbreviation
      
      @param course Bearing in degrees (0-359)
      @return Pointer to cardinal direction string (N, NNE, NE, etc.)
    */
    const char* getCardinal(int course);
    
    /**
      Convert latitude/longitude to Maidenhead grid locator
      
      @param lat Latitude in decimal degrees
      @param lng Longitude in decimal degrees
    */
    void  getLocator(float lat, float lng);
    
    geo_t current;    ///< Current location data
    geo_t previous;   ///< Previous location data
    char  locator[7]; ///< Maidenhead locator (6 chars + null terminator)
    int   distance;   ///< Distance traveled since last location (meters)
    float speed;      ///< Speed in meters/second
    int   knots;      ///< Speed in knots (rounded)
    int   bearing;    ///< Bearing in degrees from previous location
    int   netCount;   ///< Number of WiFi networks found in last scan

  private:
    /**
      Internal structure for storing WiFi network data
    */
    struct BSSID_RSSI {
      uint8_t bssid[WL_MAC_ADDR_LENGTH];  ///< MAC address of access point
      int8_t  rssi;                       ///< Signal strength in dBm
    } nets[MAXNETS];  ///< Array of network data
};

#endif /* WIGGLE_H */
