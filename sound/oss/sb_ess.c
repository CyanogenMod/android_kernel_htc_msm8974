#undef FKS_LOGGING
#undef FKS_TEST




#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>

#include "sound_config.h"
#include "sb_mixer.h"
#include "sb.h"

#include "sb_ess.h"

#define ESSTYPE_LIKE20	-1		
#define ESSTYPE_DETECT	0		

#define SUBMDL_ES1788	0x10	
#define SUBMDL_ES1868	0x11	
#define SUBMDL_ES1869	0x12	
#define SUBMDL_ES1878	0x13	
#define SUBMDL_ES1879	0x16    
#define SUBMDL_ES1887	0x14	
#define SUBMDL_ES1888	0x15	

#define SB_CAP_ES18XX_RATE 0x100

#define ES1688_CLOCK1 795444 
#define ES1688_CLOCK2 397722 
#define ES18XX_CLOCK1 793800 
#define ES18XX_CLOCK2 768000 

#ifdef FKS_LOGGING
static void ess_show_mixerregs (sb_devc *devc);
#endif
static int ess_read (sb_devc * devc, unsigned char reg);
static int ess_write (sb_devc * devc, unsigned char reg, unsigned char data);
static void ess_chgmixer
	(sb_devc * devc, unsigned int reg, unsigned int mask, unsigned int val);


struct ess_command {short cmd; short data;};

static struct ess_command ess_i08m[] =		
	{ {0xb7, 0x51}, {0xb7, 0xd0}, {-1, 0} };
static struct ess_command ess_i16m[] =		
	{ {0xb7, 0x71}, {0xb7, 0xf4}, {-1, 0} };
static struct ess_command ess_i08s[] =		
	{ {0xb7, 0x51}, {0xb7, 0x98}, {-1, 0} };
static struct ess_command ess_i16s[] =		
	{ {0xb7, 0x71}, {0xb7, 0xbc}, {-1, 0} };

static struct ess_command *ess_inp_cmds[] =
	{ ess_i08m, ess_i16m, ess_i08s, ess_i16s };


static struct ess_command ess_o08m[] =		
	{ {0xb6, 0x80}, {0xb7, 0x51}, {0xb7, 0xd0}, {-1, 0} };
static struct ess_command ess_o16m[] =		
	{ {0xb6, 0x00}, {0xb7, 0x71}, {0xb7, 0xf4}, {-1, 0} };
static struct ess_command ess_o08s[] =		
	{ {0xb6, 0x80}, {0xb7, 0x51}, {0xb7, 0x98}, {-1, 0} };
static struct ess_command ess_o16s[] =		
	{ {0xb6, 0x00}, {0xb7, 0x71}, {0xb7, 0xbc}, {-1, 0} };

static struct ess_command *ess_out_cmds[] =
	{ ess_o08m, ess_o16m, ess_o08s, ess_o16s };

static void ess_exec_commands
	(sb_devc *devc, struct ess_command *cmdtab[])
{
	struct ess_command *cmd;

	cmd = cmdtab [ ((devc->channels != 1) << 1) + (devc->bits != AFMT_U8) ];

	while (cmd->cmd != -1) {
		ess_write (devc, cmd->cmd, cmd->data);
		cmd++;
	}
}

static void ess_change
	(sb_devc *devc, unsigned int reg, unsigned int mask, unsigned int val)
{
	int value;

	value = ess_read (devc, reg);
	value = (value & ~mask) | (val & mask);
	ess_write (devc, reg, value);
}

static void ess_set_output_parms
	(int dev, unsigned long buf, int nr_bytes, int intrflag)
{
	sb_devc *devc = audio_devs[dev]->devc;

	if (devc->duplex) {
		devc->trg_buf_16 = buf;
		devc->trg_bytes_16 = nr_bytes;
		devc->trg_intrflag_16 = intrflag;
		devc->irq_mode_16 = IMODE_OUTPUT;
	} else {
		devc->trg_buf = buf;
		devc->trg_bytes = nr_bytes;
		devc->trg_intrflag = intrflag;
		devc->irq_mode = IMODE_OUTPUT;
	}
}

static void ess_set_input_parms
	(int dev, unsigned long buf, int count, int intrflag)
{
	sb_devc *devc = audio_devs[dev]->devc;

	devc->trg_buf = buf;
	devc->trg_bytes = count;
	devc->trg_intrflag = intrflag;
	devc->irq_mode = IMODE_INPUT;
}

static int ess_calc_div (int clock, int revert, int *speedp, int *diffp)
{
	int divider;
	int speed, diff;
	int retval;

	speed   = *speedp;
	divider = (clock + speed / 2) / speed;
	retval  = revert - divider;
	if (retval > revert - 1) {
		retval  = revert - 1;
		divider = revert - retval;
	}

	*speedp	= clock / divider;
	diff	= speed - *speedp;
	if (diff < 0) diff =-diff;
	*diffp  = diff;

	return retval;
}

static int ess_calc_best_speed
	(int clock1, int rev1, int clock2, int rev2, int *divp, int *speedp)
{
	int speed1 = *speedp, speed2 = *speedp;
	int div1, div2;
	int diff1, diff2;
	int retval;

	div1 = ess_calc_div (clock1, rev1, &speed1, &diff1);
	div2 = ess_calc_div (clock2, rev2, &speed2, &diff2);

	if (diff1 < diff2) {
		*divp   = div1;
		*speedp = speed1;
		retval  = 1;
	} else {
	
		*divp   = 0x80 | div2;
		*speedp = speed2;
		retval  = 2;
	}

	return retval;
}

static void ess_common_speed (sb_devc *devc, int *speedp, int *divp)
{
	int diff = 0, div;

	if (devc->duplex) {
		if (devc->submodel == SUBMDL_ES1888) {
			div = 0x80 | ess_calc_div (795500, 256, speedp, &diff);
		} else {
			div = 0x80 | ess_calc_div (795500, 128, speedp, &diff);
		}
	} else if(devc->caps & SB_CAP_ES18XX_RATE) {
		if (devc->submodel == SUBMDL_ES1888) {
			ess_calc_best_speed(397700, 128, 795500, 256, 
						&div, speedp);
		} else {
			ess_calc_best_speed(ES18XX_CLOCK1, 128, ES18XX_CLOCK2, 256, 
						&div, speedp);
		}
	} else {
		if (*speedp > 22000) {
			div = 0x80 | ess_calc_div (ES1688_CLOCK1, 256, speedp, &diff);
		} else {
			div = 0x00 | ess_calc_div (ES1688_CLOCK2, 128, speedp, &diff);
		}
	}
	*divp = div;
}

