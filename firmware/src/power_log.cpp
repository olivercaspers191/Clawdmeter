#include "power_log.h"
#include "hal/power_hal.h"
#include <Arduino.h>
#include <esp_sleep.h>
#include <sys/time.h>

// 288 * 8 B = 2.3 KB of the S3's 8 KB RTC slow memory — 24 h at 5 min. Shares
// that memory with power_sleep.cpp's UsageData stash, which is far smaller.
#define PLOG_MAX   288u
#define PLOG_MAGIC 0x50C0DE10u

typedef struct {
    uint32_t t_s;    // seconds on the monotonic timebase (see now_s)
    uint8_t  pct;    // battery %, 0xFF = PMU had no reading
    uint8_t  phase;  // plog_phase_t
    uint8_t  flags;  // bit0 charging, bit1 vbus present
    uint8_t  wake;   // esp_sleep_wakeup_cause_t at the boot that wrote this
} plog_entry_t;

RTC_DATA_ATTR static uint32_t     plog_magic;
RTC_DATA_ATTR static plog_entry_t plog[PLOG_MAX];
RTC_DATA_ATTR static uint16_t     plog_head;      // next slot to write
RTC_DATA_ATTR static uint16_t     plog_count;     // entries held (saturates at PLOG_MAX)
RTC_DATA_ATTR static uint32_t     plog_dropped;   // samples lost to ring wrap
RTC_DATA_ATTR static uint32_t     plog_last_s;    // timebase of the last sample
RTC_DATA_ATTR static int64_t      plog_wall_ofs;  // wall epoch - monotonic, 0 = unknown
RTC_DATA_ATTR static uint8_t      plog_wall_set;

// Monotonic seconds that survive deep sleep. ESP-IDF keeps system time running
// across deep sleep off the RTC timer, and we never settimeofday(), so this only
// ever moves forward — which the sample deltas depend on. Wall-clock times in
// the dump come from plog_wall_ofs instead.
static uint32_t now_s(void) {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (uint32_t)tv.tv_sec;
}

void power_log_boot(void) {
    if (plog_magic == PLOG_MAGIC) return;   // survived deep sleep — keep the log
    plog_magic    = PLOG_MAGIC;             // cold boot — start fresh
    plog_head     = 0;
    plog_count    = 0;
    plog_dropped  = 0;
    plog_last_s   = 0;
    plog_wall_ofs = 0;
    plog_wall_set = 0;
    Serial.println("plog: started (cold boot)");
}

void power_log_sample(plog_phase_t phase) {
    int  pct      = power_hal_battery_pct();
    bool charging = power_hal_is_charging();
    bool vbus     = power_hal_is_vbus_in();

    plog_entry_t e;
    e.t_s   = now_s();
    e.pct   = (pct < 0 || pct > 100) ? 0xFF : (uint8_t)pct;
    e.phase = (uint8_t)phase;
    e.flags = (uint8_t)((charging ? 1 : 0) | (vbus ? 2 : 0));
    e.wake  = (uint8_t)esp_sleep_get_wakeup_cause();

    plog[plog_head] = e;
    plog_head = (uint16_t)((plog_head + 1) % PLOG_MAX);
    if (plog_count < PLOG_MAX) plog_count++;
    else                       plog_dropped++;   // oldest sample just fell off
    plog_last_s = e.t_s;
}

void power_log_tick(plog_phase_t phase) {
    uint32_t now = now_s();
    if (plog_count && (now - plog_last_s) < PLOG_INTERVAL_S) return;
    power_log_sample(phase);
}

void power_log_set_wall_clock(long epoch) {
    if (epoch <= 0) return;
    plog_wall_ofs = (int64_t)epoch - (int64_t)now_s();
    plog_wall_set = 1;
}

static const char* phase_name(uint8_t p) {
    switch (p) {
    case PLOG_ACTIVE: return "active";
    case PLOG_DOZE:   return "doze";
    case PLOG_DEEP:   return "deep";
    case PLOG_BOOT:   return "boot";
    case PLOG_WAKE:   return "wake";
    default:          return "?";
    }
}

void power_log_dump(void) {
    Serial.println("PLOG_START");
    Serial.printf("# samples=%u dropped=%u interval=%us wall_clock=%s\n",
                  plog_count, plog_dropped, PLOG_INTERVAL_S,
                  plog_wall_set ? "yes" : "no (times are since boot)");
    Serial.println("i,t_s,clock,pct,phase,charging,vbus,wake");

    // Oldest first. Once the ring has wrapped, head is the oldest slot.
    uint16_t start = (plog_count < PLOG_MAX) ? 0 : plog_head;
    for (uint16_t i = 0; i < plog_count; i++) {
        const plog_entry_t& e = plog[(start + i) % PLOG_MAX];

        char clock[16];
        if (plog_wall_set) {
            // The daemon's epoch is already local wall-clock, so no timezone
            // maths — just split it.
            uint32_t secs = (uint32_t)(((int64_t)e.t_s + plog_wall_ofs) % 86400);
            snprintf(clock, sizeof(clock), "%02u:%02u:%02u",
                     secs / 3600, (secs / 60) % 60, secs % 60);
        } else {
            snprintf(clock, sizeof(clock), "-");
        }

        char pct[8];
        if (e.pct == 0xFF) snprintf(pct, sizeof(pct), "-");
        else               snprintf(pct, sizeof(pct), "%u", e.pct);

        Serial.printf("%u,%lu,%s,%s,%s,%u,%u,%u\n",
                      i, (unsigned long)e.t_s, clock, pct, phase_name(e.phase),
                      (e.flags & 1) ? 1 : 0, (e.flags & 2) ? 1 : 0, e.wake);
        if ((i & 0x0F) == 0x0F) Serial.flush();   // don't overrun the CDC buffer
    }
    Serial.println("PLOG_END");
    Serial.flush();
}

void power_log_clear(void) {
    plog_magic = 0;
    power_log_boot();
    Serial.println("plog: cleared");
}
