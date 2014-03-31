#ifndef _INC_SBE_PROMFORMAT_H_
#define _INC_SBE_PROMFORMAT_H_

/*-----------------------------------------------------------------------------
 * sbe_promformat.h - Contents of seeprom used by dvt and manufacturing tests
 *
 * Copyright (C) 2002-2005  SBE, Inc.
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
 *
 *-----------------------------------------------------------------------------
 */





#define STRUCT_OFFSET(type, symbol)  ((long)&(((type *)0)->symbol))

#define PROM_FORMAT_Unk   (-1)
#define PROM_FORMAT_TYPE1   1
#define PROM_FORMAT_TYPE2   2


    typedef struct
    {
        char        type;       
        char        Id[2];      
        char        SubId[2];   
        char        Serial[6];  
        char        CreateTime[4];      
        char        HeatRunTime[4];     
        char        HeatRunIterations[4];       
        char        HeatRunErrors[4];   
        char        Crc32[4];   
    }           FLD_TYPE1;


    typedef struct
    {
        char        type;       
        char        length[2];  
        char        Crc32[4];   
        char        Id[2];      
        char        SubId[2];   
        char        Serial[6];  
        char        CreateTime[4];      
        char        HeatRunTime[4];     
        char        HeatRunIterations[4];       
        char        HeatRunErrors[4];   
    }           FLD_TYPE2;




#define SBE_EEPROM_SIZE    128
#define SBE_MFG_INFO_SIZE  sizeof(FLD_TYPE2)

    typedef union
    {
        char        bytes[128];
        FLD_TYPE1   fldType1;
        FLD_TYPE2   fldType2;
    }           PROMFORMAT;

#endif                          
