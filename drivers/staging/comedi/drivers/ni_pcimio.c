/*
    comedi/drivers/ni_pcimio.c
    Hardware driver for NI PCI-MIO E series cards

    COMEDI - Linux Control and Measurement Device Interface
    Copyright (C) 1997-8 David A. Schleef <ds@schleef.org>

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
/*
	The PCI-MIO E series driver was originally written by
	Tomasz Motylewski <...>, and ported to comedi by ds.

	References:

	   341079b.pdf  PCI E Series Register-Level Programmer Manual
	   340934b.pdf  DAQ-STC reference manual

	   322080b.pdf  6711/6713/6715 User Manual

	   320945c.pdf  PCI E Series User Manual
	   322138a.pdf  PCI-6052E and DAQPad-6052E User Manual

	ISSUES:

	need to deal with external reference for DAC, and other DAC
	properties in board properties

	deal with at-mio-16de-10 revision D to N changes, etc.

	need to add other CALDAC type

	need to slow down DAC loading.  I don't trust NI's claim that
	two writes to the PCI bus slows IO enough.  I would prefer to
	use udelay().  Timing specs: (clock)
		AD8522		30ns
		DAC8043		120ns
		DAC8800		60ns
		MB88341		?

*/

#include "../comedidev.h"

#include <asm/byteorder.h>
#include <linux/delay.h>

#include "ni_stc.h"
#include "mite.h"


#define PCIDMA

#define PCIMIO 1
#undef ATMIO

#define MAX_N_CALDACS (16+16+2)

#define DRV_NAME "ni_pcimio"

static DEFINE_PCI_DEVICE_TABLE(ni_pci_table) = {
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x0162)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x1170)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x1180)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x1190)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x11b0)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x11c0)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x11d0)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x1270)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x1330)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x1340)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x1350)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x14e0)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x14f0)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x1580)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x15b0)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x1880)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x1870)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x18b0)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x18c0)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x2410)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x2420)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x2430)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x2890)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x28c0)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x2a60)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x2a70)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x2a80)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x2ab0)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x2b80)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x2b90)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x2c80)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x2ca0)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x70aa)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x70ab)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x70ac)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x70af)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x70b0)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x70b4)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x70b6)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x70b7)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x70b8)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x70bc)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x70bd)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x70bf)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x70c0)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x70f2)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x710d)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x716c)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x716d)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x717f)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x71bc)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x717d)},
	{PCI_DEVICE(PCI_VENDOR_ID_NI, 0x72e8)},
	{0}
};

MODULE_DEVICE_TABLE(pci, ni_pci_table);

static const struct comedi_lrange range_ni_M_628x_ao = { 8, {
							     RANGE(-10, 10),
							     RANGE(-5, 5),
							     RANGE(-2, 2),
							     RANGE(-1, 1),
							     RANGE(-5, 15),
							     RANGE(0, 10),
							     RANGE(3, 7),
							     RANGE(4, 6),
							     RANGE_ext(-1, 1)
							     }
};

static const struct comedi_lrange range_ni_M_625x_ao = { 3, {
							     RANGE(-10, 10),
							     RANGE(-5, 5),
							     RANGE_ext(-1, 1)
							     }
};

static const struct comedi_lrange range_ni_M_622x_ao = { 1, {
							     RANGE(-10, 10),
							     }
};

