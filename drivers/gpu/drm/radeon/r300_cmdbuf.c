/* r300_cmdbuf.c -- Command buffer emission for R300 -*- linux-c -*-
 *
 * Copyright (C) The Weather Channel, Inc.  2002.
 * Copyright (C) 2004 Nicolai Haehnle.
 * All Rights Reserved.
 *
 * The Weather Channel (TM) funded Tungsten Graphics to develop the
 * initial release of the Radeon 8500 driver under the XFree86 license.
 * This notice must be preserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Nicolai Haehnle <prefect_@gmx.net>
 */

#include "drmP.h"
#include "drm.h"
#include "drm_buffer.h"
#include "radeon_drm.h"
#include "radeon_drv.h"
#include "r300_reg.h"

#include <asm/unaligned.h>

#define R300_SIMULTANEOUS_CLIPRECTS		4

static const int r300_cliprect_cntl[4] = {
	0xAAAA,
	0xEEEE,
	0xFEFE,
	0xFFFE
};

static int r300_emit_cliprects(drm_radeon_private_t *dev_priv,
			       drm_radeon_kcmd_buffer_t *cmdbuf, int n)
{
	struct drm_clip_rect box;
	int nr;
	int i;
	RING_LOCALS;

	nr = cmdbuf->nbox - n;
	if (nr > R300_SIMULTANEOUS_CLIPRECTS)
		nr = R300_SIMULTANEOUS_CLIPRECTS;

	DRM_DEBUG("%i cliprects\n", nr);

	if (nr) {
		BEGIN_RING(6 + nr * 2);
		OUT_RING(CP_PACKET0(R300_RE_CLIPRECT_TL_0, nr * 2 - 1));

		for (i = 0; i < nr; ++i) {
			if (DRM_COPY_FROM_USER_UNCHECKED
			    (&box, &cmdbuf->boxes[n + i], sizeof(box))) {
				DRM_ERROR("copy cliprect faulted\n");
				return -EFAULT;
			}

			box.x2--; 
			box.y2--;

			if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV515) {
				box.x1 = (box.x1) &
					R300_CLIPRECT_MASK;
				box.y1 = (box.y1) &
					R300_CLIPRECT_MASK;
				box.x2 = (box.x2) &
					R300_CLIPRECT_MASK;
				box.y2 = (box.y2) &
					R300_CLIPRECT_MASK;
			} else {
				box.x1 = (box.x1 + R300_CLIPRECT_OFFSET) &
					R300_CLIPRECT_MASK;
				box.y1 = (box.y1 + R300_CLIPRECT_OFFSET) &
					R300_CLIPRECT_MASK;
				box.x2 = (box.x2 + R300_CLIPRECT_OFFSET) &
					R300_CLIPRECT_MASK;
				box.y2 = (box.y2 + R300_CLIPRECT_OFFSET) &
					R300_CLIPRECT_MASK;
			}

			OUT_RING((box.x1 << R300_CLIPRECT_X_SHIFT) |
				 (box.y1 << R300_CLIPRECT_Y_SHIFT));
			OUT_RING((box.x2 << R300_CLIPRECT_X_SHIFT) |
				 (box.y2 << R300_CLIPRECT_Y_SHIFT));

		}

		OUT_RING_REG(R300_RE_CLIPRECT_CNTL, r300_cliprect_cntl[nr - 1]);

		OUT_RING(CP_PACKET0(R300_RE_SCISSORS_TL, 1));
		OUT_RING(0);
		OUT_RING(R300_SCISSORS_X_MASK | R300_SCISSORS_Y_MASK);
		ADVANCE_RING();
	} else {
		BEGIN_RING(2);
		OUT_RING_REG(R300_RE_CLIPRECT_CNTL, 0);
		ADVANCE_RING();
	}

	
	BEGIN_RING(2);
	OUT_RING(CP_PACKET0(R300_RB3D_DSTCACHE_CTLSTAT, 0));
	OUT_RING(R300_RB3D_DC_FLUSH);
	ADVANCE_RING();
	BEGIN_RING(2);
	OUT_RING(CP_PACKET0(RADEON_WAIT_UNTIL, 0));
	OUT_RING(RADEON_WAIT_3D_IDLECLEAN);
	ADVANCE_RING();
	
	dev_priv->track_flush |= RADEON_FLUSH_EMITED;

	return 0;
}

static u8 r300_reg_flags[0x10000 >> 2];

