/*

    bttv-cards.c

    this file has configuration informations - card-specific stuff
    like the big tvcards array for the most part

    Copyright (C) 1996,97,98 Ralph  Metzler (rjkm@thp.uni-koeln.de)
			   & Marcus Metzler (mocm@thp.uni-koeln.de)
    (c) 1999-2001 Gerd Knorr <kraxel@goldbach.in-berlin.de>

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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kmod.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <net/checksum.h>

#include <asm/unaligned.h>
#include <asm/io.h>

#include "bttvp.h"
#include <media/v4l2-common.h>
#include <media/tvaudio.h>
#include "bttv-audio-hook.h"

static void boot_msp34xx(struct bttv *btv, int pin);
static void hauppauge_eeprom(struct bttv *btv);
static void avermedia_eeprom(struct bttv *btv);
static void osprey_eeprom(struct bttv *btv, const u8 ee[256]);
static void modtec_eeprom(struct bttv *btv);
static void init_PXC200(struct bttv *btv);
static void init_RTV24(struct bttv *btv);

static void rv605_muxsel(struct bttv *btv, unsigned int input);
static void eagle_muxsel(struct bttv *btv, unsigned int input);
static void xguard_muxsel(struct bttv *btv, unsigned int input);
static void ivc120_muxsel(struct bttv *btv, unsigned int input);
static void gvc1100_muxsel(struct bttv *btv, unsigned int input);

static void PXC200_muxsel(struct bttv *btv, unsigned int input);

static void picolo_tetra_muxsel(struct bttv *btv, unsigned int input);
static void picolo_tetra_init(struct bttv *btv);

static void tibetCS16_muxsel(struct bttv *btv, unsigned int input);
static void tibetCS16_init(struct bttv *btv);

static void kodicom4400r_muxsel(struct bttv *btv, unsigned int input);
static void kodicom4400r_init(struct bttv *btv);

static void sigmaSLC_muxsel(struct bttv *btv, unsigned int input);
static void sigmaSQ_muxsel(struct bttv *btv, unsigned int input);

static void geovision_muxsel(struct bttv *btv, unsigned int input);

static void phytec_muxsel(struct bttv *btv, unsigned int input);

static void gv800s_muxsel(struct bttv *btv, unsigned int input);
static void gv800s_init(struct bttv *btv);

static void td3116_muxsel(struct bttv *btv, unsigned int input);

static int terratec_active_radio_upgrade(struct bttv *btv);
static int tea5757_read(struct bttv *btv);
static int tea5757_write(struct bttv *btv, int value);
static void identify_by_eeprom(struct bttv *btv,
			       unsigned char eeprom_data[256]);
static int __devinit pvr_boot(struct bttv *btv);

static unsigned int triton1;
static unsigned int vsfx;
static unsigned int latency = UNSET;
int no_overlay=-1;

static unsigned int card[BTTV_MAX]   = { [ 0 ... (BTTV_MAX-1) ] = UNSET };
static unsigned int pll[BTTV_MAX]    = { [ 0 ... (BTTV_MAX-1) ] = UNSET };
static unsigned int tuner[BTTV_MAX]  = { [ 0 ... (BTTV_MAX-1) ] = UNSET };
static unsigned int svhs[BTTV_MAX]   = { [ 0 ... (BTTV_MAX-1) ] = UNSET };
static unsigned int remote[BTTV_MAX] = { [ 0 ... (BTTV_MAX-1) ] = UNSET };
static unsigned int audiodev[BTTV_MAX];
static unsigned int saa6588[BTTV_MAX];
static struct bttv  *master[BTTV_MAX] = { [ 0 ... (BTTV_MAX-1) ] = NULL };
static unsigned int autoload = UNSET;
static unsigned int gpiomask = UNSET;
static unsigned int audioall = UNSET;
static unsigned int audiomux[5] = { [ 0 ... 4 ] = UNSET };

module_param(triton1,    int, 0444);
module_param(vsfx,       int, 0444);
module_param(no_overlay, int, 0444);
module_param(latency,    int, 0444);
module_param(gpiomask,   int, 0444);
module_param(audioall,   int, 0444);
module_param(autoload,   int, 0444);

module_param_array(card,     int, NULL, 0444);
module_param_array(pll,      int, NULL, 0444);
module_param_array(tuner,    int, NULL, 0444);
module_param_array(svhs,     int, NULL, 0444);
module_param_array(remote,   int, NULL, 0444);
module_param_array(audiodev, int, NULL, 0444);
module_param_array(audiomux, int, NULL, 0444);

MODULE_PARM_DESC(triton1,"set ETBF pci config bit "
		 "[enable bug compatibility for triton1 + others]");
MODULE_PARM_DESC(vsfx,"set VSFX pci config bit "
		 "[yet another chipset flaw workaround]");
MODULE_PARM_DESC(latency,"pci latency timer");
MODULE_PARM_DESC(card,"specify TV/grabber card model, see CARDLIST file for a list");
MODULE_PARM_DESC(pll,"specify installed crystal (0=none, 28=28 MHz, 35=35 MHz)");
MODULE_PARM_DESC(tuner,"specify installed tuner type");
MODULE_PARM_DESC(autoload, "obsolete option, please do not use anymore");
MODULE_PARM_DESC(audiodev, "specify audio device:\n"
		"\t\t-1 = no audio\n"
		"\t\t 0 = autodetect (default)\n"
		"\t\t 1 = msp3400\n"
		"\t\t 2 = tda7432\n"
		"\t\t 3 = tvaudio");
MODULE_PARM_DESC(saa6588, "if 1, then load the saa6588 RDS module, default (0) is to use the card definition.");
MODULE_PARM_DESC(no_overlay,"allow override overlay default (0 disables, 1 enables)"
		" [some VIA/SIS chipsets are known to have problem with overlay]");


static struct CARD {
	unsigned id;
	int cardnr;
	char *name;
} cards[] __devinitdata = {
	{ 0x13eb0070, BTTV_BOARD_HAUPPAUGE878,  "Hauppauge WinTV" },
	{ 0x39000070, BTTV_BOARD_HAUPPAUGE878,  "Hauppauge WinTV-D" },
	{ 0x45000070, BTTV_BOARD_HAUPPAUGEPVR,  "Hauppauge WinTV/PVR" },
	{ 0xff000070, BTTV_BOARD_OSPREY1x0,     "Osprey-100" },
	{ 0xff010070, BTTV_BOARD_OSPREY2x0_SVID,"Osprey-200" },
	{ 0xff020070, BTTV_BOARD_OSPREY500,     "Osprey-500" },
	{ 0xff030070, BTTV_BOARD_OSPREY2000,    "Osprey-2000" },
	{ 0xff040070, BTTV_BOARD_OSPREY540,     "Osprey-540" },
	{ 0xff070070, BTTV_BOARD_OSPREY440,     "Osprey-440" },

	{ 0x00011002, BTTV_BOARD_ATI_TVWONDER,  "ATI TV Wonder" },
	{ 0x00031002, BTTV_BOARD_ATI_TVWONDERVE,"ATI TV Wonder/VE" },

	{ 0x6606107d, BTTV_BOARD_WINFAST2000,   "Leadtek WinFast TV 2000" },
	{ 0x6607107d, BTTV_BOARD_WINFASTVC100,  "Leadtek WinFast VC 100" },
	{ 0x6609107d, BTTV_BOARD_WINFAST2000,   "Leadtek TV 2000 XP" },
	{ 0x263610b4, BTTV_BOARD_STB2,          "STB TV PCI FM, Gateway P/N 6000704" },
	{ 0x264510b4, BTTV_BOARD_STB2,          "STB TV PCI FM, Gateway P/N 6000704" },
	{ 0x402010fc, BTTV_BOARD_GVBCTV3PCI,    "I-O Data Co. GV-BCTV3/PCI" },
	{ 0x405010fc, BTTV_BOARD_GVBCTV4PCI,    "I-O Data Co. GV-BCTV4/PCI" },
	{ 0x407010fc, BTTV_BOARD_GVBCTV5PCI,    "I-O Data Co. GV-BCTV5/PCI" },
	{ 0xd01810fc, BTTV_BOARD_GVBCTV5PCI,    "I-O Data Co. GV-BCTV5/PCI" },

	{ 0x001211bd, BTTV_BOARD_PINNACLE,      "Pinnacle PCTV" },
	
	{ 0x1200bd11, BTTV_BOARD_PINNACLE,      "Pinnacle PCTV [bswap]" },
	{ 0xff00bd11, BTTV_BOARD_PINNACLE,      "Pinnacle PCTV [bswap]" },
	
	{ 0xff1211bd, BTTV_BOARD_PINNACLE,      "Pinnacle PCTV" },

	{ 0x3000121a, BTTV_BOARD_VOODOOTV_200,  "3Dfx VoodooTV 200" },
	{ 0x263710b4, BTTV_BOARD_VOODOOTV_FM,   "3Dfx VoodooTV FM" },
	{ 0x3060121a, BTTV_BOARD_STB2,	  "3Dfx VoodooTV 100/ STB OEM" },

	{ 0x3000144f, BTTV_BOARD_MAGICTVIEW063, "(Askey Magic/others) TView99 CPH06x" },
	{ 0xa005144f, BTTV_BOARD_MAGICTVIEW063, "CPH06X TView99-Card" },
	{ 0x3002144f, BTTV_BOARD_MAGICTVIEW061, "(Askey Magic/others) TView99 CPH05x" },
	{ 0x3005144f, BTTV_BOARD_MAGICTVIEW061, "(Askey Magic/others) TView99 CPH061/06L (T1/LC)" },
	{ 0x5000144f, BTTV_BOARD_MAGICTVIEW061, "Askey CPH050" },
	{ 0x300014ff, BTTV_BOARD_MAGICTVIEW061, "TView 99 (CPH061)" },
	{ 0x300214ff, BTTV_BOARD_PHOEBE_TVMAS,  "Phoebe TV Master (CPH060)" },

	{ 0x00011461, BTTV_BOARD_AVPHONE98,     "AVerMedia TVPhone98" },
	{ 0x00021461, BTTV_BOARD_AVERMEDIA98,   "AVermedia TVCapture 98" },
	{ 0x00031461, BTTV_BOARD_AVPHONE98,     "AVerMedia TVPhone98" },
	{ 0x00041461, BTTV_BOARD_AVERMEDIA98,   "AVerMedia TVCapture 98" },
	{ 0x03001461, BTTV_BOARD_AVERMEDIA98,   "VDOMATE TV TUNER CARD" },

	{ 0x1117153b, BTTV_BOARD_TERRATVALUE,   "Terratec TValue (Philips PAL B/G)" },
	{ 0x1118153b, BTTV_BOARD_TERRATVALUE,   "Terratec TValue (Temic PAL B/G)" },
	{ 0x1119153b, BTTV_BOARD_TERRATVALUE,   "Terratec TValue (Philips PAL I)" },
	{ 0x111a153b, BTTV_BOARD_TERRATVALUE,   "Terratec TValue (Temic PAL I)" },

	{ 0x1123153b, BTTV_BOARD_TERRATVRADIO,  "Terratec TV Radio+" },
	{ 0x1127153b, BTTV_BOARD_TERRATV,       "Terratec TV+ (V1.05)"    },
	{ 0x1134153b, BTTV_BOARD_TERRATVALUE,   "Terratec TValue (LR102)" },
	{ 0x1135153b, BTTV_BOARD_TERRATVALUER,  "Terratec TValue Radio" }, 
	{ 0x5018153b, BTTV_BOARD_TERRATVALUE,   "Terratec TValue" },       
	{ 0xff3b153b, BTTV_BOARD_TERRATVALUER,  "Terratec TValue Radio" }, 

	{ 0x400015b0, BTTV_BOARD_ZOLTRIX_GENIE, "Zoltrix Genie TV" },
	{ 0x400a15b0, BTTV_BOARD_ZOLTRIX_GENIE, "Zoltrix Genie TV" },
	{ 0x400d15b0, BTTV_BOARD_ZOLTRIX_GENIE, "Zoltrix Genie TV / Radio" },
	{ 0x401015b0, BTTV_BOARD_ZOLTRIX_GENIE, "Zoltrix Genie TV / Radio" },
	{ 0x401615b0, BTTV_BOARD_ZOLTRIX_GENIE, "Zoltrix Genie TV / Radio" },

	{ 0x1430aa00, BTTV_BOARD_PV143,         "Provideo PV143A" },
	{ 0x1431aa00, BTTV_BOARD_PV143,         "Provideo PV143B" },
	{ 0x1432aa00, BTTV_BOARD_PV143,         "Provideo PV143C" },
	{ 0x1433aa00, BTTV_BOARD_PV143,         "Provideo PV143D" },
	{ 0x1433aa03, BTTV_BOARD_PV143,         "Security Eyes" },

	{ 0x1460aa00, BTTV_BOARD_PV150,         "Provideo PV150A-1" },
	{ 0x1461aa01, BTTV_BOARD_PV150,         "Provideo PV150A-2" },
	{ 0x1462aa02, BTTV_BOARD_PV150,         "Provideo PV150A-3" },
	{ 0x1463aa03, BTTV_BOARD_PV150,         "Provideo PV150A-4" },

	{ 0x1464aa04, BTTV_BOARD_PV150,         "Provideo PV150B-1" },
	{ 0x1465aa05, BTTV_BOARD_PV150,         "Provideo PV150B-2" },
	{ 0x1466aa06, BTTV_BOARD_PV150,         "Provideo PV150B-3" },
	{ 0x1467aa07, BTTV_BOARD_PV150,         "Provideo PV150B-4" },

	{ 0xa132ff00, BTTV_BOARD_IVC100,        "IVC-100"  },
	{ 0xa1550000, BTTV_BOARD_IVC200,        "IVC-200"  },
	{ 0xa1550001, BTTV_BOARD_IVC200,        "IVC-200"  },
	{ 0xa1550002, BTTV_BOARD_IVC200,        "IVC-200"  },
	{ 0xa1550003, BTTV_BOARD_IVC200,        "IVC-200"  },
	{ 0xa1550100, BTTV_BOARD_IVC200,        "IVC-200G" },
	{ 0xa1550101, BTTV_BOARD_IVC200,        "IVC-200G" },
	{ 0xa1550102, BTTV_BOARD_IVC200,        "IVC-200G" },
	{ 0xa1550103, BTTV_BOARD_IVC200,        "IVC-200G" },
	{ 0xa1550800, BTTV_BOARD_IVC200,        "IVC-200"  },
	{ 0xa1550801, BTTV_BOARD_IVC200,        "IVC-200"  },
	{ 0xa1550802, BTTV_BOARD_IVC200,        "IVC-200"  },
	{ 0xa1550803, BTTV_BOARD_IVC200,        "IVC-200"  },
	{ 0xa182ff00, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff01, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff02, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff03, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff04, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff05, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff06, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff07, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff08, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff09, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff0a, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff0b, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff0c, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff0d, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff0e, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff0f, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xf0500000, BTTV_BOARD_IVCE8784,      "IVCE-8784" },
	{ 0xf0500001, BTTV_BOARD_IVCE8784,      "IVCE-8784" },
	{ 0xf0500002, BTTV_BOARD_IVCE8784,      "IVCE-8784" },
	{ 0xf0500003, BTTV_BOARD_IVCE8784,      "IVCE-8784" },

	{ 0x41424344, BTTV_BOARD_GRANDTEC,      "GrandTec Multi Capture" },
	{ 0x01020304, BTTV_BOARD_XGUARD,        "Grandtec Grand X-Guard" },

	{ 0x18501851, BTTV_BOARD_CHRONOS_VS2,   "FlyVideo 98 (LR50)/ Chronos Video Shuttle II" },
	{ 0xa0501851, BTTV_BOARD_CHRONOS_VS2,   "FlyVideo 98 (LR50)/ Chronos Video Shuttle II" },
	{ 0x18511851, BTTV_BOARD_FLYVIDEO98EZ,  "FlyVideo 98EZ (LR51)/ CyberMail AV" },
	{ 0x18521852, BTTV_BOARD_TYPHOON_TVIEW, "FlyVideo 98FM (LR50)/ Typhoon TView TV/FM Tuner" },
	{ 0x41a0a051, BTTV_BOARD_FLYVIDEO_98FM, "Lifeview FlyVideo 98 LR50 Rev Q" },
	{ 0x18501f7f, BTTV_BOARD_FLYVIDEO_98,   "Lifeview Flyvideo 98" },

	{ 0x010115cb, BTTV_BOARD_GMV1,          "AG GMV1" },
	{ 0x010114c7, BTTV_BOARD_MODTEC_205,    "Modular Technology MM201/MM202/MM205/MM210/MM215 PCTV" },

	{ 0x10b42636, BTTV_BOARD_HAUPPAUGE878,  "STB ???" },
	{ 0x217d6606, BTTV_BOARD_WINFAST2000,   "Leadtek WinFast TV 2000" },
	{ 0xfff6f6ff, BTTV_BOARD_WINFAST2000,   "Leadtek WinFast TV 2000" },
	{ 0x03116000, BTTV_BOARD_SENSORAY311_611, "Sensoray 311" },
	{ 0x06116000, BTTV_BOARD_SENSORAY311_611, "Sensoray 611" },
	{ 0x00790e11, BTTV_BOARD_WINDVR,        "Canopus WinDVR PCI" },
	{ 0xa0fca1a0, BTTV_BOARD_ZOLTRIX,       "Face to Face Tvmax" },
	{ 0x82b2aa6a, BTTV_BOARD_SIMUS_GVC1100, "SIMUS GVC1100" },
	{ 0x146caa0c, BTTV_BOARD_PV951,         "ituner spectra8" },
	{ 0x200a1295, BTTV_BOARD_PXC200,        "ImageNation PXC200A" },

	{ 0x40111554, BTTV_BOARD_PV_BT878P_9B,  "Prolink Pixelview PV-BT" },
	{ 0x17de0a01, BTTV_BOARD_KWORLD,        "Mecer TV/FM/Video Tuner" },

	{ 0x01051805, BTTV_BOARD_PICOLO_TETRA_CHIP, "Picolo Tetra Chip #1" },
	{ 0x01061805, BTTV_BOARD_PICOLO_TETRA_CHIP, "Picolo Tetra Chip #2" },
	{ 0x01071805, BTTV_BOARD_PICOLO_TETRA_CHIP, "Picolo Tetra Chip #3" },
	{ 0x01081805, BTTV_BOARD_PICOLO_TETRA_CHIP, "Picolo Tetra Chip #4" },

	{ 0x15409511, BTTV_BOARD_ACORP_Y878F, "Acorp Y878F" },

	{ 0x53534149, BTTV_BOARD_SSAI_SECURITY, "SSAI Security Video Interface" },
	{ 0x5353414a, BTTV_BOARD_SSAI_ULTRASOUND, "SSAI Ultrasound Video Interface" },



	{ 0x109e036e, BTTV_BOARD_CONCEPTRONIC_CTVFMI2,	"Conceptronic CTVFMi v2"},

	
	{ 0x001c11bd, BTTV_BOARD_PINNACLESAT,   "Pinnacle PCTV Sat" },
	{ 0x01010071, BTTV_BOARD_NEBULA_DIGITV, "Nebula Electronics DigiTV" },
	{ 0x20007063, BTTV_BOARD_PC_HDTV,       "pcHDTV HD-2000 TV"},
	{ 0x002611bd, BTTV_BOARD_TWINHAN_DST,   "Pinnacle PCTV SAT CI" },
	{ 0x00011822, BTTV_BOARD_TWINHAN_DST,   "Twinhan VisionPlus DVB" },
	{ 0xfc00270f, BTTV_BOARD_TWINHAN_DST,   "ChainTech digitop DST-1000 DVB-S" },
	{ 0x07711461, BTTV_BOARD_AVDVBT_771,    "AVermedia AverTV DVB-T 771" },
	{ 0x07611461, BTTV_BOARD_AVDVBT_761,    "AverMedia AverTV DVB-T 761" },
	{ 0xdb1018ac, BTTV_BOARD_DVICO_DVBT_LITE,    "DViCO FusionHDTV DVB-T Lite" },
	{ 0xdb1118ac, BTTV_BOARD_DVICO_DVBT_LITE,    "Ultraview DVB-T Lite" },
	{ 0xd50018ac, BTTV_BOARD_DVICO_FUSIONHDTV_5_LITE,    "DViCO FusionHDTV 5 Lite" },
	{ 0x00261822, BTTV_BOARD_TWINHAN_DST,	"DNTV Live! Mini "},
	{ 0xd200dbc0, BTTV_BOARD_DVICO_FUSIONHDTV_2,	"DViCO FusionHDTV 2" },
	{ 0x763c008a, BTTV_BOARD_GEOVISION_GV600,	"GeoVision GV-600" },
	{ 0x18011000, BTTV_BOARD_ENLTV_FM_2,	"Encore ENL TV-FM-2" },
	{ 0x763d800a, BTTV_BOARD_GEOVISION_GV800S, "GeoVision GV-800(S) (master)" },
	{ 0x763d800b, BTTV_BOARD_GEOVISION_GV800S_SL,	"GeoVision GV-800(S) (slave)" },
	{ 0x763d800c, BTTV_BOARD_GEOVISION_GV800S_SL,	"GeoVision GV-800(S) (slave)" },
	{ 0x763d800d, BTTV_BOARD_GEOVISION_GV800S_SL,	"GeoVision GV-800(S) (slave)" },

	{ 0x15401830, BTTV_BOARD_PV183,         "Provideo PV183-1" },
	{ 0x15401831, BTTV_BOARD_PV183,         "Provideo PV183-2" },
	{ 0x15401832, BTTV_BOARD_PV183,         "Provideo PV183-3" },
	{ 0x15401833, BTTV_BOARD_PV183,         "Provideo PV183-4" },
	{ 0x15401834, BTTV_BOARD_PV183,         "Provideo PV183-5" },
	{ 0x15401835, BTTV_BOARD_PV183,         "Provideo PV183-6" },
	{ 0x15401836, BTTV_BOARD_PV183,         "Provideo PV183-7" },
	{ 0x15401837, BTTV_BOARD_PV183,         "Provideo PV183-8" },
	{ 0x3116f200, BTTV_BOARD_TVT_TD3116,	"Tongwei Video Technology TD-3116" },

	{ 0, -1, NULL }
};


struct tvcard bttv_tvcards[] = {
	
	[BTTV_BOARD_UNKNOWN] = {
		.name		= " *** UNKNOWN/GENERIC *** ",
		.video_inputs	= 4,
		.svhs		= 2,
		.muxsel		= MUXSEL(2, 3, 1, 0),
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_MIRO] = {
		.name		= "MIRO PCTV",
		.video_inputs	= 4,
		
		.svhs		= 2,
		.gpiomask	= 15,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 2, 0, 0, 0 },
		.gpiomute 	= 10,
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_HAUPPAUGE] = {
		.name		= "Hauppauge (bt848)",
		.video_inputs	= 4,
		
		.svhs		= 2,
		.gpiomask	= 7,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 1, 2, 3 },
		.gpiomute 	= 4,
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_STB] = {
		.name		= "STB, Gateway P/N 6000699 (bt848)",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 7,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 4, 0, 2, 3 },
		.gpiomute 	= 1,
		.no_msp34xx	= 1,
		.needs_tvaudio	= 1,
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.tuner_addr	= ADDR_UNSET,
		.pll            = PLL_28,
		.has_radio      = 1,
	},

	
	[BTTV_BOARD_INTEL] = {
		.name		= "Intel Create and Share PCI/ Smart Video Recorder III",
		.video_inputs	= 4,
		
		.svhs		= 2,
		.gpiomask	= 0,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0 },
		.needs_tvaudio	= 0,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_DIAMOND] = {
		.name		= "Diamond DTV2000",
		.video_inputs	= 4,
		
		.svhs		= 2,
		.gpiomask	= 3,
		.muxsel		= MUXSEL(2, 3, 1, 0),
		.gpiomux 	= { 0, 1, 0, 1 },
		.gpiomute 	= 3,
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_AVERMEDIA] = {
		.name		= "AVerMedia TVPhone",
		.video_inputs	= 3,
		
		.svhs		= 3,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomask	= 0x0f,
		.gpiomux 	= { 0x0c, 0x04, 0x08, 0x04 },
		
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= avermedia_tvphone_audio,
		.has_remote     = 1,
	},
	[BTTV_BOARD_MATRIX_VISION] = {
		.name		= "MATRIX-Vision MV-Delta",
		.video_inputs	= 5,
		
		.svhs		= 3,
		.gpiomask	= 0,
		.muxsel		= MUXSEL(2, 3, 1, 0, 0),
		.gpiomux 	= { 0 },
		.needs_tvaudio	= 1,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},

	
	[BTTV_BOARD_FLYVIDEO] = {
		.name		= "Lifeview FlyVideo II (Bt848) LR26 / MAXI TV Video PCI2 LR26",
		.video_inputs	= 4,
		
		.svhs		= 2,
		.gpiomask	= 0xc00,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0xc00, 0x800, 0x400 },
		.gpiomute 	= 0xc00,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_TURBOTV] = {
		.name		= "IMS/IXmicro TurboTV",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 3,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 1, 1, 2, 3 },
		.needs_tvaudio	= 0,
		.pll		= PLL_28,
		.tuner_type	= TUNER_TEMIC_PAL,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_HAUPPAUGE878] = {
		.name		= "Hauppauge (bt878)",
		.video_inputs	= 4,
		
		.svhs		= 2,
		.gpiomask	= 0x0f, 
		.muxsel		= MUXSEL(2, 0, 1, 1),
		.gpiomux 	= { 0, 1, 2, 3 },
		.gpiomute 	= 4,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_MIROPRO] = {
		.name		= "MIRO PCTV pro",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0x3014f,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0x20001,0x10001, 0, 0 },
		.gpiomute 	= 10,
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},

	
	[BTTV_BOARD_ADSTECH_TV] = {
		.name		= "ADS Technologies Channel Surfer TV (bt848)",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 15,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 13, 14, 11, 7 },
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_AVERMEDIA98] = {
		.name		= "AVerMedia TVCapture 98",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 15,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 13, 14, 11, 7 },
		.needs_tvaudio	= 1,
		.msp34xx_alt    = 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= avermedia_tv_stereo_audio,
		.no_gpioirq     = 1,
	},
	[BTTV_BOARD_VHX] = {
		.name		= "Aimslab Video Highway Xtreme (VHX)",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 7,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 2, 1, 3 }, 
		.gpiomute 	= 4,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_ZOLTRIX] = {
		.name		= "Zoltrix TV-Max",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 15,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0, 1, 0 },
		.gpiomute 	= 10,
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},

	
	[BTTV_BOARD_PIXVIEWPLAYTV] = {
		.name		= "Prolink Pixelview PlayTV (bt878)",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0x01fe00,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		
		.gpiomux        = { 0x001e00, 0, 0x018000, 0x014000 },
		.gpiomute 	= 0x002000,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr     = ADDR_UNSET,
	},
	[BTTV_BOARD_WINVIEW_601] = {
		.name		= "Leadtek WinView 601",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0x8300f8,
		.muxsel		= MUXSEL(2, 3, 1, 1, 0),
		.gpiomux 	= { 0x4fa007,0xcfa007,0xcfa007,0xcfa007 },
		.gpiomute 	= 0xcfa007,
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.volume_gpio	= winview_volume,
		.has_radio	= 1,
	},
	[BTTV_BOARD_AVEC_INTERCAP] = {
		.name		= "AVEC Intercapture",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 1, 0, 0, 0 },
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_LIFE_FLYKIT] = {
		.name		= "Lifeview FlyVideo II EZ /FlyKit LR38 Bt848 (capture only)",
		.video_inputs	= 4,
		
		.svhs		= NO_SVHS,
		.gpiomask	= 0x8dff00,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0 },
		.no_msp34xx	= 1,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},

	
	[BTTV_BOARD_CEI_RAFFLES] = {
		.name		= "CEI Raffles Card",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_CONFERENCETV] = {
		.name		= "Lifeview FlyVideo 98/ Lucky Star Image World ConferenceTV LR50",
		.video_inputs	= 4,
		
		.svhs		= 2,
		.gpiomask	= 0x1800,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0x800, 0x1000, 0x1000 },
		.gpiomute 	= 0x1800,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL_I,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_PHOEBE_TVMAS] = {
		.name		= "Askey CPH050/ Phoebe Tv Master + FM",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0xc00,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 1, 0x800, 0x400 },
		.gpiomute 	= 0xc00,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_MODTEC_205] = {
		.name		= "Modular Technology MM201/MM202/MM205/MM210/MM215 PCTV, bt878",
		.video_inputs	= 3,
		
		.svhs		= NO_SVHS,
		.has_dig_in	= 1,
		.gpiomask	= 7,
		.muxsel		= MUXSEL(2, 3, 0), 
		
		.gpiomux 	= { 0, 0, 0, 0 },
		.no_msp34xx	= 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ALPS_TSBB5_PAL_I,
		.tuner_addr	= ADDR_UNSET,
	},

	
	[BTTV_BOARD_MAGICTVIEW061] = {
		.name		= "Askey CPH05X/06X (bt878) [many vendors]",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0xe00,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= {0x400, 0x400, 0x400, 0x400 },
		.gpiomute 	= 0xc00,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.has_remote     = 1,
	},
	[BTTV_BOARD_VOBIS_BOOSTAR] = {
		.name           = "Terratec TerraTV+ Version 1.0 (Bt848)/ Terra TValue Version 1.0/ Vobis TV-Boostar",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask       = 0x1f0fff,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0x20000, 0x30000, 0x10000, 0 },
		.gpiomute 	= 0x40000,
		.needs_tvaudio	= 0,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= terratv_audio,
	},
	[BTTV_BOARD_HAUPPAUG_WCAM] = {
		.name		= "Hauppauge WinCam newer (bt878)",
		.video_inputs	= 4,
		
		.svhs		= 3,
		.gpiomask	= 7,
		.muxsel		= MUXSEL(2, 0, 1, 1),
		.gpiomux 	= { 0, 1, 2, 3 },
		.gpiomute 	= 4,
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_MAXI] = {
		.name		= "Lifeview FlyVideo 98/ MAXI TV Video PCI2 LR50",
		.video_inputs	= 4,
		
		.svhs		= 2,
		.gpiomask	= 0x1800,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0x800, 0x1000, 0x1000 },
		.gpiomute 	= 0x1800,
		.pll            = PLL_28,
		.tuner_type	= TUNER_PHILIPS_SECAM,
		.tuner_addr	= ADDR_UNSET,
	},

	
	[BTTV_BOARD_TERRATV] = {
		.name           = "Terratec TerraTV+ Version 1.1 (bt878)",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0x1f0fff,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0x20000, 0x30000, 0x10000, 0x00000 },
		.gpiomute 	= 0x40000,
		.needs_tvaudio	= 0,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= terratv_audio,

	},
	[BTTV_BOARD_PXC200] = {
		
		.name		= "Imagenation PXC200",
		.video_inputs	= 5,
		
		.svhs		= 1, 
		.gpiomask	= 0,
		.muxsel		= MUXSEL(2, 3, 1, 0, 0),
		.gpiomux 	= { 0 },
		.needs_tvaudio	= 1,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.muxsel_hook    = PXC200_muxsel,

	},
	[BTTV_BOARD_FLYVIDEO_98] = {
		.name		= "Lifeview FlyVideo 98 LR50",
		.video_inputs	= 4,
		
		.svhs		= 2,
		.gpiomask	= 0x1800,  
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0x0800, 0x1000, 0x1000 },
		.gpiomute 	= 0x1800,
		.pll            = PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_IPROTV] = {
		.name		= "Formac iProTV, Formac ProTV I (bt848)",
		.video_inputs	= 4,
		
		.svhs		= 3,
		.gpiomask	= 1,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 1, 0, 0, 0 },
		.pll            = PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
	},

	
	[BTTV_BOARD_INTEL_C_S_PCI] = {
		.name		= "Intel Create and Share PCI/ Smart Video Recorder III",
		.video_inputs	= 4,
		
		.svhs		= 2,
		.gpiomask	= 0,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0 },
		.needs_tvaudio	= 0,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_TERRATVALUE] = {
		.name           = "Terratec TerraTValue Version Bt878",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0xffff00,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0x500, 0, 0x300, 0x900 },
		.gpiomute 	= 0x900,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_WINFAST2000] = {
		.name		= "Leadtek WinFast 2000/ WinFast 2000 XP",
		.video_inputs	= 4,
		
		.svhs		= 2,
		
		.muxsel		= MUXSEL(2, 3, 1, 1, 0),
		
		.gpiomask	= 0xb33000,
		.gpiomux 	= { 0x122000,0x1000,0x0000,0x620000 },
		.gpiomute 	= 0x800000,
		.needs_tvaudio	= 0,
		.pll		= PLL_28,
		.has_radio	= 1,
		.tuner_type	= TUNER_PHILIPS_PAL, 
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= winfast2000_audio,
		.has_remote     = 1,
	},
	[BTTV_BOARD_CHRONOS_VS2] = {
		.name		= "Lifeview FlyVideo 98 LR50 / Chronos Video Shuttle II",
		.video_inputs	= 4,
		
		.svhs		= 2,
		.gpiomask	= 0x1800,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0x800, 0x1000, 0x1000 },
		.gpiomute 	= 0x1800,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},

	
	[BTTV_BOARD_TYPHOON_TVIEW] = {
		.name		= "Lifeview FlyVideo 98FM LR50 / Typhoon TView TV/FM Tuner",
		.video_inputs	= 4,
		
		.svhs		= 2,
		.gpiomask	= 0x1800,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0x800, 0x1000, 0x1000 },
		.gpiomute 	= 0x1800,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.has_radio	= 1,
	},
	[BTTV_BOARD_PXELVWPLTVPRO] = {
		.name		= "Prolink PixelView PlayTV pro",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0xff,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0x21, 0x20, 0x24, 0x2c },
		.gpiomute 	= 0x29,
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_MAGICTVIEW063] = {
		.name		= "Askey CPH06X TView99",
		.video_inputs	= 4,
		
		.svhs		= 2,
		.gpiomask	= 0x551e00,
		.muxsel		= MUXSEL(2, 3, 1, 0),
		.gpiomux 	= { 0x551400, 0x551200, 0, 0 },
		.gpiomute 	= 0x551c00,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL_I,
		.tuner_addr	= ADDR_UNSET,
		.has_remote     = 1,
	},
	[BTTV_BOARD_PINNACLE] = {
		.name		= "Pinnacle PCTV Studio/Rave",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0x03000F,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 2, 0xd0001, 0, 0 },
		.gpiomute 	= 1,
		.needs_tvaudio	= 0,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},

	
	[BTTV_BOARD_STB2] = {
		.name		= "STB TV PCI FM, Gateway P/N 6000704 (bt878), 3Dfx VoodooTV 100",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 7,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 4, 0, 2, 3 },
		.gpiomute 	= 1,
		.no_msp34xx	= 1,
		.needs_tvaudio	= 1,
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.tuner_addr	= ADDR_UNSET,
		.pll            = PLL_28,
		.has_radio      = 1,
	},
	[BTTV_BOARD_AVPHONE98] = {
		.name		= "AVerMedia TVPhone 98",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 15,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 13, 4, 11, 7 },
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.has_radio	= 1,
		.audio_mode_gpio= avermedia_tvphone_audio,
	},
	[BTTV_BOARD_PV951] = {
		.name		= "ProVideo PV951", 
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0, 0, 0},
		.needs_tvaudio	= 1,
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL_I,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_ONAIR_TV] = {
		.name		= "Little OnAir TV",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0xe00b,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0xff9ff6, 0xff9ff6, 0xff1ff7, 0 },
		.gpiomute 	= 0xff3ffc,
		.no_msp34xx	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},

	
	[BTTV_BOARD_SIGMA_TVII_FM] = {
		.name		= "Sigma TVII-FM",
		.video_inputs	= 2,
		
		.svhs		= NO_SVHS,
		.gpiomask	= 3,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 1, 1, 0, 2 },
		.gpiomute 	= 3,
		.no_msp34xx	= 1,
		.pll		= PLL_NONE,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_MATRIX_VISION2] = {
		.name		= "MATRIX-Vision MV-Delta 2",
		.video_inputs	= 5,
		
		.svhs		= 3,
		.gpiomask	= 0,
		.muxsel		= MUXSEL(2, 3, 1, 0, 0),
		.gpiomux 	= { 0 },
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_ZOLTRIX_GENIE] = {
		.name		= "Zoltrix Genie TV/FM",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0xbcf03f,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0xbc803f, 0xbc903f, 0xbcb03f, 0 },
		.gpiomute 	= 0xbcb03f,
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_TEMIC_4039FR5_NTSC,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_TERRATVRADIO] = {
		.name		= "Terratec TV/Radio+",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0x70000,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0x20000, 0x30000, 0x10000, 0 },
		.gpiomute 	= 0x40000,
		.needs_tvaudio	= 1,
		.no_msp34xx	= 1,
		.pll		= PLL_35,
		.tuner_type	= TUNER_PHILIPS_PAL_I,
		.tuner_addr	= ADDR_UNSET,
		.has_radio	= 1,
	},

	
	[BTTV_BOARD_DYNALINK] = {
		.name		= "Askey CPH03x/ Dynalink Magic TView",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 15,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= {2,0,0,0 },
		.gpiomute 	= 1,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_GVBCTV3PCI] = {
		.name		= "IODATA GV-BCTV3/PCI",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0x010f00,
		.muxsel		= MUXSEL(2, 3, 0, 0),
		.gpiomux 	= {0x10000, 0, 0x10000, 0 },
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_ALPS_TSHC6_NTSC,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= gvbctv3pci_audio,
	},
	[BTTV_BOARD_PXELVWPLTVPAK] = {
		.name		= "Prolink PV-BT878P+4E / PixelView PlayTV PAK / Lenco MXTV-9578 CP",
		.video_inputs	= 5,
		
		.svhs		= 3,
		.has_dig_in	= 1,
		.gpiomask	= 0xAA0000,
		.muxsel		= MUXSEL(2, 3, 1, 1, 0), 
		
		.gpiomux 	= { 0x20000, 0, 0x80000, 0x80000 },
		.gpiomute 	= 0xa8000,
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL_I,
		.tuner_addr	= ADDR_UNSET,
		.has_remote	= 1,
	},
	[BTTV_BOARD_EAGLE] = {
		.name           = "Eagle Wireless Capricorn2 (bt878A)",
		.video_inputs   = 4,
		
		.svhs           = 2,
		.gpiomask       = 7,
		.muxsel         = MUXSEL(2, 0, 1, 1),
		.gpiomux        = { 0, 1, 2, 3 },
		.gpiomute 	= 4,
		.pll            = PLL_28,
		.tuner_type     = UNSET ,
		.tuner_addr	= ADDR_UNSET,
	},

	
	[BTTV_BOARD_PINNACLEPRO] = {
		
		.name           = "Pinnacle PCTV Studio Pro",
		.video_inputs   = 4,
		
		.svhs           = 3,
		.gpiomask       = 0x03000F,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 1, 0xd0001, 0, 0 },
		.gpiomute 	= 10,
		.needs_tvaudio  = 0,
		.pll            = PLL_28,
		.tuner_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_TVIEW_RDS_FM] = {
		.name		= "Typhoon TView RDS + FM Stereo / KNC1 TV Station RDS",
		.video_inputs	= 4,
		
		.svhs		= 2,
		.gpiomask	= 0x1c,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0, 0x10, 8 },
		.gpiomute 	= 4,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.has_radio	= 1,
	},
	[BTTV_BOARD_LIFETEC_9415] = {
		.name		= "Lifeview FlyVideo 2000 /FlyVideo A2/ Lifetec LT 9415 TV [LR90]",
		.video_inputs	= 4,
		
		.svhs		= 2,
		.gpiomask	= 0x18e0,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0x0000,0x0800,0x1000,0x1000 },
		.gpiomute 	= 0x18e0,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_BESTBUY_EASYTV] = {
		.name           = "Askey CPH031/ BESTBUY Easy TV",
		.video_inputs	= 4,
		
		.svhs           = 2,
		.gpiomask       = 0xF,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.gpiomux        = { 2, 0, 0, 0 },
		.gpiomute 	= 10,
		.needs_tvaudio  = 0,
		.pll		= PLL_28,
		.tuner_type	= TUNER_TEMIC_PAL,
		.tuner_addr	= ADDR_UNSET,
	},

	
	[BTTV_BOARD_FLYVIDEO_98FM] = {
		
		.name           = "Lifeview FlyVideo 98FM LR50",
		.video_inputs   = 4,
		
		.svhs           = 2,
		.gpiomask       = 0x1800,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 0x800, 0x1000, 0x1000 },
		.gpiomute 	= 0x1800,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_GRANDTEC] = {
		.name           = "GrandTec 'Grand Video Capture' (Bt848)",
		.video_inputs   = 2,
		
		.svhs           = 1,
		.gpiomask       = 0,
		.muxsel         = MUXSEL(3, 1),
		.gpiomux        = { 0 },
		.needs_tvaudio  = 0,
		.no_msp34xx     = 1,
		.pll            = PLL_35,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_ASKEY_CPH060] = {
		
		.name           = "Askey CPH060/ Phoebe TV Master Only (No FM)",
		.video_inputs   = 3,
		
		.svhs           = 2,
		.gpiomask       = 0xe00,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0x400, 0x400, 0x400, 0x400 },
		.gpiomute 	= 0x800,
		.needs_tvaudio  = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_TEMIC_4036FY5_NTSC,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_ASKEY_CPH03X] = {
		
		.name		= "Askey CPH03x TV Capturer",
		.video_inputs	= 4,
		
		.svhs		= 2,
		.gpiomask       = 0x03000F,
		.muxsel		= MUXSEL(2, 3, 1, 0),
		.gpiomux        = { 2, 0, 0, 0 },
		.gpiomute 	= 1,
		.pll            = PLL_28,
		.tuner_type	= TUNER_TEMIC_PAL,
		.tuner_addr	= ADDR_UNSET,
		.has_remote	= 1,
	},

	
	[BTTV_BOARD_MM100PCTV] = {
		
		.name           = "Modular Technology MM100PCTV",
		.video_inputs   = 2,
		
		.svhs		= NO_SVHS,
		.gpiomask       = 11,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 2, 0, 0, 1 },
		.gpiomute 	= 8,
		.pll            = PLL_35,
		.tuner_type     = TUNER_TEMIC_PAL,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_GMV1] = {
		
		.name		= "AG Electronics GMV1",
		.video_inputs   = 2,
		
		.svhs		= 1,
		.gpiomask       = 0xF,
		.muxsel		= MUXSEL(2, 2),
		.gpiomux        = { },
		.no_msp34xx     = 1,
		.needs_tvaudio  = 0,
		.pll		= PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_BESTBUY_EASYTV2] = {
		.name           = "Askey CPH061/ BESTBUY Easy TV (bt878)",
		.video_inputs	= 3,
		
		.svhs           = 2,
		.gpiomask       = 0xFF,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.gpiomux        = { 1, 0, 4, 4 },
		.gpiomute 	= 9,
		.needs_tvaudio  = 0,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_ATI_TVWONDER] = {
		
		.name		= "ATI TV-Wonder",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0xf03f,
		.muxsel		= MUXSEL(2, 3, 1, 0),
		.gpiomux 	= { 0xbffe, 0, 0xbfff, 0 },
		.gpiomute 	= 0xbffe,
		.pll		= PLL_28,
		.tuner_type	= TUNER_TEMIC_4006FN5_MULTI_PAL,
		.tuner_addr	= ADDR_UNSET,
	},

	
	[BTTV_BOARD_ATI_TVWONDERVE] = {
		
		.name		= "ATI TV-Wonder VE",
		.video_inputs	= 2,
		
		.svhs		= NO_SVHS,
		.gpiomask	= 1,
		.muxsel		= MUXSEL(2, 3, 0, 1),
		.gpiomux 	= { 0, 0, 1, 0 },
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_TEMIC_4006FN5_MULTI_PAL,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_FLYVIDEO2000] = {
		
		.name           = "Lifeview FlyVideo 2000S LR90",
		.video_inputs   = 3,
		
		.svhs           = 2,
		.gpiomask	= 0x18e0,
		.muxsel		= MUXSEL(2, 3, 0, 1),
				
		.gpiomux 	= { 0x0000,0x0800,0x1000,0x1000 },
		.gpiomute 	= 0x1800,
		.audio_mode_gpio= fv2000s_audio,
		.no_msp34xx	= 1,
		.needs_tvaudio  = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_TERRATVALUER] = {
		.name		= "Terratec TValueRadio",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0xffff00,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0x500, 0x500, 0x300, 0x900 },
		.gpiomute 	= 0x900,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.has_radio	= 1,
	},
	[BTTV_BOARD_GVBCTV4PCI] = {
		
		.name           = "IODATA GV-BCTV4/PCI",
		.video_inputs   = 3,
		
		.svhs           = 2,
		.gpiomask       = 0x010f00,
		.muxsel         = MUXSEL(2, 3, 0, 0),
		.gpiomux        = {0x10000, 0, 0x10000, 0 },
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_SHARP_2U5JF5540_NTSC,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= gvbctv3pci_audio,
	},

	
	[BTTV_BOARD_VOODOOTV_FM] = {
		.name           = "3Dfx VoodooTV FM (Euro)",
		.video_inputs   = 4,
		
		.svhs           = NO_SVHS,
		.gpiomask       = 0x4f8a00,
		.gpiomux        = {0x947fff, 0x987fff,0x947fff,0x947fff },
		.gpiomute 	= 0x947fff,
		.muxsel         = MUXSEL(2, 3, 0, 1),
		.tuner_type     = TUNER_MT2032,
		.tuner_addr	= ADDR_UNSET,
		.pll		= PLL_28,
		.has_radio	= 1,
	},
	[BTTV_BOARD_VOODOOTV_200] = {
		.name           = "VoodooTV 200 (USA)",
		.video_inputs   = 4,
		
		.svhs           = NO_SVHS,
		.gpiomask       = 0x4f8a00,
		.gpiomux        = {0x947fff, 0x987fff,0x947fff,0x947fff },
		.gpiomute 	= 0x947fff,
		.muxsel         = MUXSEL(2, 3, 0, 1),
		.tuner_type     = TUNER_MT2032,
		.tuner_addr	= ADDR_UNSET,
		.pll		= PLL_28,
		.has_radio	= 1,
	},
	[BTTV_BOARD_AIMMS] = {
		
		.name           = "Active Imaging AIMMS",
		.video_inputs   = 1,
		
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.pll            = PLL_28,
		.muxsel         = MUXSEL(2),
		.gpiomask       = 0
	},
	[BTTV_BOARD_PV_BT878P_PLUS] = {
		
		.name           = "Prolink Pixelview PV-BT878P+ (Rev.4C,8E)",
		.video_inputs   = 3,
		
		.svhs           = 2,
		.gpiomask       = 15,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 0, 11, 7 }, 
		.gpiomute 	= 13,
		.needs_tvaudio  = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_LG_PAL_I_FM,
		.tuner_addr	= ADDR_UNSET,
		.has_remote     = 1,
	},
	[BTTV_BOARD_FLYVIDEO98EZ] = {
		.name		= "Lifeview FlyVideo 98EZ (capture only) LR51",
		.video_inputs	= 4,
		
		.svhs		= 2,
		
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.pll		= PLL_28,
		.no_msp34xx	= 1,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},

	
	[BTTV_BOARD_PV_BT878P_9B] = {
		
		.name		= "Prolink Pixelview PV-BT878P+9B (PlayTV Pro rev.9B FM+NICAM)",
		.video_inputs	= 4,
		
		.svhs		= 2,
		.gpiomask	= 0x3f,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0x01, 0x00, 0x03, 0x03 },
		.gpiomute 	= 0x09,
		.needs_tvaudio  = 1,
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= pvbt878p9b_audio, 
		.has_radio	= 1,  
		.has_remote     = 1,
	},
	[BTTV_BOARD_SENSORAY311_611] = {
		
		
		.name           = "Sensoray 311/611",
		.video_inputs   = 5,
		
		.svhs           = 4,
		.gpiomask       = 0,
		.muxsel         = MUXSEL(2, 3, 1, 0, 0),
		.gpiomux        = { 0 },
		.needs_tvaudio  = 0,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_RV605] = {
		
		.name           = "RemoteVision MX (RV605)",
		.video_inputs   = 16,
		
		.svhs           = NO_SVHS,
		.gpiomask       = 0x00,
		.gpiomask2      = 0x07ff,
		.muxsel         = MUXSEL(3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3),
		.no_msp34xx     = 1,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.muxsel_hook    = rv605_muxsel,
	},
	[BTTV_BOARD_POWERCLR_MTV878] = {
		.name           = "Powercolor MTV878/ MTV878R/ MTV878F",
		.video_inputs   = 3,
		
		.svhs           = 2,
		.gpiomask       = 0x1C800F,  
		.muxsel         = MUXSEL(2, 1, 1),
		.gpiomux        = { 0, 1, 2, 2 },
		.gpiomute 	= 4,
		.needs_tvaudio  = 0,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.pll		= PLL_28,
		.has_radio	= 1,
	},

	
	[BTTV_BOARD_WINDVR] = {
		
		.name           = "Canopus WinDVR PCI (COMPAQ Presario 3524JP, 5112JP)",
		.video_inputs   = 3,
		
		.svhs           = 2,
		.gpiomask       = 0x140007,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 1, 2, 3 },
		.gpiomute 	= 4,
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= windvr_audio,
	},
	[BTTV_BOARD_GRANDTEC_MULTI] = {
		.name           = "GrandTec Multi Capture Card (Bt878)",
		.video_inputs   = 4,
		
		.svhs           = NO_SVHS,
		.gpiomask       = 0,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.gpiomux        = { 0 },
		.needs_tvaudio  = 0,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_KWORLD] = {
		.name           = "Jetway TV/Capture JW-TV878-FBK, Kworld KW-TV878RF",
		.video_inputs   = 4,
		
		.svhs           = 2,
		.gpiomask       = 7,
		
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 0, 4, 4 },
		.gpiomute 	= 4,
		.needs_tvaudio  = 0,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.has_radio	= 1,
	},
	[BTTV_BOARD_DSP_TCVIDEO] = {
		
		.name           = "DSP Design TCVIDEO",
		.video_inputs   = 4,
		.svhs           = NO_SVHS,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.pll            = PLL_28,
		.tuner_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
	},

		
	[BTTV_BOARD_HAUPPAUGEPVR] = {
		.name           = "Hauppauge WinTV PVR",
		.video_inputs   = 4,
		
		.svhs           = 2,
		.muxsel         = MUXSEL(2, 0, 1, 1),
		.needs_tvaudio  = 1,
		.pll            = PLL_28,
		.tuner_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,

		.gpiomask       = 7,
		.gpiomux        = {7},
	},
	[BTTV_BOARD_GVBCTV5PCI] = {
		.name           = "IODATA GV-BCTV5/PCI",
		.video_inputs   = 3,
		
		.svhs           = 2,
		.gpiomask       = 0x0f0f80,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.gpiomux        = {0x030000, 0x010000, 0, 0 },
		.gpiomute 	= 0x020000,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= gvbctv5pci_audio,
		.has_radio      = 1,
	},
	[BTTV_BOARD_OSPREY1x0] = {
		.name           = "Osprey 100/150 (878)", 
		.video_inputs   = 4,                  
		
		.svhs           = 3,
		.muxsel         = MUXSEL(3, 2, 0, 1),
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
	},
	[BTTV_BOARD_OSPREY1x0_848] = {
		.name           = "Osprey 100/150 (848)", 
		.video_inputs   = 3,
		
		.svhs           = 2,
		.muxsel         = MUXSEL(2, 3, 1),
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
	},

		
	[BTTV_BOARD_OSPREY101_848] = {
		.name           = "Osprey 101 (848)", 
		.video_inputs   = 2,
		
		.svhs           = 1,
		.muxsel         = MUXSEL(3, 1),
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
	},
	[BTTV_BOARD_OSPREY1x1] = {
		.name           = "Osprey 101/151",       
		.video_inputs   = 1,
		
		.svhs           = NO_SVHS,
		.muxsel         = MUXSEL(0),
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
	},
	[BTTV_BOARD_OSPREY1x1_SVID] = {
		.name           = "Osprey 101/151 w/ svid",  
		.video_inputs   = 2,
		
		.svhs           = 1,
		.muxsel         = MUXSEL(0, 1),
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
	},
	[BTTV_BOARD_OSPREY2xx] = {
		.name           = "Osprey 200/201/250/251",  
		.video_inputs   = 1,
		
		.svhs           = NO_SVHS,
		.muxsel         = MUXSEL(0),
		.pll            = PLL_28,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
	},

		
	[BTTV_BOARD_OSPREY2x0_SVID] = {
		.name           = "Osprey 200/250",   
		.video_inputs   = 2,
		
		.svhs           = 1,
		.muxsel         = MUXSEL(0, 1),
		.pll            = PLL_28,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
	},
	[BTTV_BOARD_OSPREY2x0] = {
		.name           = "Osprey 210/220/230",   
		.video_inputs   = 2,
		
		.svhs           = 1,
		.muxsel         = MUXSEL(2, 3),
		.pll            = PLL_28,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
	},
	[BTTV_BOARD_OSPREY500] = {
		.name           = "Osprey 500",   
		.video_inputs   = 2,
		
		.svhs           = 1,
		.muxsel         = MUXSEL(2, 3),
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
	},
	[BTTV_BOARD_OSPREY540] = {
		.name           = "Osprey 540",   
		.video_inputs   = 4,
		
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
	},

		
	[BTTV_BOARD_OSPREY2000] = {
		.name           = "Osprey 2000",  
		.video_inputs   = 2,
		
		.svhs           = 1,
		.muxsel         = MUXSEL(2, 3),
		.pll            = PLL_28,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,      
	},
	[BTTV_BOARD_IDS_EAGLE] = {
		
		.name           = "IDS Eagle",
		.video_inputs   = 4,
		
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.svhs           = NO_SVHS,
		.gpiomask       = 0,
		.muxsel         = MUXSEL(2, 2, 2, 2),
		.muxsel_hook    = eagle_muxsel,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
	},
	[BTTV_BOARD_PINNACLESAT] = {
		.name           = "Pinnacle PCTV Sat",
		.video_inputs   = 2,
		
		.svhs           = 1,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
		.muxsel         = MUXSEL(3, 1),
		.pll            = PLL_28,
		.no_gpioirq     = 1,
		.has_dvb        = 1,
	},
	[BTTV_BOARD_FORMAC_PROTV] = {
		.name           = "Formac ProTV II (bt878)",
		.video_inputs   = 4,
		
		.svhs           = 3,
		.gpiomask       = 2,
		
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 2, 2, 0, 0 },
		.pll            = PLL_28,
		.has_radio      = 1,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
	},

		
	[BTTV_BOARD_MACHTV] = {
		.name           = "MachTV",
		.video_inputs   = 3,
		
		.svhs           = NO_SVHS,
		.gpiomask       = 7,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 1, 2, 3},
		.gpiomute 	= 4,
		.needs_tvaudio  = 1,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.pll            = PLL_28,
	},
	[BTTV_BOARD_EURESYS_PICOLO] = {
		.name           = "Euresys Picolo",
		.video_inputs   = 3,
		
		.svhs           = 2,
		.gpiomask       = 0,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
		.muxsel         = MUXSEL(2, 0, 1),
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_PV150] = {
		
		.name           = "ProVideo PV150", 
		.video_inputs   = 2,
		
		.svhs           = NO_SVHS,
		.gpiomask       = 0,
		.muxsel         = MUXSEL(2, 3),
		.gpiomux        = { 0 },
		.needs_tvaudio  = 0,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_AD_TVK503] = {
		
		
		.name           = "AD-TVK503", 
		.video_inputs   = 4,
		
		.svhs           = 2,
		.gpiomask       = 0x001e8007,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		
		.gpiomux        = { 0x08,  0x0f,  0x0a,     0x08 },
		.gpiomute 	= 0x0f,
		.needs_tvaudio  = 0,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= adtvk503_audio,
	},

		
	[BTTV_BOARD_HERCULES_SM_TV] = {
		.name           = "Hercules Smart TV Stereo",
		.video_inputs   = 4,
		
		.svhs           = 2,
		.gpiomask       = 0x00,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.needs_tvaudio  = 1,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_PACETV] = {
		.name           = "Pace TV & Radio Card",
		.video_inputs   = 4,
		
		.svhs           = 2,
		
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomask       = 0,
		.no_tda7432     = 1,
		.tuner_type     = TUNER_PHILIPS_PAL_I,
		.tuner_addr	= ADDR_UNSET,
		.has_radio      = 1,
		.pll            = PLL_28,
	},
	[BTTV_BOARD_IVC200] = {
		
		.name           = "IVC-200",
		.video_inputs   = 1,
		
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.svhs           = NO_SVHS,
		.gpiomask       = 0xdf,
		.muxsel         = MUXSEL(2),
		.pll            = PLL_28,
	},
	[BTTV_BOARD_IVCE8784] = {
		.name           = "IVCE-8784",
		.video_inputs   = 1,
		
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr     = ADDR_UNSET,
		.svhs           = NO_SVHS,
		.gpiomask       = 0xdf,
		.muxsel         = MUXSEL(2),
		.pll            = PLL_28,
	},
	[BTTV_BOARD_XGUARD] = {
		.name           = "Grand X-Guard / Trust 814PCI",
		.video_inputs   = 16,
		
		.svhs           = NO_SVHS,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.gpiomask2      = 0xff,
		.muxsel         = MUXSEL(2,2,2,2, 3,3,3,3, 1,1,1,1, 0,0,0,0),
		.muxsel_hook    = xguard_muxsel,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
		.pll            = PLL_28,
	},

		
	[BTTV_BOARD_NEBULA_DIGITV] = {
		.name           = "Nebula Electronics DigiTV",
		.video_inputs   = 1,
		.svhs           = NO_SVHS,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.has_dvb        = 1,
		.has_remote	= 1,
		.gpiomask	= 0x1b,
		.no_gpioirq     = 1,
	},
	[BTTV_BOARD_PV143] = {
		
		.name           = "ProVideo PV143",
		.video_inputs   = 4,
		
		.svhs           = NO_SVHS,
		.gpiomask       = 0,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.gpiomux        = { 0 },
		.needs_tvaudio  = 0,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_VD009X1_VD011_MINIDIN] = {
		
		.name           = "PHYTEC VD-009-X1 VD-011 MiniDIN (bt878)",
		.video_inputs   = 4,
		
		.svhs           = 3,
		.gpiomask       = 0x00,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.gpiomux        = { 0, 0, 0, 0 }, 
		.needs_tvaudio  = 0,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_VD009X1_VD011_COMBI] = {
		.name           = "PHYTEC VD-009-X1 VD-011 Combi (bt878)",
		.video_inputs   = 4,
		
		.svhs           = 3,
		.gpiomask       = 0x00,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 0, 0, 0 }, 
		.needs_tvaudio  = 0,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},

		
	[BTTV_BOARD_VD009_MINIDIN] = {
		.name           = "PHYTEC VD-009 MiniDIN (bt878)",
		.video_inputs   = 10,
		
		.svhs           = 9,
		.gpiomask       = 0x00,
		.gpiomask2      = 0x03, 
		.muxsel         = MUXSEL(2, 2, 2, 2, 3, 3, 3, 3, 1, 0),
		.muxsel_hook	= phytec_muxsel,
		.gpiomux        = { 0, 0, 0, 0 }, 
		.needs_tvaudio  = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_VD009_COMBI] = {
		.name           = "PHYTEC VD-009 Combi (bt878)",
		.video_inputs   = 10,
		
		.svhs           = 9,
		.gpiomask       = 0x00,
		.gpiomask2      = 0x03, 
		.muxsel         = MUXSEL(2, 2, 2, 2, 3, 3, 3, 3, 1, 1),
		.muxsel_hook	= phytec_muxsel,
		.gpiomux        = { 0, 0, 0, 0 }, 
		.needs_tvaudio  = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_IVC100] = {
		.name           = "IVC-100",
		.video_inputs   = 4,
		
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.svhs           = NO_SVHS,
		.gpiomask       = 0xdf,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.pll            = PLL_28,
	},
	[BTTV_BOARD_IVC120] = {
		
		.name           = "IVC-120G",
		.video_inputs   = 16,
		
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.svhs           = NO_SVHS,   
		.needs_tvaudio  = 0,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
		.gpiomask       = 0x00,
		.muxsel         = MUXSEL(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
		.muxsel_hook    = ivc120_muxsel,
		.pll            = PLL_28,
	},

		
	[BTTV_BOARD_PC_HDTV] = {
		.name           = "pcHDTV HD-2000 TV",
		.video_inputs   = 4,
		
		.svhs           = 2,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.tuner_type     = TUNER_PHILIPS_FCV1236D,
		.tuner_addr	= ADDR_UNSET,
		.has_dvb        = 1,
	},
	[BTTV_BOARD_TWINHAN_DST] = {
		.name           = "Twinhan DST + clones",
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_video       = 1,
		.has_dvb        = 1,
	},
	[BTTV_BOARD_WINFASTVC100] = {
		.name           = "Winfast VC100",
		.video_inputs   = 3,
		
		.svhs           = 1,
		
		.muxsel		= MUXSEL(3, 1, 1, 3),
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.pll            = PLL_28,
	},
	[BTTV_BOARD_TEV560] = {
		.name           = "Teppro TEV-560/InterVision IV-560",
		.video_inputs   = 3,
		
		.svhs           = 2,
		.gpiomask       = 3,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 1, 1, 1, 1 },
		.needs_tvaudio  = 1,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.pll            = PLL_35,
	},

		
	[BTTV_BOARD_SIMUS_GVC1100] = {
		.name           = "SIMUS GVC1100",
		.video_inputs   = 4,
		
		.svhs           = NO_SVHS,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.pll            = PLL_28,
		.muxsel         = MUXSEL(2, 2, 2, 2),
		.gpiomask       = 0x3F,
		.muxsel_hook    = gvc1100_muxsel,
	},
	[BTTV_BOARD_NGSTV_PLUS] = {
		
		.name           = "NGS NGSTV+",
		.video_inputs   = 3,
		.svhs           = 2,
		.gpiomask       = 0x008007,
		.muxsel         = MUXSEL(2, 3, 0, 0),
		.gpiomux        = { 0, 0, 0, 0 },
		.gpiomute 	= 0x000003,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.has_remote     = 1,
	},
	[BTTV_BOARD_LMLBT4] = {
		
		.name           = "LMLBT4",
		.video_inputs   = 4, 
		
		.svhs           = NO_SVHS,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
		.needs_tvaudio  = 0,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_TEKRAM_M205] = {
		
		.name           = "Tekram M205 PRO",
		.video_inputs   = 3,
		
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.svhs           = 2,
		.needs_tvaudio  = 0,
		.gpiomask       = 0x68,
		.muxsel         = MUXSEL(2, 3, 1),
		.gpiomux        = { 0x68, 0x68, 0x61, 0x61 },
		.pll            = PLL_28,
	},

		
	[BTTV_BOARD_CONTVFMI] = {
		
		
		.name           = "Conceptronic CONTVFMi",
		.video_inputs   = 3,
		
		.svhs           = 2,
		.gpiomask       = 0x008007,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 1, 2, 2 },
		.gpiomute 	= 3,
		.needs_tvaudio  = 0,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.has_remote     = 1,
		.has_radio      = 1,
	},
	[BTTV_BOARD_PICOLO_TETRA_CHIP] = {
		
		
		
		
		.name           = "Euresys Picolo Tetra",
		.video_inputs   = 4,
		
		.svhs           = NO_SVHS,
		.gpiomask       = 0,
		.gpiomask2      = 0x3C<<16,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
		
		.muxsel         = MUXSEL(2, 2, 2, 2),
		.gpiomux        = { 0, 0, 0, 0 }, 
		.pll            = PLL_28,
		.needs_tvaudio  = 0,
		.muxsel_hook    = picolo_tetra_muxsel,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_SPIRIT_TV] = {
		
		
		.name           = "Spirit TV Tuner",
		.video_inputs   = 3,
		
		.svhs           = 2,
		.gpiomask       = 0x0000000f,
		.muxsel         = MUXSEL(2, 1, 1),
		.gpiomux        = { 0x02, 0x00, 0x00, 0x00 },
		.tuner_type     = TUNER_TEMIC_PAL,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
	},
	[BTTV_BOARD_AVDVBT_771] = {
		
		.name           = "AVerMedia AVerTV DVB-T 771",
		.video_inputs   = 2,
		.svhs           = 1,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.muxsel         = MUXSEL(3, 3),
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
		.pll            = PLL_28,
		.has_dvb        = 1,
		.no_gpioirq     = 1,
		.has_remote     = 1,
	},
		
	[BTTV_BOARD_AVDVBT_761] = {
		
		
		.name           = "AverMedia AverTV DVB-T 761",
		.video_inputs   = 2,
		.svhs           = 1,
		.muxsel         = MUXSEL(3, 1, 2, 0), 
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.has_dvb        = 1,
		.no_gpioirq     = 1,
		.has_remote     = 1,
	},
	[BTTV_BOARD_MATRIX_VISIONSQ] = {
		
		.name		= "MATRIX Vision Sigma-SQ",
		.video_inputs	= 16,
		
		.svhs		= NO_SVHS,
		.gpiomask	= 0x0,
		.muxsel		= MUXSEL(2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3),
		.muxsel_hook	= sigmaSQ_muxsel,
		.gpiomux	= { 0 },
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_MATRIX_VISIONSLC] = {
		
		.name		= "MATRIX Vision Sigma-SLC",
		.video_inputs	= 4,
		
		.svhs		= NO_SVHS,
		.gpiomask	= 0x0,
		.muxsel		= MUXSEL(2, 2, 2, 2),
		.muxsel_hook	= sigmaSLC_muxsel,
		.gpiomux	= { 0 },
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
		
	[BTTV_BOARD_APAC_VIEWCOMP] = {
		
		
		.name           = "APAC Viewcomp 878(AMAX)",
		.video_inputs   = 2,
		
		.svhs           = NO_SVHS,
		.gpiomask       = 0xFF,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 2, 0, 0, 0 },
		.gpiomute 	= 10,
		.needs_tvaudio  = 0,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.has_remote     = 1,   
		.has_radio      = 1,   
	},

		
	[BTTV_BOARD_DVICO_DVBT_LITE] = {
		
		.name           = "DViCO FusionHDTV DVB-T Lite",
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
		.pll            = PLL_28,
		.no_video       = 1,
		.has_dvb        = 1,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_VGEAR_MYVCD] = {
		
		.name           = "V-Gear MyVCD",
		.video_inputs   = 3,
		
		.svhs           = 2,
		.gpiomask       = 0x3f,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.gpiomux        = {0x31, 0x31, 0x31, 0x31 },
		.gpiomute 	= 0x31,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.tuner_addr	= ADDR_UNSET,
		.has_radio      = 0,
	},
	[BTTV_BOARD_SUPER_TV] = {
		
		.name           = "Super TV Tuner",
		.video_inputs   = 4,
		
		.svhs           = 2,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.tuner_addr	= ADDR_UNSET,
		.gpiomask       = 0x008007,
		.gpiomux        = { 0, 0x000001,0,0 },
		.needs_tvaudio  = 1,
		.has_radio      = 1,
	},
	[BTTV_BOARD_TIBET_CS16] = {
		
		.name           = "Tibet Systems 'Progress DVR' CS16",
		.video_inputs   = 16,
		
		.svhs           = NO_SVHS,
		.muxsel         = MUXSEL(2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2),
		.pll		= PLL_28,
		.no_msp34xx     = 1,
		.no_tda7432	= 1,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.muxsel_hook    = tibetCS16_muxsel,
	},
	[BTTV_BOARD_KODICOM_4400R] = {
		
		.name           = "Kodicom 4400R (master)",
		.video_inputs	= 16,
		
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.svhs		= NO_SVHS,
		.gpiomask	= 0x0003ff,
		.no_gpioirq     = 1,
		.muxsel		= MUXSEL(3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3),
		.pll		= PLL_28,
		.no_msp34xx	= 1,
		.no_tda7432	= 1,
		.muxsel_hook	= kodicom4400r_muxsel,
	},
	[BTTV_BOARD_KODICOM_4400R_SL] = {
		
		.name		= "Kodicom 4400R (slave)",
		.video_inputs	= 16,
		
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.svhs		= NO_SVHS,
		.gpiomask	= 0x010000,
		.no_gpioirq     = 1,
		.muxsel		= MUXSEL(3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3),
		.pll		= PLL_28,
		.no_msp34xx	= 1,
		.no_tda7432	= 1,
		.muxsel_hook	= kodicom4400r_muxsel,
	},
		
	[BTTV_BOARD_ADLINK_RTV24] = {
		
		
		.name           = "Adlink RTV24",
		.video_inputs   = 4,
		
		.svhs           = 2,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.tuner_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.pll            = PLL_28,
	},
		
	[BTTV_BOARD_DVICO_FUSIONHDTV_5_LITE] = {
		
		.name           = "DViCO FusionHDTV 5 Lite",
		.tuner_type     = TUNER_LG_TDVS_H06XF, 
		.tuner_addr	= ADDR_UNSET,
		.video_inputs   = 3,
		
		.svhs           = 2,
		.muxsel		= MUXSEL(2, 3, 1),
		.gpiomask       = 0x00e00007,
		.gpiomux        = { 0x00400005, 0, 0x00000001, 0 },
		.gpiomute 	= 0x00c00007,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
		.has_dvb        = 1,
	},
		
	[BTTV_BOARD_ACORP_Y878F] = {
		
		.name		= "Acorp Y878F",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0x01fe00,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0x001e00, 0, 0x018000, 0x014000 },
		.gpiomute 	= 0x002000,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_YMEC_TVF66T5_B_DFF,
		.tuner_addr	= 0xc1 >>1,
		.has_radio	= 1,
	},
		
	[BTTV_BOARD_CONCEPTRONIC_CTVFMI2] = {
		.name           = "Conceptronic CTVFMi v2",
		.video_inputs   = 3,
		
		.svhs           = 2,
		.gpiomask       = 0x001c0007,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 1, 2, 2 },
		.gpiomute 	= 3,
		.needs_tvaudio  = 0,
		.pll            = PLL_28,
		.tuner_type     = TUNER_TENA_9533_DI,
		.tuner_addr	= ADDR_UNSET,
		.has_remote     = 1,
		.has_radio      = 1,
	},
		
	[BTTV_BOARD_PV_BT878P_2E] = {
		.name		= "Prolink Pixelview PV-BT878P+ (Rev.2E)",
		.video_inputs	= 5,
		
		.svhs		= 3,
		.has_dig_in	= 1,
		.gpiomask	= 0x01fe00,
		.muxsel		= MUXSEL(2, 3, 1, 1, 0), 
		
		.gpiomux	= { 0x00400, 0x10400, 0x04400, 0x80000 },
		.gpiomute	= 0x12400,
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_LG_PAL_FM,
		.tuner_addr	= ADDR_UNSET,
		.has_remote	= 1,
	},
		
	[BTTV_BOARD_PV_M4900] = {
		
		.name           = "Prolink PixelView PlayTV MPEG2 PV-M4900",
		.video_inputs   = 3,
		
		.svhs           = 2,
		.gpiomask       = 0x3f,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0x21, 0x20, 0x24, 0x2c },
		.gpiomute 	= 0x29,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_YMEC_TVF_5533MF,
		.tuner_addr     = ADDR_UNSET,
		.has_radio      = 1,
		.has_remote     = 1,
	},
		
	[BTTV_BOARD_OSPREY440]  = {
		.name           = "Osprey 440",
		.video_inputs   = 4,
		
		.svhs           = NO_SVHS,
		.muxsel         = MUXSEL(2, 3, 0, 1), 
		.gpiomask	= 0x303,
		.gpiomute	= 0x000, 
		.gpiomux	= { 0, 0, 0x000, 0x100},
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr     = ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
	},
		
	[BTTV_BOARD_ASOUND_SKYEYE] = {
		.name		= "Asound Skyeye PCTV",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 15,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 2, 0, 0, 0 },
		.gpiomute 	= 1,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_NTSC,
		.tuner_addr	= ADDR_UNSET,
	},
		
	[BTTV_BOARD_SABRENT_TVFM] = {
		.name		= "Sabrent TV-FM (bttv version)",
		.video_inputs	= 3,
		
		.svhs		= 2,
		.gpiomask	= 0x108007,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 100000, 100002, 100002, 100000 },
		.no_msp34xx	= 1,
		.no_tda7432     = 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_TNF_5335MF,
		.tuner_addr	= ADDR_UNSET,
		.has_radio      = 1,
	},
	
	[BTTV_BOARD_HAUPPAUGE_IMPACTVCB] = {
		.name		= "Hauppauge ImpactVCB (bt878)",
		.video_inputs	= 4,
		
		.svhs		= NO_SVHS,
		.gpiomask	= 0x0f, 
		.muxsel		= MUXSEL(0, 1, 3, 2), 
		.no_msp34xx	= 1,
		.no_tda7432     = 1,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_MACHTV_MAGICTV] = {

		.name           = "MagicTV", 
		.video_inputs   = 3,
		
		.svhs           = 2,
		.gpiomask       = 7,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 1, 2, 3 },
		.gpiomute 	= 4,
		.tuner_type     = TUNER_TEMIC_4009FR5_PAL,
		.tuner_addr     = ADDR_UNSET,
		.pll            = PLL_28,
		.has_radio      = 1,
		.has_remote     = 1,
	},
	[BTTV_BOARD_SSAI_SECURITY] = {
		.name		= "SSAI Security Video Interface",
		.video_inputs	= 4,
		
		.svhs		= NO_SVHS,
		.muxsel		= MUXSEL(0, 1, 2, 3),
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_SSAI_ULTRASOUND] = {
		.name		= "SSAI Ultrasound Video Interface",
		.video_inputs	= 2,
		
		.svhs		= 1,
		.muxsel		= MUXSEL(2, 0, 1, 3),
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	
	[BTTV_BOARD_DVICO_FUSIONHDTV_2] = {
		.name           = "DViCO FusionHDTV 2",
		.tuner_type     = TUNER_PHILIPS_FCV1236D,
		.tuner_addr	= ADDR_UNSET,
		.video_inputs   = 3,
		
		.svhs           = 2,
		.muxsel		= MUXSEL(2, 3, 1),
		.gpiomask       = 0x00e00007,
		.gpiomux        = { 0x00400005, 0, 0x00000001, 0 },
		.gpiomute 	= 0x00c00007,
		.no_msp34xx     = 1,
		.no_tda7432     = 1,
	},
	
	[BTTV_BOARD_TYPHOON_TVTUNERPCI] = {
		.name           = "Typhoon TV-Tuner PCI (50684)",
		.video_inputs   = 3,
		
		.svhs           = 2,
		.gpiomask       = 0x3014f,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0x20001,0x10001, 0, 0 },
		.gpiomute       = 10,
		.needs_tvaudio  = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_PAL_I,
		.tuner_addr     = ADDR_UNSET,
	},
	[BTTV_BOARD_GEOVISION_GV600] = {
		
		.name		= "Geovision GV-600",
		.video_inputs	= 16,
		
		.svhs		= NO_SVHS,
		.gpiomask	= 0x0,
		.muxsel		= MUXSEL(2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2),
		.muxsel_hook	= geovision_muxsel,
		.gpiomux	= { 0 },
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_KOZUMI_KTV_01C] = {

		.name           = "Kozumi KTV-01C",
		.video_inputs   = 3,
		
		.svhs           = 2,
		.gpiomask       = 0x008007,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 1, 2, 2 }, 
		.gpiomute 	= 3, 
		.needs_tvaudio  = 0,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3, 
		.tuner_addr     = ADDR_UNSET,
		.pll            = PLL_28,
		.has_radio      = 1,
		.has_remote     = 1,
	},
	[BTTV_BOARD_ENLTV_FM_2] = {
		.name           = "Encore ENL TV-FM-2",
		.video_inputs   = 3,
		
		.svhs           = 2,
		.gpiomask       = 0x060040,
		.muxsel         = MUXSEL(2, 3, 3),
		.gpiomux        = { 0x60000, 0x60000, 0x20000, 0x20000 },
		.gpiomute 	= 0,
		.tuner_type	= TUNER_TCL_MF02GIP_5N,
		.tuner_addr     = ADDR_UNSET,
		.pll            = PLL_28,
		.has_radio      = 1,
		.has_remote     = 1,
	},
		[BTTV_BOARD_VD012] = {
		
		.name           = "PHYTEC VD-012 (bt878)",
		.video_inputs   = 4,
		
		.svhs           = NO_SVHS,
		.gpiomask       = 0x00,
		.muxsel         = MUXSEL(0, 2, 3, 1),
		.gpiomux        = { 0, 0, 0, 0 }, 
		.needs_tvaudio  = 0,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
		[BTTV_BOARD_VD012_X1] = {
		
		.name           = "PHYTEC VD-012-X1 (bt878)",
		.video_inputs   = 4,
		
		.svhs           = 3,
		.gpiomask       = 0x00,
		.muxsel         = MUXSEL(2, 3, 1),
		.gpiomux        = { 0, 0, 0, 0 }, 
		.needs_tvaudio  = 0,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
		[BTTV_BOARD_VD012_X2] = {
		
		.name           = "PHYTEC VD-012-X2 (bt878)",
		.video_inputs   = 4,
		
		.svhs           = 3,
		.gpiomask       = 0x00,
		.muxsel         = MUXSEL(3, 2, 1),
		.gpiomux        = { 0, 0, 0, 0 }, 
		.needs_tvaudio  = 0,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
		[BTTV_BOARD_GEOVISION_GV800S] = {
		.name           = "Geovision GV-800(S) (master)",
		.video_inputs   = 4,
		
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.svhs           = NO_SVHS,
		.gpiomask	= 0xf107f,
		.no_gpioirq     = 1,
		.muxsel		= MUXSEL(2, 2, 2, 2),
		.pll		= PLL_28,
		.no_msp34xx	= 1,
		.no_tda7432	= 1,
		.muxsel_hook    = gv800s_muxsel,
	},
		[BTTV_BOARD_GEOVISION_GV800S_SL] = {
		.name           = "Geovision GV-800(S) (slave)",
		.video_inputs   = 4,
		
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.svhs           = NO_SVHS,
		.gpiomask	= 0x00,
		.no_gpioirq     = 1,
		.muxsel		= MUXSEL(2, 2, 2, 2),
		.pll		= PLL_28,
		.no_msp34xx	= 1,
		.no_tda7432	= 1,
		.muxsel_hook    = gv800s_muxsel,
	},
	[BTTV_BOARD_PV183] = {
		.name           = "ProVideo PV183", 
		.video_inputs   = 2,
		
		.svhs           = NO_SVHS,
		.gpiomask       = 0,
		.muxsel         = MUXSEL(2, 3),
		.gpiomux        = { 0 },
		.needs_tvaudio  = 0,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_TVT_TD3116] = {
		.name           = "Tongwei Video Technology TD-3116",
		.video_inputs   = 16,
		.gpiomask       = 0xc00ff,
		.muxsel         = MUXSEL(2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2),
		.muxsel_hook    = td3116_muxsel,
		.svhs           = NO_SVHS,
		.pll		= PLL_28,
		.tuner_type     = TUNER_ABSENT,
	},
};

static const unsigned int bttv_num_tvcards = ARRAY_SIZE(bttv_tvcards);


static unsigned char eeprom_data[256];

void __devinit bttv_idcard(struct bttv *btv)
{
	unsigned int gpiobits;
	int i,type;

	
	btv->cardid  = btv->c.pci->subsystem_device << 16;
	btv->cardid |= btv->c.pci->subsystem_vendor;

	if (0 != btv->cardid && 0xffffffff != btv->cardid) {
		
		for (type = -1, i = 0; cards[i].id != 0; i++)
			if (cards[i].id  == btv->cardid)
				type = i;

		if (type != -1) {
			
			pr_info("%d: detected: %s [card=%d], PCI subsystem ID is %04x:%04x\n",
				btv->c.nr, cards[type].name, cards[type].cardnr,
				btv->cardid & 0xffff,
				(btv->cardid >> 16) & 0xffff);
			btv->c.type = cards[type].cardnr;
		} else {
			
			pr_info("%d: subsystem: %04x:%04x (UNKNOWN)\n",
				btv->c.nr, btv->cardid & 0xffff,
				(btv->cardid >> 16) & 0xffff);
			pr_debug("please mail id, board name and the correct card= insmod option to linux-media@vger.kernel.org\n");
		}
	}

	
	if (card[btv->c.nr] < bttv_num_tvcards)
		btv->c.type=card[btv->c.nr];

	
	pr_info("%d: using: %s [card=%d,%s]\n",
		btv->c.nr, bttv_tvcards[btv->c.type].name, btv->c.type,
		card[btv->c.nr] < bttv_num_tvcards
		? "insmod option" : "autodetected");

	
	if (UNSET == audioall && UNSET == audiomux[0])
		return;

	if (UNSET != audiomux[0]) {
		gpiobits = 0;
		for (i = 0; i < ARRAY_SIZE(bttv_tvcards->gpiomux); i++) {
			bttv_tvcards[btv->c.type].gpiomux[i] = audiomux[i];
			gpiobits |= audiomux[i];
		}
	} else {
		gpiobits = audioall;
		for (i = 0; i < ARRAY_SIZE(bttv_tvcards->gpiomux); i++) {
			bttv_tvcards[btv->c.type].gpiomux[i] = audioall;
		}
	}
	bttv_tvcards[btv->c.type].gpiomask = (UNSET != gpiomask) ? gpiomask : gpiobits;
	pr_info("%d: gpio config override: mask=0x%x, mux=",
		btv->c.nr, bttv_tvcards[btv->c.type].gpiomask);
	for (i = 0; i < ARRAY_SIZE(bttv_tvcards->gpiomux); i++) {
		pr_cont("%s0x%x",
			i ? "," : "", bttv_tvcards[btv->c.type].gpiomux[i]);
	}
	pr_cont("\n");
}


static void identify_by_eeprom(struct bttv *btv, unsigned char eeprom_data[256])
{
	int type = -1;

	if (0 == strncmp(eeprom_data,"GET MM20xPCTV",13))
		type = BTTV_BOARD_MODTEC_205;
	else if (0 == strncmp(eeprom_data+20,"Picolo",7))
		type = BTTV_BOARD_EURESYS_PICOLO;
	else if (eeprom_data[0] == 0x84 && eeprom_data[2]== 0)
		type = BTTV_BOARD_HAUPPAUGE; 

	if (-1 != type) {
		btv->c.type = type;
		pr_info("%d: detected by eeprom: %s [card=%d]\n",
			btv->c.nr, bttv_tvcards[btv->c.type].name, btv->c.type);
	}
}

static void flyvideo_gpio(struct bttv *btv)
{
	int gpio, has_remote, has_radio, is_capture_only;
	int is_lr90, has_tda9820_tda9821;
	int tuner_type = UNSET, ttype;

	gpio_inout(0xffffff, 0);
	udelay(8);  
	gpio = gpio_read();
	

	ttype = (gpio & 0x0f0000) >> 16;
	switch (ttype) {
	case 0x0:
		tuner_type = 2;  
		break;
	case 0x2:
		tuner_type = 39; 
		break;
	case 0x4:
		tuner_type = 5;  
		break;
	case 0x6:
		tuner_type = 37; 
		break;
	case 0xC:
		tuner_type = 3;  
		break;
	default:
		pr_info("%d: FlyVideo_gpio: unknown tuner type\n", btv->c.nr);
		break;
	}

	has_remote          =   gpio & 0x800000;
	has_radio	    =   gpio & 0x400000;
	is_capture_only     = !(gpio & 0x008000); 
	has_tda9820_tda9821 = !(gpio & 0x004000);
	is_lr90             = !(gpio & 0x002000); 

	if (is_capture_only)
		tuner_type = TUNER_ABSENT; 

	pr_info("%d: FlyVideo Radio=%s RemoteControl=%s Tuner=%d gpio=0x%06x\n",
		btv->c.nr, has_radio ? "yes" : "no",
		has_remote ? "yes" : "no", tuner_type, gpio);
	pr_info("%d: FlyVideo  LR90=%s tda9821/tda9820=%s capture_only=%s\n",
		btv->c.nr, is_lr90 ? "yes" : "no",
		has_tda9820_tda9821 ? "yes" : "no",
		is_capture_only ? "yes" : "no");

	if (tuner_type != UNSET) 
		btv->tuner_type = tuner_type;
	btv->has_radio = has_radio;

	if (has_tda9820_tda9821)
		btv->audio_mode_gpio = lt9415_audio;
	
}

static int miro_tunermap[] = { 0,6,2,3,   4,5,6,0,  3,0,4,5,  5,2,16,1,
			       14,2,17,1, 4,1,4,3,  1,2,16,1, 4,4,4,4 };
static int miro_fmtuner[]  = { 0,0,0,0,   0,0,0,0,  0,0,0,0,  0,0,0,1,
			       1,1,1,1,   1,1,1,0,  0,0,0,0,  0,1,0,0 };

static void miro_pinnacle_gpio(struct bttv *btv)
{
	int id,msp,gpio;
	char *info;

	gpio_inout(0xffffff, 0);
	gpio = gpio_read();
	id   = ((gpio>>10) & 63) -1;
	msp  = bttv_I2CRead(btv, I2C_ADDR_MSP3400, "MSP34xx");
	if (id < 32) {
		btv->tuner_type = miro_tunermap[id];
		if (0 == (gpio & 0x20)) {
			btv->has_radio = 1;
			if (!miro_fmtuner[id]) {
				btv->has_matchbox = 1;
				btv->mbox_we    = (1<<6);
				btv->mbox_most  = (1<<7);
				btv->mbox_clk   = (1<<8);
				btv->mbox_data  = (1<<9);
				btv->mbox_mask  = (1<<6)|(1<<7)|(1<<8)|(1<<9);
			}
		} else {
			btv->has_radio = 0;
		}
		if (-1 != msp) {
			if (btv->c.type == BTTV_BOARD_MIRO)
				btv->c.type = BTTV_BOARD_MIROPRO;
			if (btv->c.type == BTTV_BOARD_PINNACLE)
				btv->c.type = BTTV_BOARD_PINNACLEPRO;
		}
		pr_info("%d: miro: id=%d tuner=%d radio=%s stereo=%s\n",
			btv->c.nr, id+1, btv->tuner_type,
			!btv->has_radio ? "no" :
			(btv->has_matchbox ? "matchbox" : "fmtuner"),
			(-1 == msp) ? "no" : "yes");
	} else {
		
		id = 63 - id;
		btv->has_radio = 0;
		switch (id) {
		case 1:
			info = "PAL / mono";
			btv->tda9887_conf = TDA9887_INTERCARRIER;
			break;
		case 2:
			info = "PAL+SECAM / stereo";
			btv->has_radio = 1;
			btv->tda9887_conf = TDA9887_QSS;
			break;
		case 3:
			info = "NTSC / stereo";
			btv->has_radio = 1;
			btv->tda9887_conf = TDA9887_QSS;
			break;
		case 4:
			info = "PAL+SECAM / mono";
			btv->tda9887_conf = TDA9887_QSS;
			break;
		case 5:
			info = "NTSC / mono";
			btv->tda9887_conf = TDA9887_INTERCARRIER;
			break;
		case 6:
			info = "NTSC / stereo";
			btv->tda9887_conf = TDA9887_INTERCARRIER;
			break;
		case 7:
			info = "PAL / stereo";
			btv->tda9887_conf = TDA9887_INTERCARRIER;
			break;
		default:
			info = "oops: unknown card";
			break;
		}
		if (-1 != msp)
			btv->c.type = BTTV_BOARD_PINNACLEPRO;
		pr_info("%d: pinnacle/mt: id=%d info=\"%s\" radio=%s\n",
			btv->c.nr, id, info, btv->has_radio ? "yes" : "no");
		btv->tuner_type = TUNER_MT2032;
	}
}

#define LM1882_SYNC_DRIVE     0x200000L

static void init_ids_eagle(struct bttv *btv)
{
	gpio_inout(0xffffff,0xFFFF37);
	gpio_write(0x200020);

	
	gpio_write(0x200024);

	
	gpio_bits(LM1882_SYNC_DRIVE,LM1882_SYNC_DRIVE);

	
	btaor((2)<<5, ~(2<<5), BT848_IFORM);
}

static void eagle_muxsel(struct bttv *btv, unsigned int input)
{
	gpio_bits(3, input & 3);

	
	
	btor(BT848_ADC_C_SLEEP, BT848_ADC);
	
	btand(~BT848_CONTROL_COMP, BT848_E_CONTROL);
	btand(~BT848_CONTROL_COMP, BT848_O_CONTROL);

	
	gpio_bits(LM1882_SYNC_DRIVE,LM1882_SYNC_DRIVE);
}

static void gvc1100_muxsel(struct bttv *btv, unsigned int input)
{
	static const int masks[] = {0x30, 0x01, 0x12, 0x23};
	gpio_write(masks[input%4]);
}


static void init_lmlbt4x(struct bttv *btv)
{
	pr_debug("LMLBT4x init\n");
	btwrite(0x000000, BT848_GPIO_REG_INP);
	gpio_inout(0xffffff, 0x0006C0);
	gpio_write(0x000000);
}

static void sigmaSQ_muxsel(struct bttv *btv, unsigned int input)
{
	unsigned int inmux = input % 8;
	gpio_inout( 0xf, 0xf );
	gpio_bits( 0xf, inmux );
}

static void sigmaSLC_muxsel(struct bttv *btv, unsigned int input)
{
	unsigned int inmux = input % 4;
	gpio_inout( 3<<9, 3<<9 );
	gpio_bits( 3<<9, inmux<<9 );
}

static void geovision_muxsel(struct bttv *btv, unsigned int input)
{
	unsigned int inmux = input % 16;
	gpio_inout(0xf, 0xf);
	gpio_bits(0xf, inmux);
}


static void td3116_latch_value(struct bttv *btv, u32 value)
{
	gpio_bits((1<<18) | 0xff, value);
	gpio_bits((1<<18) | 0xff, (1<<18) | value);
	udelay(1);
	gpio_bits((1<<18) | 0xff, value);
}

static void td3116_muxsel(struct bttv *btv, unsigned int input)
{
	u32 value;
	u32 highbit;

	highbit = (input & 0x8) >> 3 ;

	
	value = 0x11; 
	value |= ((input & 0x7) << 1)  << (4 * highbit);
	td3116_latch_value(btv, value);

	
	value &= ~0x11;
	value |= ((highbit ^ 0x1) << 4) | highbit;
	td3116_latch_value(btv, value);
}


static void bttv_reset_audio(struct bttv *btv)
{
	if (btv->id != 878)
		return;

	if (bttv_debug)
		pr_debug("%d: BT878A ARESET\n", btv->c.nr);
	btwrite((1<<7), 0x058);
	udelay(10);
	btwrite(     0, 0x058);
}

void __devinit bttv_init_card1(struct bttv *btv)
{
	switch (btv->c.type) {
	case BTTV_BOARD_HAUPPAUGE:
	case BTTV_BOARD_HAUPPAUGE878:
		boot_msp34xx(btv,5);
		break;
	case BTTV_BOARD_VOODOOTV_200:
	case BTTV_BOARD_VOODOOTV_FM:
		boot_msp34xx(btv,20);
		break;
	case BTTV_BOARD_AVERMEDIA98:
		boot_msp34xx(btv,11);
		break;
	case BTTV_BOARD_HAUPPAUGEPVR:
		pvr_boot(btv);
		break;
	case BTTV_BOARD_TWINHAN_DST:
	case BTTV_BOARD_AVDVBT_771:
	case BTTV_BOARD_PINNACLESAT:
		btv->use_i2c_hw = 1;
		break;
	case BTTV_BOARD_ADLINK_RTV24:
		init_RTV24( btv );
		break;

	}
	if (!bttv_tvcards[btv->c.type].has_dvb)
		bttv_reset_audio(btv);
}

void __devinit bttv_init_card2(struct bttv *btv)
{
	btv->tuner_type = UNSET;

	if (BTTV_BOARD_UNKNOWN == btv->c.type) {
		bttv_readee(btv,eeprom_data,0xa0);
		identify_by_eeprom(btv,eeprom_data);
	}

	switch (btv->c.type) {
	case BTTV_BOARD_MIRO:
	case BTTV_BOARD_MIROPRO:
	case BTTV_BOARD_PINNACLE:
	case BTTV_BOARD_PINNACLEPRO:
		
		miro_pinnacle_gpio(btv);
		break;
	case BTTV_BOARD_FLYVIDEO_98:
	case BTTV_BOARD_MAXI:
	case BTTV_BOARD_LIFE_FLYKIT:
	case BTTV_BOARD_FLYVIDEO:
	case BTTV_BOARD_TYPHOON_TVIEW:
	case BTTV_BOARD_CHRONOS_VS2:
	case BTTV_BOARD_FLYVIDEO_98FM:
	case BTTV_BOARD_FLYVIDEO2000:
	case BTTV_BOARD_FLYVIDEO98EZ:
	case BTTV_BOARD_CONFERENCETV:
	case BTTV_BOARD_LIFETEC_9415:
		flyvideo_gpio(btv);
		break;
	case BTTV_BOARD_HAUPPAUGE:
	case BTTV_BOARD_HAUPPAUGE878:
	case BTTV_BOARD_HAUPPAUGEPVR:
		
		bttv_readee(btv,eeprom_data,0xa0);
		hauppauge_eeprom(btv);
		break;
	case BTTV_BOARD_AVERMEDIA98:
	case BTTV_BOARD_AVPHONE98:
		bttv_readee(btv,eeprom_data,0xa0);
		avermedia_eeprom(btv);
		break;
	case BTTV_BOARD_PXC200:
		init_PXC200(btv);
		break;
	case BTTV_BOARD_PICOLO_TETRA_CHIP:
		picolo_tetra_init(btv);
		break;
	case BTTV_BOARD_VHX:
		btv->has_radio    = 1;
		btv->has_matchbox = 1;
		btv->mbox_we      = 0x20;
		btv->mbox_most    = 0;
		btv->mbox_clk     = 0x08;
		btv->mbox_data    = 0x10;
		btv->mbox_mask    = 0x38;
		break;
	case BTTV_BOARD_VOBIS_BOOSTAR:
	case BTTV_BOARD_TERRATV:
		terratec_active_radio_upgrade(btv);
		break;
	case BTTV_BOARD_MAGICTVIEW061:
		if (btv->cardid == 0x3002144f) {
			btv->has_radio=1;
			pr_info("%d: radio detected by subsystem id (CPH05x)\n",
				btv->c.nr);
		}
		break;
	case BTTV_BOARD_STB2:
		if (btv->cardid == 0x3060121a) {
			btv->has_radio=0;
			btv->tuner_type=TUNER_TEMIC_NTSC;
		}
		break;
	case BTTV_BOARD_OSPREY1x0:
	case BTTV_BOARD_OSPREY1x0_848:
	case BTTV_BOARD_OSPREY101_848:
	case BTTV_BOARD_OSPREY1x1:
	case BTTV_BOARD_OSPREY1x1_SVID:
	case BTTV_BOARD_OSPREY2xx:
	case BTTV_BOARD_OSPREY2x0_SVID:
	case BTTV_BOARD_OSPREY2x0:
	case BTTV_BOARD_OSPREY440:
	case BTTV_BOARD_OSPREY500:
	case BTTV_BOARD_OSPREY540:
	case BTTV_BOARD_OSPREY2000:
		bttv_readee(btv,eeprom_data,0xa0);
		osprey_eeprom(btv, eeprom_data);
		break;
	case BTTV_BOARD_IDS_EAGLE:
		init_ids_eagle(btv);
		break;
	case BTTV_BOARD_MODTEC_205:
		bttv_readee(btv,eeprom_data,0xa0);
		modtec_eeprom(btv);
		break;
	case BTTV_BOARD_LMLBT4:
		init_lmlbt4x(btv);
		break;
	case BTTV_BOARD_TIBET_CS16:
		tibetCS16_init(btv);
		break;
	case BTTV_BOARD_KODICOM_4400R:
		kodicom4400r_init(btv);
		break;
	case BTTV_BOARD_GEOVISION_GV800S:
		gv800s_init(btv);
		break;
	}

	
	if (!(btv->id==848 && btv->revision==0x11)) {
		
		if (PLL_28 == bttv_tvcards[btv->c.type].pll) {
			btv->pll.pll_ifreq=28636363;
			btv->pll.pll_crystal=BT848_IFORM_XT0;
		}
		if (PLL_35 == bttv_tvcards[btv->c.type].pll) {
			btv->pll.pll_ifreq=35468950;
			btv->pll.pll_crystal=BT848_IFORM_XT1;
		}
		
		switch (pll[btv->c.nr]) {
		case 0: 
			btv->pll.pll_crystal = 0;
			btv->pll.pll_ifreq   = 0;
			btv->pll.pll_ofreq   = 0;
			break;
		case 1: 
		case 28:
			btv->pll.pll_ifreq   = 28636363;
			btv->pll.pll_ofreq   = 0;
			btv->pll.pll_crystal = BT848_IFORM_XT0;
			break;
		case 2: 
		case 35:
			btv->pll.pll_ifreq   = 35468950;
			btv->pll.pll_ofreq   = 0;
			btv->pll.pll_crystal = BT848_IFORM_XT1;
			break;
		}
	}
	btv->pll.pll_current = -1;

	
	if (UNSET != bttv_tvcards[btv->c.type].tuner_type)
		if (UNSET == btv->tuner_type)
			btv->tuner_type = bttv_tvcards[btv->c.type].tuner_type;
	if (UNSET != tuner[btv->c.nr])
		btv->tuner_type = tuner[btv->c.nr];

	if (btv->tuner_type == TUNER_ABSENT)
		pr_info("%d: tuner absent\n", btv->c.nr);
	else if (btv->tuner_type == UNSET)
		pr_warn("%d: tuner type unset\n", btv->c.nr);
	else
		pr_info("%d: tuner type=%d\n", btv->c.nr, btv->tuner_type);

	if (autoload != UNSET) {
		pr_warn("%d: the autoload option is obsolete\n", btv->c.nr);
		pr_warn("%d: use option msp3400, tda7432 or tvaudio to override which audio module should be used\n",
			btv->c.nr);
	}

	if (UNSET == btv->tuner_type)
		btv->tuner_type = TUNER_ABSENT;

	btv->dig = bttv_tvcards[btv->c.type].has_dig_in ?
		   bttv_tvcards[btv->c.type].video_inputs - 1 : UNSET;
	btv->svhs = bttv_tvcards[btv->c.type].svhs == NO_SVHS ?
		    UNSET : bttv_tvcards[btv->c.type].svhs;
	if (svhs[btv->c.nr] != UNSET)
		btv->svhs = svhs[btv->c.nr];
	if (remote[btv->c.nr] != UNSET)
		btv->has_remote = remote[btv->c.nr];

	if (bttv_tvcards[btv->c.type].has_radio)
		btv->has_radio = 1;
	if (bttv_tvcards[btv->c.type].has_remote)
		btv->has_remote = 1;
	if (!bttv_tvcards[btv->c.type].no_gpioirq)
		btv->gpioirq = 1;
	if (bttv_tvcards[btv->c.type].volume_gpio)
		btv->volume_gpio = bttv_tvcards[btv->c.type].volume_gpio;
	if (bttv_tvcards[btv->c.type].audio_mode_gpio)
		btv->audio_mode_gpio = bttv_tvcards[btv->c.type].audio_mode_gpio;

	if (btv->tuner_type == TUNER_ABSENT)
		return;  

	if (btv->has_saa6588 || saa6588[btv->c.nr]) {
		
		static const unsigned short addrs[] = {
			0x20 >> 1,
			0x22 >> 1,
			I2C_CLIENT_END
		};
		struct v4l2_subdev *sd;

		sd = v4l2_i2c_new_subdev(&btv->c.v4l2_dev,
			&btv->c.i2c_adap, "saa6588", 0, addrs);
		btv->has_saa6588 = (sd != NULL);
	}

	


	switch (audiodev[btv->c.nr]) {
	case -1:
		return;	

	case 0: 
		break;

	case 1: {
		
		static const unsigned short addrs[] = {
			I2C_ADDR_MSP3400 >> 1,
			I2C_ADDR_MSP3400_ALT >> 1,
			I2C_CLIENT_END
		};

		btv->sd_msp34xx = v4l2_i2c_new_subdev(&btv->c.v4l2_dev,
			&btv->c.i2c_adap, "msp3400", 0, addrs);
		if (btv->sd_msp34xx)
			return;
		goto no_audio;
	}

	case 2: {
		
		static const unsigned short addrs[] = {
			I2C_ADDR_TDA7432 >> 1,
			I2C_CLIENT_END
		};

		if (v4l2_i2c_new_subdev(&btv->c.v4l2_dev,
				&btv->c.i2c_adap, "tda7432", 0, addrs))
			return;
		goto no_audio;
	}

	case 3: {
		
		btv->sd_tvaudio = v4l2_i2c_new_subdev(&btv->c.v4l2_dev,
			&btv->c.i2c_adap, "tvaudio", 0, tvaudio_addrs());
		if (btv->sd_tvaudio)
			return;
		goto no_audio;
	}

	default:
		pr_warn("%d: unknown audiodev value!\n", btv->c.nr);
		return;
	}


	if (!bttv_tvcards[btv->c.type].no_msp34xx) {
		btv->sd_msp34xx = v4l2_i2c_new_subdev(&btv->c.v4l2_dev,
			&btv->c.i2c_adap, "msp3400",
			0, I2C_ADDRS(I2C_ADDR_MSP3400 >> 1));
	} else if (bttv_tvcards[btv->c.type].msp34xx_alt) {
		btv->sd_msp34xx = v4l2_i2c_new_subdev(&btv->c.v4l2_dev,
			&btv->c.i2c_adap, "msp3400",
			0, I2C_ADDRS(I2C_ADDR_MSP3400_ALT >> 1));
	}

	
	if (btv->sd_msp34xx)
		return;

	
	if (!bttv_tvcards[btv->c.type].no_tda7432) {
		static const unsigned short addrs[] = {
			I2C_ADDR_TDA7432 >> 1,
			I2C_CLIENT_END
		};

		if (v4l2_i2c_new_subdev(&btv->c.v4l2_dev,
				&btv->c.i2c_adap, "tda7432", 0, addrs))
			return;
	}

	
	btv->sd_tvaudio = v4l2_i2c_new_subdev(&btv->c.v4l2_dev,
		&btv->c.i2c_adap, "tvaudio", 0, tvaudio_addrs());
	if (btv->sd_tvaudio)
		return;

no_audio:
	pr_warn("%d: audio absent, no audio device found!\n", btv->c.nr);
}


void __devinit bttv_init_tuner(struct bttv *btv)
{
	int addr = ADDR_UNSET;

	if (ADDR_UNSET != bttv_tvcards[btv->c.type].tuner_addr)
		addr = bttv_tvcards[btv->c.type].tuner_addr;

	if (btv->tuner_type != TUNER_ABSENT) {
		struct tuner_setup tun_setup;

		
		if (bttv_tvcards[btv->c.type].has_radio)
			v4l2_i2c_new_subdev(&btv->c.v4l2_dev,
				&btv->c.i2c_adap, "tuner",
				0, v4l2_i2c_tuner_addrs(ADDRS_RADIO));
		v4l2_i2c_new_subdev(&btv->c.v4l2_dev,
				&btv->c.i2c_adap, "tuner",
				0, v4l2_i2c_tuner_addrs(ADDRS_DEMOD));
		v4l2_i2c_new_subdev(&btv->c.v4l2_dev,
				&btv->c.i2c_adap, "tuner",
				0, v4l2_i2c_tuner_addrs(ADDRS_TV_WITH_DEMOD));

		tun_setup.mode_mask = T_ANALOG_TV;
		tun_setup.type = btv->tuner_type;
		tun_setup.addr = addr;

		if (bttv_tvcards[btv->c.type].has_radio)
			tun_setup.mode_mask |= T_RADIO;

		bttv_call_all(btv, tuner, s_type_addr, &tun_setup);
	}

	if (btv->tda9887_conf) {
		struct v4l2_priv_tun_config tda9887_cfg;

		tda9887_cfg.tuner = TUNER_TDA9887;
		tda9887_cfg.priv = &btv->tda9887_conf;

		bttv_call_all(btv, tuner, s_config, &tda9887_cfg);
	}
}


static void modtec_eeprom(struct bttv *btv)
{
	if( strncmp(&(eeprom_data[0x1e]),"Temic 4066 FY5",14) ==0) {
		btv->tuner_type=TUNER_TEMIC_4066FY5_PAL_I;
		pr_info("%d: Modtec: Tuner autodetected by eeprom: %s\n",
			btv->c.nr, &eeprom_data[0x1e]);
	} else if (strncmp(&(eeprom_data[0x1e]),"Alps TSBB5",10) ==0) {
		btv->tuner_type=TUNER_ALPS_TSBB5_PAL_I;
		pr_info("%d: Modtec: Tuner autodetected by eeprom: %s\n",
			btv->c.nr, &eeprom_data[0x1e]);
	} else if (strncmp(&(eeprom_data[0x1e]),"Philips FM1246",14) ==0) {
		btv->tuner_type=TUNER_PHILIPS_NTSC;
		pr_info("%d: Modtec: Tuner autodetected by eeprom: %s\n",
			btv->c.nr, &eeprom_data[0x1e]);
	} else {
		pr_info("%d: Modtec: Unknown TunerString: %s\n",
			btv->c.nr, &eeprom_data[0x1e]);
	}
}

static void __devinit hauppauge_eeprom(struct bttv *btv)
{
	struct tveeprom tv;

	tveeprom_hauppauge_analog(&btv->i2c_client, &tv, eeprom_data);
	btv->tuner_type = tv.tuner_type;
	btv->has_radio  = tv.has_radio;

	pr_info("%d: Hauppauge eeprom indicates model#%d\n",
		btv->c.nr, tv.model);

	if(tv.model == 64900) {
		pr_info("%d: Switching board type from %s to %s\n",
			btv->c.nr,
			bttv_tvcards[btv->c.type].name,
			bttv_tvcards[BTTV_BOARD_HAUPPAUGE_IMPACTVCB].name);
		btv->c.type = BTTV_BOARD_HAUPPAUGE_IMPACTVCB;
	}
}

static int terratec_active_radio_upgrade(struct bttv *btv)
{
	int freq;

	btv->has_radio    = 1;
	btv->has_matchbox = 1;
	btv->mbox_we      = 0x10;
	btv->mbox_most    = 0x20;
	btv->mbox_clk     = 0x08;
	btv->mbox_data    = 0x04;
	btv->mbox_mask    = 0x3c;

	btv->mbox_iow     = 1 <<  8;
	btv->mbox_ior     = 1 <<  9;
	btv->mbox_csel    = 1 << 10;

	freq=88000/62.5;
	tea5757_write(btv, 5 * freq + 0x358); 
	if (0x1ed8 == tea5757_read(btv)) {
		pr_info("%d: Terratec Active Radio Upgrade found\n", btv->c.nr);
		btv->has_radio    = 1;
		btv->has_saa6588  = 1;
		btv->has_matchbox = 1;
	} else {
		btv->has_radio    = 0;
		btv->has_matchbox = 0;
	}
	return 0;
}



#define PVR_GPIO_DELAY		10

#define BTTV_ALT_DATA		0x000001
#define BTTV_ALT_DCLK		0x100000
#define BTTV_ALT_NCONFIG	0x800000

static int __devinit pvr_altera_load(struct bttv *btv, const u8 *micro,
				     u32 microlen)
{
	u32 n;
	u8 bits;
	int i;

	gpio_inout(0xffffff,BTTV_ALT_DATA|BTTV_ALT_DCLK|BTTV_ALT_NCONFIG);
	gpio_write(0);
	udelay(PVR_GPIO_DELAY);

	gpio_write(BTTV_ALT_NCONFIG);
	udelay(PVR_GPIO_DELAY);

	for (n = 0; n < microlen; n++) {
		bits = micro[n];
		for (i = 0 ; i < 8 ; i++) {
			gpio_bits(BTTV_ALT_DCLK,0);
			if (bits & 0x01)
				gpio_bits(BTTV_ALT_DATA,BTTV_ALT_DATA);
			else
				gpio_bits(BTTV_ALT_DATA,0);
			gpio_bits(BTTV_ALT_DCLK,BTTV_ALT_DCLK);
			bits >>= 1;
		}
	}
	gpio_bits(BTTV_ALT_DCLK,0);
	udelay(PVR_GPIO_DELAY);

	
	for (i = 0 ; i < 30 ; i++) {
		gpio_bits(BTTV_ALT_DCLK,0);
		gpio_bits(BTTV_ALT_DCLK,BTTV_ALT_DCLK);
	}
	gpio_bits(BTTV_ALT_DCLK,0);
	return 0;
}

static int __devinit pvr_boot(struct bttv *btv)
{
	const struct firmware *fw_entry;
	int rc;

	rc = request_firmware(&fw_entry, "hcwamc.rbf", &btv->c.pci->dev);
	if (rc != 0) {
		pr_warn("%d: no altera firmware [via hotplug]\n", btv->c.nr);
		return rc;
	}
	rc = pvr_altera_load(btv, fw_entry->data, fw_entry->size);
	pr_info("%d: altera firmware upload %s\n",
		btv->c.nr, (rc < 0) ? "failed" : "ok");
	release_firmware(fw_entry);
	return rc;
}


static void __devinit osprey_eeprom(struct bttv *btv, const u8 ee[256])
{
	int i;
	u32 serial = 0;
	int cardid = -1;

	
	if (btv->c.type == BTTV_BOARD_UNKNOWN) {
		
		if (!strncmp(ee, "MMAC", 4)) {
			u8 checksum = 0;
			for (i = 0; i < 21; i++)
				checksum += ee[i];
			if (checksum != ee[21])
				return;
			cardid = BTTV_BOARD_OSPREY1x0_848;
			for (i = 12; i < 21; i++)
				serial *= 10, serial += ee[i] - '0';
		}
	} else {
		unsigned short type;

		for (i = 4*16; i < 8*16; i += 16) {
			u16 checksum = ip_compute_csum(ee + i, 16);

			if ((checksum&0xff) + (checksum>>8) == 0xff)
				break;
		}
		if (i >= 8*16)
			return;
		ee += i;

		
		type = get_unaligned_be16((__be16 *)(ee+4));

		switch(type) {
		
		case 0x0004:
			cardid = BTTV_BOARD_OSPREY1x0_848;
			break;
		case 0x0005:
			cardid = BTTV_BOARD_OSPREY101_848;
			break;

		
		case 0x0012:
		case 0x0013:
			cardid = BTTV_BOARD_OSPREY1x0;
			break;
		case 0x0014:
		case 0x0015:
			cardid = BTTV_BOARD_OSPREY1x1;
			break;
		case 0x0016:
		case 0x0017:
		case 0x0020:
			cardid = BTTV_BOARD_OSPREY1x1_SVID;
			break;
		case 0x0018:
		case 0x0019:
		case 0x001E:
		case 0x001F:
			cardid = BTTV_BOARD_OSPREY2xx;
			break;
		case 0x001A:
		case 0x001B:
			cardid = BTTV_BOARD_OSPREY2x0_SVID;
			break;
		case 0x0040:
			cardid = BTTV_BOARD_OSPREY500;
			break;
		case 0x0050:
		case 0x0056:
			cardid = BTTV_BOARD_OSPREY540;
			
			break;
		case 0x0060:
		case 0x0070:
		case 0x00A0:
			cardid = BTTV_BOARD_OSPREY2x0;
			
			gpio_inout(0xffffff,0x000303);
			break;
		case 0x00D8:
			cardid = BTTV_BOARD_OSPREY440;
			break;
		default:
			
			pr_info("%d: osprey eeprom: unknown card type 0x%04x\n",
				btv->c.nr, type);
			break;
		}
		serial = get_unaligned_be32((__be32 *)(ee+6));
	}

	pr_info("%d: osprey eeprom: card=%d '%s' serial=%u\n",
		btv->c.nr, cardid,
		cardid > 0 ? bttv_tvcards[cardid].name : "Unknown", serial);

	if (cardid<0 || btv->c.type == cardid)
		return;

	
	if (card[btv->c.nr] < bttv_num_tvcards) {
		pr_warn("%d: osprey eeprom: Not overriding user specified card type\n",
			btv->c.nr);
	} else {
		pr_info("%d: osprey eeprom: Changing card type from %d to %d\n",
			btv->c.nr, btv->c.type, cardid);
		btv->c.type = cardid;
	}
}


static int tuner_0_table[] = {
	TUNER_PHILIPS_NTSC,  TUNER_PHILIPS_PAL ,
	TUNER_PHILIPS_PAL,   TUNER_PHILIPS_PAL ,
	TUNER_PHILIPS_PAL,   TUNER_PHILIPS_PAL,
	TUNER_PHILIPS_SECAM, TUNER_PHILIPS_SECAM,
	TUNER_PHILIPS_SECAM, TUNER_PHILIPS_PAL,
	TUNER_PHILIPS_FM1216ME_MK3 };

static int tuner_1_table[] = {
	TUNER_TEMIC_NTSC,  TUNER_TEMIC_PAL,
	TUNER_TEMIC_PAL,   TUNER_TEMIC_PAL,
	TUNER_TEMIC_PAL,   TUNER_TEMIC_PAL,
	TUNER_TEMIC_4012FY5, TUNER_TEMIC_4012FY5, 
	TUNER_TEMIC_4012FY5, TUNER_TEMIC_PAL};

static void __devinit avermedia_eeprom(struct bttv *btv)
{
	int tuner_make, tuner_tv_fm, tuner_format, tuner_type = 0;

	tuner_make      = (eeprom_data[0x41] & 0x7);
	tuner_tv_fm     = (eeprom_data[0x41] & 0x18) >> 3;
	tuner_format    = (eeprom_data[0x42] & 0xf0) >> 4;
	btv->has_remote = (eeprom_data[0x42] & 0x01);

	if (tuner_make == 0 || tuner_make == 2)
		if (tuner_format <= 0x0a)
			tuner_type = tuner_0_table[tuner_format];
	if (tuner_make == 1)
		if (tuner_format <= 9)
			tuner_type = tuner_1_table[tuner_format];

	if (tuner_make == 4)
		if (tuner_format == 0x09)
			tuner_type = TUNER_LG_NTSC_NEW_TAPC; 

	pr_info("%d: Avermedia eeprom[0x%02x%02x]: tuner=",
		btv->c.nr, eeprom_data[0x41], eeprom_data[0x42]);
	if (tuner_type) {
		btv->tuner_type = tuner_type;
		pr_cont("%d", tuner_type);
	} else
		pr_cont("Unknown type");
	pr_cont(" radio:%s remote control:%s\n",
	       tuner_tv_fm     ? "yes" : "no",
	       btv->has_remote ? "yes" : "no");
}

u32 bttv_tda9880_setnorm(struct bttv *btv, u32 gpiobits)
{

	if (btv->audio == TVAUDIO_INPUT_TUNER) {
		if (bttv_tvnorms[btv->tvnorm].v4l2_id & V4L2_STD_MN)
			gpiobits |= 0x10000;
		else
			gpiobits &= ~0x10000;
	}

	gpio_bits(bttv_tvcards[btv->c.type].gpiomask, gpiobits);
	return gpiobits;
}


static void __devinit boot_msp34xx(struct bttv *btv, int pin)
{
	int mask = (1 << pin);

	gpio_inout(mask,mask);
	gpio_bits(mask,0);
	mdelay(2);
	udelay(500);
	gpio_bits(mask,mask);

	if (bttv_gpio)
		bttv_gpio_tracking(btv,"msp34xx");
	if (bttv_verbose)
		pr_info("%d: Hauppauge/Voodoo msp34xx: reset line init [%d]\n",
			btv->c.nr, pin);
}


static void __devinit init_PXC200(struct bttv *btv)
{
	static int vals[] __devinitdata = { 0x08, 0x09, 0x0a, 0x0b, 0x0d, 0x0d,
					    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
					    0x00 };
	unsigned int i;
	int tmp;
	u32 val;

	
	gpio_inout(0xffffff, (1<<13));
	gpio_write(0);
	udelay(3);
	gpio_write(1<<13);
	gpio_bits(0xffffff, 0);
	if (bttv_gpio)
		bttv_gpio_tracking(btv,"pxc200");


	btwrite(BT848_ADC_RESERVED|BT848_ADC_AGC_EN, BT848_ADC);

	
	pr_info("Setting DAC reference voltage level ...\n");
	bttv_I2CWrite(btv,0x5E,0,0x80,1);

	


	pr_info("Initialising 12C508 PIC chip ...\n");

	
	val = btread(BT848_GPIO_DMA_CTL);
	val |= BT848_GPIO_DMA_CTL_GPCLKMODE;
	btwrite(val, BT848_GPIO_DMA_CTL);

	gpio_inout(0xffffff,(1<<2));
	gpio_write(0);
	udelay(10);
	gpio_write(1<<2);

	for (i = 0; i < ARRAY_SIZE(vals); i++) {
		tmp=bttv_I2CWrite(btv,0x1E,0,vals[i],1);
		if (tmp != -1) {
			pr_info("I2C Write(%2.2x) = %i\nI2C Read () = %2.2x\n\n",
			       vals[i],tmp,bttv_I2CRead(btv,0x1F,NULL));
		}
	}

	pr_info("PXC200 Initialised\n");
}



static void
init_RTV24 (struct bttv *btv)
{
	uint32_t dataRead = 0;
	long watchdog_value = 0x0E;

	pr_info("%d: Adlink RTV-24 initialisation in progress ...\n",
		btv->c.nr);

	btwrite (0x00c3feff, BT848_GPIO_OUT_EN);

	btwrite (0 + watchdog_value, BT848_GPIO_DATA);
	msleep (1);
	btwrite (0x10 + watchdog_value, BT848_GPIO_DATA);
	msleep (10);
	btwrite (0 + watchdog_value, BT848_GPIO_DATA);

	dataRead = btread (BT848_GPIO_DATA);

	if ((((dataRead >> 18) & 0x01) != 0) || (((dataRead >> 19) & 0x01) != 1)) {
		pr_info("%d: Adlink RTV-24 initialisation(1) ERROR_CPLD_Check_Failed (read %d)\n",
			btv->c.nr, dataRead);
	}

	btwrite (0x4400 + watchdog_value, BT848_GPIO_DATA);
	msleep (10);
	btwrite (0x4410 + watchdog_value, BT848_GPIO_DATA);
	msleep (1);
	btwrite (watchdog_value, BT848_GPIO_DATA);
	msleep (1);
	dataRead = btread (BT848_GPIO_DATA);

	if ((((dataRead >> 18) & 0x01) != 0) || (((dataRead >> 19) & 0x01) != 0)) {
		pr_info("%d: Adlink RTV-24 initialisation(2) ERROR_CPLD_Check_Failed (read %d)\n",
			btv->c.nr, dataRead);

		return;
	}

	pr_info("%d: Adlink RTV-24 initialisation complete\n", btv->c.nr);
}



/*
 * Copyright (c) 1999 Csaba Halasz <qgehali@uni-miskolc.hu>
 * This code is placed under the terms of the GNU General Public License
 *
 * Brutally hacked by Dan Sheridan <dan.sheridan@contact.org.uk> djs52 8/3/00
 */

