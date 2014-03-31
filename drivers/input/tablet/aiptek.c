/*
 *  Native support for the Aiptek HyperPen USB Tablets
 *  (4000U/5000U/6000U/8000U/12000U)
 *
 *  Copyright (c) 2001      Chris Atenasio   <chris@crud.net>
 *  Copyright (c) 2002-2004 Bryan W. Headley <bwheadley@earthlink.net>
 *
 *  based on wacom.c by
 *     Vojtech Pavlik      <vojtech@suse.cz>
 *     Andreas Bach Aaen   <abach@stofanet.dk>
 *     Clifford Wolf       <clifford@clifford.at>
 *     Sam Mosel           <sam.mosel@computer.org>
 *     James E. Blair      <corvus@gnu.org>
 *     Daniel Egger        <egger@suse.de>
 *
 *  Many thanks to Oliver Kuechemann for his support.
 *
 *  ChangeLog:
 *      v0.1 - Initial release
 *      v0.2 - Hack to get around fake event 28's. (Bryan W. Headley)
 *      v0.3 - Make URB dynamic (Bryan W. Headley, Jun-8-2002)
 *             Released to Linux 2.4.19 and 2.5.x
 *      v0.4 - Rewrote substantial portions of the code to deal with
 *             corrected control sequences, timing, dynamic configuration,
 *             support of 6000U - 12000U, procfs, and macro key support
 *             (Jan-1-2003 - Feb-5-2003, Bryan W. Headley)
 *      v1.0 - Added support for diagnostic messages, count of messages
 *             received from URB - Mar-8-2003, Bryan W. Headley
 *      v1.1 - added support for tablet resolution, changed DV and proximity
 *             some corrections - Jun-22-2003, martin schneebacher
 *           - Added support for the sysfs interface, deprecating the
 *             procfs interface for 2.5.x kernel. Also added support for
 *             Wheel command. Bryan W. Headley July-15-2003.
 *      v1.2 - Reworked jitter timer as a kernel thread.
 *             Bryan W. Headley November-28-2003/Jan-10-2004.
 *      v1.3 - Repaired issue of kernel thread going nuts on single-processor
 *             machines, introduced programmableDelay as a command line
 *             parameter. Feb 7 2004, Bryan W. Headley.
 *      v1.4 - Re-wire jitter so it does not require a thread. Courtesy of
 *             Rene van Paassen. Added reporting of physical pointer device
 *             (e.g., stylus, mouse in reports 2, 3, 4, 5. We don't know
 *             for reports 1, 6.)
 *             what physical device reports for reports 1, 6.) Also enabled
 *             MOUSE and LENS tool button modes. Renamed "rubber" to "eraser".
 *             Feb 20, 2004, Bryan W. Headley.
 *      v1.5 - Added previousJitterable, so we don't do jitter delay when the
 *             user is holding a button down for periods of time.
 *
 * NOTE:
 *      This kernel driver is augmented by the "Aiptek" XFree86 input
 *      driver for your X server, as well as the Gaiptek GUI Front-end
 *      "Tablet Manager".
 *      These three products are highly interactive with one another,
 *      so therefore it's easier to document them all as one subsystem.
 *      Please visit the project's "home page", located at,
 *      http://aiptektablet.sourceforge.net.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>

#define DRIVER_VERSION "v2.3 (May 2, 2007)"
#define DRIVER_AUTHOR  "Bryan W. Headley/Chris Atenasio/Cedric Brun/Rene van Paassen"
#define DRIVER_DESC    "Aiptek HyperPen USB Tablet Driver (Linux 2.6.x)"


#define USB_VENDOR_ID_AIPTEK				0x08ca
#define USB_VENDOR_ID_KYE				0x0458
#define USB_REQ_GET_REPORT				0x01
#define USB_REQ_SET_REPORT				0x09

#define AIPTEK_POINTER_ONLY_MOUSE_MODE			0
#define AIPTEK_POINTER_ONLY_STYLUS_MODE			1
#define AIPTEK_POINTER_EITHER_MODE			2

#define AIPTEK_POINTER_ALLOW_MOUSE_MODE(a)		\
	(a == AIPTEK_POINTER_ONLY_MOUSE_MODE ||		\
	 a == AIPTEK_POINTER_EITHER_MODE)
#define AIPTEK_POINTER_ALLOW_STYLUS_MODE(a)		\
	(a == AIPTEK_POINTER_ONLY_STYLUS_MODE ||	\
	 a == AIPTEK_POINTER_EITHER_MODE)

#define AIPTEK_COORDINATE_RELATIVE_MODE			0
#define AIPTEK_COORDINATE_ABSOLUTE_MODE			1

#define AIPTEK_TILT_MIN					(-128)
#define AIPTEK_TILT_MAX					127
#define AIPTEK_TILT_DISABLE				(-10101)

#define AIPTEK_WHEEL_MIN				0
#define AIPTEK_WHEEL_MAX				1024
#define AIPTEK_WHEEL_DISABLE				(-10101)

#define AIPTEK_TOOL_BUTTON_PEN_MODE			BTN_TOOL_PEN
#define AIPTEK_TOOL_BUTTON_PENCIL_MODE			BTN_TOOL_PENCIL
#define AIPTEK_TOOL_BUTTON_BRUSH_MODE			BTN_TOOL_BRUSH
#define AIPTEK_TOOL_BUTTON_AIRBRUSH_MODE		BTN_TOOL_AIRBRUSH
#define AIPTEK_TOOL_BUTTON_ERASER_MODE			BTN_TOOL_RUBBER
#define AIPTEK_TOOL_BUTTON_MOUSE_MODE			BTN_TOOL_MOUSE
#define AIPTEK_TOOL_BUTTON_LENS_MODE			BTN_TOOL_LENS

#define AIPTEK_DIAGNOSTIC_NA				0
#define AIPTEK_DIAGNOSTIC_SENDING_RELATIVE_IN_ABSOLUTE	1
#define AIPTEK_DIAGNOSTIC_SENDING_ABSOLUTE_IN_RELATIVE	2
#define AIPTEK_DIAGNOSTIC_TOOL_DISALLOWED		3

#define AIPTEK_JITTER_DELAY_DEFAULT			50

#define AIPTEK_PROGRAMMABLE_DELAY_25		25
#define AIPTEK_PROGRAMMABLE_DELAY_50		50
#define AIPTEK_PROGRAMMABLE_DELAY_100		100
#define AIPTEK_PROGRAMMABLE_DELAY_200		200
#define AIPTEK_PROGRAMMABLE_DELAY_300		300
#define AIPTEK_PROGRAMMABLE_DELAY_400		400
#define AIPTEK_PROGRAMMABLE_DELAY_DEFAULT	AIPTEK_PROGRAMMABLE_DELAY_400

#define AIPTEK_MOUSE_LEFT_BUTTON		0x04
#define AIPTEK_MOUSE_RIGHT_BUTTON		0x08
#define AIPTEK_MOUSE_MIDDLE_BUTTON		0x10

#define AIPTEK_STYLUS_LOWER_BUTTON		0x08
#define AIPTEK_STYLUS_UPPER_BUTTON		0x10

#define AIPTEK_PACKET_LENGTH			8

#define AIPTEK_REPORT_TOOL_UNKNOWN		0x10
#define AIPTEK_REPORT_TOOL_STYLUS		0x20
#define AIPTEK_REPORT_TOOL_MOUSE		0x40

static int programmableDelay = AIPTEK_PROGRAMMABLE_DELAY_DEFAULT;
static int jitterDelay = AIPTEK_JITTER_DELAY_DEFAULT;

struct aiptek_features {
	int odmCode;		
	int modelCode;		
	int firmwareCode;	
	char usbPath[64 + 1];	
};

struct aiptek_settings {
	int pointerMode;	
	int coordinateMode;	
	int toolMode;		
	int xTilt;		
	int yTilt;		
	int wheel;		
	int stylusButtonUpper;	
	int stylusButtonLower;	
	int mouseButtonLeft;	
	int mouseButtonMiddle;	
	int mouseButtonRight;	
	int programmableDelay;	
	int jitterDelay;	
};

struct aiptek {
	struct input_dev *inputdev;		
	struct usb_device *usbdev;		
	struct urb *urb;			
	dma_addr_t data_dma;			
	struct aiptek_features features;	
	struct aiptek_settings curSetting;	
	struct aiptek_settings newSetting;	
	unsigned int ifnum;			
	int diagnostic;				
	unsigned long eventCount;		
	int inDelay;				
	unsigned long endDelay;			
	int previousJitterable;			

	int lastMacro;				
	int previousToolMode;			
	unsigned char *data;			
};

static const int eventTypes[] = {
        EV_KEY, EV_ABS, EV_REL, EV_MSC,
};

static const int absEvents[] = {
        ABS_X, ABS_Y, ABS_PRESSURE, ABS_TILT_X, ABS_TILT_Y,
        ABS_WHEEL, ABS_MISC,
};

static const int relEvents[] = {
        REL_X, REL_Y, REL_WHEEL,
};

static const int buttonEvents[] = {
	BTN_LEFT, BTN_RIGHT, BTN_MIDDLE,
	BTN_TOOL_PEN, BTN_TOOL_RUBBER, BTN_TOOL_PENCIL, BTN_TOOL_AIRBRUSH,
	BTN_TOOL_BRUSH, BTN_TOOL_MOUSE, BTN_TOOL_LENS, BTN_TOUCH,
	BTN_STYLUS, BTN_STYLUS2,
};

static const int macroKeyEvents[] = {
	KEY_ESC, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
	KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11,
	KEY_F12, KEY_F13, KEY_F14, KEY_F15, KEY_F16, KEY_F17,
	KEY_F18, KEY_F19, KEY_F20, KEY_F21, KEY_F22, KEY_F23,
	KEY_F24, KEY_STOP, KEY_AGAIN, KEY_PROPS, KEY_UNDO,
	KEY_FRONT, KEY_COPY, KEY_OPEN, KEY_PASTE, 0
};

#define AIPTEK_INVALID_VALUE	-1

struct aiptek_map {
	const char *string;
	int value;
};

static int map_str_to_val(const struct aiptek_map *map, const char *str, size_t count)
{
	const struct aiptek_map *p;

	if (str[count - 1] == '\n')
		count--;

	for (p = map; p->string; p++)
	        if (!strncmp(str, p->string, count))
			return p->value;

	return AIPTEK_INVALID_VALUE;
}

static const char *map_val_to_str(const struct aiptek_map *map, int val)
{
	const struct aiptek_map *p;

	for (p = map; p->value != AIPTEK_INVALID_VALUE; p++)
		if (val == p->value)
			return p->string;

	return "unknown";
}


static void aiptek_irq(struct urb *urb)
{
	struct aiptek *aiptek = urb->context;
	unsigned char *data = aiptek->data;
	struct input_dev *inputdev = aiptek->inputdev;
	int jitterable = 0;
	int retval, macro, x, y, z, left, right, middle, p, dv, tip, bs, pck;

	switch (urb->status) {
	case 0:
		
		break;

	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		
		dbg("%s - urb shutting down with status: %d",
		    __func__, urb->status);
		return;

	default:
		dbg("%s - nonzero urb status received: %d",
		    __func__, urb->status);
		goto exit;
	}

	if (aiptek->inDelay == 1 && time_after(aiptek->endDelay, jiffies)) {
		goto exit;
	}

	aiptek->inDelay = 0;
	aiptek->eventCount++;

	if (data[0] == 1) {
		if (aiptek->curSetting.coordinateMode ==
		    AIPTEK_COORDINATE_ABSOLUTE_MODE) {
			aiptek->diagnostic =
			    AIPTEK_DIAGNOSTIC_SENDING_RELATIVE_IN_ABSOLUTE;
		} else {
			x = (signed char) data[2];
			y = (signed char) data[3];

			jitterable = data[1] & 0x07;

			left = (data[1] & aiptek->curSetting.mouseButtonLeft >> 2) != 0 ? 1 : 0;
			right = (data[1] & aiptek->curSetting.mouseButtonRight >> 2) != 0 ? 1 : 0;
			middle = (data[1] & aiptek->curSetting.mouseButtonMiddle >> 2) != 0 ? 1 : 0;

			input_report_key(inputdev, BTN_LEFT, left);
			input_report_key(inputdev, BTN_MIDDLE, middle);
			input_report_key(inputdev, BTN_RIGHT, right);

			input_report_abs(inputdev, ABS_MISC,
					 1 | AIPTEK_REPORT_TOOL_UNKNOWN);
			input_report_rel(inputdev, REL_X, x);
			input_report_rel(inputdev, REL_Y, y);

			if (aiptek->curSetting.wheel != AIPTEK_WHEEL_DISABLE) {
				input_report_rel(inputdev, REL_WHEEL,
						 aiptek->curSetting.wheel);
				aiptek->curSetting.wheel = AIPTEK_WHEEL_DISABLE;
			}
			if (aiptek->lastMacro != -1) {
			        input_report_key(inputdev,
						 macroKeyEvents[aiptek->lastMacro], 0);
				aiptek->lastMacro = -1;
			}
			input_sync(inputdev);
		}
	}
	else if (data[0] == 2) {
		if (aiptek->curSetting.coordinateMode == AIPTEK_COORDINATE_RELATIVE_MODE) {
			aiptek->diagnostic = AIPTEK_DIAGNOSTIC_SENDING_ABSOLUTE_IN_RELATIVE;
		} else if (!AIPTEK_POINTER_ALLOW_STYLUS_MODE
			    (aiptek->curSetting.pointerMode)) {
				aiptek->diagnostic = AIPTEK_DIAGNOSTIC_TOOL_DISALLOWED;
		} else {
			x = get_unaligned_le16(data + 1);
			y = get_unaligned_le16(data + 3);
			z = get_unaligned_le16(data + 6);

			dv = (data[5] & 0x01) != 0 ? 1 : 0;
			p = (data[5] & 0x02) != 0 ? 1 : 0;
			tip = (data[5] & 0x04) != 0 ? 1 : 0;

			jitterable = data[5] & 0x18;

			bs = (data[5] & aiptek->curSetting.stylusButtonLower) != 0 ? 1 : 0;
			pck = (data[5] & aiptek->curSetting.stylusButtonUpper) != 0 ? 1 : 0;

			if (dv != 0) {
				if (aiptek->previousToolMode !=
				    aiptek->curSetting.toolMode) {
				        input_report_key(inputdev,
							 aiptek->previousToolMode, 0);
					input_report_key(inputdev,
							 aiptek->curSetting.toolMode,
							 1);
					aiptek->previousToolMode =
					          aiptek->curSetting.toolMode;
				}

				if (p != 0) {
					input_report_abs(inputdev, ABS_X, x);
					input_report_abs(inputdev, ABS_Y, y);
					input_report_abs(inputdev, ABS_PRESSURE, z);

					input_report_key(inputdev, BTN_TOUCH, tip);
					input_report_key(inputdev, BTN_STYLUS, bs);
					input_report_key(inputdev, BTN_STYLUS2, pck);

					if (aiptek->curSetting.xTilt !=
					    AIPTEK_TILT_DISABLE) {
						input_report_abs(inputdev,
								 ABS_TILT_X,
								 aiptek->curSetting.xTilt);
					}
					if (aiptek->curSetting.yTilt != AIPTEK_TILT_DISABLE) {
						input_report_abs(inputdev,
								 ABS_TILT_Y,
								 aiptek->curSetting.yTilt);
					}

					if (aiptek->curSetting.wheel !=
					    AIPTEK_WHEEL_DISABLE) {
						input_report_abs(inputdev,
								 ABS_WHEEL,
								 aiptek->curSetting.wheel);
						aiptek->curSetting.wheel = AIPTEK_WHEEL_DISABLE;
					}
				}
				input_report_abs(inputdev, ABS_MISC, p | AIPTEK_REPORT_TOOL_STYLUS);
				if (aiptek->lastMacro != -1) {
			                input_report_key(inputdev,
							 macroKeyEvents[aiptek->lastMacro], 0);
					aiptek->lastMacro = -1;
				}
				input_sync(inputdev);
			}
		}
	}
	else if (data[0] == 3) {
		if (aiptek->curSetting.coordinateMode == AIPTEK_COORDINATE_RELATIVE_MODE) {
			aiptek->diagnostic = AIPTEK_DIAGNOSTIC_SENDING_ABSOLUTE_IN_RELATIVE;
		} else if (!AIPTEK_POINTER_ALLOW_MOUSE_MODE
			(aiptek->curSetting.pointerMode)) {
			aiptek->diagnostic = AIPTEK_DIAGNOSTIC_TOOL_DISALLOWED;
		} else {
			x = get_unaligned_le16(data + 1);
			y = get_unaligned_le16(data + 3);

			jitterable = data[5] & 0x1c;

			dv = (data[5] & 0x01) != 0 ? 1 : 0;
			p = (data[5] & 0x02) != 0 ? 1 : 0;
			left = (data[5] & aiptek->curSetting.mouseButtonLeft) != 0 ? 1 : 0;
			right = (data[5] & aiptek->curSetting.mouseButtonRight) != 0 ? 1 : 0;
			middle = (data[5] & aiptek->curSetting.mouseButtonMiddle) != 0 ? 1 : 0;

			if (dv != 0) {
				if (aiptek->previousToolMode !=
				    aiptek->curSetting.toolMode) {
				        input_report_key(inputdev,
							 aiptek->previousToolMode, 0);
					input_report_key(inputdev,
							 aiptek->curSetting.toolMode,
							 1);
					aiptek->previousToolMode =
					          aiptek->curSetting.toolMode;
				}

				if (p != 0) {
					input_report_abs(inputdev, ABS_X, x);
					input_report_abs(inputdev, ABS_Y, y);

					input_report_key(inputdev, BTN_LEFT, left);
					input_report_key(inputdev, BTN_MIDDLE, middle);
					input_report_key(inputdev, BTN_RIGHT, right);

					if (aiptek->curSetting.wheel != AIPTEK_WHEEL_DISABLE) {
						input_report_abs(inputdev,
								 ABS_WHEEL,
								 aiptek->curSetting.wheel);
						aiptek->curSetting.wheel = AIPTEK_WHEEL_DISABLE;
					}
				}
				input_report_abs(inputdev, ABS_MISC, p | AIPTEK_REPORT_TOOL_MOUSE);
				if (aiptek->lastMacro != -1) {
			                input_report_key(inputdev,
							 macroKeyEvents[aiptek->lastMacro], 0);
				        aiptek->lastMacro = -1;
				}
				input_sync(inputdev);
			}
		}
	}
	else if (data[0] == 4) {
		jitterable = data[1] & 0x18;

		dv = (data[1] & 0x01) != 0 ? 1 : 0;
		p = (data[1] & 0x02) != 0 ? 1 : 0;
		tip = (data[1] & 0x04) != 0 ? 1 : 0;
		bs = (data[1] & aiptek->curSetting.stylusButtonLower) != 0 ? 1 : 0;
		pck = (data[1] & aiptek->curSetting.stylusButtonUpper) != 0 ? 1 : 0;

		macro = dv && p && tip && !(data[3] & 1) ? (data[3] >> 1) : -1;
		z = get_unaligned_le16(data + 4);

		if (dv) {
		        if (aiptek->previousToolMode !=
			    aiptek->curSetting.toolMode) {
			        input_report_key(inputdev,
						 aiptek->previousToolMode, 0);
				input_report_key(inputdev,
						 aiptek->curSetting.toolMode,
						 1);
				aiptek->previousToolMode =
				        aiptek->curSetting.toolMode;
			}
		}

		if (aiptek->lastMacro != -1 && aiptek->lastMacro != macro) {
		        input_report_key(inputdev, macroKeyEvents[aiptek->lastMacro], 0);
			aiptek->lastMacro = -1;
		}

		if (macro != -1 && macro != aiptek->lastMacro) {
			input_report_key(inputdev, macroKeyEvents[macro], 1);
			aiptek->lastMacro = macro;
		}
		input_report_abs(inputdev, ABS_MISC,
				 p | AIPTEK_REPORT_TOOL_STYLUS);
		input_sync(inputdev);
	}
	else if (data[0] == 5) {
		jitterable = data[1] & 0x1c;

		dv = (data[1] & 0x01) != 0 ? 1 : 0;
		p = (data[1] & 0x02) != 0 ? 1 : 0;
		left = (data[1]& aiptek->curSetting.mouseButtonLeft) != 0 ? 1 : 0;
		right = (data[1] & aiptek->curSetting.mouseButtonRight) != 0 ? 1 : 0;
		middle = (data[1] & aiptek->curSetting.mouseButtonMiddle) != 0 ? 1 : 0;
		macro = dv && p && left && !(data[3] & 1) ? (data[3] >> 1) : 0;

		if (dv) {
		        if (aiptek->previousToolMode !=
			    aiptek->curSetting.toolMode) {
		                input_report_key(inputdev,
						 aiptek->previousToolMode, 0);
			        input_report_key(inputdev,
						 aiptek->curSetting.toolMode, 1);
			        aiptek->previousToolMode = aiptek->curSetting.toolMode;
			}
		}

		if (aiptek->lastMacro != -1 && aiptek->lastMacro != macro) {
		        input_report_key(inputdev, macroKeyEvents[aiptek->lastMacro], 0);
			aiptek->lastMacro = -1;
		}

		if (macro != -1 && macro != aiptek->lastMacro) {
			input_report_key(inputdev, macroKeyEvents[macro], 1);
			aiptek->lastMacro = macro;
		}

		input_report_abs(inputdev, ABS_MISC,
				 p | AIPTEK_REPORT_TOOL_MOUSE);
		input_sync(inputdev);
	}
	else if (data[0] == 6) {
		macro = get_unaligned_le16(data + 1);
		if (macro > 0) {
			input_report_key(inputdev, macroKeyEvents[macro - 1],
					 0);
		}
		if (macro < 25) {
			input_report_key(inputdev, macroKeyEvents[macro + 1],
					 0);
		}

		if (aiptek->previousToolMode !=
		    aiptek->curSetting.toolMode) {
		        input_report_key(inputdev,
					 aiptek->previousToolMode, 0);
			input_report_key(inputdev,
					 aiptek->curSetting.toolMode,
					 1);
			aiptek->previousToolMode =
				aiptek->curSetting.toolMode;
		}

		input_report_key(inputdev, macroKeyEvents[macro], 1);
		input_report_abs(inputdev, ABS_MISC,
				 1 | AIPTEK_REPORT_TOOL_UNKNOWN);
		input_sync(inputdev);
	} else {
		dbg("Unknown report %d", data[0]);
	}


	if (aiptek->previousJitterable != jitterable &&
	    aiptek->curSetting.jitterDelay != 0 && aiptek->inDelay != 1) {
		aiptek->endDelay = jiffies +
		    ((aiptek->curSetting.jitterDelay * HZ) / 1000);
		aiptek->inDelay = 1;
	}
	aiptek->previousJitterable = jitterable;

exit:
	retval = usb_submit_urb(urb, GFP_ATOMIC);
	if (retval != 0) {
		err("%s - usb_submit_urb failed with result %d",
		    __func__, retval);
	}
}

static const struct usb_device_id aiptek_ids[] = {
	{USB_DEVICE(USB_VENDOR_ID_AIPTEK, 0x01)},
	{USB_DEVICE(USB_VENDOR_ID_AIPTEK, 0x10)},
	{USB_DEVICE(USB_VENDOR_ID_AIPTEK, 0x20)},
	{USB_DEVICE(USB_VENDOR_ID_AIPTEK, 0x21)},
	{USB_DEVICE(USB_VENDOR_ID_AIPTEK, 0x22)},
	{USB_DEVICE(USB_VENDOR_ID_AIPTEK, 0x23)},
	{USB_DEVICE(USB_VENDOR_ID_AIPTEK, 0x24)},
	{USB_DEVICE(USB_VENDOR_ID_KYE, 0x5003)},
	{}
};

MODULE_DEVICE_TABLE(usb, aiptek_ids);

static int aiptek_open(struct input_dev *inputdev)
{
	struct aiptek *aiptek = input_get_drvdata(inputdev);

	aiptek->urb->dev = aiptek->usbdev;
	if (usb_submit_urb(aiptek->urb, GFP_KERNEL) != 0)
		return -EIO;

	return 0;
}

static void aiptek_close(struct input_dev *inputdev)
{
	struct aiptek *aiptek = input_get_drvdata(inputdev);

	usb_kill_urb(aiptek->urb);
}

static int
aiptek_set_report(struct aiptek *aiptek,
		  unsigned char report_type,
		  unsigned char report_id, void *buffer, int size)
{
	return usb_control_msg(aiptek->usbdev,
			       usb_sndctrlpipe(aiptek->usbdev, 0),
			       USB_REQ_SET_REPORT,
			       USB_TYPE_CLASS | USB_RECIP_INTERFACE |
			       USB_DIR_OUT, (report_type << 8) + report_id,
			       aiptek->ifnum, buffer, size, 5000);
}

static int
aiptek_get_report(struct aiptek *aiptek,
		  unsigned char report_type,
		  unsigned char report_id, void *buffer, int size)
{
	return usb_control_msg(aiptek->usbdev,
			       usb_rcvctrlpipe(aiptek->usbdev, 0),
			       USB_REQ_GET_REPORT,
			       USB_TYPE_CLASS | USB_RECIP_INTERFACE |
			       USB_DIR_IN, (report_type << 8) + report_id,
			       aiptek->ifnum, buffer, size, 5000);
}

static int
aiptek_command(struct aiptek *aiptek, unsigned char command, unsigned char data)
{
	const int sizeof_buf = 3 * sizeof(u8);
	int ret;
	u8 *buf;

	buf = kmalloc(sizeof_buf, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf[0] = 2;
	buf[1] = command;
	buf[2] = data;

	if ((ret =
	     aiptek_set_report(aiptek, 3, 2, buf, sizeof_buf)) != sizeof_buf) {
		dbg("aiptek_program: failed, tried to send: 0x%02x 0x%02x",
		    command, data);
	}
	kfree(buf);
	return ret < 0 ? ret : 0;
}

static int
aiptek_query(struct aiptek *aiptek, unsigned char command, unsigned char data)
{
	const int sizeof_buf = 3 * sizeof(u8);
	int ret;
	u8 *buf;

	buf = kmalloc(sizeof_buf, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf[0] = 2;
	buf[1] = command;
	buf[2] = data;

	if (aiptek_command(aiptek, command, data) != 0) {
		kfree(buf);
		return -EIO;
	}
	msleep(aiptek->curSetting.programmableDelay);

	if ((ret =
	     aiptek_get_report(aiptek, 3, 2, buf, sizeof_buf)) != sizeof_buf) {
		dbg("aiptek_query failed: returned 0x%02x 0x%02x 0x%02x",
		    buf[0], buf[1], buf[2]);
		ret = -EIO;
	} else {
		ret = get_unaligned_le16(buf + 1);
	}
	kfree(buf);
	return ret;
}

static int aiptek_program_tablet(struct aiptek *aiptek)
{
	int ret;
	
	if ((ret = aiptek_command(aiptek, 0x18, 0x04)) < 0)
		return ret;

	
	if ((ret = aiptek_query(aiptek, 0x02, 0x00)) < 0)
		return ret;
	aiptek->features.modelCode = ret & 0xff;

	
	if ((ret = aiptek_query(aiptek, 0x03, 0x00)) < 0)
		return ret;
	aiptek->features.odmCode = ret;

	
	if ((ret = aiptek_query(aiptek, 0x04, 0x00)) < 0)
		return ret;
	aiptek->features.firmwareCode = ret;

	
	if ((ret = aiptek_query(aiptek, 0x01, 0x00)) < 0)
		return ret;
	input_set_abs_params(aiptek->inputdev, ABS_X, 0, ret - 1, 0, 0);

	
	if ((ret = aiptek_query(aiptek, 0x01, 0x01)) < 0)
		return ret;
	input_set_abs_params(aiptek->inputdev, ABS_Y, 0, ret - 1, 0, 0);

	
	if ((ret = aiptek_query(aiptek, 0x08, 0x00)) < 0)
		return ret;
	input_set_abs_params(aiptek->inputdev, ABS_PRESSURE, 0, ret - 1, 0, 0);

	if (aiptek->curSetting.coordinateMode ==
	    AIPTEK_COORDINATE_ABSOLUTE_MODE) {
		
		if ((ret = aiptek_command(aiptek, 0x10, 0x01)) < 0) {
			return ret;
		}
	} else {
		
		if ((ret = aiptek_command(aiptek, 0x10, 0x00)) < 0) {
			return ret;
		}
	}

	
	if ((ret = aiptek_command(aiptek, 0x11, 0x02)) < 0)
		return ret;
#if 0
	
	if ((ret = aiptek_command(aiptek, 0x17, 0x00)) < 0)
		return ret;
#endif

	
	if ((ret = aiptek_command(aiptek, 0x12, 0xff)) < 0)
		return ret;

	aiptek->diagnostic = AIPTEK_DIAGNOSTIC_NA;
	aiptek->eventCount = 0;

	return 0;
}


static ssize_t show_tabletSize(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%dx%d\n",
			input_abs_get_max(aiptek->inputdev, ABS_X) + 1,
			input_abs_get_max(aiptek->inputdev, ABS_Y) + 1);
}

static DEVICE_ATTR(size, S_IRUGO, show_tabletSize, NULL);

static struct aiptek_map pointer_mode_map[] = {
	{ "stylus",	AIPTEK_POINTER_ONLY_STYLUS_MODE },
	{ "mouse",	AIPTEK_POINTER_ONLY_MOUSE_MODE },
	{ "either",	AIPTEK_POINTER_EITHER_MODE },
	{ NULL,		AIPTEK_INVALID_VALUE }
};

static ssize_t show_tabletPointerMode(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			map_val_to_str(pointer_mode_map,
					aiptek->curSetting.pointerMode));
}

static ssize_t
store_tabletPointerMode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);
	int new_mode = map_str_to_val(pointer_mode_map, buf, count);

	if (new_mode == AIPTEK_INVALID_VALUE)
		return -EINVAL;

	aiptek->newSetting.pointerMode = new_mode;
	return count;
}

static DEVICE_ATTR(pointer_mode,
		   S_IRUGO | S_IWUSR,
		   show_tabletPointerMode, store_tabletPointerMode);


static struct aiptek_map coordinate_mode_map[] = {
	{ "absolute",	AIPTEK_COORDINATE_ABSOLUTE_MODE },
	{ "relative",	AIPTEK_COORDINATE_RELATIVE_MODE },
	{ NULL,		AIPTEK_INVALID_VALUE }
};

static ssize_t show_tabletCoordinateMode(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			map_val_to_str(coordinate_mode_map,
					aiptek->curSetting.coordinateMode));
}

static ssize_t
store_tabletCoordinateMode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);
	int new_mode = map_str_to_val(coordinate_mode_map, buf, count);

	if (new_mode == AIPTEK_INVALID_VALUE)
		return -EINVAL;

	aiptek->newSetting.coordinateMode = new_mode;
	return count;
}

static DEVICE_ATTR(coordinate_mode,
		   S_IRUGO | S_IWUSR,
		   show_tabletCoordinateMode, store_tabletCoordinateMode);


static struct aiptek_map tool_mode_map[] = {
	{ "mouse",	AIPTEK_TOOL_BUTTON_MOUSE_MODE },
	{ "eraser",	AIPTEK_TOOL_BUTTON_ERASER_MODE },
	{ "pencil",	AIPTEK_TOOL_BUTTON_PENCIL_MODE },
	{ "pen",	AIPTEK_TOOL_BUTTON_PEN_MODE },
	{ "brush",	AIPTEK_TOOL_BUTTON_BRUSH_MODE },
	{ "airbrush",	AIPTEK_TOOL_BUTTON_AIRBRUSH_MODE },
	{ "lens",	AIPTEK_TOOL_BUTTON_LENS_MODE },
	{ NULL,		AIPTEK_INVALID_VALUE }
};

static ssize_t show_tabletToolMode(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			map_val_to_str(tool_mode_map,
					aiptek->curSetting.toolMode));
}

static ssize_t
store_tabletToolMode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);
	int new_mode = map_str_to_val(tool_mode_map, buf, count);

	if (new_mode == AIPTEK_INVALID_VALUE)
		return -EINVAL;

	aiptek->newSetting.toolMode = new_mode;
	return count;
}

static DEVICE_ATTR(tool_mode,
		   S_IRUGO | S_IWUSR,
		   show_tabletToolMode, store_tabletToolMode);

static ssize_t show_tabletXtilt(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	if (aiptek->curSetting.xTilt == AIPTEK_TILT_DISABLE) {
		return snprintf(buf, PAGE_SIZE, "disable\n");
	} else {
		return snprintf(buf, PAGE_SIZE, "%d\n",
				aiptek->curSetting.xTilt);
	}
}

static ssize_t
store_tabletXtilt(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);
	int x;

	if (kstrtoint(buf, 10, &x)) {
		size_t len = buf[count - 1] == '\n' ? count - 1 : count;

		if (strncmp(buf, "disable", len))
			return -EINVAL;

		aiptek->newSetting.xTilt = AIPTEK_TILT_DISABLE;
	} else {
		if (x < AIPTEK_TILT_MIN || x > AIPTEK_TILT_MAX)
			return -EINVAL;

		aiptek->newSetting.xTilt = x;
	}

	return count;
}

static DEVICE_ATTR(xtilt,
		   S_IRUGO | S_IWUSR, show_tabletXtilt, store_tabletXtilt);

static ssize_t show_tabletYtilt(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	if (aiptek->curSetting.yTilt == AIPTEK_TILT_DISABLE) {
		return snprintf(buf, PAGE_SIZE, "disable\n");
	} else {
		return snprintf(buf, PAGE_SIZE, "%d\n",
				aiptek->curSetting.yTilt);
	}
}

static ssize_t
store_tabletYtilt(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);
	int y;

	if (kstrtoint(buf, 10, &y)) {
		size_t len = buf[count - 1] == '\n' ? count - 1 : count;

		if (strncmp(buf, "disable", len))
			return -EINVAL;

		aiptek->newSetting.yTilt = AIPTEK_TILT_DISABLE;
	} else {
		if (y < AIPTEK_TILT_MIN || y > AIPTEK_TILT_MAX)
			return -EINVAL;

		aiptek->newSetting.yTilt = y;
	}

	return count;
}

static DEVICE_ATTR(ytilt,
		   S_IRUGO | S_IWUSR, show_tabletYtilt, store_tabletYtilt);

static ssize_t show_tabletJitterDelay(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", aiptek->curSetting.jitterDelay);
}

static ssize_t
store_tabletJitterDelay(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);
	int err, j;

	err = kstrtoint(buf, 10, &j);
	if (err)
		return err;

	aiptek->newSetting.jitterDelay = j;
	return count;
}

static DEVICE_ATTR(jitter,
		   S_IRUGO | S_IWUSR,
		   show_tabletJitterDelay, store_tabletJitterDelay);

static ssize_t show_tabletProgrammableDelay(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			aiptek->curSetting.programmableDelay);
}

static ssize_t
store_tabletProgrammableDelay(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);
	int err, d;

	err = kstrtoint(buf, 10, &d);
	if (err)
		return err;

	aiptek->newSetting.programmableDelay = d;
	return count;
}

static DEVICE_ATTR(delay,
		   S_IRUGO | S_IWUSR,
		   show_tabletProgrammableDelay, store_tabletProgrammableDelay);

static ssize_t show_tabletEventsReceived(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%ld\n", aiptek->eventCount);
}

static DEVICE_ATTR(event_count, S_IRUGO, show_tabletEventsReceived, NULL);

static ssize_t show_tabletDiagnosticMessage(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);
	char *retMsg;

	switch (aiptek->diagnostic) {
	case AIPTEK_DIAGNOSTIC_NA:
		retMsg = "no errors\n";
		break;

	case AIPTEK_DIAGNOSTIC_SENDING_RELATIVE_IN_ABSOLUTE:
		retMsg = "Error: receiving relative reports\n";
		break;

	case AIPTEK_DIAGNOSTIC_SENDING_ABSOLUTE_IN_RELATIVE:
		retMsg = "Error: receiving absolute reports\n";
		break;

	case AIPTEK_DIAGNOSTIC_TOOL_DISALLOWED:
		if (aiptek->curSetting.pointerMode ==
		    AIPTEK_POINTER_ONLY_MOUSE_MODE) {
			retMsg = "Error: receiving stylus reports\n";
		} else {
			retMsg = "Error: receiving mouse reports\n";
		}
		break;

	default:
		return 0;
	}
	return snprintf(buf, PAGE_SIZE, retMsg);
}

static DEVICE_ATTR(diagnostic, S_IRUGO, show_tabletDiagnosticMessage, NULL);


static struct aiptek_map stylus_button_map[] = {
	{ "upper",	AIPTEK_STYLUS_UPPER_BUTTON },
	{ "lower",	AIPTEK_STYLUS_LOWER_BUTTON },
	{ NULL,		AIPTEK_INVALID_VALUE }
};

static ssize_t show_tabletStylusUpper(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			map_val_to_str(stylus_button_map,
					aiptek->curSetting.stylusButtonUpper));
}

static ssize_t
store_tabletStylusUpper(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);
	int new_button = map_str_to_val(stylus_button_map, buf, count);

	if (new_button == AIPTEK_INVALID_VALUE)
		return -EINVAL;

	aiptek->newSetting.stylusButtonUpper = new_button;
	return count;
}

static DEVICE_ATTR(stylus_upper,
		   S_IRUGO | S_IWUSR,
		   show_tabletStylusUpper, store_tabletStylusUpper);


static ssize_t show_tabletStylusLower(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			map_val_to_str(stylus_button_map,
					aiptek->curSetting.stylusButtonLower));
}

static ssize_t
store_tabletStylusLower(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);
	int new_button = map_str_to_val(stylus_button_map, buf, count);

	if (new_button == AIPTEK_INVALID_VALUE)
		return -EINVAL;

	aiptek->newSetting.stylusButtonLower = new_button;
	return count;
}

static DEVICE_ATTR(stylus_lower,
		   S_IRUGO | S_IWUSR,
		   show_tabletStylusLower, store_tabletStylusLower);


static struct aiptek_map mouse_button_map[] = {
	{ "left",	AIPTEK_MOUSE_LEFT_BUTTON },
	{ "middle",	AIPTEK_MOUSE_MIDDLE_BUTTON },
	{ "right",	AIPTEK_MOUSE_RIGHT_BUTTON },
	{ NULL,		AIPTEK_INVALID_VALUE }
};

static ssize_t show_tabletMouseLeft(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			map_val_to_str(mouse_button_map,
					aiptek->curSetting.mouseButtonLeft));
}

static ssize_t
store_tabletMouseLeft(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);
	int new_button = map_str_to_val(mouse_button_map, buf, count);

	if (new_button == AIPTEK_INVALID_VALUE)
		return -EINVAL;

	aiptek->newSetting.mouseButtonLeft = new_button;
	return count;
}

static DEVICE_ATTR(mouse_left,
		   S_IRUGO | S_IWUSR,
		   show_tabletMouseLeft, store_tabletMouseLeft);

static ssize_t show_tabletMouseMiddle(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			map_val_to_str(mouse_button_map,
					aiptek->curSetting.mouseButtonMiddle));
}

static ssize_t
store_tabletMouseMiddle(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);
	int new_button = map_str_to_val(mouse_button_map, buf, count);

	if (new_button == AIPTEK_INVALID_VALUE)
		return -EINVAL;

	aiptek->newSetting.mouseButtonMiddle = new_button;
	return count;
}

static DEVICE_ATTR(mouse_middle,
		   S_IRUGO | S_IWUSR,
		   show_tabletMouseMiddle, store_tabletMouseMiddle);

static ssize_t show_tabletMouseRight(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			map_val_to_str(mouse_button_map,
					aiptek->curSetting.mouseButtonRight));
}

static ssize_t
store_tabletMouseRight(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);
	int new_button = map_str_to_val(mouse_button_map, buf, count);

	if (new_button == AIPTEK_INVALID_VALUE)
		return -EINVAL;

	aiptek->newSetting.mouseButtonRight = new_button;
	return count;
}

static DEVICE_ATTR(mouse_right,
		   S_IRUGO | S_IWUSR,
		   show_tabletMouseRight, store_tabletMouseRight);

static ssize_t show_tabletWheel(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	if (aiptek->curSetting.wheel == AIPTEK_WHEEL_DISABLE) {
		return snprintf(buf, PAGE_SIZE, "disable\n");
	} else {
		return snprintf(buf, PAGE_SIZE, "%d\n",
				aiptek->curSetting.wheel);
	}
}

static ssize_t
store_tabletWheel(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);
	int err, w;

	err = kstrtoint(buf, 10, &w);
	if (err)
		return err;

	aiptek->newSetting.wheel = w;
	return count;
}

static DEVICE_ATTR(wheel,
		   S_IRUGO | S_IWUSR, show_tabletWheel, store_tabletWheel);

static ssize_t show_tabletExecute(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			"Write anything to this file to program your tablet.\n");
}

static ssize_t
store_tabletExecute(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	memcpy(&aiptek->curSetting, &aiptek->newSetting,
	       sizeof(struct aiptek_settings));

	if (aiptek_program_tablet(aiptek) < 0)
		return -EIO;

	return count;
}

static DEVICE_ATTR(execute,
		   S_IRUGO | S_IWUSR, show_tabletExecute, store_tabletExecute);

static ssize_t show_tabletODMCode(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%04x\n", aiptek->features.odmCode);
}

static DEVICE_ATTR(odm_code, S_IRUGO, show_tabletODMCode, NULL);

static ssize_t show_tabletModelCode(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%04x\n", aiptek->features.modelCode);
}

static DEVICE_ATTR(model_code, S_IRUGO, show_tabletModelCode, NULL);

static ssize_t show_firmwareCode(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aiptek *aiptek = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%04x\n",
			aiptek->features.firmwareCode);
}

static DEVICE_ATTR(firmware_code, S_IRUGO, show_firmwareCode, NULL);

static struct attribute *aiptek_attributes[] = {
	&dev_attr_size.attr,
	&dev_attr_pointer_mode.attr,
	&dev_attr_coordinate_mode.attr,
	&dev_attr_tool_mode.attr,
	&dev_attr_xtilt.attr,
	&dev_attr_ytilt.attr,
	&dev_attr_jitter.attr,
	&dev_attr_delay.attr,
	&dev_attr_event_count.attr,
	&dev_attr_diagnostic.attr,
	&dev_attr_odm_code.attr,
	&dev_attr_model_code.attr,
	&dev_attr_firmware_code.attr,
	&dev_attr_stylus_lower.attr,
	&dev_attr_stylus_upper.attr,
	&dev_attr_mouse_left.attr,
	&dev_attr_mouse_middle.attr,
	&dev_attr_mouse_right.attr,
	&dev_attr_wheel.attr,
	&dev_attr_execute.attr,
	NULL
};

static struct attribute_group aiptek_attribute_group = {
	.attrs	= aiptek_attributes,
};

static int
aiptek_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *usbdev = interface_to_usbdev(intf);
	struct usb_endpoint_descriptor *endpoint;
	struct aiptek *aiptek;
	struct input_dev *inputdev;
	int i;
	int speeds[] = { 0,
		AIPTEK_PROGRAMMABLE_DELAY_50,
		AIPTEK_PROGRAMMABLE_DELAY_400,
		AIPTEK_PROGRAMMABLE_DELAY_25,
		AIPTEK_PROGRAMMABLE_DELAY_100,
		AIPTEK_PROGRAMMABLE_DELAY_200,
		AIPTEK_PROGRAMMABLE_DELAY_300
	};
	int err = -ENOMEM;

	speeds[0] = programmableDelay;

	aiptek = kzalloc(sizeof(struct aiptek), GFP_KERNEL);
	inputdev = input_allocate_device();
	if (!aiptek || !inputdev) {
		dev_warn(&intf->dev,
			 "cannot allocate memory or input device\n");
		goto fail1;
        }

	aiptek->data = usb_alloc_coherent(usbdev, AIPTEK_PACKET_LENGTH,
					  GFP_ATOMIC, &aiptek->data_dma);
        if (!aiptek->data) {
		dev_warn(&intf->dev, "cannot allocate usb buffer\n");
		goto fail1;
	}

	aiptek->urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!aiptek->urb) {
	        dev_warn(&intf->dev, "cannot allocate urb\n");
		goto fail2;
	}

	aiptek->inputdev = inputdev;
	aiptek->usbdev = usbdev;
	aiptek->ifnum = intf->altsetting[0].desc.bInterfaceNumber;
	aiptek->inDelay = 0;
	aiptek->endDelay = 0;
	aiptek->previousJitterable = 0;
	aiptek->lastMacro = -1;

	aiptek->curSetting.pointerMode = AIPTEK_POINTER_EITHER_MODE;
	aiptek->curSetting.coordinateMode = AIPTEK_COORDINATE_ABSOLUTE_MODE;
	aiptek->curSetting.toolMode = AIPTEK_TOOL_BUTTON_PEN_MODE;
	aiptek->curSetting.xTilt = AIPTEK_TILT_DISABLE;
	aiptek->curSetting.yTilt = AIPTEK_TILT_DISABLE;
	aiptek->curSetting.mouseButtonLeft = AIPTEK_MOUSE_LEFT_BUTTON;
	aiptek->curSetting.mouseButtonMiddle = AIPTEK_MOUSE_MIDDLE_BUTTON;
	aiptek->curSetting.mouseButtonRight = AIPTEK_MOUSE_RIGHT_BUTTON;
	aiptek->curSetting.stylusButtonUpper = AIPTEK_STYLUS_UPPER_BUTTON;
	aiptek->curSetting.stylusButtonLower = AIPTEK_STYLUS_LOWER_BUTTON;
	aiptek->curSetting.jitterDelay = jitterDelay;
	aiptek->curSetting.programmableDelay = programmableDelay;

	aiptek->newSetting = aiptek->curSetting;

	usb_make_path(usbdev, aiptek->features.usbPath,
			sizeof(aiptek->features.usbPath));
	strlcat(aiptek->features.usbPath, "/input0",
		sizeof(aiptek->features.usbPath));

	inputdev->name = "Aiptek";
	inputdev->phys = aiptek->features.usbPath;
	usb_to_input_id(usbdev, &inputdev->id);
	inputdev->dev.parent = &intf->dev;

	input_set_drvdata(inputdev, aiptek);

	inputdev->open = aiptek_open;
	inputdev->close = aiptek_close;

	for (i = 0; i < ARRAY_SIZE(eventTypes); ++i)
	        __set_bit(eventTypes[i], inputdev->evbit);

	for (i = 0; i < ARRAY_SIZE(absEvents); ++i)
	        __set_bit(absEvents[i], inputdev->absbit);

	for (i = 0; i < ARRAY_SIZE(relEvents); ++i)
	        __set_bit(relEvents[i], inputdev->relbit);

	__set_bit(MSC_SERIAL, inputdev->mscbit);

	
	for (i = 0; i < ARRAY_SIZE(buttonEvents); ++i)
		__set_bit(buttonEvents[i], inputdev->keybit);

	for (i = 0; i < ARRAY_SIZE(macroKeyEvents); ++i)
		__set_bit(macroKeyEvents[i], inputdev->keybit);

	input_set_abs_params(inputdev, ABS_X, 0, 2999, 0, 0);
	input_set_abs_params(inputdev, ABS_Y, 0, 2249, 0, 0);
	input_set_abs_params(inputdev, ABS_PRESSURE, 0, 511, 0, 0);
	input_set_abs_params(inputdev, ABS_TILT_X, AIPTEK_TILT_MIN, AIPTEK_TILT_MAX, 0, 0);
	input_set_abs_params(inputdev, ABS_TILT_Y, AIPTEK_TILT_MIN, AIPTEK_TILT_MAX, 0, 0);
	input_set_abs_params(inputdev, ABS_WHEEL, AIPTEK_WHEEL_MIN, AIPTEK_WHEEL_MAX - 1, 0, 0);

	endpoint = &intf->altsetting[0].endpoint[0].desc;

	usb_fill_int_urb(aiptek->urb,
			 aiptek->usbdev,
			 usb_rcvintpipe(aiptek->usbdev,
					endpoint->bEndpointAddress),
			 aiptek->data, 8, aiptek_irq, aiptek,
			 endpoint->bInterval);

	aiptek->urb->transfer_dma = aiptek->data_dma;
	aiptek->urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;


	for (i = 0; i < ARRAY_SIZE(speeds); ++i) {
		aiptek->curSetting.programmableDelay = speeds[i];
		(void)aiptek_program_tablet(aiptek);
		if (input_abs_get_max(aiptek->inputdev, ABS_X) > 0) {
			dev_info(&intf->dev,
				 "Aiptek using %d ms programming speed\n",
				 aiptek->curSetting.programmableDelay);
			break;
		}
	}

	if (i == ARRAY_SIZE(speeds)) {
		dev_info(&intf->dev,
			 "Aiptek tried all speeds, no sane response\n");
		goto fail2;
	}

	usb_set_intfdata(intf, aiptek);

	err = sysfs_create_group(&intf->dev.kobj, &aiptek_attribute_group);
	if (err) {
		dev_warn(&intf->dev, "cannot create sysfs group err: %d\n",
			 err);
		goto fail3;
        }

	err = input_register_device(aiptek->inputdev);
	if (err) {
		dev_warn(&intf->dev,
			 "input_register_device returned err: %d\n", err);
		goto fail4;
        }
	return 0;

 fail4:	sysfs_remove_group(&intf->dev.kobj, &aiptek_attribute_group);
 fail3: usb_free_urb(aiptek->urb);
 fail2:	usb_free_coherent(usbdev, AIPTEK_PACKET_LENGTH, aiptek->data,
			  aiptek->data_dma);
 fail1: usb_set_intfdata(intf, NULL);
	input_free_device(inputdev);
	kfree(aiptek);
	return err;
}

static void aiptek_disconnect(struct usb_interface *intf)
{
	struct aiptek *aiptek = usb_get_intfdata(intf);

	usb_set_intfdata(intf, NULL);
	if (aiptek != NULL) {
		usb_kill_urb(aiptek->urb);
		input_unregister_device(aiptek->inputdev);
		sysfs_remove_group(&intf->dev.kobj, &aiptek_attribute_group);
		usb_free_urb(aiptek->urb);
		usb_free_coherent(interface_to_usbdev(intf),
				  AIPTEK_PACKET_LENGTH,
				  aiptek->data, aiptek->data_dma);
		kfree(aiptek);
	}
}

static struct usb_driver aiptek_driver = {
	.name = "aiptek",
	.probe = aiptek_probe,
	.disconnect = aiptek_disconnect,
	.id_table = aiptek_ids,
};

module_usb_driver(aiptek_driver);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

module_param(programmableDelay, int, 0);
MODULE_PARM_DESC(programmableDelay, "delay used during tablet programming");
module_param(jitterDelay, int, 0);
MODULE_PARM_DESC(jitterDelay, "stylus/mouse settlement delay");