void r300_init_reg_flags(struct drm_device *dev)
{
	int i;
	drm_radeon_private_t *dev_priv = dev->dev_private;

	memset(r300_reg_flags, 0, 0x10000 >> 2);
#define ADD_RANGE_MARK(reg, count,mark) \
		for(i=((reg)>>2);i<((reg)>>2)+(count);i++)\
			r300_reg_flags[i]|=(mark);

#define MARK_SAFE		1
#define MARK_CHECK_OFFSET	2

#define ADD_RANGE(reg, count)	ADD_RANGE_MARK(reg, count, MARK_SAFE)

	
	ADD_RANGE(R300_SE_VPORT_XSCALE, 6);
	ADD_RANGE(R300_VAP_CNTL, 1);
	ADD_RANGE(R300_SE_VTE_CNTL, 2);
	ADD_RANGE(0x2134, 2);
	ADD_RANGE(R300_VAP_CNTL_STATUS, 1);
	ADD_RANGE(R300_VAP_INPUT_CNTL_0, 2);
	ADD_RANGE(0x21DC, 1);
	ADD_RANGE(R300_VAP_UNKNOWN_221C, 1);
	ADD_RANGE(R300_VAP_CLIP_X_0, 4);
	ADD_RANGE(R300_VAP_PVS_STATE_FLUSH_REG, 1);
	ADD_RANGE(R300_VAP_UNKNOWN_2288, 1);
	ADD_RANGE(R300_VAP_OUTPUT_VTX_FMT_0, 2);
	ADD_RANGE(R300_VAP_PVS_CNTL_1, 3);
	ADD_RANGE(R300_GB_ENABLE, 1);
	ADD_RANGE(R300_GB_MSPOS0, 5);
	ADD_RANGE(R300_TX_INVALTAGS, 1);
	ADD_RANGE(R300_TX_ENABLE, 1);
	ADD_RANGE(0x4200, 4);
	ADD_RANGE(0x4214, 1);
	ADD_RANGE(R300_RE_POINTSIZE, 1);
	ADD_RANGE(0x4230, 3);
	ADD_RANGE(R300_RE_LINE_CNT, 1);
	ADD_RANGE(R300_RE_UNK4238, 1);
	ADD_RANGE(0x4260, 3);
	ADD_RANGE(R300_RE_SHADE, 4);
	ADD_RANGE(R300_RE_POLYGON_MODE, 5);
	ADD_RANGE(R300_RE_ZBIAS_CNTL, 1);
	ADD_RANGE(R300_RE_ZBIAS_T_FACTOR, 4);
	ADD_RANGE(R300_RE_OCCLUSION_CNTL, 1);
	ADD_RANGE(R300_RE_CULL_CNTL, 1);
	ADD_RANGE(0x42C0, 2);
	ADD_RANGE(R300_RS_CNTL_0, 2);

	ADD_RANGE(R300_SU_REG_DEST, 1);
	if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV530)
		ADD_RANGE(RV530_FG_ZBREG_DEST, 1);

	ADD_RANGE(R300_SC_HYPERZ, 2);
	ADD_RANGE(0x43E8, 1);

	ADD_RANGE(0x46A4, 5);

	ADD_RANGE(R300_RE_FOG_STATE, 1);
	ADD_RANGE(R300_FOG_COLOR_R, 3);
	ADD_RANGE(R300_PP_ALPHA_TEST, 2);
	ADD_RANGE(0x4BD8, 1);
	ADD_RANGE(R300_PFS_PARAM_0_X, 64);
	ADD_RANGE(0x4E00, 1);
	ADD_RANGE(R300_RB3D_CBLEND, 2);
	ADD_RANGE(R300_RB3D_COLORMASK, 1);
	ADD_RANGE(R300_RB3D_BLEND_COLOR, 3);
	ADD_RANGE_MARK(R300_RB3D_COLOROFFSET0, 1, MARK_CHECK_OFFSET);	
	ADD_RANGE(R300_RB3D_COLORPITCH0, 1);
	ADD_RANGE(0x4E50, 9);
	ADD_RANGE(0x4E88, 1);
	ADD_RANGE(0x4EA0, 2);
	ADD_RANGE(R300_ZB_CNTL, 3);
	ADD_RANGE(R300_ZB_FORMAT, 4);
	ADD_RANGE_MARK(R300_ZB_DEPTHOFFSET, 1, MARK_CHECK_OFFSET);	
	ADD_RANGE(R300_ZB_DEPTHPITCH, 1);
	ADD_RANGE(R300_ZB_DEPTHCLEARVALUE, 1);
	ADD_RANGE(R300_ZB_ZMASK_OFFSET, 13);
	ADD_RANGE(R300_ZB_ZPASS_DATA, 2); 

	ADD_RANGE(R300_TX_FILTER_0, 16);
	ADD_RANGE(R300_TX_FILTER1_0, 16);
	ADD_RANGE(R300_TX_SIZE_0, 16);
	ADD_RANGE(R300_TX_FORMAT_0, 16);
	ADD_RANGE(R300_TX_PITCH_0, 16);
	
	ADD_RANGE_MARK(R300_TX_OFFSET_0, 16, MARK_CHECK_OFFSET);
	ADD_RANGE(R300_TX_CHROMA_KEY_0, 16);
	ADD_RANGE(R300_TX_BORDER_COLOR_0, 16);

	
	ADD_RANGE(R300_ZB_ZCACHE_CTLSTAT, 1);
	ADD_RANGE(R300_RB3D_DSTCACHE_CTLSTAT, 1);
	ADD_RANGE(R300_VAP_INPUT_ROUTE_0_0, 8);
	ADD_RANGE(R300_VAP_INPUT_ROUTE_1_0, 8);

	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV515) {
		ADD_RANGE(R500_VAP_INDEX_OFFSET, 1);
		ADD_RANGE(R500_US_CONFIG, 2);
		ADD_RANGE(R500_US_CODE_ADDR, 3);
		ADD_RANGE(R500_US_FC_CTRL, 1);
		ADD_RANGE(R500_RS_IP_0, 16);
		ADD_RANGE(R500_RS_INST_0, 16);
		ADD_RANGE(R500_RB3D_COLOR_CLEAR_VALUE_AR, 2);
		ADD_RANGE(R500_RB3D_CONSTANT_COLOR_AR, 2);
		ADD_RANGE(R500_ZB_FIFO_SIZE, 2);
	} else {
		ADD_RANGE(R300_PFS_CNTL_0, 3);
		ADD_RANGE(R300_PFS_NODE_0, 4);
		ADD_RANGE(R300_PFS_TEXI_0, 64);
		ADD_RANGE(R300_PFS_INSTR0_0, 64);
		ADD_RANGE(R300_PFS_INSTR1_0, 64);
		ADD_RANGE(R300_PFS_INSTR2_0, 64);
		ADD_RANGE(R300_PFS_INSTR3_0, 64);
		ADD_RANGE(R300_RS_INTERP_0, 8);
		ADD_RANGE(R300_RS_ROUTE_0, 8);

	}
}