static void ess_speed (sb_devc *devc, int audionum)
{
	int speed;
	int div, div2;

	ess_common_speed (devc, &(devc->speed), &div);

#ifdef FKS_REG_LOGGING
printk (KERN_INFO "FKS: ess_speed (%d) b speed = %d, div=%x\n", audionum, devc->speed, div);
#endif

	
	speed = (devc->speed * 9) / 20;

	div2 = 256 - 7160000 / (speed * 82);

	if (!devc->duplex) audionum = 1;

	if (audionum == 1) {
		ess_write (devc, 0xa1, div);
		ess_write (devc, 0xa2, div2);
	} else {
		ess_setmixer (devc, 0x70, div);
		ess_write (devc, 0xa2, div2);
		ess_setmixer (devc, 0x72, div2);
	}
}

static int ess_audio_prepare_for_input(int dev, int bsize, int bcount)
{
	sb_devc *devc = audio_devs[dev]->devc;

	ess_speed(devc, 1);

	sb_dsp_command(devc, DSP_CMD_SPKOFF);

	ess_write (devc, 0xb8, 0x0e);	
	ess_change (devc, 0xa8, 0x03, 3 - devc->channels);	
	ess_write (devc, 0xb9, 2);	

	ess_exec_commands (devc, ess_inp_cmds);

	ess_change (devc, 0xb1, 0xf0, 0x50);
	ess_change (devc, 0xb2, 0xf0, 0x50);

	devc->trigger_bits = 0;
	return 0;
}

static int ess_audio_prepare_for_output_audio1 (int dev, int bsize, int bcount)
{
	sb_devc *devc = audio_devs[dev]->devc;

	sb_dsp_reset(devc);
	ess_speed(devc, 1);
	ess_write (devc, 0xb8, 4);	
	ess_change (devc, 0xa8, 0x03, 3 - devc->channels);	
	ess_write (devc, 0xb9, 2);	

	ess_exec_commands (devc, ess_out_cmds);

	ess_change (devc, 0xb1, 0xf0, 0x50);	
	ess_change (devc, 0xb2, 0xf0, 0x50);	

	sb_dsp_command(devc, DSP_CMD_SPKON);	

	devc->trigger_bits = 0;
	return 0;
}

static int ess_audio_prepare_for_output_audio2 (int dev, int bsize, int bcount)
{
	sb_devc *devc = audio_devs[dev]->devc;
	unsigned char bits;


	ess_chgmixer (devc, 0x78, 0xd0, 0xd0);

	ess_speed(devc, 2);

	
	bits = ess_getmixer (devc, 0x7a) & 0x18;

	
	if (devc->channels != 1) bits |= 0x02;

	
	if (devc->bits != AFMT_U8) bits |= 0x05;	

	
	bits |= 0x60;

	ess_setmixer (devc, 0x7a, bits);

	ess_mixer_reload (devc, SOUND_MIXER_PCM);	

	devc->trigger_bits = 0;
	return 0;
}

static int ess_audio_prepare_for_output(int dev, int bsize, int bcount)
{
	sb_devc *devc = audio_devs[dev]->devc;

#ifdef FKS_REG_LOGGING
printk(KERN_INFO "ess_audio_prepare_for_output: dma_out=%d,dma_in=%d\n"
, audio_devs[dev]->dmap_out->dma, audio_devs[dev]->dmap_in->dma);
#endif

	if (devc->duplex) {
		return ess_audio_prepare_for_output_audio2 (dev, bsize, bcount);
	} else {
		return ess_audio_prepare_for_output_audio1 (dev, bsize, bcount);
	}
}

static void ess_audio_halt_xfer(int dev)
{
	unsigned long flags;
	sb_devc *devc = audio_devs[dev]->devc;

	spin_lock_irqsave(&devc->lock, flags);
	sb_dsp_reset(devc);
	spin_unlock_irqrestore(&devc->lock, flags);

	if (devc->duplex) ess_chgmixer(devc, 0x78, 0x03, 0x00);
}

static void ess_audio_start_input
	(int dev, unsigned long buf, int nr_bytes, int intrflag)
{
	int count = nr_bytes;
	sb_devc *devc = audio_devs[dev]->devc;
	short c = -nr_bytes;


	if (audio_devs[dev]->dmap_in->dma > 3) count >>= 1;
	count--;

	devc->irq_mode = IMODE_INPUT;

	ess_write (devc, 0xa4, (unsigned char) ((unsigned short) c & 0xff));
	ess_write (devc, 0xa5, (unsigned char) (((unsigned short) c >> 8) & 0xff));

	ess_change (devc, 0xb8, 0x0f, 0x0f);	
	devc->intr_active = 1;
}

static void ess_audio_output_block_audio1
	(int dev, unsigned long buf, int nr_bytes, int intrflag)
{
	int count = nr_bytes;
	sb_devc *devc = audio_devs[dev]->devc;
	short c = -nr_bytes;

	if (audio_devs[dev]->dmap_out->dma > 3)
		count >>= 1;
	count--;

	devc->irq_mode = IMODE_OUTPUT;

	ess_write (devc, 0xa4, (unsigned char) ((unsigned short) c & 0xff));
	ess_write (devc, 0xa5, (unsigned char) (((unsigned short) c >> 8) & 0xff));

	ess_change (devc, 0xb8, 0x05, 0x05);	
	devc->intr_active = 1;
}

