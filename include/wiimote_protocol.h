//
// Enums, constants and structs related to the Wiimote protocol
//

#pragma once

#include <stdint.h>

#pragma pack(push, 1)

#define HID_INPUT_REPORT  (0xA1)
#define HID_OUTPUT_REPORT (0xA2)

#define WIIM_REPORT_RUMBLE                                       (0x10)
#define WIIM_REPORT_LED                                          (0x11)
#define WIIM_REPORT_DATA_REPORT_MODE                             (0x12)
#define WIIM_REPORT_IR_CAMERA_ENABLE                             (0x13)
#define WIIM_REPORT_SPEAKER_ENABLE                               (0x14)
#define WIIM_REPORT_STATUS_INFO_REQUEST                          (0x15)
#define WIIM_REPORT_WRITE_MEM_AND_REGS                           (0x16)
#define WIIM_REPORT_READ_MEM_AND_REGS                            (0x17)
#define WIIM_REPORT_SPEAKER_DATA                                 (0x18)
#define WIIM_REPORT_SPEAKER_MUTE                                 (0x19)
#define WIIM_REPORT_IR_CAMERA_ENABLE_2                           (0x1A)
#define WIIM_REPORT_STATUS_INFO                                  (0x20)
#define WIIM_REPORT_READ_MEM_AND_REGS_DATA                       (0x21)
#define WIIM_REPORT_ACKNOWLEDGE_OUTPUT                           (0x22)
#define WIIM_REPORT_DATA_BUTTONS                                 (0x30)
#define WIIM_REPORT_DATA_BUTTONS_ACCEL                           (0x31)
#define WIIM_REPORT_DATA_BUTTONS_EXT8                            (0x32)
#define WIIM_REPORT_DATA_BUTTONS_ACCEL_IR12                      (0x33)
#define WIIM_REPORT_DATA_BUTTONS_EXT19                           (0x34)
#define WIIM_REPORT_DATA_BUTTONS_ACCEL_EXT16                     (0x35)
#define WIIM_REPORT_DATA_BUTTONS_IR10_EXT6                       (0x36)
#define WIIM_REPORT_DATA_EXT21                                   (0x3D)
#define WIIM_REPORT_DATA_BUTTONS_ACCEL_IR36_INTER0               (0x3E)
#define WIIM_REPORT_DATA_BUTTONS_ACCEL_IR36_INTER1               (0x3F)

#define WIIM_REPORT_MODE_BUTTONS                                 (0x30)
#define WIIM_REPORT_MODE_BUTTONS_ACCEL                           (0x31)
#define WIIM_REPORT_MODE_BUTTONS_EXT8                            (0x32)
#define WIIM_REPORT_MODE_BUTTONS_ACCEL_IR12                      (0x33)
#define WIIM_REPORT_MODE_BUTTONS_EXT19                           (0x34)
#define WIIM_REPORT_MODE_BUTTONS_ACCEL_EXT16                     (0x35)
#define WIIM_REPORT_MODE_BUTTONS_IR10_EXT6                       (0x36)
#define WIIM_REPORT_MODE_EXT21                                   (0x3D)
#define WIIM_REPORT_MODE_BUTTONS_ACCEL_IR36_INTER0               (0x3E)
#define WIIM_REPORT_MODE_BUTTONS_ACCEL_IR36_INTER1               (0x3F)

#define WIIM_LED_1  (0x10)
#define WIIM_LED_2  (0x20)
#define WIIM_LED_3  (0x40)
#define WIIM_LED_4  (0x80)

#define WIIM_DRM_FLAG_RUMBLE        (0x01)
#define WIIM_DRM_FLAG_CONTINUOUS    (0x04)

#define WIIM_ADDRSPACE_EEPROM (0x00)
#define WIIM_ADDRSPACE_CTLREG (0x04)

struct hid_header {
    uint8_t code;
};

struct wiimote_header {
    struct hid_header hdr;
    uint8_t code;
};

struct pkt_set_data_report_mode {
    struct wiimote_header hdr;
    uint8_t flags;
    uint8_t mode;
};

struct pkt_led {
    struct wiimote_header hdr;
    uint8_t led_ctl;
};

