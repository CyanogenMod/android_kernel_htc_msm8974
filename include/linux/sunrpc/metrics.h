/*
 *  linux/include/linux/sunrpc/metrics.h
 *
 *  Declarations for RPC client per-operation metrics
 *
 *  Copyright (C) 2005	Chuck Lever <cel@netapp.com>
 *
 *  RPC client per-operation statistics provide latency and retry
 *  information about each type of RPC procedure in a given RPC program.
 *  These statistics are not for detailed problem diagnosis, but simply
 *  to indicate whether the problem is local or remote.
 *
 *  These counters are not meant to be human-readable, but are meant to be
 *  integrated into system monitoring tools such as "sar" and "iostat".  As
 *  such, the counters are sampled by the tools over time, and are never
 *  zeroed after a file system is mounted.  Moving averages can be computed
 *  by the tools by taking the difference between two instantaneous samples
 *  and dividing that by the time between the samples.
 *
 *  The counters are maintained in a single array per RPC client, indexed
 *  by procedure number.  There is no need to maintain separate counter
 *  arrays per-CPU because these counters are always modified behind locks.
 */

#ifndef _LINUX_SUNRPC_METRICS_H
#define _LINUX_SUNRPC_METRICS_H

#include <linux/seq_file.h>
#include <linux/ktime.h>

#define RPC_IOSTATS_VERS	"1.0"

struct rpc_iostats {
	unsigned long		om_ops,		
				om_ntrans,	
				om_timeouts;	

	unsigned long long      om_bytes_sent,	
				om_bytes_recv;	

	ktime_t			om_queue,	
				om_rtt,		
				om_execute;	
} ____cacheline_aligned;

struct rpc_task;
struct rpc_clnt;


#ifdef CONFIG_PROC_FS

struct rpc_iostats *	rpc_alloc_iostats(struct rpc_clnt *);
void			rpc_count_iostats(const struct rpc_task *,
					  struct rpc_iostats *);
void			rpc_print_iostats(struct seq_file *, struct rpc_clnt *);
void			rpc_free_iostats(struct rpc_iostats *);

#else  

static inline struct rpc_iostats *rpc_alloc_iostats(struct rpc_clnt *clnt) { return NULL; }
static inline void rpc_count_iostats(const struct rpc_task *task,
				     struct rpc_iostats *stats) {}
static inline void rpc_print_iostats(struct seq_file *seq, struct rpc_clnt *clnt) {}
static inline void rpc_free_iostats(struct rpc_iostats *stats) {}

#endif  

#endif 
