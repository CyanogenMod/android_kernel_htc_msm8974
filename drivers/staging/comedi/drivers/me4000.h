/*
    me4000.h
    Register descriptions and defines for the ME-4000 board family

    COMEDI - Linux Control and Measurement Device Interface
    Copyright (C) 1998-9 David A. Schleef <ds@schleef.org>

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

#ifndef _ME4000_H_
#define _ME4000_H_


#undef ME4000_CALL_DEBUG	
#undef ME4000_PORT_DEBUG	
#undef ME4000_ISR_DEBUG		
#undef ME4000_DEBUG		

#ifdef ME4000_CALL_DEBUG
#undef CALL_PDEBUG
#define CALL_PDEBUG(fmt, args...) printk(KERN_DEBUG"comedi%d: me4000: " fmt, dev->minor, ##args)
#else
# define CALL_PDEBUG(fmt, args...)	
#endif

#ifdef ME4000_PORT_DEBUG
#undef PORT_PDEBUG
#define PORT_PDEBUG(fmt, args...) printk(KERN_DEBUG"comedi%d: me4000: " fmt, dev->minor,  ##args)
#else
#define PORT_PDEBUG(fmt, args...)	
#endif

#ifdef ME4000_ISR_DEBUG
#undef ISR_PDEBUG
#define ISR_PDEBUG(fmt, args...) printk(KERN_DEBUG"comedi%d: me4000: " fmt, dev->minor,  ##args)
#else
#define ISR_PDEBUG(fmt, args...)	
#endif

#ifdef ME4000_DEBUG
#undef PDEBUG
#define PDEBUG(fmt, args...) printk(KERN_DEBUG"comedi%d: me4000: " fmt, dev->minor,  ##args)
#else
#define PDEBUG(fmt, args...)	
#endif


#define PCI_VENDOR_ID_MEILHAUS 0x1402

#define PCI_DEVICE_ID_MEILHAUS_ME4650	0x4650	

#define PCI_DEVICE_ID_MEILHAUS_ME4660	0x4660	
#define PCI_DEVICE_ID_MEILHAUS_ME4660I	0x4661	
#define PCI_DEVICE_ID_MEILHAUS_ME4660S	0x4662	
#define PCI_DEVICE_ID_MEILHAUS_ME4660IS	0x4663	

#define PCI_DEVICE_ID_MEILHAUS_ME4670	0x4670	
#define PCI_DEVICE_ID_MEILHAUS_ME4670I	0x4671	
#define PCI_DEVICE_ID_MEILHAUS_ME4670S	0x4672	
#define PCI_DEVICE_ID_MEILHAUS_ME4670IS	0x4673	

#define PCI_DEVICE_ID_MEILHAUS_ME4680	0x4680	
#define PCI_DEVICE_ID_MEILHAUS_ME4680I	0x4681	
#define PCI_DEVICE_ID_MEILHAUS_ME4680S	0x4682	
#define PCI_DEVICE_ID_MEILHAUS_ME4680IS	0x4683	


#define ME4000_AO_00_CTRL_REG			0x00	
#define ME4000_AO_00_STATUS_REG			0x04	
#define ME4000_AO_00_FIFO_REG			0x08	
#define ME4000_AO_00_SINGLE_REG			0x0C	
#define ME4000_AO_00_TIMER_REG			0x10	

#define ME4000_AO_01_CTRL_REG			0x18	
#define ME4000_AO_01_STATUS_REG			0x1C	
#define ME4000_AO_01_FIFO_REG			0x20	
#define ME4000_AO_01_SINGLE_REG			0x24	
#define ME4000_AO_01_TIMER_REG			0x28	

#define ME4000_AO_02_CTRL_REG			0x30	
#define ME4000_AO_02_STATUS_REG			0x34	
#define ME4000_AO_02_FIFO_REG			0x38	
#define ME4000_AO_02_SINGLE_REG			0x3C	
#define ME4000_AO_02_TIMER_REG			0x40	

#define ME4000_AO_03_CTRL_REG			0x48	
#define ME4000_AO_03_STATUS_REG			0x4C	
#define ME4000_AO_03_FIFO_REG			0x50	
#define ME4000_AO_03_SINGLE_REG			0x54	
#define ME4000_AO_03_TIMER_REG			0x58	

#define ME4000_AI_CTRL_REG			0x74	
#define ME4000_AI_STATUS_REG			0x74	
#define ME4000_AI_CHANNEL_LIST_REG		0x78	
#define ME4000_AI_DATA_REG			0x7C	
#define ME4000_AI_CHAN_TIMER_REG		0x80	
#define ME4000_AI_CHAN_PRE_TIMER_REG		0x84	
#define ME4000_AI_SCAN_TIMER_LOW_REG		0x88	
#define ME4000_AI_SCAN_TIMER_HIGH_REG		0x8C	
#define ME4000_AI_SCAN_PRE_TIMER_LOW_REG	0x90	
#define ME4000_AI_SCAN_PRE_TIMER_HIGH_REG	0x94	
#define ME4000_AI_START_REG			0x98	

#define ME4000_IRQ_STATUS_REG			0x9C	

#define ME4000_DIO_PORT_0_REG			0xA0	
#define ME4000_DIO_PORT_1_REG			0xA4	
#define ME4000_DIO_PORT_2_REG			0xA8	
#define ME4000_DIO_PORT_3_REG			0xAC	
#define ME4000_DIO_DIR_REG			0xB0	

#define ME4000_AO_LOADSETREG_XX			0xB4	

#define ME4000_DIO_CTRL_REG			0xB8	

#define ME4000_AO_DEMUX_ADJUST_REG		0xBC	

#define ME4000_AI_SAMPLE_COUNTER_REG		0xC0	


#define ME4000_AO_DEMUX_ADJUST_VALUE            0x4C


#define ME4000_CNT_COUNTER_0_REG		0x00
#define ME4000_CNT_COUNTER_1_REG		0x01
#define ME4000_CNT_COUNTER_2_REG		0x02
#define ME4000_CNT_CTRL_REG			0x03


#define PLX_INTCSR	0x4C	
#define PLX_ICR		0x50	


#define PLX_INTCSR_LOCAL_INT1_EN             0x01	
#define PLX_INTCSR_LOCAL_INT1_POL            0x02	
#define PLX_INTCSR_LOCAL_INT1_STATE          0x04	
#define PLX_INTCSR_LOCAL_INT2_EN             0x08	
#define PLX_INTCSR_LOCAL_INT2_POL            0x10	
#define PLX_INTCSR_LOCAL_INT2_STATE          0x20	
#define PLX_INTCSR_PCI_INT_EN                0x40	
#define PLX_INTCSR_SOFT_INT                  0x80	


#define PLX_ICR_BIT_EEPROM_CLOCK_SET		0x01000000
#define PLX_ICR_BIT_EEPROM_CHIP_SELECT		0x02000000
#define PLX_ICR_BIT_EEPROM_WRITE		0x04000000
#define PLX_ICR_BIT_EEPROM_READ			0x08000000
#define PLX_ICR_BIT_EEPROM_VALID		0x10000000

#define PLX_ICR_MASK_EEPROM			0x1F000000

#define EEPROM_DELAY				1


#define ME4000_AO_CTRL_BIT_MODE_0		0x001
#define ME4000_AO_CTRL_BIT_MODE_1		0x002
#define ME4000_AO_CTRL_MASK_MODE		0x003
#define ME4000_AO_CTRL_BIT_STOP			0x004
#define ME4000_AO_CTRL_BIT_ENABLE_FIFO		0x008
#define ME4000_AO_CTRL_BIT_ENABLE_EX_TRIG	0x010
#define ME4000_AO_CTRL_BIT_EX_TRIG_EDGE		0x020
#define ME4000_AO_CTRL_BIT_IMMEDIATE_STOP	0x080
#define ME4000_AO_CTRL_BIT_ENABLE_DO		0x100
#define ME4000_AO_CTRL_BIT_ENABLE_IRQ		0x200
#define ME4000_AO_CTRL_BIT_RESET_IRQ		0x400


#define ME4000_AO_STATUS_BIT_FSM		0x01
#define ME4000_AO_STATUS_BIT_FF			0x02
#define ME4000_AO_STATUS_BIT_HF			0x04
#define ME4000_AO_STATUS_BIT_EF			0x08


#define ME4000_AI_CTRL_BIT_MODE_0		0x00000001
#define ME4000_AI_CTRL_BIT_MODE_1		0x00000002
#define ME4000_AI_CTRL_BIT_MODE_2		0x00000004
#define ME4000_AI_CTRL_BIT_SAMPLE_HOLD		0x00000008
#define ME4000_AI_CTRL_BIT_IMMEDIATE_STOP	0x00000010
#define ME4000_AI_CTRL_BIT_STOP			0x00000020
#define ME4000_AI_CTRL_BIT_CHANNEL_FIFO		0x00000040
#define ME4000_AI_CTRL_BIT_DATA_FIFO		0x00000080
#define ME4000_AI_CTRL_BIT_FULLSCALE		0x00000100
#define ME4000_AI_CTRL_BIT_OFFSET		0x00000200
#define ME4000_AI_CTRL_BIT_EX_TRIG_ANALOG	0x00000400
#define ME4000_AI_CTRL_BIT_EX_TRIG		0x00000800
#define ME4000_AI_CTRL_BIT_EX_TRIG_FALLING	0x00001000
#define ME4000_AI_CTRL_BIT_EX_IRQ		0x00002000
#define ME4000_AI_CTRL_BIT_EX_IRQ_RESET		0x00004000
#define ME4000_AI_CTRL_BIT_LE_IRQ		0x00008000
#define ME4000_AI_CTRL_BIT_LE_IRQ_RESET		0x00010000
#define ME4000_AI_CTRL_BIT_HF_IRQ		0x00020000
#define ME4000_AI_CTRL_BIT_HF_IRQ_RESET		0x00040000
#define ME4000_AI_CTRL_BIT_SC_IRQ		0x00080000
#define ME4000_AI_CTRL_BIT_SC_IRQ_RESET		0x00100000
#define ME4000_AI_CTRL_BIT_SC_RELOAD		0x00200000
#define ME4000_AI_CTRL_BIT_EX_TRIG_BOTH		0x80000000


#define ME4000_AI_STATUS_BIT_EF_CHANNEL		0x00400000
#define ME4000_AI_STATUS_BIT_HF_CHANNEL		0x00800000
#define ME4000_AI_STATUS_BIT_FF_CHANNEL		0x01000000
#define ME4000_AI_STATUS_BIT_EF_DATA		0x02000000
#define ME4000_AI_STATUS_BIT_HF_DATA		0x04000000
#define ME4000_AI_STATUS_BIT_FF_DATA		0x08000000
#define ME4000_AI_STATUS_BIT_LE			0x10000000
#define ME4000_AI_STATUS_BIT_FSM		0x20000000


#define ME4000_IRQ_STATUS_BIT_EX		0x01
#define ME4000_IRQ_STATUS_BIT_LE		0x02
#define ME4000_IRQ_STATUS_BIT_AI_HF		0x04
#define ME4000_IRQ_STATUS_BIT_AO_0_HF		0x08
#define ME4000_IRQ_STATUS_BIT_AO_1_HF		0x10
#define ME4000_IRQ_STATUS_BIT_AO_2_HF		0x20
#define ME4000_IRQ_STATUS_BIT_AO_3_HF		0x40
#define ME4000_IRQ_STATUS_BIT_SC		0x80


#define ME4000_DIO_CTRL_BIT_MODE_0		0x0001
#define ME4000_DIO_CTRL_BIT_MODE_1		0x0002
#define ME4000_DIO_CTRL_BIT_MODE_2		0x0004
#define ME4000_DIO_CTRL_BIT_MODE_3		0x0008
#define ME4000_DIO_CTRL_BIT_MODE_4		0x0010
#define ME4000_DIO_CTRL_BIT_MODE_5		0x0020
#define ME4000_DIO_CTRL_BIT_MODE_6		0x0040
#define ME4000_DIO_CTRL_BIT_MODE_7		0x0080

#define ME4000_DIO_CTRL_BIT_FUNCTION_0		0x0100
#define ME4000_DIO_CTRL_BIT_FUNCTION_1		0x0200

#define ME4000_DIO_CTRL_BIT_FIFO_HIGH_0		0x0400
#define ME4000_DIO_CTRL_BIT_FIFO_HIGH_1		0x0800
#define ME4000_DIO_CTRL_BIT_FIFO_HIGH_2		0x1000
#define ME4000_DIO_CTRL_BIT_FIFO_HIGH_3		0x2000


struct me4000_ao_info {
	int count;
	int fifo_count;
};

struct me4000_ai_info {
	int count;
	int sh_count;
	int diff_count;
	int ex_trig_analog;
};

struct me4000_dio_info {
	int count;
};

struct me4000_cnt_info {
	int count;
};

struct me4000_board {
	const char *name;
	unsigned short device_id;
	struct me4000_ao_info ao;
	struct me4000_ai_info ai;
	struct me4000_dio_info dio;
	struct me4000_cnt_info cnt;
};

#define thisboard ((const struct me4000_board *)dev->board_ptr)


struct me4000_ao_context {
	int irq;

	unsigned long mirror;	/*  Store the last written value */

	unsigned long ctrl_reg;
	unsigned long status_reg;
	unsigned long fifo_reg;
	unsigned long single_reg;
	unsigned long timer_reg;
	unsigned long irq_status_reg;
	unsigned long preload_reg;
};

