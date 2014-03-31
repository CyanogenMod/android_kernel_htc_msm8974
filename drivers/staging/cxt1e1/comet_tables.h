#ifndef _INC_COMET_TBLS_H_
#define _INC_COMET_TBLS_H_

/*-----------------------------------------------------------------------------
 * comet_tables.h - Waveform Tables for the PM4351 'COMET'
 *
 * Copyright (C) 2005  SBE, Inc.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 * For further information, contact via email: support@sbei.com
 * SBE, Inc.  San Ramon, California  U.S.A.
 *-----------------------------------------------------------------------------
 */



extern u_int8_t TWVLongHaul0DB[25][5];      
extern u_int8_t TWVLongHaul7_5DB[25][5];    
extern u_int8_t TWVLongHaul15DB[25][5];     
extern u_int8_t TWVLongHaul22_5DB[25][5];   
extern u_int8_t TWVShortHaul0[25][5];       
extern u_int8_t TWVShortHaul1[25][5];       
extern u_int8_t TWVShortHaul2[25][5];       
extern u_int8_t TWVShortHaul3[25][5];       
extern u_int8_t TWVShortHaul4[25][5];       
extern u_int8_t TWVShortHaul5[25][5];       
extern u_int8_t TWV_E1_75Ohm[25][5];        
extern u_int8_t TWV_E1_120Ohm[25][5];       
extern u_int32_t T1_Equalizer[256];    
extern u_int32_t E1_Equalizer[256];    

#endif                          