static __inline__ int r300_check_range(unsigned reg, int count)
{
	int i;
	if (reg & ~0xffff)
		return -1;
	for (i = (reg >> 2); i < (reg >> 2) + count; i++)
		if (r300_reg_flags[i] != MARK_SAFE)
			return 1;
	return 0;
}

static __inline__ int r300_emit_carefully_checked_packet0(drm_radeon_private_t *
							  dev_priv,
							  drm_radeon_kcmd_buffer_t
							  * cmdbuf,
							  drm_r300_cmd_header_t
							  header)
{
	int reg;
	int sz;
	int i;
	u32 *value;
	RING_LOCALS;

	sz = header.packet0.count;
	reg = (header.packet0.reghi << 8) | header.packet0.reglo;

	if ((sz > 64) || (sz < 0)) {
		DRM_ERROR("Cannot emit more than 64 values at a time (reg=%04x sz=%d)\n",
			 reg, sz);
		return -EINVAL;
	}

	for (i = 0; i < sz; i++) {
		switch (r300_reg_flags[(reg >> 2) + i]) {
		case MARK_SAFE:
			break;
		case MARK_CHECK_OFFSET:
			value = drm_buffer_pointer_to_dword(cmdbuf->buffer, i);
			if (!radeon_check_offset(dev_priv, *value)) {
				DRM_ERROR("Offset failed range check (reg=%04x sz=%d)\n",
					 reg, sz);
				return -EINVAL;
			}
			break;
		default:
			DRM_ERROR("Register %04x failed check as flag=%02x\n",
				reg + i * 4, r300_reg_flags[(reg >> 2) + i]);
			return -EINVAL;
		}
	}

	BEGIN_RING(1 + sz);
	OUT_RING(CP_PACKET0(reg, sz - 1));
	OUT_RING_DRM_BUFFER(cmdbuf->buffer, sz);
	ADVANCE_RING();

	return 0;
}

static __inline__ int r300_emit_packet0(drm_radeon_private_t *dev_priv,
					drm_radeon_kcmd_buffer_t *cmdbuf,
					drm_r300_cmd_header_t header)
{
	int reg;
	int sz;
	RING_LOCALS;

	sz = header.packet0.count;
	reg = (header.packet0.reghi << 8) | header.packet0.reglo;

	if (!sz)
		return 0;

	if (sz * 4 > drm_buffer_unprocessed(cmdbuf->buffer))
		return -EINVAL;

	if (reg + sz * 4 >= 0x10000) {
		DRM_ERROR("No such registers in hardware reg=%04x sz=%d\n", reg,
			  sz);
		return -EINVAL;
	}

	if (r300_check_range(reg, sz)) {
		
		return r300_emit_carefully_checked_packet0(dev_priv, cmdbuf,
							   header);
	}
	

	BEGIN_RING(1 + sz);
	OUT_RING(CP_PACKET0(reg, sz - 1));
	OUT_RING_DRM_BUFFER(cmdbuf->buffer, sz);
	ADVANCE_RING();

	return 0;
}

static __inline__ int r300_emit_vpu(drm_radeon_private_t *dev_priv,
				    drm_radeon_kcmd_buffer_t *cmdbuf,
				    drm_r300_cmd_header_t header)
{
	int sz;
	int addr;
	RING_LOCALS;

	sz = header.vpu.count;
	addr = (header.vpu.adrhi << 8) | header.vpu.adrlo;

	if (!sz)
		return 0;
	if (sz * 16 > drm_buffer_unprocessed(cmdbuf->buffer))
		return -EINVAL;

	BEGIN_RING(6);
	OUT_RING(CP_PACKET0(R300_RB3D_DSTCACHE_CTLSTAT, 0));
	OUT_RING(R300_RB3D_DC_FLUSH);
	OUT_RING(CP_PACKET0(RADEON_WAIT_UNTIL, 0));
	OUT_RING(RADEON_WAIT_3D_IDLECLEAN);
	OUT_RING(CP_PACKET0(R300_VAP_PVS_STATE_FLUSH_REG, 0));
	OUT_RING(0);
	ADVANCE_RING();
	
	dev_priv->track_flush |= RADEON_FLUSH_EMITED;

	BEGIN_RING(3 + sz * 4);
	OUT_RING_REG(R300_VAP_PVS_UPLOAD_ADDRESS, addr);
	OUT_RING(CP_PACKET0_TABLE(R300_VAP_PVS_UPLOAD_DATA, sz * 4 - 1));
	OUT_RING_DRM_BUFFER(cmdbuf->buffer, sz * 4);
	ADVANCE_RING();

	BEGIN_RING(2);
	OUT_RING(CP_PACKET0(R300_VAP_PVS_STATE_FLUSH_REG, 0));
	OUT_RING(0);
	ADVANCE_RING();

	return 0;
}

