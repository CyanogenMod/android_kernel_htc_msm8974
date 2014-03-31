/*
 * osd.h
 *
 * Copyright (C) 2001 Ralph  Metzler <ralph@convergence.de>
 *                  & Marcus Metzler <marcus@convergence.de>
 *                    for convergence integrated media GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Lesser Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef _DVBOSD_H_
#define _DVBOSD_H_

#include <linux/compiler.h>

typedef enum {
  
  OSD_Close=1,    
  
  
  OSD_Open,       
  
  
  OSD_Show,       
  
  
  OSD_Hide,       
  
  
  OSD_Clear,      
  
  
  OSD_Fill,       
  
  
  OSD_SetColor,   
  
  
  
  
  
  
  
  OSD_SetPalette, 
  
  
  
  
  OSD_SetTrans,   
  
  
  OSD_SetPixel,   
  
  
  OSD_GetPixel,   
  
  OSD_SetRow,     
  
  
  OSD_SetBlock,   
  
  
  
  
  OSD_FillRow,    
  
  
  OSD_FillBlock,  
  
  
  OSD_Line,       
  
  
  OSD_Query,      
  
  
  OSD_Test,       
  
  
  OSD_Text,       
  OSD_SetWindow, 
  OSD_MoveWindow, 
  OSD_OpenRaw,	
} OSD_Command;

typedef struct osd_cmd_s {
	OSD_Command cmd;
	int x0;
	int y0;
	int x1;
	int y1;
	int color;
	void __user *data;
} osd_cmd_t;

typedef enum {
	OSD_BITMAP1,           
	OSD_BITMAP2,           
	OSD_BITMAP4,           
	OSD_BITMAP8,           
	OSD_BITMAP1HR,         
	OSD_BITMAP2HR,         
	OSD_BITMAP4HR,         
	OSD_BITMAP8HR,         
	OSD_YCRCB422,          
	OSD_YCRCB444,          
	OSD_YCRCB444HR,        
	OSD_VIDEOTSIZE,        
	OSD_VIDEOHSIZE,        
	OSD_VIDEOQSIZE,        
	OSD_VIDEODSIZE,        
	OSD_VIDEOTHSIZE,       
	OSD_VIDEOTQSIZE,       
	OSD_VIDEOTDSIZE,       
	OSD_VIDEONSIZE,        
	OSD_CURSOR             
} osd_raw_window_t;

typedef struct osd_cap_s {
	int  cmd;
#define OSD_CAP_MEMSIZE         1  
	long val;
} osd_cap_t;


#define OSD_SEND_CMD            _IOW('o', 160, osd_cmd_t)
#define OSD_GET_CAPABILITY      _IOR('o', 161, osd_cap_t)

#endif
