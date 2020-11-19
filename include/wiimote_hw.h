//
// Wiimote communication abstraction layer
//

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int wiimote_init();
int wiimote_shutdown();

// Handle to a Wiimote device
typedef struct wiimote_device *HWIIMOTE;

typedef void (*wiimote_device_found)(HWIIMOTE hDev, void *user);

struct wiimote_listener {
    wiimote_device_found on_device_found;
};

// Initiates a scan for Wiimotes
// Calls wiimote_listener::on_device_found for every device found.
int wiimote_scan(struct wiimote_listener *l, void *user);

// Disconnect from a Wiimote
// This invalidates the handle
int wiimote_disconnect(HWIIMOTE hDev);

// Send a raw packet to the Wiimote
int wiimote_send(HWIIMOTE hDev, void const *data, size_t length);

// Receive a packet from the Wiimote
// Returns the length of the received packet, zero if no packet was
// received since the last call or -1 on error.
int wiimote_recv(HWIIMOTE hDev, void       *data, size_t length);

#ifdef __cplusplus
}
#endif
