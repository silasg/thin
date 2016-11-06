#pragma once

#include <pebble.h>

// Layout
#define MARGIN 5
#define HAND_TIP_SIZE_DELTA -2
#define THICKNESS 3
#define ANIMATION_DELAY 300
#define ANIMATION_DURATION 1000
#define HAND_LENGTH_SEC 68
#define HAND_LENGTH_MIN HAND_LENGTH_SEC
#define HAND_LENGTH_HOUR (HAND_LENGTH_SEC - 20)
#define HAND_SECONDS_TAP_DURATION 10

#define PERSIST_DEFAULTS_SET 3489

// Persist
#define PERSIST_KEY_DATE        0
#define PERSIST_KEY_DAY         1
#define PERSIST_KEY_BT          2
#define PERSIST_KEY_BATTERY     3
#define PERSIST_KEY_SECOND_HAND 4
#define PERSIST_KEY_NO_MARKERS  5
#define PERSIST_KEY_LIGHT_THEME 6
#define PERSIST_KEY_SECOND_BATTERY 7
#define PERSIST_KEY_SECOND_NIGHT 8
#define PERSIST_KEY_MINUTE_MARKERS 9
#define PERSIST_KEY_SECOND_TAP 10
#define PERSIST_KEY_DATE_STYLE 11
#define NUM_SETTINGS            12

typedef struct {
  int days;
  int hours;
  int minutes;
  int seconds;
} Time;

#define TIME_HOURS 1
#define TIME_MINUTES 2
#define TIME_SECONDS 4
