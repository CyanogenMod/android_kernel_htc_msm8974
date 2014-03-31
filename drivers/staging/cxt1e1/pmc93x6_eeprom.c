/* pmc93x6_eeprom.c - PMC's 93LC46 EEPROM Device
 *
 *    The 93LC46 is a low-power, serial Electrically Erasable and
 *    Programmable Read Only Memory organized as 128 8-bit bytes.
 *
 *    Accesses to the 93LC46 are done in a bit serial stream, organized
 *    in a 3 wire format.  Writes are internally timed by the device
 *    (the In data bit is pulled low until the write is complete and
 *    then is pulled high) and take about 6 milliseconds.
 *
 * Copyright (C) 2003-2005  SBE, Inc.
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
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include "pmcc4_sysdep.h"
#include "sbecom_inline_linux.h"
#include "pmcc4.h"
#include "sbe_promformat.h"

#ifndef TRUE
#define TRUE   1
#define FALSE  0
#endif

#ifdef SBE_INCLUDE_SYMBOLS
#define STATIC
#else
#define STATIC  static
#endif



#define EE_MFG      (long)0     
#define EE_FIRST    0x28        
#define EE_LIMIT    128         



#define EPROM_EWEN      0x0019  
#define EPROM_EWDS      0x0001  
#define EPROM_READ      0x0003  
#define EPROM_WRITE     0x0005  
#define EPROM_ERASE     0x0007  
#define EPROM_ERAL      0x0009  
#define EPROM_WRAL      0x0011  

#define EPROM_ADR_SZ    7       
#define EPROM_OP_SZ     3       
#define SIZE_ADDR_OP    (EPROM_ADR_SZ + EPROM_OP_SZ)
#define LC46A_MAX_OPS   10      
#define NUM_OF_BITS     8       


#define EPROM_ACTIVE_OUT_BIT    0x0001  
#define EPROM_ACTIVE_IN_BIT     0x0002  
#define ACTIVE_IN_BIT_SHIFT     0x0001  
#define EPROM_ENCS              0x0004  



static unsigned char ByteReverse[256];
static int  ByteReverseBuilt = FALSE;



short       mfg_template[sizeof (FLD_TYPE2)] =
{
    PROM_FORMAT_TYPE2,          
    0x00, 0x1A,                 
    0x00, 0x00, 0x00, 0x00,     
    0x11, 0x76,                 
    0x07, 0x05,                 
    0x00, 0xA0, 0xD6, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00,     
    0x00, 0x00, 0x00, 0x00,     
    0x00, 0x00, 0x00, 0x00,     
    0x00, 0x00, 0x00, 0x00,     
};



STATIC void
BuildByteReverse (void)
{
    long        half;           
    int         i;

    ByteReverse[0] = 0;

    for (half = 1; half < sizeof (ByteReverse); half <<= 1)
        for (i = 0; i < half; i++)
            ByteReverse[half + i] = (char) (ByteReverse[i] | (0x80 / half));

    ByteReverseBuilt = TRUE;
}



STATIC void
eeprom_delay (void)
{
    int         timeout;

    for (timeout = 20; timeout; --timeout)
    {
        OS_uwait_dummy ();
    }
}



void
eeprom_put_byte (long addr, long data, int count)
{
    u_int32_t output;

    while (--count >= 0)
    {
        output = (data & EPROM_ACTIVE_OUT_BIT) ? 1 : 0; 
        output |= EPROM_ENCS;       
        data >>= 1;

        eeprom_delay ();
        pci_write_32 ((u_int32_t *) addr, output);      
    }
}



u_int32_t
eeprom_get_byte (long addr)
{
    u_int32_t   input;
    u_int32_t   data;
    int         count;


    input = pci_read_32 ((u_int32_t *) addr);

    data = 0;
    count = NUM_OF_BITS;
    while (--count >= 0)
    {
        eeprom_delay ();
        input = pci_read_32 ((u_int32_t *) addr);

        data <<= 1;                 
        data |= (input & EPROM_ACTIVE_IN_BIT) ? 1 : 0;

    }

    return data;
}



STATIC void
disable_pmc_eeprom (long addr)
{
    eeprom_put_byte (addr, EPROM_EWDS, SIZE_ADDR_OP);

    pci_write_32 ((u_int32_t *) addr, 0);       
}



STATIC void
enable_pmc_eeprom (long addr)
{
    eeprom_put_byte (addr, EPROM_EWEN, SIZE_ADDR_OP);

    pci_write_32 ((u_int32_t *) addr, 0);       
}



u_int32_t
pmc_eeprom_read (long addr, long mem_offset)
{
    u_int32_t   data;           

    if (!ByteReverseBuilt)
        BuildByteReverse ();

    mem_offset = ByteReverse[0x7F & mem_offset];        
    mem_offset <<= 2;               
    mem_offset |= EPROM_READ;       

    eeprom_put_byte (addr, mem_offset, SIZE_ADDR_OP);   

    data = eeprom_get_byte (addr);  

    pci_write_32 ((u_int32_t *) addr, 0);       

    return (data & 0x000000FF);
}



int
pmc_eeprom_write (long addr, long mem_offset, u_int32_t data)
{
    volatile u_int32_t temp;
    int         count;

    if (!ByteReverseBuilt)
        BuildByteReverse ();

    mem_offset = ByteReverse[0x7F & mem_offset];        
    mem_offset <<= 2;               
    mem_offset |= EPROM_WRITE;      

    eeprom_put_byte (addr, mem_offset, SIZE_ADDR_OP);   

    data = ByteReverse[0xFF & data];
    eeprom_put_byte (addr, data, NUM_OF_BITS);  

    pci_write_32 ((u_int32_t *) addr, 0);       

    pci_write_32 ((u_int32_t *) addr, EPROM_ENCS);      
    temp = pci_read_32 ((u_int32_t *) addr);    
    temp = pci_read_32 ((u_int32_t *) addr);
    if (temp & EPROM_ACTIVE_IN_BIT)
    {
        temp = pci_read_32 ((u_int32_t *) addr);
        if (temp & EPROM_ACTIVE_IN_BIT)
        {
            pci_write_32 ((u_int32_t *) addr, 0);       
            return (1);
        }
    }
    count = 1000;
    while (count--)
    {
        for (temp = 0; temp < 0x10; temp++)
            OS_uwait_dummy ();

        if (pci_read_32 ((u_int32_t *) addr) & EPROM_ACTIVE_IN_BIT)
            break;
    }

    if (count == -1)
        return (2);

    return (0);
}



long
pmcGetBuffValue (char *ptr, int size)
{
    long        value = 0;
    int         index;

    for (index = 0; index < size; ++index)
    {
        value <<= 8;
        value |= ptr[index] & 0xFF;
    }

    return value;
}



void
pmcSetBuffValue (char *ptr, long value, int size)
{
    int         index = size;

    while (--index >= 0)
    {
        ptr[index] = (char) (value & 0xFF);
        value >>= 8;
    }
}



void
pmc_eeprom_read_buffer (long addr, long mem_offset, char *dest_ptr, int size)
{
    while (--size >= 0)
        *dest_ptr++ = (char) pmc_eeprom_read (addr, mem_offset++);
}



void
pmc_eeprom_write_buffer (long addr, long mem_offset, char *dest_ptr, int size)
{
    enable_pmc_eeprom (addr);

    while (--size >= 0)
        pmc_eeprom_write (addr, mem_offset++, *dest_ptr++);

    disable_pmc_eeprom (addr);
}



u_int32_t
pmcCalcCrc_T01 (void *bufp)
{
    FLD_TYPE2  *buf = bufp;
    u_int32_t   crc;            

    
    sbeCrc (
            (u_int8_t *) &buf->type,
            (u_int32_t) STRUCT_OFFSET (FLD_TYPE1, Crc32),
            (u_int32_t) 0,
            (u_int32_t *) &crc);

#ifdef EEPROM_TYPE_DEBUG
    pr_info("sbeCrc: crc 1 calculated as %08x\n", crc); 
#endif
    return ~crc;
}

u_int32_t
pmcCalcCrc_T02 (void *bufp)
{
    FLD_TYPE2  *buf = bufp;
    u_int32_t   crc;            

    
    sbeCrc (
            (u_int8_t *) &buf->type,
            (u_int32_t) STRUCT_OFFSET (FLD_TYPE2, Crc32),
            (u_int32_t) 0,
            (u_int32_t *) &crc);

    
    sbeCrc (
            (u_int8_t *) &buf->Id[0],
            (u_int32_t) (sizeof (FLD_TYPE2) - STRUCT_OFFSET (FLD_TYPE2, Id)),
            (u_int32_t) crc,
            (u_int32_t *) &crc);

#ifdef EEPROM_TYPE_DEBUG
    pr_info("sbeCrc: crc 2 calculated as %08x\n", crc); 
#endif
    return crc;
}



void
pmc_init_seeprom (u_int32_t addr, u_int32_t serialNum)
{
    PROMFORMAT  buffer;         
    u_int32_t   crc;            
    time_t      createTime;
    int         i;

    createTime = get_seconds ();

    
    for (i = 0; i < sizeof (FLD_TYPE2); ++i)
        buffer.bytes[i] = mfg_template[i];

    
    pmcSetBuffValue (&buffer.fldType2.Serial[3], serialNum, 3);

    
    pmcSetBuffValue (&buffer.fldType2.CreateTime[0], createTime, 4);

    
    crc = pmcCalcCrc_T02 (&buffer);
    pmcSetBuffValue (&buffer.fldType2.Crc32[0], crc, 4);

#ifdef DEBUG
    for (i = 0; i < sizeof (FLD_TYPE2); ++i)
        pr_info("[%02X] = %02X\n", i, buffer.bytes[i] & 0xFF);
#endif

    
    pmc_eeprom_write_buffer (addr, EE_MFG, (char *) &buffer, sizeof (FLD_TYPE2));
}


char
pmc_verify_cksum (void *bufp)
{
    FLD_TYPE1  *buf1 = bufp;
    FLD_TYPE2  *buf2 = bufp;
    u_int32_t   crc1, crc2;     

    
    crc1 = pmcGetBuffValue (&buf1->Crc32[0], sizeof (buf1->Crc32));
#ifdef EEPROM_TYPE_DEBUG
    pr_info("EEPROM: chksum 1 reads   as %08x\n", crc1);        
#endif
    if ((buf1->type == PROM_FORMAT_TYPE1) &&
        (pmcCalcCrc_T01 ((void *) buf1) == crc1))
        return PROM_FORMAT_TYPE1;   

    crc2 = pmcGetBuffValue (&buf2->Crc32[0], sizeof (buf2->Crc32));
#ifdef EEPROM_TYPE_DEBUG
    pr_info("EEPROM: chksum 2 reads   as %08x\n", crc2);        
#endif
    if ((buf2->type == PROM_FORMAT_TYPE2) &&
        (pmcCalcCrc_T02 ((void *) buf2) == crc2))
        return PROM_FORMAT_TYPE2;   

    return PROM_FORMAT_Unk;         
}


