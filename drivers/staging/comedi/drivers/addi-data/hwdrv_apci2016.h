/*
 * Copyright (C) 2004,2005  ADDI-DATA GmbH for the source code of this module.
 *
 *	ADDI-DATA GmbH
 *	Dieselstrasse 3
 *	D-77833 Ottersweier
 *	Tel: +19(0)7223/9493-0
 *	Fax: +49(0)7223/9493-92
 *	http://www.addi-data.com
 *	info@addi-data.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#define APCI2016_BOARD_VENDOR_ID 0x15B8
#define APCI2016_ADDRESS_RANGE   8


#define APCI2016_DIGITAL_OP                 	0x04
#define APCI2016_DIGITAL_OP_RW                 	4


#define ADDIDATA_ENABLE                            1
#define ADDIDATA_DISABLE                           0


#define ADDIDATA_WATCHDOG                          2
#define APCI2016_DIGITAL_OP_WATCHDOG               0
#define APCI2016_WATCHDOG_ENABLEDISABLE            12
#define APCI2016_WATCHDOG_RELOAD_VALUE             4
#define APCI2016_WATCHDOG_STATUS                   16


int i_APCI2016_ConfigDigitalOutput(struct comedi_device *dev, struct comedi_subdevice *s,
				   struct comedi_insn *insn, unsigned int *data);

int i_APCI2016_WriteDigitalOutput(struct comedi_device *dev, struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data);

int i_APCI2016_BitsDigitalOutput(struct comedi_device *dev, struct comedi_subdevice *s,
				 struct comedi_insn *insn, unsigned int *data);


int i_APCI2016_ConfigWatchdog(struct comedi_device *dev, struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data);

int i_APCI2016_StartStopWriteWatchdog(struct comedi_device *dev, struct comedi_subdevice *s,
				      struct comedi_insn *insn, unsigned int *data);

int i_APCI2016_ReadWatchdog(struct comedi_device *dev, struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data);



int i_APCI2016_Reset(struct comedi_device *dev);