static void bus_low(struct bttv *btv, int bit)
{
	if (btv->mbox_ior) {
		gpio_bits(btv->mbox_ior | btv->mbox_iow | btv->mbox_csel,
			  btv->mbox_ior | btv->mbox_iow | btv->mbox_csel);
		udelay(5);
	}

	gpio_bits(bit,0);
	udelay(5);

	if (btv->mbox_ior) {
		gpio_bits(btv->mbox_iow | btv->mbox_csel, 0);
		udelay(5);
	}
}

static void bus_high(struct bttv *btv, int bit)
{
	if (btv->mbox_ior) {
		gpio_bits(btv->mbox_ior | btv->mbox_iow | btv->mbox_csel,
			  btv->mbox_ior | btv->mbox_iow | btv->mbox_csel);
		udelay(5);
	}

	gpio_bits(bit,bit);
	udelay(5);

	if (btv->mbox_ior) {
		gpio_bits(btv->mbox_iow | btv->mbox_csel, 0);
		udelay(5);
	}
}

static int bus_in(struct bttv *btv, int bit)
{
	if (btv->mbox_ior) {
		gpio_bits(btv->mbox_ior | btv->mbox_iow | btv->mbox_csel,
			  btv->mbox_ior | btv->mbox_iow | btv->mbox_csel);
		udelay(5);

		gpio_bits(btv->mbox_iow | btv->mbox_csel, 0);
		udelay(5);
	}
	return gpio_read() & (bit);
}

