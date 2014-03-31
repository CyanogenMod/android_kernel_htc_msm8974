/*
 * drxk_hard: DRX-K DVB-C/T demodulator driver
 *
 * Copyright (C) 2010-2011 Digital Devices GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 only, as published by the Free Software Foundation.
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <asm/div64.h>

#include "dvb_frontend.h"
#include "drxk.h"
#include "drxk_hard.h"

static int PowerDownDVBT(struct drxk_state *state, bool setPowerMode);
static int PowerDownQAM(struct drxk_state *state);
static int SetDVBTStandard(struct drxk_state *state,
			   enum OperationMode oMode);
static int SetQAMStandard(struct drxk_state *state,
			  enum OperationMode oMode);
static int SetQAM(struct drxk_state *state, u16 IntermediateFreqkHz,
		  s32 tunerFreqOffset);
static int SetDVBTStandard(struct drxk_state *state,
			   enum OperationMode oMode);
static int DVBTStart(struct drxk_state *state);
static int SetDVBT(struct drxk_state *state, u16 IntermediateFreqkHz,
		   s32 tunerFreqOffset);
static int GetQAMLockStatus(struct drxk_state *state, u32 *pLockStatus);
static int GetDVBTLockStatus(struct drxk_state *state, u32 *pLockStatus);
static int SwitchAntennaToQAM(struct drxk_state *state);
static int SwitchAntennaToDVBT(struct drxk_state *state);

static bool IsDVBT(struct drxk_state *state)
{
	return state->m_OperationMode == OM_DVBT;
}

static bool IsQAM(struct drxk_state *state)
{
	return state->m_OperationMode == OM_QAM_ITU_A ||
	    state->m_OperationMode == OM_QAM_ITU_B ||
	    state->m_OperationMode == OM_QAM_ITU_C;
}

bool IsA1WithPatchCode(struct drxk_state *state)
{
	return state->m_DRXK_A1_PATCH_CODE;
}

bool IsA1WithRomCode(struct drxk_state *state)
{
	return state->m_DRXK_A1_ROM_CODE;
}

#define NOA1ROM 0

#define DRXDAP_FASI_SHORT_FORMAT(addr) (((addr) & 0xFC30FF80) == 0)
#define DRXDAP_FASI_LONG_FORMAT(addr)  (((addr) & 0xFC30FF80) != 0)

#define DEFAULT_MER_83  165
#define DEFAULT_MER_93  250

#ifndef DRXK_MPEG_SERIAL_OUTPUT_PIN_DRIVE_STRENGTH
#define DRXK_MPEG_SERIAL_OUTPUT_PIN_DRIVE_STRENGTH (0x02)
#endif

#ifndef DRXK_MPEG_PARALLEL_OUTPUT_PIN_DRIVE_STRENGTH
#define DRXK_MPEG_PARALLEL_OUTPUT_PIN_DRIVE_STRENGTH (0x03)
#endif

#define DEFAULT_DRXK_MPEG_LOCK_TIMEOUT 700
#define DEFAULT_DRXK_DEMOD_LOCK_TIMEOUT 500

#ifndef DRXK_KI_RAGC_ATV
#define DRXK_KI_RAGC_ATV   4
#endif
#ifndef DRXK_KI_IAGC_ATV
#define DRXK_KI_IAGC_ATV   6
#endif
#ifndef DRXK_KI_DAGC_ATV
#define DRXK_KI_DAGC_ATV   7
#endif

#ifndef DRXK_KI_RAGC_QAM
#define DRXK_KI_RAGC_QAM   3
#endif
#ifndef DRXK_KI_IAGC_QAM
#define DRXK_KI_IAGC_QAM   4
#endif
#ifndef DRXK_KI_DAGC_QAM
#define DRXK_KI_DAGC_QAM   7
#endif
#ifndef DRXK_KI_RAGC_DVBT
#define DRXK_KI_RAGC_DVBT  (IsA1WithPatchCode(state) ? 3 : 2)
#endif
#ifndef DRXK_KI_IAGC_DVBT
#define DRXK_KI_IAGC_DVBT  (IsA1WithPatchCode(state) ? 4 : 2)
#endif
#ifndef DRXK_KI_DAGC_DVBT
#define DRXK_KI_DAGC_DVBT  (IsA1WithPatchCode(state) ? 10 : 7)
#endif

#ifndef DRXK_AGC_DAC_OFFSET
#define DRXK_AGC_DAC_OFFSET (0x800)
#endif

#ifndef DRXK_BANDWIDTH_8MHZ_IN_HZ
#define DRXK_BANDWIDTH_8MHZ_IN_HZ  (0x8B8249L)
#endif

#ifndef DRXK_BANDWIDTH_7MHZ_IN_HZ
#define DRXK_BANDWIDTH_7MHZ_IN_HZ  (0x7A1200L)
#endif

#ifndef DRXK_BANDWIDTH_6MHZ_IN_HZ
#define DRXK_BANDWIDTH_6MHZ_IN_HZ  (0x68A1B6L)
#endif

#ifndef DRXK_QAM_SYMBOLRATE_MAX
#define DRXK_QAM_SYMBOLRATE_MAX         (7233000)
#endif

#define DRXK_BL_ROM_OFFSET_TAPS_DVBT    56
#define DRXK_BL_ROM_OFFSET_TAPS_ITU_A   64
#define DRXK_BL_ROM_OFFSET_TAPS_ITU_C   0x5FE0
#define DRXK_BL_ROM_OFFSET_TAPS_BG      24
#define DRXK_BL_ROM_OFFSET_TAPS_DKILLP  32
#define DRXK_BL_ROM_OFFSET_TAPS_NTSC    40
#define DRXK_BL_ROM_OFFSET_TAPS_FM      48
#define DRXK_BL_ROM_OFFSET_UCODE        0

#define DRXK_BLC_TIMEOUT                100

#define DRXK_BLCC_NR_ELEMENTS_TAPS      2
#define DRXK_BLCC_NR_ELEMENTS_UCODE     6

#define DRXK_BLDC_NR_ELEMENTS_TAPS      28

#ifndef DRXK_OFDM_NE_NOTCH_WIDTH
#define DRXK_OFDM_NE_NOTCH_WIDTH             (4)
#endif

#define DRXK_QAM_SL_SIG_POWER_QAM16       (40960)
#define DRXK_QAM_SL_SIG_POWER_QAM32       (20480)
#define DRXK_QAM_SL_SIG_POWER_QAM64       (43008)
#define DRXK_QAM_SL_SIG_POWER_QAM128      (20992)
#define DRXK_QAM_SL_SIG_POWER_QAM256      (43520)

static unsigned int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "enable debug messages");

#define dprintk(level, fmt, arg...) do {			\
if (debug >= level)						\
	printk(KERN_DEBUG "drxk: %s" fmt, __func__, ## arg);	\
} while (0)


static inline u32 MulDiv32(u32 a, u32 b, u32 c)
{
	u64 tmp64;

	tmp64 = (u64) a * (u64) b;
	do_div(tmp64, c);

	return (u32) tmp64;
}

inline u32 Frac28a(u32 a, u32 c)
{
	int i = 0;
	u32 Q1 = 0;
	u32 R0 = 0;

	R0 = (a % c) << 4;	
	Q1 = a / c;		

	
	for (i = 0; i < 7; i++) {
		Q1 = (Q1 << 4) | (R0 / c);
		R0 = (R0 % c) << 4;
	}
	
	if ((R0 >> 3) >= c)
		Q1++;

	return Q1;
}

static u32 Log10Times100(u32 x)
{
	static const u8 scale = 15;
	static const u8 indexWidth = 5;
	u8 i = 0;
	u32 y = 0;
	u32 d = 0;
	u32 k = 0;
	u32 r = 0;

	static const u32 log2lut[] = {
		0,		
		290941,		
		573196,		
		847269,		
		1113620,	
		1372674,	
		1624818,	
		1870412,	
		2109788,	
		2343253,	
		2571091,	
		2793569,	
		3010931,	
		3223408,	
		3431216,	
		3634553,	
		3833610,	
		4028562,	
		4219576,	
		4406807,	
		4590402,	
		4770499,	
		4947231,	
		5120719,	
		5291081,	
		5458428,	
		5622864,	
		5784489,	
		5943398,	
		6099680,	
		6253421,	
		6404702,	
		6553600,	
	};


	if (x == 0)
		return 0;

	
	
	if ((x & ((0xffffffff) << (scale + 1))) == 0) {
		for (k = scale; k > 0; k--) {
			if (x & (((u32) 1) << scale))
				break;
			x <<= 1;
		}
	} else {
		for (k = scale; k < 31; k++) {
			if ((x & (((u32) (-1)) << (scale + 1))) == 0)
				break;
			x >>= 1;
		}
	}

	
	y = k * ((((u32) 1) << scale) * 200);

	
	x &= ((((u32) 1) << scale) - 1);
	
	i = (u8) (x >> (scale - indexWidth));
	
	d = x & ((((u32) 1) << (scale - indexWidth)) - 1);
	
	y += log2lut[i] +
	    ((d * (log2lut[i + 1] - log2lut[i])) >> (scale - indexWidth));
	
	y /= 108853;		
	r = (y >> 1);
	
	if (y & ((u32) 1))
		r++;
	return r;
}


static int i2c_read1(struct i2c_adapter *adapter, u8 adr, u8 *val)
{
	struct i2c_msg msgs[1] = { {.addr = adr, .flags = I2C_M_RD,
				    .buf = val, .len = 1}
	};

	return i2c_transfer(adapter, msgs, 1);
}

static int i2c_write(struct i2c_adapter *adap, u8 adr, u8 *data, int len)
{
	int status;
	struct i2c_msg msg = {
	    .addr = adr, .flags = 0, .buf = data, .len = len };

	dprintk(3, ":");
	if (debug > 2) {
		int i;
		for (i = 0; i < len; i++)
			printk(KERN_CONT " %02x", data[i]);
		printk(KERN_CONT "\n");
	}
	status = i2c_transfer(adap, &msg, 1);
	if (status >= 0 && status != 1)
		status = -EIO;

	if (status < 0)
		printk(KERN_ERR "drxk: i2c write error at addr 0x%02x\n", adr);

	return status;
}

static int i2c_read(struct i2c_adapter *adap,
		    u8 adr, u8 *msg, int len, u8 *answ, int alen)
{
	int status;
	struct i2c_msg msgs[2] = {
		{.addr = adr, .flags = 0,
				    .buf = msg, .len = len},
		{.addr = adr, .flags = I2C_M_RD,
		 .buf = answ, .len = alen}
	};

	status = i2c_transfer(adap, msgs, 2);
	if (status != 2) {
		if (debug > 2)
			printk(KERN_CONT ": ERROR!\n");
		if (status >= 0)
			status = -EIO;

		printk(KERN_ERR "drxk: i2c read error at addr 0x%02x\n", adr);
		return status;
	}
	if (debug > 2) {
		int i;
		dprintk(2, ": read from");
		for (i = 0; i < len; i++)
			printk(KERN_CONT " %02x", msg[i]);
		printk(KERN_CONT ", value = ");
		for (i = 0; i < alen; i++)
			printk(KERN_CONT " %02x", answ[i]);
		printk(KERN_CONT "\n");
	}
	return 0;
}

static int read16_flags(struct drxk_state *state, u32 reg, u16 *data, u8 flags)
{
	int status;
	u8 adr = state->demod_address, mm1[4], mm2[2], len;

	if (state->single_master)
		flags |= 0xC0;

	if (DRXDAP_FASI_LONG_FORMAT(reg) || (flags != 0)) {
		mm1[0] = (((reg << 1) & 0xFF) | 0x01);
		mm1[1] = ((reg >> 16) & 0xFF);
		mm1[2] = ((reg >> 24) & 0xFF) | flags;
		mm1[3] = ((reg >> 7) & 0xFF);
		len = 4;
	} else {
		mm1[0] = ((reg << 1) & 0xFF);
		mm1[1] = (((reg >> 16) & 0x0F) | ((reg >> 18) & 0xF0));
		len = 2;
	}
	dprintk(2, "(0x%08x, 0x%02x)\n", reg, flags);
	status = i2c_read(state->i2c, adr, mm1, len, mm2, 2);
	if (status < 0)
		return status;
	if (data)
		*data = mm2[0] | (mm2[1] << 8);

	return 0;
}

static int read16(struct drxk_state *state, u32 reg, u16 *data)
{
	return read16_flags(state, reg, data, 0);
}

static int read32_flags(struct drxk_state *state, u32 reg, u32 *data, u8 flags)
{
	int status;
	u8 adr = state->demod_address, mm1[4], mm2[4], len;

	if (state->single_master)
		flags |= 0xC0;

	if (DRXDAP_FASI_LONG_FORMAT(reg) || (flags != 0)) {
		mm1[0] = (((reg << 1) & 0xFF) | 0x01);
		mm1[1] = ((reg >> 16) & 0xFF);
		mm1[2] = ((reg >> 24) & 0xFF) | flags;
		mm1[3] = ((reg >> 7) & 0xFF);
		len = 4;
	} else {
		mm1[0] = ((reg << 1) & 0xFF);
		mm1[1] = (((reg >> 16) & 0x0F) | ((reg >> 18) & 0xF0));
		len = 2;
	}
	dprintk(2, "(0x%08x, 0x%02x)\n", reg, flags);
	status = i2c_read(state->i2c, adr, mm1, len, mm2, 4);
	if (status < 0)
		return status;
	if (data)
		*data = mm2[0] | (mm2[1] << 8) |
		    (mm2[2] << 16) | (mm2[3] << 24);

	return 0;
}

static int read32(struct drxk_state *state, u32 reg, u32 *data)
{
	return read32_flags(state, reg, data, 0);
}

static int write16_flags(struct drxk_state *state, u32 reg, u16 data, u8 flags)
{
	u8 adr = state->demod_address, mm[6], len;

	if (state->single_master)
		flags |= 0xC0;
	if (DRXDAP_FASI_LONG_FORMAT(reg) || (flags != 0)) {
		mm[0] = (((reg << 1) & 0xFF) | 0x01);
		mm[1] = ((reg >> 16) & 0xFF);
		mm[2] = ((reg >> 24) & 0xFF) | flags;
		mm[3] = ((reg >> 7) & 0xFF);
		len = 4;
	} else {
		mm[0] = ((reg << 1) & 0xFF);
		mm[1] = (((reg >> 16) & 0x0F) | ((reg >> 18) & 0xF0));
		len = 2;
	}
	mm[len] = data & 0xff;
	mm[len + 1] = (data >> 8) & 0xff;

	dprintk(2, "(0x%08x, 0x%04x, 0x%02x)\n", reg, data, flags);
	return i2c_write(state->i2c, adr, mm, len + 2);
}

static int write16(struct drxk_state *state, u32 reg, u16 data)
{
	return write16_flags(state, reg, data, 0);
}

static int write32_flags(struct drxk_state *state, u32 reg, u32 data, u8 flags)
{
	u8 adr = state->demod_address, mm[8], len;

	if (state->single_master)
		flags |= 0xC0;
	if (DRXDAP_FASI_LONG_FORMAT(reg) || (flags != 0)) {
		mm[0] = (((reg << 1) & 0xFF) | 0x01);
		mm[1] = ((reg >> 16) & 0xFF);
		mm[2] = ((reg >> 24) & 0xFF) | flags;
		mm[3] = ((reg >> 7) & 0xFF);
		len = 4;
	} else {
		mm[0] = ((reg << 1) & 0xFF);
		mm[1] = (((reg >> 16) & 0x0F) | ((reg >> 18) & 0xF0));
		len = 2;
	}
	mm[len] = data & 0xff;
	mm[len + 1] = (data >> 8) & 0xff;
	mm[len + 2] = (data >> 16) & 0xff;
	mm[len + 3] = (data >> 24) & 0xff;
	dprintk(2, "(0x%08x, 0x%08x, 0x%02x)\n", reg, data, flags);

	return i2c_write(state->i2c, adr, mm, len + 4);
}

static int write32(struct drxk_state *state, u32 reg, u32 data)
{
	return write32_flags(state, reg, data, 0);
}

static int write_block(struct drxk_state *state, u32 Address,
		      const int BlockSize, const u8 pBlock[])
{
	int status = 0, BlkSize = BlockSize;
	u8 Flags = 0;

	if (state->single_master)
		Flags |= 0xC0;

	while (BlkSize > 0) {
		int Chunk = BlkSize > state->m_ChunkSize ?
		    state->m_ChunkSize : BlkSize;
		u8 *AdrBuf = &state->Chunk[0];
		u32 AdrLength = 0;

		if (DRXDAP_FASI_LONG_FORMAT(Address) || (Flags != 0)) {
			AdrBuf[0] = (((Address << 1) & 0xFF) | 0x01);
			AdrBuf[1] = ((Address >> 16) & 0xFF);
			AdrBuf[2] = ((Address >> 24) & 0xFF);
			AdrBuf[3] = ((Address >> 7) & 0xFF);
			AdrBuf[2] |= Flags;
			AdrLength = 4;
			if (Chunk == state->m_ChunkSize)
				Chunk -= 2;
		} else {
			AdrBuf[0] = ((Address << 1) & 0xFF);
			AdrBuf[1] = (((Address >> 16) & 0x0F) |
				     ((Address >> 18) & 0xF0));
			AdrLength = 2;
		}
		memcpy(&state->Chunk[AdrLength], pBlock, Chunk);
		dprintk(2, "(0x%08x, 0x%02x)\n", Address, Flags);
		if (debug > 1) {
			int i;
			if (pBlock)
				for (i = 0; i < Chunk; i++)
					printk(KERN_CONT " %02x", pBlock[i]);
			printk(KERN_CONT "\n");
		}
		status = i2c_write(state->i2c, state->demod_address,
				   &state->Chunk[0], Chunk + AdrLength);
		if (status < 0) {
			printk(KERN_ERR "drxk: %s: i2c write error at addr 0x%02x\n",
			       __func__, Address);
			break;
		}
		pBlock += Chunk;
		Address += (Chunk >> 1);
		BlkSize -= Chunk;
	}
	return status;
}

#ifndef DRXK_MAX_RETRIES_POWERUP
#define DRXK_MAX_RETRIES_POWERUP 20
#endif

int PowerUpDevice(struct drxk_state *state)
{
	int status;
	u8 data = 0;
	u16 retryCount = 0;

	dprintk(1, "\n");

	status = i2c_read1(state->i2c, state->demod_address, &data);
	if (status < 0) {
		do {
			data = 0;
			status = i2c_write(state->i2c, state->demod_address,
					   &data, 1);
			msleep(10);
			retryCount++;
			if (status < 0)
				continue;
			status = i2c_read1(state->i2c, state->demod_address,
					   &data);
		} while (status < 0 &&
			 (retryCount < DRXK_MAX_RETRIES_POWERUP));
		if (status < 0 && retryCount >= DRXK_MAX_RETRIES_POWERUP)
			goto error;
	}

	
	status = write16(state, SIO_CC_PWD_MODE__A, SIO_CC_PWD_MODE_LEVEL_NONE);
	if (status < 0)
		goto error;
	status = write16(state, SIO_CC_UPDATE__A, SIO_CC_UPDATE_KEY);
	if (status < 0)
		goto error;
	
	status = write16(state, SIO_CC_PLL_LOCK__A, 1);
	if (status < 0)
		goto error;

	state->m_currentPowerMode = DRX_POWER_UP;

error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);

	return status;
}


static int init_state(struct drxk_state *state)
{
	u32 ulVSBIfAgcMode = DRXK_AGC_CTRL_AUTO;
	u32 ulVSBIfAgcOutputLevel = 0;
	u32 ulVSBIfAgcMinLevel = 0;
	u32 ulVSBIfAgcMaxLevel = 0x7FFF;
	u32 ulVSBIfAgcSpeed = 3;

	u32 ulVSBRfAgcMode = DRXK_AGC_CTRL_AUTO;
	u32 ulVSBRfAgcOutputLevel = 0;
	u32 ulVSBRfAgcMinLevel = 0;
	u32 ulVSBRfAgcMaxLevel = 0x7FFF;
	u32 ulVSBRfAgcSpeed = 3;
	u32 ulVSBRfAgcTop = 9500;
	u32 ulVSBRfAgcCutOffCurrent = 4000;

	u32 ulATVIfAgcMode = DRXK_AGC_CTRL_AUTO;
	u32 ulATVIfAgcOutputLevel = 0;
	u32 ulATVIfAgcMinLevel = 0;
	u32 ulATVIfAgcMaxLevel = 0;
	u32 ulATVIfAgcSpeed = 3;

	u32 ulATVRfAgcMode = DRXK_AGC_CTRL_OFF;
	u32 ulATVRfAgcOutputLevel = 0;
	u32 ulATVRfAgcMinLevel = 0;
	u32 ulATVRfAgcMaxLevel = 0;
	u32 ulATVRfAgcTop = 9500;
	u32 ulATVRfAgcCutOffCurrent = 4000;
	u32 ulATVRfAgcSpeed = 3;

	u32 ulQual83 = DEFAULT_MER_83;
	u32 ulQual93 = DEFAULT_MER_93;

	u32 ulMpegLockTimeOut = DEFAULT_DRXK_MPEG_LOCK_TIMEOUT;
	u32 ulDemodLockTimeOut = DEFAULT_DRXK_DEMOD_LOCK_TIMEOUT;

	
	
	
	u32 ulGPIOCfg = 0x0113;
	u32 ulInvertTSClock = 0;
	u32 ulTSDataStrength = DRXK_MPEG_SERIAL_OUTPUT_PIN_DRIVE_STRENGTH;
	u32 ulDVBTBitrate = 50000000;
	u32 ulDVBCBitrate = DRXK_QAM_SYMBOLRATE_MAX * 8;

	u32 ulInsertRSByte = 0;

	u32 ulRfMirror = 1;
	u32 ulPowerDown = 0;

	dprintk(1, "\n");

	state->m_hasLNA = false;
	state->m_hasDVBT = false;
	state->m_hasDVBC = false;
	state->m_hasATV = false;
	state->m_hasOOB = false;
	state->m_hasAudio = false;

	if (!state->m_ChunkSize)
		state->m_ChunkSize = 124;

	state->m_oscClockFreq = 0;
	state->m_smartAntInverted = false;
	state->m_bPDownOpenBridge = false;

	
	state->m_sysClockFreq = 151875;
	
	
	state->m_HICfgTimingDiv = ((state->m_sysClockFreq / 1000) *
				   HI_I2C_DELAY) / 1000;
	
	if (state->m_HICfgTimingDiv > SIO_HI_RA_RAM_PAR_2_CFG_DIV__M)
		state->m_HICfgTimingDiv = SIO_HI_RA_RAM_PAR_2_CFG_DIV__M;
	state->m_HICfgWakeUpKey = (state->demod_address << 1);
	
	state->m_HICfgCtrl = SIO_HI_RA_RAM_PAR_5_CFG_SLV0_SLAVE;

	state->m_bPowerDown = (ulPowerDown != 0);

	state->m_DRXK_A1_PATCH_CODE = false;
	state->m_DRXK_A1_ROM_CODE = false;
	state->m_DRXK_A2_ROM_CODE = false;
	state->m_DRXK_A3_ROM_CODE = false;
	state->m_DRXK_A2_PATCH_CODE = false;
	state->m_DRXK_A3_PATCH_CODE = false;

	
	
	state->m_vsbIfAgcCfg.ctrlMode = (ulVSBIfAgcMode);
	state->m_vsbIfAgcCfg.outputLevel = (ulVSBIfAgcOutputLevel);
	state->m_vsbIfAgcCfg.minOutputLevel = (ulVSBIfAgcMinLevel);
	state->m_vsbIfAgcCfg.maxOutputLevel = (ulVSBIfAgcMaxLevel);
	state->m_vsbIfAgcCfg.speed = (ulVSBIfAgcSpeed);
	state->m_vsbPgaCfg = 140;

	
	state->m_vsbRfAgcCfg.ctrlMode = (ulVSBRfAgcMode);
	state->m_vsbRfAgcCfg.outputLevel = (ulVSBRfAgcOutputLevel);
	state->m_vsbRfAgcCfg.minOutputLevel = (ulVSBRfAgcMinLevel);
	state->m_vsbRfAgcCfg.maxOutputLevel = (ulVSBRfAgcMaxLevel);
	state->m_vsbRfAgcCfg.speed = (ulVSBRfAgcSpeed);
	state->m_vsbRfAgcCfg.top = (ulVSBRfAgcTop);
	state->m_vsbRfAgcCfg.cutOffCurrent = (ulVSBRfAgcCutOffCurrent);
	state->m_vsbPreSawCfg.reference = 0x07;
	state->m_vsbPreSawCfg.usePreSaw = true;

	state->m_Quality83percent = DEFAULT_MER_83;
	state->m_Quality93percent = DEFAULT_MER_93;
	if (ulQual93 <= 500 && ulQual83 < ulQual93) {
		state->m_Quality83percent = ulQual83;
		state->m_Quality93percent = ulQual93;
	}

	
	state->m_atvIfAgcCfg.ctrlMode = (ulATVIfAgcMode);
	state->m_atvIfAgcCfg.outputLevel = (ulATVIfAgcOutputLevel);
	state->m_atvIfAgcCfg.minOutputLevel = (ulATVIfAgcMinLevel);
	state->m_atvIfAgcCfg.maxOutputLevel = (ulATVIfAgcMaxLevel);
	state->m_atvIfAgcCfg.speed = (ulATVIfAgcSpeed);

	
	state->m_atvRfAgcCfg.ctrlMode = (ulATVRfAgcMode);
	state->m_atvRfAgcCfg.outputLevel = (ulATVRfAgcOutputLevel);
	state->m_atvRfAgcCfg.minOutputLevel = (ulATVRfAgcMinLevel);
	state->m_atvRfAgcCfg.maxOutputLevel = (ulATVRfAgcMaxLevel);
	state->m_atvRfAgcCfg.speed = (ulATVRfAgcSpeed);
	state->m_atvRfAgcCfg.top = (ulATVRfAgcTop);
	state->m_atvRfAgcCfg.cutOffCurrent = (ulATVRfAgcCutOffCurrent);
	state->m_atvPreSawCfg.reference = 0x04;
	state->m_atvPreSawCfg.usePreSaw = true;


	
	state->m_dvbtRfAgcCfg.ctrlMode = DRXK_AGC_CTRL_OFF;
	state->m_dvbtRfAgcCfg.outputLevel = 0;
	state->m_dvbtRfAgcCfg.minOutputLevel = 0;
	state->m_dvbtRfAgcCfg.maxOutputLevel = 0xFFFF;
	state->m_dvbtRfAgcCfg.top = 0x2100;
	state->m_dvbtRfAgcCfg.cutOffCurrent = 4000;
	state->m_dvbtRfAgcCfg.speed = 1;


	
	state->m_dvbtIfAgcCfg.ctrlMode = DRXK_AGC_CTRL_AUTO;
	state->m_dvbtIfAgcCfg.outputLevel = 0;
	state->m_dvbtIfAgcCfg.minOutputLevel = 0;
	state->m_dvbtIfAgcCfg.maxOutputLevel = 9000;
	state->m_dvbtIfAgcCfg.top = 13424;
	state->m_dvbtIfAgcCfg.cutOffCurrent = 0;
	state->m_dvbtIfAgcCfg.speed = 3;
	state->m_dvbtIfAgcCfg.FastClipCtrlDelay = 30;
	state->m_dvbtIfAgcCfg.IngainTgtMax = 30000;
	

	state->m_dvbtPreSawCfg.reference = 4;
	state->m_dvbtPreSawCfg.usePreSaw = false;

	
	state->m_qamRfAgcCfg.ctrlMode = DRXK_AGC_CTRL_OFF;
	state->m_qamRfAgcCfg.outputLevel = 0;
	state->m_qamRfAgcCfg.minOutputLevel = 6023;
	state->m_qamRfAgcCfg.maxOutputLevel = 27000;
	state->m_qamRfAgcCfg.top = 0x2380;
	state->m_qamRfAgcCfg.cutOffCurrent = 4000;
	state->m_qamRfAgcCfg.speed = 3;

	
	state->m_qamIfAgcCfg.ctrlMode = DRXK_AGC_CTRL_AUTO;
	state->m_qamIfAgcCfg.outputLevel = 0;
	state->m_qamIfAgcCfg.minOutputLevel = 0;
	state->m_qamIfAgcCfg.maxOutputLevel = 9000;
	state->m_qamIfAgcCfg.top = 0x0511;
	state->m_qamIfAgcCfg.cutOffCurrent = 0;
	state->m_qamIfAgcCfg.speed = 3;
	state->m_qamIfAgcCfg.IngainTgtMax = 5119;
	state->m_qamIfAgcCfg.FastClipCtrlDelay = 50;

	state->m_qamPgaCfg = 140;
	state->m_qamPreSawCfg.reference = 4;
	state->m_qamPreSawCfg.usePreSaw = false;

	state->m_OperationMode = OM_NONE;
	state->m_DrxkState = DRXK_UNINITIALIZED;

	
	state->m_enableMPEGOutput = true;	
	state->m_insertRSByte = false;	
	state->m_invertDATA = false;	
	state->m_invertERR = false;	
	state->m_invertSTR = false;	
	state->m_invertVAL = false;	
	state->m_invertCLK = (ulInvertTSClock != 0);	


	state->m_DVBTBitrate = ulDVBTBitrate;
	state->m_DVBCBitrate = ulDVBCBitrate;

	state->m_TSDataStrength = (ulTSDataStrength & 0x07);

	
	state->m_mpegTsStaticBitrate = 19392658;
	state->m_disableTEIhandling = false;

	if (ulInsertRSByte)
		state->m_insertRSByte = true;

	state->m_MpegLockTimeOut = DEFAULT_DRXK_MPEG_LOCK_TIMEOUT;
	if (ulMpegLockTimeOut < 10000)
		state->m_MpegLockTimeOut = ulMpegLockTimeOut;
	state->m_DemodLockTimeOut = DEFAULT_DRXK_DEMOD_LOCK_TIMEOUT;
	if (ulDemodLockTimeOut < 10000)
		state->m_DemodLockTimeOut = ulDemodLockTimeOut;

	
	state->m_Constellation = DRX_CONSTELLATION_AUTO;
	state->m_qamInterleaveMode = DRXK_QAM_I12_J17;
	state->m_fecRsPlen = 204 * 8;	
	state->m_fecRsPrescale = 1;

	state->m_sqiSpeed = DRXK_DVBT_SQI_SPEED_MEDIUM;
	state->m_agcFastClipCtrlDelay = 0;

	state->m_GPIOCfg = (ulGPIOCfg);

	state->m_bPowerDown = false;
	state->m_currentPowerMode = DRX_POWER_DOWN;

	state->m_rfmirror = (ulRfMirror == 0);
	state->m_IfAgcPol = false;
	return 0;
}

static int DRXX_Open(struct drxk_state *state)
{
	int status = 0;
	u32 jtag = 0;
	u16 bid = 0;
	u16 key = 0;

	dprintk(1, "\n");
	
	status = write16(state, SCU_RAM_GPIO__A, SCU_RAM_GPIO_HW_LOCK_IND_DISABLE);
	if (status < 0)
		goto error;
	
	status = read16(state, SIO_TOP_COMM_KEY__A, &key);
	if (status < 0)
		goto error;
	status = write16(state, SIO_TOP_COMM_KEY__A, SIO_TOP_COMM_KEY_KEY);
	if (status < 0)
		goto error;
	status = read32(state, SIO_TOP_JTAGID_LO__A, &jtag);
	if (status < 0)
		goto error;
	status = read16(state, SIO_PDR_UIO_IN_HI__A, &bid);
	if (status < 0)
		goto error;
	status = write16(state, SIO_TOP_COMM_KEY__A, key);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int GetDeviceCapabilities(struct drxk_state *state)
{
	u16 sioPdrOhwCfg = 0;
	u32 sioTopJtagidLo = 0;
	int status;
	const char *spin = "";

	dprintk(1, "\n");

	
	
	status = write16(state, SCU_RAM_GPIO__A, SCU_RAM_GPIO_HW_LOCK_IND_DISABLE);
	if (status < 0)
		goto error;
	status = write16(state, SIO_TOP_COMM_KEY__A, 0xFABA);
	if (status < 0)
		goto error;
	status = read16(state, SIO_PDR_OHW_CFG__A, &sioPdrOhwCfg);
	if (status < 0)
		goto error;
	status = write16(state, SIO_TOP_COMM_KEY__A, 0x0000);
	if (status < 0)
		goto error;

	switch ((sioPdrOhwCfg & SIO_PDR_OHW_CFG_FREF_SEL__M)) {
	case 0:
		
		break;
	case 1:
		
		state->m_oscClockFreq = 27000;
		break;
	case 2:
		
		state->m_oscClockFreq = 20250;
		break;
	case 3:
		
		state->m_oscClockFreq = 20250;
		break;
	default:
		printk(KERN_ERR "drxk: Clock Frequency is unkonwn\n");
		return -EINVAL;
	}
	status = read32(state, SIO_TOP_JTAGID_LO__A, &sioTopJtagidLo);
	if (status < 0)
		goto error;

printk(KERN_ERR "drxk: status = 0x%08x\n", sioTopJtagidLo);

	
	switch ((sioTopJtagidLo >> 29) & 0xF) {
	case 0:
		state->m_deviceSpin = DRXK_SPIN_A1;
		spin = "A1";
		break;
	case 2:
		state->m_deviceSpin = DRXK_SPIN_A2;
		spin = "A2";
		break;
	case 3:
		state->m_deviceSpin = DRXK_SPIN_A3;
		spin = "A3";
		break;
	default:
		state->m_deviceSpin = DRXK_SPIN_UNKNOWN;
		status = -EINVAL;
		printk(KERN_ERR "drxk: Spin %d unknown\n",
		       (sioTopJtagidLo >> 29) & 0xF);
		goto error2;
	}
	switch ((sioTopJtagidLo >> 12) & 0xFF) {
	case 0x13:
		
		state->m_hasLNA = false;
		state->m_hasOOB = false;
		state->m_hasATV = false;
		state->m_hasAudio = false;
		state->m_hasDVBT = true;
		state->m_hasDVBC = true;
		state->m_hasSAWSW = true;
		state->m_hasGPIO2 = false;
		state->m_hasGPIO1 = false;
		state->m_hasIRQN = false;
		break;
	case 0x15:
		
		state->m_hasLNA = false;
		state->m_hasOOB = false;
		state->m_hasATV = true;
		state->m_hasAudio = false;
		state->m_hasDVBT = true;
		state->m_hasDVBC = false;
		state->m_hasSAWSW = true;
		state->m_hasGPIO2 = true;
		state->m_hasGPIO1 = true;
		state->m_hasIRQN = false;
		break;
	case 0x16:
		
		state->m_hasLNA = false;
		state->m_hasOOB = false;
		state->m_hasATV = true;
		state->m_hasAudio = false;
		state->m_hasDVBT = true;
		state->m_hasDVBC = false;
		state->m_hasSAWSW = true;
		state->m_hasGPIO2 = true;
		state->m_hasGPIO1 = true;
		state->m_hasIRQN = false;
		break;
	case 0x18:
		
		state->m_hasLNA = false;
		state->m_hasOOB = false;
		state->m_hasATV = true;
		state->m_hasAudio = true;
		state->m_hasDVBT = true;
		state->m_hasDVBC = false;
		state->m_hasSAWSW = true;
		state->m_hasGPIO2 = true;
		state->m_hasGPIO1 = true;
		state->m_hasIRQN = false;
		break;
	case 0x21:
		
		state->m_hasLNA = false;
		state->m_hasOOB = false;
		state->m_hasATV = true;
		state->m_hasAudio = true;
		state->m_hasDVBT = true;
		state->m_hasDVBC = true;
		state->m_hasSAWSW = true;
		state->m_hasGPIO2 = true;
		state->m_hasGPIO1 = true;
		state->m_hasIRQN = false;
		break;
	case 0x23:
		
		state->m_hasLNA = false;
		state->m_hasOOB = false;
		state->m_hasATV = true;
		state->m_hasAudio = true;
		state->m_hasDVBT = true;
		state->m_hasDVBC = true;
		state->m_hasSAWSW = true;
		state->m_hasGPIO2 = true;
		state->m_hasGPIO1 = true;
		state->m_hasIRQN = false;
		break;
	case 0x25:
		
		state->m_hasLNA = false;
		state->m_hasOOB = false;
		state->m_hasATV = true;
		state->m_hasAudio = true;
		state->m_hasDVBT = true;
		state->m_hasDVBC = true;
		state->m_hasSAWSW = true;
		state->m_hasGPIO2 = true;
		state->m_hasGPIO1 = true;
		state->m_hasIRQN = false;
		break;
	case 0x26:
		
		state->m_hasLNA = false;
		state->m_hasOOB = false;
		state->m_hasATV = true;
		state->m_hasAudio = false;
		state->m_hasDVBT = true;
		state->m_hasDVBC = true;
		state->m_hasSAWSW = true;
		state->m_hasGPIO2 = true;
		state->m_hasGPIO1 = true;
		state->m_hasIRQN = false;
		break;
	default:
		printk(KERN_ERR "drxk: DeviceID 0x%02x not supported\n",
			((sioTopJtagidLo >> 12) & 0xFF));
		status = -EINVAL;
		goto error2;
	}

	printk(KERN_INFO
	       "drxk: detected a drx-39%02xk, spin %s, xtal %d.%03d MHz\n",
	       ((sioTopJtagidLo >> 12) & 0xFF), spin,
	       state->m_oscClockFreq / 1000,
	       state->m_oscClockFreq % 1000);

error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);

error2:
	return status;
}

static int HI_Command(struct drxk_state *state, u16 cmd, u16 *pResult)
{
	int status;
	bool powerdown_cmd;

	dprintk(1, "\n");

	
	status = write16(state, SIO_HI_RA_RAM_CMD__A, cmd);
	if (status < 0)
		goto error;
	if (cmd == SIO_HI_RA_RAM_CMD_RESET)
		msleep(1);

	powerdown_cmd =
	    (bool) ((cmd == SIO_HI_RA_RAM_CMD_CONFIG) &&
		    ((state->m_HICfgCtrl) &
		     SIO_HI_RA_RAM_PAR_5_CFG_SLEEP__M) ==
		    SIO_HI_RA_RAM_PAR_5_CFG_SLEEP_ZZZ);
	if (powerdown_cmd == false) {
		
		u32 retryCount = 0;
		u16 waitCmd;

		do {
			msleep(1);
			retryCount += 1;
			status = read16(state, SIO_HI_RA_RAM_CMD__A,
					  &waitCmd);
		} while ((status < 0) && (retryCount < DRXK_MAX_RETRIES)
			 && (waitCmd != 0));
		if (status < 0)
			goto error;
		status = read16(state, SIO_HI_RA_RAM_RES__A, pResult);
	}
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);

	return status;
}

static int HI_CfgCommand(struct drxk_state *state)
{
	int status;

	dprintk(1, "\n");

	mutex_lock(&state->mutex);

	status = write16(state, SIO_HI_RA_RAM_PAR_6__A, state->m_HICfgTimeout);
	if (status < 0)
		goto error;
	status = write16(state, SIO_HI_RA_RAM_PAR_5__A, state->m_HICfgCtrl);
	if (status < 0)
		goto error;
	status = write16(state, SIO_HI_RA_RAM_PAR_4__A, state->m_HICfgWakeUpKey);
	if (status < 0)
		goto error;
	status = write16(state, SIO_HI_RA_RAM_PAR_3__A, state->m_HICfgBridgeDelay);
	if (status < 0)
		goto error;
	status = write16(state, SIO_HI_RA_RAM_PAR_2__A, state->m_HICfgTimingDiv);
	if (status < 0)
		goto error;
	status = write16(state, SIO_HI_RA_RAM_PAR_1__A, SIO_HI_RA_RAM_PAR_1_PAR1_SEC_KEY);
	if (status < 0)
		goto error;
	status = HI_Command(state, SIO_HI_RA_RAM_CMD_CONFIG, 0);
	if (status < 0)
		goto error;

	state->m_HICfgCtrl &= ~SIO_HI_RA_RAM_PAR_5_CFG_SLEEP_ZZZ;
error:
	mutex_unlock(&state->mutex);
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int InitHI(struct drxk_state *state)
{
	dprintk(1, "\n");

	state->m_HICfgWakeUpKey = (state->demod_address << 1);
	state->m_HICfgTimeout = 0x96FF;
	
	state->m_HICfgCtrl = SIO_HI_RA_RAM_PAR_5_CFG_SLV0_SLAVE;

	return HI_CfgCommand(state);
}

static int MPEGTSConfigurePins(struct drxk_state *state, bool mpegEnable)
{
	int status = -1;
	u16 sioPdrMclkCfg = 0;
	u16 sioPdrMdxCfg = 0;
	u16 err_cfg = 0;

	dprintk(1, ": mpeg %s, %s mode\n",
		mpegEnable ? "enable" : "disable",
		state->m_enableParallel ? "parallel" : "serial");

	
	status = write16(state, SCU_RAM_GPIO__A, SCU_RAM_GPIO_HW_LOCK_IND_DISABLE);
	if (status < 0)
		goto error;

	
	status = write16(state, SIO_TOP_COMM_KEY__A, 0xFABA);
	if (status < 0)
		goto error;

	if (mpegEnable == false) {
		
		status = write16(state, SIO_PDR_MSTRT_CFG__A, 0x0000);
		if (status < 0)
			goto error;
		status = write16(state, SIO_PDR_MERR_CFG__A, 0x0000);
		if (status < 0)
			goto error;
		status = write16(state, SIO_PDR_MCLK_CFG__A, 0x0000);
		if (status < 0)
			goto error;
		status = write16(state, SIO_PDR_MVAL_CFG__A, 0x0000);
		if (status < 0)
			goto error;
		status = write16(state, SIO_PDR_MD0_CFG__A, 0x0000);
		if (status < 0)
			goto error;
		status = write16(state, SIO_PDR_MD1_CFG__A, 0x0000);
		if (status < 0)
			goto error;
		status = write16(state, SIO_PDR_MD2_CFG__A, 0x0000);
		if (status < 0)
			goto error;
		status = write16(state, SIO_PDR_MD3_CFG__A, 0x0000);
		if (status < 0)
			goto error;
		status = write16(state, SIO_PDR_MD4_CFG__A, 0x0000);
		if (status < 0)
			goto error;
		status = write16(state, SIO_PDR_MD5_CFG__A, 0x0000);
		if (status < 0)
			goto error;
		status = write16(state, SIO_PDR_MD6_CFG__A, 0x0000);
		if (status < 0)
			goto error;
		status = write16(state, SIO_PDR_MD7_CFG__A, 0x0000);
		if (status < 0)
			goto error;
	} else {
		
		sioPdrMdxCfg =
			((state->m_TSDataStrength <<
			SIO_PDR_MD0_CFG_DRIVE__B) | 0x0003);
		sioPdrMclkCfg = ((state->m_TSClockkStrength <<
					SIO_PDR_MCLK_CFG_DRIVE__B) |
					0x0003);

		status = write16(state, SIO_PDR_MSTRT_CFG__A, sioPdrMdxCfg);
		if (status < 0)
			goto error;

		if (state->enable_merr_cfg)
			err_cfg = sioPdrMdxCfg;

		status = write16(state, SIO_PDR_MERR_CFG__A, err_cfg);
		if (status < 0)
			goto error;
		status = write16(state, SIO_PDR_MVAL_CFG__A, err_cfg);
		if (status < 0)
			goto error;

		if (state->m_enableParallel == true) {
			
			status = write16(state, SIO_PDR_MD1_CFG__A, sioPdrMdxCfg);
			if (status < 0)
				goto error;
			status = write16(state, SIO_PDR_MD2_CFG__A, sioPdrMdxCfg);
			if (status < 0)
				goto error;
			status = write16(state, SIO_PDR_MD3_CFG__A, sioPdrMdxCfg);
			if (status < 0)
				goto error;
			status = write16(state, SIO_PDR_MD4_CFG__A, sioPdrMdxCfg);
			if (status < 0)
				goto error;
			status = write16(state, SIO_PDR_MD5_CFG__A, sioPdrMdxCfg);
			if (status < 0)
				goto error;
			status = write16(state, SIO_PDR_MD6_CFG__A, sioPdrMdxCfg);
			if (status < 0)
				goto error;
			status = write16(state, SIO_PDR_MD7_CFG__A, sioPdrMdxCfg);
			if (status < 0)
				goto error;
		} else {
			sioPdrMdxCfg = ((state->m_TSDataStrength <<
						SIO_PDR_MD0_CFG_DRIVE__B)
					| 0x0003);
			
			status = write16(state, SIO_PDR_MD1_CFG__A, 0x0000);
			if (status < 0)
				goto error;
			status = write16(state, SIO_PDR_MD2_CFG__A, 0x0000);
			if (status < 0)
				goto error;
			status = write16(state, SIO_PDR_MD3_CFG__A, 0x0000);
			if (status < 0)
				goto error;
			status = write16(state, SIO_PDR_MD4_CFG__A, 0x0000);
			if (status < 0)
				goto error;
			status = write16(state, SIO_PDR_MD5_CFG__A, 0x0000);
			if (status < 0)
				goto error;
			status = write16(state, SIO_PDR_MD6_CFG__A, 0x0000);
			if (status < 0)
				goto error;
			status = write16(state, SIO_PDR_MD7_CFG__A, 0x0000);
			if (status < 0)
				goto error;
		}
		status = write16(state, SIO_PDR_MCLK_CFG__A, sioPdrMclkCfg);
		if (status < 0)
			goto error;
		status = write16(state, SIO_PDR_MD0_CFG__A, sioPdrMdxCfg);
		if (status < 0)
			goto error;
	}
	
	status = write16(state, SIO_PDR_MON_CFG__A, 0x0000);
	if (status < 0)
		goto error;
	
	status = write16(state, SIO_TOP_COMM_KEY__A, 0x0000);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int MPEGTSDisable(struct drxk_state *state)
{
	dprintk(1, "\n");

	return MPEGTSConfigurePins(state, false);
}

static int BLChainCmd(struct drxk_state *state,
		      u16 romOffset, u16 nrOfElements, u32 timeOut)
{
	u16 blStatus = 0;
	int status;
	unsigned long end;

	dprintk(1, "\n");
	mutex_lock(&state->mutex);
	status = write16(state, SIO_BL_MODE__A, SIO_BL_MODE_CHAIN);
	if (status < 0)
		goto error;
	status = write16(state, SIO_BL_CHAIN_ADDR__A, romOffset);
	if (status < 0)
		goto error;
	status = write16(state, SIO_BL_CHAIN_LEN__A, nrOfElements);
	if (status < 0)
		goto error;
	status = write16(state, SIO_BL_ENABLE__A, SIO_BL_ENABLE_ON);
	if (status < 0)
		goto error;

	end = jiffies + msecs_to_jiffies(timeOut);
	do {
		msleep(1);
		status = read16(state, SIO_BL_STATUS__A, &blStatus);
		if (status < 0)
			goto error;
	} while ((blStatus == 0x1) &&
			((time_is_after_jiffies(end))));

	if (blStatus == 0x1) {
		printk(KERN_ERR "drxk: SIO not ready\n");
		status = -EINVAL;
		goto error2;
	}
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
error2:
	mutex_unlock(&state->mutex);
	return status;
}


static int DownloadMicrocode(struct drxk_state *state,
			     const u8 pMCImage[], u32 Length)
{
	const u8 *pSrc = pMCImage;
	u16 Flags;
	u16 Drain;
	u32 Address;
	u16 nBlocks;
	u16 BlockSize;
	u16 BlockCRC;
	u32 offset = 0;
	u32 i;
	int status = 0;

	dprintk(1, "\n");

	
	Drain = (pSrc[0] << 8) | pSrc[1];
	pSrc += sizeof(u16);
	offset += sizeof(u16);
	nBlocks = (pSrc[0] << 8) | pSrc[1];
	pSrc += sizeof(u16);
	offset += sizeof(u16);

	for (i = 0; i < nBlocks; i += 1) {
		Address = (pSrc[0] << 24) | (pSrc[1] << 16) |
		    (pSrc[2] << 8) | pSrc[3];
		pSrc += sizeof(u32);
		offset += sizeof(u32);

		BlockSize = ((pSrc[0] << 8) | pSrc[1]) * sizeof(u16);
		pSrc += sizeof(u16);
		offset += sizeof(u16);

		Flags = (pSrc[0] << 8) | pSrc[1];
		pSrc += sizeof(u16);
		offset += sizeof(u16);

		BlockCRC = (pSrc[0] << 8) | pSrc[1];
		pSrc += sizeof(u16);
		offset += sizeof(u16);

		if (offset + BlockSize > Length) {
			printk(KERN_ERR "drxk: Firmware is corrupted.\n");
			return -EINVAL;
		}

		status = write_block(state, Address, BlockSize, pSrc);
		if (status < 0) {
			printk(KERN_ERR "drxk: Error %d while loading firmware\n", status);
			break;
		}
		pSrc += BlockSize;
		offset += BlockSize;
	}
	return status;
}

static int DVBTEnableOFDMTokenRing(struct drxk_state *state, bool enable)
{
	int status;
	u16 data = 0;
	u16 desiredCtrl = SIO_OFDM_SH_OFDM_RING_ENABLE_ON;
	u16 desiredStatus = SIO_OFDM_SH_OFDM_RING_STATUS_ENABLED;
	unsigned long end;

	dprintk(1, "\n");

	if (enable == false) {
		desiredCtrl = SIO_OFDM_SH_OFDM_RING_ENABLE_OFF;
		desiredStatus = SIO_OFDM_SH_OFDM_RING_STATUS_DOWN;
	}

	status = read16(state, SIO_OFDM_SH_OFDM_RING_STATUS__A, &data);
	if (status >= 0 && data == desiredStatus) {
		
		return status;
	}
	
	status = write16(state, SIO_OFDM_SH_OFDM_RING_ENABLE__A, desiredCtrl);

	end = jiffies + msecs_to_jiffies(DRXK_OFDM_TR_SHUTDOWN_TIMEOUT);
	do {
		status = read16(state, SIO_OFDM_SH_OFDM_RING_STATUS__A, &data);
		if ((status >= 0 && data == desiredStatus) || time_is_after_jiffies(end))
			break;
		msleep(1);
	} while (1);
	if (data != desiredStatus) {
		printk(KERN_ERR "drxk: SIO not ready\n");
		return -EINVAL;
	}
	return status;
}

static int MPEGTSStop(struct drxk_state *state)
{
	int status = 0;
	u16 fecOcSncMode = 0;
	u16 fecOcIprMode = 0;

	dprintk(1, "\n");

	
	status = read16(state, FEC_OC_SNC_MODE__A, &fecOcSncMode);
	if (status < 0)
		goto error;
	fecOcSncMode |= FEC_OC_SNC_MODE_SHUTDOWN__M;
	status = write16(state, FEC_OC_SNC_MODE__A, fecOcSncMode);
	if (status < 0)
		goto error;

	
	status = read16(state, FEC_OC_IPR_MODE__A, &fecOcIprMode);
	if (status < 0)
		goto error;
	fecOcIprMode |= FEC_OC_IPR_MODE_MCLK_DIS_DAT_ABS__M;
	status = write16(state, FEC_OC_IPR_MODE__A, fecOcIprMode);

error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);

	return status;
}

static int scu_command(struct drxk_state *state,
		       u16 cmd, u8 parameterLen,
		       u16 *parameter, u8 resultLen, u16 *result)
{
#if (SCU_RAM_PARAM_0__A - SCU_RAM_PARAM_15__A) != 15
#error DRXK register mapping no longer compatible with this routine!
#endif
	u16 curCmd = 0;
	int status = -EINVAL;
	unsigned long end;
	u8 buffer[34];
	int cnt = 0, ii;
	const char *p;
	char errname[30];

	dprintk(1, "\n");

	if ((cmd == 0) || ((parameterLen > 0) && (parameter == NULL)) ||
	    ((resultLen > 0) && (result == NULL))) {
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
		return status;
	}

	mutex_lock(&state->mutex);

	for (ii = parameterLen - 1; ii >= 0; ii -= 1) {
		buffer[cnt++] = (parameter[ii] & 0xFF);
		buffer[cnt++] = ((parameter[ii] >> 8) & 0xFF);
	}
	buffer[cnt++] = (cmd & 0xFF);
	buffer[cnt++] = ((cmd >> 8) & 0xFF);

	write_block(state, SCU_RAM_PARAM_0__A -
			(parameterLen - 1), cnt, buffer);
	
	end = jiffies + msecs_to_jiffies(DRXK_MAX_WAITTIME);
	do {
		msleep(1);
		status = read16(state, SCU_RAM_COMMAND__A, &curCmd);
		if (status < 0)
			goto error;
	} while (!(curCmd == DRX_SCU_READY) && (time_is_after_jiffies(end)));
	if (curCmd != DRX_SCU_READY) {
		printk(KERN_ERR "drxk: SCU not ready\n");
		status = -EIO;
		goto error2;
	}
	
	if ((resultLen > 0) && (result != NULL)) {
		s16 err;
		int ii;

		for (ii = resultLen - 1; ii >= 0; ii -= 1) {
			status = read16(state, SCU_RAM_PARAM_0__A - ii, &result[ii]);
			if (status < 0)
				goto error;
		}

		
		err = (s16)result[0];
		if (err >= 0)
			goto error;

		
		switch (err) {
		case SCU_RESULT_UNKCMD:
			p = "SCU_RESULT_UNKCMD";
			break;
		case SCU_RESULT_UNKSTD:
			p = "SCU_RESULT_UNKSTD";
			break;
		case SCU_RESULT_SIZE:
			p = "SCU_RESULT_SIZE";
			break;
		case SCU_RESULT_INVPAR:
			p = "SCU_RESULT_INVPAR";
			break;
		default: 
			sprintf(errname, "ERROR: %d\n", err);
			p = errname;
		}
		printk(KERN_ERR "drxk: %s while sending cmd 0x%04x with params:", p, cmd);
		print_hex_dump_bytes("drxk: ", DUMP_PREFIX_NONE, buffer, cnt);
		status = -EINVAL;
		goto error2;
	}

error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
error2:
	mutex_unlock(&state->mutex);
	return status;
}

static int SetIqmAf(struct drxk_state *state, bool active)
{
	u16 data = 0;
	int status;

	dprintk(1, "\n");

	
	status = read16(state, IQM_AF_STDBY__A, &data);
	if (status < 0)
		goto error;

	if (!active) {
		data |= (IQM_AF_STDBY_STDBY_ADC_STANDBY
				| IQM_AF_STDBY_STDBY_AMP_STANDBY
				| IQM_AF_STDBY_STDBY_PD_STANDBY
				| IQM_AF_STDBY_STDBY_TAGC_IF_STANDBY
				| IQM_AF_STDBY_STDBY_TAGC_RF_STANDBY);
	} else {
		data &= ((~IQM_AF_STDBY_STDBY_ADC_STANDBY)
				& (~IQM_AF_STDBY_STDBY_AMP_STANDBY)
				& (~IQM_AF_STDBY_STDBY_PD_STANDBY)
				& (~IQM_AF_STDBY_STDBY_TAGC_IF_STANDBY)
				& (~IQM_AF_STDBY_STDBY_TAGC_RF_STANDBY)
			);
	}
	status = write16(state, IQM_AF_STDBY__A, data);

error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int CtrlPowerMode(struct drxk_state *state, enum DRXPowerMode *mode)
{
	int status = 0;
	u16 sioCcPwdMode = 0;

	dprintk(1, "\n");

	
	if (mode == NULL)
		return -EINVAL;

	switch (*mode) {
	case DRX_POWER_UP:
		sioCcPwdMode = SIO_CC_PWD_MODE_LEVEL_NONE;
		break;
	case DRXK_POWER_DOWN_OFDM:
		sioCcPwdMode = SIO_CC_PWD_MODE_LEVEL_OFDM;
		break;
	case DRXK_POWER_DOWN_CORE:
		sioCcPwdMode = SIO_CC_PWD_MODE_LEVEL_CLOCK;
		break;
	case DRXK_POWER_DOWN_PLL:
		sioCcPwdMode = SIO_CC_PWD_MODE_LEVEL_PLL;
		break;
	case DRX_POWER_DOWN:
		sioCcPwdMode = SIO_CC_PWD_MODE_LEVEL_OSC;
		break;
	default:
		
		return -EINVAL;
	}

	
	if (state->m_currentPowerMode == *mode)
		return 0;

	
	if (state->m_currentPowerMode != DRX_POWER_UP) {
		status = PowerUpDevice(state);
		if (status < 0)
			goto error;
		status = DVBTEnableOFDMTokenRing(state, true);
		if (status < 0)
			goto error;
	}

	if (*mode == DRX_POWER_UP) {
		
	} else {
		
		
		
		
		
		
		
		switch (state->m_OperationMode) {
		case OM_DVBT:
			status = MPEGTSStop(state);
			if (status < 0)
				goto error;
			status = PowerDownDVBT(state, false);
			if (status < 0)
				goto error;
			break;
		case OM_QAM_ITU_A:
		case OM_QAM_ITU_C:
			status = MPEGTSStop(state);
			if (status < 0)
				goto error;
			status = PowerDownQAM(state);
			if (status < 0)
				goto error;
			break;
		default:
			break;
		}
		status = DVBTEnableOFDMTokenRing(state, false);
		if (status < 0)
			goto error;
		status = write16(state, SIO_CC_PWD_MODE__A, sioCcPwdMode);
		if (status < 0)
			goto error;
		status = write16(state, SIO_CC_UPDATE__A, SIO_CC_UPDATE_KEY);
		if (status < 0)
			goto error;

		if (*mode != DRXK_POWER_DOWN_OFDM) {
			state->m_HICfgCtrl |=
				SIO_HI_RA_RAM_PAR_5_CFG_SLEEP_ZZZ;
			status = HI_CfgCommand(state);
			if (status < 0)
				goto error;
		}
	}
	state->m_currentPowerMode = *mode;

error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);

	return status;
}

static int PowerDownDVBT(struct drxk_state *state, bool setPowerMode)
{
	enum DRXPowerMode powerMode = DRXK_POWER_DOWN_OFDM;
	u16 cmdResult = 0;
	u16 data = 0;
	int status;

	dprintk(1, "\n");

	status = read16(state, SCU_COMM_EXEC__A, &data);
	if (status < 0)
		goto error;
	if (data == SCU_COMM_EXEC_ACTIVE) {
		
		status = scu_command(state, SCU_RAM_COMMAND_STANDARD_OFDM | SCU_RAM_COMMAND_CMD_DEMOD_STOP, 0, NULL, 1, &cmdResult);
		if (status < 0)
			goto error;
		
		status = scu_command(state, SCU_RAM_COMMAND_STANDARD_OFDM | SCU_RAM_COMMAND_CMD_DEMOD_RESET, 0, NULL, 1, &cmdResult);
		if (status < 0)
			goto error;
	}

	
	status = write16(state, OFDM_SC_COMM_EXEC__A, OFDM_SC_COMM_EXEC_STOP);
	if (status < 0)
		goto error;
	status = write16(state, OFDM_LC_COMM_EXEC__A, OFDM_LC_COMM_EXEC_STOP);
	if (status < 0)
		goto error;
	status = write16(state, IQM_COMM_EXEC__A, IQM_COMM_EXEC_B_STOP);
	if (status < 0)
		goto error;

	
	status = SetIqmAf(state, false);
	if (status < 0)
		goto error;

	
	if (setPowerMode) {
		status = CtrlPowerMode(state, &powerMode);
		if (status < 0)
			goto error;
	}
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int SetOperationMode(struct drxk_state *state,
			    enum OperationMode oMode)
{
	int status = 0;

	dprintk(1, "\n");

	
	status = write16(state, SCU_RAM_GPIO__A, SCU_RAM_GPIO_HW_LOCK_IND_DISABLE);
	if (status < 0)
		goto error;

	
	if (state->m_OperationMode == oMode)
		return 0;

	switch (state->m_OperationMode) {
		
	case OM_NONE:
		break;
	case OM_DVBT:
		status = MPEGTSStop(state);
		if (status < 0)
			goto error;
		status = PowerDownDVBT(state, true);
		if (status < 0)
			goto error;
		state->m_OperationMode = OM_NONE;
		break;
	case OM_QAM_ITU_A:	
	case OM_QAM_ITU_C:
		status = MPEGTSStop(state);
		if (status < 0)
			goto error;
		status = PowerDownQAM(state);
		if (status < 0)
			goto error;
		state->m_OperationMode = OM_NONE;
		break;
	case OM_QAM_ITU_B:
	default:
		status = -EINVAL;
		goto error;
	}

	switch (oMode) {
	case OM_DVBT:
		dprintk(1, ": DVB-T\n");
		state->m_OperationMode = oMode;
		status = SetDVBTStandard(state, oMode);
		if (status < 0)
			goto error;
		break;
	case OM_QAM_ITU_A:	
	case OM_QAM_ITU_C:
		dprintk(1, ": DVB-C Annex %c\n",
			(state->m_OperationMode == OM_QAM_ITU_A) ? 'A' : 'C');
		state->m_OperationMode = oMode;
		status = SetQAMStandard(state, oMode);
		if (status < 0)
			goto error;
		break;
	case OM_QAM_ITU_B:
	default:
		status = -EINVAL;
	}
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int Start(struct drxk_state *state, s32 offsetFreq,
		 s32 IntermediateFrequency)
{
	int status = -EINVAL;

	u16 IFreqkHz;
	s32 OffsetkHz = offsetFreq / 1000;

	dprintk(1, "\n");
	if (state->m_DrxkState != DRXK_STOPPED &&
		state->m_DrxkState != DRXK_DTV_STARTED)
		goto error;

	state->m_bMirrorFreqSpect = (state->props.inversion == INVERSION_ON);

	if (IntermediateFrequency < 0) {
		state->m_bMirrorFreqSpect = !state->m_bMirrorFreqSpect;
		IntermediateFrequency = -IntermediateFrequency;
	}

	switch (state->m_OperationMode) {
	case OM_QAM_ITU_A:
	case OM_QAM_ITU_C:
		IFreqkHz = (IntermediateFrequency / 1000);
		status = SetQAM(state, IFreqkHz, OffsetkHz);
		if (status < 0)
			goto error;
		state->m_DrxkState = DRXK_DTV_STARTED;
		break;
	case OM_DVBT:
		IFreqkHz = (IntermediateFrequency / 1000);
		status = MPEGTSStop(state);
		if (status < 0)
			goto error;
		status = SetDVBT(state, IFreqkHz, OffsetkHz);
		if (status < 0)
			goto error;
		status = DVBTStart(state);
		if (status < 0)
			goto error;
		state->m_DrxkState = DRXK_DTV_STARTED;
		break;
	default:
		break;
	}
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int ShutDown(struct drxk_state *state)
{
	dprintk(1, "\n");

	MPEGTSStop(state);
	return 0;
}

static int GetLockStatus(struct drxk_state *state, u32 *pLockStatus,
			 u32 Time)
{
	int status = -EINVAL;

	dprintk(1, "\n");

	if (pLockStatus == NULL)
		goto error;

	*pLockStatus = NOT_LOCKED;

	
	switch (state->m_OperationMode) {
	case OM_QAM_ITU_A:
	case OM_QAM_ITU_B:
	case OM_QAM_ITU_C:
		status = GetQAMLockStatus(state, pLockStatus);
		break;
	case OM_DVBT:
		status = GetDVBTLockStatus(state, pLockStatus);
		break;
	default:
		break;
	}
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int MPEGTSStart(struct drxk_state *state)
{
	int status;

	u16 fecOcSncMode = 0;

	
	status = read16(state, FEC_OC_SNC_MODE__A, &fecOcSncMode);
	if (status < 0)
		goto error;
	fecOcSncMode &= ~FEC_OC_SNC_MODE_SHUTDOWN__M;
	status = write16(state, FEC_OC_SNC_MODE__A, fecOcSncMode);
	if (status < 0)
		goto error;
	status = write16(state, FEC_OC_SNC_UNLOCK__A, 1);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int MPEGTSDtoInit(struct drxk_state *state)
{
	int status;

	dprintk(1, "\n");

	
	status = write16(state, FEC_OC_RCN_CTL_STEP_LO__A, 0x0000);
	if (status < 0)
		goto error;
	status = write16(state, FEC_OC_RCN_CTL_STEP_HI__A, 0x000C);
	if (status < 0)
		goto error;
	status = write16(state, FEC_OC_RCN_GAIN__A, 0x000A);
	if (status < 0)
		goto error;
	status = write16(state, FEC_OC_AVR_PARM_A__A, 0x0008);
	if (status < 0)
		goto error;
	status = write16(state, FEC_OC_AVR_PARM_B__A, 0x0006);
	if (status < 0)
		goto error;
	status = write16(state, FEC_OC_TMD_HI_MARGIN__A, 0x0680);
	if (status < 0)
		goto error;
	status = write16(state, FEC_OC_TMD_LO_MARGIN__A, 0x0080);
	if (status < 0)
		goto error;
	status = write16(state, FEC_OC_TMD_COUNT__A, 0x03F4);
	if (status < 0)
		goto error;

	
	status = write16(state, FEC_OC_OCR_INVERT__A, 0);
	if (status < 0)
		goto error;
	status = write16(state, FEC_OC_SNC_LWM__A, 2);
	if (status < 0)
		goto error;
	status = write16(state, FEC_OC_SNC_HWM__A, 12);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);

	return status;
}

static int MPEGTSDtoSetup(struct drxk_state *state,
			  enum OperationMode oMode)
{
	int status;

	u16 fecOcRegMode = 0;	
	u16 fecOcRegIprMode = 0;	
	u16 fecOcDtoMode = 0;	
	u16 fecOcFctMode = 0;	
	u16 fecOcDtoPeriod = 2;	
	u16 fecOcDtoBurstLen = 188;	
	u32 fecOcRcnCtlRate = 0;	
	u16 fecOcTmdMode = 0;
	u16 fecOcTmdIntUpdRate = 0;
	u32 maxBitRate = 0;
	bool staticCLK = false;

	dprintk(1, "\n");

	
	status = read16(state, FEC_OC_MODE__A, &fecOcRegMode);
	if (status < 0)
		goto error;
	status = read16(state, FEC_OC_IPR_MODE__A, &fecOcRegIprMode);
	if (status < 0)
		goto error;
	fecOcRegMode &= (~FEC_OC_MODE_PARITY__M);
	fecOcRegIprMode &= (~FEC_OC_IPR_MODE_MVAL_DIS_PAR__M);
	if (state->m_insertRSByte == true) {
		
		fecOcRegMode |= FEC_OC_MODE_PARITY__M;
		
		fecOcRegIprMode |= FEC_OC_IPR_MODE_MVAL_DIS_PAR__M;
		
		fecOcDtoBurstLen = 204;
	}

	
	fecOcRegIprMode &= (~(FEC_OC_IPR_MODE_SERIAL__M));
	if (state->m_enableParallel == false) {
		
		fecOcRegIprMode |= FEC_OC_IPR_MODE_SERIAL__M;
	}

	switch (oMode) {
	case OM_DVBT:
		maxBitRate = state->m_DVBTBitrate;
		fecOcTmdMode = 3;
		fecOcRcnCtlRate = 0xC00000;
		staticCLK = state->m_DVBTStaticCLK;
		break;
	case OM_QAM_ITU_A:	
	case OM_QAM_ITU_C:
		fecOcTmdMode = 0x0004;
		fecOcRcnCtlRate = 0xD2B4EE;	
		maxBitRate = state->m_DVBCBitrate;
		staticCLK = state->m_DVBCStaticCLK;
		break;
	default:
		status = -EINVAL;
	}		
	if (status < 0)
		goto error;

	
	if (staticCLK) {
		u32 bitRate = 0;

		fecOcDtoMode = (FEC_OC_DTO_MODE_DYNAMIC__M |
				FEC_OC_DTO_MODE_OFFSET_ENABLE__M);
		fecOcFctMode = (FEC_OC_FCT_MODE_RAT_ENA__M |
				FEC_OC_FCT_MODE_VIRT_ENA__M);

		
		bitRate = maxBitRate;
		if (bitRate > 75900000UL) {	
			bitRate = 75900000UL;
		}
		fecOcDtoPeriod = (u16) (((state->m_sysClockFreq)
						* 1000) / bitRate);
		if (fecOcDtoPeriod <= 2)
			fecOcDtoPeriod = 0;
		else
			fecOcDtoPeriod -= 2;
		fecOcTmdIntUpdRate = 8;
	} else {
		
		fecOcDtoMode = FEC_OC_DTO_MODE_DYNAMIC__M;
		fecOcFctMode = FEC_OC_FCT_MODE__PRE;
		fecOcTmdIntUpdRate = 5;
	}

	
	status = write16(state, FEC_OC_DTO_BURST_LEN__A, fecOcDtoBurstLen);
	if (status < 0)
		goto error;
	status = write16(state, FEC_OC_DTO_PERIOD__A, fecOcDtoPeriod);
	if (status < 0)
		goto error;
	status = write16(state, FEC_OC_DTO_MODE__A, fecOcDtoMode);
	if (status < 0)
		goto error;
	status = write16(state, FEC_OC_FCT_MODE__A, fecOcFctMode);
	if (status < 0)
		goto error;
	status = write16(state, FEC_OC_MODE__A, fecOcRegMode);
	if (status < 0)
		goto error;
	status = write16(state, FEC_OC_IPR_MODE__A, fecOcRegIprMode);
	if (status < 0)
		goto error;

	
	status = write32(state, FEC_OC_RCN_CTL_RATE_LO__A, fecOcRcnCtlRate);
	if (status < 0)
		goto error;
	status = write16(state, FEC_OC_TMD_INT_UPD_RATE__A, fecOcTmdIntUpdRate);
	if (status < 0)
		goto error;
	status = write16(state, FEC_OC_TMD_MODE__A, fecOcTmdMode);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int MPEGTSConfigurePolarity(struct drxk_state *state)
{
	u16 fecOcRegIprInvert = 0;

	
	u16 InvertDataMask =
	    FEC_OC_IPR_INVERT_MD7__M | FEC_OC_IPR_INVERT_MD6__M |
	    FEC_OC_IPR_INVERT_MD5__M | FEC_OC_IPR_INVERT_MD4__M |
	    FEC_OC_IPR_INVERT_MD3__M | FEC_OC_IPR_INVERT_MD2__M |
	    FEC_OC_IPR_INVERT_MD1__M | FEC_OC_IPR_INVERT_MD0__M;

	dprintk(1, "\n");

	
	fecOcRegIprInvert &= (~(InvertDataMask));
	if (state->m_invertDATA == true)
		fecOcRegIprInvert |= InvertDataMask;
	fecOcRegIprInvert &= (~(FEC_OC_IPR_INVERT_MERR__M));
	if (state->m_invertERR == true)
		fecOcRegIprInvert |= FEC_OC_IPR_INVERT_MERR__M;
	fecOcRegIprInvert &= (~(FEC_OC_IPR_INVERT_MSTRT__M));
	if (state->m_invertSTR == true)
		fecOcRegIprInvert |= FEC_OC_IPR_INVERT_MSTRT__M;
	fecOcRegIprInvert &= (~(FEC_OC_IPR_INVERT_MVAL__M));
	if (state->m_invertVAL == true)
		fecOcRegIprInvert |= FEC_OC_IPR_INVERT_MVAL__M;
	fecOcRegIprInvert &= (~(FEC_OC_IPR_INVERT_MCLK__M));
	if (state->m_invertCLK == true)
		fecOcRegIprInvert |= FEC_OC_IPR_INVERT_MCLK__M;

	return write16(state, FEC_OC_IPR_INVERT__A, fecOcRegIprInvert);
}

#define   SCU_RAM_AGC_KI_INV_RF_POL__M 0x4000

static int SetAgcRf(struct drxk_state *state,
		    struct SCfgAgc *pAgcCfg, bool isDTV)
{
	int status = -EINVAL;
	u16 data = 0;
	struct SCfgAgc *pIfAgcSettings;

	dprintk(1, "\n");

	if (pAgcCfg == NULL)
		goto error;

	switch (pAgcCfg->ctrlMode) {
	case DRXK_AGC_CTRL_AUTO:
		
		status = read16(state, IQM_AF_STDBY__A, &data);
		if (status < 0)
			goto error;
		data &= ~IQM_AF_STDBY_STDBY_TAGC_RF_STANDBY;
		status = write16(state, IQM_AF_STDBY__A, data);
		if (status < 0)
			goto error;
		status = read16(state, SCU_RAM_AGC_CONFIG__A, &data);
		if (status < 0)
			goto error;

		
		data &= ~SCU_RAM_AGC_CONFIG_DISABLE_RF_AGC__M;

		
		if (state->m_RfAgcPol)
			data |= SCU_RAM_AGC_CONFIG_INV_RF_POL__M;
		else
			data &= ~SCU_RAM_AGC_CONFIG_INV_RF_POL__M;
		status = write16(state, SCU_RAM_AGC_CONFIG__A, data);
		if (status < 0)
			goto error;

		
		status = read16(state, SCU_RAM_AGC_KI_RED__A, &data);
		if (status < 0)
			goto error;

		data &= ~SCU_RAM_AGC_KI_RED_RAGC_RED__M;
		data |= (~(pAgcCfg->speed <<
				SCU_RAM_AGC_KI_RED_RAGC_RED__B)
				& SCU_RAM_AGC_KI_RED_RAGC_RED__M);

		status = write16(state, SCU_RAM_AGC_KI_RED__A, data);
		if (status < 0)
			goto error;

		if (IsDVBT(state))
			pIfAgcSettings = &state->m_dvbtIfAgcCfg;
		else if (IsQAM(state))
			pIfAgcSettings = &state->m_qamIfAgcCfg;
		else
			pIfAgcSettings = &state->m_atvIfAgcCfg;
		if (pIfAgcSettings == NULL) {
			status = -EINVAL;
			goto error;
		}

		
		if (pIfAgcSettings->ctrlMode == DRXK_AGC_CTRL_AUTO)
			status = write16(state, SCU_RAM_AGC_IF_IACCU_HI_TGT_MAX__A, pAgcCfg->top);
			if (status < 0)
				goto error;

		
		status = write16(state, SCU_RAM_AGC_RF_IACCU_HI_CO__A, pAgcCfg->cutOffCurrent);
		if (status < 0)
			goto error;

		
		status = write16(state, SCU_RAM_AGC_RF_MAX__A, pAgcCfg->maxOutputLevel);
		if (status < 0)
			goto error;

		break;

	case DRXK_AGC_CTRL_USER:
		
		status = read16(state, IQM_AF_STDBY__A, &data);
		if (status < 0)
			goto error;
		data &= ~IQM_AF_STDBY_STDBY_TAGC_RF_STANDBY;
		status = write16(state, IQM_AF_STDBY__A, data);
		if (status < 0)
			goto error;

		
		status = read16(state, SCU_RAM_AGC_CONFIG__A, &data);
		if (status < 0)
			goto error;
		data |= SCU_RAM_AGC_CONFIG_DISABLE_RF_AGC__M;
		if (state->m_RfAgcPol)
			data |= SCU_RAM_AGC_CONFIG_INV_RF_POL__M;
		else
			data &= ~SCU_RAM_AGC_CONFIG_INV_RF_POL__M;
		status = write16(state, SCU_RAM_AGC_CONFIG__A, data);
		if (status < 0)
			goto error;

		
		status = write16(state, SCU_RAM_AGC_RF_IACCU_HI_CO__A, 0);
		if (status < 0)
			goto error;

		
		status = write16(state, SCU_RAM_AGC_RF_IACCU_HI__A, pAgcCfg->outputLevel);
		if (status < 0)
			goto error;
		break;

	case DRXK_AGC_CTRL_OFF:
		
		status = read16(state, IQM_AF_STDBY__A, &data);
		if (status < 0)
			goto error;
		data |= IQM_AF_STDBY_STDBY_TAGC_RF_STANDBY;
		status = write16(state, IQM_AF_STDBY__A, data);
		if (status < 0)
			goto error;

		
		status = read16(state, SCU_RAM_AGC_CONFIG__A, &data);
		if (status < 0)
			goto error;
		data |= SCU_RAM_AGC_CONFIG_DISABLE_RF_AGC__M;
		status = write16(state, SCU_RAM_AGC_CONFIG__A, data);
		if (status < 0)
			goto error;
		break;

	default:
		status = -EINVAL;

	}
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

#define SCU_RAM_AGC_KI_INV_IF_POL__M 0x2000

static int SetAgcIf(struct drxk_state *state,
		    struct SCfgAgc *pAgcCfg, bool isDTV)
{
	u16 data = 0;
	int status = 0;
	struct SCfgAgc *pRfAgcSettings;

	dprintk(1, "\n");

	switch (pAgcCfg->ctrlMode) {
	case DRXK_AGC_CTRL_AUTO:

		
		status = read16(state, IQM_AF_STDBY__A, &data);
		if (status < 0)
			goto error;
		data &= ~IQM_AF_STDBY_STDBY_TAGC_IF_STANDBY;
		status = write16(state, IQM_AF_STDBY__A, data);
		if (status < 0)
			goto error;

		status = read16(state, SCU_RAM_AGC_CONFIG__A, &data);
		if (status < 0)
			goto error;

		
		data &= ~SCU_RAM_AGC_CONFIG_DISABLE_IF_AGC__M;

		
		if (state->m_IfAgcPol)
			data |= SCU_RAM_AGC_CONFIG_INV_IF_POL__M;
		else
			data &= ~SCU_RAM_AGC_CONFIG_INV_IF_POL__M;
		status = write16(state, SCU_RAM_AGC_CONFIG__A, data);
		if (status < 0)
			goto error;

		
		status = read16(state, SCU_RAM_AGC_KI_RED__A, &data);
		if (status < 0)
			goto error;
		data &= ~SCU_RAM_AGC_KI_RED_IAGC_RED__M;
		data |= (~(pAgcCfg->speed <<
				SCU_RAM_AGC_KI_RED_IAGC_RED__B)
				& SCU_RAM_AGC_KI_RED_IAGC_RED__M);

		status = write16(state, SCU_RAM_AGC_KI_RED__A, data);
		if (status < 0)
			goto error;

		if (IsQAM(state))
			pRfAgcSettings = &state->m_qamRfAgcCfg;
		else
			pRfAgcSettings = &state->m_atvRfAgcCfg;
		if (pRfAgcSettings == NULL)
			return -1;
		
		status = write16(state, SCU_RAM_AGC_IF_IACCU_HI_TGT_MAX__A, pRfAgcSettings->top);
		if (status < 0)
			goto error;
		break;

	case DRXK_AGC_CTRL_USER:

		
		status = read16(state, IQM_AF_STDBY__A, &data);
		if (status < 0)
			goto error;
		data &= ~IQM_AF_STDBY_STDBY_TAGC_IF_STANDBY;
		status = write16(state, IQM_AF_STDBY__A, data);
		if (status < 0)
			goto error;

		status = read16(state, SCU_RAM_AGC_CONFIG__A, &data);
		if (status < 0)
			goto error;

		
		data |= SCU_RAM_AGC_CONFIG_DISABLE_IF_AGC__M;

		
		if (state->m_IfAgcPol)
			data |= SCU_RAM_AGC_CONFIG_INV_IF_POL__M;
		else
			data &= ~SCU_RAM_AGC_CONFIG_INV_IF_POL__M;
		status = write16(state, SCU_RAM_AGC_CONFIG__A, data);
		if (status < 0)
			goto error;

		
		status = write16(state, SCU_RAM_AGC_IF_IACCU_HI_TGT_MAX__A, pAgcCfg->outputLevel);
		if (status < 0)
			goto error;
		break;

	case DRXK_AGC_CTRL_OFF:

		
		status = read16(state, IQM_AF_STDBY__A, &data);
		if (status < 0)
			goto error;
		data |= IQM_AF_STDBY_STDBY_TAGC_IF_STANDBY;
		status = write16(state, IQM_AF_STDBY__A, data);
		if (status < 0)
			goto error;

		
		status = read16(state, SCU_RAM_AGC_CONFIG__A, &data);
		if (status < 0)
			goto error;
		data |= SCU_RAM_AGC_CONFIG_DISABLE_IF_AGC__M;
		status = write16(state, SCU_RAM_AGC_CONFIG__A, data);
		if (status < 0)
			goto error;
		break;
	}		

	status = write16(state, SCU_RAM_AGC_INGAIN_TGT_MIN__A, pAgcCfg->top);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int ReadIFAgc(struct drxk_state *state, u32 *pValue)
{
	u16 agcDacLvl;
	int status;
	u16 Level = 0;

	dprintk(1, "\n");

	status = read16(state, IQM_AF_AGC_IF__A, &agcDacLvl);
	if (status < 0) {
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
		return status;
	}

	*pValue = 0;

	if (agcDacLvl > DRXK_AGC_DAC_OFFSET)
		Level = agcDacLvl - DRXK_AGC_DAC_OFFSET;
	if (Level < 14000)
		*pValue = (14000 - Level) / 4;
	else
		*pValue = 0;

	return status;
}

static int GetQAMSignalToNoise(struct drxk_state *state,
			       s32 *pSignalToNoise)
{
	int status = 0;
	u16 qamSlErrPower = 0;	
	u32 qamSlSigPower = 0;	
	u32 qamSlMer = 0;	

	dprintk(1, "\n");

	

	
	status = read16(state, QAM_SL_ERR_POWER__A, &qamSlErrPower);
	if (status < 0) {
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
		return -EINVAL;
	}

	switch (state->props.modulation) {
	case QAM_16:
		qamSlSigPower = DRXK_QAM_SL_SIG_POWER_QAM16 << 2;
		break;
	case QAM_32:
		qamSlSigPower = DRXK_QAM_SL_SIG_POWER_QAM32 << 2;
		break;
	case QAM_64:
		qamSlSigPower = DRXK_QAM_SL_SIG_POWER_QAM64 << 2;
		break;
	case QAM_128:
		qamSlSigPower = DRXK_QAM_SL_SIG_POWER_QAM128 << 2;
		break;
	default:
	case QAM_256:
		qamSlSigPower = DRXK_QAM_SL_SIG_POWER_QAM256 << 2;
		break;
	}

	if (qamSlErrPower > 0) {
		qamSlMer = Log10Times100(qamSlSigPower) -
			Log10Times100((u32) qamSlErrPower);
	}
	*pSignalToNoise = qamSlMer;

	return status;
}

static int GetDVBTSignalToNoise(struct drxk_state *state,
				s32 *pSignalToNoise)
{
	int status;
	u16 regData = 0;
	u32 EqRegTdSqrErrI = 0;
	u32 EqRegTdSqrErrQ = 0;
	u16 EqRegTdSqrErrExp = 0;
	u16 EqRegTdTpsPwrOfs = 0;
	u16 EqRegTdReqSmbCnt = 0;
	u32 tpsCnt = 0;
	u32 SqrErrIQ = 0;
	u32 a = 0;
	u32 b = 0;
	u32 c = 0;
	u32 iMER = 0;
	u16 transmissionParams = 0;

	dprintk(1, "\n");

	status = read16(state, OFDM_EQ_TOP_TD_TPS_PWR_OFS__A, &EqRegTdTpsPwrOfs);
	if (status < 0)
		goto error;
	status = read16(state, OFDM_EQ_TOP_TD_REQ_SMB_CNT__A, &EqRegTdReqSmbCnt);
	if (status < 0)
		goto error;
	status = read16(state, OFDM_EQ_TOP_TD_SQR_ERR_EXP__A, &EqRegTdSqrErrExp);
	if (status < 0)
		goto error;
	status = read16(state, OFDM_EQ_TOP_TD_SQR_ERR_I__A, &regData);
	if (status < 0)
		goto error;
	
	EqRegTdSqrErrI = (u32) regData;
	if ((EqRegTdSqrErrExp > 11) &&
		(EqRegTdSqrErrI < 0x00000FFFUL)) {
		EqRegTdSqrErrI += 0x00010000UL;
	}
	status = read16(state, OFDM_EQ_TOP_TD_SQR_ERR_Q__A, &regData);
	if (status < 0)
		goto error;
	
	EqRegTdSqrErrQ = (u32) regData;
	if ((EqRegTdSqrErrExp > 11) &&
		(EqRegTdSqrErrQ < 0x00000FFFUL))
		EqRegTdSqrErrQ += 0x00010000UL;

	status = read16(state, OFDM_SC_RA_RAM_OP_PARAM__A, &transmissionParams);
	if (status < 0)
		goto error;

	

	
	if ((EqRegTdTpsPwrOfs == 0) || (EqRegTdReqSmbCnt == 0))
		iMER = 0;
	else if ((EqRegTdSqrErrI + EqRegTdSqrErrQ) == 0) {
		iMER = 0;
	} else {
		SqrErrIQ = (EqRegTdSqrErrI + EqRegTdSqrErrQ) <<
			EqRegTdSqrErrExp;
		if ((transmissionParams &
			OFDM_SC_RA_RAM_OP_PARAM_MODE__M)
			== OFDM_SC_RA_RAM_OP_PARAM_MODE_2K)
			tpsCnt = 17;
		else
			tpsCnt = 68;


		
		a = Log10Times100(EqRegTdTpsPwrOfs *
					EqRegTdTpsPwrOfs);
		
		b = Log10Times100(EqRegTdReqSmbCnt * tpsCnt);
		
		c = Log10Times100(SqrErrIQ);

		iMER = a + b;
		
		if (iMER > c)
			iMER -= c;
		else
			iMER = 0;
	}
	*pSignalToNoise = iMER;

error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int GetSignalToNoise(struct drxk_state *state, s32 *pSignalToNoise)
{
	dprintk(1, "\n");

	*pSignalToNoise = 0;
	switch (state->m_OperationMode) {
	case OM_DVBT:
		return GetDVBTSignalToNoise(state, pSignalToNoise);
	case OM_QAM_ITU_A:
	case OM_QAM_ITU_C:
		return GetQAMSignalToNoise(state, pSignalToNoise);
	default:
		break;
	}
	return 0;
}

#if 0
static int GetDVBTQuality(struct drxk_state *state, s32 *pQuality)
{
	
	int status = 0;

	dprintk(1, "\n");

	static s32 QE_SN[] = {
		51,		
		69,		
		79,		
		89,		
		97,		
		108,		
		131,		
		146,		
		156,		
		160,		
		165,		
		187,		
		202,		
		216,		
		225,		
	};

	*pQuality = 0;

	do {
		s32 SignalToNoise = 0;
		u16 Constellation = 0;
		u16 CodeRate = 0;
		u32 SignalToNoiseRel;
		u32 BERQuality;

		status = GetDVBTSignalToNoise(state, &SignalToNoise);
		if (status < 0)
			break;
		status = read16(state, OFDM_EQ_TOP_TD_TPS_CONST__A, &Constellation);
		if (status < 0)
			break;
		Constellation &= OFDM_EQ_TOP_TD_TPS_CONST__M;

		status = read16(state, OFDM_EQ_TOP_TD_TPS_CODE_HP__A, &CodeRate);
		if (status < 0)
			break;
		CodeRate &= OFDM_EQ_TOP_TD_TPS_CODE_HP__M;

		if (Constellation > OFDM_EQ_TOP_TD_TPS_CONST_64QAM ||
		    CodeRate > OFDM_EQ_TOP_TD_TPS_CODE_LP_7_8)
			break;
		SignalToNoiseRel = SignalToNoise -
		    QE_SN[Constellation * 5 + CodeRate];
		BERQuality = 100;

		if (SignalToNoiseRel < -70)
			*pQuality = 0;
		else if (SignalToNoiseRel < 30)
			*pQuality = ((SignalToNoiseRel + 70) *
				     BERQuality) / 100;
		else
			*pQuality = BERQuality;
	} while (0);
	return 0;
};

static int GetDVBCQuality(struct drxk_state *state, s32 *pQuality)
{
	int status = 0;
	*pQuality = 0;

	dprintk(1, "\n");

	do {
		u32 SignalToNoise = 0;
		u32 BERQuality = 100;
		u32 SignalToNoiseRel = 0;

		status = GetQAMSignalToNoise(state, &SignalToNoise);
		if (status < 0)
			break;

		switch (state->props.modulation) {
		case QAM_16:
			SignalToNoiseRel = SignalToNoise - 200;
			break;
		case QAM_32:
			SignalToNoiseRel = SignalToNoise - 230;
			break;	
		case QAM_64:
			SignalToNoiseRel = SignalToNoise - 260;
			break;
		case QAM_128:
			SignalToNoiseRel = SignalToNoise - 290;
			break;
		default:
		case QAM_256:
			SignalToNoiseRel = SignalToNoise - 320;
			break;
		}

		if (SignalToNoiseRel < -70)
			*pQuality = 0;
		else if (SignalToNoiseRel < 30)
			*pQuality = ((SignalToNoiseRel + 70) *
				     BERQuality) / 100;
		else
			*pQuality = BERQuality;
	} while (0);

	return status;
}

static int GetQuality(struct drxk_state *state, s32 *pQuality)
{
	dprintk(1, "\n");

	switch (state->m_OperationMode) {
	case OM_DVBT:
		return GetDVBTQuality(state, pQuality);
	case OM_QAM_ITU_A:
		return GetDVBCQuality(state, pQuality);
	default:
		break;
	}

	return 0;
}
#endif

#define SIO_HI_RA_RAM_USR_BEGIN__A 0x420040
#define SIO_HI_RA_RAM_USR_END__A   0x420060

#define DRXK_HI_ATOMIC_BUF_START (SIO_HI_RA_RAM_USR_BEGIN__A)
#define DRXK_HI_ATOMIC_BUF_END   (SIO_HI_RA_RAM_USR_BEGIN__A + 7)
#define DRXK_HI_ATOMIC_READ      SIO_HI_RA_RAM_PAR_3_ACP_RW_READ
#define DRXK_HI_ATOMIC_WRITE     SIO_HI_RA_RAM_PAR_3_ACP_RW_WRITE

#define DRXDAP_FASI_ADDR2BLOCK(addr)  (((addr) >> 22) & 0x3F)
#define DRXDAP_FASI_ADDR2BANK(addr)   (((addr) >> 16) & 0x3F)
#define DRXDAP_FASI_ADDR2OFFSET(addr) ((addr) & 0x7FFF)

static int ConfigureI2CBridge(struct drxk_state *state, bool bEnableBridge)
{
	int status = -EINVAL;

	dprintk(1, "\n");

	if (state->m_DrxkState == DRXK_UNINITIALIZED)
		goto error;
	if (state->m_DrxkState == DRXK_POWERED_DOWN)
		goto error;

	if (state->no_i2c_bridge)
		return 0;

	status = write16(state, SIO_HI_RA_RAM_PAR_1__A, SIO_HI_RA_RAM_PAR_1_PAR1_SEC_KEY);
	if (status < 0)
		goto error;
	if (bEnableBridge) {
		status = write16(state, SIO_HI_RA_RAM_PAR_2__A, SIO_HI_RA_RAM_PAR_2_BRD_CFG_CLOSED);
		if (status < 0)
			goto error;
	} else {
		status = write16(state, SIO_HI_RA_RAM_PAR_2__A, SIO_HI_RA_RAM_PAR_2_BRD_CFG_OPEN);
		if (status < 0)
			goto error;
	}

	status = HI_Command(state, SIO_HI_RA_RAM_CMD_BRDCTRL, 0);

error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int SetPreSaw(struct drxk_state *state,
		     struct SCfgPreSaw *pPreSawCfg)
{
	int status = -EINVAL;

	dprintk(1, "\n");

	if ((pPreSawCfg == NULL)
	    || (pPreSawCfg->reference > IQM_AF_PDREF__M))
		goto error;

	status = write16(state, IQM_AF_PDREF__A, pPreSawCfg->reference);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int BLDirectCmd(struct drxk_state *state, u32 targetAddr,
		       u16 romOffset, u16 nrOfElements, u32 timeOut)
{
	u16 blStatus = 0;
	u16 offset = (u16) ((targetAddr >> 0) & 0x00FFFF);
	u16 blockbank = (u16) ((targetAddr >> 16) & 0x000FFF);
	int status;
	unsigned long end;

	dprintk(1, "\n");

	mutex_lock(&state->mutex);
	status = write16(state, SIO_BL_MODE__A, SIO_BL_MODE_DIRECT);
	if (status < 0)
		goto error;
	status = write16(state, SIO_BL_TGT_HDR__A, blockbank);
	if (status < 0)
		goto error;
	status = write16(state, SIO_BL_TGT_ADDR__A, offset);
	if (status < 0)
		goto error;
	status = write16(state, SIO_BL_SRC_ADDR__A, romOffset);
	if (status < 0)
		goto error;
	status = write16(state, SIO_BL_SRC_LEN__A, nrOfElements);
	if (status < 0)
		goto error;
	status = write16(state, SIO_BL_ENABLE__A, SIO_BL_ENABLE_ON);
	if (status < 0)
		goto error;

	end = jiffies + msecs_to_jiffies(timeOut);
	do {
		status = read16(state, SIO_BL_STATUS__A, &blStatus);
		if (status < 0)
			goto error;
	} while ((blStatus == 0x1) && time_is_after_jiffies(end));
	if (blStatus == 0x1) {
		printk(KERN_ERR "drxk: SIO not ready\n");
		status = -EINVAL;
		goto error2;
	}
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
error2:
	mutex_unlock(&state->mutex);
	return status;

}

static int ADCSyncMeasurement(struct drxk_state *state, u16 *count)
{
	u16 data = 0;
	int status;

	dprintk(1, "\n");

	
	status = write16(state, IQM_AF_COMM_EXEC__A, IQM_AF_COMM_EXEC_ACTIVE);
	if (status < 0)
		goto error;
	status = write16(state, IQM_AF_START_LOCK__A, 1);
	if (status < 0)
		goto error;

	*count = 0;
	status = read16(state, IQM_AF_PHASE0__A, &data);
	if (status < 0)
		goto error;
	if (data == 127)
		*count = *count + 1;
	status = read16(state, IQM_AF_PHASE1__A, &data);
	if (status < 0)
		goto error;
	if (data == 127)
		*count = *count + 1;
	status = read16(state, IQM_AF_PHASE2__A, &data);
	if (status < 0)
		goto error;
	if (data == 127)
		*count = *count + 1;

error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int ADCSynchronization(struct drxk_state *state)
{
	u16 count = 0;
	int status;

	dprintk(1, "\n");

	status = ADCSyncMeasurement(state, &count);
	if (status < 0)
		goto error;

	if (count == 1) {
		
		u16 clkNeg = 0;

		status = read16(state, IQM_AF_CLKNEG__A, &clkNeg);
		if (status < 0)
			goto error;
		if ((clkNeg | IQM_AF_CLKNEG_CLKNEGDATA__M) ==
			IQM_AF_CLKNEG_CLKNEGDATA_CLK_ADC_DATA_POS) {
			clkNeg &= (~(IQM_AF_CLKNEG_CLKNEGDATA__M));
			clkNeg |=
				IQM_AF_CLKNEG_CLKNEGDATA_CLK_ADC_DATA_NEG;
		} else {
			clkNeg &= (~(IQM_AF_CLKNEG_CLKNEGDATA__M));
			clkNeg |=
				IQM_AF_CLKNEG_CLKNEGDATA_CLK_ADC_DATA_POS;
		}
		status = write16(state, IQM_AF_CLKNEG__A, clkNeg);
		if (status < 0)
			goto error;
		status = ADCSyncMeasurement(state, &count);
		if (status < 0)
			goto error;
	}

	if (count < 2)
		status = -EINVAL;
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int SetFrequencyShifter(struct drxk_state *state,
			       u16 intermediateFreqkHz,
			       s32 tunerFreqOffset, bool isDTV)
{
	bool selectPosImage = false;
	u32 rfFreqResidual = tunerFreqOffset;
	u32 fmFrequencyShift = 0;
	bool tunerMirror = !state->m_bMirrorFreqSpect;
	u32 adcFreq;
	bool adcFlip;
	int status;
	u32 ifFreqActual;
	u32 samplingFrequency = (u32) (state->m_sysClockFreq / 3);
	u32 frequencyShift;
	bool imageToSelect;

	dprintk(1, "\n");

	if (isDTV) {
		if ((state->m_OperationMode == OM_QAM_ITU_A) ||
		    (state->m_OperationMode == OM_QAM_ITU_C) ||
		    (state->m_OperationMode == OM_DVBT))
			selectPosImage = true;
		else
			selectPosImage = false;
	}
	if (tunerMirror)
		
		ifFreqActual = intermediateFreqkHz +
		    rfFreqResidual + fmFrequencyShift;
	else
		
		ifFreqActual = intermediateFreqkHz -
		    rfFreqResidual - fmFrequencyShift;
	if (ifFreqActual > samplingFrequency / 2) {
		
		adcFreq = samplingFrequency - ifFreqActual;
		adcFlip = true;
	} else {
		
		adcFreq = ifFreqActual;
		adcFlip = false;
	}

	frequencyShift = adcFreq;
	imageToSelect = state->m_rfmirror ^ tunerMirror ^
	    adcFlip ^ selectPosImage;
	state->m_IqmFsRateOfs =
	    Frac28a((frequencyShift), samplingFrequency);

	if (imageToSelect)
		state->m_IqmFsRateOfs = ~state->m_IqmFsRateOfs + 1;

	
	
	status = write32(state, IQM_FS_RATE_OFS_LO__A,
			 state->m_IqmFsRateOfs);
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int InitAGC(struct drxk_state *state, bool isDTV)
{
	u16 ingainTgt = 0;
	u16 ingainTgtMin = 0;
	u16 ingainTgtMax = 0;
	u16 clpCyclen = 0;
	u16 clpSumMin = 0;
	u16 clpDirTo = 0;
	u16 snsSumMin = 0;
	u16 snsSumMax = 0;
	u16 clpSumMax = 0;
	u16 snsDirTo = 0;
	u16 kiInnergainMin = 0;
	u16 ifIaccuHiTgt = 0;
	u16 ifIaccuHiTgtMin = 0;
	u16 ifIaccuHiTgtMax = 0;
	u16 data = 0;
	u16 fastClpCtrlDelay = 0;
	u16 clpCtrlMode = 0;
	int status = 0;

	dprintk(1, "\n");

	
	snsSumMax = 1023;
	ifIaccuHiTgtMin = 2047;
	clpCyclen = 500;
	clpSumMax = 1023;

	
	if (!IsQAM(state)) {
		printk(KERN_ERR "drxk: %s: mode %d is not DVB-C\n", __func__, state->m_OperationMode);
		return -EINVAL;
	}

	

	
	clpSumMin = 8;
	clpDirTo = (u16) -9;
	clpCtrlMode = 0;
	snsSumMin = 8;
	snsDirTo = (u16) -9;
	kiInnergainMin = (u16) -1030;
	ifIaccuHiTgtMax = 0x2380;
	ifIaccuHiTgt = 0x2380;
	ingainTgtMin = 0x0511;
	ingainTgt = 0x0511;
	ingainTgtMax = 5119;
	fastClpCtrlDelay = state->m_qamIfAgcCfg.FastClipCtrlDelay;

	status = write16(state, SCU_RAM_AGC_FAST_CLP_CTRL_DELAY__A, fastClpCtrlDelay);
	if (status < 0)
		goto error;

	status = write16(state, SCU_RAM_AGC_CLP_CTRL_MODE__A, clpCtrlMode);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_INGAIN_TGT__A, ingainTgt);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_INGAIN_TGT_MIN__A, ingainTgtMin);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_INGAIN_TGT_MAX__A, ingainTgtMax);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_IF_IACCU_HI_TGT_MIN__A, ifIaccuHiTgtMin);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_IF_IACCU_HI_TGT_MAX__A, ifIaccuHiTgtMax);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_IF_IACCU_HI__A, 0);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_IF_IACCU_LO__A, 0);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_RF_IACCU_HI__A, 0);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_RF_IACCU_LO__A, 0);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_CLP_SUM_MAX__A, clpSumMax);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_SNS_SUM_MAX__A, snsSumMax);
	if (status < 0)
		goto error;

	status = write16(state, SCU_RAM_AGC_KI_INNERGAIN_MIN__A, kiInnergainMin);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_IF_IACCU_HI_TGT__A, ifIaccuHiTgt);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_CLP_CYCLEN__A, clpCyclen);
	if (status < 0)
		goto error;

	status = write16(state, SCU_RAM_AGC_RF_SNS_DEV_MAX__A, 1023);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_RF_SNS_DEV_MIN__A, (u16) -1023);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_FAST_SNS_CTRL_DELAY__A, 50);
	if (status < 0)
		goto error;

	status = write16(state, SCU_RAM_AGC_KI_MAXMINGAIN_TH__A, 20);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_CLP_SUM_MIN__A, clpSumMin);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_SNS_SUM_MIN__A, snsSumMin);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_CLP_DIR_TO__A, clpDirTo);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_SNS_DIR_TO__A, snsDirTo);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_KI_MINGAIN__A, 0x7fff);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_KI_MAXGAIN__A, 0x0);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_KI_MIN__A, 0x0117);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_KI_MAX__A, 0x0657);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_CLP_SUM__A, 0);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_CLP_CYCCNT__A, 0);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_CLP_DIR_WD__A, 0);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_CLP_DIR_STP__A, 1);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_SNS_SUM__A, 0);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_SNS_CYCCNT__A, 0);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_SNS_DIR_WD__A, 0);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_SNS_DIR_STP__A, 1);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_SNS_CYCLEN__A, 500);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_KI_CYCLEN__A, 500);
	if (status < 0)
		goto error;

	
	status = read16(state, SCU_RAM_AGC_KI__A, &data);
	if (status < 0)
		goto error;

	data = 0x0657;
	data &= ~SCU_RAM_AGC_KI_RF__M;
	data |= (DRXK_KI_RAGC_QAM << SCU_RAM_AGC_KI_RF__B);
	data &= ~SCU_RAM_AGC_KI_IF__M;
	data |= (DRXK_KI_IAGC_QAM << SCU_RAM_AGC_KI_IF__B);

	status = write16(state, SCU_RAM_AGC_KI__A, data);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int DVBTQAMGetAccPktErr(struct drxk_state *state, u16 *packetErr)
{
	int status;

	dprintk(1, "\n");
	if (packetErr == NULL)
		status = write16(state, SCU_RAM_FEC_ACCUM_PKT_FAILURES__A, 0);
	else
		status = read16(state, SCU_RAM_FEC_ACCUM_PKT_FAILURES__A, packetErr);
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int DVBTScCommand(struct drxk_state *state,
			 u16 cmd, u16 subcmd,
			 u16 param0, u16 param1, u16 param2,
			 u16 param3, u16 param4)
{
	u16 curCmd = 0;
	u16 errCode = 0;
	u16 retryCnt = 0;
	u16 scExec = 0;
	int status;

	dprintk(1, "\n");
	status = read16(state, OFDM_SC_COMM_EXEC__A, &scExec);
	if (scExec != 1) {
		
		status = -EINVAL;
	}
	if (status < 0)
		goto error;

	
	retryCnt = 0;
	do {
		msleep(1);
		status = read16(state, OFDM_SC_RA_RAM_CMD__A, &curCmd);
		retryCnt++;
	} while ((curCmd != 0) && (retryCnt < DRXK_MAX_RETRIES));
	if (retryCnt >= DRXK_MAX_RETRIES && (status < 0))
		goto error;

	
	switch (cmd) {
		
	case OFDM_SC_RA_RAM_CMD_PROC_START:
	case OFDM_SC_RA_RAM_CMD_SET_PREF_PARAM:
	case OFDM_SC_RA_RAM_CMD_PROGRAM_PARAM:
		status = write16(state, OFDM_SC_RA_RAM_CMD_ADDR__A, subcmd);
		if (status < 0)
			goto error;
		break;
	default:
		
		break;
	}

	
	switch (cmd) {
		
		
		
		
	case OFDM_SC_RA_RAM_CMD_PROC_START:
	case OFDM_SC_RA_RAM_CMD_SET_PREF_PARAM:
	case OFDM_SC_RA_RAM_CMD_PROGRAM_PARAM:
		status = write16(state, OFDM_SC_RA_RAM_PARAM1__A, param1);
		
	case OFDM_SC_RA_RAM_CMD_SET_ECHO_TIMING:
	case OFDM_SC_RA_RAM_CMD_USER_IO:
		status = write16(state, OFDM_SC_RA_RAM_PARAM0__A, param0);
		
	case OFDM_SC_RA_RAM_CMD_GET_OP_PARAM:
	case OFDM_SC_RA_RAM_CMD_NULL:
		
		status = write16(state, OFDM_SC_RA_RAM_CMD__A, cmd);
		break;
	default:
		
		status = -EINVAL;
	}
	if (status < 0)
		goto error;

	
	retryCnt = 0;
	do {
		msleep(1);
		status = read16(state, OFDM_SC_RA_RAM_CMD__A, &curCmd);
		retryCnt++;
	} while ((curCmd != 0) && (retryCnt < DRXK_MAX_RETRIES));
	if (retryCnt >= DRXK_MAX_RETRIES && (status < 0))
		goto error;

	
	status = read16(state, OFDM_SC_RA_RAM_CMD_ADDR__A, &errCode);
	if (errCode == 0xFFFF) {
		
		status = -EINVAL;
	}
	if (status < 0)
		goto error;

	
	switch (cmd) {
		
		
		
		
		
	case OFDM_SC_RA_RAM_CMD_USER_IO:
	case OFDM_SC_RA_RAM_CMD_GET_OP_PARAM:
		status = read16(state, OFDM_SC_RA_RAM_PARAM0__A, &(param0));
		
	case OFDM_SC_RA_RAM_CMD_SET_ECHO_TIMING:
	case OFDM_SC_RA_RAM_CMD_SET_TIMER:
	case OFDM_SC_RA_RAM_CMD_PROC_START:
	case OFDM_SC_RA_RAM_CMD_SET_PREF_PARAM:
	case OFDM_SC_RA_RAM_CMD_PROGRAM_PARAM:
	case OFDM_SC_RA_RAM_CMD_NULL:
		break;
	default:
		
		status = -EINVAL;
		break;
	}			
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int PowerUpDVBT(struct drxk_state *state)
{
	enum DRXPowerMode powerMode = DRX_POWER_UP;
	int status;

	dprintk(1, "\n");
	status = CtrlPowerMode(state, &powerMode);
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int DVBTCtrlSetIncEnable(struct drxk_state *state, bool *enabled)
{
	int status;

	dprintk(1, "\n");
	if (*enabled == true)
		status = write16(state, IQM_CF_BYPASSDET__A, 0);
	else
		status = write16(state, IQM_CF_BYPASSDET__A, 1);
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

#define DEFAULT_FR_THRES_8K     4000
static int DVBTCtrlSetFrEnable(struct drxk_state *state, bool *enabled)
{

	int status;

	dprintk(1, "\n");
	if (*enabled == true) {
		
		status = write16(state, OFDM_SC_RA_RAM_FR_THRES_8K__A,
				   DEFAULT_FR_THRES_8K);
	} else {
		
		status = write16(state, OFDM_SC_RA_RAM_FR_THRES_8K__A, 0);
	}
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);

	return status;
}

static int DVBTCtrlSetEchoThreshold(struct drxk_state *state,
				    struct DRXKCfgDvbtEchoThres_t *echoThres)
{
	u16 data = 0;
	int status;

	dprintk(1, "\n");
	status = read16(state, OFDM_SC_RA_RAM_ECHO_THRES__A, &data);
	if (status < 0)
		goto error;

	switch (echoThres->fftMode) {
	case DRX_FFTMODE_2K:
		data &= ~OFDM_SC_RA_RAM_ECHO_THRES_2K__M;
		data |= ((echoThres->threshold <<
			OFDM_SC_RA_RAM_ECHO_THRES_2K__B)
			& (OFDM_SC_RA_RAM_ECHO_THRES_2K__M));
		break;
	case DRX_FFTMODE_8K:
		data &= ~OFDM_SC_RA_RAM_ECHO_THRES_8K__M;
		data |= ((echoThres->threshold <<
			OFDM_SC_RA_RAM_ECHO_THRES_8K__B)
			& (OFDM_SC_RA_RAM_ECHO_THRES_8K__M));
		break;
	default:
		return -EINVAL;
	}

	status = write16(state, OFDM_SC_RA_RAM_ECHO_THRES__A, data);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int DVBTCtrlSetSqiSpeed(struct drxk_state *state,
			       enum DRXKCfgDvbtSqiSpeed *speed)
{
	int status = -EINVAL;

	dprintk(1, "\n");

	switch (*speed) {
	case DRXK_DVBT_SQI_SPEED_FAST:
	case DRXK_DVBT_SQI_SPEED_MEDIUM:
	case DRXK_DVBT_SQI_SPEED_SLOW:
		break;
	default:
		goto error;
	}
	status = write16(state, SCU_RAM_FEC_PRE_RS_BER_FILTER_SH__A,
			   (u16) *speed);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}


static int DVBTActivatePresets(struct drxk_state *state)
{
	int status;
	bool setincenable = false;
	bool setfrenable = true;

	struct DRXKCfgDvbtEchoThres_t echoThres2k = { 0, DRX_FFTMODE_2K };
	struct DRXKCfgDvbtEchoThres_t echoThres8k = { 0, DRX_FFTMODE_8K };

	dprintk(1, "\n");
	status = DVBTCtrlSetIncEnable(state, &setincenable);
	if (status < 0)
		goto error;
	status = DVBTCtrlSetFrEnable(state, &setfrenable);
	if (status < 0)
		goto error;
	status = DVBTCtrlSetEchoThreshold(state, &echoThres2k);
	if (status < 0)
		goto error;
	status = DVBTCtrlSetEchoThreshold(state, &echoThres8k);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_AGC_INGAIN_TGT_MAX__A, state->m_dvbtIfAgcCfg.IngainTgtMax);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}


static int SetDVBTStandard(struct drxk_state *state,
			   enum OperationMode oMode)
{
	u16 cmdResult = 0;
	u16 data = 0;
	int status;

	dprintk(1, "\n");

	PowerUpDVBT(state);
	
	SwitchAntennaToDVBT(state);
	
	status = scu_command(state, SCU_RAM_COMMAND_STANDARD_OFDM | SCU_RAM_COMMAND_CMD_DEMOD_RESET, 0, NULL, 1, &cmdResult);
	if (status < 0)
		goto error;

	
	status = scu_command(state, SCU_RAM_COMMAND_STANDARD_OFDM | SCU_RAM_COMMAND_CMD_DEMOD_SET_ENV, 0, NULL, 1, &cmdResult);
	if (status < 0)
		goto error;

	
	status = write16(state, OFDM_SC_COMM_EXEC__A, OFDM_SC_COMM_EXEC_STOP);
	if (status < 0)
		goto error;
	status = write16(state, OFDM_LC_COMM_EXEC__A, OFDM_LC_COMM_EXEC_STOP);
	if (status < 0)
		goto error;
	status = write16(state, IQM_COMM_EXEC__A, IQM_COMM_EXEC_B_STOP);
	if (status < 0)
		goto error;

	
	
	status = write16(state, IQM_AF_UPD_SEL__A, 1);
	if (status < 0)
		goto error;
	
	status = write16(state, IQM_AF_CLP_LEN__A, 0);
	if (status < 0)
		goto error;
	
	status = write16(state, IQM_AF_SNS_LEN__A, 0);
	if (status < 0)
		goto error;
	
	status = write16(state, IQM_AF_AMUX__A, IQM_AF_AMUX_SIGNAL2ADC);
	if (status < 0)
		goto error;
	status = SetIqmAf(state, true);
	if (status < 0)
		goto error;

	status = write16(state, IQM_AF_AGC_RF__A, 0);
	if (status < 0)
		goto error;

	
	status = write16(state, IQM_AF_INC_LCT__A, 0);	
	if (status < 0)
		goto error;
	status = write16(state, IQM_CF_DET_LCT__A, 0);	
	if (status < 0)
		goto error;
	status = write16(state, IQM_CF_WND_LEN__A, 3);	
	if (status < 0)
		goto error;

	status = write16(state, IQM_RC_STRETCH__A, 16);
	if (status < 0)
		goto error;
	status = write16(state, IQM_CF_OUT_ENA__A, 0x4);	
	if (status < 0)
		goto error;
	status = write16(state, IQM_CF_DS_ENA__A, 0x4);	
	if (status < 0)
		goto error;
	status = write16(state, IQM_CF_SCALE__A, 1600);
	if (status < 0)
		goto error;
	status = write16(state, IQM_CF_SCALE_SH__A, 0);
	if (status < 0)
		goto error;

	
	status = write16(state, IQM_AF_CLP_TH__A, 448);
	if (status < 0)
		goto error;
	status = write16(state, IQM_CF_DATATH__A, 495);	
	if (status < 0)
		goto error;

	status = BLChainCmd(state, DRXK_BL_ROM_OFFSET_TAPS_DVBT, DRXK_BLCC_NR_ELEMENTS_TAPS, DRXK_BLC_TIMEOUT);
	if (status < 0)
		goto error;

	status = write16(state, IQM_CF_PKDTH__A, 2);	
	if (status < 0)
		goto error;
	status = write16(state, IQM_CF_POW_MEAS_LEN__A, 2);
	if (status < 0)
		goto error;
	
	status = write16(state, IQM_CF_COMM_INT_MSK__A, 1);
	if (status < 0)
		goto error;
	status = write16(state, IQM_COMM_EXEC__A, IQM_COMM_EXEC_B_ACTIVE);
	if (status < 0)
		goto error;

	
	status = ADCSynchronization(state);
	if (status < 0)
		goto error;
	status = SetPreSaw(state, &state->m_dvbtPreSawCfg);
	if (status < 0)
		goto error;

	
	status = write16(state, SCU_COMM_EXEC__A, SCU_COMM_EXEC_HOLD);
	if (status < 0)
		goto error;

	status = SetAgcRf(state, &state->m_dvbtRfAgcCfg, true);
	if (status < 0)
		goto error;
	status = SetAgcIf(state, &state->m_dvbtIfAgcCfg, true);
	if (status < 0)
		goto error;

	
	status = read16(state, OFDM_SC_RA_RAM_CONFIG__A, &data);
	if (status < 0)
		goto error;
	data |= OFDM_SC_RA_RAM_CONFIG_NE_FIX_ENABLE__M;
	status = write16(state, OFDM_SC_RA_RAM_CONFIG__A, data);
	if (status < 0)
		goto error;

	
	status = write16(state, SCU_COMM_EXEC__A, SCU_COMM_EXEC_ACTIVE);
	if (status < 0)
		goto error;

	if (!state->m_DRXK_A3_ROM_CODE) {
		
		status = write16(state, SCU_RAM_AGC_FAST_CLP_CTRL_DELAY__A, state->m_dvbtIfAgcCfg.FastClipCtrlDelay);
		if (status < 0)
			goto error;
	}

	
#ifdef COMPILE_FOR_NONRT
	status = write16(state, OFDM_SC_RA_RAM_BE_OPT_DELAY__A, 1);
	if (status < 0)
		goto error;
	status = write16(state, OFDM_SC_RA_RAM_BE_OPT_INIT_DELAY__A, 2);
	if (status < 0)
		goto error;
#endif

	
	status = write16(state, FEC_DI_INPUT_CTL__A, 1);	
	if (status < 0)
		goto error;


#ifdef COMPILE_FOR_NONRT
	status = write16(state, FEC_RS_MEASUREMENT_PERIOD__A, 0x400);
	if (status < 0)
		goto error;
#else
	status = write16(state, FEC_RS_MEASUREMENT_PERIOD__A, 0x1000);
	if (status < 0)
		goto error;
#endif
	status = write16(state, FEC_RS_MEASUREMENT_PRESCALE__A, 0x0001);
	if (status < 0)
		goto error;

	
	status = MPEGTSDtoSetup(state, OM_DVBT);
	if (status < 0)
		goto error;
	
	status = DVBTActivatePresets(state);
	if (status < 0)
		goto error;

error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int DVBTStart(struct drxk_state *state)
{
	u16 param1;
	int status;
	

	dprintk(1, "\n");
	
	
	param1 = OFDM_SC_RA_RAM_LOCKTRACK_MIN;
	status = DVBTScCommand(state, OFDM_SC_RA_RAM_CMD_PROC_START, 0, OFDM_SC_RA_RAM_SW_EVENT_RUN_NMASK__M, param1, 0, 0, 0);
	if (status < 0)
		goto error;
	
	status = MPEGTSStart(state);
	if (status < 0)
		goto error;
	status = write16(state, FEC_COMM_EXEC__A, FEC_COMM_EXEC_ACTIVE);
	if (status < 0)
		goto error;
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}



static int SetDVBT(struct drxk_state *state, u16 IntermediateFreqkHz,
		   s32 tunerFreqOffset)
{
	u16 cmdResult = 0;
	u16 transmissionParams = 0;
	u16 operationMode = 0;
	u32 iqmRcRateOfs = 0;
	u32 bandwidth = 0;
	u16 param1;
	int status;

	dprintk(1, "IF =%d, TFO = %d\n", IntermediateFreqkHz, tunerFreqOffset);

	status = scu_command(state, SCU_RAM_COMMAND_STANDARD_OFDM | SCU_RAM_COMMAND_CMD_DEMOD_STOP, 0, NULL, 1, &cmdResult);
	if (status < 0)
		goto error;

	
	status = write16(state, SCU_COMM_EXEC__A, SCU_COMM_EXEC_HOLD);
	if (status < 0)
		goto error;

	
	status = write16(state, OFDM_SC_COMM_EXEC__A, OFDM_SC_COMM_EXEC_STOP);
	if (status < 0)
		goto error;
	status = write16(state, OFDM_LC_COMM_EXEC__A, OFDM_LC_COMM_EXEC_STOP);
	if (status < 0)
		goto error;

	status = write16(state, OFDM_CP_COMM_EXEC__A, OFDM_CP_COMM_EXEC_STOP);
	if (status < 0)
		goto error;

	

	
	switch (state->props.transmission_mode) {
	case TRANSMISSION_MODE_AUTO:
	default:
		operationMode |= OFDM_SC_RA_RAM_OP_AUTO_MODE__M;
		
	case TRANSMISSION_MODE_8K:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_MODE_8K;
		break;
	case TRANSMISSION_MODE_2K:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_MODE_2K;
		break;
	}

	
	switch (state->props.guard_interval) {
	default:
	case GUARD_INTERVAL_AUTO:
		operationMode |= OFDM_SC_RA_RAM_OP_AUTO_GUARD__M;
		
	case GUARD_INTERVAL_1_4:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_GUARD_4;
		break;
	case GUARD_INTERVAL_1_32:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_GUARD_32;
		break;
	case GUARD_INTERVAL_1_16:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_GUARD_16;
		break;
	case GUARD_INTERVAL_1_8:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_GUARD_8;
		break;
	}

	
	switch (state->props.hierarchy) {
	case HIERARCHY_AUTO:
	case HIERARCHY_NONE:
	default:
		operationMode |= OFDM_SC_RA_RAM_OP_AUTO_HIER__M;
		
		
		
	case HIERARCHY_1:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_HIER_A1;
		break;
	case HIERARCHY_2:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_HIER_A2;
		break;
	case HIERARCHY_4:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_HIER_A4;
		break;
	}


	
	switch (state->props.modulation) {
	case QAM_AUTO:
	default:
		operationMode |= OFDM_SC_RA_RAM_OP_AUTO_CONST__M;
		
	case QAM_64:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_CONST_QAM64;
		break;
	case QPSK:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_CONST_QPSK;
		break;
	case QAM_16:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_CONST_QAM16;
		break;
	}
#if 0
	
	
	switch (channel->priority) {
	case DRX_PRIORITY_LOW:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_PRIO_LO;
		WR16(devAddr, OFDM_EC_SB_PRIOR__A,
			OFDM_EC_SB_PRIOR_LO);
		break;
	case DRX_PRIORITY_HIGH:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_PRIO_HI;
		WR16(devAddr, OFDM_EC_SB_PRIOR__A,
			OFDM_EC_SB_PRIOR_HI));
		break;
	case DRX_PRIORITY_UNKNOWN:	
	default:
		status = -EINVAL;
		goto error;
	}
#else
	
	transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_PRIO_HI;
	status = write16(state, OFDM_EC_SB_PRIOR__A, OFDM_EC_SB_PRIOR_HI);
	if (status < 0)
		goto error;
#endif

	
	switch (state->props.code_rate_HP) {
	case FEC_AUTO:
	default:
		operationMode |= OFDM_SC_RA_RAM_OP_AUTO_RATE__M;
		
	case FEC_2_3:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_RATE_2_3;
		break;
	case FEC_1_2:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_RATE_1_2;
		break;
	case FEC_3_4:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_RATE_3_4;
		break;
	case FEC_5_6:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_RATE_5_6;
		break;
	case FEC_7_8:
		transmissionParams |= OFDM_SC_RA_RAM_OP_PARAM_RATE_7_8;
		break;
	}

	
	
	switch (state->props.bandwidth_hz) {
	case 0:
		state->props.bandwidth_hz = 8000000;
		
	case 8000000:
		bandwidth = DRXK_BANDWIDTH_8MHZ_IN_HZ;
		status = write16(state, OFDM_SC_RA_RAM_SRMM_FIX_FACT_8K__A, 3052);
		if (status < 0)
			goto error;
		
		status = write16(state, OFDM_SC_RA_RAM_NI_INIT_8K_PER_LEFT__A, 7);
		if (status < 0)
			goto error;
		status = write16(state, OFDM_SC_RA_RAM_NI_INIT_8K_PER_RIGHT__A, 7);
		if (status < 0)
			goto error;
		status = write16(state, OFDM_SC_RA_RAM_NI_INIT_2K_PER_LEFT__A, 7);
		if (status < 0)
			goto error;
		status = write16(state, OFDM_SC_RA_RAM_NI_INIT_2K_PER_RIGHT__A, 1);
		if (status < 0)
			goto error;
		break;
	case 7000000:
		bandwidth = DRXK_BANDWIDTH_7MHZ_IN_HZ;
		status = write16(state, OFDM_SC_RA_RAM_SRMM_FIX_FACT_8K__A, 3491);
		if (status < 0)
			goto error;
		
		status = write16(state, OFDM_SC_RA_RAM_NI_INIT_8K_PER_LEFT__A, 8);
		if (status < 0)
			goto error;
		status = write16(state, OFDM_SC_RA_RAM_NI_INIT_8K_PER_RIGHT__A, 8);
		if (status < 0)
			goto error;
		status = write16(state, OFDM_SC_RA_RAM_NI_INIT_2K_PER_LEFT__A, 4);
		if (status < 0)
			goto error;
		status = write16(state, OFDM_SC_RA_RAM_NI_INIT_2K_PER_RIGHT__A, 1);
		if (status < 0)
			goto error;
		break;
	case 6000000:
		bandwidth = DRXK_BANDWIDTH_6MHZ_IN_HZ;
		status = write16(state, OFDM_SC_RA_RAM_SRMM_FIX_FACT_8K__A, 4073);
		if (status < 0)
			goto error;
		
		status = write16(state, OFDM_SC_RA_RAM_NI_INIT_8K_PER_LEFT__A, 19);
		if (status < 0)
			goto error;
		status = write16(state, OFDM_SC_RA_RAM_NI_INIT_8K_PER_RIGHT__A, 19);
		if (status < 0)
			goto error;
		status = write16(state, OFDM_SC_RA_RAM_NI_INIT_2K_PER_LEFT__A, 14);
		if (status < 0)
			goto error;
		status = write16(state, OFDM_SC_RA_RAM_NI_INIT_2K_PER_RIGHT__A, 1);
		if (status < 0)
			goto error;
		break;
	default:
		status = -EINVAL;
		goto error;
	}

	if (iqmRcRateOfs == 0) {
		
		iqmRcRateOfs = Frac28a((u32)
					((state->m_sysClockFreq *
						1000) / 3), bandwidth);
		
		if ((iqmRcRateOfs & 0x7fL) >= 0x40)
			iqmRcRateOfs += 0x80L;
		iqmRcRateOfs = iqmRcRateOfs >> 7;
		
		iqmRcRateOfs = iqmRcRateOfs - (1 << 23);
	}

	iqmRcRateOfs &=
		((((u32) IQM_RC_RATE_OFS_HI__M) <<
		IQM_RC_RATE_OFS_LO__W) | IQM_RC_RATE_OFS_LO__M);
	status = write32(state, IQM_RC_RATE_OFS_LO__A, iqmRcRateOfs);
	if (status < 0)
		goto error;

	

#if 0
	status = DVBTSetFrequencyShift(demod, channel, tunerOffset);
	if (status < 0)
		goto error;
#endif
	status = SetFrequencyShifter(state, IntermediateFreqkHz, tunerFreqOffset, true);
	if (status < 0)
		goto error;

	

	
	status = write16(state, SCU_COMM_EXEC__A, SCU_COMM_EXEC_ACTIVE);
	if (status < 0)
		goto error;

	
	status = write16(state, OFDM_SC_COMM_STATE__A, 0);
	if (status < 0)
		goto error;
	status = write16(state, OFDM_SC_COMM_EXEC__A, 1);
	if (status < 0)
		goto error;


	status = scu_command(state, SCU_RAM_COMMAND_STANDARD_OFDM | SCU_RAM_COMMAND_CMD_DEMOD_START, 0, NULL, 1, &cmdResult);
	if (status < 0)
		goto error;

	
	param1 = (OFDM_SC_RA_RAM_OP_AUTO_MODE__M |
			OFDM_SC_RA_RAM_OP_AUTO_GUARD__M |
			OFDM_SC_RA_RAM_OP_AUTO_CONST__M |
			OFDM_SC_RA_RAM_OP_AUTO_HIER__M |
			OFDM_SC_RA_RAM_OP_AUTO_RATE__M);
	status = DVBTScCommand(state, OFDM_SC_RA_RAM_CMD_SET_PREF_PARAM,
				0, transmissionParams, param1, 0, 0, 0);
	if (status < 0)
		goto error;

	if (!state->m_DRXK_A3_ROM_CODE)
		status = DVBTCtrlSetSqiSpeed(state, &state->m_sqiSpeed);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);

	return status;
}



static int GetDVBTLockStatus(struct drxk_state *state, u32 *pLockStatus)
{
	int status;
	const u16 mpeg_lock_mask = (OFDM_SC_RA_RAM_LOCK_MPEG__M |
				    OFDM_SC_RA_RAM_LOCK_FEC__M);
	const u16 fec_lock_mask = (OFDM_SC_RA_RAM_LOCK_FEC__M);
	const u16 demod_lock_mask = OFDM_SC_RA_RAM_LOCK_DEMOD__M;

	u16 ScRaRamLock = 0;
	u16 ScCommExec = 0;

	dprintk(1, "\n");

	*pLockStatus = NOT_LOCKED;
	
	
	status = read16(state, OFDM_SC_COMM_EXEC__A, &ScCommExec);
	if (status < 0)
		goto end;
	if (ScCommExec == OFDM_SC_COMM_EXEC_STOP)
		goto end;

	status = read16(state, OFDM_SC_RA_RAM_LOCK__A, &ScRaRamLock);
	if (status < 0)
		goto end;

	if ((ScRaRamLock & mpeg_lock_mask) == mpeg_lock_mask)
		*pLockStatus = MPEG_LOCK;
	else if ((ScRaRamLock & fec_lock_mask) == fec_lock_mask)
		*pLockStatus = FEC_LOCK;
	else if ((ScRaRamLock & demod_lock_mask) == demod_lock_mask)
		*pLockStatus = DEMOD_LOCK;
	else if (ScRaRamLock & OFDM_SC_RA_RAM_LOCK_NODVBT__M)
		*pLockStatus = NEVER_LOCK;
end:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);

	return status;
}

static int PowerUpQAM(struct drxk_state *state)
{
	enum DRXPowerMode powerMode = DRXK_POWER_DOWN_OFDM;
	int status;

	dprintk(1, "\n");
	status = CtrlPowerMode(state, &powerMode);
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);

	return status;
}


static int PowerDownQAM(struct drxk_state *state)
{
	u16 data = 0;
	u16 cmdResult;
	int status = 0;

	dprintk(1, "\n");
	status = read16(state, SCU_COMM_EXEC__A, &data);
	if (status < 0)
		goto error;
	if (data == SCU_COMM_EXEC_ACTIVE) {
		
		status = write16(state, QAM_COMM_EXEC__A, QAM_COMM_EXEC_STOP);
		if (status < 0)
			goto error;
		status = scu_command(state, SCU_RAM_COMMAND_STANDARD_QAM | SCU_RAM_COMMAND_CMD_DEMOD_STOP, 0, NULL, 1, &cmdResult);
		if (status < 0)
			goto error;
	}
	
	status = SetIqmAf(state, false);

error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);

	return status;
}


static int SetQAMMeasurement(struct drxk_state *state,
			     enum EDrxkConstellation modulation,
			     u32 symbolRate)
{
	u32 fecBitsDesired = 0;	
	u32 fecRsPeriodTotal = 0;	
	u16 fecRsPrescale = 0;	
	u16 fecRsPeriod = 0;	
	int status = 0;

	dprintk(1, "\n");

	fecRsPrescale = 1;
	switch (modulation) {
	case DRX_CONSTELLATION_QAM16:
		fecBitsDesired = 4 * symbolRate;
		break;
	case DRX_CONSTELLATION_QAM32:
		fecBitsDesired = 5 * symbolRate;
		break;
	case DRX_CONSTELLATION_QAM64:
		fecBitsDesired = 6 * symbolRate;
		break;
	case DRX_CONSTELLATION_QAM128:
		fecBitsDesired = 7 * symbolRate;
		break;
	case DRX_CONSTELLATION_QAM256:
		fecBitsDesired = 8 * symbolRate;
		break;
	default:
		status = -EINVAL;
	}
	if (status < 0)
		goto error;

	fecBitsDesired /= 1000;	
	fecBitsDesired *= 500;	

	
	
	fecRsPeriodTotal = (fecBitsDesired / 1632UL) + 1;	

	
	fecRsPrescale = 1 + (u16) (fecRsPeriodTotal >> 16);
	if (fecRsPrescale == 0) {
		
		status = -EINVAL;
		if (status < 0)
			goto error;
	}
	fecRsPeriod =
		((u16) fecRsPeriodTotal +
		(fecRsPrescale >> 1)) / fecRsPrescale;

	
	status = write16(state, FEC_RS_MEASUREMENT_PERIOD__A, fecRsPeriod);
	if (status < 0)
		goto error;
	status = write16(state, FEC_RS_MEASUREMENT_PRESCALE__A, fecRsPrescale);
	if (status < 0)
		goto error;
	status = write16(state, FEC_OC_SNC_FAIL_PERIOD__A, fecRsPeriod);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int SetQAM16(struct drxk_state *state)
{
	int status = 0;

	dprintk(1, "\n");
	
	
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD0__A, 13517);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD1__A, 13517);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD2__A, 13517);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD3__A, 13517);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD4__A, 13517);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD5__A, 13517);
	if (status < 0)
		goto error;
	
	status = write16(state, QAM_DQ_QUAL_FUN0__A, 2);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN1__A, 2);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN2__A, 2);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN3__A, 2);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN4__A, 2);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN5__A, 0);
	if (status < 0)
		goto error;

	status = write16(state, QAM_SY_SYNC_HWM__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, QAM_SY_SYNC_AWM__A, 4);
	if (status < 0)
		goto error;
	status = write16(state, QAM_SY_SYNC_LWM__A, 3);
	if (status < 0)
		goto error;

	
	status = write16(state, SCU_RAM_QAM_SL_SIG_POWER__A, DRXK_QAM_SL_SIG_POWER_QAM16);
	if (status < 0)
		goto error;

	
	status = write16(state, SCU_RAM_QAM_LC_CA_FINE__A, 15);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CA_COARSE__A, 40);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EP_FINE__A, 12);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EP_MEDIUM__A, 24);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EP_COARSE__A, 24);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EI_FINE__A, 12);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EI_MEDIUM__A, 16);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EI_COARSE__A, 16);
	if (status < 0)
		goto error;

	status = write16(state, SCU_RAM_QAM_LC_CP_FINE__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CP_MEDIUM__A, 20);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CP_COARSE__A, 80);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CI_FINE__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CI_MEDIUM__A, 20);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CI_COARSE__A, 50);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF_FINE__A, 16);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF_MEDIUM__A, 16);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF_COARSE__A, 32);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF1_FINE__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF1_MEDIUM__A, 10);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF1_COARSE__A, 10);
	if (status < 0)
		goto error;


	

	status = write16(state, SCU_RAM_QAM_FSM_RTH__A, 140);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_FTH__A, 50);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_CTH__A, 95);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_PTH__A, 120);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_QTH__A, 230);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_MTH__A, 105);
	if (status < 0)
		goto error;

	status = write16(state, SCU_RAM_QAM_FSM_RATE_LIM__A, 40);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_COUNT_LIM__A, 4);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_FREQ_LIM__A, 24);
	if (status < 0)
		goto error;


	

	status = write16(state, SCU_RAM_QAM_FSM_MEDIAN_AV_MULT__A, (u16) 16);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_RADIUS_AV_LIMIT__A, (u16) 220);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET1__A, (u16) 25);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET2__A, (u16) 6);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET3__A, (u16) -24);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET4__A, (u16) -65);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET5__A, (u16) -127);
	if (status < 0)
		goto error;

error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}


static int SetQAM32(struct drxk_state *state)
{
	int status = 0;

	dprintk(1, "\n");

	
	
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD0__A, 6707);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD1__A, 6707);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD2__A, 6707);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD3__A, 6707);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD4__A, 6707);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD5__A, 6707);
	if (status < 0)
		goto error;

	
	status = write16(state, QAM_DQ_QUAL_FUN0__A, 3);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN1__A, 3);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN2__A, 3);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN3__A, 3);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN4__A, 3);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN5__A, 0);
	if (status < 0)
		goto error;

	status = write16(state, QAM_SY_SYNC_HWM__A, 6);
	if (status < 0)
		goto error;
	status = write16(state, QAM_SY_SYNC_AWM__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, QAM_SY_SYNC_LWM__A, 3);
	if (status < 0)
		goto error;

	

	status = write16(state, SCU_RAM_QAM_SL_SIG_POWER__A, DRXK_QAM_SL_SIG_POWER_QAM32);
	if (status < 0)
		goto error;


	

	status = write16(state, SCU_RAM_QAM_LC_CA_FINE__A, 15);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CA_COARSE__A, 40);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EP_FINE__A, 12);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EP_MEDIUM__A, 24);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EP_COARSE__A, 24);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EI_FINE__A, 12);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EI_MEDIUM__A, 16);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EI_COARSE__A, 16);
	if (status < 0)
		goto error;

	status = write16(state, SCU_RAM_QAM_LC_CP_FINE__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CP_MEDIUM__A, 20);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CP_COARSE__A, 80);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CI_FINE__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CI_MEDIUM__A, 20);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CI_COARSE__A, 50);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF_FINE__A, 16);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF_MEDIUM__A, 16);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF_COARSE__A, 16);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF1_FINE__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF1_MEDIUM__A, 10);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF1_COARSE__A, 0);
	if (status < 0)
		goto error;


	

	status = write16(state, SCU_RAM_QAM_FSM_RTH__A, 90);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_FTH__A, 50);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_CTH__A, 80);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_PTH__A, 100);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_QTH__A, 170);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_MTH__A, 100);
	if (status < 0)
		goto error;

	status = write16(state, SCU_RAM_QAM_FSM_RATE_LIM__A, 40);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_COUNT_LIM__A, 4);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_FREQ_LIM__A, 10);
	if (status < 0)
		goto error;


	

	status = write16(state, SCU_RAM_QAM_FSM_MEDIAN_AV_MULT__A, (u16) 12);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_RADIUS_AV_LIMIT__A, (u16) 140);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET1__A, (u16) -8);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET2__A, (u16) -16);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET3__A, (u16) -26);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET4__A, (u16) -56);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET5__A, (u16) -86);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}


static int SetQAM64(struct drxk_state *state)
{
	int status = 0;

	dprintk(1, "\n");
	
	
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD0__A, 13336);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD1__A, 12618);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD2__A, 11988);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD3__A, 13809);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD4__A, 13809);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD5__A, 15609);
	if (status < 0)
		goto error;

	
	status = write16(state, QAM_DQ_QUAL_FUN0__A, 4);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN1__A, 4);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN2__A, 4);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN3__A, 4);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN4__A, 3);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN5__A, 0);
	if (status < 0)
		goto error;

	status = write16(state, QAM_SY_SYNC_HWM__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, QAM_SY_SYNC_AWM__A, 4);
	if (status < 0)
		goto error;
	status = write16(state, QAM_SY_SYNC_LWM__A, 3);
	if (status < 0)
		goto error;

	
	status = write16(state, SCU_RAM_QAM_SL_SIG_POWER__A, DRXK_QAM_SL_SIG_POWER_QAM64);
	if (status < 0)
		goto error;


	

	status = write16(state, SCU_RAM_QAM_LC_CA_FINE__A, 15);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CA_COARSE__A, 40);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EP_FINE__A, 12);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EP_MEDIUM__A, 24);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EP_COARSE__A, 24);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EI_FINE__A, 12);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EI_MEDIUM__A, 16);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EI_COARSE__A, 16);
	if (status < 0)
		goto error;

	status = write16(state, SCU_RAM_QAM_LC_CP_FINE__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CP_MEDIUM__A, 30);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CP_COARSE__A, 100);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CI_FINE__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CI_MEDIUM__A, 30);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CI_COARSE__A, 50);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF_FINE__A, 16);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF_MEDIUM__A, 25);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF_COARSE__A, 48);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF1_FINE__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF1_MEDIUM__A, 10);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF1_COARSE__A, 10);
	if (status < 0)
		goto error;


	

	status = write16(state, SCU_RAM_QAM_FSM_RTH__A, 100);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_FTH__A, 60);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_CTH__A, 80);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_PTH__A, 110);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_QTH__A, 200);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_MTH__A, 95);
	if (status < 0)
		goto error;

	status = write16(state, SCU_RAM_QAM_FSM_RATE_LIM__A, 40);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_COUNT_LIM__A, 4);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_FREQ_LIM__A, 15);
	if (status < 0)
		goto error;


	

	status = write16(state, SCU_RAM_QAM_FSM_MEDIAN_AV_MULT__A, (u16) 12);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_RADIUS_AV_LIMIT__A, (u16) 141);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET1__A, (u16) 7);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET2__A, (u16) 0);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET3__A, (u16) -15);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET4__A, (u16) -45);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET5__A, (u16) -80);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);

	return status;
}


static int SetQAM128(struct drxk_state *state)
{
	int status = 0;

	dprintk(1, "\n");
	
	
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD0__A, 6564);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD1__A, 6598);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD2__A, 6394);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD3__A, 6409);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD4__A, 6656);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD5__A, 7238);
	if (status < 0)
		goto error;

	
	status = write16(state, QAM_DQ_QUAL_FUN0__A, 6);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN1__A, 6);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN2__A, 6);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN3__A, 6);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN4__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN5__A, 0);
	if (status < 0)
		goto error;

	status = write16(state, QAM_SY_SYNC_HWM__A, 6);
	if (status < 0)
		goto error;
	status = write16(state, QAM_SY_SYNC_AWM__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, QAM_SY_SYNC_LWM__A, 3);
	if (status < 0)
		goto error;


	

	status = write16(state, SCU_RAM_QAM_SL_SIG_POWER__A, DRXK_QAM_SL_SIG_POWER_QAM128);
	if (status < 0)
		goto error;


	

	status = write16(state, SCU_RAM_QAM_LC_CA_FINE__A, 15);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CA_COARSE__A, 40);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EP_FINE__A, 12);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EP_MEDIUM__A, 24);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EP_COARSE__A, 24);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EI_FINE__A, 12);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EI_MEDIUM__A, 16);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EI_COARSE__A, 16);
	if (status < 0)
		goto error;

	status = write16(state, SCU_RAM_QAM_LC_CP_FINE__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CP_MEDIUM__A, 40);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CP_COARSE__A, 120);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CI_FINE__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CI_MEDIUM__A, 40);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CI_COARSE__A, 60);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF_FINE__A, 16);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF_MEDIUM__A, 25);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF_COARSE__A, 64);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF1_FINE__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF1_MEDIUM__A, 10);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF1_COARSE__A, 0);
	if (status < 0)
		goto error;


	

	status = write16(state, SCU_RAM_QAM_FSM_RTH__A, 50);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_FTH__A, 60);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_CTH__A, 80);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_PTH__A, 100);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_QTH__A, 140);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_MTH__A, 100);
	if (status < 0)
		goto error;

	status = write16(state, SCU_RAM_QAM_FSM_RATE_LIM__A, 40);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_COUNT_LIM__A, 5);
	if (status < 0)
		goto error;

	status = write16(state, SCU_RAM_QAM_FSM_FREQ_LIM__A, 12);
	if (status < 0)
		goto error;

	

	status = write16(state, SCU_RAM_QAM_FSM_MEDIAN_AV_MULT__A, (u16) 8);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_RADIUS_AV_LIMIT__A, (u16) 65);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET1__A, (u16) 5);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET2__A, (u16) 3);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET3__A, (u16) -1);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET4__A, (u16) -12);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET5__A, (u16) -23);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);

	return status;
}


static int SetQAM256(struct drxk_state *state)
{
	int status = 0;

	dprintk(1, "\n");
	
	
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD0__A, 11502);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD1__A, 12084);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD2__A, 12543);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD3__A, 12931);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD4__A, 13629);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_EQ_CMA_RAD5__A, 15385);
	if (status < 0)
		goto error;

	
	status = write16(state, QAM_DQ_QUAL_FUN0__A, 8);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN1__A, 8);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN2__A, 8);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN3__A, 8);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN4__A, 6);
	if (status < 0)
		goto error;
	status = write16(state, QAM_DQ_QUAL_FUN5__A, 0);
	if (status < 0)
		goto error;

	status = write16(state, QAM_SY_SYNC_HWM__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, QAM_SY_SYNC_AWM__A, 4);
	if (status < 0)
		goto error;
	status = write16(state, QAM_SY_SYNC_LWM__A, 3);
	if (status < 0)
		goto error;

	

	status = write16(state, SCU_RAM_QAM_SL_SIG_POWER__A, DRXK_QAM_SL_SIG_POWER_QAM256);
	if (status < 0)
		goto error;


	

	status = write16(state, SCU_RAM_QAM_LC_CA_FINE__A, 15);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CA_COARSE__A, 40);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EP_FINE__A, 12);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EP_MEDIUM__A, 24);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EP_COARSE__A, 24);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EI_FINE__A, 12);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EI_MEDIUM__A, 16);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_EI_COARSE__A, 16);
	if (status < 0)
		goto error;

	status = write16(state, SCU_RAM_QAM_LC_CP_FINE__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CP_MEDIUM__A, 50);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CP_COARSE__A, 250);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CI_FINE__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CI_MEDIUM__A, 50);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CI_COARSE__A, 125);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF_FINE__A, 16);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF_MEDIUM__A, 25);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF_COARSE__A, 48);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF1_FINE__A, 5);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF1_MEDIUM__A, 10);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_LC_CF1_COARSE__A, 10);
	if (status < 0)
		goto error;


	

	status = write16(state, SCU_RAM_QAM_FSM_RTH__A, 50);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_FTH__A, 60);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_CTH__A, 80);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_PTH__A, 100);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_QTH__A, 150);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_MTH__A, 110);
	if (status < 0)
		goto error;

	status = write16(state, SCU_RAM_QAM_FSM_RATE_LIM__A, 40);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_COUNT_LIM__A, 4);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_FREQ_LIM__A, 12);
	if (status < 0)
		goto error;


	

	status = write16(state, SCU_RAM_QAM_FSM_MEDIAN_AV_MULT__A, (u16) 8);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_RADIUS_AV_LIMIT__A, (u16) 74);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET1__A, (u16) 18);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET2__A, (u16) 13);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET3__A, (u16) 7);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET4__A, (u16) 0);
	if (status < 0)
		goto error;
	status = write16(state, SCU_RAM_QAM_FSM_LCAVG_OFFSET5__A, (u16) -8);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}


static int QAMResetQAM(struct drxk_state *state)
{
	int status;
	u16 cmdResult;

	dprintk(1, "\n");
	
	status = write16(state, QAM_COMM_EXEC__A, QAM_COMM_EXEC_STOP);
	if (status < 0)
		goto error;

	status = scu_command(state, SCU_RAM_COMMAND_STANDARD_QAM | SCU_RAM_COMMAND_CMD_DEMOD_RESET, 0, NULL, 1, &cmdResult);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}


static int QAMSetSymbolrate(struct drxk_state *state)
{
	u32 adcFrequency = 0;
	u32 symbFreq = 0;
	u32 iqmRcRate = 0;
	u16 ratesel = 0;
	u32 lcSymbRate = 0;
	int status;

	dprintk(1, "\n");
	
	adcFrequency = (state->m_sysClockFreq * 1000) / 3;
	ratesel = 0;
	
	if (state->props.symbol_rate <= 1188750)
		ratesel = 3;
	else if (state->props.symbol_rate <= 2377500)
		ratesel = 2;
	else if (state->props.symbol_rate <= 4755000)
		ratesel = 1;
	status = write16(state, IQM_FD_RATESEL__A, ratesel);
	if (status < 0)
		goto error;

	symbFreq = state->props.symbol_rate * (1 << ratesel);
	if (symbFreq == 0) {
		
		status = -EINVAL;
		goto error;
	}
	iqmRcRate = (adcFrequency / symbFreq) * (1 << 21) +
		(Frac28a((adcFrequency % symbFreq), symbFreq) >> 7) -
		(1 << 23);
	status = write32(state, IQM_RC_RATE_OFS_LO__A, iqmRcRate);
	if (status < 0)
		goto error;
	state->m_iqmRcRate = iqmRcRate;
	symbFreq = state->props.symbol_rate;
	if (adcFrequency == 0) {
		
		status = -EINVAL;
		goto error;
	}
	lcSymbRate = (symbFreq / adcFrequency) * (1 << 12) +
		(Frac28a((symbFreq % adcFrequency), adcFrequency) >>
		16);
	if (lcSymbRate > 511)
		lcSymbRate = 511;
	status = write16(state, QAM_LC_SYMBOL_FREQ__A, (u16) lcSymbRate);

error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}



static int GetQAMLockStatus(struct drxk_state *state, u32 *pLockStatus)
{
	int status;
	u16 Result[2] = { 0, 0 };

	dprintk(1, "\n");
	*pLockStatus = NOT_LOCKED;
	status = scu_command(state,
			SCU_RAM_COMMAND_STANDARD_QAM |
			SCU_RAM_COMMAND_CMD_DEMOD_GET_LOCK, 0, NULL, 2,
			Result);
	if (status < 0)
		printk(KERN_ERR "drxk: %s status = %08x\n", __func__, status);

	if (Result[1] < SCU_RAM_QAM_LOCKED_LOCKED_DEMOD_LOCKED) {
		
	} else if (Result[1] < SCU_RAM_QAM_LOCKED_LOCKED_LOCKED) {
		
		*pLockStatus = DEMOD_LOCK;
	} else if (Result[1] < SCU_RAM_QAM_LOCKED_LOCKED_NEVER_LOCK) {
		
		*pLockStatus = MPEG_LOCK;
	} else {
		
		
		*pLockStatus = NEVER_LOCK;
	}
	return status;
}

#define QAM_MIRROR__M         0x03
#define QAM_MIRROR_NORMAL     0x00
#define QAM_MIRRORED          0x01
#define QAM_MIRROR_AUTO_ON    0x02
#define QAM_LOCKRANGE__M      0x10
#define QAM_LOCKRANGE_NORMAL  0x10

static int SetQAM(struct drxk_state *state, u16 IntermediateFreqkHz,
		  s32 tunerFreqOffset)
{
	int status;
	u16 setParamParameters[4] = { 0, 0, 0, 0 };
	u16 cmdResult;

	dprintk(1, "\n");
	status = write16(state, FEC_DI_COMM_EXEC__A, FEC_DI_COMM_EXEC_STOP);
	if (status < 0)
		goto error;
	status = write16(state, FEC_RS_COMM_EXEC__A, FEC_RS_COMM_EXEC_STOP);
	if (status < 0)
		goto error;
	status = QAMResetQAM(state);
	if (status < 0)
		goto error;

	status = QAMSetSymbolrate(state);
	if (status < 0)
		goto error;

	
	switch (state->props.modulation) {
	case QAM_256:
		state->m_Constellation = DRX_CONSTELLATION_QAM256;
		break;
	case QAM_AUTO:
	case QAM_64:
		state->m_Constellation = DRX_CONSTELLATION_QAM64;
		break;
	case QAM_16:
		state->m_Constellation = DRX_CONSTELLATION_QAM16;
		break;
	case QAM_32:
		state->m_Constellation = DRX_CONSTELLATION_QAM32;
		break;
	case QAM_128:
		state->m_Constellation = DRX_CONSTELLATION_QAM128;
		break;
	default:
		status = -EINVAL;
		break;
	}
	if (status < 0)
		goto error;
	setParamParameters[0] = state->m_Constellation;	
	setParamParameters[1] = DRXK_QAM_I12_J17;	
	if (state->m_OperationMode == OM_QAM_ITU_C)
		setParamParameters[2] = QAM_TOP_ANNEX_C;
	else
		setParamParameters[2] = QAM_TOP_ANNEX_A;
	setParamParameters[3] |= (QAM_MIRROR_AUTO_ON);
	
	
	

	status = scu_command(state, SCU_RAM_COMMAND_STANDARD_QAM | SCU_RAM_COMMAND_CMD_DEMOD_SET_PARAM, 4, setParamParameters, 1, &cmdResult);
	if (status < 0) {
		
		if (state->m_OperationMode == OM_QAM_ITU_C)
			setParamParameters[0] = QAM_TOP_ANNEX_C;
		else
			setParamParameters[0] = QAM_TOP_ANNEX_A;
		status = scu_command(state, SCU_RAM_COMMAND_STANDARD_QAM | SCU_RAM_COMMAND_CMD_DEMOD_SET_ENV, 1, setParamParameters, 1, &cmdResult);
		if (status < 0)
			goto error;

		setParamParameters[0] = state->m_Constellation; 
		setParamParameters[1] = DRXK_QAM_I12_J17;       
		status = scu_command(state, SCU_RAM_COMMAND_STANDARD_QAM | SCU_RAM_COMMAND_CMD_DEMOD_SET_PARAM, 2, setParamParameters, 1, &cmdResult);
	}
	if (status < 0)
		goto error;

#if 0
	status = SetFrequency(channel, tunerFreqOffset));
	if (status < 0)
		goto error;
#endif
	status = SetFrequencyShifter(state, IntermediateFreqkHz, tunerFreqOffset, true);
	if (status < 0)
		goto error;

	
	status = SetQAMMeasurement(state, state->m_Constellation, state->props.symbol_rate);
	if (status < 0)
		goto error;

	
	status = write16(state, IQM_CF_SCALE_SH__A, IQM_CF_SCALE_SH__PRE);
	if (status < 0)
		goto error;
	status = write16(state, QAM_SY_TIMEOUT__A, QAM_SY_TIMEOUT__PRE);
	if (status < 0)
		goto error;

	
	status = write16(state, QAM_LC_RATE_LIMIT__A, 3);
	if (status < 0)
		goto error;
	status = write16(state, QAM_LC_LPF_FACTORP__A, 4);
	if (status < 0)
		goto error;
	status = write16(state, QAM_LC_LPF_FACTORI__A, 4);
	if (status < 0)
		goto error;
	status = write16(state, QAM_LC_MODE__A, 7);
	if (status < 0)
		goto error;

	status = write16(state, QAM_LC_QUAL_TAB0__A, 1);
	if (status < 0)
		goto error;
	status = write16(state, QAM_LC_QUAL_TAB1__A, 1);
	if (status < 0)
		goto error;
	status = write16(state, QAM_LC_QUAL_TAB2__A, 1);
	if (status < 0)
		goto error;
	status = write16(state, QAM_LC_QUAL_TAB3__A, 1);
	if (status < 0)
		goto error;
	status = write16(state, QAM_LC_QUAL_TAB4__A, 2);
	if (status < 0)
		goto error;
	status = write16(state, QAM_LC_QUAL_TAB5__A, 2);
	if (status < 0)
		goto error;
	status = write16(state, QAM_LC_QUAL_TAB6__A, 2);
	if (status < 0)
		goto error;
	status = write16(state, QAM_LC_QUAL_TAB8__A, 2);
	if (status < 0)
		goto error;
	status = write16(state, QAM_LC_QUAL_TAB9__A, 2);
	if (status < 0)
		goto error;
	status = write16(state, QAM_LC_QUAL_TAB10__A, 2);
	if (status < 0)
		goto error;
	status = write16(state, QAM_LC_QUAL_TAB12__A, 2);
	if (status < 0)
		goto error;
	status = write16(state, QAM_LC_QUAL_TAB15__A, 3);
	if (status < 0)
		goto error;
	status = write16(state, QAM_LC_QUAL_TAB16__A, 3);
	if (status < 0)
		goto error;
	status = write16(state, QAM_LC_QUAL_TAB20__A, 4);
	if (status < 0)
		goto error;
	status = write16(state, QAM_LC_QUAL_TAB25__A, 4);
	if (status < 0)
		goto error;

	
	status = write16(state, QAM_SY_SP_INV__A, QAM_SY_SP_INV_SPECTRUM_INV_DIS);
	if (status < 0)
		goto error;

	
	status = write16(state, SCU_COMM_EXEC__A, SCU_COMM_EXEC_HOLD);
	if (status < 0)
		goto error;

	
	switch (state->props.modulation) {
	case QAM_16:
		status = SetQAM16(state);
		break;
	case QAM_32:
		status = SetQAM32(state);
		break;
	case QAM_AUTO:
	case QAM_64:
		status = SetQAM64(state);
		break;
	case QAM_128:
		status = SetQAM128(state);
		break;
	case QAM_256:
		status = SetQAM256(state);
		break;
	default:
		status = -EINVAL;
		break;
	}
	if (status < 0)
		goto error;

	
	status = write16(state, SCU_COMM_EXEC__A, SCU_COMM_EXEC_ACTIVE);
	if (status < 0)
		goto error;

	
	
	
	status = MPEGTSDtoSetup(state, state->m_OperationMode);
	if (status < 0)
		goto error;

	
	status = MPEGTSStart(state);
	if (status < 0)
		goto error;
	status = write16(state, FEC_COMM_EXEC__A, FEC_COMM_EXEC_ACTIVE);
	if (status < 0)
		goto error;
	status = write16(state, QAM_COMM_EXEC__A, QAM_COMM_EXEC_ACTIVE);
	if (status < 0)
		goto error;
	status = write16(state, IQM_COMM_EXEC__A, IQM_COMM_EXEC_B_ACTIVE);
	if (status < 0)
		goto error;

	
	status = scu_command(state, SCU_RAM_COMMAND_STANDARD_QAM | SCU_RAM_COMMAND_CMD_DEMOD_START, 0, NULL, 1, &cmdResult);
	if (status < 0)
		goto error;

	

error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int SetQAMStandard(struct drxk_state *state,
			  enum OperationMode oMode)
{
	int status;
#ifdef DRXK_QAM_TAPS
#define DRXK_QAMA_TAPS_SELECT
#include "drxk_filters.h"
#undef DRXK_QAMA_TAPS_SELECT
#endif

	dprintk(1, "\n");

	
	SwitchAntennaToQAM(state);

	
	status = PowerUpQAM(state);
	if (status < 0)
		goto error;
	
	status = QAMResetQAM(state);
	if (status < 0)
		goto error;

	

	status = write16(state, IQM_COMM_EXEC__A, IQM_COMM_EXEC_B_STOP);
	if (status < 0)
		goto error;
	status = write16(state, IQM_AF_AMUX__A, IQM_AF_AMUX_SIGNAL2ADC);
	if (status < 0)
		goto error;

	switch (oMode) {
	case OM_QAM_ITU_A:
		status = BLChainCmd(state, DRXK_BL_ROM_OFFSET_TAPS_ITU_A, DRXK_BLCC_NR_ELEMENTS_TAPS, DRXK_BLC_TIMEOUT);
		break;
	case OM_QAM_ITU_C:
		status = BLDirectCmd(state, IQM_CF_TAP_RE0__A, DRXK_BL_ROM_OFFSET_TAPS_ITU_C, DRXK_BLDC_NR_ELEMENTS_TAPS, DRXK_BLC_TIMEOUT);
		if (status < 0)
			goto error;
		status = BLDirectCmd(state, IQM_CF_TAP_IM0__A, DRXK_BL_ROM_OFFSET_TAPS_ITU_C, DRXK_BLDC_NR_ELEMENTS_TAPS, DRXK_BLC_TIMEOUT);
		break;
	default:
		status = -EINVAL;
	}
	if (status < 0)
		goto error;

	status = write16(state, IQM_CF_OUT_ENA__A, (1 << IQM_CF_OUT_ENA_QAM__B));
	if (status < 0)
		goto error;
	status = write16(state, IQM_CF_SYMMETRIC__A, 0);
	if (status < 0)
		goto error;
	status = write16(state, IQM_CF_MIDTAP__A, ((1 << IQM_CF_MIDTAP_RE__B) | (1 << IQM_CF_MIDTAP_IM__B)));
	if (status < 0)
		goto error;

	status = write16(state, IQM_RC_STRETCH__A, 21);
	if (status < 0)
		goto error;
	status = write16(state, IQM_AF_CLP_LEN__A, 0);
	if (status < 0)
		goto error;
	status = write16(state, IQM_AF_CLP_TH__A, 448);
	if (status < 0)
		goto error;
	status = write16(state, IQM_AF_SNS_LEN__A, 0);
	if (status < 0)
		goto error;
	status = write16(state, IQM_CF_POW_MEAS_LEN__A, 0);
	if (status < 0)
		goto error;

	status = write16(state, IQM_FS_ADJ_SEL__A, 1);
	if (status < 0)
		goto error;
	status = write16(state, IQM_RC_ADJ_SEL__A, 1);
	if (status < 0)
		goto error;
	status = write16(state, IQM_CF_ADJ_SEL__A, 1);
	if (status < 0)
		goto error;
	status = write16(state, IQM_AF_UPD_SEL__A, 0);
	if (status < 0)
		goto error;

	
	status = write16(state, IQM_CF_CLP_VAL__A, 500);
	if (status < 0)
		goto error;
	status = write16(state, IQM_CF_DATATH__A, 1000);
	if (status < 0)
		goto error;
	status = write16(state, IQM_CF_BYPASSDET__A, 1);
	if (status < 0)
		goto error;
	status = write16(state, IQM_CF_DET_LCT__A, 0);
	if (status < 0)
		goto error;
	status = write16(state, IQM_CF_WND_LEN__A, 1);
	if (status < 0)
		goto error;
	status = write16(state, IQM_CF_PKDTH__A, 1);
	if (status < 0)
		goto error;
	status = write16(state, IQM_AF_INC_BYPASS__A, 1);
	if (status < 0)
		goto error;

	
	status = SetIqmAf(state, true);
	if (status < 0)
		goto error;
	status = write16(state, IQM_AF_START_LOCK__A, 0x01);
	if (status < 0)
		goto error;

	
	status = ADCSynchronization(state);
	if (status < 0)
		goto error;

	
	status = write16(state, SCU_RAM_QAM_FSM_STEP_PERIOD__A, 2000);
	if (status < 0)
		goto error;

	
	status = write16(state, SCU_COMM_EXEC__A, SCU_COMM_EXEC_HOLD);
	if (status < 0)
		goto error;


	status = InitAGC(state, true);
	if (status < 0)
		goto error;
	status = SetPreSaw(state, &(state->m_qamPreSawCfg));
	if (status < 0)
		goto error;

	
	status = SetAgcRf(state, &(state->m_qamRfAgcCfg), true);
	if (status < 0)
		goto error;
	status = SetAgcIf(state, &(state->m_qamIfAgcCfg), true);
	if (status < 0)
		goto error;

	
	status = write16(state, SCU_COMM_EXEC__A, SCU_COMM_EXEC_ACTIVE);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int WriteGPIO(struct drxk_state *state)
{
	int status;
	u16 value = 0;

	dprintk(1, "\n");
	
	status = write16(state, SCU_RAM_GPIO__A, SCU_RAM_GPIO_HW_LOCK_IND_DISABLE);
	if (status < 0)
		goto error;

	
	status = write16(state, SIO_TOP_COMM_KEY__A, SIO_TOP_COMM_KEY_KEY);
	if (status < 0)
		goto error;

	if (state->m_hasSAWSW) {
		if (state->UIO_mask & 0x0001) { 
			
			status = write16(state, SIO_PDR_SMA_TX_CFG__A, state->m_GPIOCfg);
			if (status < 0)
				goto error;

			
			status = read16(state, SIO_PDR_UIO_OUT_LO__A, &value);
			if (status < 0)
				goto error;
			if ((state->m_GPIO & 0x0001) == 0)
				value &= 0x7FFF;	
			else
				value |= 0x8000;	
			
			status = write16(state, SIO_PDR_UIO_OUT_LO__A, value);
			if (status < 0)
				goto error;
		}
		if (state->UIO_mask & 0x0002) { 
			
			status = write16(state, SIO_PDR_SMA_TX_CFG__A, state->m_GPIOCfg);
			if (status < 0)
				goto error;

			
			status = read16(state, SIO_PDR_UIO_OUT_LO__A, &value);
			if (status < 0)
				goto error;
			if ((state->m_GPIO & 0x0002) == 0)
				value &= 0xBFFF;	
			else
				value |= 0x4000;	
			
			status = write16(state, SIO_PDR_UIO_OUT_LO__A, value);
			if (status < 0)
				goto error;
		}
		if (state->UIO_mask & 0x0004) { 
			
			status = write16(state, SIO_PDR_SMA_TX_CFG__A, state->m_GPIOCfg);
			if (status < 0)
				goto error;

			
			status = read16(state, SIO_PDR_UIO_OUT_LO__A, &value);
			if (status < 0)
				goto error;
			if ((state->m_GPIO & 0x0004) == 0)
				value &= 0xFFFB;            
			else
				value |= 0x0004;            
			
			status = write16(state, SIO_PDR_UIO_OUT_LO__A, value);
			if (status < 0)
				goto error;
		}
	}
	
	status = write16(state, SIO_TOP_COMM_KEY__A, 0x0000);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int SwitchAntennaToQAM(struct drxk_state *state)
{
	int status = 0;
	bool gpio_state;

	dprintk(1, "\n");

	if (!state->antenna_gpio)
		return 0;

	gpio_state = state->m_GPIO & state->antenna_gpio;

	if (state->antenna_dvbt ^ gpio_state) {
		
		if (state->antenna_dvbt)
			state->m_GPIO &= ~state->antenna_gpio;
		else
			state->m_GPIO |= state->antenna_gpio;
		status = WriteGPIO(state);
	}
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}

static int SwitchAntennaToDVBT(struct drxk_state *state)
{
	int status = 0;
	bool gpio_state;

	dprintk(1, "\n");

	if (!state->antenna_gpio)
		return 0;

	gpio_state = state->m_GPIO & state->antenna_gpio;

	if (!(state->antenna_dvbt ^ gpio_state)) {
		
		if (state->antenna_dvbt)
			state->m_GPIO |= state->antenna_gpio;
		else
			state->m_GPIO &= ~state->antenna_gpio;
		status = WriteGPIO(state);
	}
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);
	return status;
}


static int PowerDownDevice(struct drxk_state *state)
{
	
	
	
	
	
	
	int status;

	dprintk(1, "\n");
	if (state->m_bPDownOpenBridge) {
		
		status = ConfigureI2CBridge(state, true);
		if (status < 0)
			goto error;
	}
	
	status = DVBTEnableOFDMTokenRing(state, false);
	if (status < 0)
		goto error;

	status = write16(state, SIO_CC_PWD_MODE__A, SIO_CC_PWD_MODE_LEVEL_CLOCK);
	if (status < 0)
		goto error;
	status = write16(state, SIO_CC_UPDATE__A, SIO_CC_UPDATE_KEY);
	if (status < 0)
		goto error;
	state->m_HICfgCtrl |= SIO_HI_RA_RAM_PAR_5_CFG_SLEEP_ZZZ;
	status = HI_CfgCommand(state);
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);

	return status;
}

static int load_microcode(struct drxk_state *state, const char *mc_name)
{
	const struct firmware *fw = NULL;
	int err = 0;

	dprintk(1, "\n");

	err = request_firmware(&fw, mc_name, state->i2c->dev.parent);
	if (err < 0) {
		printk(KERN_ERR
		       "drxk: Could not load firmware file %s.\n", mc_name);
		printk(KERN_INFO
		       "drxk: Copy %s to your hotplug directory!\n", mc_name);
		return err;
	}
	err = DownloadMicrocode(state, fw->data, fw->size);
	release_firmware(fw);
	return err;
}

static int init_drxk(struct drxk_state *state)
{
	int status = 0;
	enum DRXPowerMode powerMode = DRXK_POWER_DOWN_OFDM;
	u16 driverVersion;

	dprintk(1, "\n");
	if ((state->m_DrxkState == DRXK_UNINITIALIZED)) {
		status = PowerUpDevice(state);
		if (status < 0)
			goto error;
		status = DRXX_Open(state);
		if (status < 0)
			goto error;
		
		status = write16(state, SIO_CC_SOFT_RST__A, SIO_CC_SOFT_RST_OFDM__M | SIO_CC_SOFT_RST_SYS__M | SIO_CC_SOFT_RST_OSC__M);
		if (status < 0)
			goto error;
		status = write16(state, SIO_CC_UPDATE__A, SIO_CC_UPDATE_KEY);
		if (status < 0)
			goto error;
		
		msleep(1);
		state->m_DRXK_A3_PATCH_CODE = true;
		status = GetDeviceCapabilities(state);
		if (status < 0)
			goto error;

		
		
		
		state->m_HICfgBridgeDelay =
			(u16) ((state->m_oscClockFreq / 1000) *
				HI_I2C_BRIDGE_DELAY) / 1000;
		
		if (state->m_HICfgBridgeDelay >
			SIO_HI_RA_RAM_PAR_3_CFG_DBL_SDA__M) {
			state->m_HICfgBridgeDelay =
				SIO_HI_RA_RAM_PAR_3_CFG_DBL_SDA__M;
		}
		
		state->m_HICfgBridgeDelay +=
			state->m_HICfgBridgeDelay <<
			SIO_HI_RA_RAM_PAR_3_CFG_DBL_SCL__B;

		status = InitHI(state);
		if (status < 0)
			goto error;
		
#if NOA1ROM
		if (!(state->m_DRXK_A1_ROM_CODE)
			&& !(state->m_DRXK_A2_ROM_CODE))
#endif
		{
			status = write16(state, SCU_RAM_GPIO__A, SCU_RAM_GPIO_HW_LOCK_IND_DISABLE);
			if (status < 0)
				goto error;
		}

		
		status = MPEGTSDisable(state);
		if (status < 0)
			goto error;

		
		status = write16(state, AUD_COMM_EXEC__A, AUD_COMM_EXEC_STOP);
		if (status < 0)
			goto error;
		status = write16(state, SCU_COMM_EXEC__A, SCU_COMM_EXEC_STOP);
		if (status < 0)
			goto error;

		
		status = write16(state, SIO_OFDM_SH_OFDM_RING_ENABLE__A, SIO_OFDM_SH_OFDM_RING_ENABLE_ON);
		if (status < 0)
			goto error;

		
		status = write16(state, SIO_BL_COMM_EXEC__A, SIO_BL_COMM_EXEC_ACTIVE);
		if (status < 0)
			goto error;
		status = BLChainCmd(state, 0, 6, 100);
		if (status < 0)
			goto error;

		if (state->microcode_name)
			load_microcode(state, state->microcode_name);

		
		status = write16(state, SIO_OFDM_SH_OFDM_RING_ENABLE__A, SIO_OFDM_SH_OFDM_RING_ENABLE_OFF);
		if (status < 0)
			goto error;

		
		status = write16(state, SCU_COMM_EXEC__A, SCU_COMM_EXEC_ACTIVE);
		if (status < 0)
			goto error;
		status = DRXX_Open(state);
		if (status < 0)
			goto error;
		
		msleep(30);

		powerMode = DRXK_POWER_DOWN_OFDM;
		status = CtrlPowerMode(state, &powerMode);
		if (status < 0)
			goto error;

		driverVersion =
			(((DRXK_VERSION_MAJOR / 100) % 10) << 12) +
			(((DRXK_VERSION_MAJOR / 10) % 10) << 8) +
			((DRXK_VERSION_MAJOR % 10) << 4) +
			(DRXK_VERSION_MINOR % 10);
		status = write16(state, SCU_RAM_DRIVER_VER_HI__A, driverVersion);
		if (status < 0)
			goto error;
		driverVersion =
			(((DRXK_VERSION_PATCH / 1000) % 10) << 12) +
			(((DRXK_VERSION_PATCH / 100) % 10) << 8) +
			(((DRXK_VERSION_PATCH / 10) % 10) << 4) +
			(DRXK_VERSION_PATCH % 10);
		status = write16(state, SCU_RAM_DRIVER_VER_LO__A, driverVersion);
		if (status < 0)
			goto error;

		printk(KERN_INFO "DRXK driver version %d.%d.%d\n",
			DRXK_VERSION_MAJOR, DRXK_VERSION_MINOR,
			DRXK_VERSION_PATCH);


		

		
		status = write16(state, SCU_RAM_DRIVER_DEBUG__A, 0);
		if (status < 0)
			goto error;
		
		status = write16(state, FEC_COMM_EXEC__A, FEC_COMM_EXEC_STOP);
		if (status < 0)
			goto error;
		
		status = MPEGTSDtoInit(state);
		if (status < 0)
			goto error;
		status = MPEGTSStop(state);
		if (status < 0)
			goto error;
		status = MPEGTSConfigurePolarity(state);
		if (status < 0)
			goto error;
		status = MPEGTSConfigurePins(state, state->m_enableMPEGOutput);
		if (status < 0)
			goto error;
		
		status = WriteGPIO(state);
		if (status < 0)
			goto error;

		state->m_DrxkState = DRXK_STOPPED;

		if (state->m_bPowerDown) {
			status = PowerDownDevice(state);
			if (status < 0)
				goto error;
			state->m_DrxkState = DRXK_POWERED_DOWN;
		} else
			state->m_DrxkState = DRXK_STOPPED;
	}
error:
	if (status < 0)
		printk(KERN_ERR "drxk: Error %d on %s\n", status, __func__);

	return status;
}

static void drxk_release(struct dvb_frontend *fe)
{
	struct drxk_state *state = fe->demodulator_priv;

	dprintk(1, "\n");
	kfree(state);
}

static int drxk_sleep(struct dvb_frontend *fe)
{
	struct drxk_state *state = fe->demodulator_priv;

	dprintk(1, "\n");
	ShutDown(state);
	return 0;
}

static int drxk_gate_ctrl(struct dvb_frontend *fe, int enable)
{
	struct drxk_state *state = fe->demodulator_priv;

	dprintk(1, "%s\n", enable ? "enable" : "disable");
	return ConfigureI2CBridge(state, enable ? true : false);
}

static int drxk_set_parameters(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	u32 delsys  = p->delivery_system, old_delsys;
	struct drxk_state *state = fe->demodulator_priv;
	u32 IF;

	dprintk(1, "\n");

	if (!fe->ops.tuner_ops.get_if_frequency) {
		printk(KERN_ERR
		       "drxk: Error: get_if_frequency() not defined at tuner. Can't work without it!\n");
		return -EINVAL;
	}

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);
	if (fe->ops.tuner_ops.set_params)
		fe->ops.tuner_ops.set_params(fe);
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);

	old_delsys = state->props.delivery_system;
	state->props = *p;

	if (old_delsys != delsys) {
		ShutDown(state);
		switch (delsys) {
		case SYS_DVBC_ANNEX_A:
		case SYS_DVBC_ANNEX_C:
			if (!state->m_hasDVBC)
				return -EINVAL;
			state->m_itut_annex_c = (delsys == SYS_DVBC_ANNEX_C) ? true : false;
			if (state->m_itut_annex_c)
				SetOperationMode(state, OM_QAM_ITU_C);
			else
				SetOperationMode(state, OM_QAM_ITU_A);
			break;
		case SYS_DVBT:
			if (!state->m_hasDVBT)
				return -EINVAL;
			SetOperationMode(state, OM_DVBT);
			break;
		default:
			return -EINVAL;
		}
	}

	fe->ops.tuner_ops.get_if_frequency(fe, &IF);
	Start(state, 0, IF);

	

	return 0;
}

static int drxk_read_status(struct dvb_frontend *fe, fe_status_t *status)
{
	struct drxk_state *state = fe->demodulator_priv;
	u32 stat;

	dprintk(1, "\n");
	*status = 0;
	GetLockStatus(state, &stat, 0);
	if (stat == MPEG_LOCK)
		*status |= 0x1f;
	if (stat == FEC_LOCK)
		*status |= 0x0f;
	if (stat == DEMOD_LOCK)
		*status |= 0x07;
	return 0;
}

static int drxk_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	dprintk(1, "\n");

	*ber = 0;
	return 0;
}

static int drxk_read_signal_strength(struct dvb_frontend *fe,
				     u16 *strength)
{
	struct drxk_state *state = fe->demodulator_priv;
	u32 val = 0;

	dprintk(1, "\n");
	ReadIFAgc(state, &val);
	*strength = val & 0xffff;
	return 0;
}

static int drxk_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct drxk_state *state = fe->demodulator_priv;
	s32 snr2;

	dprintk(1, "\n");
	GetSignalToNoise(state, &snr2);
	*snr = snr2 & 0xffff;
	return 0;
}

static int drxk_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	struct drxk_state *state = fe->demodulator_priv;
	u16 err;

	dprintk(1, "\n");
	DVBTQAMGetAccPktErr(state, &err);
	*ucblocks = (u32) err;
	return 0;
}

static int drxk_get_tune_settings(struct dvb_frontend *fe, struct dvb_frontend_tune_settings
				    *sets)
{
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;

	dprintk(1, "\n");
	switch (p->delivery_system) {
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
	case SYS_DVBT:
		sets->min_delay_ms = 3000;
		sets->max_drift = 0;
		sets->step_size = 0;
		return 0;
	default:
		return -EINVAL;
	}
}

static struct dvb_frontend_ops drxk_ops = {
	
	.info = {
		.name = "DRXK",
		.frequency_min = 47000000,
		.frequency_max = 865000000,
		 
		.symbol_rate_min = 870000,
		.symbol_rate_max = 11700000,
		
		.frequency_stepsize = 166667,

		.caps = FE_CAN_QAM_16 | FE_CAN_QAM_32 | FE_CAN_QAM_64 |
			FE_CAN_QAM_128 | FE_CAN_QAM_256 | FE_CAN_FEC_AUTO |
			FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_MUTE_TS |
			FE_CAN_TRANSMISSION_MODE_AUTO | FE_CAN_RECOVER |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO
	},

	.release = drxk_release,
	.sleep = drxk_sleep,
	.i2c_gate_ctrl = drxk_gate_ctrl,

	.set_frontend = drxk_set_parameters,
	.get_tune_settings = drxk_get_tune_settings,

	.read_status = drxk_read_status,
	.read_ber = drxk_read_ber,
	.read_signal_strength = drxk_read_signal_strength,
	.read_snr = drxk_read_snr,
	.read_ucblocks = drxk_read_ucblocks,
};

struct dvb_frontend *drxk_attach(const struct drxk_config *config,
				 struct i2c_adapter *i2c)
{
	int n;

	struct drxk_state *state = NULL;
	u8 adr = config->adr;

	dprintk(1, "\n");
	state = kzalloc(sizeof(struct drxk_state), GFP_KERNEL);
	if (!state)
		return NULL;

	state->i2c = i2c;
	state->demod_address = adr;
	state->single_master = config->single_master;
	state->microcode_name = config->microcode_name;
	state->no_i2c_bridge = config->no_i2c_bridge;
	state->antenna_gpio = config->antenna_gpio;
	state->antenna_dvbt = config->antenna_dvbt;
	state->m_ChunkSize = config->chunk_size;
	state->enable_merr_cfg = config->enable_merr_cfg;

	if (config->dynamic_clk) {
		state->m_DVBTStaticCLK = 0;
		state->m_DVBCStaticCLK = 0;
	} else {
		state->m_DVBTStaticCLK = 1;
		state->m_DVBCStaticCLK = 1;
	}


	if (config->mpeg_out_clk_strength)
		state->m_TSClockkStrength = config->mpeg_out_clk_strength & 0x07;
	else
		state->m_TSClockkStrength = 0x06;

	if (config->parallel_ts)
		state->m_enableParallel = true;
	else
		state->m_enableParallel = false;

	
	state->UIO_mask = config->antenna_gpio;

	
	if (!state->antenna_dvbt && state->antenna_gpio)
		state->m_GPIO |= state->antenna_gpio;
	else
		state->m_GPIO &= ~state->antenna_gpio;

	mutex_init(&state->mutex);

	memcpy(&state->frontend.ops, &drxk_ops, sizeof(drxk_ops));
	state->frontend.demodulator_priv = state;

	init_state(state);
	if (init_drxk(state) < 0)
		goto error;

	
	n = 0;
	if (state->m_hasDVBC) {
		state->frontend.ops.delsys[n++] = SYS_DVBC_ANNEX_A;
		state->frontend.ops.delsys[n++] = SYS_DVBC_ANNEX_C;
		strlcat(state->frontend.ops.info.name, " DVB-C",
			sizeof(state->frontend.ops.info.name));
	}
	if (state->m_hasDVBT) {
		state->frontend.ops.delsys[n++] = SYS_DVBT;
		strlcat(state->frontend.ops.info.name, " DVB-T",
			sizeof(state->frontend.ops.info.name));
	}

	printk(KERN_INFO "drxk: frontend initialized.\n");
	return &state->frontend;

error:
	printk(KERN_ERR "drxk: not found\n");
	kfree(state);
	return NULL;
}
EXPORT_SYMBOL(drxk_attach);

MODULE_DESCRIPTION("DRX-K driver");
MODULE_AUTHOR("Ralph Metzler");
MODULE_LICENSE("GPL");
