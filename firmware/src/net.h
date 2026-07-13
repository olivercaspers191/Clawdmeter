#pragma once
#include <stdint.h>

// WiFi transport — drop-in replacement for the BLE data path. The firmware
// polls the homeserver usage service over HTTP and fills the same rx buffer the
// BLE path used, so main.cpp's parse_json()/ui_update() flow is unchanged.
// Selected at build time via -DUSE_WIFI_TRANSPORT (see platformio.ini).

enum net_state_t {
    NET_STATE_INIT,
    NET_STATE_CONNECTING,
    NET_STATE_CONNECTED,     // WiFi up AND at least one usage poll succeeded
    NET_STATE_DISCONNECTED,
};

void net_init(void);
void net_tick(void);
net_state_t net_get_state(void);

const char* net_get_ssid(void);   // configured SSID
const char* net_get_ip(void);     // "192.168.x.x" once associated, else ""

bool net_has_data(void);          // true when a fresh payload is waiting
const char* net_get_data(void);   // returns latest JSON; clears the has_data flag
void net_request_refresh(void);   // force a poll on the next tick (e.g. first boot)
