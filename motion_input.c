#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>

#include "motion_input.h"
#include "wiimote_protocol.h"

typedef enum ext_status {
    // Extension detection hasn't begun yet
    EXT_STATUS_UNKNOWN = 0,
    // Detection is in progress
    EXT_STATUS_IN_PROGRESS = 1,
    // Extension type info is available
    EXT_STATUS_FOUND = 2,
} ext_status_t;

typedef enum wiimote_ext_kind {
    EXT_KIND_NONE = 0,
    EXT_KIND_NUNCHUCK,
    EXT_KIND_INACTIVE_MOTION_PLUS,
    EXT_KIND_ACTIVE_MOTION_PLUS,
    EXT_KIND_ACTIVE_MOTION_PLUS_NUNCHUCK_PASSTHRU,
    EXT_KIND_MAX
} wiimote_ext_kind_t;

#define BUTTON_PRESS_RING_SIZ (32)
typedef struct button_press_ring {
    int rd, wr;
    motion_button_press_t ev[BUTTON_PRESS_RING_SIZ];
} button_press_ring_t;

inline void init_btn_press_ring(button_press_ring_t* r) {
    r->rd = 0;
    r->wr = 0;
}

inline void put_btn_press_ring(
        button_press_ring_t* r,
        motion_button_press_t *mb) {
    r->ev[r->wr] = *mb;
    r->wr = (r->wr + 1) % BUTTON_PRESS_RING_SIZ;
    if(r->wr == r->rd) {
        r->rd = r->rd + 1;
    }
}

inline int get_btn_press_ring(
        button_press_ring_t* r,
        motion_button_press_t *mb) {
    if(r->rd == r->wr) {
        return 0;
    }

    *mb = r->ev[r->rd];
    r->rd = (r->rd + 1) % BUTTON_PRESS_RING_SIZ;

    return 1;
}

typedef struct accel_data_f32 {
    float x, y, z;
} accel_data_f32_t;

struct motion_device {
    struct motion_device *next;

    HWIIMOTE hDevice;
    uint8_t current_reporting_mode;
    int rumble;

    ext_status_t ext_status;
    wiimote_ext_kind_t ext_kind;

    int btn_state_ready;
    buttons_t btn_state;
    button_press_ring_t btn_press_ring;

    int accel_changed;
    accel_data_f32_t accel;
};

static struct motion_device *gDevices = NULL;
static int gIsInit = 0;

static void wm_on_device_found(HWIIMOTE hDevice, void *user) {
    struct motion_device **next_ptr = &gDevices;

    while(*next_ptr != NULL) {
        next_ptr = &(*next_ptr)->next;
    }

    *next_ptr = (struct motion_device*)malloc(sizeof(struct motion_device));
    memset(*next_ptr, 0, sizeof(struct motion_device));
    (*next_ptr)->next = NULL;
    (*next_ptr)->hDevice = hDevice;

    (*next_ptr)->current_reporting_mode = 0x30;
}

static void send_led_output_report(HWIIMOTE hDev, uint8_t led_ctl) {
    struct pkt_led pkt;
    pkt.hdr.hdr.code = HID_OUTPUT_REPORT;
    pkt.hdr.code = WIIM_REPORT_LED;
    pkt.led_ctl = led_ctl;

    wiimote_send(hDev, &pkt, sizeof(pkt));
}

