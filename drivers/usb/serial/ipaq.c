/*
 * USB Compaq iPAQ driver
 *
 *	Copyright (C) 2001 - 2002
 *	    Ganesh Varadarajan <ganesh@veritas.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>

#define KP_RETRIES	100


#define DRIVER_VERSION "v1.0"
#define DRIVER_AUTHOR "Ganesh Varadarajan <ganesh@veritas.com>"
#define DRIVER_DESC "USB PocketPC PDA driver"

static __u16 product, vendor;
static bool debug;
static int connect_retries = KP_RETRIES;
static int initial_wait;

static int  ipaq_open(struct tty_struct *tty,
			struct usb_serial_port *port);
static int  ipaq_calc_num_ports(struct usb_serial *serial);
static int  ipaq_startup(struct usb_serial *serial);

static struct usb_device_id ipaq_id_table [] = {
	
	{ USB_DEVICE(0x049F, 0x0003) },
	{ USB_DEVICE(0x0104, 0x00BE) }, 
	{ USB_DEVICE(0x03F0, 0x1016) }, 
	{ USB_DEVICE(0x03F0, 0x1116) }, 
	{ USB_DEVICE(0x03F0, 0x1216) }, 
	{ USB_DEVICE(0x03F0, 0x2016) }, 
	{ USB_DEVICE(0x03F0, 0x2116) }, 
	{ USB_DEVICE(0x03F0, 0x2216) }, 
	{ USB_DEVICE(0x03F0, 0x3016) }, 
	{ USB_DEVICE(0x03F0, 0x3116) }, 
	{ USB_DEVICE(0x03F0, 0x3216) }, 
	{ USB_DEVICE(0x03F0, 0x4016) }, 
	{ USB_DEVICE(0x03F0, 0x4116) }, 
	{ USB_DEVICE(0x03F0, 0x4216) }, 
	{ USB_DEVICE(0x03F0, 0x5016) }, 
	{ USB_DEVICE(0x03F0, 0x5116) }, 
	{ USB_DEVICE(0x03F0, 0x5216) }, 
	{ USB_DEVICE(0x0409, 0x00D5) }, 
	{ USB_DEVICE(0x0409, 0x00D6) }, 
	{ USB_DEVICE(0x0409, 0x00D7) }, 
	{ USB_DEVICE(0x0409, 0x8024) }, 
	{ USB_DEVICE(0x0409, 0x8025) }, 
	{ USB_DEVICE(0x043E, 0x9C01) }, 
	{ USB_DEVICE(0x045E, 0x00CE) }, 
	{ USB_DEVICE(0x045E, 0x0400) }, 
	{ USB_DEVICE(0x045E, 0x0401) }, 
	{ USB_DEVICE(0x045E, 0x0402) }, 
	{ USB_DEVICE(0x045E, 0x0403) }, 
	{ USB_DEVICE(0x045E, 0x0404) }, 
	{ USB_DEVICE(0x045E, 0x0405) }, 
	{ USB_DEVICE(0x045E, 0x0406) }, 
	{ USB_DEVICE(0x045E, 0x0407) }, 
	{ USB_DEVICE(0x045E, 0x0408) }, 
	{ USB_DEVICE(0x045E, 0x0409) }, 
	{ USB_DEVICE(0x045E, 0x040A) }, 
	{ USB_DEVICE(0x045E, 0x040B) }, 
	{ USB_DEVICE(0x045E, 0x040C) }, 
	{ USB_DEVICE(0x045E, 0x040D) }, 
	{ USB_DEVICE(0x045E, 0x040E) }, 
	{ USB_DEVICE(0x045E, 0x040F) }, 
	{ USB_DEVICE(0x045E, 0x0410) }, 
	{ USB_DEVICE(0x045E, 0x0411) }, 
	{ USB_DEVICE(0x045E, 0x0412) }, 
	{ USB_DEVICE(0x045E, 0x0413) }, 
	{ USB_DEVICE(0x045E, 0x0414) }, 
	{ USB_DEVICE(0x045E, 0x0415) }, 
	{ USB_DEVICE(0x045E, 0x0416) }, 
	{ USB_DEVICE(0x045E, 0x0417) }, 
	{ USB_DEVICE(0x045E, 0x0432) }, 
	{ USB_DEVICE(0x045E, 0x0433) }, 
	{ USB_DEVICE(0x045E, 0x0434) }, 
	{ USB_DEVICE(0x045E, 0x0435) }, 
	{ USB_DEVICE(0x045E, 0x0436) }, 
	{ USB_DEVICE(0x045E, 0x0437) }, 
	{ USB_DEVICE(0x045E, 0x0438) }, 
	{ USB_DEVICE(0x045E, 0x0439) }, 
	{ USB_DEVICE(0x045E, 0x043A) }, 
	{ USB_DEVICE(0x045E, 0x043B) }, 
	{ USB_DEVICE(0x045E, 0x043C) }, 
	{ USB_DEVICE(0x045E, 0x043D) }, 
	{ USB_DEVICE(0x045E, 0x043E) }, 
	{ USB_DEVICE(0x045E, 0x043F) }, 
	{ USB_DEVICE(0x045E, 0x0440) }, 
	{ USB_DEVICE(0x045E, 0x0441) }, 
	{ USB_DEVICE(0x045E, 0x0442) }, 
	{ USB_DEVICE(0x045E, 0x0443) }, 
	{ USB_DEVICE(0x045E, 0x0444) }, 
	{ USB_DEVICE(0x045E, 0x0445) }, 
	{ USB_DEVICE(0x045E, 0x0446) }, 
	{ USB_DEVICE(0x045E, 0x0447) }, 
	{ USB_DEVICE(0x045E, 0x0448) }, 
	{ USB_DEVICE(0x045E, 0x0449) }, 
	{ USB_DEVICE(0x045E, 0x044A) }, 
	{ USB_DEVICE(0x045E, 0x044B) }, 
	{ USB_DEVICE(0x045E, 0x044C) }, 
	{ USB_DEVICE(0x045E, 0x044D) }, 
	{ USB_DEVICE(0x045E, 0x044E) }, 
	{ USB_DEVICE(0x045E, 0x044F) }, 
	{ USB_DEVICE(0x045E, 0x0450) }, 
	{ USB_DEVICE(0x045E, 0x0451) }, 
	{ USB_DEVICE(0x045E, 0x0452) }, 
	{ USB_DEVICE(0x045E, 0x0453) }, 
	{ USB_DEVICE(0x045E, 0x0454) }, 
	{ USB_DEVICE(0x045E, 0x0455) }, 
	{ USB_DEVICE(0x045E, 0x0456) }, 
	{ USB_DEVICE(0x045E, 0x0457) }, 
	{ USB_DEVICE(0x045E, 0x0458) }, 
	{ USB_DEVICE(0x045E, 0x0459) }, 
	{ USB_DEVICE(0x045E, 0x045A) }, 
	{ USB_DEVICE(0x045E, 0x045B) }, 
	{ USB_DEVICE(0x045E, 0x045C) }, 
	{ USB_DEVICE(0x045E, 0x045D) }, 
	{ USB_DEVICE(0x045E, 0x045E) }, 
	{ USB_DEVICE(0x045E, 0x045F) }, 
	{ USB_DEVICE(0x045E, 0x0460) }, 
	{ USB_DEVICE(0x045E, 0x0461) }, 
	{ USB_DEVICE(0x045E, 0x0462) }, 
	{ USB_DEVICE(0x045E, 0x0463) }, 
	{ USB_DEVICE(0x045E, 0x0464) }, 
	{ USB_DEVICE(0x045E, 0x0465) }, 
	{ USB_DEVICE(0x045E, 0x0466) }, 
	{ USB_DEVICE(0x045E, 0x0467) }, 
	{ USB_DEVICE(0x045E, 0x0468) }, 
	{ USB_DEVICE(0x045E, 0x0469) }, 
	{ USB_DEVICE(0x045E, 0x046A) }, 
	{ USB_DEVICE(0x045E, 0x046B) }, 
	{ USB_DEVICE(0x045E, 0x046C) }, 
	{ USB_DEVICE(0x045E, 0x046D) }, 
	{ USB_DEVICE(0x045E, 0x046E) }, 
	{ USB_DEVICE(0x045E, 0x046F) }, 
	{ USB_DEVICE(0x045E, 0x0470) }, 
	{ USB_DEVICE(0x045E, 0x0471) }, 
	{ USB_DEVICE(0x045E, 0x0472) }, 
	{ USB_DEVICE(0x045E, 0x0473) }, 
	{ USB_DEVICE(0x045E, 0x0474) }, 
	{ USB_DEVICE(0x045E, 0x0475) }, 
	{ USB_DEVICE(0x045E, 0x0476) }, 
	{ USB_DEVICE(0x045E, 0x0477) }, 
	{ USB_DEVICE(0x045E, 0x0478) }, 
	{ USB_DEVICE(0x045E, 0x0479) }, 
	{ USB_DEVICE(0x045E, 0x047A) }, 
	{ USB_DEVICE(0x045E, 0x047B) }, 
	{ USB_DEVICE(0x045E, 0x04C8) }, 
	{ USB_DEVICE(0x045E, 0x04C9) }, 
	{ USB_DEVICE(0x045E, 0x04CA) }, 
	{ USB_DEVICE(0x045E, 0x04CB) }, 
	{ USB_DEVICE(0x045E, 0x04CC) }, 
	{ USB_DEVICE(0x045E, 0x04CD) }, 
	{ USB_DEVICE(0x045E, 0x04CE) }, 
	{ USB_DEVICE(0x045E, 0x04D7) }, 
	{ USB_DEVICE(0x045E, 0x04D8) }, 
	{ USB_DEVICE(0x045E, 0x04D9) }, 
	{ USB_DEVICE(0x045E, 0x04DA) }, 
	{ USB_DEVICE(0x045E, 0x04DB) }, 
	{ USB_DEVICE(0x045E, 0x04DC) }, 
	{ USB_DEVICE(0x045E, 0x04DD) }, 
	{ USB_DEVICE(0x045E, 0x04DE) }, 
	{ USB_DEVICE(0x045E, 0x04DF) }, 
	{ USB_DEVICE(0x045E, 0x04E0) }, 
	{ USB_DEVICE(0x045E, 0x04E1) }, 
	{ USB_DEVICE(0x045E, 0x04E2) }, 
	{ USB_DEVICE(0x045E, 0x04E3) }, 
	{ USB_DEVICE(0x045E, 0x04E4) }, 
	{ USB_DEVICE(0x045E, 0x04E5) }, 
	{ USB_DEVICE(0x045E, 0x04E6) }, 
	{ USB_DEVICE(0x045E, 0x04E7) }, 
	{ USB_DEVICE(0x045E, 0x04E8) }, 
	{ USB_DEVICE(0x045E, 0x04E9) }, 
	{ USB_DEVICE(0x045E, 0x04EA) }, 
	{ USB_DEVICE(0x049F, 0x0003) }, 
	{ USB_DEVICE(0x049F, 0x0032) }, 
	{ USB_DEVICE(0x04A4, 0x0014) }, 
	{ USB_DEVICE(0x04AD, 0x0301) }, 
	{ USB_DEVICE(0x04AD, 0x0302) }, 
	{ USB_DEVICE(0x04AD, 0x0303) }, 
	{ USB_DEVICE(0x04AD, 0x0306) }, 
	{ USB_DEVICE(0x04B7, 0x0531) }, 
	{ USB_DEVICE(0x04C5, 0x1058) }, 
	{ USB_DEVICE(0x04C5, 0x1079) }, 
	{ USB_DEVICE(0x04DA, 0x2500) }, 
	{ USB_DEVICE(0x04DD, 0x9102) }, 
	{ USB_DEVICE(0x04DD, 0x9121) }, 
	{ USB_DEVICE(0x04DD, 0x9123) }, 
	{ USB_DEVICE(0x04DD, 0x9151) }, 
	{ USB_DEVICE(0x04DD, 0x91AC) }, 
	{ USB_DEVICE(0x04E8, 0x5F00) }, 
	{ USB_DEVICE(0x04E8, 0x5F01) }, 
	{ USB_DEVICE(0x04E8, 0x5F02) }, 
	{ USB_DEVICE(0x04E8, 0x5F03) }, 
	{ USB_DEVICE(0x04E8, 0x5F04) }, 
	{ USB_DEVICE(0x04E8, 0x6611) }, 
	{ USB_DEVICE(0x04E8, 0x6613) }, 
	{ USB_DEVICE(0x04E8, 0x6615) }, 
	{ USB_DEVICE(0x04E8, 0x6617) }, 
	{ USB_DEVICE(0x04E8, 0x6619) }, 
	{ USB_DEVICE(0x04E8, 0x661B) }, 
	{ USB_DEVICE(0x04E8, 0x662E) }, 
	{ USB_DEVICE(0x04E8, 0x6630) }, 
	{ USB_DEVICE(0x04E8, 0x6632) }, 
	{ USB_DEVICE(0x04f1, 0x3011) }, 
	{ USB_DEVICE(0x04F1, 0x3012) }, 
	{ USB_DEVICE(0x0502, 0x1631) }, 
	{ USB_DEVICE(0x0502, 0x1632) }, 
	{ USB_DEVICE(0x0502, 0x16E1) }, 
	{ USB_DEVICE(0x0502, 0x16E2) }, 
	{ USB_DEVICE(0x0502, 0x16E3) }, 
	{ USB_DEVICE(0x0536, 0x01A0) }, 
	{ USB_DEVICE(0x0543, 0x0ED9) }, 
	{ USB_DEVICE(0x0543, 0x1527) }, 
	{ USB_DEVICE(0x0543, 0x1529) }, 
	{ USB_DEVICE(0x0543, 0x152B) }, 
	{ USB_DEVICE(0x0543, 0x152E) }, 
	{ USB_DEVICE(0x0543, 0x1921) }, 
	{ USB_DEVICE(0x0543, 0x1922) }, 
	{ USB_DEVICE(0x0543, 0x1923) }, 
	{ USB_DEVICE(0x05E0, 0x2000) }, 
	{ USB_DEVICE(0x05E0, 0x2001) }, 
	{ USB_DEVICE(0x05E0, 0x2002) }, 
	{ USB_DEVICE(0x05E0, 0x2003) }, 
	{ USB_DEVICE(0x05E0, 0x2004) }, 
	{ USB_DEVICE(0x05E0, 0x2005) }, 
	{ USB_DEVICE(0x05E0, 0x2006) }, 
	{ USB_DEVICE(0x05E0, 0x2007) }, 
	{ USB_DEVICE(0x05E0, 0x2008) }, 
	{ USB_DEVICE(0x05E0, 0x2009) }, 
	{ USB_DEVICE(0x05E0, 0x200A) }, 
	{ USB_DEVICE(0x067E, 0x1001) }, 
	{ USB_DEVICE(0x07CF, 0x2001) }, 
	{ USB_DEVICE(0x07CF, 0x2002) }, 
	{ USB_DEVICE(0x07CF, 0x2003) }, 
	{ USB_DEVICE(0x0930, 0x0700) }, 
	{ USB_DEVICE(0x0930, 0x0705) }, 
	{ USB_DEVICE(0x0930, 0x0706) }, 
	{ USB_DEVICE(0x0930, 0x0707) }, 
	{ USB_DEVICE(0x0930, 0x0708) }, 
	{ USB_DEVICE(0x0930, 0x0709) }, 
	{ USB_DEVICE(0x0930, 0x070A) }, 
	{ USB_DEVICE(0x0930, 0x070B) }, 
	{ USB_DEVICE(0x094B, 0x0001) }, 
	{ USB_DEVICE(0x0960, 0x0065) }, 
	{ USB_DEVICE(0x0960, 0x0066) }, 
	{ USB_DEVICE(0x0960, 0x0067) }, 
	{ USB_DEVICE(0x0961, 0x0010) }, 
	{ USB_DEVICE(0x099E, 0x0052) }, 
	{ USB_DEVICE(0x099E, 0x4000) }, 
	{ USB_DEVICE(0x0B05, 0x4200) }, 
	{ USB_DEVICE(0x0B05, 0x4201) }, 
	{ USB_DEVICE(0x0B05, 0x4202) }, 
	{ USB_DEVICE(0x0B05, 0x420F) }, 
	{ USB_DEVICE(0x0B05, 0x9200) }, 
	{ USB_DEVICE(0x0B05, 0x9202) }, 
	{ USB_DEVICE(0x0BB4, 0x00CE) }, 
	{ USB_DEVICE(0x0BB4, 0x00CF) }, 
	{ USB_DEVICE(0x0BB4, 0x0A01) }, 
	{ USB_DEVICE(0x0BB4, 0x0A02) }, 
	{ USB_DEVICE(0x0BB4, 0x0A03) }, 
	{ USB_DEVICE(0x0BB4, 0x0A04) }, 
	{ USB_DEVICE(0x0BB4, 0x0A05) }, 
	{ USB_DEVICE(0x0BB4, 0x0A06) }, 
	{ USB_DEVICE(0x0BB4, 0x0A07) }, 
	{ USB_DEVICE(0x0BB4, 0x0A08) }, 
	{ USB_DEVICE(0x0BB4, 0x0A09) }, 
	{ USB_DEVICE(0x0BB4, 0x0A0A) }, 
	{ USB_DEVICE(0x0BB4, 0x0A0B) }, 
	{ USB_DEVICE(0x0BB4, 0x0A0C) }, 
	{ USB_DEVICE(0x0BB4, 0x0A0D) }, 
	{ USB_DEVICE(0x0BB4, 0x0A0E) }, 
	{ USB_DEVICE(0x0BB4, 0x0A0F) }, 
	{ USB_DEVICE(0x0BB4, 0x0A10) }, 
	{ USB_DEVICE(0x0BB4, 0x0A11) }, 
	{ USB_DEVICE(0x0BB4, 0x0A12) }, 
	{ USB_DEVICE(0x0BB4, 0x0A13) }, 
	{ USB_DEVICE(0x0BB4, 0x0A14) }, 
	{ USB_DEVICE(0x0BB4, 0x0A15) }, 
	{ USB_DEVICE(0x0BB4, 0x0A16) }, 
	{ USB_DEVICE(0x0BB4, 0x0A17) }, 
	{ USB_DEVICE(0x0BB4, 0x0A18) }, 
	{ USB_DEVICE(0x0BB4, 0x0A19) }, 
	{ USB_DEVICE(0x0BB4, 0x0A1A) }, 
	{ USB_DEVICE(0x0BB4, 0x0A1B) }, 
	{ USB_DEVICE(0x0BB4, 0x0A1C) }, 
	{ USB_DEVICE(0x0BB4, 0x0A1D) }, 
	{ USB_DEVICE(0x0BB4, 0x0A1E) }, 
	{ USB_DEVICE(0x0BB4, 0x0A1F) }, 
	{ USB_DEVICE(0x0BB4, 0x0A20) }, 
	{ USB_DEVICE(0x0BB4, 0x0A21) }, 
	{ USB_DEVICE(0x0BB4, 0x0A22) }, 
	{ USB_DEVICE(0x0BB4, 0x0A23) }, 
	{ USB_DEVICE(0x0BB4, 0x0A24) }, 
	{ USB_DEVICE(0x0BB4, 0x0A25) }, 
	{ USB_DEVICE(0x0BB4, 0x0A26) }, 
	{ USB_DEVICE(0x0BB4, 0x0A27) }, 
	{ USB_DEVICE(0x0BB4, 0x0A28) }, 
	{ USB_DEVICE(0x0BB4, 0x0A29) }, 
	{ USB_DEVICE(0x0BB4, 0x0A2A) }, 
	{ USB_DEVICE(0x0BB4, 0x0A2B) }, 
	{ USB_DEVICE(0x0BB4, 0x0A2C) }, 
	{ USB_DEVICE(0x0BB4, 0x0A2D) }, 
	{ USB_DEVICE(0x0BB4, 0x0A2E) }, 
	{ USB_DEVICE(0x0BB4, 0x0A2F) }, 
	{ USB_DEVICE(0x0BB4, 0x0A30) }, 
	{ USB_DEVICE(0x0BB4, 0x0A31) }, 
	{ USB_DEVICE(0x0BB4, 0x0A32) }, 
	{ USB_DEVICE(0x0BB4, 0x0A33) }, 
	{ USB_DEVICE(0x0BB4, 0x0A34) }, 
	{ USB_DEVICE(0x0BB4, 0x0A35) }, 
	{ USB_DEVICE(0x0BB4, 0x0A36) }, 
	{ USB_DEVICE(0x0BB4, 0x0A37) }, 
	{ USB_DEVICE(0x0BB4, 0x0A38) }, 
	{ USB_DEVICE(0x0BB4, 0x0A39) }, 
	{ USB_DEVICE(0x0BB4, 0x0A3A) }, 
	{ USB_DEVICE(0x0BB4, 0x0A3B) }, 
	{ USB_DEVICE(0x0BB4, 0x0A3C) }, 
	{ USB_DEVICE(0x0BB4, 0x0A3D) }, 
	{ USB_DEVICE(0x0BB4, 0x0A3E) }, 
	{ USB_DEVICE(0x0BB4, 0x0A3F) }, 
	{ USB_DEVICE(0x0BB4, 0x0A40) }, 
	{ USB_DEVICE(0x0BB4, 0x0A41) }, 
	{ USB_DEVICE(0x0BB4, 0x0A42) }, 
	{ USB_DEVICE(0x0BB4, 0x0A43) }, 
	{ USB_DEVICE(0x0BB4, 0x0A44) }, 
	{ USB_DEVICE(0x0BB4, 0x0A45) }, 
	{ USB_DEVICE(0x0BB4, 0x0A46) }, 
	{ USB_DEVICE(0x0BB4, 0x0A47) }, 
	{ USB_DEVICE(0x0BB4, 0x0A48) }, 
	{ USB_DEVICE(0x0BB4, 0x0A49) }, 
	{ USB_DEVICE(0x0BB4, 0x0A4A) }, 
	{ USB_DEVICE(0x0BB4, 0x0A4B) }, 
	{ USB_DEVICE(0x0BB4, 0x0A4C) }, 
	{ USB_DEVICE(0x0BB4, 0x0A4D) }, 
	{ USB_DEVICE(0x0BB4, 0x0A4E) }, 
	{ USB_DEVICE(0x0BB4, 0x0A4F) }, 
	{ USB_DEVICE(0x0BB4, 0x0A50) }, 
	{ USB_DEVICE(0x0BB4, 0x0A51) }, 
	{ USB_DEVICE(0x0BB4, 0x0A52) }, 
	{ USB_DEVICE(0x0BB4, 0x0A53) }, 
	{ USB_DEVICE(0x0BB4, 0x0A54) }, 
	{ USB_DEVICE(0x0BB4, 0x0A55) }, 
	{ USB_DEVICE(0x0BB4, 0x0A56) }, 
	{ USB_DEVICE(0x0BB4, 0x0A57) }, 
	{ USB_DEVICE(0x0BB4, 0x0A58) }, 
	{ USB_DEVICE(0x0BB4, 0x0A59) }, 
	{ USB_DEVICE(0x0BB4, 0x0A5A) }, 
	{ USB_DEVICE(0x0BB4, 0x0A5B) }, 
	{ USB_DEVICE(0x0BB4, 0x0A5C) }, 
	{ USB_DEVICE(0x0BB4, 0x0A5D) }, 
	{ USB_DEVICE(0x0BB4, 0x0A5E) }, 
	{ USB_DEVICE(0x0BB4, 0x0A5F) }, 
	{ USB_DEVICE(0x0BB4, 0x0A60) }, 
	{ USB_DEVICE(0x0BB4, 0x0A61) }, 
	{ USB_DEVICE(0x0BB4, 0x0A62) }, 
	{ USB_DEVICE(0x0BB4, 0x0A63) }, 
	{ USB_DEVICE(0x0BB4, 0x0A64) }, 
	{ USB_DEVICE(0x0BB4, 0x0A65) }, 
	{ USB_DEVICE(0x0BB4, 0x0A66) }, 
	{ USB_DEVICE(0x0BB4, 0x0A67) }, 
	{ USB_DEVICE(0x0BB4, 0x0A68) }, 
	{ USB_DEVICE(0x0BB4, 0x0A69) }, 
	{ USB_DEVICE(0x0BB4, 0x0A6A) }, 
	{ USB_DEVICE(0x0BB4, 0x0A6B) }, 
	{ USB_DEVICE(0x0BB4, 0x0A6C) }, 
	{ USB_DEVICE(0x0BB4, 0x0A6D) }, 
	{ USB_DEVICE(0x0BB4, 0x0A6E) }, 
	{ USB_DEVICE(0x0BB4, 0x0A6F) }, 
	{ USB_DEVICE(0x0BB4, 0x0A70) }, 
	{ USB_DEVICE(0x0BB4, 0x0A71) }, 
	{ USB_DEVICE(0x0BB4, 0x0A72) }, 
	{ USB_DEVICE(0x0BB4, 0x0A73) }, 
	{ USB_DEVICE(0x0BB4, 0x0A74) }, 
	{ USB_DEVICE(0x0BB4, 0x0A75) }, 
	{ USB_DEVICE(0x0BB4, 0x0A76) }, 
	{ USB_DEVICE(0x0BB4, 0x0A77) }, 
	{ USB_DEVICE(0x0BB4, 0x0A78) }, 
	{ USB_DEVICE(0x0BB4, 0x0A79) }, 
	{ USB_DEVICE(0x0BB4, 0x0A7A) }, 
	{ USB_DEVICE(0x0BB4, 0x0A7B) }, 
	{ USB_DEVICE(0x0BB4, 0x0A7C) }, 
	{ USB_DEVICE(0x0BB4, 0x0A7D) }, 
	{ USB_DEVICE(0x0BB4, 0x0A7E) }, 
	{ USB_DEVICE(0x0BB4, 0x0A7F) }, 
	{ USB_DEVICE(0x0BB4, 0x0A80) }, 
	{ USB_DEVICE(0x0BB4, 0x0A81) }, 
	{ USB_DEVICE(0x0BB4, 0x0A82) }, 
	{ USB_DEVICE(0x0BB4, 0x0A83) }, 
	{ USB_DEVICE(0x0BB4, 0x0A84) }, 
	{ USB_DEVICE(0x0BB4, 0x0A85) }, 
	{ USB_DEVICE(0x0BB4, 0x0A86) }, 
	{ USB_DEVICE(0x0BB4, 0x0A87) }, 
	{ USB_DEVICE(0x0BB4, 0x0A88) }, 
	{ USB_DEVICE(0x0BB4, 0x0A89) }, 
	{ USB_DEVICE(0x0BB4, 0x0A8A) }, 
	{ USB_DEVICE(0x0BB4, 0x0A8B) }, 
	{ USB_DEVICE(0x0BB4, 0x0A8C) }, 
	{ USB_DEVICE(0x0BB4, 0x0A8D) }, 
	{ USB_DEVICE(0x0BB4, 0x0A8E) }, 
	{ USB_DEVICE(0x0BB4, 0x0A8F) }, 
	{ USB_DEVICE(0x0BB4, 0x0A90) }, 
	{ USB_DEVICE(0x0BB4, 0x0A91) }, 
	{ USB_DEVICE(0x0BB4, 0x0A92) }, 
	{ USB_DEVICE(0x0BB4, 0x0A93) }, 
	{ USB_DEVICE(0x0BB4, 0x0A94) }, 
	{ USB_DEVICE(0x0BB4, 0x0A95) }, 
	{ USB_DEVICE(0x0BB4, 0x0A96) }, 
	{ USB_DEVICE(0x0BB4, 0x0A97) }, 
	{ USB_DEVICE(0x0BB4, 0x0A98) }, 
	{ USB_DEVICE(0x0BB4, 0x0A99) }, 
	{ USB_DEVICE(0x0BB4, 0x0A9A) }, 
	{ USB_DEVICE(0x0BB4, 0x0A9B) }, 
	{ USB_DEVICE(0x0BB4, 0x0A9C) }, 
	{ USB_DEVICE(0x0BB4, 0x0A9D) }, 
	{ USB_DEVICE(0x0BB4, 0x0A9E) }, 
	{ USB_DEVICE(0x0BB4, 0x0A9F) }, 
	{ USB_DEVICE(0x0BB4, 0x0BCE) }, 
	{ USB_DEVICE(0x0BF8, 0x1001) }, 
	{ USB_DEVICE(0x0C44, 0x03A2) }, 
	{ USB_DEVICE(0x0C8E, 0x6000) }, 
	{ USB_DEVICE(0x0CAD, 0x9001) }, 
	{ USB_DEVICE(0x0F4E, 0x0200) }, 
	{ USB_DEVICE(0x0F98, 0x0201) }, 
	{ USB_DEVICE(0x0FB8, 0x3001) }, 
	{ USB_DEVICE(0x0FB8, 0x3002) }, 
	{ USB_DEVICE(0x0FB8, 0x3003) }, 
	{ USB_DEVICE(0x0FB8, 0x4001) }, 
	{ USB_DEVICE(0x1066, 0x00CE) }, 
	{ USB_DEVICE(0x1066, 0x0300) }, 
	{ USB_DEVICE(0x1066, 0x0500) }, 
	{ USB_DEVICE(0x1066, 0x0600) }, 
	{ USB_DEVICE(0x1066, 0x0700) }, 
	{ USB_DEVICE(0x1114, 0x0001) }, 
	{ USB_DEVICE(0x1114, 0x0004) }, 
	{ USB_DEVICE(0x1114, 0x0006) }, 
	{ USB_DEVICE(0x1182, 0x1388) }, 
	{ USB_DEVICE(0x11D9, 0x1002) }, 
	{ USB_DEVICE(0x11D9, 0x1003) }, 
	{ USB_DEVICE(0x1231, 0xCE01) }, 
	{ USB_DEVICE(0x1231, 0xCE02) }, 
	{ USB_DEVICE(0x1690, 0x0601) }, 
	{ USB_DEVICE(0x22B8, 0x4204) }, 
	{ USB_DEVICE(0x22B8, 0x4214) }, 
	{ USB_DEVICE(0x22B8, 0x4224) }, 
	{ USB_DEVICE(0x22B8, 0x4234) }, 
	{ USB_DEVICE(0x22B8, 0x4244) }, 
	{ USB_DEVICE(0x3340, 0x011C) }, 
	{ USB_DEVICE(0x3340, 0x0326) }, 
	{ USB_DEVICE(0x3340, 0x0426) }, 
	{ USB_DEVICE(0x3340, 0x043A) }, 
	{ USB_DEVICE(0x3340, 0x051C) }, 
	{ USB_DEVICE(0x3340, 0x053A) }, 
	{ USB_DEVICE(0x3340, 0x071C) }, 
	{ USB_DEVICE(0x3340, 0x0B1C) }, 
	{ USB_DEVICE(0x3340, 0x0E3A) }, 
	{ USB_DEVICE(0x3340, 0x0F1C) }, 
	{ USB_DEVICE(0x3340, 0x0F3A) }, 
	{ USB_DEVICE(0x3340, 0x1326) }, 
	{ USB_DEVICE(0x3340, 0x191C) }, 
	{ USB_DEVICE(0x3340, 0x2326) }, 
	{ USB_DEVICE(0x3340, 0x3326) }, 
	{ USB_DEVICE(0x3708, 0x20CE) }, 
	{ USB_DEVICE(0x3708, 0x21CE) }, 
	{ USB_DEVICE(0x4113, 0x0210) }, 
	{ USB_DEVICE(0x4113, 0x0211) }, 
	{ USB_DEVICE(0x4113, 0x0400) }, 
	{ USB_DEVICE(0x4113, 0x0410) }, 
	{ USB_DEVICE(0x413C, 0x4001) }, 
	{ USB_DEVICE(0x413C, 0x4002) }, 
	{ USB_DEVICE(0x413C, 0x4003) }, 
	{ USB_DEVICE(0x413C, 0x4004) }, 
	{ USB_DEVICE(0x413C, 0x4005) }, 
	{ USB_DEVICE(0x413C, 0x4006) }, 
	{ USB_DEVICE(0x413C, 0x4007) }, 
	{ USB_DEVICE(0x413C, 0x4008) }, 
	{ USB_DEVICE(0x413C, 0x4009) }, 
	{ USB_DEVICE(0x4505, 0x0010) }, 
	{ USB_DEVICE(0x5E04, 0xCE00) }, 
	{ }                             
};

MODULE_DEVICE_TABLE(usb, ipaq_id_table);

static struct usb_driver ipaq_driver = {
	.name =		"ipaq",
	.probe =	usb_serial_probe,
	.disconnect =	usb_serial_disconnect,
	.id_table =	ipaq_id_table,
};


static struct usb_serial_driver ipaq_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"ipaq",
	},
	.description =		"PocketPC PDA",
	.id_table =		ipaq_id_table,
	.bulk_in_size =		256,
	.bulk_out_size =	256,
	.open =			ipaq_open,
	.attach =		ipaq_startup,
	.calc_num_ports =	ipaq_calc_num_ports,
};

static struct usb_serial_driver * const serial_drivers[] = {
	&ipaq_device, NULL
};

static int ipaq_open(struct tty_struct *tty,
			struct usb_serial_port *port)
{
	struct usb_serial	*serial = port->serial;
	int			result = 0;
	int			retries = connect_retries;

	dbg("%s - port %d", __func__, port->number);

	msleep(1000*initial_wait);

	while (retries--) {
		result = usb_control_msg(serial->dev,
				usb_sndctrlpipe(serial->dev, 0), 0x22, 0x21,
				0x1, 0, NULL, 0, 100);
		if (!result)
			break;

		msleep(1000);
	}
	if (!retries && result) {
		dev_err(&port->dev, "%s - failed doing control urb, error %d\n",
							__func__, result);
		return result;
	}

	return usb_serial_generic_open(tty, port);
}

static int ipaq_calc_num_ports(struct usb_serial *serial)
{
	int ipaq_num_ports = 1;

	dbg("%s - numberofendpoints: %d", __FUNCTION__,
		(int)serial->interface->cur_altsetting->desc.bNumEndpoints);

	if (serial->interface->cur_altsetting->desc.bNumEndpoints > 3) {
		ipaq_num_ports = 2;
	}

	return ipaq_num_ports;
}


static int ipaq_startup(struct usb_serial *serial)
{
	dbg("%s", __func__);

	if (serial->num_bulk_in < serial->num_ports ||
			serial->num_bulk_out < serial->num_ports)
		return -ENODEV;

	if (serial->dev->actconfig->desc.bConfigurationValue != 1) {

		dev_err(&serial->dev->dev, "active config #%d != 1 ??\n",
			serial->dev->actconfig->desc.bConfigurationValue);
		return -ENODEV;
	}

	dbg("%s - iPAQ module configured for %d ports",
		__FUNCTION__, serial->num_ports);

	return usb_reset_configuration(serial->dev);
}

static int __init ipaq_init(void)
{
	int retval;

	if (vendor) {
		ipaq_id_table[0].idVendor = vendor;
		ipaq_id_table[0].idProduct = product;
	}

	retval = usb_serial_register_drivers(&ipaq_driver, serial_drivers);
	if (retval == 0)
		printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_VERSION ":"
			       DRIVER_DESC "\n");
	return retval;
}

static void __exit ipaq_exit(void)
{
	usb_serial_deregister_drivers(&ipaq_driver, serial_drivers);
}


module_init(ipaq_init);
module_exit(ipaq_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug enabled or not");

module_param(vendor, ushort, 0);
MODULE_PARM_DESC(vendor, "User specified USB idVendor");

module_param(product, ushort, 0);
MODULE_PARM_DESC(product, "User specified USB idProduct");

module_param(connect_retries, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(connect_retries,
		"Maximum number of connect retries (one second each)");

module_param(initial_wait, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(initial_wait,
		"Time to wait before attempting a connection (in seconds)");