static const struct ni_board_struct ni_boards[] = {
	{
	 .device_id = 0x0162,	
	 .name = "pci-mio-16xe-50",
	 .n_adchan = 16,
	 .adbits = 16,
	 .ai_fifo_depth = 2048,
	 .alwaysdither = 1,
	 .gainlkup = ai_gain_8,
	 .ai_speed = 50000,
	 .n_aochan = 2,
	 .aobits = 12,
	 .ao_fifo_depth = 0,
	 .ao_range_table = &range_bipolar10,
	 .ao_unipolar = 0,
	 .ao_speed = 50000,
	 .num_p0_dio_channels = 8,
	 .caldac = {dac8800, dac8043},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x1170,
	 .name = "pci-mio-16xe-10",	
	 .n_adchan = 16,
	 .adbits = 16,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 1,
	 .gainlkup = ai_gain_14,
	 .ai_speed = 10000,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_fifo_depth = 2048,
	 .ao_range_table = &range_ni_E_ao_ext,
	 .ao_unipolar = 1,
	 .ao_speed = 10000,
	 .num_p0_dio_channels = 8,
	 .caldac = {dac8800, dac8043, ad8522},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x28c0,
	 .name = "pci-6014",
	 .n_adchan = 16,
	 .adbits = 16,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 1,
	 .gainlkup = ai_gain_4,
	 .ai_speed = 5000,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_fifo_depth = 0,
	 .ao_range_table = &range_bipolar10,
	 .ao_unipolar = 0,
	 .ao_speed = 100000,
	 .num_p0_dio_channels = 8,
	 .caldac = {ad8804_debug},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x11d0,
	 .name = "pxi-6030e",
	 .n_adchan = 16,
	 .adbits = 16,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 1,
	 .gainlkup = ai_gain_14,
	 .ai_speed = 10000,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_fifo_depth = 2048,
	 .ao_range_table = &range_ni_E_ao_ext,
	 .ao_unipolar = 1,
	 .ao_speed = 10000,
	 .num_p0_dio_channels = 8,
	 .caldac = {dac8800, dac8043, ad8522},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x1180,
	 .name = "pci-mio-16e-1",	
	 .n_adchan = 16,
	 .adbits = 12,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 0,
	 .gainlkup = ai_gain_16,
	 .ai_speed = 800,
	 .n_aochan = 2,
	 .aobits = 12,
	 .ao_fifo_depth = 2048,
	 .ao_range_table = &range_ni_E_ao_ext,
	 .ao_unipolar = 1,
	 .ao_speed = 1000,
	 .num_p0_dio_channels = 8,
	 .caldac = {mb88341},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x1190,
	 .name = "pci-mio-16e-4",	
	 .n_adchan = 16,
	 .adbits = 12,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 0,
	 .gainlkup = ai_gain_16,
	 .ai_speed = 2000,
	 .n_aochan = 2,
	 .aobits = 12,
	 .ao_fifo_depth = 512,
	 .ao_range_table = &range_ni_E_ao_ext,
	 .ao_unipolar = 1,
	 .ao_speed = 1000,
	 .num_p0_dio_channels = 8,
	 .caldac = {ad8804_debug},	
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x11c0,
	 .name = "pxi-6040e",
	 .n_adchan = 16,
	 .adbits = 12,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 0,
	 .gainlkup = ai_gain_16,
	 .ai_speed = 2000,
	 .n_aochan = 2,
	 .aobits = 12,
	 .ao_fifo_depth = 512,
	 .ao_range_table = &range_ni_E_ao_ext,
	 .ao_unipolar = 1,
	 .ao_speed = 1000,
	 .num_p0_dio_channels = 8,
	 .caldac = {mb88341},
	 .has_8255 = 0,
	 },

	{
	 .device_id = 0x1330,
	 .name = "pci-6031e",
	 .n_adchan = 64,
	 .adbits = 16,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 1,
	 .gainlkup = ai_gain_14,
	 .ai_speed = 10000,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_fifo_depth = 2048,
	 .ao_range_table = &range_ni_E_ao_ext,
	 .ao_unipolar = 1,
	 .ao_speed = 10000,
	 .num_p0_dio_channels = 8,
	 .caldac = {dac8800, dac8043, ad8522},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x1270,
	 .name = "pci-6032e",
	 .n_adchan = 16,
	 .adbits = 16,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 1,
	 .gainlkup = ai_gain_14,
	 .ai_speed = 10000,
	 .n_aochan = 0,
	 .aobits = 0,
	 .ao_fifo_depth = 0,
	 .ao_unipolar = 0,
	 .num_p0_dio_channels = 8,
	 .caldac = {dac8800, dac8043, ad8522},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x1340,
	 .name = "pci-6033e",
	 .n_adchan = 64,
	 .adbits = 16,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 1,
	 .gainlkup = ai_gain_14,
	 .ai_speed = 10000,
	 .n_aochan = 0,
	 .aobits = 0,
	 .ao_fifo_depth = 0,
	 .ao_unipolar = 0,
	 .num_p0_dio_channels = 8,
	 .caldac = {dac8800, dac8043, ad8522},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x1350,
	 .name = "pci-6071e",
	 .n_adchan = 64,
	 .adbits = 12,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 1,
	 .gainlkup = ai_gain_16,
	 .ai_speed = 800,
	 .n_aochan = 2,
	 .aobits = 12,
	 .ao_fifo_depth = 2048,
	 .ao_range_table = &range_ni_E_ao_ext,
	 .ao_unipolar = 1,
	 .ao_speed = 1000,
	 .num_p0_dio_channels = 8,
	 .caldac = {ad8804_debug},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x2a60,
	 .name = "pci-6023e",
	 .n_adchan = 16,
	 .adbits = 12,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 0,
	 .gainlkup = ai_gain_4,
	 .ai_speed = 5000,
	 .n_aochan = 0,
	 .aobits = 0,
	 .ao_unipolar = 0,
	 .num_p0_dio_channels = 8,
	 .caldac = {ad8804_debug},	
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x2a70,
	 .name = "pci-6024e",
	 .n_adchan = 16,
	 .adbits = 12,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 0,
	 .gainlkup = ai_gain_4,
	 .ai_speed = 5000,
	 .n_aochan = 2,
	 .aobits = 12,
	 .ao_fifo_depth = 0,
	 .ao_range_table = &range_bipolar10,
	 .ao_unipolar = 0,
	 .ao_speed = 100000,
	 .num_p0_dio_channels = 8,
	 .caldac = {ad8804_debug},	
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x2a80,
	 .name = "pci-6025e",
	 .n_adchan = 16,
	 .adbits = 12,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 0,
	 .gainlkup = ai_gain_4,
	 .ai_speed = 5000,
	 .n_aochan = 2,
	 .aobits = 12,
	 .ao_fifo_depth = 0,
	 .ao_range_table = &range_bipolar10,
	 .ao_unipolar = 0,
	 .ao_speed = 100000,
	 .num_p0_dio_channels = 8,
	 .caldac = {ad8804_debug},	
	 .has_8255 = 1,
	 },
	{
	 .device_id = 0x2ab0,
	 .name = "pxi-6025e",
	 .n_adchan = 16,
	 .adbits = 12,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 0,
	 .gainlkup = ai_gain_4,
	 .ai_speed = 5000,
	 .n_aochan = 2,
	 .aobits = 12,
	 .ao_fifo_depth = 0,
	 .ao_range_table = &range_ni_E_ao_ext,
	 .ao_unipolar = 1,
	 .ao_speed = 100000,
	 .num_p0_dio_channels = 8,
	 .caldac = {ad8804_debug},	
	 .has_8255 = 1,
	 },

	{
	 .device_id = 0x2ca0,
	 .name = "pci-6034e",
	 .n_adchan = 16,
	 .adbits = 16,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 1,
	 .gainlkup = ai_gain_4,
	 .ai_speed = 5000,
	 .n_aochan = 0,
	 .aobits = 0,
	 .ao_fifo_depth = 0,
	 .ao_unipolar = 0,
	 .num_p0_dio_channels = 8,
	 .caldac = {ad8804_debug},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x2c80,
	 .name = "pci-6035e",
	 .n_adchan = 16,
	 .adbits = 16,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 1,
	 .gainlkup = ai_gain_4,
	 .ai_speed = 5000,
	 .n_aochan = 2,
	 .aobits = 12,
	 .ao_fifo_depth = 0,
	 .ao_range_table = &range_bipolar10,
	 .ao_unipolar = 0,
	 .ao_speed = 100000,
	 .num_p0_dio_channels = 8,
	 .caldac = {ad8804_debug},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x18b0,
	 .name = "pci-6052e",
	 .n_adchan = 16,
	 .adbits = 16,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 1,
	 .gainlkup = ai_gain_16,
	 .ai_speed = 3000,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_unipolar = 1,
	 .ao_fifo_depth = 2048,
	 .ao_range_table = &range_ni_E_ao_ext,
	 .ao_speed = 3000,
	 .num_p0_dio_channels = 8,
	 .caldac = {ad8804_debug, ad8804_debug, ad8522},	
	 },
	{.device_id = 0x14e0,
	 .name = "pci-6110",
	 .n_adchan = 4,
	 .adbits = 12,
	 .ai_fifo_depth = 8192,
	 .alwaysdither = 0,
	 .gainlkup = ai_gain_611x,
	 .ai_speed = 200,
	 .n_aochan = 2,
	 .aobits = 16,
	 .reg_type = ni_reg_611x,
	 .ao_range_table = &range_bipolar10,
	 .ao_unipolar = 0,
	 .ao_fifo_depth = 2048,
	 .ao_speed = 250,
	 .num_p0_dio_channels = 8,
	 .caldac = {ad8804, ad8804},
	 },
	{
	 .device_id = 0x14f0,
	 .name = "pci-6111",
	 .n_adchan = 2,
	 .adbits = 12,
	 .ai_fifo_depth = 8192,
	 .alwaysdither = 0,
	 .gainlkup = ai_gain_611x,
	 .ai_speed = 200,
	 .n_aochan = 2,
	 .aobits = 16,
	 .reg_type = ni_reg_611x,
	 .ao_range_table = &range_bipolar10,
	 .ao_unipolar = 0,
	 .ao_fifo_depth = 2048,
	 .ao_speed = 250,
	 .num_p0_dio_channels = 8,
	 .caldac = {ad8804, ad8804},
	 },
#if 0
	
	{
	 .device_id = 0x2ed0,
	 .name = "pci-6115",
	 .n_adchan = 4,
	 .adbits = 12,
	 .ai_fifo_depth = 8192,
	 .alwaysdither = 0,
	 .gainlkup = ai_gain_611x,
	 .ai_speed = 100,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_671x = 1,
	 .ao_unipolar = 0,
	 .ao_fifo_depth = 2048,
	 .ao_speed = 250,
	 .num_p0_dio_channels = 8,
	 .reg_611x = 1,
	 .caldac = {ad8804_debug, ad8804_debug, ad8804_debug},	
	 },
#endif
#if 0
	{
	 .device_id = 0x0000,
	 .name = "pxi-6115",
	 .n_adchan = 4,
	 .adbits = 12,
	 .ai_fifo_depth = 8192,
	 .alwaysdither = 0,
	 .gainlkup = ai_gain_611x,
	 .ai_speed = 100,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_671x = 1,
	 .ao_unipolar = 0,
	 .ao_fifo_depth = 2048,
	 .ao_speed = 250,
	 .reg_611x = 1,
	 .num_p0_dio_channels = 8,
	 caldac = {ad8804_debug, ad8804_debug, ad8804_debug},	
	 },
#endif
	{
	 .device_id = 0x1880,
	 .name = "pci-6711",
	 .n_adchan = 0,		
	 .n_aochan = 4,
	 .aobits = 12,
	 .ao_unipolar = 0,
	 .ao_fifo_depth = 16384,
	 
	 .ao_range_table = &range_bipolar10,
	 .ao_speed = 1000,
	 .num_p0_dio_channels = 8,
	 .reg_type = ni_reg_6711,
	 .caldac = {ad8804_debug},
	 },
	{
	 .device_id = 0x2b90,
	 .name = "pxi-6711",
	 .n_adchan = 0,		
	 .n_aochan = 4,
	 .aobits = 12,
	 .ao_unipolar = 0,
	 .ao_fifo_depth = 16384,
	 .ao_range_table = &range_bipolar10,
	 .ao_speed = 1000,
	 .num_p0_dio_channels = 8,
	 .reg_type = ni_reg_6711,
	 .caldac = {ad8804_debug},
	 },
	{
	 .device_id = 0x1870,
	 .name = "pci-6713",
	 .n_adchan = 0,		
	 .n_aochan = 8,
	 .aobits = 12,
	 .ao_unipolar = 0,
	 .ao_fifo_depth = 16384,
	 .ao_range_table = &range_bipolar10,
	 .ao_speed = 1000,
	 .num_p0_dio_channels = 8,
	 .reg_type = ni_reg_6713,
	 .caldac = {ad8804_debug, ad8804_debug},
	 },
	{
	 .device_id = 0x2b80,
	 .name = "pxi-6713",
	 .n_adchan = 0,		
	 .n_aochan = 8,
	 .aobits = 12,
	 .ao_unipolar = 0,
	 .ao_fifo_depth = 16384,
	 .ao_range_table = &range_bipolar10,
	 .ao_speed = 1000,
	 .num_p0_dio_channels = 8,
	 .reg_type = ni_reg_6713,
	 .caldac = {ad8804_debug, ad8804_debug},
	 },
	{
	 .device_id = 0x2430,
	 .name = "pci-6731",
	 .n_adchan = 0,		
	 .n_aochan = 4,
	 .aobits = 16,
	 .ao_unipolar = 0,
	 .ao_fifo_depth = 8192,
	 .ao_range_table = &range_bipolar10,
	 .ao_speed = 1000,
	 .num_p0_dio_channels = 8,
	 .reg_type = ni_reg_6711,
	 .caldac = {ad8804_debug},
	 },
#if 0				
	{
	 .device_id = 0x0,
	 .name = "pxi-6731",
	 .n_adchan = 0,		
	 .n_aochan = 4,
	 .aobits = 16,
	 .ao_unipolar = 0,
	 .ao_fifo_depth = 8192,
	 .ao_range_table = &range_bipolar10,
	 .num_p0_dio_channels = 8,
	 .reg_type = ni_reg_6711,
	 .caldac = {ad8804_debug},
	 },
#endif
	{
	 .device_id = 0x2410,
	 .name = "pci-6733",
	 .n_adchan = 0,		
	 .n_aochan = 8,
	 .aobits = 16,
	 .ao_unipolar = 0,
	 .ao_fifo_depth = 16384,
	 .ao_range_table = &range_bipolar10,
	 .ao_speed = 1000,
	 .num_p0_dio_channels = 8,
	 .reg_type = ni_reg_6713,
	 .caldac = {ad8804_debug, ad8804_debug},
	 },
	{
	 .device_id = 0x2420,
	 .name = "pxi-6733",
	 .n_adchan = 0,		
	 .n_aochan = 8,
	 .aobits = 16,
	 .ao_unipolar = 0,
	 .ao_fifo_depth = 16384,
	 .ao_range_table = &range_bipolar10,
	 .ao_speed = 1000,
	 .num_p0_dio_channels = 8,
	 .reg_type = ni_reg_6713,
	 .caldac = {ad8804_debug, ad8804_debug},
	 },
	{
	 .device_id = 0x15b0,
	 .name = "pxi-6071e",
	 .n_adchan = 64,
	 .adbits = 12,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 1,
	 .gainlkup = ai_gain_16,
	 .ai_speed = 800,
	 .n_aochan = 2,
	 .aobits = 12,
	 .ao_fifo_depth = 2048,
	 .ao_range_table = &range_ni_E_ao_ext,
	 .ao_unipolar = 1,
	 .ao_speed = 1000,
	 .num_p0_dio_channels = 8,
	 .caldac = {ad8804_debug},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x11b0,
	 .name = "pxi-6070e",
	 .n_adchan = 16,
	 .adbits = 12,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 1,
	 .gainlkup = ai_gain_16,
	 .ai_speed = 800,
	 .n_aochan = 2,
	 .aobits = 12,
	 .ao_fifo_depth = 2048,
	 .ao_range_table = &range_ni_E_ao_ext,
	 .ao_unipolar = 1,
	 .ao_speed = 1000,
	 .num_p0_dio_channels = 8,
	 .caldac = {ad8804_debug},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x18c0,
	 .name = "pxi-6052e",
	 .n_adchan = 16,
	 .adbits = 16,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 1,
	 .gainlkup = ai_gain_16,
	 .ai_speed = 3000,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_unipolar = 1,
	 .ao_fifo_depth = 2048,
	 .ao_range_table = &range_ni_E_ao_ext,
	 .ao_speed = 3000,
	 .num_p0_dio_channels = 8,
	 .caldac = {mb88341, mb88341, ad8522},
	 },
	{
	 .device_id = 0x1580,
	 .name = "pxi-6031e",
	 .n_adchan = 64,
	 .adbits = 16,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 1,
	 .gainlkup = ai_gain_14,
	 .ai_speed = 10000,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_fifo_depth = 2048,
	 .ao_range_table = &range_ni_E_ao_ext,
	 .ao_unipolar = 1,
	 .ao_speed = 10000,
	 .num_p0_dio_channels = 8,
	 .caldac = {dac8800, dac8043, ad8522},
	 },
	{
	 .device_id = 0x2890,
	 .name = "pci-6036e",
	 .n_adchan = 16,
	 .adbits = 16,
	 .ai_fifo_depth = 512,
	 .alwaysdither = 1,
	 .gainlkup = ai_gain_4,
	 .ai_speed = 5000,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_fifo_depth = 0,
	 .ao_range_table = &range_bipolar10,
	 .ao_unipolar = 0,
	 .ao_speed = 100000,
	 .num_p0_dio_channels = 8,
	 .caldac = {ad8804_debug},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x70b0,
	 .name = "pci-6220",
	 .n_adchan = 16,
	 .adbits = 16,
	 .ai_fifo_depth = 512,
	 
	 .gainlkup = ai_gain_622x,
	 .ai_speed = 4000,
	 .n_aochan = 0,
	 .aobits = 0,
	 .ao_fifo_depth = 0,
	 .num_p0_dio_channels = 8,
	 .reg_type = ni_reg_622x,
	 .ao_unipolar = 0,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x70af,
	 .name = "pci-6221",
	 .n_adchan = 16,
	 .adbits = 16,
	 .ai_fifo_depth = 4095,
	 .gainlkup = ai_gain_622x,
	 .ai_speed = 4000,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_fifo_depth = 8191,
	 .ao_range_table = &range_ni_M_622x_ao,
	 .reg_type = ni_reg_622x,
	 .ao_unipolar = 0,
	 .ao_speed = 1200,
	 .num_p0_dio_channels = 8,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x71bc,
	 .name = "pci-6221_37pin",
	 .n_adchan = 16,
	 .adbits = 16,
	 .ai_fifo_depth = 4095,
	 .gainlkup = ai_gain_622x,
	 .ai_speed = 4000,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_fifo_depth = 8191,
	 .ao_range_table = &range_ni_M_622x_ao,
	 .reg_type = ni_reg_622x,
	 .ao_unipolar = 0,
	 .ao_speed = 1200,
	 .num_p0_dio_channels = 8,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x70f2,
	 .name = "pci-6224",
	 .n_adchan = 32,
	 .adbits = 16,
	 .ai_fifo_depth = 4095,
	 .gainlkup = ai_gain_622x,
	 .ai_speed = 4000,
	 .n_aochan = 0,
	 .aobits = 0,
	 .ao_fifo_depth = 0,
	 .reg_type = ni_reg_622x,
	 .ao_unipolar = 0,
	 .num_p0_dio_channels = 32,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x70f3,
	 .name = "pxi-6224",
	 .n_adchan = 32,
	 .adbits = 16,
	 .ai_fifo_depth = 4095,
	 .gainlkup = ai_gain_622x,
	 .ai_speed = 4000,
	 .n_aochan = 0,
	 .aobits = 0,
	 .ao_fifo_depth = 0,
	 .reg_type = ni_reg_622x,
	 .ao_unipolar = 0,
	 .num_p0_dio_channels = 32,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x716c,
	 .name = "pci-6225",
	 .n_adchan = 80,
	 .adbits = 16,
	 .ai_fifo_depth = 4095,
	 .gainlkup = ai_gain_622x,
	 .ai_speed = 4000,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_fifo_depth = 8191,
	 .ao_range_table = &range_ni_M_622x_ao,
	 .reg_type = ni_reg_622x,
	 .ao_unipolar = 0,
	 .ao_speed = 1200,
	 .num_p0_dio_channels = 32,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x716d,
	 .name = "pxi-6225",
	 .n_adchan = 80,
	 .adbits = 16,
	 .ai_fifo_depth = 4095,
	 .gainlkup = ai_gain_622x,
	 .ai_speed = 4000,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_fifo_depth = 8191,
	 .ao_range_table = &range_ni_M_622x_ao,
	 .reg_type = ni_reg_622x,
	 .ao_unipolar = 0,
	 .ao_speed = 1200,
	 .num_p0_dio_channels = 32,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	},
	{
	 .device_id = 0x70aa,
	 .name = "pci-6229",
	 .n_adchan = 32,
	 .adbits = 16,
	 .ai_fifo_depth = 4095,
	 .gainlkup = ai_gain_622x,
	 .ai_speed = 4000,
	 .n_aochan = 4,
	 .aobits = 16,
	 .ao_fifo_depth = 8191,
	 .ao_range_table = &range_ni_M_622x_ao,
	 .reg_type = ni_reg_622x,
	 .ao_unipolar = 0,
	 .ao_speed = 1200,
	 .num_p0_dio_channels = 32,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x70b4,
	 .name = "pci-6250",
	 .n_adchan = 16,
	 .adbits = 16,
	 .ai_fifo_depth = 4095,
	 .gainlkup = ai_gain_628x,
	 .ai_speed = 800,
	 .n_aochan = 0,
	 .aobits = 0,
	 .ao_fifo_depth = 0,
	 .reg_type = ni_reg_625x,
	 .ao_unipolar = 0,
	 .num_p0_dio_channels = 8,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x70b8,
	 .name = "pci-6251",
	 .n_adchan = 16,
	 .adbits = 16,
	 .ai_fifo_depth = 4095,
	 .gainlkup = ai_gain_628x,
	 .ai_speed = 800,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_fifo_depth = 8191,
	 .ao_range_table = &range_ni_M_625x_ao,
	 .reg_type = ni_reg_625x,
	 .ao_unipolar = 0,
	 .ao_speed = 357,
	 .num_p0_dio_channels = 8,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x717d,
	 .name = "pcie-6251",
	 .n_adchan = 16,
	 .adbits = 16,
	 .ai_fifo_depth = 4095,
	 .gainlkup = ai_gain_628x,
	 .ai_speed = 800,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_fifo_depth = 8191,
	 .ao_range_table = &range_ni_M_625x_ao,
	 .reg_type = ni_reg_625x,
	 .ao_unipolar = 0,
	 .ao_speed = 357,
	 .num_p0_dio_channels = 8,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x72e8,
	 .name = "pxie-6251",
	 .n_adchan = 16,
	 .adbits = 16,
	 .ai_fifo_depth = 4095,
	 .gainlkup = ai_gain_628x,
	 .ai_speed = 800,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_fifo_depth = 8191,
	 .ao_range_table = &range_ni_M_625x_ao,
	 .reg_type = ni_reg_625x,
	 .ao_unipolar = 0,
	 .ao_speed = 357,
	 .num_p0_dio_channels = 8,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x70b7,
	 .name = "pci-6254",
	 .n_adchan = 32,
	 .adbits = 16,
	 .ai_fifo_depth = 4095,
	 .gainlkup = ai_gain_628x,
	 .ai_speed = 800,
	 .n_aochan = 0,
	 .aobits = 0,
	 .ao_fifo_depth = 0,
	 .reg_type = ni_reg_625x,
	 .ao_unipolar = 0,
	 .num_p0_dio_channels = 32,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x70ab,
	 .name = "pci-6259",
	 .n_adchan = 32,
	 .adbits = 16,
	 .ai_fifo_depth = 4095,
	 .gainlkup = ai_gain_628x,
	 .ai_speed = 800,
	 .n_aochan = 4,
	 .aobits = 16,
	 .ao_fifo_depth = 8191,
	 .ao_range_table = &range_ni_M_625x_ao,
	 .reg_type = ni_reg_625x,
	 .ao_unipolar = 0,
	 .ao_speed = 357,
	 .num_p0_dio_channels = 32,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x717f,
	 .name = "pcie-6259",
	 .n_adchan = 32,
	 .adbits = 16,
	 .ai_fifo_depth = 4095,
	 .gainlkup = ai_gain_628x,
	 .ai_speed = 800,
	 .n_aochan = 4,
	 .aobits = 16,
	 .ao_fifo_depth = 8191,
	 .ao_range_table = &range_ni_M_625x_ao,
	 .reg_type = ni_reg_625x,
	 .ao_unipolar = 0,
	 .ao_speed = 357,
	 .num_p0_dio_channels = 32,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x70b6,
	 .name = "pci-6280",
	 .n_adchan = 16,
	 .adbits = 18,
	 .ai_fifo_depth = 2047,
	 .gainlkup = ai_gain_628x,
	 .ai_speed = 1600,
	 .n_aochan = 0,
	 .aobits = 0,
	 .ao_fifo_depth = 8191,
	 .reg_type = ni_reg_628x,
	 .ao_unipolar = 0,
	 .num_p0_dio_channels = 8,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x70bd,
	 .name = "pci-6281",
	 .n_adchan = 16,
	 .adbits = 18,
	 .ai_fifo_depth = 2047,
	 .gainlkup = ai_gain_628x,
	 .ai_speed = 1600,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_fifo_depth = 8191,
	 .ao_range_table = &range_ni_M_628x_ao,
	 .reg_type = ni_reg_628x,
	 .ao_unipolar = 1,
	 .ao_speed = 357,
	 .num_p0_dio_channels = 8,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x70bf,
	 .name = "pxi-6281",
	 .n_adchan = 16,
	 .adbits = 18,
	 .ai_fifo_depth = 2047,
	 .gainlkup = ai_gain_628x,
	 .ai_speed = 1600,
	 .n_aochan = 2,
	 .aobits = 16,
	 .ao_fifo_depth = 8191,
	 .ao_range_table = &range_ni_M_628x_ao,
	 .reg_type = ni_reg_628x,
	 .ao_unipolar = 1,
	 .ao_speed = 357,
	 .num_p0_dio_channels = 8,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x70bc,
	 .name = "pci-6284",
	 .n_adchan = 32,
	 .adbits = 18,
	 .ai_fifo_depth = 2047,
	 .gainlkup = ai_gain_628x,
	 .ai_speed = 1600,
	 .n_aochan = 0,
	 .aobits = 0,
	 .ao_fifo_depth = 0,
	 .reg_type = ni_reg_628x,
	 .ao_unipolar = 0,
	 .num_p0_dio_channels = 32,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x70ac,
	 .name = "pci-6289",
	 .n_adchan = 32,
	 .adbits = 18,
	 .ai_fifo_depth = 2047,
	 .gainlkup = ai_gain_628x,
	 .ai_speed = 1600,
	 .n_aochan = 4,
	 .aobits = 16,
	 .ao_fifo_depth = 8191,
	 .ao_range_table = &range_ni_M_628x_ao,
	 .reg_type = ni_reg_628x,
	 .ao_unipolar = 1,
	 .ao_speed = 357,
	 .num_p0_dio_channels = 32,
	 .caldac = {caldac_none},
	 .has_8255 = 0,
	 },
	{
	 .device_id = 0x70C0,
	 .name = "pci-6143",
	 .n_adchan = 8,
	 .adbits = 16,
	 .ai_fifo_depth = 1024,
	 .alwaysdither = 0,
	 .gainlkup = ai_gain_6143,
	 .ai_speed = 4000,
	 .n_aochan = 0,
	 .aobits = 0,
	 .reg_type = ni_reg_6143,
	 .ao_unipolar = 0,
	 .ao_fifo_depth = 0,
	 .num_p0_dio_channels = 8,
	 .caldac = {ad8804_debug, ad8804_debug},
	 },
	{
	 .device_id = 0x710D,
	 .name = "pxi-6143",
	 .n_adchan = 8,
	 .adbits = 16,
	 .ai_fifo_depth = 1024,
	 .alwaysdither = 0,
	 .gainlkup = ai_gain_6143,
	 .ai_speed = 4000,
	 .n_aochan = 0,
	 .aobits = 0,
	 .reg_type = ni_reg_6143,
	 .ao_unipolar = 0,
	 .ao_fifo_depth = 0,
	 .num_p0_dio_channels = 8,
	 .caldac = {ad8804_debug, ad8804_debug},
	 },
};

