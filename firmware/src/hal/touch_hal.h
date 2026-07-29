#pragma once
#include <stdint.h>

// Touch abstraction. The board owns the touch controller driver and the
// TP_INT pin wiring. The HAL implementation is responsible for keeping its
// own internal "latest sample" state — shared code calls touch_hal_read()
// once per loop and feeds it into LVGL.
//
// Implementations should complete touch_hal_read() in well under 5 ms (a
// single I2C burst). LVGL polls this at the screen refresh rate.

void touch_hal_init(void);

// Pump the controller and return the latest sample. *pressed reflects
// whether any finger is currently down; coordinates are valid only when
// pressed is true and are in display (post-orientation) coordinates.
void touch_hal_read(uint16_t* x, uint16_t* y, bool* pressed);

// Put the touch controller to sleep before deep sleep. The CST9220 keeps
// scanning for fingers otherwise, on the panel's always-on rail, so it draws
// all night. COSTS tap-to-wake: a sleeping controller won't assert its INT on
// touch, so deep sleep must then be woken by a physical button (GPIO0/18), not
// the screen. Re-inited by touch_hal_init() on the next boot (deep sleep resets
// the chip). No-op on boards that never enter deep sleep.
void touch_hal_sleep(void);
