#ifndef __PERF_QUOTE_H
#define __PERF_QUOTE_H

#include <stddef.h>
#include <stdio.h>


extern void sq_quote_argv(struct strbuf *, const char **argv, size_t maxlen);

#endif 
