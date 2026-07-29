#pragma once

// Auto-sleep / idle screen-off configuration.
// All tunables live here so nothing is hard-coded in main.cpp / idle.cpp.

// Three-gear idle timeline (all measured from the last interaction):
//   0..IDLE_DIM_TIMEOUT_MS          active   — screen on, WiFi polling, 240 MHz.
//   DIM..IDLE_DEEPSLEEP_TIMEOUT_MS  light    — screen dark, WiFi off, and the
//                                              CPU HALTED in light sleep between
//                                              brief housekeeping ticks. A tap or
//                                              button wakes it instantly (state is
//                                              retained — no reboot). No shake
//                                              (the CPU is off), so the IMU can't
//                                              wake this gear.
//   IDLE_DEEPSLEEP_TIMEOUT_MS..     deep     — true ESP32 deep sleep (~sub-mA);
//                                              a button wakes it (touch is slept),
//                                              via a full reboot.
// All gated by IDLE_SLEEP_WHEN_CHARGING below, so on USB power none fire and the
// display stays on continuously. The light-sleep gear only exists on boards with
// a wake source (the 2.16); elsewhere the middle gear stays a plain screen-off.
#define IDLE_DIM_TIMEOUT_MS        (20UL * 60UL * 1000UL)  // 20 min → dark + light sleep
#define IDLE_DEEPSLEEP_TIMEOUT_MS  (60UL * 60UL * 1000UL)  // 60 min → deep sleep

// How long light sleep may nap before it wakes to run one housekeeping loop
// (re-check the deep-sleep threshold, tick the power log) and nap again. A tap
// or button interrupts the nap immediately regardless of this. Kept short enough
// that deep sleep starts within one tick of the 60-min mark; the wake itself is
// only a few ms, so more ticks cost almost nothing.
#define LIGHT_SLEEP_TICK_MS        (60UL * 1000UL)         // 60 s housekeeping nap

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
