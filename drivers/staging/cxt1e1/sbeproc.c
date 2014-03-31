/* Copyright (C) 2004-2005  SBE, Inc.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include "pmcc4_sysdep.h"
#include "sbecom_inline_linux.h"
#include "pmcc4_private.h"
#include "sbeproc.h"

void        sbecom_get_brdinfo (ci_t *, struct sbe_brd_info *, u_int8_t *);
extern struct s_hdw_info hdw_info[MAX_BOARDS];

#ifdef CONFIG_PROC_FS



void
sbecom_proc_brd_cleanup (ci_t * ci)
{
    if (ci->dir_dev)
    {
	char dir[7 + SBE_IFACETMPL_SIZE + 1];
	snprintf(dir, sizeof(dir), "driver/%s", ci->devname);
        remove_proc_entry("info", ci->dir_dev);
        remove_proc_entry(dir, NULL);
        ci->dir_dev = NULL;
    }
}


static int
sbecom_proc_get_sbe_info (char *buffer, char **start, off_t offset,
                          int length, int *eof, void *priv)
{
    ci_t       *ci = (ci_t *) priv;
    int         len = 0;
    char       *spd;
    struct sbe_brd_info *bip;

    if (!(bip = OS_kmalloc (sizeof (struct sbe_brd_info))))
    {
        return -ENOMEM;
    }
#if 0
    
    pr_info(">> sbecom_proc_get_sbe_info: entered, offset %d. length %d.\n",
            (int) offset, (int) length);
#endif

    {
        hdw_info_t *hi = &hdw_info[ci->brdno];

        u_int8_t *bsn = 0;

        switch (hi->promfmt)
        {
        case PROM_FORMAT_TYPE1:
            bsn = (u_int8_t *) hi->mfg_info.pft1.Serial;
            break;
        case PROM_FORMAT_TYPE2:
            bsn = (u_int8_t *) hi->mfg_info.pft2.Serial;
            break;
        }

        sbecom_get_brdinfo (ci, bip, bsn);
    }

#if 0
    
    pr_info(">> sbecom_get_brdinfo: returned, first_if %p <%s> last_if %p <%s>\n",
            (char *) &bip->first_iname, (char *) &bip->first_iname,
            (char *) &bip->last_iname, (char *) &bip->last_iname);
#endif
    len += sprintf (buffer + len, "Board Type:    ");
    switch (bip->brd_id)
    {
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C1T3):
        len += sprintf (buffer + len, "wanPMC-C1T3");
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPTMC_256T3_E1):
        len += sprintf (buffer + len, "wanPTMC-256T3 <E1>");
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPTMC_256T3_T1):
        len += sprintf (buffer + len, "wanPTMC-256T3 <T1>");
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPTMC_C24TE1):
        len += sprintf (buffer + len, "wanPTMC-C24TE1");
        break;

    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C4T1E1):
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C4T1E1_L):
        len += sprintf (buffer + len, "wanPMC-C4T1E1");
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C2T1E1):
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C2T1E1_L):
        len += sprintf (buffer + len, "wanPMC-C2T1E1");
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C1T1E1):
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C1T1E1_L):
        len += sprintf (buffer + len, "wanPMC-C1T1E1");
        break;

    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPCI_C4T1E1):
        len += sprintf (buffer + len, "wanPCI-C4T1E1");
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPCI_C2T1E1):
        len += sprintf (buffer + len, "wanPCI-C2T1E1");
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPCI_C1T1E1):
        len += sprintf (buffer + len, "wanPCI-C1T1E1");
        break;

    default:
        len += sprintf (buffer + len, "unknown");
        break;
    }
    len += sprintf (buffer + len, "  [%08X]\n", bip->brd_id);

    len += sprintf (buffer + len, "Board Number:  %d\n", bip->brdno);
    len += sprintf (buffer + len, "Hardware ID:   0x%02X\n", ci->hdw_bid);
    len += sprintf (buffer + len, "Board SN:      %06X\n", bip->brd_sn);
	len += sprintf(buffer + len, "Board MAC:     %pMF\n",
		bip->brd_mac_addr);
    len += sprintf (buffer + len, "Ports:         %d\n", ci->max_port);
    len += sprintf (buffer + len, "Channels:      %d\n", bip->brd_chan_cnt);
#if 1
    len += sprintf (buffer + len, "Interface:     %s -> %s\n",
                    (char *) &bip->first_iname, (char *) &bip->last_iname);
#else
    len += sprintf (buffer + len, "Interface:     <not available> 1st %p lst %p\n",
                    (char *) &bip->first_iname, (char *) &bip->last_iname);
#endif

    switch (bip->brd_pci_speed)
    {
    case BINFO_PCI_SPEED_33:
        spd = "33Mhz";
        break;
    case BINFO_PCI_SPEED_66:
        spd = "66Mhz";
        break;
    default:
        spd = "<not available>";
        break;
    }
    len += sprintf (buffer + len, "PCI Bus Speed: %s\n", spd);
    len += sprintf (buffer + len, "Release:       %s\n", ci->release);

#ifdef SBE_PMCC4_ENABLE
    {
               extern int cxt1e1_max_mru;
#if 0
        extern int max_chans_used;
        extern int cxt1e1_max_mtu;
#endif
        extern int max_rxdesc_used, max_txdesc_used;

        len += sprintf (buffer + len, "\ncxt1e1_max_mru:         %d\n", cxt1e1_max_mru);
#if 0
        len += sprintf (buffer + len, "\nmax_chans_used:  %d\n", max_chans_used);
        len += sprintf (buffer + len, "cxt1e1_max_mtu:         %d\n", cxt1e1_max_mtu);
#endif
        len += sprintf (buffer + len, "max_rxdesc_used: %d\n", max_rxdesc_used);
        len += sprintf (buffer + len, "max_txdesc_used: %d\n", max_txdesc_used);
    }
#endif

    OS_kfree (bip);                 


#if 1
    
    *eof = 1;
#endif

#if 0
    if (len <= offset + length)
        *eof = 1;
    *start = buffer + offset;
    len -= offset;
    if (len > length)
        len = length;
    if (len < 0)
        len = 0;
#endif

#if 0                               
    {
        off_t       begin = 0;
        int         size = 0;
        off_t       pos = 0;

        size = len;
        pos = begin + size;
        if (pos < offset)
        {
            len = 0;
            begin = pos;
        }
        *start = buffer + (offset - begin);     
        len -= (offset - begin);    
        if (len > length)
            len = length;           
    }
#endif

#if 0                               
    len = strlen (buffer);
    *start = NULL;
    if (offset + length >= len)
        *eof = 1;
    else
        *eof = 0;
#endif

#if 0
    pr_info(">> proc_fs: returned len = %d., start %p\n", len, start);  
#endif


    return len;
}


int         __init
sbecom_proc_brd_init (ci_t * ci)
{
    struct proc_dir_entry *e;
    char dir[7 + SBE_IFACETMPL_SIZE + 1];

    
    snprintf(dir, sizeof(dir), "driver/%s", ci->devname);
    ci->dir_dev = proc_mkdir(dir, NULL);
    if (!ci->dir_dev)
    {
        pr_err("Unable to create directory /proc/driver/%s\n", ci->devname);
        goto fail;
    }
    e = create_proc_read_entry ("info", S_IFREG | S_IRUGO,
                                ci->dir_dev, sbecom_proc_get_sbe_info, ci);
    if (!e)
    {
        pr_err("Unable to create entry /proc/driver/%s/info\n", ci->devname);
        goto fail;
    }
    return 0;

fail:
    sbecom_proc_brd_cleanup (ci);
    return 1;
}

#else                           


void
sbecom_proc_brd_cleanup (ci_t * ci)
{
}

int         __init
sbecom_proc_brd_init (ci_t * ci)
{
    return 0;
}

#endif                          