static void ess_audio_output_block_audio2
	(int dev, unsigned long buf, int nr_bytes, int intrflag)
{
	int count = nr_bytes;
	sb_devc *devc = audio_devs[dev]->devc;
	short c = -nr_bytes;

	if (audio_devs[dev]->dmap_out->dma > 3) count >>= 1;
	count--;

	ess_setmixer (devc, 0x74, (unsigned char) ((unsigned short) c & 0xff));
	ess_setmixer (devc, 0x76, (unsigned char) (((unsigned short) c >> 8) & 0xff));
	ess_chgmixer (devc, 0x78, 0x03, 0x03);   

	devc->irq_mode_16 = IMODE_OUTPUT;
		devc->intr_active_16 = 1;
}

static void ess_audio_output_block
	(int dev, unsigned long buf, int nr_bytes, int intrflag)
{
	sb_devc *devc = audio_devs[dev]->devc;

	if (devc->duplex) {
		ess_audio_output_block_audio2 (dev, buf, nr_bytes, intrflag);
	} else {
		ess_audio_output_block_audio1 (dev, buf, nr_bytes, intrflag);
	}
}

static void ess_audio_trigger(int dev, int bits)
{
	sb_devc *devc = audio_devs[dev]->devc;

	int bits_16 = bits & devc->irq_mode_16;
	bits &= devc->irq_mode;

	if (!bits && !bits_16) {
		
		sb_dsp_command(devc, 0xd0);	
	}

	if (bits) {
		switch (devc->irq_mode)
		{
			case IMODE_INPUT:
				ess_audio_start_input(dev, devc->trg_buf, devc->trg_bytes,
					devc->trg_intrflag);
				break;

			case IMODE_OUTPUT:
				ess_audio_output_block(dev, devc->trg_buf, devc->trg_bytes,
					devc->trg_intrflag);
				break;
		}
	}

	if (bits_16) {
		switch (devc->irq_mode_16) {
		case IMODE_INPUT:
			ess_audio_start_input(dev, devc->trg_buf_16, devc->trg_bytes_16,
					devc->trg_intrflag_16);
			break;

		case IMODE_OUTPUT:
			ess_audio_output_block(dev, devc->trg_buf_16, devc->trg_bytes_16,
					devc->trg_intrflag_16);
			break;
		}
	}

	devc->trigger_bits = bits | bits_16;
}

static int ess_audio_set_speed(int dev, int speed)
{
	sb_devc *devc = audio_devs[dev]->devc;
	int minspeed, maxspeed, dummydiv;

	if (speed > 0) {
		minspeed = (devc->duplex ? 6215  : 5000 );
		maxspeed = (devc->duplex ? 44100 : 48000);
		if (speed < minspeed) speed = minspeed;
		if (speed > maxspeed) speed = maxspeed;

		ess_common_speed (devc, &speed, &dummydiv);

		devc->speed = speed;
	}
	return devc->speed;
}

static unsigned int ess_audio_set_bits(int dev, unsigned int bits)
{
	sb_devc *devc = audio_devs[dev]->devc;

	if (bits != 0) {
		if (bits == AFMT_U8 || bits == AFMT_S16_LE) {
			devc->bits = bits;
		} else {
			devc->bits = AFMT_U8;
		}
	}

	return devc->bits;
}

static short ess_audio_set_channels(int dev, short channels)
{
	sb_devc *devc = audio_devs[dev]->devc;

	if (channels == 1 || channels == 2) devc->channels = channels;

	return devc->channels;
}

static struct audio_driver ess_audio_driver =   
{
	.owner			= THIS_MODULE,
	.open			= sb_audio_open,
	.close			= sb_audio_close,
	.output_block		= ess_set_output_parms,
	.start_input		= ess_set_input_parms,
	.prepare_for_input	= ess_audio_prepare_for_input,
	.prepare_for_output	= ess_audio_prepare_for_output,
	.halt_io		= ess_audio_halt_xfer,
	.trigger		= ess_audio_trigger,
	.set_speed		= ess_audio_set_speed,
	.set_bits		= ess_audio_set_bits,
	.set_channels		= ess_audio_set_channels
};

struct audio_driver *ess_audio_init
		(sb_devc *devc, int *audio_flags, int *format_mask)
{
	*audio_flags = DMA_AUTOMODE;
	*format_mask |= AFMT_S16_LE;

	if (devc->duplex) {
		int tmp_dma;
		tmp_dma		= devc->dma16;
		devc->dma16	= devc->dma8;
		devc->dma8	= tmp_dma;

		*audio_flags |= DMA_DUPLEX;
	}

	return &ess_audio_driver;
}

static void ess_handle_channel
	(char *channel, int dev, int intr_active, unsigned char flag, int irq_mode)
{
	if (!intr_active || !flag) return;
#ifdef FKS_REG_LOGGING
printk(KERN_INFO "FKS: ess_handle_channel %s irq_mode=%d\n", channel, irq_mode);
#endif
	switch (irq_mode) {
		case IMODE_OUTPUT:
			DMAbuf_outputintr (dev, 1);
			break;

		case IMODE_INPUT:
			DMAbuf_inputintr (dev);
			break;

		case IMODE_INIT:
			break;

		default:;
			
	}
}

void ess_intr (sb_devc *devc)
{
	int				status;
	unsigned char	src;

	if (devc->submodel == SUBMDL_ES1887) {
		src = ess_getmixer (devc, 0x7f) >> 4;
	} else {
		src = 0xff;
	}

#ifdef FKS_REG_LOGGING
printk(KERN_INFO "FKS: sbintr src=%x\n",(int)src);
#endif
	ess_handle_channel
		( "Audio 1"
		, devc->dev, devc->intr_active   , src & 0x01, devc->irq_mode   );
	ess_handle_channel
		( "Audio 2"
		, devc->dev, devc->intr_active_16, src & 0x02, devc->irq_mode_16);
	if (devc->submodel == SUBMDL_ES1887 && (src & 0x02)) {
		ess_chgmixer (devc, 0x7a, 0x80, 0x00);
	}

	if (src & 0x01) {
		status = inb(DSP_DATA_AVAIL);
	}
}

static void ess_extended (sb_devc * devc)
{
	

	sb_dsp_command(devc, 0xc6);
}

static int ess_write (sb_devc * devc, unsigned char reg, unsigned char data)
{
#ifdef FKS_REG_LOGGING
printk(KERN_INFO "FKS: write reg %x: %x\n", reg, data);
#endif
	

	if (!sb_dsp_command(devc, reg))
		return 0;

	return sb_dsp_command(devc, data);
}

