/*
 *    Filename: cfag12864b.h
 *     Version: 0.1.0
 * Description: cfag12864b LCD driver header
 *     License: GPLv2
 *
 *      Author: Copyright (C) Miguel Ojeda Sandonis
 *        Date: 2006-10-12
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

#ifndef _CFAG12864B_H_
#define _CFAG12864B_H_

#define CFAG12864B_WIDTH	(128)
#define CFAG12864B_HEIGHT	(64)
#define CFAG12864B_CONTROLLERS	(2)
#define CFAG12864B_PAGES	(8)
#define CFAG12864B_ADDRESSES	(64)
#define CFAG12864B_SIZE		((CFAG12864B_CONTROLLERS) * \
				(CFAG12864B_PAGES) * \
				(CFAG12864B_ADDRESSES))

extern unsigned char * cfag12864b_buffer;

extern unsigned int cfag12864b_getrate(void);

extern unsigned char cfag12864b_enable(void);

extern void cfag12864b_disable(void);

extern unsigned char cfag12864b_isenabled(void);

extern unsigned char cfag12864b_isinited(void);

#endif 

