/*

SiI8558 / SiI8240 Linux Driver

Copyright (C) 2013 Silicon Image, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation version 2.
This program is distributed AS-IS WITHOUT ANY WARRANTY of any
kind, whether express or implied; INCLUDING without the implied warranty
of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.  See
the GNU General Public License for more details at http://www.gnu.org/licenses/gpl-2.0.html.

*/

#if !defined(MHL_LINUXDRV_IOCTL_H)
#define MHL_LINUXDRV_IOCTL_H

#include <linux/ioctl.h>


#ifdef __cplusplus
extern "C" {
#endif  


struct mhl_tx_event_packet {
	__u32	event;
	__u32	event_param;
};


struct mhl_tx_read_dev_cap {
	__u8	reg_num;
	__u8	reg_value;
};


#define ADOPTER_ID_SIZE					2
#define MAX_SCRATCH_PAD_TRANSFER_SIZE	16
#define SCRATCH_PAD_SIZE				64
struct mhl_tx_scratch_pad_access {
	__u8	offset;
	__u8 	length;
	__u8	data[MAX_SCRATCH_PAD_TRANSFER_SIZE];
};



#define	MHL_TX_EVENT_NONE			0x00

#define	MHL_TX_EVENT_DISCONNECTION	0x01

#define	MHL_TX_EVENT_CONNECTION		0x02

#define	MHL_TX_EVENT_RCP_RECEIVED	0x04

#define	MHL_TX_EVENT_RCPK_RECEIVED	0x05

#define	MHL_TX_EVENT_RCPE_RECEIVED	0x06

#define	MHL_TX_EVENT_UCP_RECEIVED	0x07

#define	MHL_TX_EVENT_UCPK_RECEIVED	0x08

#define	MHL_TX_EVENT_UCPE_RECEIVED	0x09

#define MHL_TX_EVENT_SPAD_RECEIVED	0x0A

#define	MHL_TX_EVENT_POW_BIT_CHG	0x0B

#define MHL_TX_EVENT_RAP_RECEIVED	0x0C


#define	MHL_RCPE_STATUS_NO_ERROR				0x00
#define	MHL_RCPE_STATUS_INEEFECTIVE_KEY_CODE	0x01
#define	MHL_RCPE_STATUS_BUSY					0x02

#define IOC_SII_MHL_TYPE ('S')


#define SII_MHL_GET_MHL_TX_EVENT \
    _IOR(IOC_SII_MHL_TYPE, 0x01, struct mhl_tx_event_packet *)


#define SII_MHL_RCP_SEND \
    _IOW(IOC_SII_MHL_TYPE, 0x02, __u8)


#define SII_MHL_RCP_SEND_ACK \
    _IOW(IOC_SII_MHL_TYPE, 0x03, __u8)


#define SII_MHL_RCP_SEND_ERR_ACK \
    _IOW(IOC_SII_MHL_TYPE, 0x04, __u8)


#define SII_MHL_GET_MHL_CONNECTION_STATUS \
    _IOR(IOC_SII_MHL_TYPE, 0x05, struct mhl_tx_event_packet *)


#define SII_MHL_GET_DEV_CAP_VALUE \
    _IOWR(IOC_SII_MHL_TYPE, 0x06, struct mhl_tx_read_dev_cap *)


/*
 * IOCTL to write the Scratch Pad registers of an attached MHL device.
 *
 * An application can use this IOCTL to write up to 16 data bytes within the
 * scratch pad register space of an attached MHL device.
 *
 * The parameter passed with this IOCTL is a pointer to an
 * mhl_tx_scratch_pad_access structure.  The offset field of this structure
 * must be filled in by the caller with the starting offset within the
 * Scratch Pad register address space of the 1st register to write.  The length
 * field specifies the total number of bytes passed in the data field.  The
 * data field contains the data bytes to be written.  Per the MHL specification,
 * the write data must be prepended with the Adopter ID of the receiving device.
 * The value passed in the length field must include the Adopter ID bytes.
 *
 * This IOCTL will return EINVAL if an MHL connection is not established at the
 * time of the call or if the offset or length fields contain out of range values.
 * The driver will return EAGAIN if it is currently in a state where it cannot
 * immediately satisfy the request.  If EAGAIN is returned the caller should wait
 * a short amount of time and retry the request.
 */
#define SII_MHL_WRITE_SCRATCH_PAD \
    _IOWR(IOC_SII_MHL_TYPE, 0x07, struct mhl_tx_scratch_pad_access *)


#define SII_MHL_READ_SCRATCH_PAD \
    _IOWR(IOC_SII_MHL_TYPE, 0x08, struct mhl_tx_scratch_pad_access *)


#define SII_MHL_UCP_SEND \
    _IOW(IOC_SII_MHL_TYPE, 0x09, __u8)


#define SII_MHL_UCP_SEND_ACK \
    _IOW(IOC_SII_MHL_TYPE, 0x0A, __u8)


#define SII_MHL_UCP_SEND_ERR_ACK \
    _IOW(IOC_SII_MHL_TYPE, 0x0B, __u8)


#ifdef __cplusplus
}
#endif  

#endif 
