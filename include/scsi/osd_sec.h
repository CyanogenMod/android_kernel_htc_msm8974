/*
 * osd_sec.h - OSD security manager API
 *
 * Copyright (C) 2008 Panasas Inc.  All rights reserved.
 *
 * Authors:
 *   Boaz Harrosh <bharrosh@panasas.com>
 *   Benny Halevy <bhalevy@panasas.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 *
 */
#ifndef __OSD_SEC_H__
#define __OSD_SEC_H__

#include "osd_protocol.h"
#include "osd_types.h"

void osd_sec_encode_caps(void *caps, ...);
void osd_sec_init_nosec_doall_caps(void *caps,
	const struct osd_obj_id *obj, bool is_collection, const bool is_v1);

bool osd_is_sec_alldata(struct osd_security_parameters *sec_params);

void osd_sec_sign_cdb(struct osd_cdb *ocdb, const u8 *cap_key);

void osd_sec_sign_data(void *data_integ, struct bio *bio, const u8 *cap_key);

void osd_set_caps(struct osd_cdb *cdb, const void *caps);

#endif 
