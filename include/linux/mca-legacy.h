

#ifndef _LINUX_MCA_LEGACY_H
#define _LINUX_MCA_LEGACY_H

#include <linux/mca.h>

#warning "MCA legacy - please move your driver to the new sysfs api"

#define MCA_NOTFOUND	(-1)



extern int mca_find_adapter(int id, int start);
extern int mca_find_unused_adapter(int id, int start);

extern int mca_mark_as_used(int slot);
extern void mca_mark_as_unused(int slot);

extern unsigned char mca_read_stored_pos(int slot, int reg);

extern void mca_set_adapter_name(int slot, char* name);


extern unsigned char mca_read_pos(int slot, int reg);

extern void mca_write_pos(int slot, int reg, unsigned char byte);

#endif