static void write_memory(
        struct motion_device *dev,
        uint8_t address_space,
        uint32_t address,
        void *data, uint32_t size) {
    struct pkt_memory_write pkt;
    pkt.hdr.hdr.code = HID_OUTPUT_REPORT;
    pkt.hdr.code = WIIM_REPORT_WRITE_MEM_AND_REGS;
    pkt.address_space = address_space;
    pkt.siz = 16;

    uint32_t remains = size;
    uint32_t cur = address;
    char *cur_data = data;
    while(remains >= 16) {
        uint32_t addr = cur;
        printf("HtoD memcpy space=%u addr=0x%x size=16\n", address_space, addr);
        pkt.off_lo = addr & 0xFF;
        addr >>= 8;
        pkt.off_mi = addr & 0xFF;
        addr >>= 8;
        pkt.off_hi = addr & 0xFF;

        memcpy(pkt.data, cur_data, 16);

        remains -= 16;
        cur += 16;
        cur_data += 16;

        wiimote_send(dev->hDevice, &pkt, sizeof(pkt));
    }

    if(remains > 0) {
        uint32_t addr = cur;
        printf("HtoD memcpy space=%u addr=0x%x size=%d\n", address_space, addr, remains);
        pkt.off_lo = addr & 0xFF;
        addr >>= 8;
        pkt.off_mi = addr & 0xFF;
        addr >>= 8;
        pkt.off_hi = addr & 0xFF;

        pkt.siz = remains;

        memcpy(pkt.data, cur_data, 16);
        wiimote_send(dev->hDevice, &pkt, sizeof(pkt));
    }
}

static void set_report_mode(struct motion_device *dev, int is_continuous, uint8_t report_mode) {
    struct pkt_set_data_report_mode pkt;
    pkt.hdr.hdr.code = HID_OUTPUT_REPORT;
    pkt.hdr.code = WIIM_REPORT_DATA_REPORT_MODE;
    pkt.flags = 0;
    pkt.flags |= (is_continuous) ? WIIM_DRM_FLAG_CONTINUOUS : 0;
    pkt.flags |= (dev->rumble) ? WIIM_DRM_FLAG_RUMBLE : 0;
    pkt.mode = report_mode;

    wiimote_send(dev->hDevice, &pkt, sizeof(pkt));
}

static void rumble(struct motion_device *dev, int rumble) {
    struct pkt_status_request pkt;
    pkt.hdr.hdr.code = HID_OUTPUT_REPORT;
    pkt.hdr.code = WIIM_REPORT_RUMBLE;
    pkt.flags = WIIM_DRM_FLAG_RUMBLE;

    wiimote_send(dev->hDevice, &pkt, sizeof(pkt));
    dev->rumble = rumble;
}

static void request_status_info(struct motion_device *dev) {
    struct pkt_status_request pkt;
    pkt.hdr.hdr.code = HID_OUTPUT_REPORT;
    pkt.hdr.code = WIIM_REPORT_STATUS_INFO_REQUEST;
    pkt.flags = 0;
    pkt.flags |= (dev->rumble) ? WIIM_DRM_FLAG_RUMBLE : 0;

    wiimote_send(dev->hDevice, &pkt, sizeof(pkt));
}

static void sleep_millis(int millis) {
    int secs = millis / 1000;
    int nanos = (millis % 1000) * 1000000;
    struct timespec ts = { secs, nanos };
    nanosleep(&ts, NULL);
}

static void process_extension_signature(struct motion_device *dev, void const *sig) {
    dev->ext_status = EXT_STATUS_FOUND;
    dev->ext_kind = EXT_KIND_NONE;

    extension_signature_t* s = (extension_signature_t*)sig;

    if(s->id == EXT_ID_MOTIONPLUS) {
        if(s->base == 0xA620) {
            dev->ext_kind = EXT_KIND_INACTIVE_MOTION_PLUS;
        } else {
            dev->ext_kind = EXT_KIND_ACTIVE_MOTION_PLUS;
        }
    } else {
        printf("Unknown extension id %u\n", s->id);
    }
}

static void on_memory_read_results(struct motion_device *dev, struct wiimote_header *hdr) {
    struct pkt_memory_read_response* res = (struct pkt_memory_read_response*)hdr;
    if(res->off_mi == 0x00 && res->off_lo == 0xFA && dev->ext_status < EXT_STATUS_FOUND) {
        process_extension_signature(dev, res->data);
    }
}

static void put_button_event(
        struct motion_device *dev,
        motion_button_t btn,
        int released) {
    motion_button_press_t ev = { btn, released };
    put_btn_press_ring(&dev->btn_press_ring, &ev);
    printf("button ev: key:%d released:%d\n", btn, released);
}

