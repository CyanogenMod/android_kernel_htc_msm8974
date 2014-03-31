#ifndef _DVB_USB_DIGITV_H_
#define _DVB_USB_DIGITV_H_

#define DVB_USB_LOG_PREFIX "digitv"
#include "dvb-usb.h"

struct digitv_state {
    int is_nxt6000;
};

#define USB_READ_EEPROM         1

#define USB_READ_COFDM          2
#define USB_WRITE_COFDM         5

#define USB_WRITE_TUNER         6

#define USB_READ_REMOTE         3
#define USB_WRITE_REMOTE        7
#define USB_WRITE_REMOTE_TYPE   8

#define USB_DEV_INIT            9

#endif
