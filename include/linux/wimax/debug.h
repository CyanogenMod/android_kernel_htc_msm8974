/*
 * Linux WiMAX
 * Collection of tools to manage debug operations.
 *
 *
 * Copyright (C) 2005-2007 Intel Corporation
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 *
 * Don't #include this file directly, read on!
 *
 *
 * EXECUTING DEBUGGING ACTIONS OR NOT
 *
 * The main thing this framework provides is decission power to take a
 * debug action (like printing a message) if the current debug level
 * allows it.
 *
 * The decission power is at two levels: at compile-time (what does
 * not make it is compiled out) and at run-time. The run-time
 * selection is done per-submodule (as they are declared by the user
 * of the framework).
 *
 * A call to d_test(L) (L being the target debug level) returns true
 * if the action should be taken because the current debug levels
 * allow it (both compile and run time).
 *
 * It follows that a call to d_test() that can be determined to be
 * always false at compile time will get the code depending on it
 * compiled out by optimization.
 *
 *
 * DEBUG LEVELS
 *
 * It is up to the caller to define how much a debugging level is.
 *
 * Convention sets 0 as "no debug" (so an action marked as debug level 0
 * will always be taken). The increasing debug levels are used for
 * increased verbosity.
 *
 *
 * USAGE
 *
 * Group the code in modules and submodules inside each module [which
 * in most cases maps to Linux modules and .c files that compose
 * those].
 *
 *
 * For each module, there is:
 *
 *  - a MODULENAME (single word, legal C identifier)
 *
 *  - a debug-levels.h header file that declares the list of
 *    submodules and that is included by all .c files that use
 *    the debugging tools. The file name can be anything.
 *
 *  - some (optional) .c code to manipulate the runtime debug levels
 *    through debugfs.
 *
 * The debug-levels.h file would look like:
 *
 *     #ifndef __debug_levels__h__
 *     #define __debug_levels__h__
 *
 *     #define D_MODULENAME modulename
 *     #define D_MASTER 10
 *
 *     #include <linux/wimax/debug.h>
 *
 *     enum d_module {
 *             D_SUBMODULE_DECLARE(submodule_1),
 *             D_SUBMODULE_DECLARE(submodule_2),
 *             ...
 *             D_SUBMODULE_DECLARE(submodule_N)
 *     };
 *
 *     #endif
 *
 * D_MASTER is the maximum compile-time debug level; any debug actions
 * above this will be out. D_MODULENAME is the module name (legal C
 * identifier), which has to be unique for each module (to avoid
 * namespace collisions during linkage). Note those #defines need to
 * be done before #including debug.h
 *
 * We declare N different submodules whose debug level can be
 * independently controlled during runtime.
 *
 * In a .c file of the module (and only in one of them), define the
 * following code:
 *
 *     struct d_level D_LEVEL[] = {
 *             D_SUBMODULE_DEFINE(submodule_1),
 *             D_SUBMODULE_DEFINE(submodule_2),
 *             ...
 *             D_SUBMODULE_DEFINE(submodule_N),
 *     };
 *     size_t D_LEVEL_SIZE = ARRAY_SIZE(D_LEVEL);
 *
 * Externs for d_level_MODULENAME and d_level_size_MODULENAME are used
 * and declared in this file using the D_LEVEL and D_LEVEL_SIZE macros
 * #defined also in this file.
 *
 * To manipulate from user space the levels, create a debugfs dentry
 * and then register each submodule with:
 *
 *     result = d_level_register_debugfs("PREFIX_", submodule_X, parent);
 *     if (result < 0)
 *            goto error;
 *
 * Where PREFIX_ is a name of your chosing. This will create debugfs
 * file with a single numeric value that can be use to tweak it. To
 * remove the entires, just use debugfs_remove_recursive() on 'parent'.
 *
 * NOTE: remember that even if this will show attached to some
 *     particular instance of a device, the settings are *global*.
 *
 *
 * On each submodule (for example, .c files), the debug infrastructure
 * should be included like this:
 *
 *     #define D_SUBMODULE submodule_x     // matches one in debug-levels.h
 *     #include "debug-levels.h"
 *
 * after #including all your include files.
 *
 *
 * Now you can use the d_*() macros below [d_test(), d_fnstart(),
 * d_fnend(), d_printf(), d_dump()].
 *
 * If their debug level is greater than D_MASTER, they will be
 * compiled out.
 *
 * If their debug level is lower or equal than D_MASTER but greater
 * than the current debug level of their submodule, they'll be
 * ignored.
 *
 * Otherwise, the action will be performed.
 */
#ifndef __debug__h__
#define __debug__h__

#include <linux/types.h>
#include <linux/slab.h>

struct device;


static inline
void __d_head(char *head, size_t head_size,
	      struct device *dev)
{
	if (dev == NULL)
		head[0] = 0;
	else if ((unsigned long)dev < 4096) {
		printk(KERN_ERR "E: Corrupt dev %p\n", dev);
		WARN_ON(1);
	} else
		snprintf(head, head_size, "%s %s: ",
			 dev_driver_string(dev), dev_name(dev));
}