#define n_pcimio_boards ARRAY_SIZE(ni_boards)

static int pcimio_attach(struct comedi_device *dev,
			 struct comedi_devconfig *it);
static int pcimio_detach(struct comedi_device *dev);
static struct comedi_driver driver_pcimio = {
	.driver_name = DRV_NAME,
	.module = THIS_MODULE,
	.attach = pcimio_attach,
	.detach = pcimio_detach,
};

static int __devinit driver_pcimio_pci_probe(struct pci_dev *dev,
					     const struct pci_device_id *ent)
{
	return comedi_pci_auto_config(dev, driver_pcimio.driver_name);
}

static void __devexit driver_pcimio_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static struct pci_driver driver_pcimio_pci_driver = {
	.id_table = ni_pci_table,
	.probe = &driver_pcimio_pci_probe,
	.remove = __devexit_p(&driver_pcimio_pci_remove)
};

static int __init driver_pcimio_init_module(void)
{
	int retval;

	retval = comedi_driver_register(&driver_pcimio);
	if (retval < 0)
		return retval;

	driver_pcimio_pci_driver.name = (char *)driver_pcimio.driver_name;
	return pci_register_driver(&driver_pcimio_pci_driver);
}

static void __exit driver_pcimio_cleanup_module(void)
{
	pci_unregister_driver(&driver_pcimio_pci_driver);
	comedi_driver_unregister(&driver_pcimio);
}