#define TEA_FREQ		0:14
#define TEA_BUFFER		15:15

#define TEA_SIGNAL_STRENGTH	16:17

#define TEA_PORT1		18:18
#define TEA_PORT0		19:19

#define TEA_BAND		20:21
#define TEA_BAND_FM		0
#define TEA_BAND_MW		1
#define TEA_BAND_LW		2
#define TEA_BAND_SW		3

#define TEA_MONO		22:22
#define TEA_ALLOW_STEREO	0
#define TEA_FORCE_MONO		1

#define TEA_SEARCH_DIRECTION	23:23
#define TEA_SEARCH_DOWN		0
#define TEA_SEARCH_UP		1

#define TEA_STATUS		24:24
#define TEA_STATUS_TUNED	0
#define TEA_STATUS_SEARCHING	1

static int tea5757_read(struct bttv *btv)
{
	unsigned long timeout;
	int value = 0;
	int i;

	
	gpio_inout(btv->mbox_mask, btv->mbox_clk | btv->mbox_we);

	if (btv->mbox_ior) {
		gpio_bits(btv->mbox_ior | btv->mbox_iow | btv->mbox_csel,
			  btv->mbox_ior | btv->mbox_iow | btv->mbox_csel);
		udelay(5);
	}

	if (bttv_gpio)
		bttv_gpio_tracking(btv,"tea5757 read");

	bus_low(btv,btv->mbox_we);
	bus_low(btv,btv->mbox_clk);

	udelay(10);
	timeout= jiffies + msecs_to_jiffies(1000);

	
	while (bus_in(btv,btv->mbox_data) && time_before(jiffies, timeout))
		schedule();
	if (bus_in(btv,btv->mbox_data)) {
		pr_warn("%d: tea5757: read timeout\n", btv->c.nr);
		return -1;
	}

	dprintk("%d: tea5757:", btv->c.nr);
	for (i = 0; i < 24; i++) {
		udelay(5);
		bus_high(btv,btv->mbox_clk);
		udelay(5);
		dprintk_cont("%c",
			     bus_in(btv, btv->mbox_most) == 0 ? 'T' : '-');
		bus_low(btv,btv->mbox_clk);
		value <<= 1;
		value |= (bus_in(btv,btv->mbox_data) == 0)?0:1;  
		dprintk_cont("%c",
			     bus_in(btv, btv->mbox_most) == 0 ? 'S' : 'M');
	}
	dprintk_cont("\n");
	dprintk("%d: tea5757: read 0x%X\n", btv->c.nr, value);
	return value;
}