#define _d_printf(l, tag, dev, f, a...)					\
do {									\
	char head[64];							\
	if (!d_test(l))							\
		break;							\
	__d_head(head, sizeof(head), dev);				\
	printk(KERN_ERR "%s%s%s: " f, head, __func__, tag, ##a);	\
} while (0)


#define __D_PASTE__(varname, modulename) varname##_##modulename
#define __D_PASTE(varname, modulename) (__D_PASTE__(varname, modulename))
#define _D_SUBMODULE_INDEX(_name) (D_SUBMODULE_DECLARE(_name))


struct d_level {
	u8 level;
	const char *name;
};


#define D_LEVEL __D_PASTE(d_level, D_MODULENAME)
#define D_LEVEL_SIZE __D_PASTE(d_level_size, D_MODULENAME)

extern struct d_level D_LEVEL[];
extern size_t D_LEVEL_SIZE;



#ifndef D_MODULENAME
#error D_MODULENAME is not defined in your debug-levels.h file
#define D_MODULENAME undefined_modulename
#endif


#ifndef D_MASTER
#warning D_MASTER not defined, but debug.h included! [see docs]
#define D_MASTER 0
#endif

#ifndef D_SUBMODULE
#error D_SUBMODULE not defined, but debug.h included! [see docs]
#define D_SUBMODULE undefined_module
#endif


#define D_SUBMODULE_DECLARE(_name) __D_SUBMODULE_##_name


#define D_SUBMODULE_DEFINE(_name)		\
[__D_SUBMODULE_##_name] = {			\
	.level = 0,				\
	.name = #_name				\
}





#define d_test(l)							\
({									\
	unsigned __l = l;				\
	(D_MASTER) >= __l						\
	&& ({								\
		BUG_ON(_D_SUBMODULE_INDEX(D_SUBMODULE) >= D_LEVEL_SIZE);\
		D_LEVEL[_D_SUBMODULE_INDEX(D_SUBMODULE)].level >= __l;	\
	});								\
})


#define d_fnstart(l, _dev, f, a...) _d_printf(l, " FNSTART", _dev, f, ## a)


#define d_fnend(l, _dev, f, a...) _d_printf(l, " FNEND", _dev, f, ## a)


#define d_printf(l, _dev, f, a...) _d_printf(l, "", _dev, f, ## a)


#define d_dump(l, dev, ptr, size)			\
do {							\
	char head[64];					\
	if (!d_test(l))					\
		break;					\
	__d_head(head, sizeof(head), dev);		\
	print_hex_dump(KERN_ERR, head, 0, 16, 1,	\
		       ((void *) ptr), (size), 0);	\
} while (0)


#define d_level_register_debugfs(prefix, name, parent)			\
({									\
	int rc;								\
	struct dentry *fd;						\
	struct dentry *verify_parent_type = parent;			\
	fd = debugfs_create_u8(						\
		prefix #name, 0600, verify_parent_type,			\
		&(D_LEVEL[__D_SUBMODULE_ ## name].level));		\
	rc = PTR_ERR(fd);						\
	if (IS_ERR(fd) && rc != -ENODEV)				\
		printk(KERN_ERR "%s: Can't create debugfs entry %s: "	\
		       "%d\n", __func__, prefix #name, rc);		\
	else								\
		rc = 0;							\
	rc;								\
})


static inline
void d_submodule_set(struct d_level *d_level, size_t d_level_size,
		     const char *submodule, u8 level, const char *tag)
{
	struct d_level *itr, *top;
	int index = -1;

	for (itr = d_level, top = itr + d_level_size; itr < top; itr++) {
		index++;
		if (itr->name == NULL) {
			printk(KERN_ERR "%s: itr->name NULL?? (%p, #%d)\n",
			       tag, itr, index);
			continue;
		}
		if (!strcmp(itr->name, submodule)) {
			itr->level = level;
			return;
		}
	}
	printk(KERN_ERR "%s: unknown submodule %s\n", tag, submodule);
}


static inline
void d_parse_params(struct d_level *d_level, size_t d_level_size,
		    const char *_params, const char *tag)
{
	char submodule[130], *params, *params_orig, *token, *colon;
	unsigned level, tokens;

	if (_params == NULL)
		return;
	params_orig = kstrdup(_params, GFP_KERNEL);
	params = params_orig;
	while (1) {
		token = strsep(&params, " ");
		if (token == NULL)
			break;
		if (*token == '\0')	
			continue;
		colon = strchr(token, ':');
		if (colon != NULL)
			*colon = '\n';
		tokens = sscanf(token, "%s\n%u", submodule, &level);
		if (colon != NULL)
			*colon = ':';	
		if (tokens == 2)
			d_submodule_set(d_level, d_level_size,
					submodule, level, tag);
		else
			printk(KERN_ERR "%s: can't parse '%s' as a "
			       "SUBMODULE:LEVEL (%d tokens)\n",
			       tag, token, tokens);
	}
	kfree(params_orig);
}

#endif 
