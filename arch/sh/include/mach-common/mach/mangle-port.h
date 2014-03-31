/*
 * SH version cribbed from the MIPS copy:
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2003, 2004 Ralf Baechle
 */
#ifndef __MACH_COMMON_MANGLE_PORT_H
#define __MACH_COMMON_MANGLE_PORT_H

#if defined(CONFIG_SWAP_IO_SPACE)

# define ioswabb(x)		(x)
# define __mem_ioswabb(x)	(x)
# define ioswabw(x)		le16_to_cpu(x)
# define __mem_ioswabw(x)	(x)
# define ioswabl(x)		le32_to_cpu(x)
# define __mem_ioswabl(x)	(x)
# define ioswabq(x)		le64_to_cpu(x)
# define __mem_ioswabq(x)	(x)

#else

# define ioswabb(x)		(x)
# define __mem_ioswabb(x)	(x)
# define ioswabw(x)		(x)
# define __mem_ioswabw(x)	cpu_to_le16(x)
# define ioswabl(x)		(x)
# define __mem_ioswabl(x)	cpu_to_le32(x)
# define ioswabq(x)		(x)
# define __mem_ioswabq(x)	cpu_to_le32(x)

#endif

#endif 