module_init(driver_pcimio_init_module);
module_exit(driver_pcimio_cleanup_module);

struct ni_private {
NI_PRIVATE_COMMON};
#define devpriv ((struct ni_private *)dev->private)


#define ni_writel(a, b)	(writel((a), devpriv->mite->daq_io_addr + (b)))
#define ni_readl(a)	(readl(devpriv->mite->daq_io_addr + (a)))
#define ni_writew(a, b)	(writew((a), devpriv->mite->daq_io_addr + (b)))
#define ni_readw(a)	(readw(devpriv->mite->daq_io_addr + (a)))
#define ni_writeb(a, b)	(writeb((a), devpriv->mite->daq_io_addr + (b)))
#define ni_readb(a)	(readb(devpriv->mite->daq_io_addr + (a)))


/* We automatically take advantage of STC registers that can be
 * read/written directly in the I/O space of the board.  Most
 * PCIMIO devices map the low 8 STC registers to iobase+addr*2.
 * The 611x devices map the write registers to iobase+addr*2, and
 * the read registers to iobase+(addr-1)*2. */

static void e_series_win_out(struct comedi_device *dev, uint16_t data, int reg)
{
	unsigned long flags;

	spin_lock_irqsave(&devpriv->window_lock, flags);
	ni_writew(reg, Window_Address);
	ni_writew(data, Window_Data);
	spin_unlock_irqrestore(&devpriv->window_lock, flags);
}

