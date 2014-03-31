#undef TRACE_SYSTEM
#define TRACE_SYSTEM sample

#if !defined(_TRACE_EVENT_SAMPLE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVENT_SAMPLE_H

#include <linux/tracepoint.h>

TRACE_EVENT(foo_bar,

	TP_PROTO(char *foo, int bar),

	TP_ARGS(foo, bar),

	TP_STRUCT__entry(
		__array(	char,	foo,    10		)
		__field(	int,	bar			)
	),

	TP_fast_assign(
		strncpy(__entry->foo, foo, 10);
		__entry->bar	= bar;
	),

	TP_printk("foo %s %d", __entry->foo, __entry->bar)
);
#endif



#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE trace-events-sample
#include <trace/define_trace.h>
