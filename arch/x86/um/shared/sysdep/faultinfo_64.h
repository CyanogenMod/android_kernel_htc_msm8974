/*
 * Copyright (C) 2004 Fujitsu Siemens Computers GmbH
 * Author: Bodo Stroesser <bstroesser@fujitsu-siemens.com>
 * Licensed under the GPL
 */

#ifndef __FAULTINFO_X86_64_H
#define __FAULTINFO_X86_64_H

struct faultinfo {
        int error_code; 
        unsigned long cr2; 
        int trap_no; 
};

#define FAULT_WRITE(fi) ((fi).error_code & 2)
#define FAULT_ADDRESS(fi) ((fi).cr2)

#define SEGV_IS_FIXABLE(fi)	((fi)->trap_no == 14)

#define SEGV_MAYBE_FIXABLE(fi)	0

#define PTRACE_FULL_FAULTINFO 1

#endif
