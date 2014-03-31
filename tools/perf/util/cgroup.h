#ifndef __CGROUP_H__
#define __CGROUP_H__

struct option;

struct cgroup_sel {
	char *name;
	int fd;
	int refcnt;
};


extern int nr_cgroups; 
extern void close_cgroup(struct cgroup_sel *cgrp);
extern int parse_cgroups(const struct option *opt, const char *str, int unset);

#endif 