struct me4000_ai_context {
	int irq;

	unsigned long ctrl_reg;
	unsigned long status_reg;
	unsigned long channel_list_reg;
	unsigned long data_reg;
	unsigned long chan_timer_reg;
	unsigned long chan_pre_timer_reg;
	unsigned long scan_timer_low_reg;
	unsigned long scan_timer_high_reg;
	unsigned long scan_pre_timer_low_reg;
	unsigned long scan_pre_timer_high_reg;
	unsigned long start_reg;
	unsigned long irq_status_reg;
	unsigned long sample_counter_reg;
};

struct me4000_dio_context {
	unsigned long dir_reg;
	unsigned long ctrl_reg;
	unsigned long port_0_reg;
	unsigned long port_1_reg;
	unsigned long port_2_reg;
	unsigned long port_3_reg;
};

struct me4000_cnt_context {
	unsigned long ctrl_reg;
	unsigned long counter_0_reg;
	unsigned long counter_1_reg;
	unsigned long counter_2_reg;
};

struct me4000_info {
	unsigned long plx_regbase;	
	unsigned long me4000_regbase;	
	unsigned long timer_regbase;	
	unsigned long program_regbase;	

	unsigned long plx_regbase_size;	
	unsigned long me4000_regbase_size;	
	unsigned long timer_regbase_size;	
	unsigned long program_regbase_size;	