static int tea5757_write(struct bttv *btv, int value)
{
	int i;
	int reg = value;

	gpio_inout(btv->mbox_mask, btv->mbox_clk | btv->mbox_we | btv->mbox_data);

	if (btv->mbox_ior) {
		gpio_bits(btv->mbox_ior | btv->mbox_iow | btv->mbox_csel,
			  btv->mbox_ior | btv->mbox_iow | btv->mbox_csel);
		udelay(5);
	}
	if (bttv_gpio)
		bttv_gpio_tracking(btv,"tea5757 write");

	dprintk("%d: tea5757: write 0x%X\n", btv->c.nr, value);
	bus_low(btv,btv->mbox_clk);
	bus_high(btv,btv->mbox_we);
	for (i = 0; i < 25; i++) {
		if (reg & 0x1000000)
			bus_high(btv,btv->mbox_data);
		else
			bus_low(btv,btv->mbox_data);
		reg <<= 1;
		bus_high(btv,btv->mbox_clk);
		udelay(10);
		bus_low(btv,btv->mbox_clk);
		udelay(10);
	}
	bus_low(btv,btv->mbox_we);  
	return 0;
}

void tea5757_set_freq(struct bttv *btv, unsigned short freq)
{
	dprintk("tea5757_set_freq %d\n",freq);
	tea5757_write(btv, 5 * freq + 0x358); 
}

