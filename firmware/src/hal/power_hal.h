#pragma once

// Power / battery / power-button abstraction. Replaces the legacy power.h
// API but keeps the same shape so existing call sites stay clean.
//
// Some boards (AMOLED-2.16) wire PWR through the PMU's PKEY IRQ; others
// (AMOLED-1.8) route it through an IO expander. The HAL hides which
// source produced the press — shared code just polls
// power_hal_pwr_pressed() once per loop.

void power_hal_init(void);
void power_hal_tick(void);

int  power_hal_battery_pct(void);  // 0..100, or -1 if no battery (see BoardCaps.has_battery)
bool power_hal_is_charging(void);
bool power_hal_is_vbus_in(void);   // USB cable present (true even without a battery)

// Edge-triggered: returns true once per PWR short-press, then clears.
bool power_hal_pwr_pressed(void);

// Edge-triggered: true once when a PWR hold crosses the long-press threshold
// (~1.5s), then clears. Starts the hold-to-pair gesture.
bool power_hal_pwr_long_pressed(void);

// Edge-triggered: true once on the PWR release edge, then clears. Completes
// or cancels the hold-to-pair gesture.
bool power_hal_pwr_released(void);

// Bitmask of RTC-capable GPIOs that should wake the board from deep sleep via
// ext1 ANY_LOW (buttons + touch INT — all active-low, idle-high). Return 0 if
// the board has no deep-sleep wake source; deep sleep is then skipped for it.
#include <stdint.h>
uint64_t power_hal_deep_sleep_wake_mask(void);

// Halt the CPU in light sleep until a wake GPIO (tap/button, active-low) fires
// or max_ms elapses. RAM is retained, so execution resumes right here on wake —
// no reboot. Returns true when a GPIO (user interaction) caused the wake, false
// on the timer bound (a housekeeping tick) or when unsupported. Boards with no
// wake source return false immediately without sleeping, so the caller falls
// back to its ordinary screen-off idle. Only meaningful on battery — never call
// it while USB is present (light sleep + USB-CDC don't mix cleanly).
bool power_hal_light_sleep(uint32_t max_ms);
