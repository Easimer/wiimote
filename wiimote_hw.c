//
// Wiimote communication abstraction layer, POSIX implementation
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <bluetooth/l2cap.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "wiimote_hw.h"

static int iDevID, iSocket;
static int nInitCount = 0;

typedef struct wiimote_device {
    int sock_ctl, sock_dat;
    bdaddr_t addr;
} wiimote_device;

// Determines whether a given Bluetooth device is a Wiiimote
static int is_wiimote(bdaddr_t const* addr) {
    sdp_list_t *response_list = NULL, *search_list, *attrid_list;
    sdp_session_t *session = 0;
    int err;

    // Check OUI; address bytes are in reverse order
    if(addr->b[5] != 0xD8 || addr->b[4] != 0x6B || addr->b[3] != 0xF7) {
        printf("wiimote_hw: device not a wiimote: address\n");
        return 0;
    }

    // Connect to the SDP service
    session = sdp_connect(BDADDR_ANY, addr, SDP_RETRY_IF_BUSY);
    if(session == NULL) {
        printf("wiimote_hw: device not a wiimote: failed to open SDP session\n");
        return 0;
    }

    // HumanInterfaceDeviceService	0x1124	Human Interface Device (HID)
    // NOTE: Used as both Service Class Identifier and Profile Identifier.
    // PnPInformation	0x1200	Device Identification (DID)
    // NOTE: Used as both Service Class Identifier and Profile Identifier.

    uuid_t svc_did, svc_hid;
    sdp_uuid16_create(&svc_did, 0x1200);
    sdp_uuid16_create(&svc_hid, 0x1124);
    uint32_t range = 0xffff;
    attrid_list = sdp_list_append(NULL, &range);

    search_list = sdp_list_append(NULL, &svc_did);

    // Request vendor ID and product ID info from SDP
    err = sdp_service_search_attr_req(session, search_list, SDP_ATTR_REQ_RANGE, attrid_list, &response_list);

    if(err != 0) {
        printf("wiimote_hw: device not a wiimote: attr search failed\n");
        return 0;
    }

    sdp_list_t *r = response_list;

    if(r == NULL) {
        printf("wiimote_hw: device not a wiimote: attr search response list empty\n");
        return 0;
    }

    unsigned short iVendor = 0, iProduct = 0;

    for(; r != NULL; r = r->next) {
        // Service record describing the service
        sdp_record_t *rec = (sdp_record_t*) r->data;
        if(rec->handle == 0x10001) {
            sdp_list_t *a = rec->attrlist;

            for(; a; a = a->next) {
                sdp_data_t *attr = (sdp_data_t*)a->data;
                if(attr->attrId == SDP_ATTR_VENDOR_ID) {
                    iVendor = attr->val.uint16;
                } else if(attr->attrId == SDP_ATTR_PRODUCT_ID) {
                    iProduct = attr->val.uint16;
                } else {
                }
            }
        }
    }

    sdp_close(session);

    if(iVendor == 0x057E && iProduct == 0x0306) {
        return 1;
    }

    printf("wiimote_hw: device not a wiimote: vendor-product ID mismatch\n");
    return 0;
}

int wiimote_init() {
    if(nInitCount > 0) {
        return 0;
    }

    iDevID = hci_get_route(NULL);
    iSocket = hci_open_dev(iDevID);

    if(iDevID < 0 || iSocket < 0) {
        return 1;
    }

    nInitCount++;
    printf("wiimote_init: dev_id=%d socket=%d\n", iDevID, iSocket);

    return 0;
}

int wiimote_shutdown() {
    if(nInitCount == 0) {
        return 0;
    }

    close(iSocket);
    nInitCount--;
    return 0;
}

int wiimote_scan(struct wiimote_listener *l, void *user) {
    assert(nInitCount > 0);
    assert(l != NULL);

    if(nInitCount == 0) {
        return 1;
    }

    if(l == NULL) {
        return 1;
    }

    int nRSP = 0;
    //int sock, flags;
    int flags;
    char addr_buf[19];
    // char name[256];
    inquiry_info *ii;

    ii = malloc(sizeof(inquiry_info) * 16);

    flags = IREQ_CACHE_FLUSH;
    nRSP = hci_inquiry(iDevID, 8, 16, NULL, &ii, flags);

    for(int i = 0; i < nRSP; i++) {
        void *addr = &((ii + i)->bdaddr);
        ba2str(addr, addr_buf);
        printf("Device #%d addr=%s\n", i, addr_buf);

        if(is_wiimote(&ii[i].bdaddr)) {
            printf("Device #%d is a wiimote\n", i);
            int status;

            wiimote_device* dev = (wiimote_device*)malloc(sizeof(wiimote_device));
            memcpy(&dev->addr, &ii[i].bdaddr, sizeof(bdaddr_t));
            dev->sock_ctl = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
            dev->sock_dat = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);

            struct sockaddr_l2 addr = {0};
            addr.l2_family = AF_BLUETOOTH;
            memcpy(&addr.l2_bdaddr, &dev->addr, sizeof(addr.l2_bdaddr));

            addr.l2_psm = 0x11;
            status = connect(dev->sock_ctl, (struct sockaddr*)&addr, sizeof(addr));
            if(status != 0) {
                perror("wiimote_hw: connect(ctl) failed\n");
                close(dev->sock_ctl);
                close(dev->sock_dat);
                free(dev);
                continue;
            }

            addr.l2_psm = 0x13;
            status = connect(dev->sock_dat, (struct sockaddr*)&addr, sizeof(addr));
            if(status != 0) {
                perror("wiimote_hw: connect(dat) failed\n");
                close(dev->sock_ctl);
                close(dev->sock_dat);
                free(dev);
                continue;
            }

            if(l->on_device_found != NULL) {
                l->on_device_found(dev, user);
            }
        }
    }

    free(ii);

    return 0;
}

int wiimote_disconnect(HWIIMOTE hDev) {
    if(hDev == NULL) {
        return 1;
    }

    close(hDev->sock_dat);
    close(hDev->sock_ctl);
    free(hDev);

    return 0;
}

int wiimote_send(HWIIMOTE hDev, void const *data, size_t length) {
    return write(hDev->sock_dat, data, length) == length;
}

int wiimote_recv(HWIIMOTE hDev, void *data, size_t length) {
    int rd;

    rd = recv(hDev->sock_dat, data, length, MSG_DONTWAIT);
    if(rd == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        } else {
            return -1;
        }
    }

    return rd;
}