static void process_core_buttons(
        struct motion_device *dev,
        struct wiimote_header *hdr) {
    struct pkt_report_buttons_only *rep = (struct pkt_report_buttons_only *)hdr;

    if(!dev->btn_state_ready) {
        dev->btn_state = rep->btn;
        dev->btn_state_ready = 1;
    }

#define CHECK_BUTTON_STATE(BIT, ENUM) \
    if(dev->btn_state.BIT < rep->btn.BIT) put_button_event(dev, ENUM, 0); \
    else if(dev->btn_state.BIT > rep->btn.BIT) put_button_event(dev, ENUM, 1);
    CHECK_BUTTON_STATE(left,    MB_LEFT);
    CHECK_BUTTON_STATE(right,   MB_RIGHT);
    CHECK_BUTTON_STATE(down,    MB_DOWN);
    CHECK_BUTTON_STATE(up,      MB_UP);
    CHECK_BUTTON_STATE(plus,    MB_PLUS);
    CHECK_BUTTON_STATE(two,     MB_TWO);
    CHECK_BUTTON_STATE(one,     MB_ONE);
    CHECK_BUTTON_STATE(b,       MB_B);
    CHECK_BUTTON_STATE(a,       MB_A);
    CHECK_BUTTON_STATE(minus,   MB_MINUS);
    CHECK_BUTTON_STATE(home,    MB_HOME);

    dev->btn_state = rep->btn;
}

static void process_normal_accel_data(
        struct motion_device *dev,
        struct wiimote_header *hdr) {
    struct button_accel_hdr *rep = (struct button_accel_hdr*)hdr;

    process_core_buttons(dev, hdr);

    dev->accel_changed = 1;

    uint32_t x32 = ((uint32_t)rep->accel.x) << 2;
    uint32_t y32 = ((uint32_t)rep->accel.y) << 2;
    uint32_t z32 = ((uint32_t)rep->accel.z) << 2;

    printf("accel raw (%u, %u, %u) ", x32, y32, z32);

    x32 |= (rep->accel.x2 << 0);
    y32 |= (rep->accel.y2 << 1);
    z32 |= (rep->accel.z2 << 1);

    printf("with-extra (%u, %u, %u) ", x32, y32, z32);


    printf("\n");
}

static void handle_input_report(
        struct motion_device *dev,
        char const* buf,
        int len
) {
    struct wiimote_header *hdr = (struct wiimote_header *)buf;
    if(hdr->hdr.code != HID_INPUT_REPORT) {
        assert(0);
        return;
    }

    switch(hdr->code) {
        case WIIM_REPORT_STATUS_INFO:
            set_report_mode(dev, 0, WIIM_REPORT_MODE_BUTTONS_ACCEL_EXT16);
            break;
        case WIIM_REPORT_READ_MEM_AND_REGS_DATA:
            on_memory_read_results(dev, hdr);
            break;
        case WIIM_REPORT_DATA_BUTTONS_ACCEL_EXT16:
            process_normal_accel_data(dev, hdr);
            break;
        default:
            printf("packet code=%d len=%d was not handled\n", hdr->code, len);
            break;
    }
}

static void poll_device(struct motion_device *dev) {
    char buffer[128];
    int rd;
    do {
        rd = wiimote_recv(dev->hDevice, buffer, 128);

        if(rd > 0) {
            handle_input_report(dev, buffer, rd);
        }
    } while(rd != 0);
}

static int disable_encryption(struct motion_device *dev) {
    uint8_t b0 = 0x55;
    uint8_t b1 = 0x00;

    write_memory(dev, WIIM_ADDRSPACE_CTLREG, 0xA400F0, &b0, 1);
    sleep_millis(100);
    write_memory(dev, WIIM_ADDRSPACE_CTLREG, 0xA400FB, &b1, 1);
    sleep_millis(100);

    return 0;
}

