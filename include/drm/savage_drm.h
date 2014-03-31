/* savage_drm.h -- Public header for the savage driver
 *
 * Copyright 2004  Felix Kuehling
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT. IN NO EVENT SHALL FELIX KUEHLING BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __SAVAGE_DRM_H__
#define __SAVAGE_DRM_H__

#ifndef __SAVAGE_SAREA_DEFINES__
#define __SAVAGE_SAREA_DEFINES__


#define SAVAGE_CARD_HEAP		0
#define SAVAGE_AGP_HEAP			1
#define SAVAGE_NR_TEX_HEAPS		2
#define SAVAGE_NR_TEX_REGIONS		16
#define SAVAGE_LOG_MIN_TEX_REGION_SIZE	16

#endif				

typedef struct _drm_savage_sarea {
	struct drm_tex_region texList[SAVAGE_NR_TEX_HEAPS][SAVAGE_NR_TEX_REGIONS +
						      1];
	unsigned int texAge[SAVAGE_NR_TEX_HEAPS];

	int ctxOwner;
} drm_savage_sarea_t, *drm_savage_sarea_ptr;

#define DRM_SAVAGE_BCI_INIT		0x00
#define DRM_SAVAGE_BCI_CMDBUF           0x01
#define DRM_SAVAGE_BCI_EVENT_EMIT	0x02
#define DRM_SAVAGE_BCI_EVENT_WAIT	0x03

#define DRM_IOCTL_SAVAGE_BCI_INIT		DRM_IOW( DRM_COMMAND_BASE + DRM_SAVAGE_BCI_INIT, drm_savage_init_t)
#define DRM_IOCTL_SAVAGE_BCI_CMDBUF		DRM_IOW( DRM_COMMAND_BASE + DRM_SAVAGE_BCI_CMDBUF, drm_savage_cmdbuf_t)
#define DRM_IOCTL_SAVAGE_BCI_EVENT_EMIT	DRM_IOWR(DRM_COMMAND_BASE + DRM_SAVAGE_BCI_EVENT_EMIT, drm_savage_event_emit_t)
#define DRM_IOCTL_SAVAGE_BCI_EVENT_WAIT	DRM_IOW( DRM_COMMAND_BASE + DRM_SAVAGE_BCI_EVENT_WAIT, drm_savage_event_wait_t)

#define SAVAGE_DMA_PCI	1
#define SAVAGE_DMA_AGP	3
typedef struct drm_savage_init {
	enum {
		SAVAGE_INIT_BCI = 1,
		SAVAGE_CLEANUP_BCI = 2
	} func;
	unsigned int sarea_priv_offset;

	
	unsigned int cob_size;
	unsigned int bci_threshold_lo, bci_threshold_hi;
	unsigned int dma_type;

	
	unsigned int fb_bpp;
	unsigned int front_offset, front_pitch;
	unsigned int back_offset, back_pitch;
	unsigned int depth_bpp;
	unsigned int depth_offset, depth_pitch;

	
	unsigned int texture_offset;
	unsigned int texture_size;

	
	unsigned long status_offset;
	unsigned long buffers_offset;
	unsigned long agp_textures_offset;
	unsigned long cmd_dma_offset;
} drm_savage_init_t;

typedef union drm_savage_cmd_header drm_savage_cmd_header_t;
typedef struct drm_savage_cmdbuf {
	
	drm_savage_cmd_header_t __user *cmd_addr;
	unsigned int size;	

	unsigned int dma_idx;	
	int discard;		
	
	unsigned int __user *vb_addr;
	unsigned int vb_size;	
	unsigned int vb_stride;	
	
	struct drm_clip_rect __user *box_addr;
	unsigned int nbox;	
} drm_savage_cmdbuf_t;

#define SAVAGE_WAIT_2D  0x1	
#define SAVAGE_WAIT_3D  0x2	
#define SAVAGE_WAIT_IRQ 0x4	
typedef struct drm_savage_event {
	unsigned int count;
	unsigned int flags;
} drm_savage_event_emit_t, drm_savage_event_wait_t;

#define SAVAGE_CMD_STATE	0	
#define SAVAGE_CMD_DMA_PRIM	1	
#define SAVAGE_CMD_VB_PRIM	2	
#define SAVAGE_CMD_DMA_IDX	3	
#define SAVAGE_CMD_VB_IDX	4	
#define SAVAGE_CMD_CLEAR	5	
#define SAVAGE_CMD_SWAP		6	

#define SAVAGE_PRIM_TRILIST	0	
#define SAVAGE_PRIM_TRISTRIP	1	
#define SAVAGE_PRIM_TRIFAN	2	
#define SAVAGE_PRIM_TRILIST_201	3	

#define SAVAGE_SKIP_Z		0x01
#define SAVAGE_SKIP_W		0x02
#define SAVAGE_SKIP_C0		0x04
#define SAVAGE_SKIP_C1		0x08
#define SAVAGE_SKIP_S0		0x10
#define SAVAGE_SKIP_T0		0x20
#define SAVAGE_SKIP_ST0		0x30
#define SAVAGE_SKIP_S1		0x40
#define SAVAGE_SKIP_T1		0x80
#define SAVAGE_SKIP_ST1		0xc0
#define SAVAGE_SKIP_ALL_S3D	0x3f
#define SAVAGE_SKIP_ALL_S4	0xff

#define SAVAGE_FRONT		0x1
#define SAVAGE_BACK		0x2
#define SAVAGE_DEPTH		0x4

union drm_savage_cmd_header {
	struct {
		unsigned char cmd;	
		unsigned char pad0;
		unsigned short pad1;
		unsigned short pad2;
		unsigned short pad3;
	} cmd;			
	struct {
		unsigned char cmd;
		unsigned char global;	
		unsigned short count;	
		unsigned short start;	
		unsigned short pad3;
	} state;		
	struct {
		unsigned char cmd;
		unsigned char prim;	
		unsigned short skip;	
		unsigned short count;	
		unsigned short start;	
	} prim;			
	struct {
		unsigned char cmd;
		unsigned char prim;
		unsigned short skip;
		unsigned short count;	
		unsigned short pad3;
	} idx;			
	struct {
		unsigned char cmd;
		unsigned char pad0;
		unsigned short pad1;
		unsigned int flags;
	} clear0;		
	struct {
		unsigned int mask;
		unsigned int value;
	} clear1;		
};

#endif
