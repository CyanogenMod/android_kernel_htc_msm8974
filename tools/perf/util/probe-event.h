#ifndef _PROBE_EVENT_H
#define _PROBE_EVENT_H

#include <stdbool.h>
#include "strlist.h"
#include "strfilter.h"

extern bool probe_event_dry_run;

struct probe_trace_point {
	char		*symbol;	
	char		*module;	
	unsigned long	offset;		
	bool		retprobe;	
};

struct probe_trace_arg_ref {
	struct probe_trace_arg_ref	*next;	
	long				offset;	
};

struct probe_trace_arg {
	char				*name;	
	char				*value;	
	char				*type;	
	struct probe_trace_arg_ref	*ref;	
};

struct probe_trace_event {
	char				*event;	
	char				*group;	
	struct probe_trace_point	point;	
	int				nargs;	
	struct probe_trace_arg		*args;	
};

struct perf_probe_point {
	char		*file;		
	char		*function;	
	int		line;		
	bool		retprobe;	
	char		*lazy_line;	
	unsigned long	offset;		
};

struct perf_probe_arg_field {
	struct perf_probe_arg_field	*next;	
	char				*name;	
	long				index;	
	bool				ref;	
};

struct perf_probe_arg {
	char				*name;	
	char				*var;	
	char				*type;	
	struct perf_probe_arg_field	*field;	
};

struct perf_probe_event {
	char			*event;	
	char			*group;	
	struct perf_probe_point	point;	
	int			nargs;	
	struct perf_probe_arg	*args;	
};


struct line_node {
	struct list_head	list;
	int			line;
};

struct line_range {
	char			*file;		
	char			*function;	
	int			start;		
	int			end;		
	int			offset;		
	char			*path;		
	char			*comp_dir;	
	struct list_head	line_list;	
};

struct variable_list {
	struct probe_trace_point	point;	
	struct strlist			*vars;	
};

extern int parse_perf_probe_command(const char *cmd,
				    struct perf_probe_event *pev);

extern char *synthesize_perf_probe_command(struct perf_probe_event *pev);
extern char *synthesize_probe_trace_command(struct probe_trace_event *tev);
extern int synthesize_perf_probe_arg(struct perf_probe_arg *pa, char *buf,
				     size_t len);

extern bool perf_probe_event_need_dwarf(struct perf_probe_event *pev);

extern void clear_perf_probe_event(struct perf_probe_event *pev);

extern int parse_line_range_desc(const char *cmd, struct line_range *lr);

extern const char *kernel_get_module_path(const char *module);

extern int add_perf_probe_events(struct perf_probe_event *pevs, int npevs,
				 int max_probe_points, const char *module,
				 bool force_add);
extern int del_perf_probe_events(struct strlist *dellist);
extern int show_perf_probe_events(void);
extern int show_line_range(struct line_range *lr, const char *module);
extern int show_available_vars(struct perf_probe_event *pevs, int npevs,
			       int max_probe_points, const char *module,
			       struct strfilter *filter, bool externs);
extern int show_available_funcs(const char *module, struct strfilter *filter);


#define MAX_EVENT_INDEX	1024

#endif 