static __inline__ int r300_emit_clear(drm_radeon_private_t *dev_priv,
				      drm_radeon_kcmd_buffer_t *cmdbuf)
{
	RING_LOCALS;

	if (8 * 4 > drm_buffer_unprocessed(cmdbuf->buffer))
		return -EINVAL;

	BEGIN_RING(10);
	OUT_RING(CP_PACKET3(R200_3D_DRAW_IMMD_2, 8));
	OUT_RING(R300_PRIM_TYPE_POINT | R300_PRIM_WALK_RING |
		 (1 << R300_PRIM_NUM_VERTICES_SHIFT));
	OUT_RING_DRM_BUFFER(cmdbuf->buffer, 8);
	ADVANCE_RING();

	BEGIN_RING(4);
	OUT_RING(CP_PACKET0(R300_RB3D_DSTCACHE_CTLSTAT, 0));
	OUT_RING(R300_RB3D_DC_FLUSH);
	OUT_RING(CP_PACKET0(RADEON_WAIT_UNTIL, 0));
	OUT_RING(RADEON_WAIT_3D_IDLECLEAN);
	ADVANCE_RING();
	
	dev_priv->track_flush |= RADEON_FLUSH_EMITED;

	return 0;
}

static __inline__ int r300_emit_3d_load_vbpntr(drm_radeon_private_t *dev_priv,
					       drm_radeon_kcmd_buffer_t *cmdbuf,
					       u32 header)
{
	int count, i, k;
#define MAX_ARRAY_PACKET  64
	u32 *data;
	u32 narrays;
	RING_LOCALS;

	count = (header & RADEON_CP_PACKET_COUNT_MASK) >> 16;

	if ((count + 1) > MAX_ARRAY_PACKET) {
		DRM_ERROR("Too large payload in 3D_LOAD_VBPNTR (count=%d)\n",
			  count);
		return -EINVAL;
	}
	

	
	drm_buffer_advance(cmdbuf->buffer, 4);

	narrays = *(u32 *)drm_buffer_pointer_to_dword(cmdbuf->buffer, 0);
	k = 0;
	i = 1;
	while ((k < narrays) && (i < (count + 1))) {
		i++;		
		data = drm_buffer_pointer_to_dword(cmdbuf->buffer, i);
		if (!radeon_check_offset(dev_priv, *data)) {
			DRM_ERROR
			    ("Offset failed range check (k=%d i=%d) while processing 3D_LOAD_VBPNTR packet.\n",
			     k, i);
			return -EINVAL;
		}
		k++;
		i++;
		if (k == narrays)
			break;
		
		data = drm_buffer_pointer_to_dword(cmdbuf->buffer, i);
		if (!radeon_check_offset(dev_priv, *data)) {
			DRM_ERROR
			    ("Offset failed range check (k=%d i=%d) while processing 3D_LOAD_VBPNTR packet.\n",
			     k, i);
			return -EINVAL;
		}
		k++;
		i++;
	}
	
	if ((k != narrays) || (i != (count + 1))) {
		DRM_ERROR
		    ("Malformed 3D_LOAD_VBPNTR packet (k=%d i=%d narrays=%d count+1=%d).\n",
		     k, i, narrays, count + 1);
		return -EINVAL;
	}

	

	BEGIN_RING(count + 2);
	OUT_RING(header);
	OUT_RING_DRM_BUFFER(cmdbuf->buffer, count + 1);
	ADVANCE_RING();

	return 0;
}

static __inline__ int r300_emit_bitblt_multi(drm_radeon_private_t *dev_priv,
					     drm_radeon_kcmd_buffer_t *cmdbuf)
{
	u32 *cmd = drm_buffer_pointer_to_dword(cmdbuf->buffer, 0);
	int count, ret;
	RING_LOCALS;


	count = (*cmd & RADEON_CP_PACKET_COUNT_MASK) >> 16;

	if (*cmd & 0x8000) {
		u32 offset;
		u32 *cmd1 = drm_buffer_pointer_to_dword(cmdbuf->buffer, 1);
		if (*cmd1 & (RADEON_GMC_SRC_PITCH_OFFSET_CNTL
			      | RADEON_GMC_DST_PITCH_OFFSET_CNTL)) {

			u32 *cmd2 = drm_buffer_pointer_to_dword(cmdbuf->buffer, 2);
			offset = *cmd2 << 10;
			ret = !radeon_check_offset(dev_priv, offset);
			if (ret) {
				DRM_ERROR("Invalid bitblt first offset is %08X\n", offset);
				return -EINVAL;
			}
		}

		if ((*cmd1 & RADEON_GMC_SRC_PITCH_OFFSET_CNTL) &&
		    (*cmd1 & RADEON_GMC_DST_PITCH_OFFSET_CNTL)) {
			u32 *cmd3 = drm_buffer_pointer_to_dword(cmdbuf->buffer, 3);
			offset = *cmd3 << 10;
			ret = !radeon_check_offset(dev_priv, offset);
			if (ret) {
				DRM_ERROR("Invalid bitblt second offset is %08X\n", offset);
				return -EINVAL;
			}

		}
	}

	BEGIN_RING(count+2);
	OUT_RING_DRM_BUFFER(cmdbuf->buffer, count + 2);
	ADVANCE_RING();

	return 0;
}

