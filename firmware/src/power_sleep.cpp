#include "power_sleep.h"
#include "power_log.h"
#include "hal/power_hal.h"
#include "hal/display_hal.h"
#include "hal/imu_hal.h"
#include "hal/touch_hal.h"
#include <Arduino.h>
#include <esp_sleep.h>
#include "driver/rtc_io.h"
#ifdef USE_WIFI_TRANSPORT
#include <WiFi.h>
#include <esp_wifi.h>
#endif

// Marker proving the RTC struct below holds a snapshot we wrote (not power-on
// garbage). RTC slow memory survives deep sleep but not a power cycle.
#define STASH_MAGIC 0xC1A0DEEDu

RTC_DATA_ATTR static uint32_t  rtc_magic = 0;
RTC_DATA_ATTR static UsageData rtc_usage;

static bool woke_from_deep  = false;
static bool woke_from_timer = false;

void power_sleep_boot_check(void) {
    esp_sleep_wakeup_cause_t c = esp_sleep_get_wakeup_cause();
    woke_from_deep = (c == ESP_SLEEP_WAKEUP_EXT1 ||
                      c == ESP_SLEEP_WAKEUP_EXT0 ||
                      c == ESP_SLEEP_WAKEUP_GPIO ||
                      c == ESP_SLEEP_WAKEUP_TIMER);
    woke_from_timer = (c == ESP_SLEEP_WAKEUP_TIMER);
    Serial.printf("power: boot wake cause=%d (deep=%d timer=%d)\n",
                  (int)c, (int)woke_from_deep, (int)woke_from_timer);
}

bool power_sleep_woke_from_deep(void)  { return woke_from_deep; }
bool power_sleep_woke_from_timer(void) { return woke_from_timer; }

bool power_sleep_restore(UsageData* d) {
    if (!d || !woke_from_deep || rtc_magic != STASH_MAGIC || !rtc_usage.valid) return false;
    *d = rtc_usage;
    return true;
}

void power_sleep_stash(const UsageData* d) {
    if (!d || !d->valid) return;
    rtc_usage = *d;
    rtc_magic = STASH_MAGIC;
}

void power_sleep_enter_deep(void) {
    uint64_t mask = power_hal_deep_sleep_wake_mask();
    if (mask == 0) return;   // board has no deep-sleep wake source — stay awake

    Serial.println("power: entering deep sleep (wake on button)");
    Serial.flush();

    // Power down the peripherals on the always-on 3V3 rail before sleeping. Deep
    // sleep stops the SoC but not the panel driver, IMU, or touch controller —
    // measured at ~2.3-3%/hour asleep, they were the real overnight drain. Skip
    // this on a power-log timer wake: that fast path never brought them up (see
    // setup()), so there is nothing initialised to sleep, and touching an
    // unconstructed driver would fault. All re-init on the next boot's setup()
    // because deep sleep resets the chip.
    if (!woke_from_timer) {
        display_hal_sleep();   // CO5300 SLPIN — stops oscillator/boost
        imu_hal_sleep();       // QMI8658 accelerometer off
        touch_hal_sleep();     // CST9220 sleep — costs tap-to-wake (button still wakes)
    }

#ifdef USE_WIFI_TRANSPORT
    // Only tear WiFi down if this boot ever brought it up. On a power-log timer
    // wake it never did, and Arduino's WiFi.mode() would initialise the stack —
    // powering the radio on — just to switch it off again. esp_deep_sleep_start()
    // cuts the radio regardless; this is only for a clean disassociation.
    if (!woke_from_timer) {
        WiFi.disconnect(true, false);
        WiFi.mode(WIFI_OFF);
        esp_wifi_stop();
    }
#endif

    // Hold the wake pins high through sleep (RTC pullups) so a press/tap pulling
    // one low is what wakes us — ext1 ANY_LOW. All wake pins are RTC-capable and
    // active-low (buttons to GND, touch INT falling).
    for (int p = 0; p <= 21; ++p) {
        if (mask & (1ULL << p)) {
            rtc_gpio_pullup_en((gpio_num_t)p);
            rtc_gpio_pulldown_dis((gpio_num_t)p);
        }
    }
    esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);

    // Also wake on a timer, purely to sample the battery into the power log and
    // go straight back down (see the fast path in setup()). This is how deep
    // sleep's real draw gets measured — without it the log has an 8-hour hole
    // and we'd be guessing again.
    esp_sleep_enable_timer_wakeup(PLOG_SLEEP_WAKE_US);

    esp_deep_sleep_start();
    // unreachable — chip resets on wake and re-runs setup()
}