static int ess_read (sb_devc * devc, unsigned char reg)
{
	

	
	if (!sb_dsp_command(devc, 0xc0)) return -1;

	if (!sb_dsp_command(devc, reg )) return -1;

	return sb_dsp_get_byte(devc);
}

int ess_dsp_reset(sb_devc * devc)
{
	int loopc;

#ifdef FKS_REG_LOGGING
printk(KERN_INFO "FKS: ess_dsp_reset 1\n");
ess_show_mixerregs (devc);
#endif

	DEB(printk("Entered ess_dsp_reset()\n"));

	outb(3, DSP_RESET); 

	udelay(10);
	outb(0, DSP_RESET);
	udelay(30);

	for (loopc = 0; loopc < 1000 && !(inb(DSP_DATA_AVAIL) & 0x80); loopc++);

	if (inb(DSP_READ) != 0xAA) {
		DDB(printk("sb: No response to RESET\n"));
		return 0;   
	}
	ess_extended (devc);

	DEB(printk("sb_dsp_reset() OK\n"));

#ifdef FKS_LOGGING
printk(KERN_INFO "FKS: dsp_reset 2\n");
ess_show_mixerregs (devc);
#endif

	return 1;
}

static int ess_irq_bits (int irq)
{
	switch (irq) {
	case 2:
	case 9:
		return 0;

	case 5:
		return 1;

	case 7:
		return 2;

	case 10:
		return 3;

	default:
		printk(KERN_ERR "ESS1688: Invalid IRQ %d\n", irq);
		return -1;
	}
}

static int ess_common_set_irq_hw (sb_devc * devc)
{
	int irq_bits;

	if ((irq_bits = ess_irq_bits (devc->irq)) == -1) return 0;

	if (!ess_write (devc, 0xb1, 0x50 | (irq_bits << 2))) {
		printk(KERN_ERR "ES1688: Failed to write to IRQ config register\n");
		return 0;
	}
	return 1;
}

static void ess_es1887_set_irq_hw (sb_devc * devc)
{
	int irq_bits;

	if ((irq_bits = ess_irq_bits (devc->irq)) == -1) return;

	ess_chgmixer (devc, 0x7f, 0x0f, 0x01 | ((irq_bits + 1) << 1));
}

static int ess_set_irq_hw (sb_devc * devc)
{
	if (devc->submodel == SUBMDL_ES1887) ess_es1887_set_irq_hw (devc);

	return ess_common_set_irq_hw (devc);
}

#ifdef FKS_TEST


static void FKS_test (sb_devc * devc)
{
	int val1, val2;
	val1 = ess_getmixer (devc, 0x64);
	ess_setmixer (devc, 0x64, ~val1);
	val2 = ess_getmixer (devc, 0x64) ^ ~val1;
	ess_setmixer (devc, 0x64, val1);
	val1 ^= ess_getmixer (devc, 0x64);
printk (KERN_INFO "FKS: FKS_test %02x, %02x\n", (val1 & 0x0ff), (val2 & 0x0ff));
};
#endif

static unsigned int ess_identify (sb_devc * devc)
{
	unsigned int val;
	unsigned long flags;

	spin_lock_irqsave(&devc->lock, flags);
	outb(((unsigned char) (0x40 & 0xff)), MIXER_ADDR);

	udelay(20);
	val  = inb(MIXER_DATA) << 8;
	udelay(20);
	val |= inb(MIXER_DATA);
	udelay(20);
	spin_unlock_irqrestore(&devc->lock, flags);

	return val;
}

/*
 * ESS technology describes a detection scheme in their docs. It involves
 * fiddling with the bits in certain mixer registers. ess_probe is supposed
 * to help.
 *
 * FKS: tracing shows ess_probe writes wrong value to 0x64. Bit 3 reads 1, but
 * should be written 0 only. Check this.
 */
static int ess_probe (sb_devc * devc, int reg, int xorval)
{
	int  val1, val2, val3;

	val1 = ess_getmixer (devc, reg);
	val2 = val1 ^ xorval;
	ess_setmixer (devc, reg, val2);
	val3 = ess_getmixer (devc, reg);
	ess_setmixer (devc, reg, val1);

	return (val2 == val3);
}