static __inline__ int r300_emit_draw_indx_2(drm_radeon_private_t *dev_priv,
					    drm_radeon_kcmd_buffer_t *cmdbuf)
{
	u32 *cmd = drm_buffer_pointer_to_dword(cmdbuf->buffer, 0);
	u32 *cmd1 = drm_buffer_pointer_to_dword(cmdbuf->buffer, 1);
	int count;
	int expected_count;
	RING_LOCALS;

	count = (*cmd & RADEON_CP_PACKET_COUNT_MASK) >> 16;

	expected_count = *cmd1 >> 16;
	if (!(*cmd1 & R300_VAP_VF_CNTL__INDEX_SIZE_32bit))
		expected_count = (expected_count+1)/2;

	if (count && count != expected_count) {
		DRM_ERROR("3D_DRAW_INDX_2: packet size %i, expected %i\n",
			count, expected_count);
		return -EINVAL;
	}

	BEGIN_RING(count+2);
	OUT_RING_DRM_BUFFER(cmdbuf->buffer, count + 2);
	ADVANCE_RING();

	if (!count) {
		drm_r300_cmd_header_t stack_header, *header;
		u32 *cmd1, *cmd2, *cmd3;

		if (drm_buffer_unprocessed(cmdbuf->buffer)
				< 4*4 + sizeof(stack_header)) {
			DRM_ERROR("3D_DRAW_INDX_2: expect subsequent INDX_BUFFER, but stream is too short.\n");
			return -EINVAL;
		}

		header = drm_buffer_read_object(cmdbuf->buffer,
				sizeof(stack_header), &stack_header);

		cmd = drm_buffer_pointer_to_dword(cmdbuf->buffer, 0);
		cmd1 = drm_buffer_pointer_to_dword(cmdbuf->buffer, 1);
		cmd2 = drm_buffer_pointer_to_dword(cmdbuf->buffer, 2);
		cmd3 = drm_buffer_pointer_to_dword(cmdbuf->buffer, 3);

		if (header->header.cmd_type != R300_CMD_PACKET3 ||
		    header->packet3.packet != R300_CMD_PACKET3_RAW ||
		    *cmd != CP_PACKET3(RADEON_CP_INDX_BUFFER, 2)) {
			DRM_ERROR("3D_DRAW_INDX_2: expect subsequent INDX_BUFFER.\n");
			return -EINVAL;
		}

		if ((*cmd1 & 0x8000ffff) != 0x80000810) {
			DRM_ERROR("Invalid indx_buffer reg address %08X\n",
					*cmd1);
			return -EINVAL;
		}
		if (!radeon_check_offset(dev_priv, *cmd2)) {
			DRM_ERROR("Invalid indx_buffer offset is %08X\n",
					*cmd2);
			return -EINVAL;
		}
		if (*cmd3 != expected_count) {
			DRM_ERROR("INDX_BUFFER: buffer size %i, expected %i\n",
				*cmd3, expected_count);
			return -EINVAL;
		}

		BEGIN_RING(4);
		OUT_RING_DRM_BUFFER(cmdbuf->buffer, 4);
		ADVANCE_RING();
	}

	return 0;
}

static __inline__ int r300_emit_raw_packet3(drm_radeon_private_t *dev_priv,
					    drm_radeon_kcmd_buffer_t *cmdbuf)
{
	u32 *header;
	int count;
	RING_LOCALS;

	if (4 > drm_buffer_unprocessed(cmdbuf->buffer))
		return -EINVAL;


	
	header = drm_buffer_pointer_to_dword(cmdbuf->buffer, 0);

	
	if ((*header >> 30) != 0x3) {
		DRM_ERROR("Not a packet3 header (0x%08x)\n", *header);
		return -EINVAL;
	}

	count = (*header >> 16) & 0x3fff;

	
	if ((count + 2) * 4 > drm_buffer_unprocessed(cmdbuf->buffer)) {
		DRM_ERROR
		    ("Expected packet3 of length %d but have only %d bytes left\n",
		     (count + 2) * 4, drm_buffer_unprocessed(cmdbuf->buffer));
		return -EINVAL;
	}

	
	switch (*header & 0xff00) {
	case RADEON_3D_LOAD_VBPNTR:	
		return r300_emit_3d_load_vbpntr(dev_priv, cmdbuf, *header);

	case RADEON_CNTL_BITBLT_MULTI:
		return r300_emit_bitblt_multi(dev_priv, cmdbuf);

	case RADEON_CP_INDX_BUFFER:
		DRM_ERROR("packet3 INDX_BUFFER without preceding 3D_DRAW_INDX_2 is illegal.\n");
		return -EINVAL;
	case RADEON_CP_3D_DRAW_IMMD_2:
		
	case RADEON_CP_3D_DRAW_VBUF_2:
		
		dev_priv->track_flush &= ~(RADEON_FLUSH_EMITED |
					   RADEON_PURGE_EMITED);
		break;
	case RADEON_CP_3D_DRAW_INDX_2:
		
		
		dev_priv->track_flush &= ~(RADEON_FLUSH_EMITED |
					   RADEON_PURGE_EMITED);
		return r300_emit_draw_indx_2(dev_priv, cmdbuf);
	case RADEON_WAIT_FOR_IDLE:
	case RADEON_CP_NOP:
		
		break;
	default:
		DRM_ERROR("Unknown packet3 header (0x%08x)\n", *header);
		return -EINVAL;
	}

	BEGIN_RING(count + 2);
	OUT_RING_DRM_BUFFER(cmdbuf->buffer, count + 2);
	ADVANCE_RING();

	return 0;
}

