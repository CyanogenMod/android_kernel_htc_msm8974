/*
 * Line6 Linux USB driver - 0.9.1beta
 *
 * Copyright (C) 2004-2010 Markus Grabner (grabner@icg.tugraz.at)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation, version 2.
 *
 */

#ifndef POD_H
#define POD_H

#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/usb.h>
#include <linux/wait.h>

#include <sound/core.h>

#include "driver.h"
#include "dumprequest.h"

#define PODXTLIVE_INTERFACE_POD    0
#define PODXTLIVE_INTERFACE_VARIAX 1

#define	POD_NAME_OFFSET 0
#define	POD_NAME_LENGTH 16

#define POD_CONTROL_SIZE 0x80
#define POD_BUFSIZE_DUMPREQ 7
#define POD_STARTUP_DELAY 1000

enum {
	POD_STARTUP_INIT = 1,
	POD_STARTUP_DUMPREQ,
	POD_STARTUP_VERSIONREQ,
	POD_STARTUP_WORKQUEUE,
	POD_STARTUP_SETUP,
	POD_STARTUP_LAST = POD_STARTUP_SETUP - 1
};

struct ValueWait {
	int value;
	wait_queue_head_t wait;
};

struct pod_program {
	unsigned char header[0x20];

	unsigned char control[POD_CONTROL_SIZE];
};

struct usb_line6_pod {
	struct usb_line6 line6;

	struct line6_dump_request dumpreq;

	unsigned char channel_num;

	struct pod_program prog_data;

	struct pod_program prog_data_buf;

	struct ValueWait tuner_mute;

	struct ValueWait tuner_freq;

	struct ValueWait tuner_note;

	struct ValueWait tuner_pitch;

	struct ValueWait monitor_level;

	struct ValueWait routing;

	struct ValueWait clipping;

	struct timer_list startup_timer;

	struct work_struct startup_work;

	int startup_progress;

	unsigned long param_dirty[POD_CONTROL_SIZE / sizeof(unsigned long)];

	unsigned long atomic_flags;

	int serial_number;

	int firmware_version;

	int device_id;

	char dirty;

	char midi_postprocess;
};

extern void line6_pod_disconnect(struct usb_interface *interface);
extern int line6_pod_init(struct usb_interface *interface,
			  struct usb_line6_pod *pod);
extern void line6_pod_midi_postprocess(struct usb_line6_pod *pod,
				       unsigned char *data, int length);
extern void line6_pod_process_message(struct usb_line6_pod *pod);
extern void line6_pod_transmit_parameter(struct usb_line6_pod *pod, int param,
					 int value);

#endif
