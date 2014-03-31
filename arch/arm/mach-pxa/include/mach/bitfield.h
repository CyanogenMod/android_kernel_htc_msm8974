/*
 *	FILE    	bitfield.h
 *
 *	Version 	1.1
 *	Author  	Copyright (c) Marc A. Viredaz, 1998
 *	        	DEC Western Research Laboratory, Palo Alto, CA
 *	Date    	April 1998 (April 1997)
 *	System  	Advanced RISC Machine (ARM)
 *	Language	C or ARM Assembly
 *	Purpose 	Definition of macros to operate on bit fields.
 */



#ifndef __BITFIELD_H
#define __BITFIELD_H

#ifndef __ASSEMBLY__
#define UData(Data)	((unsigned long) (Data))
#else
#define UData(Data)	(Data)
#endif



#define Fld(Size, Shft)	(((Size) << 16) + (Shft))



#define FSize(Field)	((Field) >> 16)
#define FShft(Field)	((Field) & 0x0000FFFF)
#define FMsk(Field)	(((UData (1) << FSize (Field)) - 1) << FShft (Field))
#define FAlnMsk(Field)	((UData (1) << FSize (Field)) - 1)
#define F1stBit(Field)	(UData (1) << FShft (Field))



#define FInsrt(Value, Field) \
                	(UData (Value) << FShft (Field))



#define FExtr(Data, Field) \
                	((UData (Data) >> FShft (Field)) & FAlnMsk (Field))


#endif 
