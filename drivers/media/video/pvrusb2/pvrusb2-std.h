/*
 *
 *
 *  Copyright (C) 2005 Mike Isely <isely@pobox.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef __PVRUSB2_STD_H
#define __PVRUSB2_STD_H

#include <linux/videodev2.h>

int pvr2_std_str_to_id(v4l2_std_id *idPtr,const char *bufPtr,
		       unsigned int bufSize);

unsigned int pvr2_std_id_to_str(char *bufPtr, unsigned int bufSize,
				v4l2_std_id id);

struct v4l2_standard *pvr2_std_create_enum(unsigned int *countptr,
					   v4l2_std_id id);

v4l2_std_id pvr2_std_get_usable(void);

#endif 

