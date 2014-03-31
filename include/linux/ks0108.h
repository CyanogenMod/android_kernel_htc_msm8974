/*
 *    Filename: ks0108.h
 *     Version: 0.1.0
 * Description: ks0108 LCD Controller driver header
 *     License: GPLv2
 *
 *      Author: Copyright (C) Miguel Ojeda Sandonis
 *        Date: 2006-10-31
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

#ifndef _KS0108_H_
#define _KS0108_H_

extern void ks0108_writedata(unsigned char byte);

extern void ks0108_writecontrol(unsigned char byte);

extern void ks0108_displaystate(unsigned char state);

extern void ks0108_startline(unsigned char startline);

extern void ks0108_address(unsigned char address);

extern void ks0108_page(unsigned char page);

extern unsigned char ks0108_isinited(void);

#endif 
