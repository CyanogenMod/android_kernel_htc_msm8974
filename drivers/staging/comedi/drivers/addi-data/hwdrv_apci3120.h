

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


static const struct comedi_lrange range_apci3120_ai = { 8, {
						     BIP_RANGE(10),
						     BIP_RANGE(5),
						     BIP_RANGE(2),
						     BIP_RANGE(1),
						     UNI_RANGE(10),
						     UNI_RANGE(5),
						     UNI_RANGE(2),
						     UNI_RANGE(1)
						     }
};

static const struct comedi_lrange range_apci3120_ao = { 2, {
						     BIP_RANGE(10),
						     UNI_RANGE(10)
						     }
};

#define APCI3120_BIPOLAR_RANGES	4	

#define APCI3120_BOARD_VENDOR_ID                 0x10E8
#define APCI3120_ADDRESS_RANGE            			16

#define APCI3120_DISABLE                         0
#define APCI3120_ENABLE                          1

#define APCI3120_START                           1
#define APCI3120_STOP                            0

#define     APCI3120_EOC_MODE         1
#define     APCI3120_EOS_MODE         2
#define     APCI3120_DMA_MODE         3


#define APCI3120_DIGITAL_OUTPUT                  	0x0D
#define APCI3120_RD_STATUS                       	0x02
#define APCI3120_RD_FIFO                     		0x00

#define	APCI3120_SET4DIGITALOUTPUTON				1
#define APCI3120_SET4DIGITALOUTPUTOFF				0

#define APCI3120_ANALOG_OP_CHANNEL_1   0x0000
#define APCI3120_ANALOG_OP_CHANNEL_2   0x4000
#define APCI3120_ANALOG_OP_CHANNEL_3   0x8000
#define APCI3120_ANALOG_OP_CHANNEL_4   0xC000
#define APCI3120_ANALOG_OP_CHANNEL_5   0x0000
#define APCI3120_ANALOG_OP_CHANNEL_6   0x4000
#define APCI3120_ANALOG_OP_CHANNEL_7   0x8000
#define APCI3120_ANALOG_OP_CHANNEL_8   0xC000

#define APCI3120_ENABLE_EXT_TRIGGER    0x8000

#define APCI3120_UNIPOLAR 0x80	
#define APCI3120_BIPOLAR  0x00	
#define APCI3120_ANALOG_OUTPUT_1 0x08	
#define APCI3120_ANALOG_OUTPUT_2 0x0A	
#define APCI3120_1_GAIN              0x00	
#define APCI3120_2_GAIN              0x10	
#define APCI3120_5_GAIN              0x20	
#define APCI3120_10_GAIN             0x30	
#define APCI3120_SEQ_RAM_ADDRESS        0x06	
#define APCI3120_RESET_FIFO          0x0C	
#define APCI3120_TIMER_0_MODE_2      0x01	
#define APCI3120_TIMER_0_MODE_4       0x2
#define APCI3120_SELECT_TIMER_0_WORD 0x00
#define APCI3120_ENABLE_TIMER0     0x1000	
#define APCI3120_CLEAR_PR          0xF0FF
#define APCI3120_CLEAR_PA          0xFFF0
#define APCI3120_CLEAR_PA_PR       (APCI3120_CLEAR_PR & APCI3120_CLEAR_PA)

#define APCI3120_ENABLE_SCAN          0x8	
#define APCI3120_DISABLE_SCAN      (~APCI3120_ENABLE_SCAN)
#define APCI3120_ENABLE_EOS_INT       0x2	

#define APCI3120_DISABLE_EOS_INT   (~APCI3120_ENABLE_EOS_INT)
#define APCI3120_ENABLE_EOC_INT       0x1
#define APCI3120_DISABLE_EOC_INT   (~APCI3120_ENABLE_EOC_INT)
#define APCI3120_DISABLE_ALL_INTERRUPT_WITHOUT_TIMER   (APCI3120_DISABLE_EOS_INT & APCI3120_DISABLE_EOC_INT)
#define APCI3120_DISABLE_ALL_INTERRUPT   (APCI3120_DISABLE_TIMER_INT & APCI3120_DISABLE_EOS_INT & APCI3120_DISABLE_EOC_INT)

#define APCI3120_EOC                     0x8000
#define APCI3120_EOS                     0x2000

#define APCI3120_START_CONVERSION        0x02	

#define APCI3120_QUARTZ_A				  70
#define APCI3120_QUARTZ_B				  50
#define APCI3120_TIMER                            1
#define APCI3120_WATCHDOG                         2
#define APCI3120_TIMER_DISABLE                    0
#define APCI3120_TIMER_ENABLE                     1
#define APCI3120_ENABLE_TIMER2                    0x4000	
#define APCI3120_DISABLE_TIMER2                   (~APCI3120_ENABLE_TIMER2)
#define APCI3120_ENABLE_TIMER_INT                 0x04	
#define APCI3120_DISABLE_TIMER_INT                (~APCI3120_ENABLE_TIMER_INT)
#define APCI3120_WRITE_MODE_SELECT                0x0E	
#define APCI3120_SELECT_TIMER_0_WORD  0x00
#define APCI3120_SELECT_TIMER_1_WORD  0x01
#define APCI3120_TIMER_1_MODE_2       0x4

#define APCI3120_TIMER_2_MODE_0                   0x0
#define APCI3120_TIMER_2_MODE_2                   0x10
#define APCI3120_TIMER_2_MODE_5                   0x30

