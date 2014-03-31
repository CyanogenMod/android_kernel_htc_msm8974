#ifndef _DWARF_AUX_H
#define _DWARF_AUX_H
/*
 * dwarf-aux.h : libdw auxiliary interfaces
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <dwarf.h>
#include <elfutils/libdw.h>
#include <elfutils/libdwfl.h>
#include <elfutils/version.h>

extern const char *cu_find_realpath(Dwarf_Die *cu_die, const char *fname);

extern const char *cu_get_comp_dir(Dwarf_Die *cu_die);

extern int cu_find_lineinfo(Dwarf_Die *cudie, unsigned long addr,
			    const char **fname, int *lineno);

extern int cu_walk_functions_at(Dwarf_Die *cu_die, Dwarf_Addr addr,
			int (*callback)(Dwarf_Die *, void *), void *data);

extern bool die_compare_name(Dwarf_Die *dw_die, const char *tname);

extern int die_get_call_lineno(Dwarf_Die *in_die);

extern const char *die_get_call_file(Dwarf_Die *in_die);

extern Dwarf_Die *die_get_type(Dwarf_Die *vr_die, Dwarf_Die *die_mem);

extern Dwarf_Die *die_get_real_type(Dwarf_Die *vr_die, Dwarf_Die *die_mem);

extern bool die_is_signed_type(Dwarf_Die *tp_die);

extern int die_get_data_member_location(Dwarf_Die *mb_die, Dwarf_Word *offs);

enum {
	DIE_FIND_CB_END = 0,		
	DIE_FIND_CB_CHILD = 1,		
	DIE_FIND_CB_SIBLING = 2,	
	DIE_FIND_CB_CONTINUE = 3,	
};

extern Dwarf_Die *die_find_child(Dwarf_Die *rt_die,
				 int (*callback)(Dwarf_Die *, void *),
				 void *data, Dwarf_Die *die_mem);

extern Dwarf_Die *die_find_realfunc(Dwarf_Die *cu_die, Dwarf_Addr addr,
				    Dwarf_Die *die_mem);

extern Dwarf_Die *die_find_inlinefunc(Dwarf_Die *sp_die, Dwarf_Addr addr,
				      Dwarf_Die *die_mem);

extern int die_walk_instances(Dwarf_Die *in_die,
			      int (*callback)(Dwarf_Die *, void *), void *data);

typedef int (* line_walk_callback_t) (const char *fname, int lineno,
				      Dwarf_Addr addr, void *data);

extern int die_walk_lines(Dwarf_Die *rt_die, line_walk_callback_t callback,
			  void *data);

extern Dwarf_Die *die_find_variable_at(Dwarf_Die *sp_die, const char *name,
				       Dwarf_Addr addr, Dwarf_Die *die_mem);

extern Dwarf_Die *die_find_member(Dwarf_Die *st_die, const char *name,
				  Dwarf_Die *die_mem);

extern int die_get_typename(Dwarf_Die *vr_die, char *buf, int len);

extern int die_get_varname(Dwarf_Die *vr_die, char *buf, int len);
#endif
