#pragma once
#include "data.h"

// Deep-sleep power management + RTC-retained usage stash.
//
// When the panel has been idle long enough (see idle_cfg.h) and we're on
// battery, the device enters ESP32 deep sleep (~sub-mA) instead of just
// dimming the screen. The last usage snapshot is kept in RTC memory so it can
// be shown instantly on wake while WiFi reconnects in the background.

// Call once early in setup() (after board_init). Latches whether this boot is
// a wake from deep sleep vs a cold power-on / normal reset.
void power_sleep_boot_check(void);

// True if this boot was triggered by a deep-sleep wake source (button/touch).
bool power_sleep_woke_from_deep(void);
bool power_sleep_woke_from_timer(void);   // woke only to sample the power log

// If this is a deep-sleep wake and the RTC stash is valid, copies it into *d
// and returns true. Returns false on cold boot or empty/garbage RTC memory.
bool power_sleep_restore(UsageData* d);

// Save the current usage snapshot to RTC memory (no-op if data isn't valid).
void power_sleep_stash(const UsageData* d);

// Turn off the radio, arm the board's wake GPIOs (ext1 ANY_LOW) and enter
// deep sleep. Does not return — the chip resets and re-runs setup() on wake.
// No-op (returns) if the board reports no deep-sleep wake source.
void power_sleep_enter_deep(void);
