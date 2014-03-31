/*
 * SBE 2T3E3 synchronous serial card driver for Linux
 *
 * Copyright (C) 2009-2010 Krzysztof Halasa <khc@pm.waw.pl>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This code is based on a driver written by SBE Inc.
 */

#include <linux/ip.h>
#include "2t3e3.h"
#include "ctrl.h"


#define cpld_write_nolock(channel, reg, val)			\
	bootrom_write((channel), CPLD_MAP_REG(reg, channel), val)

u32 cpld_read(struct channel *channel, u32 reg)
{
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&channel->card->bootrom_lock, flags);
	val = bootrom_read((channel), CPLD_MAP_REG(reg, channel));
	spin_unlock_irqrestore(&channel->card->bootrom_lock, flags);
	return val;
}


u32 bootrom_read(struct channel *channel, u32 reg)
{
	unsigned long addr = channel->card->bootrom_addr;
	u32 result;

	
	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_PROGRAMMING_ADDRESS, reg & 0x3FFFF);

	
	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT,
		 SBE_2T3E3_21143_VAL_READ_OPERATION |
		 SBE_2T3E3_21143_VAL_BOOT_ROM_SELECT);

	udelay(2); 

	
	result = dc_read(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT) & 0xff;

	
	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT, 0);

	return result;
}

void bootrom_write(struct channel *channel, u32 reg, u32 val)
{
	unsigned long addr = channel->card->bootrom_addr;

	
	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_PROGRAMMING_ADDRESS, reg & 0x3FFFF);

	
	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT,
		 SBE_2T3E3_21143_VAL_WRITE_OPERATION |
		 SBE_2T3E3_21143_VAL_BOOT_ROM_SELECT |
		 (val & 0xff));

	udelay(2); 

	
	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT, 0);
}



static u32 serialrom_read_bit(struct channel *channel)
{
	unsigned long addr = channel->card->bootrom_addr;
	u32 bit;

	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT,
		 SBE_2T3E3_21143_VAL_READ_OPERATION |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_SELECT |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_CLOCK |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_CHIP_SELECT);	

	bit = (dc_read(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT) &
	       SBE_2T3E3_21143_VAL_SERIAL_ROM_DATA_OUT) > 0 ? 1 : 0;

	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT,
		 SBE_2T3E3_21143_VAL_READ_OPERATION |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_SELECT |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_CHIP_SELECT);	

	return bit;
}

static void serialrom_write_bit(struct channel *channel, u32 bit)
{
	unsigned long addr = channel->card->bootrom_addr;
	u32 lastbit = -1;

	bit &= 1;

	if (bit != lastbit) {
		dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT,
			 SBE_2T3E3_21143_VAL_WRITE_OPERATION |
			 SBE_2T3E3_21143_VAL_SERIAL_ROM_SELECT |
			 SBE_2T3E3_21143_VAL_SERIAL_ROM_CHIP_SELECT |
			 (bit << 2)); 

		lastbit = bit;
	}

	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT,
		 SBE_2T3E3_21143_VAL_WRITE_OPERATION |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_SELECT |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_CLOCK |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_CHIP_SELECT |
		 (bit << 2)); 

	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT,
		 SBE_2T3E3_21143_VAL_WRITE_OPERATION |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_SELECT |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_CHIP_SELECT |
		 (bit << 2)); 
}


u32 t3e3_eeprom_read_word(struct channel *channel, u32 address)
{
	unsigned long addr = channel->card->bootrom_addr;
	u32 i, val;
	unsigned long flags;

	address &= 0x3f;

	spin_lock_irqsave(&channel->card->bootrom_lock, flags);

	
	cpld_write_nolock(channel, SBE_2T3E3_CPLD_REG_SERIAL_CHIP_SELECT,
			  SBE_2T3E3_CPLD_VAL_EEPROM_SELECT);

	
	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT,
		 SBE_2T3E3_21143_VAL_READ_OPERATION |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_SELECT |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_CHIP_SELECT);        

	
	serialrom_write_bit(channel, 0);
	serialrom_write_bit(channel, 1);
	serialrom_write_bit(channel, 1);
	serialrom_write_bit(channel, 0);

	for (i = 0x20; i; i >>= 1)
		serialrom_write_bit(channel, address & i ? 1 : 0);

	val = 0;
	for (i = 0x8000; i; i >>= 1)
		val |= (serialrom_read_bit(channel) ? i : 0);

	
	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT,
		 SBE_2T3E3_21143_VAL_READ_OPERATION |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_SELECT |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_CHIP_SELECT);        
	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT, 0);

	
	cpld_write_nolock(channel, SBE_2T3E3_CPLD_REG_SERIAL_CHIP_SELECT, 0);

	spin_unlock_irqrestore(&channel->card->bootrom_lock, flags);

	return ntohs(val);
}