static int detect_extension(struct motion_device *dev) {
    struct pkt_memory_read pkt;
    init_memory_read(&pkt, WIIM_ADDRSPACE_CTLREG, 0xa600fa, 6);
    wiimote_send(dev->hDevice, &pkt, sizeof(pkt));

    while(dev->ext_status < EXT_STATUS_FOUND) {
        poll_device(dev);
    }

    if(dev->ext_kind == EXT_KIND_ACTIVE_MOTION_PLUS) {
        uint8_t b1 = 0x04;
        write_memory(dev, WIIM_ADDRSPACE_CTLREG, 0xA600F0, &b1, 1);
        sleep_millis(100);

        set_report_mode(dev, 0, dev->current_reporting_mode);
    }

    if(dev->ext_kind == EXT_KIND_INACTIVE_MOTION_PLUS) {
        uint8_t b0 = 0x55;
        write_memory(dev, WIIM_ADDRSPACE_CTLREG, 0xA600F0, &b0, 1);
        sleep_millis(100);

        detect_extension(dev);
    }

    return 0;
}

static void read_accelerometer_calibration_data(struct motion_device *dev) {
    struct pkt_memory_read pkt;
    init_memory_read(&pkt, WIIM_ADDRSPACE_EEPROM, 0x000016, 10);
    wiimote_send(dev->hDevice, &pkt, sizeof(pkt));
    // TODO: this info is ignored rn
}

static void init_devices() {
    struct motion_device *cur = gDevices;
    while(cur != NULL) {
        cur->rumble = 0;

        set_report_mode(cur, 0, WIIM_REPORT_MODE_BUTTONS_ACCEL_EXT16);
        cur->current_reporting_mode = WIIM_REPORT_MODE_BUTTONS_ACCEL_EXT16;
        set_report_mode(cur, 0, WIIM_REPORT_MODE_BUTTONS_ACCEL_EXT16);
        send_led_output_report(cur->hDevice, 0x10);
        request_status_info(cur);
        read_accelerometer_calibration_data(cur);
        rumble(cur, 1);
        sleep_millis(250);
        rumble(cur, 0);

        disable_encryption(cur);

        detect_extension(cur);

        cur = cur->next;
    }
}

int motion_init(motion_input_config_t const* cfg) {
    assert(cfg != NULL);
    if(cfg == NULL) {
        return 1;
    }

    if(wiimote_init() != 0) {
        return 1;
    }

    if(gIsInit) {
        return 0;
    }

    struct wiimote_listener scan_listener = {
        .on_device_found = wm_on_device_found,
    };

    if(wiimote_scan(&scan_listener, NULL) != 0) {
        printf("wiimote_scan() failed\n");
    }

    init_devices();

    gIsInit = 1;

    return 0;
}

int motion_shutdown() {
    if(!gIsInit) {
        return 0;
    }

    gIsInit = 0;

    if(wiimote_shutdown() != 0) {
        return 1;
    }

    return 0;
}

int motion_poll(motion_event_t *ev) {
    char buffer[128];
    int rd;

    ev->kind = MI_EV_NONE;
    struct motion_device *cur = gDevices;
    while(cur != NULL) {
        rd = wiimote_recv(cur->hDevice, buffer, 128);

        if(rd > 0) {
            // printf("received packet from wiimote len=%d\n", rd);
            handle_input_report(cur, buffer, rd);
        }


        motion_button_press_t mb;
        if(get_btn_press_ring(&cur->btn_press_ring, &mb)) {
            ev->kind = MI_EV_BUTTON;
            ev->btn = mb;
        }

        cur = cur->next;
    }

    return 0;
}

void motion_set_leds(int iPlayer, unsigned mask) {
    struct motion_device *cur = gDevices;
    while(cur != NULL && --iPlayer > 0) {
        cur = cur->next;
    }

    if(cur != NULL) {
        send_led_output_report(cur->hDevice, (mask << 4) & 0xF0);
    }
}
