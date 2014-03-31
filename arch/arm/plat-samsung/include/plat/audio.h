/* arch/arm/plat-samsung/include/plat/audio.h
 *
 * Copyright (c) 2009 Samsung Electronics Co. Ltd
 * Author: Jaswinder Singh <jassi.brar@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define S3C64XX_AC97_GPD  0
#define S3C64XX_AC97_GPE  1
extern void s3c64xx_ac97_setup_gpio(int);

#define S5PC100_SPDIF_GPD  0
#define S5PC100_SPDIF_GPG3 1
extern void s5pc100_spdif_setup_gpio(int);

struct samsung_i2s {
#define QUIRK_PRI_6CHAN		(1 << 0)
#define QUIRK_SEC_DAI		(1 << 1)
#define QUIRK_NO_MUXPSR		(1 << 2)
#define QUIRK_NEED_RSTCLR	(1 << 3)
	
	u32 quirks;

	const char **src_clk;
	dma_addr_t idma_addr;
};

struct s3c_audio_pdata {
	int (*cfg_gpio)(struct platform_device *);
	union {
		struct samsung_i2s i2s;
	} type;
};