static uint16_t e_series_win_in(struct comedi_device *dev, int reg)
{
	unsigned long flags;
	uint16_t ret;

	spin_lock_irqsave(&devpriv->window_lock, flags);
	ni_writew(reg, Window_Address);
	ret = ni_readw(Window_Data);
	spin_unlock_irqrestore(&devpriv->window_lock, flags);

	return ret;
}

static void m_series_stc_writew(struct comedi_device *dev, uint16_t data,
				int reg)
{
	unsigned offset;
	switch (reg) {
	case ADC_FIFO_Clear:
		offset = M_Offset_AI_FIFO_Clear;
		break;
	case AI_Command_1_Register:
		offset = M_Offset_AI_Command_1;
		break;
	case AI_Command_2_Register:
		offset = M_Offset_AI_Command_2;
		break;
	case AI_Mode_1_Register:
		offset = M_Offset_AI_Mode_1;
		break;
	case AI_Mode_2_Register:
		offset = M_Offset_AI_Mode_2;
		break;
	case AI_Mode_3_Register:
		offset = M_Offset_AI_Mode_3;
		break;
	case AI_Output_Control_Register:
		offset = M_Offset_AI_Output_Control;
		break;
	case AI_Personal_Register:
		offset = M_Offset_AI_Personal;
		break;
	case AI_SI2_Load_A_Register:
		
		ni_writel(data, M_Offset_AI_SI2_Load_A);
		return;
		break;
	case AI_SI2_Load_B_Register:
		
		ni_writel(data, M_Offset_AI_SI2_Load_B);
		return;
		break;
	case AI_START_STOP_Select_Register:
		offset = M_Offset_AI_START_STOP_Select;
		break;
	case AI_Trigger_Select_Register:
		offset = M_Offset_AI_Trigger_Select;
		break;
	case Analog_Trigger_Etc_Register:
		offset = M_Offset_Analog_Trigger_Etc;
		break;
	case AO_Command_1_Register:
		offset = M_Offset_AO_Command_1;
		break;
	case AO_Command_2_Register:
		offset = M_Offset_AO_Command_2;
		break;
	case AO_Mode_1_Register:
		offset = M_Offset_AO_Mode_1;
		break;
	case AO_Mode_2_Register:
		offset = M_Offset_AO_Mode_2;
		break;
	case AO_Mode_3_Register:
		offset = M_Offset_AO_Mode_3;
		break;
	case AO_Output_Control_Register:
		offset = M_Offset_AO_Output_Control;
		break;
	case AO_Personal_Register:
		offset = M_Offset_AO_Personal;
		break;
	case AO_Start_Select_Register:
		offset = M_Offset_AO_Start_Select;
		break;
	case AO_Trigger_Select_Register:
		offset = M_Offset_AO_Trigger_Select;
		break;
	case Clock_and_FOUT_Register:
		offset = M_Offset_Clock_and_FOUT;
		break;
	case Configuration_Memory_Clear:
		offset = M_Offset_Configuration_Memory_Clear;
		break;
	case DAC_FIFO_Clear:
		offset = M_Offset_AO_FIFO_Clear;
		break;
	case DIO_Control_Register:
		printk
		    ("%s: FIXME: register 0x%x does not map cleanly on to m-series boards.\n",
		     __func__, reg);
		return;
		break;
	case G_Autoincrement_Register(0):
		offset = M_Offset_G0_Autoincrement;
		break;
	case G_Autoincrement_Register(1):
		offset = M_Offset_G1_Autoincrement;
		break;
	case G_Command_Register(0):
		offset = M_Offset_G0_Command;
		break;
	case G_Command_Register(1):
		offset = M_Offset_G1_Command;
		break;
	case G_Input_Select_Register(0):
		offset = M_Offset_G0_Input_Select;
		break;
	case G_Input_Select_Register(1):
		offset = M_Offset_G1_Input_Select;
		break;
	case G_Mode_Register(0):
		offset = M_Offset_G0_Mode;
		break;
	case G_Mode_Register(1):
		offset = M_Offset_G1_Mode;
		break;
	case Interrupt_A_Ack_Register:
		offset = M_Offset_Interrupt_A_Ack;
		break;
	case Interrupt_A_Enable_Register:
		offset = M_Offset_Interrupt_A_Enable;
		break;
	case Interrupt_B_Ack_Register:
		offset = M_Offset_Interrupt_B_Ack;
		break;
	case Interrupt_B_Enable_Register:
		offset = M_Offset_Interrupt_B_Enable;
		break;
	case Interrupt_Control_Register:
		offset = M_Offset_Interrupt_Control;
		break;
	case IO_Bidirection_Pin_Register:
		offset = M_Offset_IO_Bidirection_Pin;
		break;
	case Joint_Reset_Register:
		offset = M_Offset_Joint_Reset;
		break;
	case RTSI_Trig_A_Output_Register:
		offset = M_Offset_RTSI_Trig_A_Output;
		break;
	case RTSI_Trig_B_Output_Register:
		offset = M_Offset_RTSI_Trig_B_Output;
		break;
	case RTSI_Trig_Direction_Register:
		offset = M_Offset_RTSI_Trig_Direction;
		break;
	default:
		printk(KERN_WARNING "%s: bug! unhandled register=0x%x in switch.\n",
		       __func__, reg);
		BUG();
		return;
		break;
	}
	ni_writew(data, offset);
}

