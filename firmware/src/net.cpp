#ifdef USE_WIFI_TRANSPORT
// WiFi transport — polls the homeserver usage service over HTTP and fills the
// same rx buffer the BLE path used, so main.cpp's parse_json()/ui_update() flow
// is unchanged. See net.h and transport.h.

#include "net.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "wifi_config.h"

#ifndef WIFI_SSID
#error "Create firmware/src/wifi_config.h from wifi_config.example.h (WIFI_SSID/WIFI_PASS/USAGE_URL)."
#endif

static const uint32_t POLL_INTERVAL_ACTIVE_MS = 60000;   // 1 min while the screen is on
static const uint32_t POLL_INTERVAL_IDLE_MS   = 300000;  // 5 min while dozing (screen off)
static const uint32_t WIFI_RETRY_MS    = 5000;
static const uint32_t HTTP_TIMEOUT_MS  = 5000;

static uint32_t poll_interval_ms = POLL_INTERVAL_ACTIVE_MS;

static net_state_t state = NET_STATE_INIT;
static char        rx_buf[768];
static bool        data_ready = false;
static bool        force_poll = true;   // poll as soon as WiFi is up
static uint32_t    last_poll_ms = 0;
static uint32_t    last_wifi_attempt_ms = 0;
static char        ip_buf[20] = {0};

void net_init(void) {
    state = NET_STATE_CONNECTING;
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(true);                 // WIFI_PS_MIN_MODEM: 60s poll cadence doesn't need a hot radio; big battery win
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    last_wifi_attempt_ms = millis();
    Serial.printf("net: connecting to SSID '%s'\n", WIFI_SSID);
}

static void do_poll(void) {
    HTTPClient http;
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);
    if (!http.begin(USAGE_URL)) {
        Serial.println("net: http.begin() failed");
        return;
    }
    int code = http.GET();
    if (code == 200) {
        String body = http.getString();
        size_t n = body.length();
        if (n > 0 && n < sizeof(rx_buf)) {
            memcpy(rx_buf, body.c_str(), n + 1);   // include NUL
            data_ready = true;
            state = NET_STATE_CONNECTED;           // WiFi up AND a payload in hand
            Serial.printf("net: usage %s\n", rx_buf);
        } else {
            Serial.printf("net: payload size %u out of range\n", (unsigned)n);
        }
    } else {
        Serial.printf("net: GET %s -> %d\n", USAGE_URL, code);
    }
    http.end();
}

void net_tick(void) {
    if (WiFi.status() != WL_CONNECTED) {
        if (state == NET_STATE_CONNECTED) state = NET_STATE_DISCONNECTED;
        uint32_t now = millis();
        if (now - last_wifi_attempt_ms > WIFI_RETRY_MS) {
            last_wifi_attempt_ms = now;
            state = NET_STATE_CONNECTING;
            Serial.println("net: WiFi down, reconnecting");
            WiFi.reconnect();
        }
        return;
    }

    // Associated. Stay CONNECTING (UI shows the connect hint) until the first
    // successful poll flips us to CONNECTED inside do_poll().
    uint32_t now = millis();
    if (force_poll || last_poll_ms == 0 || (now - last_poll_ms) >= poll_interval_ms) {
        force_poll = false;
        last_poll_ms = now;
        do_poll();
    }
}

net_state_t net_get_state(void) { return state; }

const char* net_get_ssid(void) { return WIFI_SSID; }

const char* net_get_ip(void) {
    if (WiFi.status() != WL_CONNECTED) { ip_buf[0] = 0; return ip_buf; }
    strlcpy(ip_buf, WiFi.localIP().toString().c_str(), sizeof(ip_buf));
    return ip_buf;
}

bool net_has_data(void) { return data_ready; }

const char* net_get_data(void) {
    data_ready = false;
    return rx_buf;
}

void net_request_refresh(void) { force_poll = true; }

void net_set_low_power(bool low) {
    // Slow the poll cadence while the screen is dozing (data isn't visible then;
    // a wake force-polls immediately). Keeps the radio idle more of the time.
    poll_interval_ms = low ? POLL_INTERVAL_IDLE_MS : POLL_INTERVAL_ACTIVE_MS;
}

#endif // USE_WIFI_TRANSPORT
