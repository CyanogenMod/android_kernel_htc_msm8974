/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef	_BRCM_SDH_H_
#define	_BRCM_SDH_H_

#include <linux/skbuff.h>

#define SDIO_FUNC_0		0
#define SDIO_FUNC_1		1
#define SDIO_FUNC_2		2

#define SDIOD_FBR_SIZE		0x100

#define SDIO_FUNC_ENABLE_1	0x02
#define SDIO_FUNC_ENABLE_2	0x04

#define SDIO_FUNC_READY_1	0x02
#define SDIO_FUNC_READY_2	0x04

#define INTR_STATUS_FUNC1	0x2
#define INTR_STATUS_FUNC2	0x4

#define SDIOD_MAX_IOFUNCS	7

#define SBSDIO_NUM_FUNCTION		3


#define SBSDIO_SPROM_CS			0x10000
#define SBSDIO_SPROM_INFO		0x10001
#define SBSDIO_SPROM_DATA_LOW		0x10002
#define SBSDIO_SPROM_DATA_HIGH		0x10003
#define SBSDIO_SPROM_ADDR_LOW		0x10004
#define SBSDIO_SPROM_ADDR_HIGH		0x10005
#define SBSDIO_CHIP_CTRL_DATA		0x10006
#define SBSDIO_CHIP_CTRL_EN		0x10007
#define SBSDIO_WATERMARK		0x10008
#define SBSDIO_DEVICE_CTL		0x10009

#define SBSDIO_FUNC1_SBADDRLOW		0x1000A
#define SBSDIO_FUNC1_SBADDRMID		0x1000B
#define SBSDIO_FUNC1_SBADDRHIGH		0x1000C
#define SBSDIO_FUNC1_FRAMECTRL		0x1000D
#define SBSDIO_FUNC1_CHIPCLKCSR		0x1000E
#define SBSDIO_FUNC1_SDIOPULLUP		0x1000F
#define SBSDIO_FUNC1_WFRAMEBCLO		0x10019
#define SBSDIO_FUNC1_WFRAMEBCHI		0x1001A
#define SBSDIO_FUNC1_RFRAMEBCLO		0x1001B
#define SBSDIO_FUNC1_RFRAMEBCHI		0x1001C

#define SBSDIO_FUNC1_MISC_REG_START	0x10000	
#define SBSDIO_FUNC1_MISC_REG_LIMIT	0x1001C	


#define SBSDIO_SB_OFT_ADDR_MASK		0x07FFF
#define SBSDIO_SB_OFT_ADDR_LIMIT	0x08000
#define SBSDIO_SB_ACCESS_2_4B_FLAG	0x08000


#define SBSDIO_SBADDRLOW_MASK		0x80	
#define SBSDIO_SBADDRMID_MASK		0xff	
#define SBSDIO_SBADDRHIGH_MASK		0xffU	
#define SBSDIO_SBWINDOW_MASK		0xffff8000

#define SDIOH_READ              0	
#define SDIOH_WRITE             1	

#define SDIOH_DATA_FIX          0	
#define SDIOH_DATA_INC          1	

#define SUCCESS	0
#define ERROR	1

#define BRCMF_SDALIGN	(1 << 6)

#define BRCMF_WD_POLL_MS	10

struct brcmf_sdreg {
	int func;
	int offset;
	int value;
};

struct brcmf_sdio;

struct brcmf_sdio_dev {
	struct sdio_func *func[SDIO_MAX_FUNCS];
	u8 num_funcs;			
	u32 func_cis_ptr[SDIOD_MAX_IOFUNCS];
	u32 sbwad;			
	bool regfail;			
	void *bus;
	atomic_t suspend;		
	wait_queue_head_t request_byte_wait;
	wait_queue_head_t request_word_wait;
	wait_queue_head_t request_chain_wait;
	wait_queue_head_t request_buffer_wait;
	struct device *dev;
	struct brcmf_bus *bus_if;
};