static uint16_t m_series_stc_readw(struct comedi_device *dev, int reg)
{
	unsigned offset;
	switch (reg) {
	case AI_Status_1_Register:
		offset = M_Offset_AI_Status_1;
		break;
	case AO_Status_1_Register:
		offset = M_Offset_AO_Status_1;
		break;
	case AO_Status_2_Register:
		offset = M_Offset_AO_Status_2;
		break;
	case DIO_Serial_Input_Register:
		return ni_readb(M_Offset_SCXI_Serial_Data_In);
		break;
	case Joint_Status_1_Register:
		offset = M_Offset_Joint_Status_1;
		break;
	case Joint_Status_2_Register:
		offset = M_Offset_Joint_Status_2;
		break;
	case G_Status_Register:
		offset = M_Offset_G01_Status;
		break;
	default:
		printk(KERN_WARNING "%s: bug! unhandled register=0x%x in switch.\n",
		       __func__, reg);
		BUG();
		return 0;
		break;
	}
	return ni_readw(offset);
}

static void m_series_stc_writel(struct comedi_device *dev, uint32_t data,
				int reg)
{
	unsigned offset;
	switch (reg) {
	case AI_SC_Load_A_Registers:
		offset = M_Offset_AI_SC_Load_A;
		break;
	case AI_SI_Load_A_Registers:
		offset = M_Offset_AI_SI_Load_A;
		break;
	case AO_BC_Load_A_Register:
		offset = M_Offset_AO_BC_Load_A;
		break;
	case AO_UC_Load_A_Register:
		offset = M_Offset_AO_UC_Load_A;
		break;
	case AO_UI_Load_A_Register:
		offset = M_Offset_AO_UI_Load_A;
		break;
	case G_Load_A_Register(0):
		offset = M_Offset_G0_Load_A;
		break;
	case G_Load_A_Register(1):
		offset = M_Offset_G1_Load_A;
		break;
	case G_Load_B_Register(0):
		offset = M_Offset_G0_Load_B;
		break;
	case G_Load_B_Register(1):
		offset = M_Offset_G1_Load_B;
		break;
	default:
		printk(KERN_WARNING "%s: bug! unhandled register=0x%x in switch.\n",
		       __func__, reg);
		BUG();
		return;
		break;
	}
	ni_writel(data, offset);
}

