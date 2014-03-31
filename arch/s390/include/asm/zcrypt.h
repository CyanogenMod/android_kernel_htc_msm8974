/*
 *  include/asm-s390/zcrypt.h
 *
 *  zcrypt 2.1.0 (user-visible header)
 *
 *  Copyright (C)  2001, 2006 IBM Corporation
 *  Author(s): Robert Burroughs
 *	       Eric Rossman (edrossma@us.ibm.com)
 *
 *  Hotplug & misc device support: Jochen Roehrig (roehrig@de.ibm.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __ASM_S390_ZCRYPT_H
#define __ASM_S390_ZCRYPT_H

#define ZCRYPT_VERSION 2
#define ZCRYPT_RELEASE 1
#define ZCRYPT_VARIANT 1

#include <linux/ioctl.h>
#include <linux/compiler.h>

struct ica_rsa_modexpo {
	char __user *	inputdata;
	unsigned int	inputdatalength;
	char __user *	outputdata;
	unsigned int	outputdatalength;
	char __user *	b_key;
	char __user *	n_modulus;
};

struct ica_rsa_modexpo_crt {
	char __user *	inputdata;
	unsigned int	inputdatalength;
	char __user *	outputdata;
	unsigned int	outputdatalength;
	char __user *	bp_key;
	char __user *	bq_key;
	char __user *	np_prime;
	char __user *	nq_prime;
	char __user *	u_mult_inv;
};

struct CPRBX {
	unsigned short	cprb_len;	
	unsigned char	cprb_ver_id;	
	unsigned char	pad_000[3];	
	unsigned char	func_id[2];	
	unsigned char	cprb_flags[4];	
	unsigned int	req_parml;	
	unsigned int	req_datal;	
	unsigned int	rpl_msgbl;	
	unsigned int	rpld_parml;	
	unsigned int	rpl_datal;	
	unsigned int	rpld_datal;	
	unsigned int	req_extbl;	
	unsigned char	pad_001[4];	
	unsigned int	rpld_extbl;	
	unsigned char	padx000[16 - sizeof (char *)];
	unsigned char *	req_parmb;	
	unsigned char	padx001[16 - sizeof (char *)];
	unsigned char *	req_datab;	
	unsigned char	padx002[16 - sizeof (char *)];
	unsigned char *	rpl_parmb;	
	unsigned char	padx003[16 - sizeof (char *)];
	unsigned char *	rpl_datab;	
	unsigned char	padx004[16 - sizeof (char *)];
	unsigned char *	req_extb;	
	unsigned char	padx005[16 - sizeof (char *)];
	unsigned char *	rpl_extb;	
	unsigned short	ccp_rtcode;	
	unsigned short	ccp_rscode;	
	unsigned int	mac_data_len;	
	unsigned char	logon_id[8];	
	unsigned char	mac_value[8];	
	unsigned char	mac_content_flgs;
	unsigned char	pad_002;	
	unsigned short	domain;		
	unsigned char	usage_domain[4];
	unsigned char	cntrl_domain[4];
	unsigned char	S390enf_mask[4];
	unsigned char	pad_004[36];	
} __attribute__((packed));

struct ica_xcRB {
	unsigned short	agent_ID;
	unsigned int	user_defined;
	unsigned short	request_ID;
	unsigned int	request_control_blk_length;
	unsigned char	padding1[16 - sizeof (char *)];
	char __user *	request_control_blk_addr;
	unsigned int	request_data_length;
	char		padding2[16 - sizeof (char *)];
	char __user *	request_data_address;
	unsigned int	reply_control_blk_length;
	char		padding3[16 - sizeof (char *)];
	char __user *	reply_control_blk_addr;
	unsigned int	reply_data_length;
	char		padding4[16 - sizeof (char *)];
	char __user *	reply_data_addr;
	unsigned short	priority_window;
	unsigned int	status;
} __attribute__((packed));
#define AUTOSELECT ((unsigned int)0xFFFFFFFF)

#define ZCRYPT_IOCTL_MAGIC 'z'


#define ICARSAMODEXPO	_IOC(_IOC_READ|_IOC_WRITE, ZCRYPT_IOCTL_MAGIC, 0x05, 0)
#define ICARSACRT	_IOC(_IOC_READ|_IOC_WRITE, ZCRYPT_IOCTL_MAGIC, 0x06, 0)
#define ZSECSENDCPRB	_IOC(_IOC_READ|_IOC_WRITE, ZCRYPT_IOCTL_MAGIC, 0x81, 0)

#define Z90STAT_TOTALCOUNT	_IOR(ZCRYPT_IOCTL_MAGIC, 0x40, int)
#define Z90STAT_PCICACOUNT	_IOR(ZCRYPT_IOCTL_MAGIC, 0x41, int)
#define Z90STAT_PCICCCOUNT	_IOR(ZCRYPT_IOCTL_MAGIC, 0x42, int)
#define Z90STAT_PCIXCCMCL2COUNT	_IOR(ZCRYPT_IOCTL_MAGIC, 0x4b, int)
#define Z90STAT_PCIXCCMCL3COUNT	_IOR(ZCRYPT_IOCTL_MAGIC, 0x4c, int)
#define Z90STAT_CEX2CCOUNT	_IOR(ZCRYPT_IOCTL_MAGIC, 0x4d, int)
#define Z90STAT_CEX2ACOUNT	_IOR(ZCRYPT_IOCTL_MAGIC, 0x4e, int)
#define Z90STAT_REQUESTQ_COUNT	_IOR(ZCRYPT_IOCTL_MAGIC, 0x44, int)
#define Z90STAT_PENDINGQ_COUNT	_IOR(ZCRYPT_IOCTL_MAGIC, 0x45, int)
#define Z90STAT_TOTALOPEN_COUNT _IOR(ZCRYPT_IOCTL_MAGIC, 0x46, int)
#define Z90STAT_DOMAIN_INDEX	_IOR(ZCRYPT_IOCTL_MAGIC, 0x47, int)
#define Z90STAT_STATUS_MASK	_IOR(ZCRYPT_IOCTL_MAGIC, 0x48, char[64])
#define Z90STAT_QDEPTH_MASK	_IOR(ZCRYPT_IOCTL_MAGIC, 0x49, char[64])
#define Z90STAT_PERDEV_REQCNT	_IOR(ZCRYPT_IOCTL_MAGIC, 0x4a, int[64])

#endif 
