#ifndef _CPU_DEVICE_ID
#define _CPU_DEVICE_ID 1


#include <linux/mod_devicetable.h>

extern const struct x86_cpu_id *x86_match_cpu(const struct x86_cpu_id *match);

#endif