#define APCI3120_SELECT_TIMER_2_LOW_WORD          0x02
#define APCI3120_SELECT_TIMER_2_HIGH_WORD         0x03

#define APCI3120_TIMER_CRT0                       0x0D	
#define APCI3120_TIMER_CRT1                       0x0C	

#define APCI3120_TIMER_VALUE                      0x04	
#define APCI3120_TIMER_STATUS_REGISTER            0x0D	
#define APCI3120_RD_STATUS                        0x02	
#define APCI3120_WR_ADDRESS                       0x00	
#define APCI3120_ENABLE_WATCHDOG                  0x20	
#define APCI3120_DISABLE_WATCHDOG                 (~APCI3120_ENABLE_WATCHDOG)
#define APCI3120_ENABLE_TIMER_COUNTER    		  0x10	
#define APCI3120_DISABLE_TIMER_COUNTER            (~APCI3120_ENABLE_TIMER_COUNTER)
#define APCI3120_FC_TIMER                         0x1000	
#define APCI3120_ENABLE_TIMER0                    0x1000
#define APCI3120_ENABLE_TIMER1                    0x2000
#define APCI3120_ENABLE_TIMER2                    0x4000
#define APCI3120_DISABLE_TIMER0			          (~APCI3120_ENABLE_TIMER0)
#define APCI3120_DISABLE_TIMER1		              (~APCI3120_ENABLE_TIMER1)
#define APCI3120_DISABLE_TIMER2	                  (~APCI3120_ENABLE_TIMER2)

#define APCI3120_TIMER2_SELECT_EOS                0xC0	
#define APCI3120_COUNTER                          3	
#define APCI3120_DISABLE_ALL_TIMER                (APCI3120_DISABLE_TIMER0 & APCI3120_DISABLE_TIMER1 & APCI3120_DISABLE_TIMER2)	

#define MAX_ANALOGINPUT_CHANNELS    32

struct str_AnalogReadInformation {

	unsigned char b_Type;		
	unsigned char b_InterruptFlag;	
	unsigned int ui_ConvertTiming;	
	unsigned char b_NbrOfChannel;	
	unsigned int ui_ChannelList[MAX_ANALOGINPUT_CHANNELS];	
	unsigned int ui_RangeList[MAX_ANALOGINPUT_CHANNELS];	

};



int i_APCI3120_SetupChannelList(struct comedi_device *dev, struct comedi_subdevice *s,
				int n_chan, unsigned int *chanlist, char check);
int i_APCI3120_ExttrigEnable(struct comedi_device *dev);
int i_APCI3120_ExttrigDisable(struct comedi_device *dev);
int i_APCI3120_StopCyclicAcquisition(struct comedi_device *dev, struct comedi_subdevice *s);
int i_APCI3120_Reset(struct comedi_device *dev);
int i_APCI3120_CyclicAnalogInput(int mode, struct comedi_device *dev,
				 struct comedi_subdevice *s);
void v_APCI3120_Interrupt(int irq, void *d);
void v_APCI3120_InterruptDmaMoveBlock16bit(struct comedi_device *dev,
					   struct comedi_subdevice *s,
					   short *dma_buffer,
					   unsigned int num_samples);
int i_APCI3120_InterruptHandleEos(struct comedi_device *dev);
void v_APCI3120_InterruptDma(int irq, void *d);


int i_APCI3120_InsnConfigTimer(struct comedi_device *dev, struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data);
int i_APCI3120_InsnWriteTimer(struct comedi_device *dev, struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data);
int i_APCI3120_InsnReadTimer(struct comedi_device *dev, struct comedi_subdevice *s,
			     struct comedi_insn *insn, unsigned int *data);


int i_APCI3120_InsnBitsDigitalInput(struct comedi_device *dev, struct comedi_subdevice *s,
				    struct comedi_insn *insn, unsigned int *data);
int i_APCI3120_InsnReadDigitalInput(struct comedi_device *dev, struct comedi_subdevice *s,
				    struct comedi_insn *insn, unsigned int *data);

int i_APCI3120_InsnConfigDigitalOutput(struct comedi_device *dev,
				       struct comedi_subdevice *s, struct comedi_insn *insn,
				       unsigned int *data);
int i_APCI3120_InsnBitsDigitalOutput(struct comedi_device *dev, struct comedi_subdevice *s,
				     struct comedi_insn *insn, unsigned int *data);
int i_APCI3120_InsnWriteDigitalOutput(struct comedi_device *dev, struct comedi_subdevice *s,
				      struct comedi_insn *insn, unsigned int *data);


int i_APCI3120_InsnWriteAnalogOutput(struct comedi_device *dev, struct comedi_subdevice *s,
				     struct comedi_insn *insn, unsigned int *data);


int i_APCI3120_InsnConfigAnalogInput(struct comedi_device *dev, struct comedi_subdevice *s,
				     struct comedi_insn *insn, unsigned int *data);
int i_APCI3120_InsnReadAnalogInput(struct comedi_device *dev, struct comedi_subdevice *s,
				   struct comedi_insn *insn, unsigned int *data);
int i_APCI3120_CommandTestAnalogInput(struct comedi_device *dev, struct comedi_subdevice *s,
				      struct comedi_cmd *cmd);
int i_APCI3120_CommandAnalogInput(struct comedi_device *dev, struct comedi_subdevice *s);
int i_APCI3120_StopCyclicAcquisition(struct comedi_device *dev, struct comedi_subdevice *s);