static void rv605_muxsel(struct bttv *btv, unsigned int input)
{
	static const u8 muxgpio[] = { 0x3, 0x1, 0x2, 0x4, 0xf, 0x7, 0xe, 0x0,
				      0xd, 0xb, 0xc, 0x6, 0x9, 0x5, 0x8, 0xa };

	gpio_bits(0x07f, muxgpio[input]);

	
	gpio_bits(0x200,0x200);
	mdelay(1);
	gpio_bits(0x200,0x000);
	mdelay(1);

	
	gpio_bits(0x480,0x480);
	mdelay(1);
	gpio_bits(0x480,0x080);
	mdelay(1);
}

static void tibetCS16_muxsel(struct bttv *btv, unsigned int input)
{
	
	gpio_bits(0x0f0000, input << 16);
}

static void tibetCS16_init(struct bttv *btv)
{
	
	gpio_inout(0xffffff, 0x0f7fff);
	gpio_write(0x0f7fff);
}


static void kodicom4400r_write(struct bttv *btv,
			       unsigned char xaddr,
			       unsigned char yaddr,
			       unsigned char data) {
	unsigned int udata;

	udata = (data << 7) | ((yaddr&3) << 4) | (xaddr&0xf);
	gpio_bits(0x1ff, udata);		
	gpio_bits(0x1ff, udata | (1 << 8));	
	gpio_bits(0x1ff, udata);		
}

