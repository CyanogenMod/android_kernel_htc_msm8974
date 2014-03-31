/*
 * tveeprom - eeprom decoder for tvcard configuration eeproms
 *
 * Data and decoding routines shamelessly borrowed from bttv-cards.c
 * eeprom access routine shamelessly borrowed from bttv-if.c
 * which are:

    Copyright (C) 1996,97,98 Ralph  Metzler (rjkm@thp.uni-koeln.de)
			   & Marcus Metzler (mocm@thp.uni-koeln.de)
    (c) 1999-2001 Gerd Knorr <kraxel@goldbach.in-berlin.de>

 * Adjustments to fit a more general model and all bugs:

	Copyright (C) 2003 John Klar <linpvr at projectplasma.com>

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/i2c.h>

#include <media/tuner.h>
#include <media/tveeprom.h>
#include <media/v4l2-common.h>
#include <media/v4l2-chip-ident.h>

MODULE_DESCRIPTION("i2c Hauppauge eeprom decoder driver");
MODULE_AUTHOR("John Klar");
MODULE_LICENSE("GPL");

static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-1)");

#define STRM(array, i) \
	(i < sizeof(array) / sizeof(char *) ? array[i] : "unknown")

#define tveeprom_info(fmt, arg...) \
	v4l_printk(KERN_INFO, "tveeprom", c->adapter, c->addr, fmt , ## arg)
#define tveeprom_warn(fmt, arg...) \
	v4l_printk(KERN_WARNING, "tveeprom", c->adapter, c->addr, fmt , ## arg)
#define tveeprom_dbg(fmt, arg...) do { \
	if (debug) \
		v4l_printk(KERN_DEBUG, "tveeprom", \
				c->adapter, c->addr, fmt , ## arg); \
	} while (0)

static struct HAUPPAUGE_TUNER_FMT
{
	int	id;
	char *name;
}
hauppauge_tuner_fmt[] =
{
	{ V4L2_STD_UNKNOWN,                   " UNKNOWN" },
	{ V4L2_STD_UNKNOWN,                   " FM" },
	{ V4L2_STD_B|V4L2_STD_GH,             " PAL(B/G)" },
	{ V4L2_STD_MN,                        " NTSC(M)" },
	{ V4L2_STD_PAL_I,                     " PAL(I)" },
	{ V4L2_STD_SECAM_L|V4L2_STD_SECAM_LC, " SECAM(L/L')" },
	{ V4L2_STD_DK,                        " PAL(D/D1/K)" },
	{ V4L2_STD_ATSC,                      " ATSC/DVB Digital" },
};

static struct HAUPPAUGE_TUNER
{
	int  id;
	char *name;
}
hauppauge_tuner[] =
{
	
	{ TUNER_ABSENT,        		"None" },
	{ TUNER_ABSENT,        		"External" },
	{ TUNER_ABSENT,        		"Unspecified" },
	{ TUNER_PHILIPS_PAL,   		"Philips FI1216" },
	{ TUNER_PHILIPS_SECAM, 		"Philips FI1216MF" },
	{ TUNER_PHILIPS_NTSC,  		"Philips FI1236" },
	{ TUNER_PHILIPS_PAL_I, 		"Philips FI1246" },
	{ TUNER_PHILIPS_PAL_DK,		"Philips FI1256" },
	{ TUNER_PHILIPS_PAL,   		"Philips FI1216 MK2" },
	{ TUNER_PHILIPS_SECAM, 		"Philips FI1216MF MK2" },
	
	{ TUNER_PHILIPS_NTSC,  		"Philips FI1236 MK2" },
	{ TUNER_PHILIPS_PAL_I, 		"Philips FI1246 MK2" },
	{ TUNER_PHILIPS_PAL_DK,		"Philips FI1256 MK2" },
	{ TUNER_TEMIC_NTSC,    		"Temic 4032FY5" },
	{ TUNER_TEMIC_PAL,     		"Temic 4002FH5" },
	{ TUNER_TEMIC_PAL_I,   		"Temic 4062FY5" },
	{ TUNER_PHILIPS_PAL,   		"Philips FR1216 MK2" },
	{ TUNER_PHILIPS_SECAM, 		"Philips FR1216MF MK2" },
	{ TUNER_PHILIPS_NTSC,  		"Philips FR1236 MK2" },
	{ TUNER_PHILIPS_PAL_I, 		"Philips FR1246 MK2" },
	
	{ TUNER_PHILIPS_PAL_DK,		"Philips FR1256 MK2" },
	{ TUNER_PHILIPS_PAL,   		"Philips FM1216" },
	{ TUNER_PHILIPS_SECAM, 		"Philips FM1216MF" },
	{ TUNER_PHILIPS_NTSC,  		"Philips FM1236" },
	{ TUNER_PHILIPS_PAL_I, 		"Philips FM1246" },
	{ TUNER_PHILIPS_PAL_DK,		"Philips FM1256" },
	{ TUNER_TEMIC_4036FY5_NTSC, 	"Temic 4036FY5" },
	{ TUNER_ABSENT,        		"Samsung TCPN9082D" },
	{ TUNER_ABSENT,        		"Samsung TCPM9092P" },
	{ TUNER_TEMIC_4006FH5_PAL, 	"Temic 4006FH5" },
	
	{ TUNER_ABSENT,        		"Samsung TCPN9085D" },
	{ TUNER_ABSENT,        		"Samsung TCPB9085P" },
	{ TUNER_ABSENT,        		"Samsung TCPL9091P" },
	{ TUNER_TEMIC_4039FR5_NTSC, 	"Temic 4039FR5" },
	{ TUNER_PHILIPS_FQ1216ME,   	"Philips FQ1216 ME" },
	{ TUNER_TEMIC_4066FY5_PAL_I, 	"Temic 4066FY5" },
	{ TUNER_PHILIPS_NTSC,        	"Philips TD1536" },
	{ TUNER_PHILIPS_NTSC,        	"Philips TD1536D" },
	{ TUNER_PHILIPS_NTSC,  		"Philips FMR1236" }, 
	{ TUNER_ABSENT,        		"Philips FI1256MP" },
	
	{ TUNER_ABSENT,        		"Samsung TCPQ9091P" },
	{ TUNER_TEMIC_4006FN5_MULTI_PAL, "Temic 4006FN5" },
	{ TUNER_TEMIC_4009FR5_PAL, 	"Temic 4009FR5" },
	{ TUNER_TEMIC_4046FM5,     	"Temic 4046FM5" },
	{ TUNER_TEMIC_4009FN5_MULTI_PAL_FM, "Temic 4009FN5" },
	{ TUNER_ABSENT,        		"Philips TD1536D FH 44"},
	{ TUNER_LG_NTSC_FM,    		"LG TP18NSR01F"},
	{ TUNER_LG_PAL_FM,     		"LG TP18PSB01D"},
	{ TUNER_LG_PAL,        		"LG TP18PSB11D"},
	{ TUNER_LG_PAL_I_FM,   		"LG TAPC-I001D"},
	
	{ TUNER_LG_PAL_I,      		"LG TAPC-I701D"},
	{ TUNER_ABSENT,       		"Temic 4042FI5"},
	{ TUNER_MICROTUNE_4049FM5, 	"Microtune 4049 FM5"},
	{ TUNER_ABSENT,        		"LG TPI8NSR11F"},
	{ TUNER_ABSENT,        		"Microtune 4049 FM5 Alt I2C"},
	{ TUNER_PHILIPS_FM1216ME_MK3, 	"Philips FQ1216ME MK3"},
	{ TUNER_ABSENT,        		"Philips FI1236 MK3"},
	{ TUNER_PHILIPS_FM1216ME_MK3, 	"Philips FM1216 ME MK3"},
	{ TUNER_PHILIPS_FM1236_MK3, 	"Philips FM1236 MK3"},
	{ TUNER_ABSENT,        		"Philips FM1216MP MK3"},
	
	{ TUNER_PHILIPS_FM1216ME_MK3, 	"LG S001D MK3"},
	{ TUNER_ABSENT,        		"LG M001D MK3"},
	{ TUNER_PHILIPS_FM1216ME_MK3, 	"LG S701D MK3"},
	{ TUNER_ABSENT,        		"LG M701D MK3"},
	{ TUNER_ABSENT,        		"Temic 4146FM5"},
	{ TUNER_ABSENT,        		"Temic 4136FY5"},
	{ TUNER_ABSENT,        		"Temic 4106FH5"},
	{ TUNER_ABSENT,        		"Philips FQ1216LMP MK3"},
	{ TUNER_LG_NTSC_TAPE,  		"LG TAPE H001F MK3"},
	{ TUNER_LG_NTSC_TAPE,  		"LG TAPE H701F MK3"},
	
	{ TUNER_ABSENT,        		"LG TALN H200T"},
	{ TUNER_ABSENT,        		"LG TALN H250T"},
	{ TUNER_ABSENT,        		"LG TALN M200T"},
	{ TUNER_ABSENT,        		"LG TALN Z200T"},
	{ TUNER_ABSENT,        		"LG TALN S200T"},
	{ TUNER_ABSENT,        		"Thompson DTT7595"},
	{ TUNER_ABSENT,        		"Thompson DTT7592"},
	{ TUNER_ABSENT,        		"Silicon TDA8275C1 8290"},
	{ TUNER_ABSENT,        		"Silicon TDA8275C1 8290 FM"},
	{ TUNER_ABSENT,        		"Thompson DTT757"},
	
	{ TUNER_PHILIPS_FQ1216LME_MK3, 	"Philips FQ1216LME MK3"},
	{ TUNER_LG_PAL_NEW_TAPC, 	"LG TAPC G701D"},
	{ TUNER_LG_NTSC_NEW_TAPC, 	"LG TAPC H791F"},
	{ TUNER_LG_PAL_NEW_TAPC, 	"TCL 2002MB 3"},
	{ TUNER_LG_PAL_NEW_TAPC, 	"TCL 2002MI 3"},
	{ TUNER_TCL_2002N,     		"TCL 2002N 6A"},
	{ TUNER_PHILIPS_FM1236_MK3, 	"Philips FQ1236 MK3"},
	{ TUNER_SAMSUNG_TCPN_2121P30A, 	"Samsung TCPN 2121P30A"},
	{ TUNER_ABSENT,        		"Samsung TCPE 4121P30A"},
	{ TUNER_PHILIPS_FM1216ME_MK3, 	"TCL MFPE05 2"},
	
	{ TUNER_ABSENT,        		"LG TALN H202T"},
	{ TUNER_PHILIPS_FQ1216AME_MK4, 	"Philips FQ1216AME MK4"},
	{ TUNER_PHILIPS_FQ1236A_MK4, 	"Philips FQ1236A MK4"},
	{ TUNER_ABSENT,       		"Philips FQ1286A MK4"},
	{ TUNER_ABSENT,        		"Philips FQ1216ME MK5"},
	{ TUNER_ABSENT,        		"Philips FQ1236 MK5"},
	{ TUNER_SAMSUNG_TCPG_6121P30A, 	"Samsung TCPG 6121P30A"},
	{ TUNER_TCL_2002MB,    		"TCL 2002MB_3H"},
	{ TUNER_ABSENT,        		"TCL 2002MI_3H"},
	{ TUNER_TCL_2002N,     		"TCL 2002N 5H"},
	
	{ TUNER_PHILIPS_FMD1216ME_MK3, 	"Philips FMD1216ME"},
	{ TUNER_TEA5767,       		"Philips TEA5768HL FM Radio"},
	{ TUNER_ABSENT,        		"Panasonic ENV57H12D5"},
	{ TUNER_PHILIPS_FM1236_MK3, 	"TCL MFNM05-4"},
	{ TUNER_PHILIPS_FM1236_MK3,	"TCL MNM05-4"},
	{ TUNER_PHILIPS_FM1216ME_MK3, 	"TCL MPE05-2"},
	{ TUNER_ABSENT,        		"TCL MQNM05-4"},
	{ TUNER_ABSENT,        		"LG TAPC-W701D"},
	{ TUNER_ABSENT,        		"TCL 9886P-WM"},
	{ TUNER_ABSENT,        		"TCL 1676NM-WM"},
	
	{ TUNER_ABSENT,        		"Thompson DTT75105"},
	{ TUNER_ABSENT,        		"Conexant_CX24109"},
	{ TUNER_TCL_2002N,     		"TCL M2523_5N_E"},
	{ TUNER_TCL_2002MB,    		"TCL M2523_3DB_E"},
	{ TUNER_ABSENT,        		"Philips 8275A"},
	{ TUNER_ABSENT,        		"Microtune MT2060"},
	{ TUNER_PHILIPS_FM1236_MK3, 	"Philips FM1236 MK5"},
	{ TUNER_PHILIPS_FM1216ME_MK3, 	"Philips FM1216ME MK5"},
	{ TUNER_ABSENT,        		"TCL M2523_3DI_E"},
	{ TUNER_ABSENT,        		"Samsung THPD5222FG30A"},
	
	{ TUNER_XC2028,        		"Xceive XC3028"},
	{ TUNER_PHILIPS_FQ1216LME_MK3,	"Philips FQ1216LME MK5"},
	{ TUNER_ABSENT,        		"Philips FQD1216LME"},
	{ TUNER_ABSENT,        		"Conexant CX24118A"},
	{ TUNER_ABSENT,        		"TCL DMF11WIP"},
	{ TUNER_ABSENT,        		"TCL MFNM05_4H_E"},
	{ TUNER_ABSENT,        		"TCL MNM05_4H_E"},
	{ TUNER_ABSENT,        		"TCL MPE05_2H_E"},
	{ TUNER_ABSENT,        		"TCL MQNM05_4_U"},
	{ TUNER_ABSENT,        		"TCL M2523_5NH_E"},
	
	{ TUNER_ABSENT,        		"TCL M2523_3DBH_E"},
	{ TUNER_ABSENT,        		"TCL M2523_3DIH_E"},
	{ TUNER_ABSENT,        		"TCL MFPE05_2_U"},
	{ TUNER_PHILIPS_FMD1216MEX_MK3,	"Philips FMD1216MEX"},
	{ TUNER_ABSENT,        		"Philips FRH2036B"},
	{ TUNER_ABSENT,        		"Panasonic ENGF75_01GF"},
	{ TUNER_ABSENT,        		"MaxLinear MXL5005"},
	{ TUNER_ABSENT,        		"MaxLinear MXL5003"},
	{ TUNER_ABSENT,        		"Xceive XC2028"},
	{ TUNER_ABSENT,        		"Microtune MT2131"},
	
	{ TUNER_ABSENT,        		"Philips 8275A_8295"},
	{ TUNER_ABSENT,        		"TCL MF02GIP_5N_E"},
	{ TUNER_ABSENT,        		"TCL MF02GIP_3DB_E"},
	{ TUNER_ABSENT,        		"TCL MF02GIP_3DI_E"},
	{ TUNER_ABSENT,        		"Microtune MT2266"},
	{ TUNER_ABSENT,        		"TCL MF10WPP_4N_E"},
	{ TUNER_ABSENT,        		"LG TAPQ_H702F"},
	{ TUNER_ABSENT,        		"TCL M09WPP_4N_E"},
	{ TUNER_ABSENT,        		"MaxLinear MXL5005_v2"},
	{ TUNER_PHILIPS_TDA8290, 	"Philips 18271_8295"},
	
	{ TUNER_XC5000,                 "Xceive XC5000"},
	{ TUNER_ABSENT,                 "Xceive XC3028L"},
	{ TUNER_ABSENT,                 "NXP 18271C2_716x"},
	{ TUNER_ABSENT,                 "Xceive XC4000"},
	{ TUNER_ABSENT,                 "Dibcom 7070"},
	{ TUNER_PHILIPS_TDA8290,        "NXP 18271C2"},
	{ TUNER_ABSENT,                 "Siano SMS1010"},
	{ TUNER_ABSENT,                 "Siano SMS1150"},
	{ TUNER_ABSENT,                 "MaxLinear 5007"},
	{ TUNER_ABSENT,                 "TCL M09WPP_2P_E"},
	
	{ TUNER_ABSENT,                 "Siano SMS1180"},
	{ TUNER_ABSENT,                 "Maxim_MAX2165"},
	{ TUNER_ABSENT,                 "Siano SMS1140"},
	{ TUNER_ABSENT,                 "Siano SMS1150 B1"},
	{ TUNER_ABSENT,                 "MaxLinear 111"},
	{ TUNER_ABSENT,                 "Dibcom 7770"},
	{ TUNER_ABSENT,                 "Siano SMS1180VNS"},
	{ TUNER_ABSENT,                 "Siano SMS1184"},
	{ TUNER_PHILIPS_FQ1236_MK5,	"TCL M30WTP-4N-E"},
	{ TUNER_ABSENT,                 "TCL_M11WPP_2PN_E"},
	
	{ TUNER_ABSENT,                 "MaxLinear 301"},
	{ TUNER_ABSENT,                 "Mirics MSi001"},
	{ TUNER_ABSENT,                 "MaxLinear MxL241SF"},
	{ TUNER_XC5000C,                "Xceive XC5000C"},
	{ TUNER_ABSENT,                 "Montage M68TS2020"},
	{ TUNER_ABSENT,                 "Siano SMS1530"},
	{ TUNER_ABSENT,                 "Dibcom 7090"},
	{ TUNER_ABSENT,                 "Xceive XC5200C"},
	{ TUNER_ABSENT,                 "NXP 18273"},
	{ TUNER_ABSENT,                 "Montage M88TS2022"},
	
	{ TUNER_ABSENT,                 "NXP 18272M"},
	{ TUNER_ABSENT,                 "NXP 18272S"},
};

static struct HAUPPAUGE_AUDIOIC
{
	u32   id;
	char *name;
}
audioIC[] =
{
	
	{ V4L2_IDENT_NONE,      "None"      },
	{ V4L2_IDENT_UNKNOWN,   "TEA6300"   },
	{ V4L2_IDENT_UNKNOWN,   "TEA6320"   },
	{ V4L2_IDENT_UNKNOWN,   "TDA9850"   },
	{ V4L2_IDENT_MSPX4XX,   "MSP3400C"  },
	
	{ V4L2_IDENT_MSPX4XX,   "MSP3410D"  },
	{ V4L2_IDENT_MSPX4XX,   "MSP3415"   },
	{ V4L2_IDENT_MSPX4XX,   "MSP3430"   },
	{ V4L2_IDENT_MSPX4XX,   "MSP3438"   },
	{ V4L2_IDENT_UNKNOWN,   "CS5331"    },
	
	{ V4L2_IDENT_MSPX4XX,   "MSP3435"   },
	{ V4L2_IDENT_MSPX4XX,   "MSP3440"   },
	{ V4L2_IDENT_MSPX4XX,   "MSP3445"   },
	{ V4L2_IDENT_MSPX4XX,   "MSP3411"   },
	{ V4L2_IDENT_MSPX4XX,   "MSP3416"   },
	
	{ V4L2_IDENT_MSPX4XX,   "MSP3425"   },
	{ V4L2_IDENT_MSPX4XX,   "MSP3451"   },
	{ V4L2_IDENT_MSPX4XX,   "MSP3418"   },
	{ V4L2_IDENT_UNKNOWN,   "Type 0x12" },
	{ V4L2_IDENT_UNKNOWN,   "OKI7716"   },
	
	{ V4L2_IDENT_MSPX4XX,   "MSP4410"   },
	{ V4L2_IDENT_MSPX4XX,   "MSP4420"   },
	{ V4L2_IDENT_MSPX4XX,   "MSP4440"   },
	{ V4L2_IDENT_MSPX4XX,   "MSP4450"   },
	{ V4L2_IDENT_MSPX4XX,   "MSP4408"   },
	
	{ V4L2_IDENT_MSPX4XX,   "MSP4418"   },
	{ V4L2_IDENT_MSPX4XX,   "MSP4428"   },
	{ V4L2_IDENT_MSPX4XX,   "MSP4448"   },
	{ V4L2_IDENT_MSPX4XX,   "MSP4458"   },
	{ V4L2_IDENT_MSPX4XX,   "Type 0x1d" },
	
	{ V4L2_IDENT_AMBIGUOUS, "CX880"     },
	{ V4L2_IDENT_AMBIGUOUS, "CX881"     },
	{ V4L2_IDENT_AMBIGUOUS, "CX883"     },
	{ V4L2_IDENT_AMBIGUOUS, "CX882"     },
	{ V4L2_IDENT_AMBIGUOUS, "CX25840"   },
	
	{ V4L2_IDENT_AMBIGUOUS, "CX25841"   },
	{ V4L2_IDENT_AMBIGUOUS, "CX25842"   },
	{ V4L2_IDENT_AMBIGUOUS, "CX25843"   },
	{ V4L2_IDENT_AMBIGUOUS, "CX23418"   },
	{ V4L2_IDENT_AMBIGUOUS, "CX23885"   },
	
	{ V4L2_IDENT_AMBIGUOUS, "CX23888"   },
	{ V4L2_IDENT_AMBIGUOUS, "SAA7131"   },
	{ V4L2_IDENT_AMBIGUOUS, "CX23887"   },
	{ V4L2_IDENT_AMBIGUOUS, "SAA7164"   },
	{ V4L2_IDENT_AMBIGUOUS, "AU8522"    },
};

static const char *decoderIC[] = {
	
	"None", "BT815", "BT817", "BT819", "BT815A",
	
	"BT817A", "BT819A", "BT827", "BT829", "BT848",
	
	"BT848A", "BT849A", "BT829A", "BT827A", "BT878",
	
	"BT879", "BT880", "VPX3226E", "SAA7114", "SAA7115",
	
	"CX880", "CX881", "CX883", "SAA7111", "SAA7113",
	
	"CX882", "TVP5150A", "CX25840", "CX25841", "CX25842",
	
	"CX25843", "CX23418", "NEC61153", "CX23885", "CX23888",
	
	"SAA7131", "CX25837", "CX23887", "CX23885A", "CX23887A",
	
	"SAA7164", "CX23885B", "AU8522"
};

static int hasRadioTuner(int tunerType)
{
	switch (tunerType) {
	case 18: 
	case 23: 
	case 38: 
	case 16: 
	case 19: 
	case 21: 
	case 24: 
	case 17: 
	case 22: 
	case 20: 
	case 25: 
	case 33: 
	case 42: 
	case 52: 
	case 54: 
	case 44: 
	case 31: 
	case 30: 
	case 46: 
	case 47: 
	case 49: 
	case 60: 
	case 57: 
	case 59: 
	case 58: 
	case 68: 
	case 61: 
	case 78: 
	case 89: 
	case 92: 
	case 105:
		return 1;
	}
	return 0;
}

void tveeprom_hauppauge_analog(struct i2c_client *c, struct tveeprom *tvee,
				unsigned char *eeprom_data)
{

	int i, j, len, done, beenhere, tag, start;

	int tuner1 = 0, t_format1 = 0, audioic = -1;
	char *t_name1 = NULL;
	const char *t_fmt_name1[8] = { " none", "", "", "", "", "", "", "" };

	int tuner2 = 0, t_format2 = 0;
	char *t_name2 = NULL;
	const char *t_fmt_name2[8] = { " none", "", "", "", "", "", "", "" };

	memset(tvee, 0, sizeof(*tvee));
	tvee->tuner_type = TUNER_ABSENT;
	tvee->tuner2_type = TUNER_ABSENT;

	done = len = beenhere = 0;

	
	if (eeprom_data[0] == 0x1a &&
	    eeprom_data[1] == 0xeb &&
	    eeprom_data[2] == 0x67 &&
	    eeprom_data[3] == 0x95)
		start = 0xa0; 
	else if ((eeprom_data[0] & 0xe1) == 0x01 &&
		 eeprom_data[1] == 0x00 &&
		 eeprom_data[2] == 0x00 &&
		 eeprom_data[8] == 0x84)
		start = 8; 
	else if (eeprom_data[1] == 0x70 &&
		 eeprom_data[2] == 0x00 &&
		 eeprom_data[4] == 0x74 &&
		 eeprom_data[8] == 0x84)
		start = 8; 
	else
		start = 0;

	for (i = start; !done && i < 256; i += len) {
		if (eeprom_data[i] == 0x84) {
			len = eeprom_data[i + 1] + (eeprom_data[i + 2] << 8);
			i += 3;
		} else if ((eeprom_data[i] & 0xf0) == 0x70) {
			if (eeprom_data[i] & 0x08) {
				
				done = 1;
				break;
			}
			len = eeprom_data[i] & 0x07;
			++i;
		} else {
			tveeprom_warn("Encountered bad packet header [%02x]. "
				"Corrupt or not a Hauppauge eeprom.\n",
				eeprom_data[i]);
			return;
		}

		if (debug) {
			tveeprom_info("Tag [%02x] + %d bytes:",
					eeprom_data[i], len - 1);
			for (j = 1; j < len; j++)
				printk(KERN_CONT " %02x", eeprom_data[i + j]);
			printk(KERN_CONT "\n");
		}

		
		tag = eeprom_data[i];
		switch (tag) {
		case 0x00:
			
			tuner1 = eeprom_data[i+6];
			t_format1 = eeprom_data[i+5];
			tvee->has_radio = eeprom_data[i+len-1];
			tvee->has_ir = 0;
			tvee->model =
				eeprom_data[i+8] +
				(eeprom_data[i+9] << 8);
			tvee->revision = eeprom_data[i+10] +
				(eeprom_data[i+11] << 8) +
				(eeprom_data[i+12] << 16);
			break;

		case 0x01:
			
			tvee->serial_number =
				eeprom_data[i+6] +
				(eeprom_data[i+7] << 8) +
				(eeprom_data[i+8] << 16);
			break;

		case 0x02:
			audioic = eeprom_data[i+2] & 0x7f;
			if (audioic < ARRAY_SIZE(audioIC))
				tvee->audio_processor = audioIC[audioic].id;
			else
				tvee->audio_processor = V4L2_IDENT_UNKNOWN;
			break;

		

		case 0x04:
			
			tvee->serial_number =
				eeprom_data[i+5] +
				(eeprom_data[i+6] << 8) +
				(eeprom_data[i+7] << 16);

			if ((eeprom_data[i + 8] & 0xf0) &&
					(tvee->serial_number < 0xffffff)) {
				tvee->MAC_address[0] = 0x00;
				tvee->MAC_address[1] = 0x0D;
				tvee->MAC_address[2] = 0xFE;
				tvee->MAC_address[3] = eeprom_data[i + 7];
				tvee->MAC_address[4] = eeprom_data[i + 6];
				tvee->MAC_address[5] = eeprom_data[i + 5];
				tvee->has_MAC_address = 1;
			}
			break;

		case 0x05:
			audioic = eeprom_data[i+1] & 0x7f;
			if (audioic < ARRAY_SIZE(audioIC))
				tvee->audio_processor = audioIC[audioic].id;
			else
				tvee->audio_processor = V4L2_IDENT_UNKNOWN;

			break;

		case 0x06:
			
			tvee->model =
				eeprom_data[i + 1] +
				(eeprom_data[i + 2] << 8) +
				(eeprom_data[i + 3] << 16) +
				(eeprom_data[i + 4] << 24);
			tvee->revision =
				eeprom_data[i + 5] +
				(eeprom_data[i + 6] << 8) +
				(eeprom_data[i + 7] << 16);
			break;

		case 0x07:
			break;

		

		case 0x09:
			
			tvee->decoder_processor = eeprom_data[i + 1];
			break;

		case 0x0a:
			
			if (beenhere == 0) {
				tuner1 = eeprom_data[i + 2];
				t_format1 = eeprom_data[i + 1];
				beenhere = 1;
			} else {
				
				tuner2 = eeprom_data[i + 2];
				t_format2 = eeprom_data[i + 1];
				
				if (t_format2 == 0)
					tvee->has_radio = 1; 
			}
			break;

		case 0x0b:
			break;

		
		

		case 0x0e:
			
			tvee->has_radio = eeprom_data[i+1];
			break;

		case 0x0f:
			
			tvee->has_ir = 1 | (eeprom_data[i+1] << 1);
			break;

		
		
		

		default:
			tveeprom_dbg("Not sure what to do with tag [%02x]\n",
					tag);
			
		}
	}

	if (!done) {
		tveeprom_warn("Ran out of data!\n");
		return;
	}

	if (tvee->revision != 0) {
		tvee->rev_str[0] = 32 + ((tvee->revision >> 18) & 0x3f);
		tvee->rev_str[1] = 32 + ((tvee->revision >> 12) & 0x3f);
		tvee->rev_str[2] = 32 + ((tvee->revision >>  6) & 0x3f);
		tvee->rev_str[3] = 32 + (tvee->revision & 0x3f);
		tvee->rev_str[4] = 0;
	}

	if (hasRadioTuner(tuner1) && !tvee->has_radio) {
		tveeprom_info("The eeprom says no radio is present, but the tuner type\n");
		tveeprom_info("indicates otherwise. I will assume that radio is present.\n");
		tvee->has_radio = 1;
	}

	if (tuner1 < ARRAY_SIZE(hauppauge_tuner)) {
		tvee->tuner_type = hauppauge_tuner[tuner1].id;
		t_name1 = hauppauge_tuner[tuner1].name;
	} else {
		t_name1 = "unknown";
	}

	if (tuner2 < ARRAY_SIZE(hauppauge_tuner)) {
		tvee->tuner2_type = hauppauge_tuner[tuner2].id;
		t_name2 = hauppauge_tuner[tuner2].name;
	} else {
		t_name2 = "unknown";
	}

	tvee->tuner_hauppauge_model = tuner1;
	tvee->tuner2_hauppauge_model = tuner2;
	tvee->tuner_formats = 0;
	tvee->tuner2_formats = 0;
	for (i = j = 0; i < 8; i++) {
		if (t_format1 & (1 << i)) {
			tvee->tuner_formats |= hauppauge_tuner_fmt[i].id;
			t_fmt_name1[j++] = hauppauge_tuner_fmt[i].name;
		}
	}
	for (i = j = 0; i < 8; i++) {
		if (t_format2 & (1 << i)) {
			tvee->tuner2_formats |= hauppauge_tuner_fmt[i].id;
			t_fmt_name2[j++] = hauppauge_tuner_fmt[i].name;
		}
	}

	tveeprom_info("Hauppauge model %d, rev %s, serial# %d\n",
		tvee->model, tvee->rev_str, tvee->serial_number);
	if (tvee->has_MAC_address == 1)
		tveeprom_info("MAC address is %pM\n", tvee->MAC_address);
	tveeprom_info("tuner model is %s (idx %d, type %d)\n",
		t_name1, tuner1, tvee->tuner_type);
	tveeprom_info("TV standards%s%s%s%s%s%s%s%s (eeprom 0x%02x)\n",
		t_fmt_name1[0], t_fmt_name1[1], t_fmt_name1[2],
		t_fmt_name1[3],	t_fmt_name1[4], t_fmt_name1[5],
		t_fmt_name1[6], t_fmt_name1[7],	t_format1);
	if (tuner2)
		tveeprom_info("second tuner model is %s (idx %d, type %d)\n",
					t_name2, tuner2, tvee->tuner2_type);
	if (t_format2)
		tveeprom_info("TV standards%s%s%s%s%s%s%s%s (eeprom 0x%02x)\n",
			t_fmt_name2[0], t_fmt_name2[1], t_fmt_name2[2],
			t_fmt_name2[3],	t_fmt_name2[4], t_fmt_name2[5],
			t_fmt_name2[6], t_fmt_name2[7], t_format2);
	if (audioic < 0) {
		tveeprom_info("audio processor is unknown (no idx)\n");
		tvee->audio_processor = V4L2_IDENT_UNKNOWN;
	} else {
		if (audioic < ARRAY_SIZE(audioIC))
			tveeprom_info("audio processor is %s (idx %d)\n",
					audioIC[audioic].name, audioic);
		else
			tveeprom_info("audio processor is unknown (idx %d)\n",
								audioic);
	}
	if (tvee->decoder_processor)
		tveeprom_info("decoder processor is %s (idx %d)\n",
			STRM(decoderIC, tvee->decoder_processor),
			tvee->decoder_processor);
	if (tvee->has_ir)
		tveeprom_info("has %sradio, has %sIR receiver, has %sIR transmitter\n",
				tvee->has_radio ? "" : "no ",
				(tvee->has_ir & 2) ? "" : "no ",
				(tvee->has_ir & 4) ? "" : "no ");
	else
		tveeprom_info("has %sradio\n",
				tvee->has_radio ? "" : "no ");
}
EXPORT_SYMBOL(tveeprom_hauppauge_analog);


int tveeprom_read(struct i2c_client *c, unsigned char *eedata, int len)
{
	unsigned char buf;
	int err;

	buf = 0;
	err = i2c_master_send(c, &buf, 1);
	if (err != 1) {
		tveeprom_info("Huh, no eeprom present (err=%d)?\n", err);
		return -1;
	}
	err = i2c_master_recv(c, eedata, len);
	if (err != len) {
		tveeprom_warn("i2c eeprom read error (err=%d)\n", err);
		return -1;
	}
	if (debug) {
		int i;

		tveeprom_info("full 256-byte eeprom dump:\n");
		for (i = 0; i < len; i++) {
			if (0 == (i % 16))
				tveeprom_info("%02x:", i);
			printk(KERN_CONT " %02x", eedata[i]);
			if (15 == (i % 16))
				printk(KERN_CONT "\n");
		}
	}
	return 0;
}
EXPORT_SYMBOL(tveeprom_read);

