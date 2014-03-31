/*
 * dwarf-aux.c : libdw auxiliary interfaces
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

#include <stdbool.h>
#include "util.h"
#include "debug.h"
#include "dwarf-aux.h"

const char *cu_find_realpath(Dwarf_Die *cu_die, const char *fname)
{
	Dwarf_Files *files;
	size_t nfiles, i;
	const char *src = NULL;
	int ret;

	if (!fname)
		return NULL;

	ret = dwarf_getsrcfiles(cu_die, &files, &nfiles);
	if (ret != 0)
		return NULL;

	for (i = 0; i < nfiles; i++) {
		src = dwarf_filesrc(files, i, NULL, NULL);
		if (strtailcmp(src, fname) == 0)
			break;
	}
	if (i == nfiles)
		return NULL;
	return src;
}

const char *cu_get_comp_dir(Dwarf_Die *cu_die)
{
	Dwarf_Attribute attr;
	if (dwarf_attr(cu_die, DW_AT_comp_dir, &attr) == NULL)
		return NULL;
	return dwarf_formstring(&attr);
}

int cu_find_lineinfo(Dwarf_Die *cu_die, unsigned long addr,
		    const char **fname, int *lineno)
{
	Dwarf_Line *line;
	Dwarf_Addr laddr;

	line = dwarf_getsrc_die(cu_die, (Dwarf_Addr)addr);
	if (line && dwarf_lineaddr(line, &laddr) == 0 &&
	    addr == (unsigned long)laddr && dwarf_lineno(line, lineno) == 0) {
		*fname = dwarf_linesrc(line, NULL, NULL);
		if (!*fname)
			
			*lineno = 0;
	}

	return *lineno ?: -ENOENT;
}

static int __die_find_inline_cb(Dwarf_Die *die_mem, void *data);

int cu_walk_functions_at(Dwarf_Die *cu_die, Dwarf_Addr addr,
		    int (*callback)(Dwarf_Die *, void *), void *data)
{
	Dwarf_Die die_mem;
	Dwarf_Die *sc_die;
	int ret = -ENOENT;

	
	for (sc_die = die_find_realfunc(cu_die, addr, &die_mem);
	     sc_die != NULL;
	     sc_die = die_find_child(sc_die, __die_find_inline_cb, &addr,
				     &die_mem)) {
		ret = callback(sc_die, data);
		if (ret)
			break;
	}

	return ret;

}

bool die_compare_name(Dwarf_Die *dw_die, const char *tname)
{
	const char *name;
	name = dwarf_diename(dw_die);
	return name ? (strcmp(tname, name) == 0) : false;
}

int die_get_call_lineno(Dwarf_Die *in_die)
{
	Dwarf_Attribute attr;
	Dwarf_Word ret;

	if (!dwarf_attr(in_die, DW_AT_call_line, &attr))
		return -ENOENT;

	dwarf_formudata(&attr, &ret);
	return (int)ret;
}

Dwarf_Die *die_get_type(Dwarf_Die *vr_die, Dwarf_Die *die_mem)
{
	Dwarf_Attribute attr;

	if (dwarf_attr_integrate(vr_die, DW_AT_type, &attr) &&
	    dwarf_formref_die(&attr, die_mem))
		return die_mem;
	else
		return NULL;
}

static Dwarf_Die *__die_get_real_type(Dwarf_Die *vr_die, Dwarf_Die *die_mem)
{
	int tag;

	do {
		vr_die = die_get_type(vr_die, die_mem);
		if (!vr_die)
			break;
		tag = dwarf_tag(vr_die);
	} while (tag == DW_TAG_const_type ||
		 tag == DW_TAG_restrict_type ||
		 tag == DW_TAG_volatile_type ||
		 tag == DW_TAG_shared_type);

	return vr_die;
}

Dwarf_Die *die_get_real_type(Dwarf_Die *vr_die, Dwarf_Die *die_mem)
{
	do {
		vr_die = __die_get_real_type(vr_die, die_mem);
	} while (vr_die && dwarf_tag(vr_die) == DW_TAG_typedef);

	return vr_die;
}

static int die_get_attr_udata(Dwarf_Die *tp_die, unsigned int attr_name,
			      Dwarf_Word *result)
{
	Dwarf_Attribute attr;

	if (dwarf_attr(tp_die, attr_name, &attr) == NULL ||
	    dwarf_formudata(&attr, result) != 0)
		return -ENOENT;

	return 0;
}

static int die_get_attr_sdata(Dwarf_Die *tp_die, unsigned int attr_name,
			      Dwarf_Sword *result)
{
	Dwarf_Attribute attr;

	if (dwarf_attr(tp_die, attr_name, &attr) == NULL ||
	    dwarf_formsdata(&attr, result) != 0)
		return -ENOENT;

	return 0;
}

bool die_is_signed_type(Dwarf_Die *tp_die)
{
	Dwarf_Word ret;

	if (die_get_attr_udata(tp_die, DW_AT_encoding, &ret))
		return false;

	return (ret == DW_ATE_signed_char || ret == DW_ATE_signed ||
		ret == DW_ATE_signed_fixed);
}

int die_get_data_member_location(Dwarf_Die *mb_die, Dwarf_Word *offs)
{
	Dwarf_Attribute attr;
	Dwarf_Op *expr;
	size_t nexpr;
	int ret;

	if (dwarf_attr(mb_die, DW_AT_data_member_location, &attr) == NULL)
		return -ENOENT;

	if (dwarf_formudata(&attr, offs) != 0) {
		
		ret = dwarf_getlocation(&attr, &expr, &nexpr);
		if (ret < 0 || nexpr == 0)
			return -ENOENT;

		if (expr[0].atom != DW_OP_plus_uconst || nexpr != 1) {
			pr_debug("Unable to get offset:Unexpected OP %x (%zd)\n",
				 expr[0].atom, nexpr);
			return -ENOTSUP;
		}
		*offs = (Dwarf_Word)expr[0].number;
	}
	return 0;
}

static int die_get_call_fileno(Dwarf_Die *in_die)
{
	Dwarf_Sword idx;

	if (die_get_attr_sdata(in_die, DW_AT_call_file, &idx) == 0)
		return (int)idx;
	else
		return -ENOENT;
}

static int die_get_decl_fileno(Dwarf_Die *pdie)
{
	Dwarf_Sword idx;

	if (die_get_attr_sdata(pdie, DW_AT_decl_file, &idx) == 0)
		return (int)idx;
	else
		return -ENOENT;
}

const char *die_get_call_file(Dwarf_Die *in_die)
{
	Dwarf_Die cu_die;
	Dwarf_Files *files;
	int idx;

	idx = die_get_call_fileno(in_die);
	if (idx < 0 || !dwarf_diecu(in_die, &cu_die, NULL, NULL) ||
	    dwarf_getsrcfiles(&cu_die, &files, NULL) != 0)
		return NULL;

	return dwarf_filesrc(files, idx, NULL, NULL);
}


Dwarf_Die *die_find_child(Dwarf_Die *rt_die,
			  int (*callback)(Dwarf_Die *, void *),
			  void *data, Dwarf_Die *die_mem)
{
	Dwarf_Die child_die;
	int ret;

	ret = dwarf_child(rt_die, die_mem);
	if (ret != 0)
		return NULL;

	do {
		ret = callback(die_mem, data);
		if (ret == DIE_FIND_CB_END)
			return die_mem;

		if ((ret & DIE_FIND_CB_CHILD) &&
		    die_find_child(die_mem, callback, data, &child_die)) {
			memcpy(die_mem, &child_die, sizeof(Dwarf_Die));
			return die_mem;
		}
	} while ((ret & DIE_FIND_CB_SIBLING) &&
		 dwarf_siblingof(die_mem, die_mem) == 0);

	return NULL;
}

struct __addr_die_search_param {
	Dwarf_Addr	addr;
	Dwarf_Die	*die_mem;
};

static int __die_search_func_cb(Dwarf_Die *fn_die, void *data)
{
	struct __addr_die_search_param *ad = data;

	if (dwarf_tag(fn_die) == DW_TAG_subprogram &&
	    dwarf_haspc(fn_die, ad->addr)) {
		memcpy(ad->die_mem, fn_die, sizeof(Dwarf_Die));
		return DWARF_CB_ABORT;
	}
	return DWARF_CB_OK;
}

Dwarf_Die *die_find_realfunc(Dwarf_Die *cu_die, Dwarf_Addr addr,
				    Dwarf_Die *die_mem)
{
	struct __addr_die_search_param ad;
	ad.addr = addr;
	ad.die_mem = die_mem;
	
	if (!dwarf_getfuncs(cu_die, __die_search_func_cb, &ad, 0))
		return NULL;
	else
		return die_mem;
}

static int __die_find_inline_cb(Dwarf_Die *die_mem, void *data)
{
	Dwarf_Addr *addr = data;

	if (dwarf_tag(die_mem) == DW_TAG_inlined_subroutine &&
	    dwarf_haspc(die_mem, *addr))
		return DIE_FIND_CB_END;

	return DIE_FIND_CB_CONTINUE;
}

Dwarf_Die *die_find_inlinefunc(Dwarf_Die *sp_die, Dwarf_Addr addr,
			       Dwarf_Die *die_mem)
{
	Dwarf_Die tmp_die;

	sp_die = die_find_child(sp_die, __die_find_inline_cb, &addr, &tmp_die);
	if (!sp_die)
		return NULL;

	
	while (sp_die) {
		memcpy(die_mem, sp_die, sizeof(Dwarf_Die));
		sp_die = die_find_child(sp_die, __die_find_inline_cb, &addr,
					&tmp_die);
	}

	return die_mem;
}

struct __instance_walk_param {
	void    *addr;
	int	(*callback)(Dwarf_Die *, void *);
	void    *data;
	int	retval;
};

static int __die_walk_instances_cb(Dwarf_Die *inst, void *data)
{
	struct __instance_walk_param *iwp = data;
	Dwarf_Attribute attr_mem;
	Dwarf_Die origin_mem;
	Dwarf_Attribute *attr;
	Dwarf_Die *origin;
	int tmp;

	attr = dwarf_attr(inst, DW_AT_abstract_origin, &attr_mem);
	if (attr == NULL)
		return DIE_FIND_CB_CONTINUE;

	origin = dwarf_formref_die(attr, &origin_mem);
	if (origin == NULL || origin->addr != iwp->addr)
		return DIE_FIND_CB_CONTINUE;

	
	if (dwarf_tag(inst) == DW_TAG_inlined_subroutine) {
		dwarf_decl_line(origin, &tmp);
		if (die_get_call_lineno(inst) == tmp) {
			tmp = die_get_decl_fileno(origin);
			if (die_get_call_fileno(inst) == tmp)
				return DIE_FIND_CB_CONTINUE;
		}
	}

	iwp->retval = iwp->callback(inst, iwp->data);

	return (iwp->retval) ? DIE_FIND_CB_END : DIE_FIND_CB_CONTINUE;
}

int die_walk_instances(Dwarf_Die *or_die, int (*callback)(Dwarf_Die *, void *),
		       void *data)
{
	Dwarf_Die cu_die;
	Dwarf_Die die_mem;
	struct __instance_walk_param iwp = {
		.addr = or_die->addr,
		.callback = callback,
		.data = data,
		.retval = -ENOENT,
	};

	if (dwarf_diecu(or_die, &cu_die, NULL, NULL) == NULL)
		return -ENOENT;

	die_find_child(&cu_die, __die_walk_instances_cb, &iwp, &die_mem);

	return iwp.retval;
}

struct __line_walk_param {
	bool recursive;
	line_walk_callback_t callback;
	void *data;
	int retval;
};

static int __die_walk_funclines_cb(Dwarf_Die *in_die, void *data)
{
	struct __line_walk_param *lw = data;
	Dwarf_Addr addr = 0;
	const char *fname;
	int lineno;

	if (dwarf_tag(in_die) == DW_TAG_inlined_subroutine) {
		fname = die_get_call_file(in_die);
		lineno = die_get_call_lineno(in_die);
		if (fname && lineno > 0 && dwarf_entrypc(in_die, &addr) == 0) {
			lw->retval = lw->callback(fname, lineno, addr, lw->data);
			if (lw->retval != 0)
				return DIE_FIND_CB_END;
		}
	}
	if (!lw->recursive)
		
		return DIE_FIND_CB_SIBLING;

	if (addr) {
		fname = dwarf_decl_file(in_die);
		if (fname && dwarf_decl_line(in_die, &lineno) == 0) {
			lw->retval = lw->callback(fname, lineno, addr, lw->data);
			if (lw->retval != 0)
				return DIE_FIND_CB_END;
		}
	}

	
	return DIE_FIND_CB_CONTINUE;
}

static int __die_walk_funclines(Dwarf_Die *sp_die, bool recursive,
				line_walk_callback_t callback, void *data)
{
	struct __line_walk_param lw = {
		.recursive = recursive,
		.callback = callback,
		.data = data,
		.retval = 0,
	};
	Dwarf_Die die_mem;
	Dwarf_Addr addr;
	const char *fname;
	int lineno;

	
	fname = dwarf_decl_file(sp_die);
	if (fname && dwarf_decl_line(sp_die, &lineno) == 0 &&
	    dwarf_entrypc(sp_die, &addr) == 0) {
		lw.retval = callback(fname, lineno, addr, data);
		if (lw.retval != 0)
			goto done;
	}
	die_find_child(sp_die, __die_walk_funclines_cb, &lw, &die_mem);
done:
	return lw.retval;
}

static int __die_walk_culines_cb(Dwarf_Die *sp_die, void *data)
{
	struct __line_walk_param *lw = data;

	lw->retval = __die_walk_funclines(sp_die, true, lw->callback, lw->data);
	if (lw->retval != 0)
		return DWARF_CB_ABORT;

	return DWARF_CB_OK;
}

int die_walk_lines(Dwarf_Die *rt_die, line_walk_callback_t callback, void *data)
{
	Dwarf_Lines *lines;
	Dwarf_Line *line;
	Dwarf_Addr addr;
	const char *fname;
	int lineno, ret = 0;
	Dwarf_Die die_mem, *cu_die;
	size_t nlines, i;

	
	if (dwarf_tag(rt_die) != DW_TAG_compile_unit)
		cu_die = dwarf_diecu(rt_die, &die_mem, NULL, NULL);
	else
		cu_die = rt_die;
	if (!cu_die) {
		pr_debug2("Failed to get CU from given DIE.\n");
		return -EINVAL;
	}

	
	if (dwarf_getsrclines(cu_die, &lines, &nlines) != 0) {
		pr_debug2("Failed to get source lines on this CU.\n");
		return -ENOENT;
	}
	pr_debug2("Get %zd lines from this CU\n", nlines);

	
	for (i = 0; i < nlines; i++) {
		line = dwarf_onesrcline(lines, i);
		if (line == NULL ||
		    dwarf_lineno(line, &lineno) != 0 ||
		    dwarf_lineaddr(line, &addr) != 0) {
			pr_debug2("Failed to get line info. "
				  "Possible error in debuginfo.\n");
			continue;
		}
		
		if (rt_die != cu_die)
			if (!dwarf_haspc(rt_die, addr) ||
			    die_find_inlinefunc(rt_die, addr, &die_mem))
				continue;
		
		fname = dwarf_linesrc(line, NULL, NULL);

		ret = callback(fname, lineno, addr, data);
		if (ret != 0)
			return ret;
	}

	if (rt_die != cu_die)
		ret = __die_walk_funclines(rt_die, false, callback, data);
	else {
		struct __line_walk_param param = {
			.callback = callback,
			.data = data,
			.retval = 0,
		};
		dwarf_getfuncs(cu_die, __die_walk_culines_cb, &param, 0);
		ret = param.retval;
	}

	return ret;
}

struct __find_variable_param {
	const char *name;
	Dwarf_Addr addr;
};

static int __die_find_variable_cb(Dwarf_Die *die_mem, void *data)
{
	struct __find_variable_param *fvp = data;
	int tag;

	tag = dwarf_tag(die_mem);
	if ((tag == DW_TAG_formal_parameter ||
	     tag == DW_TAG_variable) &&
	    die_compare_name(die_mem, fvp->name))
		return DIE_FIND_CB_END;

	if (dwarf_haspc(die_mem, fvp->addr))
		return DIE_FIND_CB_CONTINUE;
	else
		return DIE_FIND_CB_SIBLING;
}

Dwarf_Die *die_find_variable_at(Dwarf_Die *sp_die, const char *name,
				Dwarf_Addr addr, Dwarf_Die *die_mem)
{
	struct __find_variable_param fvp = { .name = name, .addr = addr};

	return die_find_child(sp_die, __die_find_variable_cb, (void *)&fvp,
			      die_mem);
}

static int __die_find_member_cb(Dwarf_Die *die_mem, void *data)
{
	const char *name = data;

	if ((dwarf_tag(die_mem) == DW_TAG_member) &&
	    die_compare_name(die_mem, name))
		return DIE_FIND_CB_END;

	return DIE_FIND_CB_SIBLING;
}

Dwarf_Die *die_find_member(Dwarf_Die *st_die, const char *name,
			   Dwarf_Die *die_mem)
{
	return die_find_child(st_die, __die_find_member_cb, (void *)name,
			      die_mem);
}

int die_get_typename(Dwarf_Die *vr_die, char *buf, int len)
{
	Dwarf_Die type;
	int tag, ret, ret2;
	const char *tmp = "";

	if (__die_get_real_type(vr_die, &type) == NULL)
		return -ENOENT;

	tag = dwarf_tag(&type);
	if (tag == DW_TAG_array_type || tag == DW_TAG_pointer_type)
		tmp = "*";
	else if (tag == DW_TAG_subroutine_type) {
		
		ret = snprintf(buf, len, "(function_type)");
		return (ret >= len) ? -E2BIG : ret;
	} else {
		if (!dwarf_diename(&type))
			return -ENOENT;
		if (tag == DW_TAG_union_type)
			tmp = "union ";
		else if (tag == DW_TAG_structure_type)
			tmp = "struct ";
		
		ret = snprintf(buf, len, "%s%s", tmp, dwarf_diename(&type));
		return (ret >= len) ? -E2BIG : ret;
	}
	ret = die_get_typename(&type, buf, len);
	if (ret > 0) {
		ret2 = snprintf(buf + ret, len - ret, "%s", tmp);
		ret = (ret2 >= len - ret) ? -E2BIG : ret2 + ret;
	}
	return ret;
}

int die_get_varname(Dwarf_Die *vr_die, char *buf, int len)
{
	int ret, ret2;

	ret = die_get_typename(vr_die, buf, len);
	if (ret < 0) {
		pr_debug("Failed to get type, make it unknown.\n");
		ret = snprintf(buf, len, "(unknown_type)");
	}
	if (ret > 0) {
		ret2 = snprintf(buf + ret, len - ret, "\t%s",
				dwarf_diename(vr_die));
		ret = (ret2 >= len - ret) ? -E2BIG : ret2 + ret;
	}
	return ret;
}