static uint32_t m_series_stc_readl(struct comedi_device *dev, int reg)
{
	unsigned offset;
	switch (reg) {
	case G_HW_Save_Register(0):
		offset = M_Offset_G0_HW_Save;
		break;
	case G_HW_Save_Register(1):
		offset = M_Offset_G1_HW_Save;
		break;
	case G_Save_Register(0):
		offset = M_Offset_G0_Save;
		break;
	case G_Save_Register(1):
		offset = M_Offset_G1_Save;
		break;
	default:
		printk(KERN_WARNING "%s: bug! unhandled register=0x%x in switch.\n",
		       __func__, reg);
		BUG();
		return 0;
		break;
	}
	return ni_readl(offset);
}

#define interrupt_pin(a)	0
#define IRQ_POLARITY 1

#define NI_E_IRQ_FLAGS		IRQF_SHARED

#include "ni_mio_common.c"

static int pcimio_find_device(struct comedi_device *dev, int bus, int slot);
static int pcimio_ai_change(struct comedi_device *dev,
			    struct comedi_subdevice *s, unsigned long new_size);
static int pcimio_ao_change(struct comedi_device *dev,
			    struct comedi_subdevice *s, unsigned long new_size);
static int pcimio_gpct0_change(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       unsigned long new_size);
static int pcimio_gpct1_change(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       unsigned long new_size);
static int pcimio_dio_change(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     unsigned long new_size);

static void m_series_init_eeprom_buffer(struct comedi_device *dev)
{
	static const int Start_Cal_EEPROM = 0x400;
	static const unsigned window_size = 10;
	static const int serial_number_eeprom_offset = 0x4;
	static const int serial_number_eeprom_length = 0x4;
	unsigned old_iodwbsr_bits;
	unsigned old_iodwbsr1_bits;
	unsigned old_iodwcr1_bits;
	int i;

	old_iodwbsr_bits = readl(devpriv->mite->mite_io_addr + MITE_IODWBSR);
	old_iodwbsr1_bits = readl(devpriv->mite->mite_io_addr + MITE_IODWBSR_1);
	old_iodwcr1_bits = readl(devpriv->mite->mite_io_addr + MITE_IODWCR_1);
	writel(0x0, devpriv->mite->mite_io_addr + MITE_IODWBSR);
	writel(((0x80 | window_size) | devpriv->mite->daq_phys_addr),
	       devpriv->mite->mite_io_addr + MITE_IODWBSR_1);
	writel(0x1 | old_iodwcr1_bits,
	       devpriv->mite->mite_io_addr + MITE_IODWCR_1);
	writel(0xf, devpriv->mite->mite_io_addr + 0x30);

	BUG_ON(serial_number_eeprom_length > sizeof(devpriv->serial_number));
	for (i = 0; i < serial_number_eeprom_length; ++i) {
		char *byte_ptr = (char *)&devpriv->serial_number + i;
		*byte_ptr = ni_readb(serial_number_eeprom_offset + i);
	}
	devpriv->serial_number = be32_to_cpu(devpriv->serial_number);

	for (i = 0; i < M_SERIES_EEPROM_SIZE; ++i)
		devpriv->eeprom_buffer[i] = ni_readb(Start_Cal_EEPROM + i);

	writel(old_iodwbsr1_bits, devpriv->mite->mite_io_addr + MITE_IODWBSR_1);
	writel(old_iodwbsr_bits, devpriv->mite->mite_io_addr + MITE_IODWBSR);
	writel(old_iodwcr1_bits, devpriv->mite->mite_io_addr + MITE_IODWCR_1);
	writel(0x0, devpriv->mite->mite_io_addr + 0x30);
}