struct pkt_status_request {
    struct wiimote_header hdr;
    uint8_t flags;
};

struct pkt_memory_read {
    struct wiimote_header hdr;
    uint8_t address_space;
    uint8_t off_hi, off_mi, off_lo;
    uint8_t siz_hi, siz_lo;
};

inline void init_memory_read(
        struct pkt_memory_read *pkt,
        uint8_t space, uint32_t addr, uint16_t size
) {
    pkt->hdr.hdr.code = HID_OUTPUT_REPORT;
    pkt->hdr.code = WIIM_REPORT_READ_MEM_AND_REGS;

    pkt->address_space = space;

    pkt->off_lo = addr & 0xFF;
    addr >>= 8;
    pkt->off_mi = addr & 0xFF;
    addr >>= 8;
    pkt->off_hi = addr & 0xFF;

    pkt->siz_lo = size & 0xFF;
    size >>= 8;
    pkt->siz_hi = size & 0xFF;
}

struct pkt_memory_write {
    struct wiimote_header hdr;
    uint8_t address_space;
    uint8_t off_hi, off_mi, off_lo;
    uint8_t siz;
    uint8_t data[16];
};

typedef struct buttons {
    uint8_t left    : 1;
    uint8_t right   : 1;
    uint8_t down    : 1;
    uint8_t up      : 1;
    uint8_t plus    : 1;
    uint8_t misc0   : 1;
    uint8_t misc1   : 1;
    uint8_t unk     : 1;
    uint8_t two     : 1;
    uint8_t one     : 1;
    uint8_t b       : 1;
    uint8_t a       : 1;
    uint8_t minus   : 1;
    uint8_t misc2   : 1;
    uint8_t misc3   : 1;
    uint8_t home    : 1;
} buttons_t;

typedef struct accel_data {
    uint8_t irr0 : 5;
    uint8_t x2 : 2;
    uint8_t irr1 : 6;
    uint8_t y2 : 1;
    uint8_t z2 : 1;
    uint8_t irr2 : 1;
    uint8_t x, y, z;
} accel_data_t;

typedef struct button_accel_hdr {
    struct wiimote_header hdr;
    accel_data_t accel;
} button_accel_hdr_t;

struct pkt_memory_read_response {
    struct wiimote_header hdr;
    buttons_t btn;
    uint8_t size : 4;
    uint8_t error : 4;
    uint8_t off_mi, off_lo;
    uint8_t data[16];
};

typedef struct extension_signature {
    uint16_t x;
    uint16_t base;
    uint8_t state;
    uint8_t id;
} extension_signature_t;

#define EXT_ID_NUNCHUCK     (0x00)
#define EXT_ID_MOTIONPLUS   (0x05)

struct pkt_report_buttons_only {
    struct wiimote_header hdr;
    buttons_t btn;
};

typedef struct motionplus_data {
    uint8_t yaw_down_speed_lo;
    uint8_t roll_left_speed_lo;
    uint8_t pitch_left_speed_lo;

    uint8_t pitch_slow_mode : 1;
    uint8_t yaw_slow_mode : 1;
    uint8_t yaw_down_speed_hi : 6;

    uint8_t ext_connected : 1;
    uint8_t roll_slow_mode : 1;
    uint8_t roll_left_speed_hi: 6;

    uint8_t zero : 1;
    uint8_t one : 1;
    uint8_t pitch_left_speed_hi : 6;
} motionplus_data_t;

typedef struct calibration_data {
    uint8_t x_0g_hi;
    uint8_t y_0g_hi;
    uint8_t z_0g_hi;

    uint8_t z_0g_lo : 2;
    uint8_t y_0g_lo : 2;
    uint8_t x_0g_lo : 2;
    uint8_t zero0 : 2;

    uint8_t x_1g_hi;
    uint8_t y_1g_hi;
    uint8_t z_1g_hi;

    uint8_t z_1g_lo : 2;
    uint8_t y_1g_lo : 2;
    uint8_t x_1g_lo : 2;
    uint8_t zero1 : 2;

    uint8_t unused;
    uint8_t checksum;
} calibration_data_t;

#pragma pack(pop)
