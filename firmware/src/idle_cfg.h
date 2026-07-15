#pragma once

// Auto-sleep / idle screen-off configuration.
// All tunables live here so nothing is hard-coded in main.cpp / idle.cpp.

// Two-stage idle timeline (both measured from the last interaction):
//   IDLE_DIM_TIMEOUT_MS       — fade the screen to black; the SoC stays awake
//                               and WiFi keeps polling, so a tap/button/shake
//                               brings it right back.
//   IDLE_DEEPSLEEP_TIMEOUT_MS — enter true ESP32 deep sleep (~sub-mA); only
//                               a tap or button press wakes it (the IMU can't).
// Both are gated by IDLE_SLEEP_WHEN_CHARGING below, so on USB power neither
// fires and the display stays on continuously.
#define IDLE_DIM_TIMEOUT_MS        (10UL * 60UL * 1000UL)  // 10 min → screen off
#define IDLE_DEEPSLEEP_TIMEOUT_MS  (30UL * 60UL * 1000UL)  // 30 min → deep sleep
#define IDLE_FADE_OUT_MS            400      // fade-to-black duration
#define IDLE_FADE_IN_MS             180      // wake fade-in (snappier)
#define IDLE_FADE_STEP_MS           20       // tick interval per fade step

#define DISPLAY_DEFAULT_BRIGHTNESS  200      // active-screen brightness

// When false, the device never enters sleep while USB power is present (also
// wakes from sleep when USB is plugged back in). Useful when sitting on a
// desk plugged in — also covers battery-less hardware that's always on USB.
// Set true to sleep regardless of power source.
#define IDLE_SLEEP_WHEN_CHARGING    false

// When true, a touch on the dark panel wakes the device (first touch is
// consumed for wake only, second touch acts normally). When false, touch is
// fully ignored during sleep — useful if cats/sleeves brushing the panel
// overnight would be a problem.
#define IDLE_WAKE_ON_TOUCH          true
