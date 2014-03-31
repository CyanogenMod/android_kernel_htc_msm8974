/*
 *
 * device driver for philips saa7134 based TV cards
 * card-specific stuff.
 *
 * (c) 2001-04 Gerd Knorr <kraxel@bytesex.org> [SuSE Labs]
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#include "saa7134-reg.h"
#include "saa7134.h"
#include "tuner-xc2028.h"
#include <media/v4l2-common.h>
#include <media/tveeprom.h>
#include "tea5767.h"
#include "tda18271.h"
#include "xc5000.h"
#include "s5h1411.h"

static char name_mute[]    = "mute";
static char name_radio[]   = "Radio";
static char name_tv[]      = "Television";
static char name_tv_mono[] = "TV (mono only)";
static char name_comp[]    = "Composite";
static char name_comp1[]   = "Composite1";
static char name_comp2[]   = "Composite2";
static char name_comp3[]   = "Composite3";
static char name_comp4[]   = "Composite4";
static char name_svideo[]  = "S-Video";



struct saa7134_board saa7134_boards[] = {
	[SAA7134_BOARD_UNKNOWN] = {
		.name		= "UNKNOWN/GENERIC",
		.audio_clock	= 0x00187de7,
		.tuner_type	= TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.inputs         = {{
			.name = "default",
			.vmux = 0,
			.amux = LINE1,
		}},
	},
	[SAA7134_BOARD_PROTEUS_PRO] = {
		
		.name		= "Proteus Pro [philips reference design]",
		.audio_clock	= 0x00187de7,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.inputs         = {{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_FLYVIDEO3000] = {
		
		.name		= "LifeView FlyVIDEO3000",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.gpiomask       = 0xe000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x8000,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x4000,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x4000,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x4000,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x2000,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x8000,
		},
	},
	[SAA7134_BOARD_FLYVIDEO2000] = {
		
		.name           = "LifeView/Typhoon FlyVIDEO2000",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_LG_PAL_NEW_TAPC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.gpiomask       = 0xe000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x4000,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x4000,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x4000,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x2000,
		},
		.mute = {
			.name = name_mute,
			.amux = LINE2,
			.gpio = 0x8000,
		},
	},
	[SAA7134_BOARD_FLYTVPLATINUM_MINI] = {
		
		.name           = "LifeView FlyTV Platinum Mini",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,     
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,	
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_FLYTVPLATINUM_FM] = {
		
		
		.name           = "LifeView FlyTV Platinum FM / Gold",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.gpiomask       = 0x1E000,	
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x10000,	
			.tv   = 1,
		},{
			.name = name_comp1,	
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,	
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,	
			.vmux = 8,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x00000,	
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x10000,
		},
	},
	[SAA7134_BOARD_ROVERMEDIA_LINK_PRO_FM] = {
		
		
		.name		= "RoverMedia TV Link Pro FM",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_PHILIPS_FM1216ME_MK3, 
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0xe000,
		.inputs         = { {
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x8000,
			.tv   = 1,
		}, {
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x4000,
		}, {
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x4000,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x4000,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x2000,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x8000,
		},
	},
	[SAA7134_BOARD_EMPRESS] = {
		
		.name		= "EMPRESS",
		.audio_clock	= 0x00187de7,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.empress_addr 	= 0x20,

		.inputs         = {{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
		.mpeg      = SAA7134_MPEG_EMPRESS,
		.video_out = CCIR656,
	},
	[SAA7134_BOARD_MONSTERTV] = {
		
		.name           = "SKNet Monster TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_MD9717] = {
		.name		= "Tevion MD 9717",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	       .mute = {
		       .name = name_mute,
		       .amux = TV,
	       },
	},
	[SAA7134_BOARD_TVSTATION_RDS] = {
		
		.name		= "KNC One TV-Station RDS / Typhoon TV Tuner RDS",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			.amux   = LINE2,
			.tv   = 1,
		},{

			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{

			.name = "CVid over SVid",
			.vmux = 0,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_TVSTATION_DVR] = {
		.name		= "KNC One TV-Station DVR",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.empress_addr 	= 0x20,
		.tda9887_conf	= TDA9887_PRESENT,
		.gpiomask	= 0x820000,
		.inputs		= {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x20000,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x20000,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x20000,
		}},
		.radio		= {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x20000,
		},
		.mpeg           = SAA7134_MPEG_EMPRESS,
		.video_out	= CCIR656,
	},
	[SAA7134_BOARD_CINERGY400] = {
		.name           = "Terratec Cinergy 400 TV",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp2, 
			.vmux = 0,
			.amux = LINE1,
		}}
	},
	[SAA7134_BOARD_MD5044] = {
		.name           = "Medion 5044",
		.audio_clock    = 0x00187de7, 
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_KWORLD] = {
		.name           = "Kworld/KuroutoShikou SAA7130-TVPCI",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_CINERGY600] = {
		.name           = "Terratec Cinergy 600 TV",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp2, 
			.vmux = 0,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_MD7134] = {
		.name           = "Medion 7134",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FMD1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE2,
	       },
	       .mute = {
		       .name = name_mute,
		       .amux = TV,
		},
	},
	[SAA7134_BOARD_TYPHOON_90031] = {
		
		
		.name           = "Typhoon TV+Radio 90031",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE2,
		},
	},
	[SAA7134_BOARD_ELSA] = {
		.name           = "ELSA EX-VISION 300TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_HITACHI_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name = name_tv,
			.vmux = 4,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_ELSA_500TV] = {
		.name           = "ELSA EX-VISION 500TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_HITACHI_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 7,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 8,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 8,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_ELSA_700TV] = {
		.name           = "ELSA EX-VISION 700TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_HITACHI_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 4,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 6,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 7,
			.amux = LINE1,
		}},
		.mute           = {
			.name = name_mute,
			.amux = TV,
		},
	},
	[SAA7134_BOARD_ASUSTeK_TVFM7134] = {
		.name           = "ASUS TV-FM 7134",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_ASUSTeK_TVFM7135] = {
		.name           = "ASUS TV-FM 7135",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0x200000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE2,
			.gpio = 0x0000,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE2,
			.gpio = 0x0000,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x200000,
		},
		.mute  = {
			.name = name_mute,
			.gpio = 0x0000,
		},

	},
	[SAA7134_BOARD_VA1000POWER] = {
		.name           = "AOPEN VA1000 POWER",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_10MOONSTVMASTER] = {
		
		.name           = "10MOONS PCI TV CAPTURE CARD",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_LG_PAL_NEW_TAPC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0xe000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x4000,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x4000,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x4000,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x2000,
		},
		.mute = {
			.name = name_mute,
			.amux = LINE2,
			.gpio = 0x8000,
		},
	},
	[SAA7134_BOARD_BMK_MPEX_NOTUNER] = {
		
		.name		= "BMK MPEX No Tuner",
		.audio_clock	= 0x200000,
		.tuner_type	= TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.empress_addr 	= 0x20,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE1,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_comp3,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_comp4,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.mpeg      = SAA7134_MPEG_EMPRESS,
		.video_out = CCIR656,
	},
	[SAA7134_BOARD_VIDEOMATE_TV] = {
		.name           = "Compro VideoMate TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_VIDEOMATE_TV_GOLD_PLUS] = {
		.name           = "Compro VideoMate TV Gold+",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.gpiomask       = 0x800c0000,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x06c00012,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x0ac20012,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x08c20012,
			.tv   = 1,
		}},				
	},
	[SAA7134_BOARD_CRONOS_PLUS] = {
		.name           = "Matrox CronosPlus",
		.tuner_type     = TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0xcf00,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 0,
			.gpio = 2 << 14,
		},{
			.name = name_comp2,
			.vmux = 0,
			.gpio = 1 << 14,
		},{
			.name = name_comp3,
			.vmux = 0,
			.gpio = 0 << 14,
		},{
			.name = name_comp4,
			.vmux = 0,
			.gpio = 3 << 14,
		},{
			.name = name_svideo,
			.vmux = 8,
			.gpio = 2 << 14,
		}},
	},
	[SAA7134_BOARD_MD2819] = {
		.name           = "AverMedia M156 / Medion 2819",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask	= 0x03,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x00,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x02,
		}, {
			.name = name_comp2,
			.vmux = 0,
			.amux = LINE1,
			.gpio = 0x02,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x02,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE1,
			.gpio = 0x01,
		},
		.mute  = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x00,
		},
	},
	[SAA7134_BOARD_BMK_MPEX_TUNER] = {
		
		.name           = "BMK MPEX Tuner",
		.audio_clock    = 0x200000,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.empress_addr 	= 0x20,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}},
		.mpeg      = SAA7134_MPEG_EMPRESS,
		.video_out = CCIR656,
	},
	[SAA7134_BOARD_ASUSTEK_TVFM7133] = {
		.name           = "ASUS TV-FM 7133",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_LG_NTSC_NEW_TAPC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,

		},{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_PINNACLE_PCTV_STEREO] = {
		.name           = "Pinnacle PCTV Stereo (saa7134)",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_MT2032,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT | TDA9887_INTERCARRIER | TDA9887_PORT2_INACTIVE,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 1,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_MANLI_MTV002] = {
		
		.name           = "Manli MuchTV M-TV002",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name   = name_comp1,
			.vmux   = 1,
			.amux   = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_MANLI_MTV001] = {
		
		.name           = "Manli MuchTV M-TV001",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}},
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_TG3000TV] = {
		
		.name           = "Nagase Sangyo TransGear 3000TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_ECS_TVP3XP] = {
		.name           = "Elitegroup ECS TVP3XP FM1216 Tuner Card(PAL-BG,FM) ",
		.audio_clock    = 0x187de7,  
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_tv_mono,
			.vmux   = 1,
			.amux   = LINE2,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		},{
			.name   = "CVid over SVid",
			.vmux   = 0,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE2,
		},
	},
	[SAA7134_BOARD_ECS_TVP3XP_4CB5] = {
		.name           = "Elitegroup ECS TVP3XP FM1236 Tuner Card (NTSC,FM)",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_tv_mono,
			.vmux   = 1,
			.amux   = LINE2,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		},{
			.name   = "CVid over SVid",
			.vmux   = 0,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE2,
		},
	},
    [SAA7134_BOARD_ECS_TVP3XP_4CB6] = {
		
		.name		= "Elitegroup ECS TVP3XP FM1246 Tuner Card (PAL,FM)",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_PHILIPS_PAL_I,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_tv_mono,
			.vmux   = 1,
			.amux   = LINE2,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		},{
			.name   = "CVid over SVid",
			.vmux   = 0,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE2,
		},
	},
	[SAA7134_BOARD_AVACSSMARTTV] = {
		
		.name           = "AVACS SmartTV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x200000,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_DVD_EZMAKER] = {
		
		.name           = "AVerMedia DVD EZMaker",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 3,
		},{
			.name = name_svideo,
			.vmux = 8,
		}},
	},
	[SAA7134_BOARD_AVERMEDIA_M103] = {
		
		.name           = "AVerMedia MiniPCI DVB-T Hybrid M103",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_XC2028,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		 .mpeg           = SAA7134_MPEG_DVB,
		 .inputs         = {{
			 .name = name_tv,
			 .vmux = 1,
			 .amux = TV,
			 .tv   = 1,
		 } },
	},
	[SAA7134_BOARD_NOVAC_PRIMETV7133] = {
		
		.name           = "Noval Prime TV 7133",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_ALPS_TSBH1_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 3,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_svideo,
			.vmux = 8,
		}},
	},
	[SAA7134_BOARD_AVERMEDIA_STUDIO_305] = {
		.name           = "AverMedia AverTV Studio 305",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1256_IH3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_STUDIO_505] = {
		
		.name           = "AverMedia AverTV Studio 505",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = { {
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		}, {
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_UPMOST_PURPLE_TV] = {
		.name           = "UPMOST PURPLE TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1236_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 7,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_svideo,
			.vmux = 7,
			.amux = LINE1,
		}},
	},
	[SAA7134_BOARD_ITEMS_MTV005] = {
		
		.name           = "Items MuchTV Plus / IT-005",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		},{
			.name   = name_comp1,
			.vmux   = 1,
			.amux   = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_CINERGY200] = {
		.name           = "Terratec Cinergy 200 TV",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp2, 
			.vmux = 0,
			.amux = LINE1,
		}},
		.mute = {
			.name = name_mute,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_VIDEOMATE_TV_PVR] = {
		
		.name           = "Compro VideoMate TV PVR/FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask	= 0x808c0080,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x00080,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x00080,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2_LEFT,
			.tv   = 1,
			.gpio = 0x00080,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x80000,
		},
		.mute = {
			.name = name_mute,
			.amux = LINE2,
			.gpio = 0x40000,
		},
	},
	[SAA7134_BOARD_SABRENT_SBTTVFM] = {
		
		.name           = "Sabrent SBT-TVFM (saa7130)",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE2,
		},
	},
	[SAA7134_BOARD_ZOLID_XPERT_TV7134] = {
		
		.name           = ":Zolid Xpert TV7134",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_EMPIRE_PCI_TV_RADIO_LE] = {
		
		.name           = "Empire PCI TV-Radio LE",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0x4000,
		.inputs         = {{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x8000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x8000,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE1,
			.gpio = 0x8000,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
			.gpio = 0x8000,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio =0x8000,
		}
	},
	[SAA7134_BOARD_AVERMEDIA_STUDIO_307] = {
		.name           = "Avermedia AVerTV Studio 307",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1256_IH3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x03,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x00,
		},{
			.name = name_comp,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x02,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x02,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
			.gpio = 0x01,
		},
		.mute  = {
			.name = name_mute,
			.amux = LINE1,
			.gpio = 0x00,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_GO_007_FM] = {
		.name           = "Avermedia AVerTV GO 007 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0x00300003,
		
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x01,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
			.gpio = 0x02,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE1,
			.gpio = 0x02,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x00300001,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x01,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_CARDBUS] = {
		
		.name           = "AVerMedia Cardbus TV/Radio (E500)",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_CARDBUS_501] = {
		
		.name           = "AVerMedia Cardbus TV/Radio (E501R)",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_ALPS_TSBE5_PAL,
		.radio_type     = TUNER_TEA5767,
		.tuner_addr	= 0x61,
		.radio_addr	= 0x60,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x08000000,
		.inputs         = { {
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x08000000,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x08000000,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x08000000,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x00000000,
		},
	},
	[SAA7134_BOARD_CINERGY400_CARDBUS] = {
		.name           = "Terratec Cinergy 400 mobile",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_ALPS_TSBE5_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
	},
	[SAA7134_BOARD_CINERGY600_MK3] = {
		.name           = "Terratec Cinergy 600 TV MK3",
		.audio_clock    = 0x00200000,
		.tuner_type	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.rds_addr 	= 0x10,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp2, 
			.vmux = 0,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_VIDEOMATE_GOLD_PLUS] = {
		
		.name		= "Compro VideoMate Gold+ Pal",
		.audio_clock	= 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask	= 0x1ce780,
		.inputs		= {{
			.name = name_svideo,
			.vmux = 0,		
			.amux = LINE1,
			.gpio = 0x008080,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x008080,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x008080,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x80000,
		},
		.mute = {
			.name = name_mute,
			.amux = LINE2,
			.gpio = 0x0c8000,
		},
	},
	[SAA7134_BOARD_PINNACLE_300I_DVBT_PAL] = {
		.name           = "Pinnacle PCTV 300i DVB-T + PAL",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_MT2032,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT | TDA9887_INTERCARRIER | TDA9887_PORT2_INACTIVE,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 1,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_PROVIDEO_PV952] = {
		
		.name		= "ProVideo PV952",
		.audio_clock	= 0x00187de7,
		.tuner_type	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_305] = {
		.name           = "AverMedia AverTV/305",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FQ1216ME,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_FLYDVBTDUO] = {
		
		
		.name           = "LifeView FlyDVB-T DUO / MSI TV@nywhere Duo",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask	= 0x00200000,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x200000,	
			.tv   = 1,
		},{
			.name = name_comp1,	
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,	
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,	
			.vmux = 8,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x000000,	
		},
	},
	[SAA7134_BOARD_PHILIPS_TOUGH] = {
		.name           = "Philips TOUGH DVB-T reference design",
		.tuner_type	= TUNER_ABSENT,
		.audio_clock    = 0x00187de7,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
	},
	[SAA7134_BOARD_AVERMEDIA_307] = {
		.name           = "Avermedia AVerTV 307",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FQ1216ME,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
	},
	[SAA7134_BOARD_ADS_INSTANT_TV] = {
		.name           = "ADS Tech Instant TV (saa7135)",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_KWORLD_VSTREAM_XPERT] = {
		.name           = "Kworld/Tevion V-Stream Xpert TV PVR7134",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_PAL_I,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask	= 0x0700,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
			.gpio   = 0x000,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
			.gpio   = 0x200,		
		},{
			.name   = name_svideo,
			.vmux   = 0,
			.amux   = LINE1,
			.gpio   = 0x200,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE1,
			.gpio   = 0x100,
		},
		.mute  = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x000,
		},
	},
	[SAA7134_BOARD_FLYDVBT_DUO_CARDBUS] = {
		.name		= "LifeView/Typhoon/Genius FlyDVB-T Duo Cardbus",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask	= 0x00200000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x200000,	
			.tv   = 1,
		},{
			.name = name_svideo,	
			.vmux = 8,
			.amux = LINE2,
		},{
			.name = name_comp1,	
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,	
			.vmux = 3,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x000000,	
		},
	},
	[SAA7134_BOARD_VIDEOMATE_TV_GOLD_PLUSII] = {
		.name           = "Compro VideoMate TV Gold+II",
		.audio_clock    = 0x002187de7,
		.tuner_type     = TUNER_LG_PAL_NEW_TAPC,
		.radio_type     = TUNER_TEA5767,
		.tuner_addr     = 0x63,
		.radio_addr     = 0x60,
		.gpiomask       = 0x8c1880,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 0,
			.amux = LINE1,
			.gpio = 0x800800,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x801000,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x800000,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x880000,
		},
		.mute = {
			.name = name_mute,
			.amux = LINE2,
			.gpio = 0x840000,
		},
	},
	[SAA7134_BOARD_KWORLD_XPERT] = {
		.name           = "Kworld Xpert TV PVR7134",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_TENA_9533_DI,
		.radio_type     = TUNER_TEA5767,
		.tuner_addr	= 0x61,
		.radio_addr	= 0x60,
		.gpiomask	= 0x0700,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
			.gpio   = 0x000,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
			.gpio   = 0x200,		
		},{
			.name   = name_svideo,
			.vmux   = 0,
			.amux   = LINE1,
			.gpio   = 0x200,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE1,
			.gpio   = 0x100,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x000,
		},
	},
	[SAA7134_BOARD_FLYTV_DIGIMATRIX] = {
		.name		= "FlyTV mini Asus Digimatrix",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_LG_TALN,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,		
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_KWORLD_TERMINATOR] = {
		
		
		.name           = "V-Stream Studio TV Terminator",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.gpiomask       = 1 << 21,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x0000000,
			.tv   = 1,
		},{
			.name = name_comp1,     
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x0000000,
		},{
			.name = name_svideo,    
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x0000000,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x0200000,
		},
	},
	[SAA7134_BOARD_YUAN_TUN900] = {
		.name           = "Yuan TUN-900 (saa7135)",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr= ADDR_UNSET,
		.radio_addr= ADDR_UNSET,
		.gpiomask       = 0x00010003,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x01,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x02,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE2,
			.gpio = 0x02,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
			.gpio = 0x00010003,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x01,
		},
	},
	[SAA7134_BOARD_BEHOLD_409FM] = {
		
		
		
		.name           = "Beholder BeholdTV 409 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			  .name = name_tv,
			  .vmux = 3,
			  .amux = TV,
			  .tv   = 1,
		},{
			  .name = name_comp1,
			  .vmux = 1,
			  .amux = LINE1,
		},{
			  .name = name_svideo,
			  .vmux = 8,
			  .amux = LINE1,
		}},
		.radio = {
			  .name = name_radio,
			  .amux = LINE2,
		},
	},
	[SAA7134_BOARD_GOTVIEW_7135] = {
		
		
		.name            = "GoTView 7135 PCI",
		.audio_clock     = 0x00187de7,
		.tuner_type      = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type      = UNSET,
		.tuner_addr      = ADDR_UNSET,
		.radio_addr      = ADDR_UNSET,
		.tda9887_conf    = TDA9887_PRESENT,
		.gpiomask        = 0x00200003,
		.inputs          = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x00200003,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x00200003,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x00200003,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x00200003,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x00200003,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x00200003,
		},
	},
	[SAA7134_BOARD_PHILIPS_EUROPA] = {
		.name           = "Philips EUROPA V3 reference design",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TD1316,
		.radio_type     = UNSET,
		.tuner_addr	= 0x61,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT | TDA9887_PORT1_ACTIVE,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 3,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE2,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE2,
		}},
	},
	[SAA7134_BOARD_VIDEOMATE_DVBT_300] = {
		.name           = "Compro Videomate DVB-T300",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TD1316,
		.radio_type     = UNSET,
		.tuner_addr	= 0x61,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT | TDA9887_PORT1_ACTIVE,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 3,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 1,
			.amux   = LINE2,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE2,
		}},
	},
	[SAA7134_BOARD_VIDEOMATE_DVBT_200] = {
		.name           = "Compro Videomate DVB-T200",
		.tuner_type	= TUNER_ABSENT,
		.audio_clock    = 0x00187de7,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
	},
	[SAA7134_BOARD_RTD_VFG7350] = {
		.name		= "RTD Embedded Technologies VFG7350",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_ABSENT,
		.radio_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.empress_addr 	= 0x21,
		.inputs		= {{
			.name   = "Composite 0",
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = "Composite 1",
			.vmux   = 1,
			.amux   = LINE2,
		},{
			.name   = "Composite 2",
			.vmux   = 2,
			.amux   = LINE1,
		},{
			.name   = "Composite 3",
			.vmux   = 3,
			.amux   = LINE2,
		},{
			.name   = "S-Video 0",
			.vmux   = 8,
			.amux   = LINE1,
		},{
			.name   = "S-Video 1",
			.vmux   = 9,
			.amux   = LINE2,
		}},
		.mpeg           = SAA7134_MPEG_EMPRESS,
		.video_out      = CCIR656,
		.vid_port_opts  = ( SET_T_CODE_POLARITY_NON_INVERTED |
				    SET_CLOCK_NOT_DELAYED |
				    SET_CLOCK_INVERTED |
				    SET_VSYNC_OFF ),
	},
	[SAA7134_BOARD_RTD_VFG7330] = {
		.name		= "RTD Embedded Technologies VFG7330",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_ABSENT,
		.radio_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs		= {{
			.name   = "Composite 0",
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = "Composite 1",
			.vmux   = 1,
			.amux   = LINE2,
		},{
			.name   = "Composite 2",
			.vmux   = 2,
			.amux   = LINE1,
		},{
			.name   = "Composite 3",
			.vmux   = 3,
			.amux   = LINE2,
		},{
			.name   = "S-Video 0",
			.vmux   = 8,
			.amux   = LINE1,
		},{
			.name   = "S-Video 1",
			.vmux   = 9,
			.amux   = LINE2,
		}},
	},
	[SAA7134_BOARD_FLYTVPLATINUM_MINI2] = {
		.name           = "LifeView FlyTV Platinum Mini2",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,     
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,	
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_AVERMEDIA_AVERTVHD_A180] = {
		.name           = "AVerMedia AVerTVHD MCE A180",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_MONSTERTV_MOBILE] = {
		.name           = "SKNet MonsterTV Mobile",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.inputs         = {{
			  .name = name_tv,
			  .vmux = 1,
			  .amux = TV,
			  .tv   = 1,
		},{
			  .name = name_comp1,
			  .vmux = 3,
			  .amux = LINE1,
		},{
			  .name = name_svideo,
			  .vmux = 6,
			  .amux = LINE1,
		}},
	},
	[SAA7134_BOARD_PINNACLE_PCTV_110i] = {
	       .name           = "Pinnacle PCTV 40i/50i/110i (saa7133)",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.gpiomask       = 0x080200000,
		.inputs         = { {
			.name = name_tv,
			.vmux = 4,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE2,
		}, {
			.name = name_comp2,
			.vmux = 0,
			.amux = LINE2,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x0200000,
		},
	},
	[SAA7134_BOARD_ASUSTeK_P7131_DUAL] = {
		.name           = "ASUSTeK P7131 Dual",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask	= 1 << 21,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x0000000,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x0200000,
		},{
			.name = name_comp2,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x0200000,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x0200000,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x0200000,
		},
	},
	[SAA7134_BOARD_SEDNA_PC_TV_CARDBUS] = {
		
		
		.name           = "Sedna/MuchTV PC TV Cardbus TV/Radio (ITO25 Rev:2B)",
				
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.gpiomask       = 0xe880c0,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_ASUSTEK_DIGIMATRIX_TV] = {
		
		.name           = "ASUS Digimatrix TV",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_FQ1216ME,
		.tda9887_conf   = TDA9887_PRESENT,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
	},
	[SAA7134_BOARD_PHILIPS_TIGER] = {
		.name           = "Philips Tiger reference design",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tuner_config   = 0,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 0x0200000,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = TV,
			.gpio   = 0x0200000,
		},
	},
	[SAA7134_BOARD_MSI_TVATANYWHERE_PLUS] = {
		.name           = "MSI TV@Anywhere plus",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 1 << 21,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE2,	
		},{
			.name   = name_comp2,
			.vmux   = 0,		
			.amux   = LINE2,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE2,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = TV,
			.gpio   = 0x0200000,
		},
	},
	[SAA7134_BOARD_CINERGY250PCI] = {
		.name           = "Terratec Cinergy 250 PCI TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0x80200000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_svideo,  
			.vmux = 8,
			.amux = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = TV,
			.gpio   = 0x0200000,
		},
	},
	[SAA7134_BOARD_FLYDVB_TRIO] = {
		
		
		.name           = "LifeView FlyDVB Trio",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask	= 0x00200000,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_tv,	
			.vmux = 1,
			.amux = TV,
			.gpio = 0x200000,	
			.tv   = 1,
		},{
			.name = name_svideo,	
			.vmux = 8,
			.amux = LINE2,
		},{
			.name = name_comp1,	
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,	
			.vmux = 3,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x000000,	
		},
	},
	[SAA7134_BOARD_AVERMEDIA_777] = {
		.name           = "AverTV DVB-T 777",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_comp1,
			.vmux   = 1,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
	},
	[SAA7134_BOARD_FLYDVBT_LR301] = {
		
		
		.name           = "LifeView FlyDVB-T / Genius VideoWonder DVB-T",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_comp1,	
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,	
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_ADS_DUO_CARDBUS_PTV331] = {
		.name           = "ADS Instant TV Duo Cardbus PTV331",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 0x00600000, 
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
			.gpio   = 0x00200000,
		}},
	},
	[SAA7134_BOARD_TEVION_DVBT_220RF] = {
		.name           = "Tevion/KWorld DVB-T 220RF",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 1 << 21,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_comp2,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = TV,
			.gpio   = 0x0200000,
		},
	},
	[SAA7134_BOARD_KWORLD_DVBT_210] = {
		.name           = "KWorld DVB-T 210",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 1 << 21,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = TV,
			.gpio   = 0x0200000,
		},
	},
	[SAA7134_BOARD_KWORLD_ATSC110] = {
		.name           = "Kworld ATSC110/115",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TUV1236D,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_AVERMEDIA_A169_B] = {
		
		
		.name		= "AVerMedia A169 B",
		.audio_clock    = 0x02187de7,
		.tuner_type	= TUNER_LG_TALN,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x0a60000,
	},
	[SAA7134_BOARD_AVERMEDIA_A169_B1] = {
		
		
		.name		= "AVerMedia A169 B1",
		.audio_clock    = 0x02187de7,
		.tuner_type	= TUNER_LG_TALN,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0xca60000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 4,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x04a61000,
		},{
			.name = name_comp2,  
			.vmux = 1,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 9,           
			.amux = LINE1,
		}},
	},
	[SAA7134_BOARD_MD7134_BRIDGE_2] = {
		
		.name           = "Medion 7134 Bridge #2",
		.audio_clock    = 0x00187de7,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
	},
	[SAA7134_BOARD_FLYDVBT_HYBRID_CARDBUS] = {
		.name		= "LifeView FlyDVB-T Hybrid Cardbus/MSI TV @nywhere A/D NB",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 0x00600000, 
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x200000,	
			.tv   = 1,
		},{
			.name = name_svideo,	
			.vmux = 8,
			.amux = LINE2,
		},{
			.name = name_comp1,	
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,	
			.vmux = 3,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x000000,	
		},
	},
	[SAA7134_BOARD_FLYVIDEO3000_NTSC] = {
		
		.name           = "LifeView FlyVIDEO3000 (NTSC)",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,

		.gpiomask       = 0xe000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x8000,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x4000,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x4000,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x4000,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x2000,
		},
			.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x8000,
		},
	},
	[SAA7134_BOARD_MEDION_MD8800_QUADRO] = {
		.name           = "Medion Md8800 Quadro",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
	},
	[SAA7134_BOARD_FLYDVBS_LR300] = {
		
		
		.name           = "LifeView FlyDVB-S /Acorp TV134DS",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_comp1,	
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_svideo,	
			.vmux = 8,
			.amux = LINE1,
		}},
	},
	[SAA7134_BOARD_PROTEUS_2309] = {
		.name           = "Proteus Pro 2309",
		.audio_clock    = 0x00187de7,
		.tuner_type	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_A16AR] = {
		
		.name           = "AVerMedia TV Hybrid A16AR",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_PHILIPS_TD1316, 
		.radio_type     = TUNER_TEA5767, 
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = 0x60,
		.tda9887_conf   = TDA9887_PRESENT,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_ASUS_EUROPA2_HYBRID] = {
		.name           = "Asus Europa2 OEM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FMD1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT| TDA9887_PORT1_ACTIVE | TDA9887_PORT2_ACTIVE,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 3,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 4,
			.amux   = LINE2,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE2,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE1,
		},
	},
	[SAA7134_BOARD_PINNACLE_PCTV_310i] = {
		.name           = "Pinnacle PCTV 310i",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tuner_config   = 1,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 0x000200000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 4,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux   = TV,
			.gpio   = 0x0200000,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_STUDIO_507] = {
		
		.name           = "Avermedia AVerTV Studio 507",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1256_IH3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x03,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x00,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x00,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x00,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x00,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x01,
		},
		.mute  = {
			.name = name_mute,
			.amux = LINE1,
			.gpio = 0x00,
		},
	},
	[SAA7134_BOARD_VIDEOMATE_DVBT_200A] = {
		
		.name           = "Compro Videomate DVB-T200A",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT | TDA9887_PORT1_ACTIVE,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 3,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 1,
			.amux   = LINE2,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE2,
		}},
	},
	[SAA7134_BOARD_HAUPPAUGE_HVR1110] = {
		
		
		.name           = "Hauppauge WinTV-HVR1110 DVB-T/Hybrid",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tuner_config   = 1,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 0x0200100,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x0000100,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x0200100,
		},
	},
	[SAA7134_BOARD_HAUPPAUGE_HVR1150] = {
		.name           = "Hauppauge WinTV-HVR1150 ATSC/QAM-Hybrid",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tuner_config   = 3,
		.mpeg           = SAA7134_MPEG_DVB,
		.ts_type	= SAA7134_MPEG_TS_SERIAL,
		.ts_force_val   = 1,
		.gpiomask       = 0x0800100, 
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x0000100,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x0800100, 
		},
	},
	[SAA7134_BOARD_HAUPPAUGE_HVR1120] = {
		.name           = "Hauppauge WinTV-HVR1120 DVB-T/Hybrid",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tuner_config   = 3,
		.mpeg           = SAA7134_MPEG_DVB,
		.ts_type	= SAA7134_MPEG_TS_SERIAL,
		.gpiomask       = 0x0800100, 
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x0000100,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x0800100, 
		},
	},
	[SAA7134_BOARD_CINERGY_HT_PCMCIA] = {
		.name           = "Terratec Cinergy HT PCMCIA",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 6,
			.amux   = LINE1,
		}},
	},
	[SAA7134_BOARD_ENCORE_ENLTV] = {
		.name           = "Encore ENLTV",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_TNF_5335MF,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = 3,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 7,
			.amux = 4,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = 2,
		},{
			.name = name_svideo,
			.vmux = 0,
			.amux = 2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x20000,

		},
		.mute = {
			.name = name_mute,
			.amux = 0,
		},
	},
	[SAA7134_BOARD_ENCORE_ENLTV_FM] = {
  
		.name           = "Encore ENLTV-FM",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_FCV1236D,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = 3,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 7,
			.amux = 4,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = 2,
		},{
			.name = name_svideo,
			.vmux = 0,
			.amux = 2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x20000,

		},
		.mute = {
			.name = name_mute,
			.amux = 0,
		},
	},
	[SAA7134_BOARD_ENCORE_ENLTV_FM53] = {
		.name           = "Encore ENLTV-FM v5.3",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_TNF_5335MF,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask	= 0x7000,
		.inputs         = { {
			.name = name_tv,
			.vmux = 1,
			.amux = 1,
			.tv   = 1,
			.gpio = 0x50000,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = 2,
			.gpio = 0x2000,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = 2,
			.gpio = 0x2000,
		} },
		.radio = {
			.name = name_radio,
			.vmux = 1,
			.amux = 1,
		},
		.mute = {
			.name = name_mute,
			.gpio = 0xf000,
			.amux = 0,
		},
	},
	[SAA7134_BOARD_ENCORE_ENLTV_FM3] = {
		.name           = "Encore ENLTV-FM 3",
		.audio_clock    = 0x02187de7,
		.tuner_type     = TUNER_TENA_TNF_5337,
		.radio_type     = TUNER_TEA5767,
		.tuner_addr	= 0x61,
		.radio_addr	= 0x60,
		.inputs         = { {
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.vmux = 1,
			.amux = LINE1,
		},
		.mute = {
			.name = name_mute,
			.amux = LINE1,
			.gpio = 0x43000,
		},
	},
	[SAA7134_BOARD_CINERGY_HT_PCI] = {
		.name           = "Terratec Cinergy HT PCI",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 6,
			.amux   = LINE1,
		}},
	},
	[SAA7134_BOARD_PHILIPS_TIGER_S] = {
		.name           = "Philips Tiger - S Reference design",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tuner_config   = 2,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 0x0200000,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = TV,
			.gpio   = 0x0200000,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_M102] = {
		.name           = "Avermedia M102",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 1<<21,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_ASUS_P7131_4871] = {
		.name           = "ASUS P7131 4871",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tuner_config   = 2,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 0x0200000,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
			.gpio   = 0x0200000,
		}},
	},
	[SAA7134_BOARD_ASUSTeK_P7131_HYBRID_LNA] = {
		.name           = "ASUSTeK P7131 Hybrid",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tuner_config   = 2,
		.gpiomask	= 1 << 21,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x0000000,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x0200000,
		},{
			.name = name_comp2,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x0200000,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x0200000,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x0200000,
		},
	},
	[SAA7134_BOARD_ASUSTeK_P7131_ANALOG] = {
	       .name           = "ASUSTeK P7131 Analog",
	       .audio_clock    = 0x00187de7,
	       .tuner_type     = TUNER_PHILIPS_TDA8290,
	       .radio_type     = UNSET,
	       .tuner_addr     = ADDR_UNSET,
	       .radio_addr     = ADDR_UNSET,
	       .gpiomask       = 1 << 21,
	       .inputs         = {{
		       .name = name_tv,
		       .vmux = 1,
		       .amux = TV,
		       .tv   = 1,
		       .gpio = 0x0000000,
	       }, {
		       .name = name_comp1,
		       .vmux = 3,
		       .amux = LINE2,
	       }, {
		       .name = name_comp2,
		       .vmux = 0,
		       .amux = LINE2,
	       }, {
		       .name = name_svideo,
		       .vmux = 8,
		       .amux = LINE2,
	       } },
	       .radio = {
		       .name = name_radio,
		       .amux = TV,
		       .gpio = 0x0200000,
	       },
	},
	[SAA7134_BOARD_SABRENT_TV_PCB05] = {
		.name           = "Sabrent PCMCIA TV-PCB05",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_comp2,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.mute = {
			.name = name_mute,
			.amux = TV,
		},
	},
	[SAA7134_BOARD_10MOONSTVMASTER3] = {
		
		.name           = "10MOONS TM300 TV Card",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_LG_PAL_NEW_TAPC,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.gpiomask       = 0x7000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x2000,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x2000,
		}},
		.mute = {
			.name = name_mute,
			.amux = LINE2,
			.gpio = 0x3000,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_SUPER_007] = {
		.name           = "Avermedia Super 007",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tuner_config   = 0,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv, 
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		}},
	},
	[SAA7134_BOARD_AVERMEDIA_M135A] = {
		.name           = "Avermedia PCI pure analog (M135A)",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tuner_config   = 2,
		.gpiomask       = 0x020200000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x00200000,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x01,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_M733A] = {
		.name		= "Avermedia PCI M733A",
		.audio_clock	= 0x00187de7,
		.tuner_type	= TUNER_PHILIPS_TDA8290,
		.radio_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tuner_config	= 0,
		.gpiomask	= 0x020200000,
		.inputs		= {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x00200000,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x01,
		},
	},
	[SAA7134_BOARD_BEHOLD_401] = {
		
		
		.name           = "Beholder BeholdTV 401",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FQ1216ME,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}},
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_BEHOLD_403] = {
		
		
		.name           = "Beholder BeholdTV 403",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FQ1216ME,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name   = name_comp1,
			.vmux   = 1,
			.amux   = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_BEHOLD_403FM] = {
		
		
		.name           = "Beholder BeholdTV 403 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FQ1216ME,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name   = name_comp1,
			.vmux   = 1,
			.amux   = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_405] = {
		
		
		.name           = "Beholder BeholdTV 405",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_BEHOLD_405FM] = {
		
		
		
		.name           = "Beholder BeholdTV 405 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_407] = {
		
		
		.name 		= "Beholder BeholdTV 407",
		.audio_clock 	= 0x00187de7,
		.tuner_type 	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type 	= UNSET,
		.tuner_addr 	= ADDR_UNSET,
		.radio_addr 	= ADDR_UNSET,
		.tda9887_conf 	= TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0xc0c000,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
			.gpio = 0xc0c000,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv = 1,
			.gpio = 0xc0c000,
		}},
	},
	[SAA7134_BOARD_BEHOLD_407FM] = {
		
		
		.name 		= "Beholder BeholdTV 407 FM",
		.audio_clock 	= 0x00187de7,
		.tuner_type 	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type 	= UNSET,
		.tuner_addr 	= ADDR_UNSET,
		.radio_addr 	= ADDR_UNSET,
		.tda9887_conf 	= TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0xc0c000,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
			.gpio = 0xc0c000,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv = 1,
			.gpio = 0xc0c000,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0xc0c000,
		},
	},
	[SAA7134_BOARD_BEHOLD_409] = {
		
		
		.name           = "Beholder BeholdTV 409",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
	},
	[SAA7134_BOARD_BEHOLD_505FM] = {
		
		
		.name           = "Beholder BeholdTV 505 FM",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_505RDS_MK5] = {
		
		
		.name           = "Beholder BeholdTV 505 RDS",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_FM1216MK5,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.rds_addr 	= 0x10,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_507_9FM] = {
		
		
		.name           = "Beholder BeholdTV 507 FM / BeholdTV 509 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
			.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_507RDS_MK5] = {
		
		
		.name           = "Beholder BeholdTV 507 RDS",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216MK5,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.rds_addr 	= 0x10,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
			.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_507RDS_MK3] = {
		
		
		.name           = "Beholder BeholdTV 507 RDS",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.rds_addr 	= 0x10,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
			.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_COLUMBUS_TVFM] = {
		
		
		.name           = "Beholder BeholdTV Columbus TV/FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_ALPS_TSBE5_PAL,
		.radio_type     = TUNER_TEA5767,
		.tuner_addr     = 0xc2 >> 1,
		.radio_addr     = 0xc0 >> 1,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x000A8004,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x000A8004,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
			.gpio = 0x000A8000,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x000A8000,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x000A8000,
		},
	},
	[SAA7134_BOARD_BEHOLD_607FM_MK3] = {
		
		.name           = "Beholder BeholdTV 607 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_609FM_MK3] = {
		
		.name           = "Beholder BeholdTV 609 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_607FM_MK5] = {
		
		.name           = "Beholder BeholdTV 607 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216MK5,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_609FM_MK5] = {
		
		.name           = "Beholder BeholdTV 609 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216MK5,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_607RDS_MK3] = {
		
		.name           = "Beholder BeholdTV 607 RDS",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.rds_addr 	= 0x10,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_609RDS_MK3] = {
		
		.name           = "Beholder BeholdTV 609 RDS",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.rds_addr 	= 0x10,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_607RDS_MK5] = {
		
		.name           = "Beholder BeholdTV 607 RDS",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216MK5,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.rds_addr 	= 0x10,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_609RDS_MK5] = {
		
		.name           = "Beholder BeholdTV 609 RDS",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216MK5,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.rds_addr 	= 0x10,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_M6] = {
		
		
		
		
		.name           = "Beholder BeholdTV M6",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.empress_addr 	= 0x20,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = { {
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
		.mpeg  = SAA7134_MPEG_EMPRESS,
		.video_out = CCIR656,
		.vid_port_opts  = (SET_T_CODE_POLARITY_NON_INVERTED |
					SET_CLOCK_NOT_DELAYED |
					SET_CLOCK_INVERTED |
					SET_VSYNC_OFF),
	},
	[SAA7134_BOARD_BEHOLD_M63] = {
		
		
		
		.name           = "Beholder BeholdTV M63",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.empress_addr 	= 0x20,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = { {
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
		.mpeg  = SAA7134_MPEG_EMPRESS,
		.video_out = CCIR656,
		.vid_port_opts  = (SET_T_CODE_POLARITY_NON_INVERTED |
					SET_CLOCK_NOT_DELAYED |
					SET_CLOCK_INVERTED |
					SET_VSYNC_OFF),
	},
	[SAA7134_BOARD_BEHOLD_M6_EXTRA] = {
		
		
		
		
		.name           = "Beholder BeholdTV M6 Extra",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216MK5,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.rds_addr 	= 0x10,
		.empress_addr 	= 0x20,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = { {
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
		.mpeg  = SAA7134_MPEG_EMPRESS,
		.video_out = CCIR656,
		.vid_port_opts  = (SET_T_CODE_POLARITY_NON_INVERTED |
					SET_CLOCK_NOT_DELAYED |
					SET_CLOCK_INVERTED |
					SET_VSYNC_OFF),
	},
	[SAA7134_BOARD_TWINHAN_DTV_DVB_3056] = {
		.name           = "Twinhan Hybrid DTV-DVB 3056 PCI",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tuner_config   = 2,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 0x0200000,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		}, {
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		}, {
			.name   = name_svideo,
			.vmux   = 8,		
			.amux   = LINE1,
		} },
		.radio = {
			.name   = name_radio,
			.amux   = TV,
			.gpio   = 0x0200000,
		},
	},
	[SAA7134_BOARD_GENIUS_TVGO_A11MCE] = {
		
		.name		= "Genius TVGO AM11MCE",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_TNF_5335MF,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0xf000,
		.inputs         = {{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x2000,
			.tv = 1
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x2000,
	} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x1000,
		},
		.mute = {
			.name = name_mute,
			.amux = LINE2,
			.gpio = 0x6000,
		},
	},
	[SAA7134_BOARD_PHILIPS_SNAKE] = {
		.name           = "NXP Snake DVB-S reference design",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		}, {
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		} },
	},
	[SAA7134_BOARD_CREATIX_CTX953] = {
		.name         = "Medion/Creatix CTX953 Hybrid",
		.audio_clock  = 0x00187de7,
		.tuner_type   = TUNER_PHILIPS_TDA8290,
		.radio_type   = UNSET,
		.tuner_addr   = ADDR_UNSET,
		.radio_addr   = ADDR_UNSET,
		.tuner_config = 0,
		.mpeg         = SAA7134_MPEG_DVB,
		.inputs       = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
	},
	[SAA7134_BOARD_MSI_TVANYWHERE_AD11] = {
		.name           = "MSI TV@nywhere A/D v1.1",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tuner_config   = 2,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 0x0200000,
		.inputs = { {
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		}, {
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		}, {
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		} },
		.radio = {
			.name   = name_radio,
			.amux   = TV,
			.gpio   = 0x0200000,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_CARDBUS_506] = {
		.name           = "AVerMedia Cardbus TV/Radio (E506R)",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_XC2028,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		 .mpeg           = SAA7134_MPEG_DVB,
		 .inputs         = {{
			 .name = name_tv,
			 .vmux = 1,
			 .amux = TV,
			 .tv   = 1,
		 }, {
			 .name = name_comp1,
			 .vmux = 3,
			 .amux = LINE1,
		 }, {
			 .name = name_svideo,
			 .vmux = 8,
			 .amux = LINE2,
		 } },
		 .radio = {
			 .name = name_radio,
			 .amux = TV,
		 },
	},
	[SAA7134_BOARD_AVERMEDIA_A16D] = {
		.name           = "AVerMedia Hybrid TV/Radio (A16D)",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_XC2028,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}, {
			.name = name_comp,
			.vmux = 0,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_M115] = {
		.name           = "Avermedia M115",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_XC2028,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		} },
	},
	[SAA7134_BOARD_VIDEOMATE_T750] = {
		
		.name           = "Compro VideoMate T750",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_XC2028,
		.radio_type     = UNSET,
		.tuner_addr	= 0x61,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 3,
			.amux   = TV,
			.tv     = 1,
		}, {
			.name   = name_comp1,
			.vmux   = 1,
			.amux   = LINE2,
		}, {
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE2,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
		}
	},
	[SAA7134_BOARD_AVERMEDIA_A700_PRO] = {
		
		.name           = "Avermedia DVB-S Pro A700",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = { {
			.name = name_comp,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE1,
		} },
	},
	[SAA7134_BOARD_AVERMEDIA_A700_HYBRID] = {
		
		.name           = "Avermedia DVB-S Hybrid+FM A700",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_XC2028,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = { {
			.name   = name_tv,
			.vmux   = 4,
			.amux   = TV,
			.tv     = 1,
		}, {
			.name = name_comp,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
		},
	},
	[SAA7134_BOARD_BEHOLD_H6] = {
		
		.name           = "Beholder BeholdTV H6",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FMD1216MEX_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_ASUSTeK_TIGER_3IN1] = {
		.name           = "Asus Tiger 3in1",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tuner_config   = 2,
		.gpiomask       = 1 << 21,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp,
			.vmux = 0,
			.amux = LINE2,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x0200000,
		},
	},
	[SAA7134_BOARD_REAL_ANGEL_220] = {
		.name           = "Zogis Real Angel 220",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_TNF_5335MF,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.gpiomask       = 0x801a8087,
		.inputs = { {
			.name   = name_tv,
			.vmux   = 3,
			.amux   = LINE2,
			.tv     = 1,
			.gpio   = 0x624000,
		}, {
			.name   = name_comp1,
			.vmux   = 1,
			.amux   = LINE1,
			.gpio   = 0x624000,
		}, {
			.name   = name_svideo,
			.vmux   = 1,
			.amux   = LINE1,
			.gpio   = 0x624000,
		} },
		.radio = {
			.name   = name_radio,
			.amux   = LINE2,
			.gpio   = 0x624001,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
		},
	},
	[SAA7134_BOARD_ADS_INSTANT_HDTV_PCI] = {
		.name           = "ADS Tech Instant HDTV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TUV1236D,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = { {
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp,
			.vmux = 4,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
	},
	[SAA7134_BOARD_ASUSTeK_TIGER] = {
		.name           = "Asus Tiger Rev:1.00",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tuner_config   = 0,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 0x0200000,
		.inputs = { {
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		}, {
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE2,
		}, {
			.name   = name_comp2,
			.vmux   = 0,
			.amux   = LINE2,
		}, {
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE2,
		} },
		.radio = {
			.name   = name_radio,
			.amux   = TV,
			.gpio   = 0x0200000,
		},
	},
	[SAA7134_BOARD_KWORLD_PLUS_TV_ANALOG] = {
		.name           = "Kworld Plus TV Analog Lite PCI",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_YMEC_TVF_5533MF,
		.radio_type     = TUNER_TEA5767,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = 0x60,
		.gpiomask       = 0x80000700,
		.inputs = { {
			.name   = name_tv,
			.vmux   = 1,
			.amux   = LINE2,
			.tv     = 1,
			.gpio   = 0x100,
		}, {
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
			.gpio   = 0x200,
		}, {
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
			.gpio   = 0x200,
		} },
		.radio = {
			.name   = name_radio,
			.vmux   = 1,
			.amux   = LINE1,
			.gpio   = 0x100,
		},
		.mute = {
			.name = name_mute,
			.vmux = 8,
			.amux = 2,
		},
	},
	[SAA7134_BOARD_KWORLD_PCI_SBTVD_FULLSEG] = {
		.name           = "Kworld PCI SBTVD/ISDB-T Full-Seg Hybrid",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.tuner_addr     = ADDR_UNSET,
		.radio_type     = UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0x8e054000,
		.mpeg           = SAA7134_MPEG_DVB,
		.ts_type	= SAA7134_MPEG_TS_PARALLEL,
		.inputs = { {
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
#if 0	
		}, {
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
			.gpio   = 0x200,
		}, {
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
			.gpio   = 0x200,
#endif
		} },
#if 0
		.radio = {
			.name   = name_radio,
			.vmux   = 1,
			.amux   = LINE1,
			.gpio   = 0x100,
		},
#endif
		.mute = {
			.name = name_mute,
			.vmux = 0,
			.amux = TV,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_GO_007_FM_PLUS] = {
		.name           = "Avermedia AVerTV GO 007 FM Plus",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0x00300003,
		
		.inputs         = { {
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x01,
		}, {
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE1,
			.gpio = 0x02,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x00300001,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x01,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_STUDIO_507UA] = {
		
		.name           = "Avermedia AVerTV Studio 507UA",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3, 
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x03,
		.inputs         = { {
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x00,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x00,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x00,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x01,
		},
		.mute  = {
			.name = name_mute,
			.amux = LINE1,
			.gpio = 0x00,
		},
	},
	[SAA7134_BOARD_VIDEOMATE_S350] = {
		
		.name		= "Compro VideoMate S350/S300",
		.audio_clock	= 0x00187de7,
		.tuner_type	= TUNER_ABSENT,
		.radio_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg		= SAA7134_MPEG_DVB,
		.inputs = { {
			.name	= name_comp1,
			.vmux	= 0,
			.amux	= LINE1,
		}, {
			.name	= name_svideo,
			.vmux	= 8, 
			.amux	= LINE1
		} },
	},
	[SAA7134_BOARD_BEHOLD_X7] = {
		
		.name           = "Beholder BeholdTV X7",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_XC5000,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = { {
			.name = name_tv,
			.vmux = 2,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 9,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
		},
	},
	[SAA7134_BOARD_ZOLID_HYBRID_PCI] = {
		.name           = "Zolid Hybrid TV Tuner PCI",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tuner_config   = 0,
		.mpeg           = SAA7134_MPEG_DVB,
		.ts_type	= SAA7134_MPEG_TS_PARALLEL,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		} },
		.radio = {	
			.name = name_radio,
			.amux = TV,
		},
	},
	[SAA7134_BOARD_ASUS_EUROPA_HYBRID] = {
		.name           = "Asus Europa Hybrid OEM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TD1316,
		.radio_type     = UNSET,
		.tuner_addr	= 0x61,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT | TDA9887_PORT1_ACTIVE,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = { {
			.name   = name_tv,
			.vmux   = 3,
			.amux   = TV,
			.tv     = 1,
		}, {
			.name   = name_comp1,
			.vmux   = 4,
			.amux   = LINE2,
		}, {
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE2,
		} },
	},
	[SAA7134_BOARD_LEADTEK_WINFAST_DTV1000S] = {
		.name           = "Leadtek Winfast DTV1000S",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = { {
			.name = name_comp1,
			.vmux = 3,
		}, {
			.name = name_svideo,
			.vmux = 8,
		} },
	},
	[SAA7134_BOARD_BEHOLD_505RDS_MK3] = {
		
		
		.name           = "Beholder BeholdTV 505 RDS",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.rds_addr 	= 0x10,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_HAWELL_HW_404M7] = {
		
		
		.name         = "Hawell HW-404M7",
		.audio_clock   = 0x00200000,
		.tuner_type    = UNSET,
		.radio_type    = UNSET,
		.tuner_addr   = ADDR_UNSET,
		.radio_addr   = ADDR_UNSET,
		.gpiomask      = 0x389c00,
		.inputs       = {{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x01fc00,
		} },
	},
	[SAA7134_BOARD_BEHOLD_H7] = {
		
		.name           = "Beholder BeholdTV H7",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_XC5000,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.ts_type	= SAA7134_MPEG_TS_PARALLEL,
		.inputs         = { {
			.name = name_tv,
			.vmux = 2,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 9,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
		},
	},
	[SAA7134_BOARD_BEHOLD_A7] = {
		
		.name           = "Beholder BeholdTV A7",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_XC5000,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.inputs         = { {
			.name = name_tv,
			.vmux = 2,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 9,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
		},
	},
	[SAA7134_BOARD_TECHNOTREND_BUDGET_T3000] = {
		.name           = "TechoTrend TT-budget T-3000",
		.tuner_type     = TUNER_PHILIPS_TD1316,
		.audio_clock    = 0x00187de7,
		.radio_type     = UNSET,
		.tuner_addr     = 0x63,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT | TDA9887_PORT1_ACTIVE,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 3,
			.amux   = TV,
			.tv     = 1,
		}, {
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE2,
		}, {
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE2,
		} },
	},
	[SAA7134_BOARD_VIDEOMATE_M1F] = {
		
		.name           = "Compro VideoMate Vista M1F",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_LG_PAL_NEW_TAPC,
		.radio_type     = TUNER_TEA5767,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = 0x60,
		.inputs         = { {
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE2,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE1,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
		},
	},
	[SAA7134_BOARD_MAGICPRO_PROHDTV_PRO2] = {
		
		.name		= "MagicPro ProHDTV Pro2 DMB-TH/Hybrid",
		.audio_clock	= 0x00187de7,
		.tuner_type	= TUNER_PHILIPS_TDA8290,
		.radio_type	= UNSET,
		.tuner_config	= 3,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask	= 0x02050000,
		.mpeg		= SAA7134_MPEG_DVB,
		.ts_type	= SAA7134_MPEG_TS_PARALLEL,
		.inputs		= { {
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
			.gpio   = 0x00050000,
		}, {
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
			.gpio   = 0x00050000,
		}, {
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
			.gpio   = 0x00050000,
		} },
		.radio = {
			.name   = name_radio,
			.amux   = TV,
			.gpio   = 0x00050000,
		},
		.mute = {
			.name   = name_mute,
			.vmux   = 0,
			.amux   = TV,
			.gpio   = 0x00050000,
		},
	},
	[SAA7134_BOARD_BEHOLD_501] = {
		
		
		.name           = "Beholder BeholdTV 501",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.gpiomask       = 0x00008000,
		.inputs         = { {
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_BEHOLD_503FM] = {
		
		
		.name           = "Beholder BeholdTV 503 FM",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.gpiomask       = 0x00008000,
		.inputs         = { {
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_SENSORAY811_911] = {
		.name		= "Sensoray 811/911",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_ABSENT,
		.radio_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs		= {{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE1,
		}, {
			.name   = name_comp3,
			.vmux   = 2,
			.amux   = LINE1,
		}, {
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		} },
	},
	[SAA7134_BOARD_KWORLD_PC150U] = {
		.name           = "Kworld PC150-U",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 1 << 21,
		.ts_type	= SAA7134_MPEG_TS_PARALLEL,
		.inputs = { {
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		}, {
			.name   = name_comp,
			.vmux   = 3,
			.amux   = LINE1,
		}, {
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE2,
		} },
		.radio = {
			.name   = name_radio,
			.amux   = TV,
			.gpio	= 0x0000000,
		},
	},

};

const unsigned int saa7134_bcount = ARRAY_SIZE(saa7134_boards);


struct pci_device_id saa7134_pci_tbl[] = {
	{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = PCI_VENDOR_ID_PHILIPS,
		.subdevice    = 0x2001,
		.driver_data  = SAA7134_BOARD_PROTEUS_PRO,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = PCI_VENDOR_ID_PHILIPS,
		.subdevice    = 0x2001,
		.driver_data  = SAA7134_BOARD_PROTEUS_PRO,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = PCI_VENDOR_ID_PHILIPS,
		.subdevice    = 0x6752,
		.driver_data  = SAA7134_BOARD_EMPRESS,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1131,
		.subdevice    = 0x4e85,
		.driver_data  = SAA7134_BOARD_MONSTERTV,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x153b,
		.subdevice    = 0x1142,
		.driver_data  = SAA7134_BOARD_CINERGY400,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x153b,
		.subdevice    = 0x1143,
		.driver_data  = SAA7134_BOARD_CINERGY600,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x153b,
		.subdevice    = 0x1158,
		.driver_data  = SAA7134_BOARD_CINERGY600_MK3,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x153b,
		.subdevice    = 0x1162,
		.driver_data  = SAA7134_BOARD_CINERGY400_CARDBUS,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x5169,
		.subdevice    = 0x0138,
		.driver_data  = SAA7134_BOARD_FLYVIDEO3000_NTSC,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x5168,
		.subdevice    = 0x0138,
		.driver_data  = SAA7134_BOARD_FLYVIDEO3000,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x4e42,				
		.subdevice    = 0x0138,
		.driver_data  = SAA7134_BOARD_FLYVIDEO3000,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x5168,
		.subdevice    = 0x0138,
		.driver_data  = SAA7134_BOARD_FLYVIDEO2000,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x4e42,		
		.subdevice    = 0x0138,		
		.driver_data  = SAA7134_BOARD_FLYVIDEO2000,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5168,
		.subdevice    = 0x0212, 
		.driver_data  = SAA7134_BOARD_FLYTVPLATINUM_MINI,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x14c0,
		.subdevice    = 0x1212, 
		.driver_data  = SAA7134_BOARD_FLYTVPLATINUM_MINI2,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x4e42,
		.subdevice    = 0x0212, 
		.driver_data  = SAA7134_BOARD_FLYTVPLATINUM_MINI,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5168,	
		.subdevice    = 0x0214, 
		.driver_data  = SAA7134_BOARD_FLYTVPLATINUM_FM,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5168,	
		.subdevice    = 0x5214, 
		.driver_data  = SAA7134_BOARD_FLYTVPLATINUM_FM,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1489, 
		.subdevice    = 0x0214, 
		.driver_data  = SAA7134_BOARD_FLYTVPLATINUM_FM, 
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x16be,
		.subdevice    = 0x0003,
		.driver_data  = SAA7134_BOARD_MD7134,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x16be, 
		.subdevice    = 0x5000, 
		.driver_data  = SAA7134_BOARD_MD7134,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x1048,
		.subdevice    = 0x226b,
		.driver_data  = SAA7134_BOARD_ELSA,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x1048,
		.subdevice    = 0x226a,
		.driver_data  = SAA7134_BOARD_ELSA_500TV,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x1048,
		.subdevice    = 0x226c,
		.driver_data  = SAA7134_BOARD_ELSA_700TV,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = PCI_VENDOR_ID_ASUSTEK,
		.subdevice    = 0x4842,
		.driver_data  = SAA7134_BOARD_ASUSTeK_TVFM7134,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = PCI_VENDOR_ID_ASUSTEK,
		.subdevice    = 0x4845,
		.driver_data  = SAA7134_BOARD_ASUSTeK_TVFM7135,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = PCI_VENDOR_ID_ASUSTEK,
		.subdevice    = 0x4830,
		.driver_data  = SAA7134_BOARD_ASUSTeK_TVFM7134,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = PCI_VENDOR_ID_ASUSTEK,
		.subdevice    = 0x4843,
		.driver_data  = SAA7134_BOARD_ASUSTEK_TVFM7133,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = PCI_VENDOR_ID_ASUSTEK,
		.subdevice    = 0x4840,
		.driver_data  = SAA7134_BOARD_ASUSTeK_TVFM7134,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = PCI_VENDOR_ID_PHILIPS,
		.subdevice    = 0xfe01,
		.driver_data  = SAA7134_BOARD_TVSTATION_RDS,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1894,
		.subdevice    = 0xfe01,
		.driver_data  = SAA7134_BOARD_TVSTATION_RDS,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1894,
		.subdevice    = 0xa006,
		.driver_data  = SAA7134_BOARD_TVSTATION_DVR,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1131,
		.subdevice    = 0x7133,
		.driver_data  = SAA7134_BOARD_VA1000POWER,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = PCI_VENDOR_ID_PHILIPS,
		.subdevice    = 0x2001,
		.driver_data  = SAA7134_BOARD_10MOONSTVMASTER,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x185b,
		.subdevice    = 0xc100,
		.driver_data  = SAA7134_BOARD_VIDEOMATE_TV,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x185b,
		.subdevice    = 0xc100,
		.driver_data  = SAA7134_BOARD_VIDEOMATE_TV_GOLD_PLUS,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = PCI_VENDOR_ID_MATROX,
		.subdevice    = 0x48d0,
		.driver_data  = SAA7134_BOARD_CRONOS_PLUS,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1461, 
		.subdevice    = 0xa70b,
		.driver_data  = SAA7134_BOARD_MD2819,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1461, 
		.subdevice    = 0xa7a1,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_A700_PRO,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1461, 
		.subdevice    = 0xa7a2,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_A700_HYBRID,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x1461, 
		.subdevice    = 0x2115,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_STUDIO_305,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x1461, 
		.subdevice    = 0xa115,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_STUDIO_505,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x1461, 
		.subdevice    = 0x2108,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_305,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x1461, 
		.subdevice    = 0x10ff,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_DVD_EZMAKER,
	},{
		
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1461, 
		.subdevice    = 0xd6ee,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_CARDBUS,
	},{
		
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1461, 
		.subdevice    = 0xb7e9,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_CARDBUS_501,
	}, {
		
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x1461, 
		.subdevice    = 0x050c,
		.driver_data  = SAA7134_BOARD_TG3000TV,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x11bd,
		.subdevice    = 0x002b,
		.driver_data  = SAA7134_BOARD_PINNACLE_PCTV_STEREO,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x11bd,
		.subdevice    = 0x002d,
		.driver_data  = SAA7134_BOARD_PINNACLE_300I_DVBT_PAL,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1019,
		.subdevice    = 0x4cb4,
		.driver_data  = SAA7134_BOARD_ECS_TVP3XP,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1019,
		.subdevice    = 0x4cb5,
		.driver_data  = SAA7134_BOARD_ECS_TVP3XP_4CB5,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1019,
		.subdevice    = 0x4cb6,
		.driver_data  = SAA7134_BOARD_ECS_TVP3XP_4CB6,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x12ab,
		.subdevice    = 0x0800,
		.driver_data  = SAA7134_BOARD_UPMOST_PURPLE_TV,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x153b,
		.subdevice    = 0x1152,
		.driver_data  = SAA7134_BOARD_CINERGY200,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x185b,
		.subdevice    = 0xc100,
		.driver_data  = SAA7134_BOARD_VIDEOMATE_TV_PVR,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1461, 
		.subdevice    = 0x9715,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_STUDIO_307,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1461, 
		.subdevice    = 0xa70a,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_307,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x185b,
		.subdevice    = 0xc200,
		.driver_data  = SAA7134_BOARD_VIDEOMATE_GOLD_PLUS,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1540,
		.subdevice    = 0x9524,
		.driver_data  = SAA7134_BOARD_PROVIDEO_PV952,

	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5168,
		.subdevice    = 0x0502,                
		.driver_data  = SAA7134_BOARD_FLYDVBT_DUO_CARDBUS,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5168,
		.subdevice    = 0x0306,                
		.driver_data  = SAA7134_BOARD_FLYDVBTDUO,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1461, 
		.subdevice    = 0xf31f,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_GO_007_FM,

	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1461, 
		.subdevice    = 0xf11d,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_M135A,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1461, 
		.subdevice    = 0x4155,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_M733A,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1461, 
		.subdevice    = 0x4255,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_M733A,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = PCI_VENDOR_ID_PHILIPS,
		.subdevice    = 0x2004,
		.driver_data  = SAA7134_BOARD_PHILIPS_TOUGH,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1421,
		.subdevice    = 0x0350,		
		.driver_data  = SAA7134_BOARD_ADS_INSTANT_TV,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1421,
		.subdevice    = 0x0351,		
		.driver_data  = SAA7134_BOARD_ADS_INSTANT_TV,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1421,
		.subdevice    = 0x0370,		
		.driver_data  = SAA7134_BOARD_ADS_INSTANT_TV,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1421,
		.subdevice    = 0x1370,        
		.driver_data  = SAA7134_BOARD_ADS_INSTANT_TV,

	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x4e42,		
		.subdevice    = 0x0502,		
		.driver_data  = SAA7134_BOARD_FLYDVBT_DUO_CARDBUS,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1043,
		.subdevice    = 0x0210,		
		.driver_data  = SAA7134_BOARD_FLYTV_DIGIMATRIX,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1043,
		.subdevice    = 0x0210,		
		.driver_data  = SAA7134_BOARD_ASUSTEK_DIGIMATRIX_TV,

	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x0000, 
		.subdevice    = 0x4091,
		.driver_data  = SAA7134_BOARD_BEHOLD_409FM,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5456, 
		.subdevice    = 0x7135,
		.driver_data  = SAA7134_BOARD_GOTVIEW_7135,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = PCI_VENDOR_ID_PHILIPS,
		.subdevice    = 0x2004,
		.driver_data  = SAA7134_BOARD_PHILIPS_EUROPA,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x185b,
		.subdevice    = 0xc900,
		.driver_data  = SAA7134_BOARD_VIDEOMATE_DVBT_300,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x185b,
		.subdevice    = 0xc901,
		.driver_data  = SAA7134_BOARD_VIDEOMATE_DVBT_200,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1435,
		.subdevice    = 0x7350,
		.driver_data  = SAA7134_BOARD_RTD_VFG7350,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1435,
		.subdevice    = 0x7330,
		.driver_data  = SAA7134_BOARD_RTD_VFG7330,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1461,
		.subdevice    = 0x1044,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_AVERTVHD_A180,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1131,
		.subdevice    = 0x4ee9,
		.driver_data  = SAA7134_BOARD_MONSTERTV_MOBILE,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x11bd,
		.subdevice    = 0x002e,
		.driver_data  = SAA7134_BOARD_PINNACLE_PCTV_110i,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1043,
		.subdevice    = 0x4862,
		.driver_data  = SAA7134_BOARD_ASUSTeK_P7131_DUAL,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = PCI_VENDOR_ID_PHILIPS,
		.subdevice    = 0x2018,
		.driver_data  = SAA7134_BOARD_PHILIPS_TIGER,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1462,
		.subdevice    = 0x6231, 
		.driver_data  = SAA7134_BOARD_MSI_TVATANYWHERE_PLUS,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1462,
		.subdevice    = 0x8624, 
		.driver_data  = SAA7134_BOARD_MSI_TVATANYWHERE_PLUS,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x153b,
		.subdevice    = 0x1160,
		.driver_data  = SAA7134_BOARD_CINERGY250PCI,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,	
		.subvendor    = 0x5168,
		.subdevice    = 0x0319,
		.driver_data  = SAA7134_BOARD_FLYDVB_TRIO,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1461,
		.subdevice    = 0x2c05,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_777,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x5168,
		.subdevice    = 0x0301,
		.driver_data  = SAA7134_BOARD_FLYDVBT_LR301,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x0331,
		.subdevice    = 0x1421,
		.driver_data  = SAA7134_BOARD_ADS_DUO_CARDBUS_PTV331,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x17de,
		.subdevice    = 0x7201,
		.driver_data  = SAA7134_BOARD_TEVION_DVBT_220RF,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x17de,
		.subdevice    = 0x7250,
		.driver_data  = SAA7134_BOARD_KWORLD_DVBT_210,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133, 
		.subvendor    = 0x17de,
		.subdevice    = 0x7350,
		.driver_data  = SAA7134_BOARD_KWORLD_ATSC110,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133, 
		.subvendor    = 0x17de,
		.subdevice    = 0x7352,
		.driver_data  = SAA7134_BOARD_KWORLD_ATSC110, 
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133, 
		.subvendor    = 0x17de,
		.subdevice    = 0xa134,
		.driver_data  = SAA7134_BOARD_KWORLD_PC150U,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1461,
		.subdevice    = 0x7360,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_A169_B,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1461,
		.subdevice    = 0x6360,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_A169_B1,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x16be,
		.subdevice    = 0x0005,
		.driver_data  = SAA7134_BOARD_MD7134_BRIDGE_2,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x5168,
		.subdevice    = 0x0300,
		.driver_data  = SAA7134_BOARD_FLYDVBS_LR300,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x4e42,
		.subdevice    = 0x0300,
		.driver_data  = SAA7134_BOARD_FLYDVBS_LR300,
	},{
		.vendor = PCI_VENDOR_ID_PHILIPS,
		.device = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor = 0x1489,
		.subdevice = 0x0301,
		.driver_data = SAA7134_BOARD_FLYDVBT_LR301,
	},{
		.vendor = PCI_VENDOR_ID_PHILIPS,
		.device = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor = 0x5168, 
		.subdevice = 0x0304,
		.driver_data = SAA7134_BOARD_FLYTVPLATINUM_FM,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5168,
		.subdevice    = 0x3306,
		.driver_data  = SAA7134_BOARD_FLYDVBT_HYBRID_CARDBUS,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5168,
		.subdevice    = 0x3502,  
		.driver_data  = SAA7134_BOARD_FLYDVBT_HYBRID_CARDBUS,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5168,
		.subdevice    = 0x3307, 
		.driver_data  = SAA7134_BOARD_FLYDVBT_HYBRID_CARDBUS,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x16be,
		.subdevice    = 0x0007,
		.driver_data  = SAA7134_BOARD_MEDION_MD8800_QUADRO,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x16be,
		.subdevice    = 0x0008,
		.driver_data  = SAA7134_BOARD_MEDION_MD8800_QUADRO,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x16be,
		.subdevice    = 0x000d, 
		.driver_data  = SAA7134_BOARD_MEDION_MD8800_QUADRO,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1461,
		.subdevice    = 0x2c05,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_777,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1489,
		.subdevice    = 0x0502,                
		.driver_data  = SAA7134_BOARD_FLYDVBT_DUO_CARDBUS,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x0919, 
		.subdevice    = 0x2003,
		.driver_data  = SAA7134_BOARD_PROTEUS_2309,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1461,
		.subdevice    = 0x2c00,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_A16AR,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1043,
		.subdevice    = 0x4860,
		.driver_data  = SAA7134_BOARD_ASUS_EUROPA2_HYBRID,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x11bd,
		.subdevice    = 0x002f,
		.driver_data  = SAA7134_BOARD_PINNACLE_PCTV_310i,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1461, 
		.subdevice    = 0x9715,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_STUDIO_507,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1461, 
		.subdevice    = 0xa11b,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_STUDIO_507UA,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1043,
		.subdevice    = 0x4876,
		.driver_data  = SAA7134_BOARD_ASUSTeK_P7131_HYBRID_LNA,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x0070,
		.subdevice    = 0x6700,
		.driver_data  = SAA7134_BOARD_HAUPPAUGE_HVR1110,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x0070,
		.subdevice    = 0x6701,
		.driver_data  = SAA7134_BOARD_HAUPPAUGE_HVR1110,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x0070,
		.subdevice    = 0x6702,
		.driver_data  = SAA7134_BOARD_HAUPPAUGE_HVR1110,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x0070,
		.subdevice    = 0x6703,
		.driver_data  = SAA7134_BOARD_HAUPPAUGE_HVR1110,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x0070,
		.subdevice    = 0x6704,
		.driver_data  = SAA7134_BOARD_HAUPPAUGE_HVR1110,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x0070,
		.subdevice    = 0x6705,
		.driver_data  = SAA7134_BOARD_HAUPPAUGE_HVR1110,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x0070,
		.subdevice    = 0x6706,
		.driver_data  = SAA7134_BOARD_HAUPPAUGE_HVR1150,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x0070,
		.subdevice    = 0x6707,
		.driver_data  = SAA7134_BOARD_HAUPPAUGE_HVR1120,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x0070,
		.subdevice    = 0x6708,
		.driver_data  = SAA7134_BOARD_HAUPPAUGE_HVR1150,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x0070,
		.subdevice    = 0x6709,
		.driver_data  = SAA7134_BOARD_HAUPPAUGE_HVR1120,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x0070,
		.subdevice    = 0x670a,
		.driver_data  = SAA7134_BOARD_HAUPPAUGE_HVR1120,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x153b,
		.subdevice    = 0x1172,
		.driver_data  = SAA7134_BOARD_CINERGY_HT_PCMCIA,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = PCI_VENDOR_ID_PHILIPS,
		.subdevice    = 0x2342,
		.driver_data  = SAA7134_BOARD_ENCORE_ENLTV,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x1131,
		.subdevice    = 0x2341,
		.driver_data  = SAA7134_BOARD_ENCORE_ENLTV,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x3016,
		.subdevice    = 0x2344,
		.driver_data  = SAA7134_BOARD_ENCORE_ENLTV,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x1131,
		.subdevice    = 0x230f,
		.driver_data  = SAA7134_BOARD_ENCORE_ENLTV_FM,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x1a7f,
		.subdevice    = 0x2008,
		.driver_data  = SAA7134_BOARD_ENCORE_ENLTV_FM53,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1a7f,
		.subdevice    = 0x2108,
		.driver_data  = SAA7134_BOARD_ENCORE_ENLTV_FM3,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x153b,
		.subdevice    = 0x1175,
		.driver_data  = SAA7134_BOARD_CINERGY_HT_PCI,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1461, 
		.subdevice    = 0xf31e,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_M102,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x4E42,         
		.subdevice    = 0x0306,         
		.driver_data  = SAA7134_BOARD_FLYDVBTDUO,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1043,
		.subdevice    = 0x4871,
		.driver_data  = SAA7134_BOARD_ASUS_P7131_4871,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1043,
		.subdevice    = 0x4857,		
		.driver_data  = SAA7134_BOARD_ASUSTeK_TIGER,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x0919, 
		.subdevice    = 0x2003, 
		.driver_data  = SAA7134_BOARD_SABRENT_TV_PCB05,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = PCI_VENDOR_ID_PHILIPS,
		.subdevice    = 0x2304,
		.driver_data  = SAA7134_BOARD_10MOONSTVMASTER3,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1461, 
		.subdevice    = 0xf01d, 
		.driver_data  = SAA7134_BOARD_AVERMEDIA_SUPER_007,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x0000,
		.subdevice    = 0x4016,
		.driver_data  = SAA7134_BOARD_BEHOLD_401,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x0000,
		.subdevice    = 0x4036,
		.driver_data  = SAA7134_BOARD_BEHOLD_403,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x0000,
		.subdevice    = 0x4037,
		.driver_data  = SAA7134_BOARD_BEHOLD_403FM,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x0000,
		.subdevice    = 0x4050,
		.driver_data  = SAA7134_BOARD_BEHOLD_405,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x0000,
		.subdevice    = 0x4051,
		.driver_data  = SAA7134_BOARD_BEHOLD_405FM,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x0000,
		.subdevice    = 0x4070,
		.driver_data  = SAA7134_BOARD_BEHOLD_407,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x0000,
		.subdevice    = 0x4071,
		.driver_data  = SAA7134_BOARD_BEHOLD_407FM,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x0000,
		.subdevice    = 0x4090,
		.driver_data  = SAA7134_BOARD_BEHOLD_409,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x0000,
		.subdevice    = 0x505B,
		.driver_data  = SAA7134_BOARD_BEHOLD_505RDS_MK5,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x0000,
		.subdevice    = 0x5051,
		.driver_data  = SAA7134_BOARD_BEHOLD_505RDS_MK3,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x5ace,
		.subdevice    = 0x5050,
		.driver_data  = SAA7134_BOARD_BEHOLD_505FM,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x0000,
		.subdevice    = 0x5071,
		.driver_data  = SAA7134_BOARD_BEHOLD_507RDS_MK3,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x0000,
		.subdevice    = 0x507B,
		.driver_data  = SAA7134_BOARD_BEHOLD_507RDS_MK5,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x5ace,
		.subdevice    = 0x5070,
		.driver_data  = SAA7134_BOARD_BEHOLD_507_9FM,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5ace,
		.subdevice    = 0x5090,
		.driver_data  = SAA7134_BOARD_BEHOLD_507_9FM,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x0000,
		.subdevice    = 0x5201,
		.driver_data  = SAA7134_BOARD_BEHOLD_COLUMBUS_TVFM,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x5ace,
		.subdevice    = 0x6070,
		.driver_data  = SAA7134_BOARD_BEHOLD_607FM_MK3,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x5ace,
		.subdevice    = 0x6071,
		.driver_data  = SAA7134_BOARD_BEHOLD_607FM_MK5,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x5ace,
		.subdevice    = 0x6072,
		.driver_data  = SAA7134_BOARD_BEHOLD_607RDS_MK3,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x5ace,
		.subdevice    = 0x6073,
		.driver_data  = SAA7134_BOARD_BEHOLD_607RDS_MK5,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5ace,
		.subdevice    = 0x6090,
		.driver_data  = SAA7134_BOARD_BEHOLD_609FM_MK3,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5ace,
		.subdevice    = 0x6091,
		.driver_data  = SAA7134_BOARD_BEHOLD_609FM_MK5,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5ace,
		.subdevice    = 0x6092,
		.driver_data  = SAA7134_BOARD_BEHOLD_609RDS_MK3,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5ace,
		.subdevice    = 0x6093,
		.driver_data  = SAA7134_BOARD_BEHOLD_609RDS_MK5,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5ace,
		.subdevice    = 0x6190,
		.driver_data  = SAA7134_BOARD_BEHOLD_M6,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5ace,
		.subdevice    = 0x6193,
		.driver_data  = SAA7134_BOARD_BEHOLD_M6_EXTRA,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5ace,
		.subdevice    = 0x6191,
		.driver_data  = SAA7134_BOARD_BEHOLD_M63,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x4e42,
		.subdevice    = 0x3502,
		.driver_data  = SAA7134_BOARD_FLYDVBT_HYBRID_CARDBUS,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1822, 
		.subdevice    = 0x0022,
		.driver_data  = SAA7134_BOARD_TWINHAN_DTV_DVB_3056,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x16be,
		.subdevice    = 0x0010, 
		.driver_data  = SAA7134_BOARD_CREATIX_CTX953,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1462, 
		.subdevice    = 0x8625, 
		.driver_data  = SAA7134_BOARD_MSI_TVANYWHERE_AD11,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1461, 
		.subdevice    = 0xf436,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_CARDBUS_506,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1461, 
		.subdevice    = 0xf936,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_A16D,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1461, 
		.subdevice    = 0xa836,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_M115,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x185b,
		.subdevice    = 0xc900,
		.driver_data  = SAA7134_BOARD_VIDEOMATE_T750,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133, 
		.subvendor    = 0x1421,
		.subdevice    = 0x0380,
		.driver_data  = SAA7134_BOARD_ADS_INSTANT_HDTV_PCI,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5169,
		.subdevice    = 0x1502,
		.driver_data  = SAA7134_BOARD_FLYTVPLATINUM_MINI,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5ace,
		.subdevice    = 0x6290,
		.driver_data  = SAA7134_BOARD_BEHOLD_H6,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1461, 
		.subdevice    = 0xf636,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_M103,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1461, 
		.subdevice    = 0xf736,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_M103,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1043,
		.subdevice    = 0x4878, 
		.driver_data  = SAA7134_BOARD_ASUSTeK_TIGER_3IN1,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x17de,
		.subdevice    = 0x7128,
		.driver_data  = SAA7134_BOARD_KWORLD_PLUS_TV_ANALOG,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x17de,
		.subdevice    = 0xb136,
		.driver_data  = SAA7134_BOARD_KWORLD_PCI_SBTVD_FULLSEG,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x1461, 
		.subdevice    = 0xf31d,
		.driver_data  = SAA7134_BOARD_AVERMEDIA_GO_007_FM_PLUS,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x185b,
		.subdevice    = 0xc900,
		.driver_data  = SAA7134_BOARD_VIDEOMATE_S350,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5ace, 
		.subdevice    = 0x7595,
		.driver_data  = SAA7134_BOARD_BEHOLD_X7,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x19d1, 
		.subdevice    = 0x0138, 
		.driver_data  = SAA7134_BOARD_ROVERMEDIA_LINK_PRO_FM,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = PCI_VENDOR_ID_PHILIPS,
		.subdevice    = 0x2004,
		.driver_data  = SAA7134_BOARD_ZOLID_HYBRID_PCI,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x1043,
		.subdevice    = 0x4847,
		.driver_data  = SAA7134_BOARD_ASUS_EUROPA_HYBRID,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x107d,
		.subdevice    = 0x6655,
		.driver_data  = SAA7134_BOARD_LEADTEK_WINFAST_DTV1000S,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x13c2,
		.subdevice    = 0x2804,
		.driver_data  = SAA7134_BOARD_TECHNOTREND_BUDGET_T3000,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5ace, 
		.subdevice    = 0x7190,
		.driver_data  = SAA7134_BOARD_BEHOLD_H7,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5ace, 
		.subdevice    = 0x7090,
		.driver_data  = SAA7134_BOARD_BEHOLD_A7,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7135,
		.subvendor    = 0x185b,
		.subdevice    = 0xc900,
		.driver_data  = SAA7134_BOARD_VIDEOMATE_M1F,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x5ace,
		.subdevice    = 0x5030,
		.driver_data  = SAA7134_BOARD_BEHOLD_503FM,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = 0x5ace,
		.subdevice    = 0x5010,
		.driver_data  = SAA7134_BOARD_BEHOLD_501,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = 0x17de,
		.subdevice    = 0xd136,
		.driver_data  = SAA7134_BOARD_MAGICPRO_PROHDTV_PRO2,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x6000,
		.subdevice    = 0x0811,
		.driver_data  = SAA7134_BOARD_SENSORAY811_911,
	}, {
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = 0x6000,
		.subdevice    = 0x0911,
		.driver_data  = SAA7134_BOARD_SENSORAY811_911,
	}, {
		
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = PCI_VENDOR_ID_PHILIPS,
		.subdevice    = 0,
		.driver_data  = SAA7134_BOARD_NOAUTO,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = PCI_VENDOR_ID_PHILIPS,
		.subdevice    = 0,
		.driver_data  = SAA7134_BOARD_NOAUTO,
	},{
		
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7130,
		.subvendor    = PCI_ANY_ID,
		.subdevice    = PCI_ANY_ID,
		.driver_data  = SAA7134_BOARD_UNKNOWN,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7133,
		.subvendor    = PCI_ANY_ID,
		.subdevice    = PCI_ANY_ID,
		.driver_data  = SAA7134_BOARD_UNKNOWN,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7134,
		.subvendor    = PCI_ANY_ID,
		.subdevice    = PCI_ANY_ID,
		.driver_data  = SAA7134_BOARD_UNKNOWN,
	},{
		.vendor       = PCI_VENDOR_ID_PHILIPS,
		.device       = PCI_DEVICE_ID_PHILIPS_SAA7135,
		.subvendor    = PCI_ANY_ID,
		.subdevice    = PCI_ANY_ID,
		.driver_data  = SAA7134_BOARD_UNKNOWN,
	},{
		
	}
};
MODULE_DEVICE_TABLE(pci, saa7134_pci_tbl);



static void board_flyvideo(struct saa7134_dev *dev)
{
	printk("%s: there are different flyvideo cards with different tuners\n"
	       "%s: out there, you might have to use the tuner=<nr> insmod\n"
	       "%s: option to override the default value.\n",
	       dev->name, dev->name, dev->name);
}

static int saa7134_xc2028_callback(struct saa7134_dev *dev,
				   int command, int arg)
{
	switch (command) {
	case XC2028_TUNER_RESET:
		saa_andorl(SAA7134_GPIO_GPSTATUS0 >> 2, 0x00008000, 0x00000000);
		saa_andorl(SAA7134_GPIO_GPSTATUS0 >> 2, 0x00008000, 0x00008000);
		switch (dev->board) {
		case SAA7134_BOARD_AVERMEDIA_CARDBUS_506:
		case SAA7134_BOARD_AVERMEDIA_M103:
			saa7134_set_gpio(dev, 23, 0);
			msleep(10);
			saa7134_set_gpio(dev, 23, 1);
		break;
		case SAA7134_BOARD_AVERMEDIA_A16D:
			saa7134_set_gpio(dev, 21, 0);
			msleep(10);
			saa7134_set_gpio(dev, 21, 1);
		break;
		case SAA7134_BOARD_AVERMEDIA_A700_HYBRID:
			saa7134_set_gpio(dev, 18, 0);
			msleep(10);
			saa7134_set_gpio(dev, 18, 1);
		break;
		case SAA7134_BOARD_VIDEOMATE_T750:
			saa7134_set_gpio(dev, 20, 0);
			msleep(10);
			saa7134_set_gpio(dev, 20, 1);
		break;
		}
	return 0;
	}
	return -EINVAL;
}

static int saa7134_xc5000_callback(struct saa7134_dev *dev,
				   int command, int arg)
{
	switch (dev->board) {
	case SAA7134_BOARD_BEHOLD_X7:
	case SAA7134_BOARD_BEHOLD_H7:
	case SAA7134_BOARD_BEHOLD_A7:
		if (command == XC5000_TUNER_RESET) {
		
			saa_writeb(SAA7134_SPECIAL_MODE, 0x00);
			msleep(10);
			saa_writeb(SAA7134_SPECIAL_MODE, 0x01);
			msleep(10);
		}
		break;
	default:
		saa_andorl(SAA7134_GPIO_GPMODE0 >> 2, 0x06e20000, 0x06e20000);
		saa_andorl(SAA7134_GPIO_GPSTATUS0 >> 2, 0x06a20000, 0x06a20000);
		saa_andorl(SAA7133_ANALOG_IO_SELECT >> 2, 0x02, 0x02);
		saa_andorl(SAA7134_ANALOG_IN_CTRL1 >> 2, 0x81, 0x81);
		saa_andorl(SAA7134_AUDIO_CLOCK0 >> 2, 0x03187de7, 0x03187de7);
		saa_andorl(SAA7134_AUDIO_PLL_CTRL >> 2, 0x03, 0x03);
		saa_andorl(SAA7134_AUDIO_CLOCKS_PER_FIELD0 >> 2,
			   0x0001e000, 0x0001e000);
		break;
	}
	return 0;
}

static int saa7134_tda8290_827x_callback(struct saa7134_dev *dev,
					 int command, int arg)
{
	u8 sync_control;

	switch (command) {
	case 0: 
		saa7134_set_gpio(dev, 22, arg) ;
		break;
	case 1: 
		saa_andorb(SAA7134_VIDEO_PORT_CTRL3, 0x80, 0x80);
		saa_andorb(SAA7134_VIDEO_PORT_CTRL6, 0x0f, 0x03);
		if (arg == 1)
			sync_control = 11;
		else
			sync_control = 17;
		saa_writeb(SAA7134_VGATE_START, sync_control);
		saa_writeb(SAA7134_VGATE_STOP, sync_control + 1);
		saa_andorb(SAA7134_MISC_VGATE_MSB, 0x03, 0x00);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static inline int saa7134_tda18271_hvr11x0_toggle_agc(struct saa7134_dev *dev,
						      enum tda18271_mode mode)
{
	
	switch (mode) {
	case TDA18271_ANALOG:
		saa7134_set_gpio(dev, 26, 0);
		break;
	case TDA18271_DIGITAL:
		saa7134_set_gpio(dev, 26, 1);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static inline int saa7134_kworld_sbtvd_toggle_agc(struct saa7134_dev *dev,
						  enum tda18271_mode mode)
{
	
	switch (mode) {
	case TDA18271_ANALOG:
		saa_writel(SAA7134_GPIO_GPMODE0 >> 2, 0x4000);
		saa_writel(SAA7134_GPIO_GPSTATUS0 >> 2, 0x4000);
		msleep(20);
		break;
	case TDA18271_DIGITAL:
		saa_writel(SAA7134_GPIO_GPMODE0 >> 2, 0x14000);
		saa_writel(SAA7134_GPIO_GPSTATUS0 >> 2, 0x14000);
		msleep(20);
		saa_writel(SAA7134_GPIO_GPMODE0 >> 2, 0x54000);
		saa_writel(SAA7134_GPIO_GPSTATUS0 >> 2, 0x54000);
		msleep(30);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int saa7134_kworld_pc150u_toggle_agc(struct saa7134_dev *dev,
					    enum tda18271_mode mode)
{
	switch (mode) {
	case TDA18271_ANALOG:
		saa7134_set_gpio(dev, 18, 0);
		break;
	case TDA18271_DIGITAL:
		saa7134_set_gpio(dev, 18, 1);
		msleep(30);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int saa7134_tda8290_18271_callback(struct saa7134_dev *dev,
					  int command, int arg)
{
	int ret = 0;

	switch (command) {
	case TDA18271_CALLBACK_CMD_AGC_ENABLE: 
		switch (dev->board) {
		case SAA7134_BOARD_HAUPPAUGE_HVR1150:
		case SAA7134_BOARD_HAUPPAUGE_HVR1120:
		case SAA7134_BOARD_MAGICPRO_PROHDTV_PRO2:
			ret = saa7134_tda18271_hvr11x0_toggle_agc(dev, arg);
			break;
		case SAA7134_BOARD_KWORLD_PCI_SBTVD_FULLSEG:
			ret = saa7134_kworld_sbtvd_toggle_agc(dev, arg);
			break;
		case SAA7134_BOARD_KWORLD_PC150U:
			ret = saa7134_kworld_pc150u_toggle_agc(dev, arg);
			break;
		default:
			break;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int saa7134_tda8290_callback(struct saa7134_dev *dev,
				    int command, int arg)
{
	int ret;

	switch (dev->board) {
	case SAA7134_BOARD_HAUPPAUGE_HVR1150:
	case SAA7134_BOARD_HAUPPAUGE_HVR1120:
	case SAA7134_BOARD_AVERMEDIA_M733A:
	case SAA7134_BOARD_KWORLD_PCI_SBTVD_FULLSEG:
	case SAA7134_BOARD_KWORLD_PC150U:
	case SAA7134_BOARD_MAGICPRO_PROHDTV_PRO2:
		
		ret = saa7134_tda8290_18271_callback(dev, command, arg);
		break;
	default:
		
		ret = saa7134_tda8290_827x_callback(dev, command, arg);
		break;
	}
	return ret;
}

int saa7134_tuner_callback(void *priv, int component, int command, int arg)
{
	struct saa7134_dev *dev = priv;

	if (dev != NULL) {
		switch (dev->tuner_type) {
		case TUNER_PHILIPS_TDA8290:
			return saa7134_tda8290_callback(dev, command, arg);
		case TUNER_XC2028:
			return saa7134_xc2028_callback(dev, command, arg);
		case TUNER_XC5000:
			return saa7134_xc5000_callback(dev, command, arg);
		}
	} else {
		printk(KERN_ERR "saa7134: Error - device struct undefined.\n");
		return -EINVAL;
	}
	return -EINVAL;
}
EXPORT_SYMBOL(saa7134_tuner_callback);


static void hauppauge_eeprom(struct saa7134_dev *dev, u8 *eeprom_data)
{
	struct tveeprom tv;

	tveeprom_hauppauge_analog(&dev->i2c_client, &tv, eeprom_data);

	
	switch (tv.model) {
	case 67019: 
	case 67109: 
	case 67201: 
	case 67301: 
	case 67209: 
	case 67559: 
	case 67569: 
	case 67579: 
	case 67589: 
	case 67599: 
	case 67651: 
	case 67659: 
		break;
	default:
		printk(KERN_WARNING "%s: warning: "
		       "unknown hauppauge model #%d\n", dev->name, tv.model);
		break;
	}

	printk(KERN_INFO "%s: hauppauge eeprom: model=%d\n",
	       dev->name, tv.model);
}


int saa7134_board_init1(struct saa7134_dev *dev)
{
	
	saa_writel(SAA7134_GPIO_GPMODE0 >> 2, 0);
	dev->gpio_value = saa_readl(SAA7134_GPIO_GPSTATUS0 >> 2);
	printk(KERN_INFO "%s: board init: gpio is %x\n", dev->name, dev->gpio_value);

	switch (dev->board) {
	case SAA7134_BOARD_FLYVIDEO2000:
	case SAA7134_BOARD_FLYVIDEO3000:
	case SAA7134_BOARD_FLYVIDEO3000_NTSC:
		dev->has_remote = SAA7134_REMOTE_GPIO;
		board_flyvideo(dev);
		break;
	case SAA7134_BOARD_FLYTVPLATINUM_MINI2:
	case SAA7134_BOARD_FLYTVPLATINUM_FM:
	case SAA7134_BOARD_CINERGY400:
	case SAA7134_BOARD_CINERGY600:
	case SAA7134_BOARD_CINERGY600_MK3:
	case SAA7134_BOARD_ECS_TVP3XP:
	case SAA7134_BOARD_ECS_TVP3XP_4CB5:
	case SAA7134_BOARD_ECS_TVP3XP_4CB6:
	case SAA7134_BOARD_MD2819:
	case SAA7134_BOARD_KWORLD_VSTREAM_XPERT:
	case SAA7134_BOARD_KWORLD_XPERT:
	case SAA7134_BOARD_AVERMEDIA_STUDIO_305:
	case SAA7134_BOARD_AVERMEDIA_STUDIO_505:
	case SAA7134_BOARD_AVERMEDIA_305:
	case SAA7134_BOARD_AVERMEDIA_STUDIO_307:
	case SAA7134_BOARD_AVERMEDIA_307:
	case SAA7134_BOARD_AVERMEDIA_STUDIO_507:
	case SAA7134_BOARD_AVERMEDIA_GO_007_FM:
	case SAA7134_BOARD_AVERMEDIA_777:
	case SAA7134_BOARD_AVERMEDIA_M135A:
 
	case SAA7134_BOARD_VIDEOMATE_TV_PVR:
	case SAA7134_BOARD_VIDEOMATE_GOLD_PLUS:
	case SAA7134_BOARD_VIDEOMATE_TV_GOLD_PLUSII:
	case SAA7134_BOARD_VIDEOMATE_M1F:
	case SAA7134_BOARD_VIDEOMATE_DVBT_300:
	case SAA7134_BOARD_VIDEOMATE_DVBT_200:
	case SAA7134_BOARD_VIDEOMATE_DVBT_200A:
	case SAA7134_BOARD_MANLI_MTV001:
	case SAA7134_BOARD_MANLI_MTV002:
	case SAA7134_BOARD_BEHOLD_409FM:
	case SAA7134_BOARD_AVACSSMARTTV:
	case SAA7134_BOARD_GOTVIEW_7135:
	case SAA7134_BOARD_KWORLD_TERMINATOR:
	case SAA7134_BOARD_SEDNA_PC_TV_CARDBUS:
	case SAA7134_BOARD_FLYDVBT_LR301:
	case SAA7134_BOARD_ASUSTeK_P7131_DUAL:
	case SAA7134_BOARD_ASUSTeK_P7131_HYBRID_LNA:
	case SAA7134_BOARD_ASUSTeK_P7131_ANALOG:
	case SAA7134_BOARD_FLYDVBTDUO:
	case SAA7134_BOARD_PROTEUS_2309:
	case SAA7134_BOARD_AVERMEDIA_A16AR:
	case SAA7134_BOARD_ENCORE_ENLTV:
	case SAA7134_BOARD_ENCORE_ENLTV_FM:
	case SAA7134_BOARD_ENCORE_ENLTV_FM53:
	case SAA7134_BOARD_ENCORE_ENLTV_FM3:
	case SAA7134_BOARD_10MOONSTVMASTER3:
	case SAA7134_BOARD_BEHOLD_401:
	case SAA7134_BOARD_BEHOLD_403:
	case SAA7134_BOARD_BEHOLD_403FM:
	case SAA7134_BOARD_BEHOLD_405:
	case SAA7134_BOARD_BEHOLD_405FM:
	case SAA7134_BOARD_BEHOLD_407:
	case SAA7134_BOARD_BEHOLD_407FM:
	case SAA7134_BOARD_BEHOLD_409:
	case SAA7134_BOARD_BEHOLD_505FM:
	case SAA7134_BOARD_BEHOLD_505RDS_MK5:
	case SAA7134_BOARD_BEHOLD_505RDS_MK3:
	case SAA7134_BOARD_BEHOLD_507_9FM:
	case SAA7134_BOARD_BEHOLD_507RDS_MK3:
	case SAA7134_BOARD_BEHOLD_507RDS_MK5:
	case SAA7134_BOARD_GENIUS_TVGO_A11MCE:
	case SAA7134_BOARD_REAL_ANGEL_220:
	case SAA7134_BOARD_KWORLD_PLUS_TV_ANALOG:
	case SAA7134_BOARD_AVERMEDIA_GO_007_FM_PLUS:
	case SAA7134_BOARD_ROVERMEDIA_LINK_PRO_FM:
	case SAA7134_BOARD_LEADTEK_WINFAST_DTV1000S:
		dev->has_remote = SAA7134_REMOTE_GPIO;
		break;
	case SAA7134_BOARD_FLYDVBS_LR300:
		saa_writeb(SAA7134_GPIO_GPMODE3, 0x80);
		saa_writeb(SAA7134_GPIO_GPSTATUS2, 0x40);
		dev->has_remote = SAA7134_REMOTE_GPIO;
		break;
	case SAA7134_BOARD_MD5044:
		printk("%s: seems there are two different versions of the MD5044\n"
		       "%s: (with the same ID) out there.  If sound doesn't work for\n"
		       "%s: you try the audio_clock_override=0x200000 insmod option.\n",
		       dev->name,dev->name,dev->name);
		break;
	case SAA7134_BOARD_CINERGY400_CARDBUS:
		
		saa_andorl(SAA7134_GPIO_GPMODE0 >> 2,   0x00040000, 0x00040000);
		saa_andorl(SAA7134_GPIO_GPSTATUS0 >> 2, 0x00040000, 0x00000000);
		break;
	case SAA7134_BOARD_PINNACLE_300I_DVBT_PAL:
		
		saa_writeb(SAA7134_GPIO_GPMODE1, 0x80);
		saa_writeb(SAA7134_GPIO_GPSTATUS1, 0x80);
		break;
	case SAA7134_BOARD_MONSTERTV_MOBILE:
		
		saa_andorl(SAA7134_GPIO_GPMODE0 >> 2,   0x00040000, 0x00040000);
		saa_andorl(SAA7134_GPIO_GPSTATUS0 >> 2, 0x00040000, 0x00000004);
		break;
	case SAA7134_BOARD_FLYDVBT_DUO_CARDBUS:
		
		saa_writeb(SAA7134_GPIO_GPMODE3, 0x08);
		saa_writeb(SAA7134_GPIO_GPSTATUS3, 0x06);
		break;
	case SAA7134_BOARD_ADS_DUO_CARDBUS_PTV331:
	case SAA7134_BOARD_FLYDVBT_HYBRID_CARDBUS:
		saa_andorl(SAA7134_GPIO_GPMODE0 >> 2, 0x08000000, 0x08000000);
		saa_andorl(SAA7134_GPIO_GPSTATUS0 >> 2, 0x08000000, 0x00000000);
		break;
	case SAA7134_BOARD_AVERMEDIA_CARDBUS:
	case SAA7134_BOARD_AVERMEDIA_M115:
		
		saa_andorl(SAA7134_GPIO_GPMODE0 >> 2,   0xffffffff, 0);
		saa_andorl(SAA7134_GPIO_GPSTATUS0 >> 2, 0xffffffff, 0);
		msleep(10);
		
		saa_andorl(SAA7134_GPIO_GPMODE0 >> 2,   0xffffffff, 0xffffffff);
		saa_andorl(SAA7134_GPIO_GPSTATUS0 >> 2, 0xffffffff, 0xffffffff);
		msleep(10);
		break;
	case SAA7134_BOARD_AVERMEDIA_CARDBUS_501:
		
		saa_andorl(SAA7134_GPIO_GPMODE0 >> 2,   0x08400000, 0x08400000);
		saa_andorl(SAA7134_GPIO_GPSTATUS0 >> 2, 0x08400000, 0);
		msleep(10);
		saa_andorl(SAA7134_GPIO_GPMODE0 >> 2,   0x08400000, 0x08400000);
		saa_andorl(SAA7134_GPIO_GPSTATUS0 >> 2, 0x08400000, 0x08400000);
		msleep(10);
		dev->has_remote = SAA7134_REMOTE_I2C;
		break;
	case SAA7134_BOARD_AVERMEDIA_CARDBUS_506:
		saa7134_set_gpio(dev, 23, 0);
		msleep(10);
		saa7134_set_gpio(dev, 23, 1);
		dev->has_remote = SAA7134_REMOTE_I2C;
		break;
	case SAA7134_BOARD_AVERMEDIA_M103:
		saa7134_set_gpio(dev, 23, 0);
		msleep(10);
		saa7134_set_gpio(dev, 23, 1);
		break;
	case SAA7134_BOARD_AVERMEDIA_A16D:
		saa7134_set_gpio(dev, 21, 0);
		msleep(10);
		saa7134_set_gpio(dev, 21, 1);
		msleep(1);
		dev->has_remote = SAA7134_REMOTE_GPIO;
		break;
	case SAA7134_BOARD_BEHOLD_COLUMBUS_TVFM:
		
		saa_andorl(SAA7134_GPIO_GPMODE0 >> 2,   0x000A8004, 0x000A8004);
		saa_andorl(SAA7134_GPIO_GPSTATUS0 >> 2, 0x000A8004, 0);
		msleep(10);
		
		saa_andorl(SAA7134_GPIO_GPMODE0 >> 2,   0x000A8004, 0x000A8004);
		saa_andorl(SAA7134_GPIO_GPSTATUS0 >> 2, 0x000A8004, 0x000A8004);
		msleep(10);
		
		dev->has_remote = SAA7134_REMOTE_GPIO;
		break;
	case SAA7134_BOARD_RTD_VFG7350:


		saa_writeb (SAA7134_PRODUCTION_TEST_MODE, 0x00);
		break;
	case SAA7134_BOARD_HAUPPAUGE_HVR1150:
	case SAA7134_BOARD_HAUPPAUGE_HVR1120:
		dev->has_remote = SAA7134_REMOTE_GPIO;
		
		saa7134_set_gpio(dev, 26, 0);
		msleep(1);

		saa7134_set_gpio(dev, 22, 0);
		msleep(10);
		saa7134_set_gpio(dev, 22, 1);
		break;
	
	case SAA7134_BOARD_PINNACLE_PCTV_110i:
	case SAA7134_BOARD_PINNACLE_PCTV_310i:
	case SAA7134_BOARD_UPMOST_PURPLE_TV:
	case SAA7134_BOARD_MSI_TVATANYWHERE_PLUS:
	case SAA7134_BOARD_HAUPPAUGE_HVR1110:
	case SAA7134_BOARD_BEHOLD_607FM_MK3:
	case SAA7134_BOARD_BEHOLD_607FM_MK5:
	case SAA7134_BOARD_BEHOLD_609FM_MK3:
	case SAA7134_BOARD_BEHOLD_609FM_MK5:
	case SAA7134_BOARD_BEHOLD_607RDS_MK3:
	case SAA7134_BOARD_BEHOLD_607RDS_MK5:
	case SAA7134_BOARD_BEHOLD_609RDS_MK3:
	case SAA7134_BOARD_BEHOLD_609RDS_MK5:
	case SAA7134_BOARD_BEHOLD_M6:
	case SAA7134_BOARD_BEHOLD_M63:
	case SAA7134_BOARD_BEHOLD_M6_EXTRA:
	case SAA7134_BOARD_BEHOLD_H6:
	case SAA7134_BOARD_BEHOLD_X7:
	case SAA7134_BOARD_BEHOLD_H7:
	case SAA7134_BOARD_BEHOLD_A7:
	case SAA7134_BOARD_KWORLD_PC150U:
		dev->has_remote = SAA7134_REMOTE_I2C;
		break;
	case SAA7134_BOARD_AVERMEDIA_A169_B:
		printk("%s: %s: dual saa713x broadcast decoders\n"
		       "%s: Sorry, none of the inputs to this chip are supported yet.\n"
		       "%s: Dual decoder functionality is disabled for now, use the other chip.\n",
		       dev->name,card(dev).name,dev->name,dev->name);
		break;
	case SAA7134_BOARD_AVERMEDIA_M102:
		
	       dev->has_remote = SAA7134_REMOTE_GPIO;
		saa_andorl(SAA7134_GPIO_GPMODE0 >> 2,   0x8c040007, 0x8c040007);
		saa_andorl(SAA7134_GPIO_GPSTATUS0 >> 2, 0x0c0007cd, 0x0c0007cd);
		break;
	case SAA7134_BOARD_AVERMEDIA_A700_HYBRID:
	case SAA7134_BOARD_AVERMEDIA_A700_PRO:
		
		saa_andorl(SAA7134_GPIO_GPMODE0 >> 2,   0x80040100, 0x80040100);
		saa_andorl(SAA7134_GPIO_GPSTATUS0 >> 2, 0x80040100, 0x00040100);
		break;
	case SAA7134_BOARD_VIDEOMATE_S350:
		dev->has_remote = SAA7134_REMOTE_GPIO;
		saa_andorl(SAA7134_GPIO_GPMODE0 >> 2,   0x0000C000, 0x0000C000);
		saa_andorl(SAA7134_GPIO_GPSTATUS0 >> 2, 0x0000C000, 0x0000C000);
		break;
	case SAA7134_BOARD_AVERMEDIA_M733A:
		saa7134_set_gpio(dev, 1, 1);
		msleep(10);
		saa7134_set_gpio(dev, 1, 0);
		msleep(10);
		saa7134_set_gpio(dev, 1, 1);
		dev->has_remote = SAA7134_REMOTE_GPIO;
		break;
	case SAA7134_BOARD_MAGICPRO_PROHDTV_PRO2:
		
		saa_andorl(SAA7134_GPIO_GPMODE0 >> 2,   0x0e050000, 0x0c050000);
		saa_andorl(SAA7134_GPIO_GPSTATUS0 >> 2, 0x0e050000, 0x0c050000);
		break;
	case SAA7134_BOARD_VIDEOMATE_T750:
		
		saa_andorl(SAA7134_GPIO_GPMODE0 >> 2,   0x00008000, 0x00008000);
		saa_andorl(SAA7134_GPIO_GPSTATUS0 >> 2, 0x00008000, 0x00008000);
		break;
	}
	return 0;
}

static void saa7134_tuner_setup(struct saa7134_dev *dev)
{
	struct tuner_setup tun_setup;
	unsigned int mode_mask = T_RADIO | T_ANALOG_TV;

	memset(&tun_setup, 0, sizeof(tun_setup));
	tun_setup.tuner_callback = saa7134_tuner_callback;

	if (saa7134_boards[dev->board].radio_type != UNSET) {
		tun_setup.type = saa7134_boards[dev->board].radio_type;
		tun_setup.addr = saa7134_boards[dev->board].radio_addr;

		tun_setup.mode_mask = T_RADIO;

		saa_call_all(dev, tuner, s_type_addr, &tun_setup);
		mode_mask &= ~T_RADIO;
	}

	if ((dev->tuner_type != TUNER_ABSENT) && (dev->tuner_type != UNSET)) {
		tun_setup.type = dev->tuner_type;
		tun_setup.addr = dev->tuner_addr;
		tun_setup.config = saa7134_boards[dev->board].tuner_config;
		tun_setup.tuner_callback = saa7134_tuner_callback;

		tun_setup.mode_mask = mode_mask;

		saa_call_all(dev, tuner, s_type_addr, &tun_setup);
	}

	if (dev->tda9887_conf) {
		struct v4l2_priv_tun_config tda9887_cfg;

		tda9887_cfg.tuner = TUNER_TDA9887;
		tda9887_cfg.priv = &dev->tda9887_conf;

		saa_call_all(dev, tuner, s_config, &tda9887_cfg);
	}

	if (dev->tuner_type == TUNER_XC2028) {
		struct v4l2_priv_tun_config  xc2028_cfg;
		struct xc2028_ctrl           ctl;

		memset(&xc2028_cfg, 0, sizeof(xc2028_cfg));
		memset(&ctl, 0, sizeof(ctl));

		ctl.fname   = XC2028_DEFAULT_FIRMWARE;
		ctl.max_len = 64;

		switch (dev->board) {
		case SAA7134_BOARD_AVERMEDIA_A16D:
		case SAA7134_BOARD_AVERMEDIA_CARDBUS_506:
		case SAA7134_BOARD_AVERMEDIA_M103:
		case SAA7134_BOARD_AVERMEDIA_A700_HYBRID:
			ctl.demod = XC3028_FE_ZARLINK456;
			break;
		default:
			ctl.demod = XC3028_FE_OREN538;
			ctl.mts = 1;
		}

		xc2028_cfg.tuner = TUNER_XC2028;
		xc2028_cfg.priv  = &ctl;

		saa_call_all(dev, tuner, s_config, &xc2028_cfg);
	}
}

int saa7134_board_init2(struct saa7134_dev *dev)
{
	unsigned char buf;
	int board;

	switch (dev->board) {
	case SAA7134_BOARD_BMK_MPEX_NOTUNER:
	case SAA7134_BOARD_BMK_MPEX_TUNER:
		dev->i2c_client.addr = 0x60;
		board = (i2c_master_recv(&dev->i2c_client, &buf, 0) < 0)
			? SAA7134_BOARD_BMK_MPEX_NOTUNER
			: SAA7134_BOARD_BMK_MPEX_TUNER;
		if (board == dev->board)
			break;
		dev->board = board;
		printk("%s: board type fixup: %s\n", dev->name,
		saa7134_boards[dev->board].name);
		dev->tuner_type = saa7134_boards[dev->board].tuner_type;

		break;
	case SAA7134_BOARD_MD7134:
	{
		u8 subaddr;
		u8 data[3];
		int ret, tuner_t;
		struct i2c_msg msg[] = {{.addr=0x50, .flags=0, .buf=&subaddr, .len = 1},
					{.addr=0x50, .flags=I2C_M_RD, .buf=data, .len = 3}};

		subaddr= 0x14;
		tuner_t = 0;

		ret = i2c_transfer(&dev->i2c_adap, msg, 2);
		if (ret != 2) {
			printk(KERN_ERR "EEPROM read failure\n");
		} else if ((data[0] != 0) && (data[0] != 0xff)) {
			
			subaddr = data[0] + 2;
			msg[1].len = 2;
			i2c_transfer(&dev->i2c_adap, msg, 2);
			tuner_t = (data[0] << 8) + data[1];
			switch (tuner_t){
			case 0x0103:
				dev->tuner_type = TUNER_PHILIPS_PAL;
				break;
			case 0x010C:
				dev->tuner_type = TUNER_PHILIPS_FM1216ME_MK3;
				break;
			default:
				printk(KERN_ERR "%s Can't determine tuner type %x from EEPROM\n", dev->name, tuner_t);
			}
		} else if ((data[1] != 0) && (data[1] != 0xff)) {
			
			subaddr = data[1] + 1;
			msg[1].len = 1;
			i2c_transfer(&dev->i2c_adap, msg, 2);
			subaddr = data[0] + 1;
			msg[1].len = 2;
			i2c_transfer(&dev->i2c_adap, msg, 2);
			tuner_t = (data[1] << 8) + data[0];
			switch (tuner_t) {
			case 0x0005:
				dev->tuner_type = TUNER_PHILIPS_FM1216ME_MK3;
				break;
			case 0x001d:
				dev->tuner_type = TUNER_PHILIPS_FMD1216ME_MK3;
					printk(KERN_INFO "%s Board has DVB-T\n", dev->name);
				break;
			default:
				printk(KERN_ERR "%s Can't determine tuner type %x from EEPROM\n", dev->name, tuner_t);
			}
		} else {
			printk(KERN_ERR "%s unexpected config structure\n", dev->name);
		}

		printk(KERN_INFO "%s Tuner type is %d\n", dev->name, dev->tuner_type);
		break;
	}
	case SAA7134_BOARD_PHILIPS_EUROPA:
		if (dev->autodetected && (dev->eedata[0x41] == 0x1c)) {
			
			dev->board = SAA7134_BOARD_PHILIPS_SNAKE;
			dev->tuner_type = saa7134_boards[dev->board].tuner_type;
			printk(KERN_INFO "%s: Reconfigured board as %s\n",
				dev->name, saa7134_boards[dev->board].name);
			break;
		}
		
	case SAA7134_BOARD_VIDEOMATE_DVBT_300:
	case SAA7134_BOARD_ASUS_EUROPA2_HYBRID:
	case SAA7134_BOARD_ASUS_EUROPA_HYBRID:
	case SAA7134_BOARD_TECHNOTREND_BUDGET_T3000:
	{

		u8 data[] = { 0x07, 0x02};
		struct i2c_msg msg = {.addr=0x08, .flags=0, .buf=data, .len = sizeof(data)};
		i2c_transfer(&dev->i2c_adap, &msg, 1);

		break;
	}
	case SAA7134_BOARD_PHILIPS_TIGER:
	case SAA7134_BOARD_PHILIPS_TIGER_S:
	{
		u8 data[] = { 0x3c, 0x33, 0x60};
		struct i2c_msg msg = {.addr=0x08, .flags=0, .buf=data, .len = sizeof(data)};
		if (dev->autodetected && (dev->eedata[0x49] == 0x50)) {
			dev->board = SAA7134_BOARD_PHILIPS_TIGER_S;
			printk(KERN_INFO "%s: Reconfigured board as %s\n",
				dev->name, saa7134_boards[dev->board].name);
		}
		if (dev->board == SAA7134_BOARD_PHILIPS_TIGER_S) {
			dev->tuner_type = TUNER_PHILIPS_TDA8290;

			data[2] = 0x68;
			i2c_transfer(&dev->i2c_adap, &msg, 1);
			break;
		}
		i2c_transfer(&dev->i2c_adap, &msg, 1);
		break;
	}
	case SAA7134_BOARD_ASUSTeK_TVFM7135:
	
	       if (dev->autodetected && (dev->eedata[0x27] == 0x03)) {
		       dev->board = SAA7134_BOARD_ASUSTeK_P7131_ANALOG;
		       printk(KERN_INFO "%s: P7131 analog only, using "
						       "entry of %s\n",
		       dev->name, saa7134_boards[dev->board].name);

			dev->has_remote = SAA7134_REMOTE_GPIO;
			saa7134_input_init1(dev);
	       }
	       break;
	case SAA7134_BOARD_HAUPPAUGE_HVR1150:
	case SAA7134_BOARD_HAUPPAUGE_HVR1120:
		hauppauge_eeprom(dev, dev->eedata+0x80);
		break;
	case SAA7134_BOARD_HAUPPAUGE_HVR1110:
		hauppauge_eeprom(dev, dev->eedata+0x80);
		
	case SAA7134_BOARD_PINNACLE_PCTV_310i:
	case SAA7134_BOARD_KWORLD_DVBT_210:
	case SAA7134_BOARD_TEVION_DVBT_220RF:
	case SAA7134_BOARD_ASUSTeK_TIGER:
	case SAA7134_BOARD_ASUSTeK_P7131_DUAL:
	case SAA7134_BOARD_ASUSTeK_P7131_HYBRID_LNA:
	case SAA7134_BOARD_MEDION_MD8800_QUADRO:
	case SAA7134_BOARD_AVERMEDIA_SUPER_007:
	case SAA7134_BOARD_TWINHAN_DTV_DVB_3056:
	case SAA7134_BOARD_CREATIX_CTX953:
	{
		u8 data[] = { 0x3c, 0x33, 0x60};
		struct i2c_msg msg = {.addr=0x08, .flags=0, .buf=data, .len = sizeof(data)};
		i2c_transfer(&dev->i2c_adap, &msg, 1);
		break;
	}
	case SAA7134_BOARD_ASUSTeK_TIGER_3IN1:
	{
		u8 data[] = { 0x3c, 0x33, 0x60};
		struct i2c_msg msg = {.addr = 0x0b, .flags = 0, .buf = data,
							.len = sizeof(data)};
		i2c_transfer(&dev->i2c_adap, &msg, 1);
		break;
	}
	case SAA7134_BOARD_FLYDVB_TRIO:
	{
		u8 temp = 0;
		int rc;
		u8 data[] = { 0x3c, 0x33, 0x62};
		struct i2c_msg msg = {.addr=0x09, .flags=0, .buf=data, .len = sizeof(data)};
		i2c_transfer(&dev->i2c_adap, &msg, 1);

		msg.buf = &temp;
		msg.addr = 0x0b;
		msg.len = 1;
		if (1 != i2c_transfer(&dev->i2c_adap, &msg, 1)) {
			printk(KERN_WARNING "%s: send wake up byte to pic16C505"
					"(IR chip) failed\n", dev->name);
		} else {
			msg.flags = I2C_M_RD;
			rc = i2c_transfer(&dev->i2c_adap, &msg, 1);
			printk(KERN_INFO "%s: probe IR chip @ i2c 0x%02x: %s\n",
				   dev->name, msg.addr,
				   (1 == rc) ? "yes" : "no");
			if (rc == 1)
				dev->has_remote = SAA7134_REMOTE_I2C;
		}
		break;
	}
	case SAA7134_BOARD_ADS_DUO_CARDBUS_PTV331:
	case SAA7134_BOARD_FLYDVBT_HYBRID_CARDBUS:
	{
		
		u8 data[] = { 0x3c, 0x33, 0x6a};
		struct i2c_msg msg = {.addr=0x08, .flags=0, .buf=data, .len = sizeof(data)};
		i2c_transfer(&dev->i2c_adap, &msg, 1);
		break;
	}
	case SAA7134_BOARD_CINERGY_HT_PCMCIA:
	case SAA7134_BOARD_CINERGY_HT_PCI:
	{
		
		u8 data[] = { 0x3c, 0x33, 0x68};
		struct i2c_msg msg = {.addr=0x08, .flags=0, .buf=data, .len = sizeof(data)};
		i2c_transfer(&dev->i2c_adap, &msg, 1);
		break;
	}
	case SAA7134_BOARD_VIDEOMATE_DVBT_200:
	case SAA7134_BOARD_VIDEOMATE_DVBT_200A:

		if (!dev->autodetected || (dev->eedata[0x41] == 0xd0))
			break;
		if (dev->eedata[0x41] == 0x02) {
			
			dev->board = SAA7134_BOARD_VIDEOMATE_DVBT_200A;
			dev->tuner_type   = saa7134_boards[dev->board].tuner_type;
			dev->tda9887_conf = saa7134_boards[dev->board].tda9887_conf;
			printk(KERN_INFO "%s: Reconfigured board as %s\n",
				dev->name, saa7134_boards[dev->board].name);
		} else {
			printk(KERN_WARNING "%s: Unexpected tuner type info: %x in eeprom\n",
				dev->name, dev->eedata[0x41]);
			break;
		}
		break;
	case SAA7134_BOARD_ADS_INSTANT_HDTV_PCI:
	case SAA7134_BOARD_KWORLD_ATSC110:
	{
		struct i2c_msg msg = { .addr = 0x0a, .flags = 0 };
		int i;
		static u8 buffer[][2] = {
			{ 0x10, 0x12 },
			{ 0x13, 0x04 },
			{ 0x16, 0x00 },
			{ 0x14, 0x04 },
			{ 0x17, 0x00 },
		};

		for (i = 0; i < ARRAY_SIZE(buffer); i++) {
			msg.buf = &buffer[i][0];
			msg.len = ARRAY_SIZE(buffer[0]);
			if (i2c_transfer(&dev->i2c_adap, &msg, 1) != 1)
				printk(KERN_WARNING
				       "%s: Unable to enable tuner(%i).\n",
				       dev->name, i);
		}
		break;
	}
	case SAA7134_BOARD_BEHOLD_H6:
	{
		u8 data[] = { 0x09, 0x9f, 0x86, 0x11};
		struct i2c_msg msg = {.addr = 0x61, .flags = 0, .buf = data,
							.len = sizeof(data)};

		
		
		
		
		
		if (i2c_transfer(&dev->i2c_adap, &msg, 1) != 1)
				printk(KERN_WARNING
				      "%s: Unable to enable IF of the tuner.\n",
				       dev->name);
		break;
	}
	case SAA7134_BOARD_KWORLD_PCI_SBTVD_FULLSEG:
		saa_writel(SAA7134_GPIO_GPMODE0 >> 2, 0x4000);
		saa_writel(SAA7134_GPIO_GPSTATUS0 >> 2, 0x4000);

		saa7134_set_gpio(dev, 27, 0);
		break;
	} 

	
	if (TUNER_ABSENT != dev->tuner_type) {
		int has_demod = (dev->tda9887_conf & TDA9887_PRESENT);

		if (dev->radio_type != UNSET)
			v4l2_i2c_new_subdev(&dev->v4l2_dev,
				&dev->i2c_adap, "tuner",
				dev->radio_addr, NULL);
		if (has_demod)
			v4l2_i2c_new_subdev(&dev->v4l2_dev,
				&dev->i2c_adap, "tuner",
				0, v4l2_i2c_tuner_addrs(ADDRS_DEMOD));
		if (dev->tuner_addr == ADDR_UNSET) {
			enum v4l2_i2c_tuner_type type =
				has_demod ? ADDRS_TV_WITH_DEMOD : ADDRS_TV;

			v4l2_i2c_new_subdev(&dev->v4l2_dev,
				&dev->i2c_adap, "tuner",
				0, v4l2_i2c_tuner_addrs(type));
		} else {
			v4l2_i2c_new_subdev(&dev->v4l2_dev,
				&dev->i2c_adap, "tuner",
				dev->tuner_addr, NULL);
		}
	}

	saa7134_tuner_setup(dev);

	switch (dev->board) {
	case SAA7134_BOARD_BEHOLD_COLUMBUS_TVFM:
	case SAA7134_BOARD_AVERMEDIA_CARDBUS_501:
	{
		struct v4l2_priv_tun_config tea5767_cfg;
		struct tea5767_ctrl ctl;

		dev->i2c_client.addr = 0xC0;
		
		memset(&ctl, 0, sizeof(ctl));
		ctl.xtal_freq = TEA5767_HIGH_LO_13MHz;
		tea5767_cfg.tuner = TUNER_TEA5767;
		tea5767_cfg.priv  = &ctl;
		saa_call_all(dev, tuner, s_config, &tea5767_cfg);
		break;
	}
	} 

	return 0;
}