static __inline__ int r300_emit_packet3(drm_radeon_private_t *dev_priv,
					drm_radeon_kcmd_buffer_t *cmdbuf,
					drm_r300_cmd_header_t header)
{
	int n;
	int ret;
	int orig_iter = cmdbuf->buffer->iterator;

	n = 0;
	do {
		if (cmdbuf->nbox > R300_SIMULTANEOUS_CLIPRECTS) {
			ret = r300_emit_cliprects(dev_priv, cmdbuf, n);
			if (ret)
				return ret;

			cmdbuf->buffer->iterator = orig_iter;
		}

		switch (header.packet3.packet) {
		case R300_CMD_PACKET3_CLEAR:
			DRM_DEBUG("R300_CMD_PACKET3_CLEAR\n");
			ret = r300_emit_clear(dev_priv, cmdbuf);
			if (ret) {
				DRM_ERROR("r300_emit_clear failed\n");
				return ret;
			}
			break;

		case R300_CMD_PACKET3_RAW:
			DRM_DEBUG("R300_CMD_PACKET3_RAW\n");
			ret = r300_emit_raw_packet3(dev_priv, cmdbuf);
			if (ret) {
				DRM_ERROR("r300_emit_raw_packet3 failed\n");
				return ret;
			}
			break;

		default:
			DRM_ERROR("bad packet3 type %i at byte %d\n",
				  header.packet3.packet,
				  cmdbuf->buffer->iterator - (int)sizeof(header));
			return -EINVAL;
		}

		n += R300_SIMULTANEOUS_CLIPRECTS;
	} while (n < cmdbuf->nbox);

	return 0;
}


static void r300_pacify(drm_radeon_private_t *dev_priv)
{
	uint32_t cache_z, cache_3d, cache_2d;
	RING_LOCALS;

	cache_z = R300_ZC_FLUSH;
	cache_2d = R300_RB2D_DC_FLUSH;
	cache_3d = R300_RB3D_DC_FLUSH;
	if (!(dev_priv->track_flush & RADEON_PURGE_EMITED)) {
		
		cache_z |= R300_ZC_FREE;
		cache_2d |= R300_RB2D_DC_FREE;
		cache_3d |= R300_RB3D_DC_FREE;
	}

	
	BEGIN_RING(2);
	OUT_RING(CP_PACKET0(R300_ZB_ZCACHE_CTLSTAT, 0));
	OUT_RING(cache_z);
	ADVANCE_RING();
	
	BEGIN_RING(2);
	OUT_RING(CP_PACKET0(R300_RB3D_DSTCACHE_CTLSTAT, 0));
	OUT_RING(cache_3d);
	ADVANCE_RING();
	
	BEGIN_RING(2);
	OUT_RING(CP_PACKET0(R300_TX_INVALTAGS, 0));
	OUT_RING(0);
	ADVANCE_RING();
	
	BEGIN_RING(2);
	OUT_RING(CP_PACKET0(R300_RB3D_AARESOLVE_CTL, 0));
	OUT_RING(0);
	ADVANCE_RING();
	BEGIN_RING(2);
	OUT_RING(CP_PACKET0(RADEON_WAIT_UNTIL, 0));
	OUT_RING(RADEON_WAIT_3D_IDLECLEAN);
	ADVANCE_RING();
	
	BEGIN_RING(4);
	OUT_RING(CP_PACKET0(R300_DSTCACHE_CTLSTAT, 0));
	OUT_RING(cache_2d);
	OUT_RING(CP_PACKET0(RADEON_WAIT_UNTIL, 0));
	OUT_RING(RADEON_WAIT_2D_IDLECLEAN |
		 RADEON_WAIT_HOST_IDLECLEAN);
	ADVANCE_RING();
	
	dev_priv->track_flush |= RADEON_FLUSH_EMITED | RADEON_PURGE_EMITED;
}

static void r300_discard_buffer(struct drm_device *dev, struct drm_master *master, struct drm_buf *buf)
{
	drm_radeon_buf_priv_t *buf_priv = buf->dev_private;
	struct drm_radeon_master_private *master_priv = master->driver_priv;

	buf_priv->age = ++master_priv->sarea_priv->last_dispatch;
	buf->pending = 1;
	buf->used = 0;
}

