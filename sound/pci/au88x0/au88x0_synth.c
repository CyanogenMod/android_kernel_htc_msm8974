/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include "au88x0.h"
#include "au88x0_wt.h"

static void vortex_fifo_setwtvalid(vortex_t * vortex, int fifo, int en);
static void vortex_connection_adb_mixin(vortex_t * vortex, int en,
					unsigned char channel,
					unsigned char source,
					unsigned char mixin);
static void vortex_connection_mixin_mix(vortex_t * vortex, int en,
					unsigned char mixin,
					unsigned char mix, int a);
static void vortex_fifo_wtinitialize(vortex_t * vortex, int fifo, int j);
static int vortex_wt_SetReg(vortex_t * vortex, unsigned char reg, int wt,
			    u32 val);


static void vortex_wt_setstereo(vortex_t * vortex, u32 wt, u32 stereo)
{
	int temp;

	
	temp = hwread(vortex->mmio, WT_STEREO(wt));
	temp = (temp & 0xfe) | (stereo & 1);
	
	hwwrite(vortex->mmio, WT_STEREO(wt), temp);
}

static void vortex_wt_setdsout(vortex_t * vortex, u32 wt, int en)
{
	int temp;

	
	temp = hwread(vortex->mmio, WT_DSREG((wt >= 0x20) ? 1 : 0));
	if (en)
		temp |= (1 << (wt & 0x1f));
	else
		temp &= (1 << ~(wt & 0x1f));
	hwwrite(vortex->mmio, WT_DSREG((wt >= 0x20) ? 1 : 0), temp);
}

static int vortex_wt_allocroute(vortex_t * vortex, int wt, int nr_ch)
{
	wt_voice_t *voice = &(vortex->wt_voice[wt]);
	int temp;

	
	if (nr_ch) {
		vortex_fifo_wtinitialize(vortex, wt, 1);
		vortex_fifo_setwtvalid(vortex, wt, 1);
		vortex_wt_setstereo(vortex, wt, nr_ch - 1);
	} else
		vortex_fifo_setwtvalid(vortex, wt, 0);
	
	
	vortex_wt_setdsout(vortex, wt, 1);
	
	hwwrite(vortex->mmio, WT_SRAMP(0), 0x880000);
	
#ifdef CHIP_AU8830
	hwwrite(vortex->mmio, WT_SRAMP(1), 0x880000);
	
#endif
	hwwrite(vortex->mmio, WT_PARM(wt, 0), 0);
	hwwrite(vortex->mmio, WT_PARM(wt, 1), 0);
	hwwrite(vortex->mmio, WT_PARM(wt, 2), 0);

	temp = hwread(vortex->mmio, WT_PARM(wt, 3));
	printk(KERN_DEBUG "vortex: WT PARM3: %x\n", temp);
	

	hwwrite(vortex->mmio, WT_DELAY(wt, 0), 0);
	hwwrite(vortex->mmio, WT_DELAY(wt, 1), 0);
	hwwrite(vortex->mmio, WT_DELAY(wt, 2), 0);
	hwwrite(vortex->mmio, WT_DELAY(wt, 3), 0);

	printk(KERN_DEBUG "vortex: WT GMODE: %x\n", hwread(vortex->mmio, WT_GMODE(wt)));

	hwwrite(vortex->mmio, WT_PARM(wt, 2), 0xffffffff);
	hwwrite(vortex->mmio, WT_PARM(wt, 3), 0xcff1c810);

	voice->parm0 = voice->parm1 = 0xcfb23e2f;
	hwwrite(vortex->mmio, WT_PARM(wt, 0), voice->parm0);
	hwwrite(vortex->mmio, WT_PARM(wt, 1), voice->parm1);
	printk(KERN_DEBUG "vortex: WT GMODE 2 : %x\n", hwread(vortex->mmio, WT_GMODE(wt)));
	return 0;
}