static void init_6143(struct comedi_device *dev)
{
	
	devpriv->stc_writew(dev, 0, Interrupt_Control_Register);

	
	ni_writeb(0x00, Magic_6143);	
	ni_writeb(0x80, PipelineDelay_6143);	
	ni_writeb(0x00, EOC_Set_6143);	

	ni_writel(boardtype.ai_fifo_depth / 2, AIFIFO_Flag_6143);	

	
	devpriv->ai_calib_source_enabled = 0;
	ni_writew(devpriv->ai_calib_source | Calibration_Channel_6143_RelayOff,
		  Calibration_Channel_6143);
	ni_writew(devpriv->ai_calib_source, Calibration_Channel_6143);
}

static int pcimio_detach(struct comedi_device *dev)
{
	mio_common_detach(dev);
	if (dev->irq)
		free_irq(dev->irq, dev);

	if (dev->private) {
		mite_free_ring(devpriv->ai_mite_ring);
		mite_free_ring(devpriv->ao_mite_ring);
		mite_free_ring(devpriv->cdo_mite_ring);
		mite_free_ring(devpriv->gpct_mite_ring[0]);
		mite_free_ring(devpriv->gpct_mite_ring[1]);
		if (devpriv->mite)
			mite_unsetup(devpriv->mite);
	}

	return 0;
}

static int pcimio_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	int ret;

	dev_info(dev->hw_dev, "comedi%d: ni_pcimio:\n", dev->minor);

	ret = ni_alloc_private(dev);
	if (ret < 0)
		return ret;

	ret = pcimio_find_device(dev, it->options[0], it->options[1]);
	if (ret < 0)
		return ret;

	dev_dbg(dev->hw_dev, "%s\n", boardtype.name);
	dev->board_name = boardtype.name;

	if (boardtype.reg_type & ni_reg_m_series_mask) {
		devpriv->stc_writew = &m_series_stc_writew;
		devpriv->stc_readw = &m_series_stc_readw;
		devpriv->stc_writel = &m_series_stc_writel;
		devpriv->stc_readl = &m_series_stc_readl;
	} else {
		devpriv->stc_writew = &e_series_win_out;
		devpriv->stc_readw = &e_series_win_in;
		devpriv->stc_writel = &win_out2;
		devpriv->stc_readl = &win_in2;
	}

	ret = mite_setup(devpriv->mite);
	if (ret < 0) {
		pr_warn("error setting up mite\n");
		return ret;
	}
	comedi_set_hw_dev(dev, &devpriv->mite->pcidev->dev);
	devpriv->ai_mite_ring = mite_alloc_ring(devpriv->mite);
	if (devpriv->ai_mite_ring == NULL)
		return -ENOMEM;
	devpriv->ao_mite_ring = mite_alloc_ring(devpriv->mite);
	if (devpriv->ao_mite_ring == NULL)
		return -ENOMEM;
	devpriv->cdo_mite_ring = mite_alloc_ring(devpriv->mite);
	if (devpriv->cdo_mite_ring == NULL)
		return -ENOMEM;
	devpriv->gpct_mite_ring[0] = mite_alloc_ring(devpriv->mite);
	if (devpriv->gpct_mite_ring[0] == NULL)
		return -ENOMEM;
	devpriv->gpct_mite_ring[1] = mite_alloc_ring(devpriv->mite);
	if (devpriv->gpct_mite_ring[1] == NULL)
		return -ENOMEM;

	if (boardtype.reg_type & ni_reg_m_series_mask)
		m_series_init_eeprom_buffer(dev);
	if (boardtype.reg_type == ni_reg_6143)
		init_6143(dev);

	dev->irq = mite_irq(devpriv->mite);

	if (dev->irq == 0) {
		pr_warn("unknown irq (bad)\n");
	} else {
		pr_debug("( irq = %u )\n", dev->irq);
		ret = request_irq(dev->irq, ni_E_interrupt, NI_E_IRQ_FLAGS,
				  DRV_NAME, dev);
		if (ret < 0) {
			pr_warn("irq not available\n");
			dev->irq = 0;
		}
	}

	ret = ni_E_init(dev, it);
	if (ret < 0)
		return ret;

	dev->subdevices[NI_AI_SUBDEV].buf_change = &pcimio_ai_change;
	dev->subdevices[NI_AO_SUBDEV].buf_change = &pcimio_ao_change;
	dev->subdevices[NI_GPCT_SUBDEV(0)].buf_change = &pcimio_gpct0_change;
	dev->subdevices[NI_GPCT_SUBDEV(1)].buf_change = &pcimio_gpct1_change;
	dev->subdevices[NI_DIO_SUBDEV].buf_change = &pcimio_dio_change;

	return ret;
}

static int pcimio_find_device(struct comedi_device *dev, int bus, int slot)
{
	struct mite_struct *mite;
	int i;

	for (mite = mite_devices; mite; mite = mite->next) {
		if (mite->used)
			continue;
		if (bus || slot) {
			if (bus != mite->pcidev->bus->number ||
			    slot != PCI_SLOT(mite->pcidev->devfn))
				continue;
		}

		for (i = 0; i < n_pcimio_boards; i++) {
			if (mite_device_id(mite) == ni_boards[i].device_id) {
				dev->board_ptr = ni_boards + i;
				devpriv->mite = mite;

				return 0;
			}
		}
	}
	pr_warn("no device found\n");
	mite_list_devices();
	return -EIO;
}

static int pcimio_ai_change(struct comedi_device *dev,
			    struct comedi_subdevice *s, unsigned long new_size)
{
	int ret;

	ret = mite_buf_change(devpriv->ai_mite_ring, s->async);
	if (ret < 0)
		return ret;

	return 0;
}

static int pcimio_ao_change(struct comedi_device *dev,
			    struct comedi_subdevice *s, unsigned long new_size)
{
	int ret;

	ret = mite_buf_change(devpriv->ao_mite_ring, s->async);
	if (ret < 0)
		return ret;

	return 0;
}

static int pcimio_gpct0_change(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       unsigned long new_size)
{
	int ret;

	ret = mite_buf_change(devpriv->gpct_mite_ring[0], s->async);
	if (ret < 0)
		return ret;

	return 0;
}

static int pcimio_gpct1_change(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       unsigned long new_size)
{
	int ret;

	ret = mite_buf_change(devpriv->gpct_mite_ring[1], s->async);
	if (ret < 0)
		return ret;

	return 0;
}

static int pcimio_dio_change(struct comedi_device *dev,
			     struct comedi_subdevice *s, unsigned long new_size)
{
	int ret;

	ret = mite_buf_change(devpriv->cdo_mite_ring, s->async);
	if (ret < 0)
		return ret;

	return 0;
}

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