static void r300_cmd_wait(drm_radeon_private_t * dev_priv,
			  drm_r300_cmd_header_t header)
{
	u32 wait_until;
	RING_LOCALS;

	if (!header.wait.flags)
		return;

	wait_until = 0;

	switch(header.wait.flags) {
	case R300_WAIT_2D:
		wait_until = RADEON_WAIT_2D_IDLE;
		break;
	case R300_WAIT_3D:
		wait_until = RADEON_WAIT_3D_IDLE;
		break;
	case R300_NEW_WAIT_2D_3D:
		wait_until = RADEON_WAIT_2D_IDLE|RADEON_WAIT_3D_IDLE;
		break;
	case R300_NEW_WAIT_2D_2D_CLEAN:
		wait_until = RADEON_WAIT_2D_IDLE|RADEON_WAIT_2D_IDLECLEAN;
		break;
	case R300_NEW_WAIT_3D_3D_CLEAN:
		wait_until = RADEON_WAIT_3D_IDLE|RADEON_WAIT_3D_IDLECLEAN;
		break;
	case R300_NEW_WAIT_2D_2D_CLEAN_3D_3D_CLEAN:
		wait_until = RADEON_WAIT_2D_IDLE|RADEON_WAIT_2D_IDLECLEAN;
		wait_until |= RADEON_WAIT_3D_IDLE|RADEON_WAIT_3D_IDLECLEAN;
		break;
	default:
		return;
	}

	BEGIN_RING(2);
	OUT_RING(CP_PACKET0(RADEON_WAIT_UNTIL, 0));
	OUT_RING(wait_until);
	ADVANCE_RING();
}

static int r300_scratch(drm_radeon_private_t *dev_priv,
			drm_radeon_kcmd_buffer_t *cmdbuf,
			drm_r300_cmd_header_t header)
{
	u32 *ref_age_base;
	u32 i, *buf_idx, h_pending;
	u64 *ptr_addr;
	u64 stack_ptr_addr;
	RING_LOCALS;

	if (drm_buffer_unprocessed(cmdbuf->buffer) <
	    (sizeof(u64) + header.scratch.n_bufs * sizeof(*buf_idx))) {
		return -EINVAL;
	}

	if (header.scratch.reg >= 5) {
		return -EINVAL;
	}

	dev_priv->scratch_ages[header.scratch.reg]++;

	ptr_addr = drm_buffer_read_object(cmdbuf->buffer,
			sizeof(stack_ptr_addr), &stack_ptr_addr);
	ref_age_base = (u32 *)(unsigned long)get_unaligned(ptr_addr);

	for (i=0; i < header.scratch.n_bufs; i++) {
		buf_idx = drm_buffer_pointer_to_dword(cmdbuf->buffer, 0);
		*buf_idx *= 2; 

		if (DRM_COPY_TO_USER(ref_age_base + *buf_idx,
				&dev_priv->scratch_ages[header.scratch.reg],
				sizeof(u32)))
			return -EINVAL;

		if (DRM_COPY_FROM_USER(&h_pending,
				ref_age_base + *buf_idx + 1,
				sizeof(u32)))
			return -EINVAL;

		if (h_pending == 0)
			return -EINVAL;

		h_pending--;

		if (DRM_COPY_TO_USER(ref_age_base + *buf_idx + 1,
					&h_pending,
					sizeof(u32)))
			return -EINVAL;

		drm_buffer_advance(cmdbuf->buffer, sizeof(*buf_idx));
	}

	BEGIN_RING(2);
	OUT_RING( CP_PACKET0( RADEON_SCRATCH_REG0 + header.scratch.reg * 4, 0 ) );
	OUT_RING( dev_priv->scratch_ages[header.scratch.reg] );
	ADVANCE_RING();

	return 0;
}

static inline int r300_emit_r500fp(drm_radeon_private_t *dev_priv,
				       drm_radeon_kcmd_buffer_t *cmdbuf,
				       drm_r300_cmd_header_t header)
{
	int sz;
	int addr;
	int type;
	int isclamp;
	int stride;
	RING_LOCALS;

	sz = header.r500fp.count;
	
	addr = ((header.r500fp.adrhi_flags & 1) << 8) | header.r500fp.adrlo;

	type = !!(header.r500fp.adrhi_flags & R500FP_CONSTANT_TYPE);
	isclamp = !!(header.r500fp.adrhi_flags & R500FP_CONSTANT_CLAMP);

	addr |= (type << 16);
	addr |= (isclamp << 17);

	stride = type ? 4 : 6;

	DRM_DEBUG("r500fp %d %d type: %d\n", sz, addr, type);
	if (!sz)
		return 0;
	if (sz * stride * 4 > drm_buffer_unprocessed(cmdbuf->buffer))
		return -EINVAL;

	BEGIN_RING(3 + sz * stride);
	OUT_RING_REG(R500_GA_US_VECTOR_INDEX, addr);
	OUT_RING(CP_PACKET0_TABLE(R500_GA_US_VECTOR_DATA, sz * stride - 1));
	OUT_RING_DRM_BUFFER(cmdbuf->buffer, sz * stride);

	ADVANCE_RING();

	return 0;
}


