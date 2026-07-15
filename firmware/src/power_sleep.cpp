#include "power_sleep.h"
#include "hal/power_hal.h"
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

static bool woke_from_deep = false;

void power_sleep_boot_check(void) {
    esp_sleep_wakeup_cause_t c = esp_sleep_get_wakeup_cause();
    woke_from_deep = (c == ESP_SLEEP_WAKEUP_EXT1 ||
                      c == ESP_SLEEP_WAKEUP_EXT0 ||
                      c == ESP_SLEEP_WAKEUP_GPIO ||
                      c == ESP_SLEEP_WAKEUP_TIMER);
    Serial.printf("power: boot wake cause=%d (deep=%d)\n", (int)c, (int)woke_from_deep);
}

bool power_sleep_woke_from_deep(void) { return woke_from_deep; }

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

    Serial.println("power: entering deep sleep (wake on tap/button)");
    Serial.flush();

#ifdef USE_WIFI_TRANSPORT
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
    esp_wifi_stop();
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
    esp_deep_sleep_start();
    // unreachable — chip resets on wake and re-runs setup()
}