static void vortex_wt_connect(vortex_t * vortex, int en)
{
	int i, ii, mix;

#define NR_WTROUTES 6
#ifdef CHIP_AU8830
#define NR_WTBLOCKS 2
#else
#define NR_WTBLOCKS 1
#endif

	for (i = 0; i < NR_WTBLOCKS; i++) {
		for (ii = 0; ii < NR_WTROUTES; ii++) {
			mix =
			    vortex_adb_checkinout(vortex,
						  vortex->fixed_res, en,
						  VORTEX_RESOURCE_MIXIN);
			vortex->mixwt[(i * NR_WTROUTES) + ii] = mix;

			vortex_route(vortex, en, 0x11,
				     ADB_WTOUT(i, ii + 0x20), ADB_MIXIN(mix));

			vortex_connection_mixin_mix(vortex, en, mix,
						    vortex->mixplayb[ii % 2], 0);
			if (VORTEX_IS_QUAD(vortex))
				vortex_connection_mixin_mix(vortex, en,
							    mix,
							    vortex->mixplayb[2 +
								     (ii % 2)], 0);
		}
	}
	for (i = 0; i < NR_WT; i++) {
		hwwrite(vortex->mmio, WT_RUN(i), 1);
	}
}

#if 0
static int vortex_wt_GetReg(vortex_t * vortex, char reg, int wt)
{
	

	if (reg == 4) {
		return hwread(vortex->mmio, WT_PARM(wt, 3));
	}
	if (reg == 7) {
		return hwread(vortex->mmio, WT_GMODE(wt));
	}

	return 0;
}

static int
vortex_wt_SetReg2(vortex_t * vortex, unsigned char reg, int wt,
		  u16 val)
{
	return 1;
}

#endif
static int
vortex_wt_SetReg(vortex_t * vortex, unsigned char reg, int wt,
		 u32 val)
{
	int ecx;

	if ((reg == 5) || ((reg >= 7) && (reg <= 10)) || (reg == 0xc)) {
		if (wt >= (NR_WT / NR_WT_PB)) {
			printk
			    ("vortex: WT SetReg: bank out of range. reg=0x%x, wt=%d\n",
			     reg, wt);
			return 0;
		}
	} else {
		if (wt >= NR_WT) {
			printk(KERN_ERR "vortex: WT SetReg: voice out of range\n");
			return 0;
		}
	}
	if (reg > 0xc)
		return 0;

	switch (reg) {
		
	case 0:		
		hwwrite(vortex->mmio, WT_RUN(wt), val);
		return 0xc;
		break;
	case 1:		
		hwwrite(vortex->mmio, WT_PARM(wt, 0), val);
		return 0xc;
		break;
	case 2:		
		hwwrite(vortex->mmio, WT_PARM(wt, 1), val);
		return 0xc;
		break;
	case 3:		
		hwwrite(vortex->mmio, WT_PARM(wt, 2), val);
		return 0xc;
		break;
	case 4:		
		hwwrite(vortex->mmio, WT_PARM(wt, 3), val);
		return 0xc;
		break;
	case 6:		
		hwwrite(vortex->mmio, WT_MUTE(wt), val);
		return 0xc;
		break;
	case 0xb:
		{		
			hwwrite(vortex->mmio, WT_DELAY(wt, 3), val);
			hwwrite(vortex->mmio, WT_DELAY(wt, 2), val);
			hwwrite(vortex->mmio, WT_DELAY(wt, 1), val);
			hwwrite(vortex->mmio, WT_DELAY(wt, 0), val);
			return 0xc;
		}
		break;
		
	case 5:		
		ecx = WT_SRAMP(wt);
		break;
	case 8:		
		ecx = WT_ARAMP(wt);
		break;
	case 9:		
		ecx = WT_MRAMP(wt);
		break;
	case 0xa:		
		ecx = WT_CTRL(wt);
		break;
	case 0xc:		
		ecx = WT_DSREG(wt);
		break;
	default:
		return 0;
		break;
	}
	hwwrite(vortex->mmio, ecx, val);
	return 1;
}