u32 exar7250_read(struct channel *channel, u32 reg)
{
	u32 result;
	unsigned long flags;

#if 0
	switch (reg) {
	case SBE_2T3E3_FRAMER_REG_OPERATING_MODE:
		return channel->framer_regs[reg];
		break;
	default:
	}
#endif

	spin_lock_irqsave(&channel->card->bootrom_lock, flags);

	result = bootrom_read(channel, cpld_reg_map[SBE_2T3E3_CPLD_REG_FRAMER_BASE_ADDRESS]
			      [channel->h.slot] + (t3e3_framer_reg_map[reg] << 2));

	spin_unlock_irqrestore(&channel->card->bootrom_lock, flags);

	return result;
}

void exar7250_write(struct channel *channel, u32 reg, u32 val)
{
	unsigned long flags;

	val &= 0xff;
	channel->framer_regs[reg] = val;

	spin_lock_irqsave(&channel->card->bootrom_lock, flags);

	bootrom_write(channel, cpld_reg_map[SBE_2T3E3_CPLD_REG_FRAMER_BASE_ADDRESS]
		      [channel->h.slot] + (t3e3_framer_reg_map[reg] << 2), val);

	spin_unlock_irqrestore(&channel->card->bootrom_lock, flags);
}



u32 exar7300_read(struct channel *channel, u32 reg)
{
	unsigned long addr = channel->card->bootrom_addr, flags;
	u32 i, val;

#if 0
	switch (reg) {
	case SBE_2T3E3_LIU_REG_REG1:
	case SBE_2T3E3_LIU_REG_REG2:
	case SBE_2T3E3_LIU_REG_REG3:
	case SBE_2T3E3_LIU_REG_REG4:
		return channel->liu_regs[reg];
		break;
	default:
	}
#endif

	

	spin_lock_irqsave(&channel->card->bootrom_lock, flags);

	cpld_write_nolock(channel, SBE_2T3E3_CPLD_REG_SERIAL_CHIP_SELECT,
			  cpld_val_map[SBE_2T3E3_CPLD_VAL_LIU_SELECT][channel->h.slot]);

	
	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT,
		 SBE_2T3E3_21143_VAL_READ_OPERATION |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_SELECT |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_CHIP_SELECT);	

	
	serialrom_write_bit(channel, 1);

	
	reg = t3e3_liu_reg_map[reg];
	for (i = 0; i < 4; i++, reg >>= 1) 
		serialrom_write_bit(channel, reg & 1);
	for (i = 0; i < 3; i++)	
		serialrom_write_bit(channel, 0);

	val = 0; 
	for (i = 0; i < 8; i++)	
		val += (serialrom_read_bit(channel) << i);

	
	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT,
		 SBE_2T3E3_21143_VAL_READ_OPERATION |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_SELECT |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_CHIP_SELECT);	
	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT, 0);

	
	cpld_write_nolock(channel, SBE_2T3E3_CPLD_REG_SERIAL_CHIP_SELECT, 0);

	spin_unlock_irqrestore(&channel->card->bootrom_lock, flags);

	return val;
}

void exar7300_write(struct channel *channel, u32 reg, u32 val)
{
	unsigned long addr = channel->card->bootrom_addr, flags;
	u32 i;

	channel->liu_regs[reg] = val;

	

	spin_lock_irqsave(&channel->card->bootrom_lock, flags);

	cpld_write_nolock(channel, SBE_2T3E3_CPLD_REG_SERIAL_CHIP_SELECT,
			  cpld_val_map[SBE_2T3E3_CPLD_VAL_LIU_SELECT][channel->h.slot]);

	
	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT,
		 SBE_2T3E3_21143_VAL_WRITE_OPERATION |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_SELECT |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_CHIP_SELECT);	

	
	serialrom_write_bit(channel, 0);

	
	reg = t3e3_liu_reg_map[reg];
	for (i = 0; i < 4; i++) {	
		serialrom_write_bit(channel, reg & 1);
		reg >>= 1;
	}
	for (i = 0; i < 3; i++)	
		serialrom_write_bit(channel, 0);

	
	for (i = 0; i < 5; i++) {
		serialrom_write_bit(channel, val & 1);
		val >>= 1;
	}
	for (i = 0; i < 3; i++)	
		serialrom_write_bit(channel, 0);

	
	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT,
		 SBE_2T3E3_21143_VAL_WRITE_OPERATION |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_SELECT |
		 SBE_2T3E3_21143_VAL_SERIAL_ROM_CHIP_SELECT);	
	dc_write(addr, SBE_2T3E3_21143_REG_BOOT_ROM_SERIAL_ROM_AND_MII_MANAGEMENT, 0);

	
	cpld_write_nolock(channel, SBE_2T3E3_CPLD_REG_SERIAL_CHIP_SELECT, 0);

	spin_unlock_irqrestore(&channel->card->bootrom_lock, flags);
}
