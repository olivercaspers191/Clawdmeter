#pragma once
#include <stdint.h>

// Optional accelerometer-driven orientation tracker. Returns 0..3 (quarter
// turns CW from default mounting). Boards without an IMU — or boards with
// rotation intentionally disabled, like AMOLED-1.8 fixed at 0° — return 0
// from imu_hal_rotation_quadrant() and no-op on init/tick.

void    imu_hal_init(void);
void    imu_hal_tick(void);
uint8_t imu_hal_rotation_quadrant(void);

// Auto-rotation lock. When disabled, imu_hal_rotation_quadrant() freezes at its
// current value so the screen holds orientation. Boards without rotation no-op.
void    imu_hal_set_rotation_enabled(bool en);
bool    imu_hal_rotation_enabled(void);

// Fast-motion (shake) detection, sampled inside imu_hal_tick(). Returns true
// once if a shake happened since the last call, then clears. Used to wake the
// dozing display. Boards without an IMU return false.
bool    imu_hal_consume_shake(void);
