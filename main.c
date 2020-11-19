#include <stdio.h>
#include "motion_input.h"

#include <time.h>

static void brightness_demo() {
    int on = 100;
    int off = 0;
    int dir = 1;

    printf("brightness demo\n");

    for(int i = 0; i < 10000; i++) {
        struct timespec slip_on = { 0, on };
        struct timespec slip_off = { 0, off };

        motion_set_leds(0, 0x0F);
        nanosleep(&slip_on, NULL);
        motion_set_leds(0, 0x00);
        nanosleep(&slip_off, NULL);

        on -= dir;
        off += dir;

        if(on == 0 || off == 0) {
            dir = -dir;
        }
    }

    printf("end of brightness demo\n");
}

int main(int argc, char **argv) {
    motion_input_config_t cfg;

    if(motion_init(&cfg) != 0) {
        printf("motion_init() failed\n");
        return 1;
    }

    for(;;) {
        motion_event_t ev;
        motion_poll(&ev);

        if(ev.kind == MI_EV_BUTTON) {
        }
    }

    brightness_demo();

    if(motion_shutdown() != 0) {
        printf("motion_shutdown() failed\n");
        return 1;
    }
    return 0;
}
