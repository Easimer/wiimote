#pragma once

#include "wiimote_hw.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct motion_input_config {
    int flags;
} motion_input_config_t;

int motion_init(motion_input_config_t const* cfg);
int motion_shutdown();

typedef enum motion_event_kind {
    MI_EV_NONE = 0,
    MI_EV_CONNECTED,
    MI_EV_DISCONNECTED,
    MI_EV_BUTTON,
    MI_EV_ACCEL,
    MI_EV_MAX
} motion_event_kind_t;

typedef enum motion_button {
    MB_LEFT = 0,
    MB_RIGHT,
    MB_DOWN,
    MB_UP,
    MB_PLUS,
    MB_TWO,
    MB_ONE,
    MB_B,
    MB_A,
    MB_MINUS,
    MB_HOME,
    MB_MAX
} motion_button_t;

typedef struct motion_button_press {
    motion_button_t btn;
    int released;
} motion_button_press_t;

typedef struct motion_accel {
    float x, y, z;
} motion_accel_t;

typedef struct motion_event {
    motion_event_kind_t kind;

    union {
        motion_button_press_t btn;
        motion_accel_t accel;
    };
} motion_event_t;

int motion_poll(motion_event_t *ev);

void motion_set_leds(int iPlayer, unsigned mask);

#ifdef __cplusplus
}
#endif
