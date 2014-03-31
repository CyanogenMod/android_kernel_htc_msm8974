#ifndef _PROBE_FINDER_H
#define _PROBE_FINDER_H

#include <stdbool.h>
#include "util.h"
#include "probe-event.h"

#define MAX_PROBE_BUFFER	1024
#define MAX_PROBES		 128

static inline int is_c_varname(const char *name)
{
	
	return isalpha(name[0]) || name[0] == '_';
}

#ifdef DWARF_SUPPORT

#include "dwarf-aux.h"


struct debuginfo {
	Dwarf		*dbg;
	Dwfl		*dwfl;
	Dwarf_Addr	bias;
};

extern struct debuginfo *debuginfo__new(const char *path);
extern struct debuginfo *debuginfo__new_online_kernel(unsigned long addr);
extern void debuginfo__delete(struct debuginfo *self);

extern int debuginfo__find_trace_events(struct debuginfo *self,
					struct perf_probe_event *pev,
					struct probe_trace_event **tevs,
					int max_tevs);

extern int debuginfo__find_probe_point(struct debuginfo *self,
				       unsigned long addr,
				       struct perf_probe_point *ppt);

extern int debuginfo__find_line_range(struct debuginfo *self,
				      struct line_range *lr);

extern int debuginfo__find_available_vars_at(struct debuginfo *self,
					     struct perf_probe_event *pev,
					     struct variable_list **vls,
					     int max_points, bool externs);

struct probe_finder {
	struct perf_probe_event	*pev;		

	
	int (*callback)(Dwarf_Die *sc_die, struct probe_finder *pf);

	
	int			lno;		
	Dwarf_Addr		addr;		
	const char		*fname;		
	Dwarf_Die		cu_die;		
	Dwarf_Die		sp_die;
	struct list_head	lcache;		

	
#if _ELFUTILS_PREREQ(0, 142)
	Dwarf_CFI		*cfi;		
#endif
	Dwarf_Op		*fb_ops;	
	struct perf_probe_arg	*pvar;		
	struct probe_trace_arg	*tvar;		
};

struct trace_event_finder {
	struct probe_finder	pf;
	struct probe_trace_event *tevs;		
	int			ntevs;		
	int			max_tevs;	
};

struct available_var_finder {
	struct probe_finder	pf;
	struct variable_list	*vls;		
	int			nvls;		
	int			max_vls;	
	bool			externs;	
	bool			child;		
};

struct line_finder {
	struct line_range	*lr;		

	const char		*fname;		
	int			lno_s;		
	int			lno_e;		
	Dwarf_Die		cu_die;		
	Dwarf_Die		sp_die;
	int			found;
};

#endif 

#endif 
