#pragma once
#include <stdbool.h>

// Auto-rotation on/off, persisted to NVS. On the AMOLED-2.16 the right button
// toggles it; boards without IMU rotation are unaffected (imu_hal stubs no-op).
void autorotate_init(void);    // load saved state from NVS and apply to the IMU
void autorotate_toggle(void);  // flip, save, apply
bool autorotate_enabled(void);
