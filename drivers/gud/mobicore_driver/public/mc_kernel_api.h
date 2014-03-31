/*
 * Interface to be used by module MobiCoreKernelAPI.
 *
 * <-- Copyright Giesecke & Devrient GmbH 2010-2012 -->
 * <-- Copyright Trustonic Limited 2013 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MC_KERNEL_API_H_
#define _MC_KERNEL_API_H_

struct mc_instance;

struct mc_instance *mobicore_open(void);

int mobicore_release(struct mc_instance *instance);

int mobicore_allocate_wsm(struct mc_instance *instance,
			  unsigned long requested_size, uint32_t *handle,
			  void **virt_kernel_addr, void **phys_addr);

int mobicore_free_wsm(struct mc_instance *instance, uint32_t handle);

int mobicore_map_vmem(struct mc_instance *instance, void *addr,
		      uint32_t len, uint32_t *handle, uint32_t *phys);

int mobicore_unmap_vmem(struct mc_instance *instance, uint32_t handle);

#endif 