static void vortex_wt_init(vortex_t * vortex)
{
	u32 var4, var8, varc, var10 = 0, edi;

	var10 &= 0xFFFFFFE3;
	var10 |= 0x22;
	var10 &= 0xFFFFFEBF;
	var10 |= 0x80;
	var10 |= 0x200;
	var10 &= 0xfffffffe;
	var10 &= 0xfffffbff;
	var10 |= 0x1800;
	
	var4 = 0x10000000;
	varc = 0x00830000;
	var8 = 0x00830000;

	
	for (edi = 0; edi < (NR_WT / NR_WT_PB); edi++) {
		vortex_wt_SetReg(vortex, 0xc, edi, 0);	
		vortex_wt_SetReg(vortex, 0xa, edi, var10);	
		vortex_wt_SetReg(vortex, 0x9, edi, var4);	
		vortex_wt_SetReg(vortex, 0x8, edi, varc);	
		vortex_wt_SetReg(vortex, 0x5, edi, var8);	
	}
	
	for (edi = 0; edi < NR_WT; edi++) {
		vortex_wt_SetReg(vortex, 0x4, edi, 0);	
		vortex_wt_SetReg(vortex, 0x3, edi, 0);	
		vortex_wt_SetReg(vortex, 0x2, edi, 0);	
		vortex_wt_SetReg(vortex, 0x1, edi, 0);	
		vortex_wt_SetReg(vortex, 0xb, edi, 0);	
	}
	var10 |= 1;
	for (edi = 0; edi < (NR_WT / NR_WT_PB); edi++)
		vortex_wt_SetReg(vortex, 0xa, edi, var10);	
}

#if 0
static void vortex_wt_SetVolume(vortex_t * vortex, int wt, int vol[])
{
	wt_voice_t *voice = &(vortex->wt_voice[wt]);
	int ecx = vol[1], eax = vol[0];

	
	voice->parm0 &= 0xff00ffff;
	voice->parm0 |= (vol[0] & 0xff) << 0x10;
	voice->parm1 &= 0xff00ffff;
	voice->parm1 |= (vol[1] & 0xff) << 0x10;

	
	hwwrite(vortex, WT_PARM(wt, 0), voice->parm0);
	hwwrite(vortex, WT_PARM(wt, 1), voice->parm0);

	if (voice->this_1D0 & 4) {
		eax >>= 8;
		ecx = eax;
		if (ecx < 0x80)
			ecx = 0x7f;
		voice->parm3 &= 0xFFFFC07F;
		voice->parm3 |= (ecx & 0x7f) << 7;
		voice->parm3 &= 0xFFFFFF80;
		voice->parm3 |= (eax & 0x7f);
	} else {
		voice->parm3 &= 0xFFE03FFF;
		voice->parm3 |= (eax & 0xFE00) << 5;
	}

	hwwrite(vortex, WT_PARM(wt, 3), voice->parm3);
}

static void vortex_wt_SetFrequency(vortex_t * vortex, int wt, unsigned int sr)
{
	wt_voice_t *voice = &(vortex->wt_voice[wt]);
	u32 eax, edx;

	
	eax = ((sr << 0xf) * 0x57619F1) & 0xffffffff;
	edx = (((sr << 0xf) * 0x57619F1)) >> 0x20;

	edx >>= 0xa;
	edx <<= 1;
	if (edx) {
		if (edx & 0x0FFF80000)
			eax = 0x7fff;
		else {
			edx <<= 0xd;
			eax = 7;
			while ((edx & 0x80000000) == 0) {
				edx <<= 1;
				eax--;
				if (eax == 0)
					break;
			}
			if (eax)
				edx <<= 1;
			eax <<= 0xc;
			edx >>= 0x14;
			eax |= edx;
		}
	} else
		eax = 0;
	voice->parm0 &= 0xffff0001;
	voice->parm0 |= (eax & 0x7fff) << 1;
	voice->parm1 = voice->parm0 | 1;
	
	
	
	hwwrite(vortex->mmio, WT_PARM(wt, 0), voice->parm0);
	hwwrite(vortex->mmio, WT_PARM(wt, 1), voice->parm1);
}
#endif

