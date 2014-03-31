#ifndef _INC_SBECOM_INLNX_H_
#define _INC_SBECOM_INLNX_H_

/*-----------------------------------------------------------------------------
 * sbecom_inline_linux.h - SBE common Linux inlined routines
 *
 * Copyright (C) 2007  One Stop Systems, Inc.
 * Copyright (C) 2005  SBE, Inc.
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
 *
 * For further information, contact via email: support@onestopsystems.com
 * One Stop Systems, Inc.  Escondido, California  U.S.A.
 *-----------------------------------------------------------------------------
 */


#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>       
#include <linux/skbuff.h>       
#include <linux/netdevice.h>    
#include <asm/byteorder.h>      

u_int32_t   pci_read_32 (u_int32_t *p);
void        pci_write_32 (u_int32_t *p, u_int32_t v);




static inline void *
OS_kmalloc (size_t size)
{
    char       *ptr = kmalloc (size, GFP_KERNEL | GFP_DMA);

    if (ptr)
        memset (ptr, 0, size);
    return ptr;
}

static inline void
OS_kfree (void *x)
{
    kfree (x);
}



static inline void *
OS_mem_token_alloc (size_t size)
{
    struct sk_buff *skb;

    skb = dev_alloc_skb (size);
    if (!skb)
    {
        
        return 0;
    }
    return skb;
}


static inline void
OS_mem_token_free (void *token)
{
    dev_kfree_skb_any (token);
}


static inline void
OS_mem_token_free_irq (void *token)
{
    dev_kfree_skb_irq (token);
}


static inline void *
OS_mem_token_data (void *token)
{
    return ((struct sk_buff *) token)->data;
}


static inline void *
OS_mem_token_next (void *token)
{
    return 0;
}


static inline int
OS_mem_token_len (void *token)
{
    return ((struct sk_buff *) token)->len;
}


static inline int
OS_mem_token_tlen (void *token)
{
    return ((struct sk_buff *) token)->len;
}



static inline u_long
OS_phystov (void *addr)
{
    return (u_long) __va (addr);
}


static inline u_long
OS_vtophys (void *addr)
{
    return __pa (addr);
}



void        OS_sem_init (void *, int);


static inline void
OS_sem_free (void *sem)
{
}

#define SD_SEM_TAKE(sem,desc)  down(sem)
#define SD_SEM_GIVE(sem)       up(sem)
#define SEM_AVAILABLE     1
#define SEM_TAKEN         0



struct watchdog
{
    struct timer_list h;
    struct work_struct work;
    void       *softc;
    void        (*func) (void *softc);
    int         ticks;
    int         init_tq;
};


static inline int
OS_start_watchdog (struct watchdog * wd)
{
    wd->h.expires = jiffies + wd->ticks;
    add_timer (&wd->h);
    return 0;
}


static inline int
OS_stop_watchdog (struct watchdog * wd)
{
    del_timer_sync (&wd->h);
    return 0;
}


static inline int
OS_free_watchdog (struct watchdog * wd)
{
    OS_stop_watchdog (wd);
    OS_kfree (wd);
    return 0;
}


void        OS_uwait (int usec, char *description);
void        OS_uwait_dummy (void);


int OS_init_watchdog(struct watchdog *wdp, void (*f) (void *), void *ci, int usec);


#endif                          
