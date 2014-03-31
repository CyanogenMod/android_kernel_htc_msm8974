/*
 * Copyright (c) 2011 Broadcom Corporation
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

#ifndef _USB_RDL_H
#define _USB_RDL_H

#define DL_GETSTATE	0	
#define DL_CHECK_CRC	1	
#define DL_GO		2	
#define DL_START	3	
#define DL_REBOOT	4	
#define DL_GETVER	5	
#define DL_GO_PROTECTED	6	
#define DL_EXEC		7	
#define DL_RESETCFG	8	
#define DL_DEFER_RESP_OK 9	

#define DL_WAITING	0	
#define DL_READY	1	
#define DL_BAD_HDR	2	
#define DL_BAD_CRC	3	
#define DL_RUNNABLE	4	
#define DL_START_FAIL	5	
#define DL_NVRAM_TOOBIG	6	
#define DL_IMAGE_TOOBIG	7	

struct rdl_state_le {
	__le32 state;
	__le32 bytes;
};

struct bootrom_id_le {
	__le32 chip;	
	__le32 chiprev;	
	__le32 ramsize;	
	__le32 remapbase;	
	__le32 boardtype;	
	__le32 boardrev;	
};

#define RDL_CHUNK	1500  

#define TRX_OFFSETS_DLFWLEN_IDX	0
#define TRX_OFFSETS_JUMPTO_IDX	1
#define TRX_OFFSETS_NVM_LEN_IDX	2

#define TRX_OFFSETS_DLBASE_IDX  0

#endif  
