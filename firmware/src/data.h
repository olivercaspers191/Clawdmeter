#pragma once
#include <Arduino.h>

struct UsageData {
    float session_pct;       // utilization 0-100 (5h window Pro/Max; spending % Enterprise)
    int session_reset_mins;  // minutes until reset
    float weekly_pct;        // 7-day utilization (Pro/Max only; 0 for Enterprise)
    int weekly_reset_mins;   // minutes until weekly reset (Pro/Max only)
    char status[16];         // "allowed", "limited", etc.
    bool chime;              // play the session-reset chime; false unless daemon opts in
    bool enterprise;         // true = Enterprise spending-limit account
    int time_pct;            // 0-100: fraction of billing period elapsed (Enterprise)
    int period_days;         // total billing period length in days (Enterprise)
    char reset_date[12];     // formatted reset date e.g. "Jul 1" (Enterprise)
    long clock_epoch;        // local wall-clock epoch (s) from daemon; 0 = not provided
    int  clock_fmt;          // 12 or 24 (hour format from daemon); defaults to 24
    // Per-model weekly limit (weekly_scoped, e.g. Fable). Present only while the
    // model is in the subscription; when it leaves, has_fable is false and the UI
    // hides the bar.
    bool  has_fable;         // true = a per-model weekly limit was supplied ("f" key)
    float fable_pct;         // 0-100 utilization of that model's weekly window
    int   fable_reset_mins;  // minutes until it resets
    char  fable_name[16];    // model display name, e.g. "Fable"
    bool ok;                 // data parse succeeded
    bool valid;              // false until first successful parse
};
