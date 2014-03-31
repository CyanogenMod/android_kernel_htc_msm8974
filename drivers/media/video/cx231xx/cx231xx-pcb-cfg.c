/*
   cx231xx-pcb-config.c - driver for Conexant
		Cx23100/101/102 USB video capture devices

   Copyright (C) 2008 <srinivasa.deevi at conexant dot com>

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

#include "cx231xx.h"
#include "cx231xx-conf-reg.h"

static unsigned int pcb_debug;
module_param(pcb_debug, int, 0644);
MODULE_PARM_DESC(pcb_debug, "enable pcb config debug messages [video]");


struct pcb_config cx231xx_Scenario[] = {
	{
	 INDEX_SELFPOWER_DIGITAL_ONLY,	
	 USB_SELF_POWER,	
	 0,			
	 MOD_DIGITAL,		
	 SOURCE_TS_BDA,		
	 NOT_SUPPORTED,		
	 NOT_SUPPORTED,		

	 0,			
	 0,			
	 0,			
	 0,			

	 1,			
	 {
	  {
	   0,			
	   {
	    0,			
	    1,			
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED	
	    }
	   ,
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  }
	 ,
	 
	 {
	  {
	   0,			
	   {
	    0,			
	    1,			
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED	
	    }
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  }
	 }
	,

	{
	 INDEX_SELFPOWER_DUAL_DIGITAL,	
	 USB_SELF_POWER,	
	 0,			
	 MOD_DIGITAL,		
	 SOURCE_TS_BDA,		
	 0,			
	 NOT_SUPPORTED,		
	 0,			
	 0,			
	 0,			
	 0,			

	 1,			
	 {
	  {
	   0,			
	   {
	    0,			
	    1,			
	    2,			
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED	
	    }
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  }
	 ,
	 
	 {
	  {
	   0,			
	   {
	    0,			
	    1,			
	    2,			
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED	
	    }
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  }
	 }
	,

	{
	 INDEX_SELFPOWER_ANALOG_ONLY,	
	 USB_SELF_POWER,	
	 0,			
	 MOD_ANALOG | MOD_DIF | MOD_EXTERNAL,	
	 NOT_SUPPORTED,		
	 NOT_SUPPORTED,		
	 0,			

	 0,			
	 0,			
	 0,			
	 0,			

	 1,			
	 {
	  {
	   0,			
	   {
	    0,			
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    1,			
	    2,			
	    3,			
	    4,			
	    NOT_SUPPORTED	
	    }
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  }
	 ,
	 
	 {
	  {
	   0,			
	   {
	    0,			
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    1,			
	    2,			
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED	
	    }
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  }
	 }
	,

	{
	 INDEX_SELFPOWER_DUAL,	
	 USB_SELF_POWER,	
	 0,			
	 
	 MOD_ANALOG | MOD_DIF | MOD_DIGITAL | MOD_EXTERNAL,
	 0,			
	 NOT_SUPPORTED,		
	 0,			
	 0,			
	 0,			
	 0,			
	 0,			
	 1,			
	 {
	  {
	   0,			
	   {
	    0,			
	    1,			
	    NOT_SUPPORTED,	
	    2,			
	    3,			
	    4,			
	    5,			
	    NOT_SUPPORTED	
	    }
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  }
	 ,
	 
	 {
	  {
	   0,			
	   {
	    0,			
	    1,			
	    NOT_SUPPORTED,	
	    2,			
	    3,			
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED	
	    }
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  }
	 }
	,

	{
	 INDEX_SELFPOWER_TRIPLE,	
	 USB_SELF_POWER,	
	 0,			
	 
	 MOD_ANALOG | MOD_DIF | MOD_DIGITAL | MOD_EXTERNAL,
	 0,			
	 0,			
	 0,			

	 0,			
	 0,			
	 0,			
	 0,			
	 1,			
	 {
	  {
	   0,			
	   {
	    0,			
	    1,			
	    2,			
	    3,			
	    4,			
	    5,			
	    6,			
	    NOT_SUPPORTED	
	    }
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  }
	 ,
	 
	 {
	  {
	   0,			
	   {
	    0,			
	    1,			
	    2,			
	    3,			
	    4,			
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED	
	    }
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  }
	 }
	,

	{
	 INDEX_SELFPOWER_COMPRESSOR,	
	 USB_SELF_POWER,	
	 0,			
	 
	 MOD_ANALOG | MOD_DIF | MOD_DIGITAL | MOD_EXTERNAL,
	 NOT_SUPPORTED,		
	 SOURCE_TS_BDA,		
	 0,			
	 0,			
	 0,			
	 0,			
	 0,			
	 1,			
	 {
	  {
	   0,			
	   {
	    0,			
	    NOT_SUPPORTED,	
	    1,			
	    2,			
	    3,			
	    4,			
	    5,			
	    NOT_SUPPORTED	
	    }
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  }
	 ,
	 
	 {
	  {
	   0,			
	   {
	    0,			
	    NOT_SUPPORTED,	
	    1,			
	    2,			
	    3,			
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED	
	    }
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  }
	 }
	,

	{
	 INDEX_BUSPOWER_DIGITAL_ONLY,	
	 USB_BUS_POWER,		
	 0,			
	 MOD_DIGITAL,		
	 SOURCE_TS_BDA,		
	 NOT_SUPPORTED,		
	 NOT_SUPPORTED,		

	 0,			
	 0,			
	 0,			
	 0,			

	 1,			
	 {
	  {
	   0,			
	   {
	    0,			
	    1,			
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED	
	    }
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  }
	 ,
	 
	 {
	  {
	   0,			
	   {
	    0,			
	    1,			
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED	
	    }
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  }
	 }
	,
	{
	 INDEX_BUSPOWER_ANALOG_ONLY,	
	 USB_BUS_POWER,		
	 0,			
	 MOD_ANALOG,		
	 NOT_SUPPORTED,		
	 NOT_SUPPORTED,		
	 SOURCE_ANALOG,		
	 0,			
	 0,			
	 0,			
	 0,			
	 1,			
	 {
	  {
	   0,			
	   {
	    0,			
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    1,			
	    2,			
	    3,			
	    4,			
	    NOT_SUPPORTED	
	    }
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  }
	 ,
	 {			
	  {
	   0,			
	   {
	    0,			
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    1,			
	    2,			
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED	
	    }
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  }
	 }
	,
	{
	 INDEX_BUSPOWER_DIF_ONLY,	
	 USB_BUS_POWER,		
	 0,			
	 
	 MOD_DIF | MOD_ANALOG | MOD_DIGITAL | MOD_EXTERNAL,
	 SOURCE_TS_BDA,		
	 NOT_SUPPORTED,		
	 SOURCE_DIF | SOURCE_ANALOG | SOURCE_EXTERNAL,	
	 0,			
	 0,			
	 0,			
	 0,			
	 1,			
	 {
	  {
	   0,			
	   {
	    0,			
	    1,			
	    NOT_SUPPORTED,	
	    2,			
	    3,			
	    4,			
	    5,			
	    NOT_SUPPORTED	
	    }
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  }
	 ,
	 {			
	  {
	   0,			
	   {
	    0,			
	    1,			
	    NOT_SUPPORTED,	
	    2,			
	    3,			
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED,	
	    NOT_SUPPORTED	
	    }
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  ,
	  {NOT_SUPPORTED, {NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED, NOT_SUPPORTED, NOT_SUPPORTED,
			   NOT_SUPPORTED}
	   }
	  }
	 }
	,

};


u32 initialize_cx231xx(struct cx231xx *dev)
{
	u32 config_info = 0;
	struct pcb_config *p_pcb_info;
	u8 usb_speed = 1;	
	u8 data[4] = { 0, 0, 0, 0 };
	u32 ts1_source = 0;
	u32 ts2_source = 0;
	u32 analog_source = 0;
	u8 _current_scenario_idx = 0xff;

	ts1_source = SOURCE_TS_BDA;
	ts2_source = SOURCE_TS_BDA;

	cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, BOARD_CFG_STAT, data, 4);

	config_info = *((u32 *) data);
	usb_speed = (u8) (config_info & 0x1);

	
	if (config_info & BUS_POWER) {	
		switch (config_info & BUSPOWER_MASK) {
		case TS1_PORT | BUS_POWER:
			cx231xx_Scenario[INDEX_BUSPOWER_DIGITAL_ONLY].speed =
			    usb_speed;
			p_pcb_info =
			    &cx231xx_Scenario[INDEX_BUSPOWER_DIGITAL_ONLY];
			_current_scenario_idx = INDEX_BUSPOWER_DIGITAL_ONLY;
			break;
		case AVDEC_ENABLE | BUS_POWER:
			cx231xx_Scenario[INDEX_BUSPOWER_ANALOG_ONLY].speed =
			    usb_speed;
			p_pcb_info =
			    &cx231xx_Scenario[INDEX_BUSPOWER_ANALOG_ONLY];
			_current_scenario_idx = INDEX_BUSPOWER_ANALOG_ONLY;
			break;
		case AVDEC_ENABLE | BUS_POWER | TS1_PORT:
			cx231xx_Scenario[INDEX_BUSPOWER_DIF_ONLY].speed =
			    usb_speed;
			p_pcb_info = &cx231xx_Scenario[INDEX_BUSPOWER_DIF_ONLY];
			_current_scenario_idx = INDEX_BUSPOWER_DIF_ONLY;
			break;
		default:
			cx231xx_info("bad config in buspower!!!!\n");
			cx231xx_info("config_info=%x\n",
				     (config_info & BUSPOWER_MASK));
			return 1;
		}
	} else {		

		switch (config_info & SELFPOWER_MASK) {
		case TS1_PORT | SELF_POWER:
			cx231xx_Scenario[INDEX_SELFPOWER_DIGITAL_ONLY].speed =
			    usb_speed;
			p_pcb_info =
			    &cx231xx_Scenario[INDEX_SELFPOWER_DIGITAL_ONLY];
			_current_scenario_idx = INDEX_SELFPOWER_DIGITAL_ONLY;
			break;
		case TS1_TS2_PORT | SELF_POWER:
			cx231xx_Scenario[INDEX_SELFPOWER_DUAL_DIGITAL].speed =
			    usb_speed;
			cx231xx_Scenario[INDEX_SELFPOWER_DUAL_DIGITAL].
			    ts2_source = ts2_source;
			p_pcb_info =
			    &cx231xx_Scenario[INDEX_SELFPOWER_DUAL_DIGITAL];
			_current_scenario_idx = INDEX_SELFPOWER_DUAL_DIGITAL;
			break;
		case AVDEC_ENABLE | SELF_POWER:
			cx231xx_Scenario[INDEX_SELFPOWER_ANALOG_ONLY].speed =
			    usb_speed;
			cx231xx_Scenario[INDEX_SELFPOWER_ANALOG_ONLY].
			    analog_source = analog_source;
			p_pcb_info =
			    &cx231xx_Scenario[INDEX_SELFPOWER_ANALOG_ONLY];
			_current_scenario_idx = INDEX_SELFPOWER_ANALOG_ONLY;
			break;
		case AVDEC_ENABLE | TS1_PORT | SELF_POWER:
			cx231xx_Scenario[INDEX_SELFPOWER_DUAL].speed =
			    usb_speed;
			cx231xx_Scenario[INDEX_SELFPOWER_DUAL].ts1_source =
			    ts1_source;
			cx231xx_Scenario[INDEX_SELFPOWER_DUAL].analog_source =
			    analog_source;
			p_pcb_info = &cx231xx_Scenario[INDEX_SELFPOWER_DUAL];
			_current_scenario_idx = INDEX_SELFPOWER_DUAL;
			break;
		case AVDEC_ENABLE | TS1_TS2_PORT | SELF_POWER:
			cx231xx_Scenario[INDEX_SELFPOWER_TRIPLE].speed =
			    usb_speed;
			cx231xx_Scenario[INDEX_SELFPOWER_TRIPLE].ts1_source =
			    ts1_source;
			cx231xx_Scenario[INDEX_SELFPOWER_TRIPLE].ts2_source =
			    ts2_source;
			cx231xx_Scenario[INDEX_SELFPOWER_TRIPLE].analog_source =
			    analog_source;
			p_pcb_info = &cx231xx_Scenario[INDEX_SELFPOWER_TRIPLE];
			_current_scenario_idx = INDEX_SELFPOWER_TRIPLE;
			break;
		case AVDEC_ENABLE | TS1VIP_TS2_PORT | SELF_POWER:
			cx231xx_Scenario[INDEX_SELFPOWER_COMPRESSOR].speed =
			    usb_speed;
			cx231xx_Scenario[INDEX_SELFPOWER_COMPRESSOR].
			    analog_source = analog_source;
			p_pcb_info =
			    &cx231xx_Scenario[INDEX_SELFPOWER_COMPRESSOR];
			_current_scenario_idx = INDEX_SELFPOWER_COMPRESSOR;
			break;
		default:
			cx231xx_info("bad senario!!!!!\n");
			cx231xx_info("config_info=%x\n",
				     (config_info & SELFPOWER_MASK));
			return 1;
		}
	}

	dev->current_scenario_idx = _current_scenario_idx;

	memcpy(&dev->current_pcb_config, p_pcb_info,
		   sizeof(struct pcb_config));

	if (pcb_debug) {
		cx231xx_info("SC(0x00) register = 0x%x\n", config_info);
		cx231xx_info("scenario %d\n",
			    (dev->current_pcb_config.index) + 1);
		cx231xx_info("type=%x\n", dev->current_pcb_config.type);
		cx231xx_info("mode=%x\n", dev->current_pcb_config.mode);
		cx231xx_info("speed=%x\n", dev->current_pcb_config.speed);
		cx231xx_info("ts1_source=%x\n",
			     dev->current_pcb_config.ts1_source);
		cx231xx_info("ts2_source=%x\n",
			     dev->current_pcb_config.ts2_source);
		cx231xx_info("analog_source=%x\n",
			     dev->current_pcb_config.analog_source);
	}

	return 0;
}
