#include "../../hal/imu_hal.h"
#include "board.h"
#include <Arduino.h>
#include <Wire.h>
#include <SensorQMI8658.hpp>

// Poll and hysteresis timing. 40 Hz sampling: rotation only needs ~10 Hz (it has
// its own 300 ms hysteresis), but shake detection has to out-sample the motion
// itself — at 10 Hz a ~5 Hz shake aliases and its peaks get missed entirely.
#define IMU_POLL_MS       25     // ~40 Hz
#define STABLE_TIME_MS    300    // orientation must hold this long before rotating
#define TILT_THRESHOLD    0.5f   // ~30° from axis (sin 30° ≈ 0.5)

// Shake detection: at rest |accel| ≈ 1g; a deliberate shake swings it well past
// this. Require a few over-threshold samples in a row (~75 ms of real motion) so
// a single desk bump doesn't trip it. Only consumed while the display is dozing,
// so a false trip just lights the screen and it re-dozes.
#define SHAKE_G_THRESH    0.45f  // deviation from 1g counting as fast motion
#define SHAKE_HITS        3      // over-threshold samples needed to latch a shake

// Resting/default orientation, used when the device lies flat and the IMU can't
// tell which way is up (accel_to_rotation returns "ambiguous"). The panel is
// mounted a quarter-turn off in the enclosure, so this default is 3 (90° CCW),
// not 0, to bring the flat/boot view upright. The four auto-rotate positions are
// computed from gravity and were already calibrated to the mounting, so they are
// NOT offset — only this fallback is.
//   3 = 90° CCW  (corrects a flat view that looked rotated 90° CW)
//   1 = 90° CW   (use this if 3 turns out the wrong way)
#define DEFAULT_ROTATION_QUADRANT  3

static SensorQMI8658 imu;
static uint8_t  current_rotation   = DEFAULT_ROTATION_QUADRANT;
static uint8_t  candidate_rotation = DEFAULT_ROTATION_QUADRANT;
static uint32_t candidate_since    = 0;
static uint32_t last_poll_ms       = 0;
static bool     imu_ok             = false;
static bool     rotation_enabled   = true;
static bool     shake_latched      = false;
static uint8_t  shake_count        = 0;

static uint8_t accel_to_rotation(float ax, float ay) {
    float abs_ax = fabsf(ax);
    float abs_ay = fabsf(ay);
    if (abs_ax < TILT_THRESHOLD && abs_ay < TILT_THRESHOLD) {
        return 255;  // ambiguous (face-up/down)
    }
    if (abs_ay > abs_ax) return (ay > 0) ? 3 : 1;
    return (ax > 0) ? 0 : 2;
}

void imu_hal_init(void) {
    if (!imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
        Serial.println("QMI8658 init failed");
        return;
    }
    Serial.println("QMI8658 init OK");
    // 128 Hz low-power ODR (valid while the gyro stays disabled). The LPF cutoff
    // is a fraction of ODR — LPF_MODE_3 is the *least* filtered option at 13.37%
    // of ODR — so the old 21 Hz ODR meant a ~2.8 Hz cutoff that smoothed shakes
    // away. At 128 Hz the cutoff is ~17 Hz, which passes a 3–8 Hz shake intact
    // while still being gentle enough for stable tilt/rotation.
    imu.configAccelerometer(
        SensorQMI8658::ACC_RANGE_4G,
        SensorQMI8658::ACC_ODR_LOWPOWER_128Hz,
        SensorQMI8658::LPF_MODE_3);
    imu.enableAccelerometer();
    imu_ok = true;
}

void imu_hal_tick(void) {
    if (!imu_ok) return;
    uint32_t now = millis();
    if (now - last_poll_ms < IMU_POLL_MS) return;
    last_poll_ms = now;

    float ax, ay, az;
    if (!imu.getAccelerometer(ax, ay, az)) return;

    // --- shake detection (runs regardless of the rotation lock) ---
    float mag = sqrtf(ax * ax + ay * ay + az * az);
    if (fabsf(mag - 1.0f) > SHAKE_G_THRESH) {
        if (shake_count < SHAKE_HITS) shake_count++;
        if (shake_count >= SHAKE_HITS && !shake_latched) {
            shake_latched = true;
            Serial.printf("imu: shake detected (|a|=%.2fg)\n", mag);
        }
    } else if (shake_count > 0) {
        shake_count--;
    }

    // --- auto-rotation (frozen while locked) ---
    if (!rotation_enabled) {
        candidate_rotation = current_rotation;
        return;
    }

    uint8_t target = accel_to_rotation(ax, ay);
    if (target == 255 || target == current_rotation) {
        candidate_rotation = current_rotation;
        return;
    }
    if (target != candidate_rotation) {
        candidate_rotation = target;
        candidate_since = now;
    } else if (now - candidate_since >= STABLE_TIME_MS) {
        current_rotation = target;
        Serial.printf("Rotation: %d\n", current_rotation);
    }
}

uint8_t imu_hal_rotation_quadrant(void) { return current_rotation; }

void imu_hal_set_rotation_enabled(bool en) { rotation_enabled = en; }
bool imu_hal_rotation_enabled(void)        { return rotation_enabled; }

bool imu_hal_consume_shake(void) {
    // Clear the latch only — NOT shake_count. This is polled every loop (~5 ms)
    // while imu_hal_tick() samples every IMU_POLL_MS, so zeroing the counter here
    // wiped it between every sample and it could never reach SHAKE_HITS. Let the
    // counter decay naturally in the tick instead.
    bool s = shake_latched;
    shake_latched = false;
    return s;
}
