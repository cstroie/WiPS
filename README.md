# WiFi APRS Tracker

An APRS (Automatic Packet Reporting System) tracker using WiFi-based geolocation with Mozilla Location Services (MLS) and Google Geolocation API.

## Overview

This project implements a wireless positioning system that determines location based on nearby WiFi access points and reports position via APRS-IS. It functions as both a positioning system and an automated position reporting system, providing GPS-compatible NMEA sentences and APRS position reports with telemetry data.

## Features

- **WiFi Geolocation**: Uses nearby WiFi access points for position determination
- **Multiple Geolocation Services**: Supports both Mozilla Location Services and Google Geolocation API
- **NMEA Sentence Generation**: Outputs standard NMEA-0183 sentences for GPS compatibility
- **APRS Position Reporting**: Sends position reports to APRS-IS with telemetry data
- **SmartBeaconing**: Adaptive reporting intervals based on movement
- **Network Services**: 
  - TCP NMEA server on port 10110
  - UDP broadcast of NMEA sentences
  - mDNS service discovery
- **Time Synchronization**: NTP client for accurate timekeeping
- **OTA Updates**: Over-The-Air firmware update capability
- **Multiple WiFi Connection Methods**: 
  - Pre-configured credentials
  - WiFi Manager captive portal
  - Known networks list
  - Open network detection
- **Multi-platform Support**: Compatible with ESP8266 and ESP32 (including ESP32-C3)

## Hardware Requirements

### ESP8266:
- ESP8266 development board (NodeMCU, Wemos D1 Mini, etc.)
- Optional: SSD1306 128x32 OLED display

### ESP32:
- ESP32 development board (ESP32 DevKit, ESP32-C3, etc.)
- Optional: SSD1306 128x32 OLED display

## Software Dependencies

- ESP8266 Arduino Core (for ESP8266) or ESP32 Arduino Core (for ESP32)
- WiFiManager library
- ArduinoOTA library

## Configuration

1. Copy `config.tpl` to `config.h`
2. Configure your:
   - WiFi credentials (optional)
   - APRS-IS server and credentials
   - Google Geolocation API key
   - NTP server

## Usage

The device will:
1. Connect to WiFi using available methods
2. Synchronize time with NTP server
3. Continuously scan for WiFi networks and determine position
4. Report position via APRS-IS when moving or on schedule
5. Output NMEA sentences via serial, TCP, and UDP

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

## Author

Costin STROIE <costinstroie@eridu.eu.org>
