#pragma once
#include <stdint.h>

// Display abstraction. The board provides the QSPI bus, panel driver, and any
// CPU-side rotation. Shared code (main.cpp, LVGL glue) never sees the GFX
// driver type. Dimensions are not declared here — query board_caps().

// Construct bus + driver objects. Safe to call before display_hal_begin().
// On boards with an IO expander gating the LCD reset, the board's
// implementation is responsible for ensuring the expander has released the
// reset before talking to the panel.
void display_hal_init(void);

// Bring the panel out of reset, clear it, and apply default brightness.
void display_hal_begin(void);

void display_hal_set_brightness(uint8_t level);  // 0..255 (driver-defined scale)
void display_hal_fill_screen(uint16_t color565);

// Write a w×h RGB565 bitmap at (x, y). Boards with software rotation
// (e.g. CO5300) transform (x, y, w, h) and the pixel buffer here before
// pushing to the panel. Shared LVGL flush_cb just calls this — no #ifdef.
void display_hal_draw_bitmap(int32_t x, int32_t y, int32_t w, int32_t h,
                             const uint16_t* pixels);

// Per-loop housekeeping for rotation-aware boards: detects orientation
// changes from the IMU, blanks the panel, invalidates LVGL, and ramps
// brightness back up. No-op on boards without rotation.
void display_hal_tick(void);

// LVGL flush regions must be even-aligned on the CO5300; harmless on others.
void display_hal_round_area(int32_t* x1, int32_t* y1, int32_t* x2, int32_t* y2);

// Put the panel driver IC into its lowest-power state (sends SLPIN) before deep
// sleep. Setting brightness to 0 only blanks the AMOLED pixels — the driver's
// oscillator and boost stay running, and on the 2.16 the panel sits on an
// always-on 3V3 rail deep sleep can't cut, so this is what actually stops it
// drawing overnight. No pairing "wake" call is needed: deep sleep resets the
// chip, so display_hal_init()/begin() bring the panel back on the next boot.
// No-op on boards that never enter deep sleep.
void display_hal_sleep(void);
