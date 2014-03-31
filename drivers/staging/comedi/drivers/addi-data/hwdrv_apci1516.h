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


#define APCI1516_BOARD_VENDOR_ID                 0x15B8
#define APCI1516_ADDRESS_RANGE                   8


#define APCI1516_DIGITAL_OP                 	4
#define APCI1516_DIGITAL_OP_RW                 	4
#define APCI1516_DIGITAL_IP                     0


#define ADDIDATA_WATCHDOG                          2
#define APCI1516_DIGITAL_OP_WATCHDOG               0
#define APCI1516_WATCHDOG_ENABLEDISABLE            12
#define APCI1516_WATCHDOG_RELOAD_VALUE             4
#define APCI1516_WATCHDOG_STATUS                   16


int i_APCI1516_ReadMoreDigitalInput(struct comedi_device *dev, struct comedi_subdevice *s,
				    struct comedi_insn *insn, unsigned int *data);
int i_APCI1516_Read1DigitalInput(struct comedi_device *dev, struct comedi_subdevice *s,
				 struct comedi_insn *insn, unsigned int *data);

int i_APCI1516_ConfigDigitalOutput(struct comedi_device *dev, struct comedi_subdevice *s,
				   struct comedi_insn *insn, unsigned int *data);
int i_APCI1516_WriteDigitalOutput(struct comedi_device *dev, struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data);
int i_APCI1516_ReadDigitalOutput(struct comedi_device *dev, struct comedi_subdevice *s,
				 struct comedi_insn *insn, unsigned int *data);

int i_APCI1516_ConfigWatchdog(struct comedi_device *dev, struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data);
int i_APCI1516_StartStopWriteWatchdog(struct comedi_device *dev, struct comedi_subdevice *s,
				      struct comedi_insn *insn, unsigned int *data);
int i_APCI1516_ReadWatchdog(struct comedi_device *dev, struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data);

int i_APCI1516_Reset(struct comedi_device *dev);
