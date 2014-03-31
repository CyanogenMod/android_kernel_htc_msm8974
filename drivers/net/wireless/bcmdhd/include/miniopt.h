/*
 * Command line options parser.
 *
 * Copyright (C) 1999-2012, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 * $Id: miniopt.h 241182 2011-02-17 21:50:03Z $
 */


#ifndef MINI_OPT_H
#define MINI_OPT_H

#ifdef __cplusplus
extern "C" {
#endif


#define MINIOPT_MAXKEY	128	
typedef struct miniopt {

	
	const char* name;		
	const char* flags;		
	bool longflags;		
	bool opt_end;		

	

	int consumed;		
	bool positional;
	bool good_int;		
	char opt;
	char key[MINIOPT_MAXKEY];
	char* valstr;		
	uint uval;		
	int  val;		
} miniopt_t;

void miniopt_init(miniopt_t *t, const char* name, const char* flags, bool longflags);
int miniopt(miniopt_t *t, char **argv);




#ifdef __cplusplus
	}
#endif

#endif  