	unsigned int serial_no;	
	unsigned char hw_revision;	
	unsigned short vendor_id;	
	unsigned short device_id;	

	struct pci_dev *pci_dev_p;	

	unsigned int irq;	

	struct me4000_ai_context ai_context;	
	struct me4000_ao_context ao_context[4];	
	struct me4000_dio_context dio_context;	
	struct me4000_cnt_context cnt_context;	
};

#define info	((struct me4000_info *)dev->private)


#define ME4000_AI_FIFO_COUNT			2048

#define ME4000_AI_MIN_TICKS			66
#define ME4000_AI_MIN_SAMPLE_TIME		2000	
#define ME4000_AI_BASE_FREQUENCY		(unsigned int) 33E6

#define ME4000_AI_CHANNEL_LIST_COUNT		1024

#define ME4000_AI_LIST_INPUT_SINGLE_ENDED	0x000
#define ME4000_AI_LIST_INPUT_DIFFERENTIAL	0x020

#define ME4000_AI_LIST_RANGE_BIPOLAR_10		0x000
#define ME4000_AI_LIST_RANGE_BIPOLAR_2_5	0x040
#define ME4000_AI_LIST_RANGE_UNIPOLAR_10	0x080
#define ME4000_AI_LIST_RANGE_UNIPOLAR_2_5	0x0C0

#define ME4000_AI_LIST_LAST_ENTRY		0x100


#define ME4000_CNT_COUNTER_0  0x00
#define ME4000_CNT_COUNTER_1  0x40
#define ME4000_CNT_COUNTER_2  0x80

#define ME4000_CNT_MODE_0     0x00	
#define ME4000_CNT_MODE_1     0x02	
#define ME4000_CNT_MODE_2     0x04	
#define ME4000_CNT_MODE_3     0x06	
#define ME4000_CNT_MODE_4     0x08	
#define ME4000_CNT_MODE_5     0x0A	

#endif
