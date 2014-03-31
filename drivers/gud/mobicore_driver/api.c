/* MobiCore driver module.(interface to the secure world SWD)
 * MobiCore Driver Kernel Module.
 *
 * <-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 * <-- Copyright Trustonic Limited 2013 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>

#include "main.h"
#include "mem.h"
#include "debug.h"


int mobicore_map_vmem(struct mc_instance *instance, void *addr,
	uint32_t len, uint32_t *handle, uint32_t *phys)
{
	return mc_register_wsm_l2(instance, (uint32_t)addr, len,
		handle, phys);
}
EXPORT_SYMBOL(mobicore_map_vmem);

int mobicore_unmap_vmem(struct mc_instance *instance, uint32_t handle)
{
	return mc_unregister_wsm_l2(instance, handle);
}
EXPORT_SYMBOL(mobicore_unmap_vmem);

int mobicore_free_wsm(struct mc_instance *instance, uint32_t handle)
{
	return mc_free_buffer(instance, handle);
}
EXPORT_SYMBOL(mobicore_free_wsm);


int mobicore_allocate_wsm(struct mc_instance *instance,
	unsigned long requested_size, uint32_t *handle, void **virt_kernel_addr,
	void **phys_addr)
{
	struct mc_buffer *buffer = NULL;

	
	if (mc_get_buffer(instance, &buffer, requested_size))
		return -EFAULT;

	*handle = buffer->handle;
	*phys_addr = buffer->phys;
	*virt_kernel_addr = buffer->addr;
	return 0;
}
EXPORT_SYMBOL(mobicore_allocate_wsm);

struct mc_instance *mobicore_open(void)
{
	struct mc_instance *instance = mc_alloc_instance();
	if(instance) {
		instance->admin = true;
	}
	return instance;
}
EXPORT_SYMBOL(mobicore_open);

int mobicore_release(struct mc_instance *instance)
{
	return mc_release_instance(instance);
}
EXPORT_SYMBOL(mobicore_release);

