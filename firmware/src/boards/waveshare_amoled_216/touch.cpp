#include "../../hal/touch_hal.h"
#include "board.h"
#include <Arduino.h>
#include <Wire.h>
#include <TouchDrvCSTXXX.hpp>

static TouchDrvCST92xx touch;
static bool              touch_ok = false;

static volatile bool     touch_data_ready = false;
static volatile bool     touch_pressed = false;
static volatile uint16_t touch_x = 0;
static volatile uint16_t touch_y = 0;

static void IRAM_ATTR touch_isr(void) {
    touch_data_ready = true;
}

void touch_hal_init(void) {
    touch.setPins(TP_RST, TP_INT);
    if (!touch.begin(Wire, CST9220_ADDR, IIC_SDA, IIC_SCL)) {
        Serial.println("Touch init failed");
        return;
    }
    touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
    touch.setSwapXY(true);
    touch.setMirrorXY(true, false);
    pinMode(TP_INT, INPUT_PULLUP);
    attachInterrupt(TP_INT, touch_isr, FALLING);
    touch_ok = true;
    Serial.println("Touch init OK");
}

// Put the CST9220 to sleep: sleep() issues the sleep command and parks INT/RST
// open-drain; our RTC pullup on the INT pin then holds it high, so it neither
// scans nor spuriously wakes us.
//
// WARNING — currently UNUSED on purpose. power_sleep.cpp no longer calls this.
// Sleeping the controller before deep sleep left it in a state the next boot's
// touch.begin() didn't cleanly recover from (touch came back dead after a
// deep-sleep→USB wake), and it also killed TP_INT tap-to-wake. Kept only as a
// HAL capability; don't re-wire it into the sleep path without fixing recovery.
void touch_hal_sleep(void) {
    if (touch_ok) touch.sleep();
}

void touch_hal_read(uint16_t* x, uint16_t* y, bool* pressed) {
    if (touch_data_ready) {
        touch_data_ready = false;
        int16_t tx[5], ty[5];
        uint8_t n = touch.getPoint(tx, ty, touch.getSupportTouchPoint());
        if (n > 0) {
            touch_pressed = true;
            touch_x = (uint16_t)tx[0];
            touch_y = (uint16_t)ty[0];
        } else {
            touch_pressed = false;
        }
    }
    *x = touch_x;
    *y = touch_y;
    *pressed = touch_pressed;
}