static void kodicom4400r_muxsel(struct bttv *btv, unsigned int input)
{
	char *sw_status;
	int xaddr, yaddr;
	struct bttv *mctlr;
	static unsigned char map[4] = {3, 0, 2, 1};

	mctlr = master[btv->c.nr];
	if (mctlr == NULL) {	
		return;
	}
	yaddr = (btv->c.nr - mctlr->c.nr + 1) & 3; 
	yaddr = map[yaddr];
	sw_status = (char *)(&mctlr->mbox_we);
	xaddr = input & 0xf;
	
	if (sw_status[yaddr] != xaddr)
	{
		
		kodicom4400r_write(mctlr, sw_status[yaddr], yaddr, 0);
		sw_status[yaddr] = xaddr;
		kodicom4400r_write(mctlr, xaddr, yaddr, 1);
	}
}

static void kodicom4400r_init(struct bttv *btv)
{
	char *sw_status = (char *)(&btv->mbox_we);
	int ix;

	gpio_inout(0x0003ff, 0x0003ff);
	gpio_write(1 << 9);	
	gpio_write(0);
	
	for (ix = 0; ix < 4; ix++) {
		sw_status[ix] = ix;
		kodicom4400r_write(btv, ix, ix, 1);
	}
	if ((btv->c.nr<1) || (btv->c.nr>BTTV_MAX-3))
	    return;
	master[btv->c.nr-1] = btv;
	master[btv->c.nr]   = btv;
	master[btv->c.nr+1] = btv;
	master[btv->c.nr+2] = btv;
}

