/*
 * mpmbox.h:  Interface and defines for the OpenProm mailbox
 *               facilities for MP machines under Linux.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef _SPARC_MPMBOX_H
#define _SPARC_MPMBOX_H



#define MAILBOX_ISRUNNING     0xf0

#define MAILBOX_EXIT          0xfb

#define MAILBOX_GOSPIN        0xfc

#define MAILBOX_BPT_SPIN      0xfd

#define MAILBOX_WDOG_STOP     0xfe

#ifndef __ASSEMBLY__


#define MBOX_POST_P(letter)  ((letter) >= 0x00 && (letter) <= 0x7f)

#define MBOX_PROMPROMPT_P(letter) ((letter) >= 0x80 && (letter) <= 0x8f)

#define MBOX_PROMSPIN_P(letter) ((letter) >= 0x90 && (letter) <= 0xef)

#define MBOX_BOGON_P(letter) ((letter) >= 0xf1 && (letter) <= 0xfa)

#define MBOX_RUNNING_P(letter) ((letter) == MAILBOX_ISRUNNING)

#endif 

#endif 
