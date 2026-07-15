#include "autorotate.h"
#include "hal/imu_hal.h"
#include <Preferences.h>
#include <Arduino.h>

static bool s_enabled = true;

void autorotate_init(void) {
    Preferences prefs;
    prefs.begin("clawdmeter", true);
    s_enabled = prefs.getBool("autorot", true);
    prefs.end();

    imu_hal_set_rotation_enabled(s_enabled);
    Serial.printf("Auto-rotate init: %s\n", s_enabled ? "on" : "off");
}

void autorotate_toggle(void) {
    s_enabled = !s_enabled;

    Preferences prefs;
    prefs.begin("clawdmeter", false);
    prefs.putBool("autorot", s_enabled);
    prefs.end();

    imu_hal_set_rotation_enabled(s_enabled);
    Serial.printf("Auto-rotate %s\n", s_enabled ? "on" : "off");
}

bool autorotate_enabled(void) { return s_enabled; }