#define ENA0    0x01
#define ENB0    0x02
#define ENA1    0x04
#define ENB1    0x08

#define IN10    0x10
#define IN00    0x20
#define IN11    0x40
#define IN01    0x80

static void xguard_muxsel(struct bttv *btv, unsigned int input)
{
	static const int masks[] = {
		ENB0, ENB0|IN00, ENB0|IN10, ENB0|IN00|IN10,
		ENA0, ENA0|IN00, ENA0|IN10, ENA0|IN00|IN10,
		ENB1, ENB1|IN01, ENB1|IN11, ENB1|IN01|IN11,
		ENA1, ENA1|IN01, ENA1|IN11, ENA1|IN01|IN11,
	};
	gpio_write(masks[input%16]);
}
static void picolo_tetra_init(struct bttv *btv)
{
	
	btwrite (0x08<<16,BT848_GPIO_DATA);
	btwrite (0x04<<16,BT848_GPIO_DATA);
}
static void picolo_tetra_muxsel (struct bttv* btv, unsigned int input)
{

	dprintk("%d : picolo_tetra_muxsel =>  input = %d\n", btv->c.nr, input);
	
	
	btwrite (input<<20,BT848_GPIO_DATA);

}


#define I2C_TDA8540        0x90
#define I2C_TDA8540_ALT1   0x92
#define I2C_TDA8540_ALT2   0x94
#define I2C_TDA8540_ALT3   0x96
#define I2C_TDA8540_ALT4   0x98
#define I2C_TDA8540_ALT5   0x9a
#define I2C_TDA8540_ALT6   0x9c

