/* $Id: isdn_divertif.h,v 1.4.6.1 2001/09/23 22:25:05 kai Exp $
 *
 * Header for the diversion supplementary interface for i4l.
 *
 * Author    Werner Cornelius (werner@titro.de)
 * Copyright by Werner Cornelius (werner@titro.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef _LINUX_ISDN_DIVERTIF_H
#define _LINUX_ISDN_DIVERTIF_H

#define DIVERT_IF_MAGIC 0x25873401
#define DIVERT_CMD_REG  0x00  
#define DIVERT_CMD_REL  0x01  
#define DIVERT_NO_ERR   0x00  
#define DIVERT_CMD_ERR  0x01  
#define DIVERT_VER_ERR  0x02  
#define DIVERT_REG_ERR  0x03  
#define DIVERT_REL_ERR  0x04  
#define DIVERT_REG_NAME isdn_register_divert

#ifdef __KERNEL__
#include <linux/isdnif.h>
#include <linux/types.h>

 
typedef struct
  { ulong if_magic; 
    int cmd; 
    int (*stat_callback)(isdn_ctrl *); 
    int (*ll_cmd)(isdn_ctrl *); 
    char * (*drv_to_name)(int); 
    int (*name_to_drv)(char *); 
  } isdn_divert_if;

extern int DIVERT_REG_NAME(isdn_divert_if *);
#endif

#endif 
