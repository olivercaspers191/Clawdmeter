#pragma once
// -----------------------------------------------------------------------------
// COPY THIS FILE to  wifi_config.h  (same folder) and fill in your own values.
// wifi_config.h is git-ignored — your SSID/password never get committed.
// -----------------------------------------------------------------------------

// Your 2.4 GHz WiFi network (ESP32-S3 does not do 5 GHz).
#define WIFI_SSID  "your-ssid"
#define WIFI_PASS  "your-password"

// The homeserver usage service (see homeserver/usage-service.mjs).
// Use the server's LAN IP or hostname and the port it listens on (default 8090).
#define USAGE_URL  "http://192.168.1.100:8090/usage"
