#pragma once
#include <stdbool.h>

void idle_init(void);
void idle_tick(void);
void idle_note_activity(void);

// Set the "awake" brightness target (0..255). idle owns display brightness
// (it fades between this and 0), so user brightness control routes through
// here. Applied immediately if the screen is currently fully awake; otherwise
// picked up by the next fade-in. See brightness.{h,cpp}.
void idle_set_awake_brightness(uint8_t level);

// Returns true if this press was consumed as a wake-up (caller MUST skip the
// button's normal action). Returns false when already awake — also notes the
// activity, so callers don't need a separate idle_note_activity() call.
bool idle_consume_wake_press(void);

// Touch should NOT count as activity (avoids accidental wakes from pets,
// sleeves, etc.). Callers use this to silently drop touch events while the
// panel is dark. True through the fade-out too, not just once fully dark.
bool idle_is_asleep(void);

// True only once the screen has fully faded to black and settled (not mid-fade).
// The light-sleep gear gates on this so the fade-out/fade-in animations render
// smoothly instead of freezing when the CPU halts.
bool idle_is_asleep_settled(void);

// True once the screen has been dark long enough (IDLE_DEEPSLEEP_TIMEOUT_MS) to
// warrant real deep sleep. The caller stashes state and calls
// power_sleep_enter_deep(). Never true on USB power.
bool idle_should_deep_sleep(void);
