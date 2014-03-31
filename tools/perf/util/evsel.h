#ifndef __PERF_EVSEL_H
#define __PERF_EVSEL_H 1

#include <linux/list.h>
#include <stdbool.h>
#include "../../../include/linux/perf_event.h"
#include "types.h"
#include "xyarray.h"
#include "cgroup.h"
#include "hist.h"
 
struct perf_counts_values {
	union {
		struct {
			u64 val;
			u64 ena;
			u64 run;
		};
		u64 values[3];
	};
};

struct perf_counts {
	s8		   	  scaled;
	struct perf_counts_values aggr;
	struct perf_counts_values cpu[];
};

struct perf_evsel;

struct perf_sample_id {
	struct hlist_node 	node;
	u64		 	id;
	struct perf_evsel	*evsel;
};

struct perf_evsel {
	struct list_head	node;
	struct perf_event_attr	attr;
	char			*filter;
	struct xyarray		*fd;
	struct xyarray		*sample_id;
	u64			*id;
	struct perf_counts	*counts;
	int			idx;
	int			ids;
	struct hists		hists;
	char			*name;
	union {
		void		*priv;
		off_t		id_offset;
	};
	struct cgroup_sel	*cgrp;
	struct {
		void		*func;
		void		*data;
	} handler;
	bool 			supported;
};

struct cpu_map;
struct thread_map;
struct perf_evlist;
struct perf_record_opts;

struct perf_evsel *perf_evsel__new(struct perf_event_attr *attr, int idx);
void perf_evsel__init(struct perf_evsel *evsel,
		      struct perf_event_attr *attr, int idx);
void perf_evsel__exit(struct perf_evsel *evsel);
void perf_evsel__delete(struct perf_evsel *evsel);

void perf_evsel__config(struct perf_evsel *evsel,
			struct perf_record_opts *opts,
			struct perf_evsel *first);

int perf_evsel__alloc_fd(struct perf_evsel *evsel, int ncpus, int nthreads);
int perf_evsel__alloc_id(struct perf_evsel *evsel, int ncpus, int nthreads);
int perf_evsel__alloc_counts(struct perf_evsel *evsel, int ncpus);
void perf_evsel__free_fd(struct perf_evsel *evsel);
void perf_evsel__free_id(struct perf_evsel *evsel);
void perf_evsel__close_fd(struct perf_evsel *evsel, int ncpus, int nthreads);

int perf_evsel__open_per_cpu(struct perf_evsel *evsel,
			     struct cpu_map *cpus, bool group,
			     struct xyarray *group_fds);
int perf_evsel__open_per_thread(struct perf_evsel *evsel,
				struct thread_map *threads, bool group,
				struct xyarray *group_fds);
int perf_evsel__open(struct perf_evsel *evsel, struct cpu_map *cpus,
		     struct thread_map *threads, bool group,
		     struct xyarray *group_fds);
void perf_evsel__close(struct perf_evsel *evsel, int ncpus, int nthreads);

#define perf_evsel__match(evsel, t, c)		\
	(evsel->attr.type == PERF_TYPE_##t &&	\
	 evsel->attr.config == PERF_COUNT_##c)

int __perf_evsel__read_on_cpu(struct perf_evsel *evsel,
			      int cpu, int thread, bool scale);

static inline int perf_evsel__read_on_cpu(struct perf_evsel *evsel,
					  int cpu, int thread)
{
	return __perf_evsel__read_on_cpu(evsel, cpu, thread, false);
}

static inline int perf_evsel__read_on_cpu_scaled(struct perf_evsel *evsel,
						 int cpu, int thread)
{
	return __perf_evsel__read_on_cpu(evsel, cpu, thread, true);
}

int __perf_evsel__read(struct perf_evsel *evsel, int ncpus, int nthreads,
		       bool scale);

static inline int perf_evsel__read(struct perf_evsel *evsel,
				    int ncpus, int nthreads)
{
	return __perf_evsel__read(evsel, ncpus, nthreads, false);
}

static inline int perf_evsel__read_scaled(struct perf_evsel *evsel,
					  int ncpus, int nthreads)
{
	return __perf_evsel__read(evsel, ncpus, nthreads, true);
}

int __perf_evsel__sample_size(u64 sample_type);

static inline int perf_evsel__sample_size(struct perf_evsel *evsel)
{
	return __perf_evsel__sample_size(evsel->attr.sample_type);
}

void hists__init(struct hists *hists);

#endif 
