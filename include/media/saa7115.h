/*
    saa7115.h - definition for saa7111/3/4/5 inputs and frequency flags

    Copyright (C) 2006 Hans Verkuil (hverkuil@xs4all.nl)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef _SAA7115_H_
#define _SAA7115_H_

#define SAA7115_COMPOSITE0 0
#define SAA7115_COMPOSITE1 1
#define SAA7115_COMPOSITE2 2
#define SAA7115_COMPOSITE3 3
#define SAA7115_COMPOSITE4 4 
#define SAA7115_COMPOSITE5 5 
#define SAA7115_SVIDEO0    6
#define SAA7115_SVIDEO1    7
#define SAA7115_SVIDEO2    8
#define SAA7115_SVIDEO3    9

#define SAA7115_FREQ_32_11_MHZ  32110000   
#define SAA7115_FREQ_24_576_MHZ 24576000   

#define SAA7115_FREQ_FL_UCGC   (1 << 0)	   
#define SAA7115_FREQ_FL_CGCDIV (1 << 1)	   
#define SAA7115_FREQ_FL_APLL   (1 << 2)	   

#define SAA7115_IPORT_ON    	1
#define SAA7115_IPORT_OFF   	0

#define SAA7111_VBI_BYPASS 	2
#define SAA7111_FMT_YUV422      0x00
#define SAA7111_FMT_RGB 	0x40
#define SAA7111_FMT_CCIR 	0x80
#define SAA7111_FMT_YUV411 	0xc0

#endif