static void ivc120_muxsel(struct bttv *btv, unsigned int input)
{
	
	int key = input % 4;
	int matrix = input / 4;

	dprintk("%d: ivc120_muxsel: Input - %02d | TDA - %02d | In - %02d\n",
		btv->c.nr, input, matrix, key);

	
	bttv_I2CWrite(btv, I2C_TDA8540_ALT3, 0x00,
		      ((matrix == 3) ? (key | key << 2) : 0x00), 1);
	bttv_I2CWrite(btv, I2C_TDA8540_ALT4, 0x00,
		      ((matrix == 0) ? (key | key << 2) : 0x00), 1);
	bttv_I2CWrite(btv, I2C_TDA8540_ALT5, 0x00,
		      ((matrix == 1) ? (key | key << 2) : 0x00), 1);
	bttv_I2CWrite(btv, I2C_TDA8540_ALT6, 0x00,
		      ((matrix == 2) ? (key | key << 2) : 0x00), 1);

	
	bttv_I2CWrite(btv, I2C_TDA8540_ALT3, 0x02,
		      ((matrix == 3) ? 0x03 : 0x00), 1);  
	bttv_I2CWrite(btv, I2C_TDA8540_ALT4, 0x02,
		      ((matrix == 0) ? 0x03 : 0x00), 1);  
	bttv_I2CWrite(btv, I2C_TDA8540_ALT5, 0x02,
		      ((matrix == 1) ? 0x03 : 0x00), 1);  
	bttv_I2CWrite(btv, I2C_TDA8540_ALT6, 0x02,
		      ((matrix == 2) ? 0x03 : 0x00), 1);  

	
}


#define PX_CFG_PXC200F 0x01
#define PX_FLAG_PXC200A  0x00001000 
#define PX_I2C_PIC       0x0f
#define PX_PXC200A_CARDID 0x200a1295
#define PX_I2C_CMD_CFG   0x00

static void PXC200_muxsel(struct bttv *btv, unsigned int input)
{
	int rc;
	long mux;
	int bitmask;
	unsigned char buf[2];

	
	
	buf[0]=0;
	buf[1]=0;
	rc=bttv_I2CWrite(btv,(PX_I2C_PIC<<1),buf[0],buf[1],1);
	if (rc) {
		pr_debug("%d: PXC200_muxsel: pic cfg write failed:%d\n",
			 btv->c.nr, rc);
	  
		return;
	}

	rc=bttv_I2CRead(btv,(PX_I2C_PIC<<1),NULL);
	if (!(rc & PX_CFG_PXC200F)) {
		pr_debug("%d: PXC200_muxsel: not PXC200F rc:%d\n",
			 btv->c.nr, rc);
		return;
	}


	
	
	
	
	mux = input;

	
	
	bitmask=0x302;
	
	if (btv->cardid == PX_PXC200A_CARDID)  {
	   bitmask ^= 0x180; 
	   bitmask |= 7<<4; 
	}
	btwrite(bitmask, BT848_GPIO_OUT_EN);

	bitmask = btread(BT848_GPIO_DATA);
	if (btv->cardid == PX_PXC200A_CARDID)
	  bitmask = (bitmask & ~0x280) | ((mux & 2) << 8) | ((mux & 1) << 7);
	else 
	  bitmask = (bitmask & ~0x300) | ((mux & 3) << 8);
	btwrite(bitmask,BT848_GPIO_DATA);

	if (btv->cardid == PX_PXC200A_CARDID)
	  btaor(2<<5, ~BT848_IFORM_MUXSEL, BT848_IFORM);
	else 
	  btand(~BT848_IFORM_MUXSEL,BT848_IFORM);

	pr_debug("%d: setting input channel to:%d\n", btv->c.nr, (int)mux);
}

static void phytec_muxsel(struct bttv *btv, unsigned int input)
{
	unsigned int mux = input % 4;

	if (input == btv->svhs)
		mux = 0;

	gpio_bits(0x3, mux);
}


static void gv800s_write(struct bttv *btv,
			 unsigned char xaddr,
			 unsigned char yaddr,
			 unsigned char data) {
	const u32 ADDRESS = ((xaddr&0xf) | (yaddr&3)<<4);
	const u32 CSELECT = 1<<16;
	const u32 STROBE = 1<<17;
	const u32 DATA = data<<18;

	gpio_bits(0x1007f, ADDRESS | CSELECT);	
	gpio_bits(0x20000, STROBE);		
	gpio_bits(0x40000, DATA);		
	gpio_bits(0x20000, ~STROBE);		
}

static void gv800s_muxsel(struct bttv *btv, unsigned int input)
{
	struct bttv *mctlr;
	char *sw_status;
	int xaddr, yaddr;
	static unsigned int map[4][4] = { { 0x0, 0x4, 0xa, 0x6 },
					  { 0x1, 0x5, 0xb, 0x7 },
					  { 0x2, 0x8, 0xc, 0xe },
					  { 0x3, 0x9, 0xd, 0xf } };
	input = input%4;
	mctlr = master[btv->c.nr];
	if (mctlr == NULL) {
		
		return;
	}
	yaddr = (btv->c.nr - mctlr->c.nr) & 3;
	sw_status = (char *)(&mctlr->mbox_we);
	xaddr = map[yaddr][input] & 0xf;

	
	if (sw_status[yaddr] != xaddr) {
		
		gv800s_write(mctlr, sw_status[yaddr], yaddr, 0);
		sw_status[yaddr] = xaddr;
		gv800s_write(mctlr, xaddr, yaddr, 1);
	}
}

static void gv800s_init(struct bttv *btv)
{
	char *sw_status = (char *)(&btv->mbox_we);
	int ix;

	gpio_inout(0xf107f, 0xf107f);
	gpio_write(1<<19); 
	gpio_write(0);

	
	for (ix = 0; ix < 4; ix++) {
		sw_status[ix] = ix;
		gv800s_write(btv, ix, ix, 1);
	}

	
	bttv_I2CWrite(btv, 0x18, 0x5, 0x90, 1);

	if (btv->c.nr > BTTV_MAX-4)
		return;
	master[btv->c.nr]   = btv;
	master[btv->c.nr+1] = btv;
	master[btv->c.nr+2] = btv;
	master[btv->c.nr+3] = btv;
}


void __init bttv_check_chipset(void)
{
	int pcipci_fail = 0;
	struct pci_dev *dev = NULL;

	if (pci_pci_problems & (PCIPCI_FAIL|PCIAGP_FAIL)) 	
		pcipci_fail = 1;
	if (pci_pci_problems & (PCIPCI_TRITON|PCIPCI_NATOMA|PCIPCI_VIAETBF))
		triton1 = 1;
	if (pci_pci_problems & PCIPCI_VSFX)
		vsfx = 1;
#ifdef PCIPCI_ALIMAGIK
	if (pci_pci_problems & PCIPCI_ALIMAGIK)
		latency = 0x0A;
#endif


	
	if (triton1)
		pr_info("Host bridge needs ETBF enabled\n");
	if (vsfx)
		pr_info("Host bridge needs VSFX enabled\n");
	if (pcipci_fail) {
		pr_info("bttv and your chipset may not work together\n");
		if (!no_overlay) {
			pr_info("overlay will be disabled\n");
			no_overlay = 1;
		} else {
			pr_info("overlay forced. Use this option at your own risk.\n");
		}
	}
	if (UNSET != latency)
		pr_info("pci latency fixup [%d]\n", latency);
	while ((dev = pci_get_device(PCI_VENDOR_ID_INTEL,
				      PCI_DEVICE_ID_INTEL_82441, dev))) {
		unsigned char b;
		pci_read_config_byte(dev, 0x53, &b);
		if (bttv_debug)
			pr_info("Host bridge: 82441FX Natoma, bufcon=0x%02x\n",
				b);
	}
}

int __devinit bttv_handle_chipset(struct bttv *btv)
{
	unsigned char command;

	if (!triton1 && !vsfx && UNSET == latency)
		return 0;

	if (bttv_verbose) {
		if (triton1)
			pr_info("%d: enabling ETBF (430FX/VP3 compatibility)\n",
				btv->c.nr);
		if (vsfx && btv->id >= 878)
			pr_info("%d: enabling VSFX\n", btv->c.nr);
		if (UNSET != latency)
			pr_info("%d: setting pci timer to %d\n",
				btv->c.nr, latency);
	}

	if (btv->id < 878) {
		
		if (triton1)
			btv->triton1 = BT848_INT_ETBF;
	} else {
		
		pci_read_config_byte(btv->c.pci, BT878_DEVCTRL, &command);
		if (triton1)
			command |= BT878_EN_TBFX;
		if (vsfx)
			command |= BT878_EN_VSFX;
		pci_write_config_byte(btv->c.pci, BT878_DEVCTRL, command);
	}
	if (UNSET != latency)
		pci_write_config_byte(btv->c.pci, PCI_LATENCY_TIMER, latency);
	return 0;
}


