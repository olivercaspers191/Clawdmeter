#pragma once
// Transport abstraction: the firmware talks to a "connection" that delivers the
// usage JSON. Two implementations, picked at build time:
//   - default            → BLE (ble.{h,cpp}, NimBLE peripheral + HID keyboard)
//   - -DUSE_WIFI_TRANSPORT → WiFi (net.{h,cpp}, HTTP poll of the homeserver)
//
// Shared code (main.cpp, ui.*) uses only conn_state_t / CONN_* and the
// transport_* wrappers below, so neither the UI nor the main loop hard-depends
// on which radio is compiled in.

#ifdef USE_WIFI_TRANSPORT
  #include "net.h"
  typedef net_state_t conn_state_t;
  #define CONN_STATE_CONNECTED NET_STATE_CONNECTED
  #define CONN_STATE_INIT      NET_STATE_INIT
  #define TRANSPORT_HAS_KEYBOARD 0

  inline void        transport_init(void)        { net_init(); }
  inline void        transport_tick(void)        { net_tick(); }
  inline conn_state_t transport_get_state(void)  { return net_get_state(); }
  inline const char* transport_get_name(void)    { return net_get_ssid(); }
  inline const char* transport_get_info(void)    { return net_get_ip(); }
  inline bool        transport_has_data(void)    { return net_has_data(); }
  inline const char* transport_get_data(void)    { return net_get_data(); }
  inline void        transport_send_ack(void)    {}
  inline void        transport_send_nack(void)   {}
  inline void        transport_request_refresh(void) { net_request_refresh(); }
  inline void        transport_sleep(void)       { net_sleep(); }  // radio off while dozing
  inline void        transport_wake(void)        { net_wake(); }   // radio on + poll on wake
#else
  #include "ble.h"
  typedef ble_state_t conn_state_t;
  #define CONN_STATE_CONNECTED BLE_STATE_CONNECTED
  #define CONN_STATE_INIT      BLE_STATE_INIT
  #define TRANSPORT_HAS_KEYBOARD 1

  inline void        transport_init(void)        { ble_init(); }
  inline void        transport_tick(void)        { ble_tick(); }
  inline conn_state_t transport_get_state(void)  { return ble_get_state(); }
  inline const char* transport_get_name(void)    { return ble_get_device_name(); }
  inline const char* transport_get_info(void)    { return ble_get_mac_address(); }
  inline bool        transport_has_data(void)    { return ble_has_data(); }
  inline const char* transport_get_data(void)    { return ble_get_data(); }
  inline void        transport_send_ack(void)    { ble_send_ack(); }
  inline void        transport_send_nack(void)   { ble_send_nack(); }
  inline void        transport_request_refresh(void) { ble_request_refresh(); }
  inline void        transport_sleep(void)       {}  // BLE stays connected; daemon owns cadence
  inline void        transport_wake(void)        {}
#endif
