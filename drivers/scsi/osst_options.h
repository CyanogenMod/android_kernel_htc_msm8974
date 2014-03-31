/*
   The compile-time configurable defaults for the Linux SCSI tape driver.

   Copyright 1995 Kai Makisara.
   
   Last modified: Wed Sep  2 21:24:07 1998 by root@home
   
   Changed (and renamed) for OnStream SCSI drives garloff@suse.de
   2000-06-21

   $Header: /cvsroot/osst/Driver/osst_options.h,v 1.6 2003/12/23 14:22:12 wriede Exp $
*/

#ifndef _OSST_OPTIONS_H
#define _OSST_OPTIONS_H

#define OSST_MAX_TAPES 4

#define OSST_IN_FILE_POS 1

#define OSST_BUFFER_BLOCKS 32

#define OSST_WRITE_THRESHOLD_BLOCKS 32

#define OSST_EOM_RESERVE 300

#define OSST_MAX_BUFFERS OSST_MAX_TAPES 

#define OSST_MAX_SG      (((OSST_BUFFER_BLOCKS*1024) / PAGE_SIZE) + 1)

#define OSST_FIRST_SG    ((OSST_BUFFER_BLOCKS*1024) / PAGE_SIZE)

#define OSST_FIRST_ORDER  (15-PAGE_SHIFT)



/* If OSST_TWO_FM is non-zero, the driver writes two filemarks after a
   file being written. Some drives can't handle two filemarks at the
   end of data. */
#define OSST_TWO_FM 0

#define OSST_BUFFER_WRITES 1

#define OSST_ASYNC_WRITES 1

#define OSST_READ_AHEAD 1

#define OSST_AUTO_LOCK 0

#define OSST_FAST_MTEOM 0

#define OSST_SCSI2LOGICAL 0

#define OSST_SYSV 0


#endif
