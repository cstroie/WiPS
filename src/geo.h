/**
  geo.h - WiFi Geolocation
         
  This library provides WiFi-based geolocation functionality using
  various geolocation services (WiGLE.net, Mozilla Location Services, Google Geolocation API).
  
  The geolocation process involves:
  1. Scanning nearby WiFi access points and collecting their BSSID and RSSI
  2. Sending this data to a geolocation service
  3. Receiving latitude, longitude, and accuracy information
  4. Calculating movement, distance, speed, and bearing between locations
  5. Converting coordinates to Maidenhead locator and cardinal directions

  Features:
  - WiFi AP scanning with BSSID and RSSI collection
  - HTTPS geolocation using various services
  - Distance and bearing calculations using spherical geometry
  - Maidenhead grid locator conversion
  - Cardinal direction mapping
  - Movement detection and speed calculation

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

#ifndef GEO_H
#define GEO_H

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
  Structure for storing WiFi network data
*/
struct nets_t {
  uint8_t bssid[WL_MAC_ADDR_LENGTH];  ///< MAC address of access point
  int8_t  rssi;                       ///< Signal strength in dBm
};


/**
  Geolocation class
  
  Provides methods for WiFi scanning, geolocation, movement tracking,
  and coordinate conversions.
*/
class GEO {
  public:
    /**
      Constructor - Initialize GEO object with default values
    */
    GEO();
    
    /**
      Initialize GEO - currently empty but reserved for future use
    */
    void  init();
    
    /**
      Scan for WiFi networks and collect BSSID/RSSI data
      
      @param sort Whether to sort networks by RSSI strength
      @return Number of networks found and stored
    */
    int   wifiScan(bool sort);
    
    /**
      Perform geolocation using collected WiFi data
      
      Sends a randomly selected BSSID to geolocation service and parses response
      to extract latitude, longitude, and accuracy.
      
      @return Accuracy in meters, or -1 on error
    */
    int   geoLocation();
    
    /**
      Calculate movement between current and previous locations
      
      Computes distance, speed, and bearing between locations.
      Updates distance, speed, knots, and bearing member variables.
      
      @return Distance in meters, or -1 if locations invalid
    */
    long  getMovement();
    
    /**
      Calculate great-circle distance between two coordinates
      
      Uses haversine formula for spherical distance calculation.
      
      @param lat1 Latitude of first point in decimal degrees
      @param long1 Longitude of first point in decimal degrees
      @param lat2 Latitude of second point in decimal degrees
      @param long2 Longitude of second point in decimal degrees
      @return Distance in meters
    */
    float getDistance(float lat1, float long1, float lat2, float long2);
    
    /**
      Calculate initial bearing between two coordinates
      
      Computes forward azimuth using spherical trigonometry.
      
      @param lat1 Latitude of first point in decimal degrees
      @param long1 Longitude of first point in decimal degrees
      @param lat2 Latitude of second point in decimal degrees
      @param long2 Longitude of second point in decimal degrees
      @return Bearing in degrees (0-359)
    */
    int   getBearing(float lat1, float long1, float lat2, float long2);
    
    /**
      Convert bearing to cardinal direction abbreviation
      
      Maps bearing degrees to 16-point compass direction.
      
      @param course Bearing in degrees (0-359)
      @return Pointer to cardinal direction string (N, NNE, NE, etc.)
    */
    const char* getCardinal(int course);
    
    /**
      Convert latitude/longitude to Maidenhead grid locator
      
      Calculates 6-character Maidenhead locator (e.g., "KN97bd").
      
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
    nets_t nets[MAXNETS];  ///< Array of network data
    
    /**
      Compare current networks with previous networks to detect significant changes
      
      @param newNets Array of newly scanned networks
      @param newCount Number of newly scanned networks
      @return true if significant changes detected, false otherwise
    */
    bool networksChanged(nets_t* newNets, int newCount);
    
    nets_t prevNets[MAXNETS];  ///< Previous network data for comparison
    int    prevNetCount;       ///< Previous number of networks
};

#endif /* GEO_H */
