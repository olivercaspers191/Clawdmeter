#pragma once
#include <stdint.h>

// Battery-drain recorder. Samples battery % into a ring buffer in RTC slow
// memory, which survives deep sleep, so an overnight run can be read back the
// next morning over serial (`powerlog`) as CSV.
//
// The point is to attribute drain to a phase. Each sample carries the phase the
// device was in, so the dump shows %/hour separately for ACTIVE (screen on,
// WiFi, 240 MHz), DOZE (screen dark, WiFi off, 80 MHz, CPU still polling the
// IMU for shake) and DEEP (SoC in deep sleep — where only the panel driver,
// touch controller, IMU and PMU still draw).
//
// DEEP samples come from a timer wake that runs a minimal path: read the PMU,
// append, sleep again — no display, no WiFi, no LVGL. That's cheap enough not
// to distort what it measures.
//
// RTC memory survives deep sleep but NOT a power cycle or a flash, so the log
// starts fresh on any cold boot. It is a diagnostic, not a feature.

enum plog_phase_t : uint8_t {
    PLOG_ACTIVE = 0,   // screen on, WiFi up, 240 MHz
    PLOG_DOZE   = 1,   // screen dark, WiFi off, 80 MHz, CPU awake for shake
    PLOG_DEEP   = 2,   // sampled by the deep-sleep timer wake
    PLOG_BOOT   = 3,   // first sample after a cold boot
    PLOG_WAKE   = 4,   // woke from deep sleep (button/tap)
};

// Sample cadence while the CPU is awake, and the deep-sleep timer-wake period.
// 5 min over ~8 h is ~96 samples — well inside the ring, and a fine enough grid
// to see a phase change.
#define PLOG_INTERVAL_S      300u
#define PLOG_SLEEP_WAKE_US   (300ull * 1000000ull)

void power_log_boot(void);                  // call once, early in setup()
void power_log_sample(plog_phase_t phase);  // append one sample now
void power_log_tick(plog_phase_t phase);    // append if PLOG_INTERVAL_S has passed
void power_log_dump(void);                  // CSV to Serial (the `powerlog` command)
void power_log_clear(void);

// The daemon payload carries local wall-clock epoch. Recording the offset from
// our monotonic timebase lets the dump print real clock times without ever
// jumping the clock the log is measured against.
void power_log_set_wall_clock(long epoch);
