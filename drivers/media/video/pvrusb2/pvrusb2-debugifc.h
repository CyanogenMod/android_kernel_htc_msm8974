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
#ifndef __PVRUSB2_DEBUGIFC_H
#define __PVRUSB2_DEBUGIFC_H

struct pvr2_hdw;

int pvr2_debugifc_print_info(struct pvr2_hdw *,
			     char *buf_ptr, unsigned int buf_size);

int pvr2_debugifc_print_status(struct pvr2_hdw *,
			       char *buf_ptr,unsigned int buf_size);

int pvr2_debugifc_docmd(struct pvr2_hdw *,
			const char *buf_ptr,unsigned int buf_size);

#endif 