int ess_init(sb_devc * devc, struct address_info *hw_config)
{
	unsigned char cfg;
	int ess_major = 0, ess_minor = 0;
	int i;
	static char name[100], modelname[10];


	sb_dsp_command(devc, 0xe7); 

	for (i = 1000; i; i--) {
		if (inb(DSP_DATA_AVAIL) & 0x80) {
			if (ess_major == 0) {
				ess_major = inb(DSP_READ);
			} else {
				ess_minor = inb(DSP_READ);
				break;
			}
		}
	}

	if (ess_major == 0) return 0;

	if (ess_major == 0x48 && (ess_minor & 0xf0) == 0x80) {
		sprintf(name, "ESS ES488 AudioDrive (rev %d)",
			ess_minor & 0x0f);
		hw_config->name = name;
		devc->model = MDL_SBPRO;
		return 1;
	}

	devc->model = MDL_ESS;
	devc->submodel = ess_minor & 0x0f;

	if (ess_major == 0x68 && (ess_minor & 0xf0) == 0x80) {
		char *chip = NULL;
		int submodel = -1;

		switch (devc->sbmo.esstype) {
		case ESSTYPE_DETECT:
		case ESSTYPE_LIKE20:
			break;
		case 688:
			submodel = 0x00;
			break;
		case 1688:
			submodel = 0x08;
			break;
		case 1868:
			submodel = SUBMDL_ES1868;
			break;
		case 1869:
			submodel = SUBMDL_ES1869;
			break;
		case 1788:
			submodel = SUBMDL_ES1788;
			break;
		case 1878:
			submodel = SUBMDL_ES1878;
			break;
		case 1879:
			submodel = SUBMDL_ES1879;
			break;
		case 1887:
			submodel = SUBMDL_ES1887;
			break;
		case 1888:
			submodel = SUBMDL_ES1888;
			break;
		default:
			printk (KERN_ERR "Invalid esstype=%d specified\n", devc->sbmo.esstype);
			return 0;
		};
		if (submodel != -1) {
			devc->submodel = submodel;
			sprintf (modelname, "ES%d", devc->sbmo.esstype);
			chip = modelname;
		};
		if (chip == NULL && (ess_minor & 0x0f) < 8) {
			chip = "ES688";
		};
#ifdef FKS_TEST
FKS_test (devc);
#endif
		if (chip == NULL && devc->sbmo.esstype == ESSTYPE_LIKE20) {
			chip = "ES1688";
		};

		if (chip == NULL) {
			int type;

			type = ess_identify (devc);

			switch (type) {
			case 0x1868:
				chip = "ES1868";
				devc->submodel = SUBMDL_ES1868;
				break;
			case 0x1869:
				chip = "ES1869";
				devc->submodel = SUBMDL_ES1869;
				break;
			case 0x1878:
				chip = "ES1878";
				devc->submodel = SUBMDL_ES1878;
				break;
			case 0x1879:
				chip = "ES1879";
				devc->submodel = SUBMDL_ES1879;
				break;
			default:
				if ((type & 0x00ff) != ((type >> 8) & 0x00ff)) {
					printk ("ess_init: Unrecognized %04x\n", type);
				}
			};
		};
#if 0
		if (chip == NULL && !ess_probe(devc, 0x64, (1 << 4))) {
#endif
		if (chip == NULL && ess_probe(devc, 0x64, (1 << 2))) {
			if (ess_probe (devc, 0x70, 0x7f)) {
				if (ess_probe (devc, 0x64, (1 << 5))) {
					chip = "ES1887";
					devc->submodel = SUBMDL_ES1887;
				} else {
					chip = "ES1888";
					devc->submodel = SUBMDL_ES1888;
				}
			} else {
				chip = "ES1788";
				devc->submodel = SUBMDL_ES1788;
			}
		};
		if (chip == NULL) {
			chip = "ES1688";
		};

	    printk ( KERN_INFO "ESS chip %s %s%s\n"
               , chip
               , ( devc->sbmo.esstype == ESSTYPE_DETECT || devc->sbmo.esstype == ESSTYPE_LIKE20
                 ? "detected"
                 : "specified"
                 )
               , ( devc->sbmo.esstype == ESSTYPE_LIKE20
                 ? " (kernel 2.0 compatible)"
                 : ""
                 )
               );

		sprintf(name,"ESS %s AudioDrive (rev %d)", chip, ess_minor & 0x0f);
	} else {
		strcpy(name, "Jazz16");
	}

	
	switch(devc->submodel) {
		
		case SUBMDL_ES1888:
			devc->caps |= SB_CAP_ES18XX_RATE;
			break;
	}

	hw_config->name = name;
	
	sb_dsp_reset(devc); 

	cfg = ess_getmixer (devc, 0x40);
	ess_setmixer (devc, 0x40, cfg | 0x03);
	if (devc->submodel >= 8) {		
		devc->caps |= SB_NO_MIDI;   
	}
	sb_dsp_reset (devc);

	return ess_set_irq_hw (devc);
}

static int ess_set_dma_hw(sb_devc * devc)
{
	unsigned char cfg, dma_bits = 0, dma16_bits;
	int dma;

#ifdef FKS_LOGGING
printk(KERN_INFO "ess_set_dma_hw: dma8=%d,dma16=%d,dup=%d\n"
, devc->dma8, devc->dma16, devc->duplex);
#endif

	dma = devc->dma8;

	if (dma > 3 || dma < 0 || dma == 2) {
		dma_bits = 0;
		printk(KERN_ERR "ESS1688: Invalid DMA8 %d\n", dma);
		return 0;
	} else {
		
		cfg = 0x50;

		if (dma == 3) {
			dma_bits = 3;
		} else {
			dma_bits = dma + 1;
		}
	}

	if (!ess_write (devc, 0xb2, cfg | (dma_bits << 2))) {
		printk(KERN_ERR "ESS1688: Failed to write to DMA config register\n");
		return 0;
	}

	if (devc->duplex) {
		dma = devc->dma16;
		dma16_bits = 0;

		if (dma >= 0) {
			switch (dma) {
			case 0:
				dma_bits = 0x04;
				break;
			case 1:
				dma_bits = 0x05;
				break;
			case 3:
				dma_bits = 0x06;
				break;
			case 5:
				dma_bits   = 0x07;
				dma16_bits = 0x20;
				break;
			default:
				printk(KERN_ERR "ESS1887: Invalid DMA16 %d\n", dma);
				return 0;
			};
			ess_chgmixer (devc, 0x78, 0x20, dma16_bits);
			ess_chgmixer (devc, 0x7d, 0x07, dma_bits);
		}
	}
	return 1;
}

int ess_dsp_init (sb_devc *devc, struct address_info *hw_config)
{
	if (devc->model != MDL_ESS) {
		printk (KERN_INFO "ess_dsp_init for non ESS chip\n");
		return 1;
	}
	if (devc->submodel == SUBMDL_ES1887) {
		if (hw_config->dma2 != -1) {
			devc->dma16 = hw_config->dma2;
		}
		if (devc->dma8 != devc->dma16 && devc->dma16 != -1) {
			devc->duplex = 1;
		}
	}
	if (!ess_set_dma_hw (devc)) {
		free_irq(devc->irq, devc);
		return 0;
	}
	return 1;
}


#define ES688_RECORDING_DEVICES	\
			( SOUND_MASK_LINE	| SOUND_MASK_MIC	| SOUND_MASK_CD		)
#define ES688_MIXER_DEVICES		\
			( SOUND_MASK_SYNTH	| SOUND_MASK_PCM	| SOUND_MASK_LINE	\
			| SOUND_MASK_MIC	| SOUND_MASK_CD		| SOUND_MASK_VOLUME	\
			| SOUND_MASK_LINE2	| SOUND_MASK_SPEAKER					)

#define ES1688_RECORDING_DEVICES	\
			( ES688_RECORDING_DEVICES					)
#define ES1688_MIXER_DEVICES		\
			( ES688_MIXER_DEVICES | SOUND_MASK_RECLEV	)

#define ES1887_RECORDING_DEVICES	\
			( ES1688_RECORDING_DEVICES | SOUND_MASK_LINE2 | SOUND_MASK_SYNTH)
#define ES1887_MIXER_DEVICES		\
			( ES1688_MIXER_DEVICES											)

#define ES_REC_MIXER_RECBASE	(SOUND_MIXER_LINE3 + 1)
#define ES_REC_MIXER_RECDIFF	(ES_REC_MIXER_RECBASE - SOUND_MIXER_SYNTH)

#define ES_REC_MIXER_RECSYNTH	(SOUND_MIXER_SYNTH	 + ES_REC_MIXER_RECDIFF)
#define ES_REC_MIXER_RECPCM		(SOUND_MIXER_PCM	 + ES_REC_MIXER_RECDIFF)
#define ES_REC_MIXER_RECSPEAKER	(SOUND_MIXER_SPEAKER + ES_REC_MIXER_RECDIFF)
#define ES_REC_MIXER_RECLINE	(SOUND_MIXER_LINE	 + ES_REC_MIXER_RECDIFF)
#define ES_REC_MIXER_RECMIC		(SOUND_MIXER_MIC	 + ES_REC_MIXER_RECDIFF)
#define ES_REC_MIXER_RECCD		(SOUND_MIXER_CD		 + ES_REC_MIXER_RECDIFF)
#define ES_REC_MIXER_RECIMIX	(SOUND_MIXER_IMIX	 + ES_REC_MIXER_RECDIFF)
#define ES_REC_MIXER_RECALTPCM	(SOUND_MIXER_ALTPCM	 + ES_REC_MIXER_RECDIFF)
#define ES_REC_MIXER_RECRECLEV	(SOUND_MIXER_RECLEV	 + ES_REC_MIXER_RECDIFF)
#define ES_REC_MIXER_RECIGAIN	(SOUND_MIXER_IGAIN	 + ES_REC_MIXER_RECDIFF)
#define ES_REC_MIXER_RECOGAIN	(SOUND_MIXER_OGAIN	 + ES_REC_MIXER_RECDIFF)
#define ES_REC_MIXER_RECLINE1	(SOUND_MIXER_LINE1	 + ES_REC_MIXER_RECDIFF)
#define ES_REC_MIXER_RECLINE2	(SOUND_MIXER_LINE2	 + ES_REC_MIXER_RECDIFF)
#define ES_REC_MIXER_RECLINE3	(SOUND_MIXER_LINE3	 + ES_REC_MIXER_RECDIFF)

static mixer_tab es688_mix = {
MIX_ENT(SOUND_MIXER_VOLUME,			0x32, 7, 4, 0x32, 3, 4),
MIX_ENT(SOUND_MIXER_BASS,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_TREBLE,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_SYNTH,			0x36, 7, 4, 0x36, 3, 4),
MIX_ENT(SOUND_MIXER_PCM,			0x14, 7, 4, 0x14, 3, 4),
MIX_ENT(SOUND_MIXER_SPEAKER,		0x3c, 2, 3, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_LINE,			0x3e, 7, 4, 0x3e, 3, 4),
MIX_ENT(SOUND_MIXER_MIC,			0x1a, 7, 4, 0x1a, 3, 4),
MIX_ENT(SOUND_MIXER_CD,				0x38, 7, 4, 0x38, 3, 4),
MIX_ENT(SOUND_MIXER_IMIX,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_ALTPCM,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_RECLEV,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_IGAIN,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_OGAIN,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_LINE1,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_LINE2,			0x3a, 7, 4, 0x3a, 3, 4),
MIX_ENT(SOUND_MIXER_LINE3,			0x00, 0, 0, 0x00, 0, 0)
};

static mixer_tab es1688_mix = {
MIX_ENT(SOUND_MIXER_VOLUME,			0x32, 7, 4, 0x32, 3, 4),
MIX_ENT(SOUND_MIXER_BASS,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_TREBLE,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_SYNTH,			0x36, 7, 4, 0x36, 3, 4),
MIX_ENT(SOUND_MIXER_PCM,			0x14, 7, 4, 0x14, 3, 4),
MIX_ENT(SOUND_MIXER_SPEAKER,		0x3c, 2, 3, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_LINE,			0x3e, 7, 4, 0x3e, 3, 4),
MIX_ENT(SOUND_MIXER_MIC,			0x1a, 7, 4, 0x1a, 3, 4),
MIX_ENT(SOUND_MIXER_CD,				0x38, 7, 4, 0x38, 3, 4),
MIX_ENT(SOUND_MIXER_IMIX,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_ALTPCM,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_RECLEV,			0xb4, 7, 4, 0xb4, 3, 4),
MIX_ENT(SOUND_MIXER_IGAIN,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_OGAIN,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_LINE1,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_LINE2,			0x3a, 7, 4, 0x3a, 3, 4),
MIX_ENT(SOUND_MIXER_LINE3,			0x00, 0, 0, 0x00, 0, 0)
};

static mixer_tab es1688later_mix = {
MIX_ENT(SOUND_MIXER_VOLUME,			0x60, 5, 6, 0x62, 5, 6),
MIX_ENT(SOUND_MIXER_BASS,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_TREBLE,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_SYNTH,			0x36, 7, 4, 0x36, 3, 4),
MIX_ENT(SOUND_MIXER_PCM,			0x14, 7, 4, 0x14, 3, 4),
MIX_ENT(SOUND_MIXER_SPEAKER,		0x3c, 2, 3, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_LINE,			0x3e, 7, 4, 0x3e, 3, 4),
MIX_ENT(SOUND_MIXER_MIC,			0x1a, 7, 4, 0x1a, 3, 4),
MIX_ENT(SOUND_MIXER_CD,				0x38, 7, 4, 0x38, 3, 4),
MIX_ENT(SOUND_MIXER_IMIX,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_ALTPCM,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_RECLEV,			0xb4, 7, 4, 0xb4, 3, 4),
MIX_ENT(SOUND_MIXER_IGAIN,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_OGAIN,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_LINE1,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_LINE2,			0x3a, 7, 4, 0x3a, 3, 4),
MIX_ENT(SOUND_MIXER_LINE3,			0x00, 0, 0, 0x00, 0, 0)
};

static mixer_tab es_rec_mix = {
MIX_ENT(SOUND_MIXER_VOLUME,			0x60, 5, 6, 0x62, 5, 6),
MIX_ENT(SOUND_MIXER_BASS,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_TREBLE,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_SYNTH,			0x36, 7, 4, 0x36, 3, 4),
MIX_ENT(SOUND_MIXER_PCM,			0x14, 7, 4, 0x14, 3, 4),
MIX_ENT(SOUND_MIXER_SPEAKER,		0x3c, 2, 3, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_LINE,			0x3e, 7, 4, 0x3e, 3, 4),
MIX_ENT(SOUND_MIXER_MIC,			0x1a, 7, 4, 0x1a, 3, 4),
MIX_ENT(SOUND_MIXER_CD,				0x38, 7, 4, 0x38, 3, 4),
MIX_ENT(SOUND_MIXER_IMIX,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_ALTPCM,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_RECLEV,			0xb4, 7, 4, 0xb4, 3, 4),
MIX_ENT(SOUND_MIXER_IGAIN,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_OGAIN,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_LINE1,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_LINE2,			0x3a, 7, 4, 0x3a, 3, 4),
MIX_ENT(SOUND_MIXER_LINE3,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(ES_REC_MIXER_RECSYNTH,		0x6b, 7, 4, 0x6b, 3, 4),
MIX_ENT(ES_REC_MIXER_RECPCM,		0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(ES_REC_MIXER_RECSPEAKER,	0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(ES_REC_MIXER_RECLINE,		0x6e, 7, 4, 0x6e, 3, 4),
MIX_ENT(ES_REC_MIXER_RECMIC,		0x68, 7, 4, 0x68, 3, 4),
MIX_ENT(ES_REC_MIXER_RECCD,			0x6a, 7, 4, 0x6a, 3, 4),
MIX_ENT(ES_REC_MIXER_RECIMIX,		0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(ES_REC_MIXER_RECALTPCM,		0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(ES_REC_MIXER_RECRECLEV,		0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(ES_REC_MIXER_RECIGAIN,		0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(ES_REC_MIXER_RECOGAIN,		0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(ES_REC_MIXER_RECLINE1,		0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(ES_REC_MIXER_RECLINE2,		0x6c, 7, 4, 0x6c, 3, 4),
MIX_ENT(ES_REC_MIXER_RECLINE3,		0x00, 0, 0, 0x00, 0, 0)
};

static mixer_tab es1887_mix = {
MIX_ENT(SOUND_MIXER_VOLUME,			0x60, 5, 6, 0x62, 5, 6),
MIX_ENT(SOUND_MIXER_BASS,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_TREBLE,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_SYNTH,			0x36, 7, 4, 0x36, 3, 4),
MIX_ENT(SOUND_MIXER_PCM,			0x7c, 7, 4, 0x7c, 3, 4),
MIX_ENT(SOUND_MIXER_SPEAKER,		0x3c, 2, 3, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_LINE,			0x3e, 7, 4, 0x3e, 3, 4),
MIX_ENT(SOUND_MIXER_MIC,			0x1a, 7, 4, 0x1a, 3, 4),
MIX_ENT(SOUND_MIXER_CD,				0x38, 7, 4, 0x38, 3, 4),
MIX_ENT(SOUND_MIXER_IMIX,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_ALTPCM,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_RECLEV,			0xb4, 7, 4, 0xb4, 3, 4),
MIX_ENT(SOUND_MIXER_IGAIN,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_OGAIN,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_LINE1,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_LINE2,			0x3a, 7, 4, 0x3a, 3, 4),
MIX_ENT(SOUND_MIXER_LINE3,			0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(ES_REC_MIXER_RECSYNTH,		0x6b, 7, 4, 0x6b, 3, 4),
MIX_ENT(ES_REC_MIXER_RECPCM,		0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(ES_REC_MIXER_RECSPEAKER,	0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(ES_REC_MIXER_RECLINE,		0x6e, 7, 4, 0x6e, 3, 4),
MIX_ENT(ES_REC_MIXER_RECMIC,		0x68, 7, 4, 0x68, 3, 4),
MIX_ENT(ES_REC_MIXER_RECCD,			0x6a, 7, 4, 0x6a, 3, 4),
MIX_ENT(ES_REC_MIXER_RECIMIX,		0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(ES_REC_MIXER_RECALTPCM,		0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(ES_REC_MIXER_RECRECLEV,		0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(ES_REC_MIXER_RECIGAIN,		0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(ES_REC_MIXER_RECOGAIN,		0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(ES_REC_MIXER_RECLINE1,		0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(ES_REC_MIXER_RECLINE2,		0x6c, 7, 4, 0x6c, 3, 4),
MIX_ENT(ES_REC_MIXER_RECLINE3,		0x00, 0, 0, 0x00, 0, 0)
};

static int ess_has_rec_mixer (int submodel)
{
	switch (submodel) {
	case SUBMDL_ES1887:
		return 1;
	default:
		return 0;
	};
};

#ifdef FKS_LOGGING
static int ess_mixer_mon_regs[]
	= { 0x70, 0x71, 0x72, 0x74, 0x76, 0x78, 0x7a, 0x7c, 0x7d, 0x7f
	  , 0xa1, 0xa2, 0xa4, 0xa5, 0xa8, 0xa9
	  , 0xb1, 0xb2, 0xb4, 0xb5, 0xb6, 0xb7, 0xb9
	  , 0x00};

static void ess_show_mixerregs (sb_devc *devc)
{
	int *mp = ess_mixer_mon_regs;

return;

	while (*mp != 0) {
		printk (KERN_INFO "res (%x)=%x\n", *mp, (int)(ess_getmixer (devc, *mp)));
		mp++;
	}
}
#endif

void ess_setmixer (sb_devc * devc, unsigned int port, unsigned int value)
{
	unsigned long flags;

#ifdef FKS_LOGGING
printk(KERN_INFO "FKS: write mixer %x: %x\n", port, value);
#endif

	spin_lock_irqsave(&devc->lock, flags);
	if (port >= 0xa0) {
		ess_write (devc, port, value);
	} else {
		outb(((unsigned char) (port & 0xff)), MIXER_ADDR);

		udelay(20);
		outb(((unsigned char) (value & 0xff)), MIXER_DATA);
		udelay(20);
	};
	spin_unlock_irqrestore(&devc->lock, flags);
}

unsigned int ess_getmixer (sb_devc * devc, unsigned int port)
{
	unsigned int val;
	unsigned long flags;

	spin_lock_irqsave(&devc->lock, flags);

	if (port >= 0xa0) {
		val = ess_read (devc, port);
	} else {
		outb(((unsigned char) (port & 0xff)), MIXER_ADDR);

		udelay(20);
		val = inb(MIXER_DATA);
		udelay(20);
	}
	spin_unlock_irqrestore(&devc->lock, flags);

	return val;
}

static void ess_chgmixer
	(sb_devc * devc, unsigned int reg, unsigned int mask, unsigned int val)
{
	int value;

	value = ess_getmixer (devc, reg);
	value = (value & ~mask) | (val & mask);
	ess_setmixer (devc, reg, value);
}

void ess_mixer_init (sb_devc * devc)
{
	devc->mixer_caps = SOUND_CAP_EXCL_INPUT;

	switch (devc->submodel) {
	case SUBMDL_ES1887:
		devc->supported_devices		= ES1887_MIXER_DEVICES;
		devc->supported_rec_devices	= ES1887_RECORDING_DEVICES;
#ifdef FKS_LOGGING
printk (KERN_INFO "FKS: ess_mixer_init dup = %d\n", devc->duplex);
#endif
		if (devc->duplex) {
			devc->iomap				= &es1887_mix;
			devc->iomap_sz                          = ARRAY_SIZE(es1887_mix);
		} else {
			devc->iomap				= &es_rec_mix;
			devc->iomap_sz                          = ARRAY_SIZE(es_rec_mix);
		}
		break;
	default:
		if (devc->submodel < 8) {
			devc->supported_devices		= ES688_MIXER_DEVICES;
			devc->supported_rec_devices	= ES688_RECORDING_DEVICES;
			devc->iomap					= &es688_mix;
			devc->iomap_sz                                  = ARRAY_SIZE(es688_mix);
		} else {
			devc->supported_devices		= ES1688_MIXER_DEVICES;
			devc->supported_rec_devices	= ES1688_RECORDING_DEVICES;
			if (devc->submodel < 0x10) {
				devc->iomap				= &es1688_mix;
				devc->iomap_sz                          = ARRAY_SIZE(es688_mix);
			} else {
				devc->iomap				= &es1688later_mix;
				devc->iomap_sz                          = ARRAY_SIZE(es1688later_mix);
			}
		}
	}
}

int ess_mixer_set(sb_devc *devc, int dev, int left, int right)
{
	if (ess_has_rec_mixer (devc->submodel) && (devc->recmask & (1 << dev))) {
		sb_common_mixer_set (devc, dev + ES_REC_MIXER_RECDIFF, left, right);
	}
	return sb_common_mixer_set (devc, dev, left, right);
}

void ess_mixer_reload (sb_devc *devc, int dev)
{
	int left, right, value;

	value = devc->levels[dev];
	left  = value & 0x000000ff;
	right = (value & 0x0000ff00) >> 8;

	sb_common_mixer_set(devc, dev, left, right);
}

static int es_rec_set_recmask(sb_devc * devc, int mask)
{
	int i, i_mask, cur_mask, diff_mask;
	int value, left, right;

#ifdef FKS_LOGGING
printk (KERN_INFO "FKS: es_rec_set_recmask mask = %x\n", mask);
#endif
	cur_mask  = devc->recmask;
	diff_mask = (cur_mask ^ mask);

	for (i = 0; i < 32; i++) {
		i_mask = (1 << i);
		if (diff_mask & i_mask) {	
			if (mask & i_mask) {	
				value = devc->levels[i];
				left  = value & 0x000000ff;
				right = (value & 0x0000ff00) >> 8;
			} else {				
				left  = 0;
				right = 0;
			}
			sb_common_mixer_set(devc, i + ES_REC_MIXER_RECDIFF, left, right);
		}
	}
	return mask;
}

int ess_set_recmask(sb_devc * devc, int *mask)
{
	

	if (ess_has_rec_mixer (devc->submodel)) {
		*mask	= es_rec_set_recmask (devc, *mask);
		return 1;									
	} else {
		return 0;									
	}
}

int ess_mixer_reset (sb_devc * devc)
{
	if (ess_has_rec_mixer (devc->submodel)) {
		switch (devc->submodel) {
		case SUBMDL_ES1887:
			ess_chgmixer(devc, 0x7a, 0x18, 0x08);
			ess_chgmixer(devc, 0x1c, 0x07, 0x07);
			break;
		};
		devc->recmask = devc->supported_rec_devices;
		es_rec_set_recmask(devc, 0);
		devc->recmask = 0;

		return 1;	
	} else {
		return 0;	
	}
}


int ess_midi_init(sb_devc * devc, struct address_info *hw_config)
{
	unsigned char   cfg, tmp;

	cfg = ess_getmixer (devc, 0x40) & 0x03;

	if (devc->submodel < 8) {
		ess_setmixer (devc, 0x40, cfg | 0x03);	
		return 0;  					 
	}
	tmp = (hw_config->io_base & 0x0f0) >> 4;

	if (tmp > 3) {
		ess_setmixer (devc, 0x40, cfg);
		return 0;
	}
	cfg |= tmp << 3;

	tmp = 1;		

	

	switch (abs(hw_config->irq)) {
		case 9:
			tmp = 0x4;
			break;
		case 5:
			tmp = 0x5;
			break;
		case 7:
			tmp = 0x6;
			break;
		case 10:
			tmp = 0x7;
			break;
		default:
			return 0;
	}

	cfg |= tmp << 5;
	ess_setmixer (devc, 0x40, cfg | 0x03);

	return 1;
}

