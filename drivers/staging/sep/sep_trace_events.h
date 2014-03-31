#undef TRACE_SYSTEM
#define TRACE_SYSTEM sep

#if !defined(_TRACE_SEP_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SEP_EVENTS_H

#ifdef SEP_PERF_DEBUG
#define SEP_TRACE_FUNC_IN() trace_sep_func_start(__func__, 0)
#define SEP_TRACE_FUNC_OUT(branch) trace_sep_func_end(__func__, branch)
#define SEP_TRACE_EVENT(branch) trace_sep_misc_event(__func__, branch)
#else
#define SEP_TRACE_FUNC_IN()
#define SEP_TRACE_FUNC_OUT(branch)
#define SEP_TRACE_EVENT(branch)
#endif


#include <linux/tracepoint.h>

TRACE_EVENT(sep_func_start,

	TP_PROTO(const char *name, int branch),

	TP_ARGS(name, branch),

	TP_STRUCT__entry(
		__array(char,	name,    20)
		__field(int,	branch)
	),

	TP_fast_assign(
		strncpy(__entry->name, name, 20);
		__entry->branch	= branch;
	),

	TP_printk("func_start %s %d", __entry->name, __entry->branch)
);

TRACE_EVENT(sep_func_end,

	TP_PROTO(const char *name, int branch),

	TP_ARGS(name, branch),

	TP_STRUCT__entry(
		__array(char,	name,    20)
		__field(int,	branch)
	),

	TP_fast_assign(
		strncpy(__entry->name, name, 20);
		__entry->branch	= branch;
	),

	TP_printk("func_end %s %d", __entry->name, __entry->branch)
);

TRACE_EVENT(sep_misc_event,

	TP_PROTO(const char *name, int branch),

	TP_ARGS(name, branch),

	TP_STRUCT__entry(
		__array(char,	name,    20)
		__field(int,	branch)
	),

	TP_fast_assign(
		strncpy(__entry->name, name, 20);
		__entry->branch	= branch;
	),

	TP_printk("misc_event %s %d", __entry->name, __entry->branch)
);


#endif



#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE sep_trace_events
#include <trace/define_trace.h>
