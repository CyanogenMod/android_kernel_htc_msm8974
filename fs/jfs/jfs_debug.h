/*
 *   Copyright (C) International Business Machines Corp., 2000-2002
 *   Portions Copyright (C) Christoph Hellwig, 2001-2002
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef _H_JFS_DEBUG
#define _H_JFS_DEBUG


#if defined(CONFIG_PROC_FS) && (defined(CONFIG_JFS_DEBUG) || defined(CONFIG_JFS_STATISTICS))
#define PROC_FS_JFS
extern void jfs_proc_init(void);
extern void jfs_proc_clean(void);
#endif

#define assert(p) do {	\
	if (!(p)) {	\
		printk(KERN_CRIT "BUG at %s:%d assert(%s)\n",	\
		       __FILE__, __LINE__, #p);			\
		BUG();	\
	}		\
} while (0)

#ifdef CONFIG_JFS_DEBUG
#define ASSERT(p) assert(p)

#define JFS_LOGLEVEL_ERR 1
#define JFS_LOGLEVEL_WARN 2
#define JFS_LOGLEVEL_DEBUG 3
#define JFS_LOGLEVEL_INFO 4

extern int jfsloglevel;

extern const struct file_operations jfs_txanchor_proc_fops;

#define jfs_info(fmt, arg...) do {			\
	if (jfsloglevel >= JFS_LOGLEVEL_INFO)		\
		printk(KERN_INFO fmt "\n", ## arg);	\
} while (0)

#define jfs_debug(fmt, arg...) do {			\
	if (jfsloglevel >= JFS_LOGLEVEL_DEBUG)		\
		printk(KERN_DEBUG fmt "\n", ## arg);	\
} while (0)

#define jfs_warn(fmt, arg...) do {			\
	if (jfsloglevel >= JFS_LOGLEVEL_WARN)		\
		printk(KERN_WARNING fmt "\n", ## arg);	\
} while (0)

#define jfs_err(fmt, arg...) do {			\
	if (jfsloglevel >= JFS_LOGLEVEL_ERR)		\
		printk(KERN_ERR fmt "\n", ## arg);	\
} while (0)

#else				
#define ASSERT(p) do {} while (0)
#define jfs_info(fmt, arg...) do {} while (0)
#define jfs_debug(fmt, arg...) do {} while (0)
#define jfs_warn(fmt, arg...) do {} while (0)
#define jfs_err(fmt, arg...) do {} while (0)
#endif				

#ifdef	CONFIG_JFS_STATISTICS
extern const struct file_operations jfs_lmstats_proc_fops;
extern const struct file_operations jfs_txstats_proc_fops;
extern const struct file_operations jfs_mpstat_proc_fops;
extern const struct file_operations jfs_xtstat_proc_fops;

#define	INCREMENT(x)		((x)++)
#define	DECREMENT(x)		((x)--)
#define	HIGHWATERMARK(x,y)	((x) = max((x), (y)))
#else
#define	INCREMENT(x)
#define	DECREMENT(x)
#define	HIGHWATERMARK(x,y)
#endif				

#endif				
