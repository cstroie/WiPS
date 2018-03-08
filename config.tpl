/**
  config.h - User settings

  Copyright (c) 2017-2018 Costin STROIE <costinstroie@eridu.eu.org>

  This file is part of WiPS.
*/

#ifndef CONFIG_H
#define CONFIG_H

// Static auth
#define WIFI_RS       ";"
#define WIFI_FS       ","
#define WIFI_SSIDPASS "ssid1,psk1;ssid2,psk2"

// Geolocation
#define GEO_APIKEY    "USE_YOUR_KEY"
#define GEO_MAXACC    250
#define GEO_MINACC    50

// APRS settings
#define APRS_SERVER   "cbaprs.de"
#define APRS_PORT     27235

// NTP
#define NTP_SERVER    "europe.pool.ntp.org"

// OTA
#define OTA_PASS      "OTA_PASS"

#endif /* CONFIG_H */
