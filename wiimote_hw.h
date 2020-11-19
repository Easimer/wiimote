#pragma once

#include <stddef.h>

int wiimote_init();
int wiimote_shutdown();

typedef struct wiimote_device *HWIIMOTE;

typedef void (*wiimote_device_found)(HWIIMOTE hDev, void *user);

struct wiimote_listener {
    wiimote_device_found on_device_found;
};

int wiimote_scan(struct wiimote_listener *l, void *user);

int wiimote_disconnect(HWIIMOTE hDev);
int wiimote_send(HWIIMOTE hDev, void const *data, size_t length);
int wiimote_recv(HWIIMOTE hDev, void       *data, size_t length);
