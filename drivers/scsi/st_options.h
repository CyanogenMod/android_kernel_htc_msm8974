/*
   The compile-time configurable defaults for the Linux SCSI tape driver.

   Copyright 1995-2003 Kai Makisara.

   Last modified: Thu Feb 21 21:47:07 2008 by kai.makisara
*/

#ifndef _ST_OPTIONS_H
#define _ST_OPTIONS_H

#define TRY_DIRECT_IO 1

#define ST_NOWAIT 0

#define ST_IN_FILE_POS 0

#define ST_RECOVERED_WRITE_FATAL 0

#define ST_DEFAULT_BLOCK 0

#define ST_FIXED_BUFFER_BLOCKS 32

#define ST_MAX_SG      256

#define ST_FIRST_SG    8

#define ST_FIRST_ORDER  5



/* If ST_TWO_FM is non-zero, the driver writes two filemarks after a
   file being written. Some drives can't handle two filemarks at the
   end of data. */
#define ST_TWO_FM 0

#define ST_BUFFER_WRITES 1

#define ST_ASYNC_WRITES 1

#define ST_READ_AHEAD 1

#define ST_AUTO_LOCK 0

#define ST_FAST_MTEOM 0

#define ST_SCSI2LOGICAL 0

#define ST_SYSV 0

#define ST_SILI 0

#define ST_BLOCK_SECONDS     120

#endif
