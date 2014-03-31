/*
 *    Disk Array driver for HP Smart Array controllers, SCSI Tape module.
 *    (C) Copyright 2001, 2007 Hewlett-Packard Development Company, L.P.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; version 2 of the License.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *    General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 300, Boston, MA
 *    02111-1307, USA.
 *
 *    Questions/Comments/Bugfixes to iss_storagedev@hp.com
 *
 */
#ifdef CONFIG_CISS_SCSI_TAPE
#ifndef _CCISS_SCSI_H_
#define _CCISS_SCSI_H_

#include <scsi/scsicam.h> 

		
#define SELF_SCSI_ID 15



struct cciss_scsi_dev_t {
	int devtype;
	int bus, target, lun;		
	unsigned char scsi3addr[8];	
	unsigned char device_id[16];	
	unsigned char vendor[8];	
	unsigned char model[16];	
	unsigned char revision[4];	
};

struct cciss_scsi_hba_t {
	char *name;
	int ndevices;
#define CCISS_MAX_SCSI_DEVS_PER_HBA 16
	struct cciss_scsi_dev_t dev[CCISS_MAX_SCSI_DEVS_PER_HBA];
};

#endif 
#endif 
