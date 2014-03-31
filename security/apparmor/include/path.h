/*
 * AppArmor security module
 *
 * This file contains AppArmor basic path manipulation function definitions.
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2010 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#ifndef __AA_PATH_H
#define __AA_PATH_H


enum path_flags {
	PATH_IS_DIR = 0x1,		
	PATH_CONNECT_PATH = 0x4,	
	PATH_CHROOT_REL = 0x8,		
	PATH_CHROOT_NSCONNECT = 0x10,	

	PATH_DELEGATE_DELETED = 0x08000, 
	PATH_MEDIATE_DELETED = 0x10000,	
};

int aa_path_name(struct path *path, int flags, char **buffer,
		 const char **name, const char **info);

#endif 