extern int
brcmf_sdcard_intr_reg(struct brcmf_sdio_dev *sdiodev);

extern int brcmf_sdcard_intr_dereg(struct brcmf_sdio_dev *sdiodev);

extern u8 brcmf_sdcard_cfg_read(struct brcmf_sdio_dev *sdiodev, uint func,
				u32 addr, int *err);
extern void brcmf_sdcard_cfg_write(struct brcmf_sdio_dev *sdiodev, uint func,
				   u32 addr, u8 data, int *err);

extern u32
brcmf_sdcard_reg_read(struct brcmf_sdio_dev *sdiodev, u32 addr, uint size);

extern u32
brcmf_sdcard_reg_write(struct brcmf_sdio_dev *sdiodev, u32 addr, uint size,
		       u32 data);

extern bool brcmf_sdcard_regfail(struct brcmf_sdio_dev *sdiodev);

extern int
brcmf_sdcard_send_pkt(struct brcmf_sdio_dev *sdiodev, u32 addr, uint fn,
		      uint flags, struct sk_buff *pkt);
extern int
brcmf_sdcard_send_buf(struct brcmf_sdio_dev *sdiodev, u32 addr, uint fn,
		      uint flags, u8 *buf, uint nbytes);

extern int
brcmf_sdcard_recv_pkt(struct brcmf_sdio_dev *sdiodev, u32 addr, uint fn,
		      uint flags, struct sk_buff *pkt);
extern int
brcmf_sdcard_recv_buf(struct brcmf_sdio_dev *sdiodev, u32 addr, uint fn,
		      uint flags, u8 *buf, uint nbytes);
extern int
brcmf_sdcard_recv_chain(struct brcmf_sdio_dev *sdiodev, u32 addr, uint fn,
			uint flags, struct sk_buff_head *pktq);


#define SDIO_REQ_4BYTE	0x1
#define SDIO_REQ_FIXED	0x2
#define SDIO_REQ_ASYNC	0x4

extern int brcmf_sdcard_rwdata(struct brcmf_sdio_dev *sdiodev, uint rw,
			       u32 addr, u8 *buf, uint nbytes);

extern int brcmf_sdcard_abort(struct brcmf_sdio_dev *sdiodev, uint fn);

extern int brcmf_sdio_probe(struct brcmf_sdio_dev *sdiodev);
extern int brcmf_sdio_remove(struct brcmf_sdio_dev *sdiodev);

extern int brcmf_sdcard_set_sbaddr_window(struct brcmf_sdio_dev *sdiodev,
					  u32 address);

extern int brcmf_sdioh_attach(struct brcmf_sdio_dev *sdiodev);
extern void brcmf_sdioh_detach(struct brcmf_sdio_dev *sdiodev);

extern int brcmf_sdioh_request_byte(struct brcmf_sdio_dev *sdiodev, uint rw,
				    uint fnc, uint addr, u8 *byte);

extern int
brcmf_sdioh_request_word(struct brcmf_sdio_dev *sdiodev,
			 uint rw, uint fnc, uint addr,
			 u32 *word, uint nbyte);

extern int
brcmf_sdioh_request_buffer(struct brcmf_sdio_dev *sdiodev,
			   uint fix_inc, uint rw, uint fnc_num, u32 addr,
			   struct sk_buff *pkt);
extern int
brcmf_sdioh_request_chain(struct brcmf_sdio_dev *sdiodev, uint fix_inc,
			  uint write, uint func, uint addr,
			  struct sk_buff_head *pktq);

extern void brcmf_sdio_wdtmr_enable(struct brcmf_sdio_dev *sdiodev,
				    bool enable);

extern void *brcmf_sdbrcm_probe(u32 regsva, struct brcmf_sdio_dev *sdiodev);
extern void brcmf_sdbrcm_disconnect(void *ptr);
extern void brcmf_sdbrcm_isr(void *arg);

extern void brcmf_sdbrcm_wd_timer(struct brcmf_sdio *bus, uint wdtick);
#endif				