int r300_do_cp_cmdbuf(struct drm_device *dev,
		      struct drm_file *file_priv,
		      drm_radeon_kcmd_buffer_t *cmdbuf)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_radeon_master_private *master_priv = file_priv->master->driver_priv;
	struct drm_device_dma *dma = dev->dma;
	struct drm_buf *buf = NULL;
	int emit_dispatch_age = 0;
	int ret = 0;

	DRM_DEBUG("\n");

	
	r300_pacify(dev_priv);

	if (cmdbuf->nbox <= R300_SIMULTANEOUS_CLIPRECTS) {
		ret = r300_emit_cliprects(dev_priv, cmdbuf, 0);
		if (ret)
			goto cleanup;
	}

	while (drm_buffer_unprocessed(cmdbuf->buffer)
			>= sizeof(drm_r300_cmd_header_t)) {
		int idx;
		drm_r300_cmd_header_t *header, stack_header;

		header = drm_buffer_read_object(cmdbuf->buffer,
				sizeof(stack_header), &stack_header);

		switch (header->header.cmd_type) {
		case R300_CMD_PACKET0:
			DRM_DEBUG("R300_CMD_PACKET0\n");
			ret = r300_emit_packet0(dev_priv, cmdbuf, *header);
			if (ret) {
				DRM_ERROR("r300_emit_packet0 failed\n");
				goto cleanup;
			}
			break;

		case R300_CMD_VPU:
			DRM_DEBUG("R300_CMD_VPU\n");
			ret = r300_emit_vpu(dev_priv, cmdbuf, *header);
			if (ret) {
				DRM_ERROR("r300_emit_vpu failed\n");
				goto cleanup;
			}
			break;

		case R300_CMD_PACKET3:
			DRM_DEBUG("R300_CMD_PACKET3\n");
			ret = r300_emit_packet3(dev_priv, cmdbuf, *header);
			if (ret) {
				DRM_ERROR("r300_emit_packet3 failed\n");
				goto cleanup;
			}
			break;

		case R300_CMD_END3D:
			DRM_DEBUG("R300_CMD_END3D\n");
			r300_pacify(dev_priv);
			break;

		case R300_CMD_CP_DELAY:
			
			DRM_DEBUG("R300_CMD_CP_DELAY\n");
			{
				int i;
				RING_LOCALS;

				BEGIN_RING(header->delay.count);
				for (i = 0; i < header->delay.count; i++)
					OUT_RING(RADEON_CP_PACKET2);
				ADVANCE_RING();
			}
			break;

		case R300_CMD_DMA_DISCARD:
			DRM_DEBUG("RADEON_CMD_DMA_DISCARD\n");
			idx = header->dma.buf_idx;
			if (idx < 0 || idx >= dma->buf_count) {
				DRM_ERROR("buffer index %d (of %d max)\n",
					  idx, dma->buf_count - 1);
				ret = -EINVAL;
				goto cleanup;
			}

			buf = dma->buflist[idx];
			if (buf->file_priv != file_priv || buf->pending) {
				DRM_ERROR("bad buffer %p %p %d\n",
					  buf->file_priv, file_priv,
					  buf->pending);
				ret = -EINVAL;
				goto cleanup;
			}

			emit_dispatch_age = 1;
			r300_discard_buffer(dev, file_priv->master, buf);
			break;

		case R300_CMD_WAIT:
			DRM_DEBUG("R300_CMD_WAIT\n");
			r300_cmd_wait(dev_priv, *header);
			break;

		case R300_CMD_SCRATCH:
			DRM_DEBUG("R300_CMD_SCRATCH\n");
			ret = r300_scratch(dev_priv, cmdbuf, *header);
			if (ret) {
				DRM_ERROR("r300_scratch failed\n");
				goto cleanup;
			}
			break;

		case R300_CMD_R500FP:
			if ((dev_priv->flags & RADEON_FAMILY_MASK) < CHIP_RV515) {
				DRM_ERROR("Calling r500 command on r300 card\n");
				ret = -EINVAL;
				goto cleanup;
			}
			DRM_DEBUG("R300_CMD_R500FP\n");
			ret = r300_emit_r500fp(dev_priv, cmdbuf, *header);
			if (ret) {
				DRM_ERROR("r300_emit_r500fp failed\n");
				goto cleanup;
			}
			break;
		default:
			DRM_ERROR("bad cmd_type %i at byte %d\n",
				  header->header.cmd_type,
				  cmdbuf->buffer->iterator - (int)sizeof(*header));
			ret = -EINVAL;
			goto cleanup;
		}
	}

	DRM_DEBUG("END\n");

      cleanup:
	r300_pacify(dev_priv);

	/* We emit the vertex buffer age here, outside the pacifier "brackets"
	 * for two reasons:
	 *  (1) This may coalesce multiple age emissions into a single one and
	 *  (2) more importantly, some chips lock up hard when scratch registers
	 *      are written inside the pacifier bracket.
	 */
	if (emit_dispatch_age) {
		RING_LOCALS;

		
		BEGIN_RING(2);
		RADEON_DISPATCH_AGE(master_priv->sarea_priv->last_dispatch);
		ADVANCE_RING();
	}

	COMMIT_RING();

	return ret;
}
