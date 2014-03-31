/**************************************************************************
 *
 * Copyright © 2009 VMware, Inc., Palo Alto, CA., USA
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "vmwgfx_drv.h"
#include "vmwgfx_reg.h"
#include "ttm/ttm_bo_api.h"
#include "ttm/ttm_placement.h"

static int vmw_cmd_invalid(struct vmw_private *dev_priv,
			   struct vmw_sw_context *sw_context,
			   SVGA3dCmdHeader *header)
{
	return capable(CAP_SYS_ADMIN) ? : -EINVAL;
}

static int vmw_cmd_ok(struct vmw_private *dev_priv,
		      struct vmw_sw_context *sw_context,
		      SVGA3dCmdHeader *header)
{
	return 0;
}

static void vmw_resource_to_validate_list(struct vmw_sw_context *sw_context,
					  struct vmw_resource **p_res)
{
	struct vmw_resource *res = *p_res;

	if (list_empty(&res->validate_head)) {
		list_add_tail(&res->validate_head, &sw_context->resource_list);
		*p_res = NULL;
	} else
		vmw_resource_unreference(p_res);
}

static int vmw_bo_to_validate_list(struct vmw_sw_context *sw_context,
				   struct ttm_buffer_object *bo,
				   uint32_t fence_flags,
				   uint32_t *p_val_node)
{
	uint32_t val_node;
	struct ttm_validate_buffer *val_buf;

	val_node = vmw_dmabuf_validate_node(bo, sw_context->cur_val_buf);

	if (unlikely(val_node >= VMWGFX_MAX_VALIDATIONS)) {
		DRM_ERROR("Max number of DMA buffers per submission"
			  " exceeded.\n");
		return -EINVAL;
	}

	val_buf = &sw_context->val_bufs[val_node];
	if (unlikely(val_node == sw_context->cur_val_buf)) {
		val_buf->new_sync_obj_arg = NULL;
		val_buf->bo = ttm_bo_reference(bo);
		list_add_tail(&val_buf->head, &sw_context->validate_nodes);
		++sw_context->cur_val_buf;
	}

	val_buf->new_sync_obj_arg = (void *)
		((unsigned long) val_buf->new_sync_obj_arg | fence_flags);
	sw_context->fence_flags |= fence_flags;

	if (p_val_node)
		*p_val_node = val_node;

	return 0;
}

static int vmw_cmd_cid_check(struct vmw_private *dev_priv,
			     struct vmw_sw_context *sw_context,
			     SVGA3dCmdHeader *header)
{
	struct vmw_resource *ctx;

	struct vmw_cid_cmd {
		SVGA3dCmdHeader header;
		__le32 cid;
	} *cmd;
	int ret;

	cmd = container_of(header, struct vmw_cid_cmd, header);
	if (likely(sw_context->cid_valid && cmd->cid == sw_context->last_cid))
		return 0;

	ret = vmw_context_check(dev_priv, sw_context->tfile, cmd->cid,
				&ctx);
	if (unlikely(ret != 0)) {
		DRM_ERROR("Could not find or use context %u\n",
			  (unsigned) cmd->cid);
		return ret;
	}

	sw_context->last_cid = cmd->cid;
	sw_context->cid_valid = true;
	sw_context->cur_ctx = ctx;
	vmw_resource_to_validate_list(sw_context, &ctx);

	return 0;
}

static int vmw_cmd_sid_check(struct vmw_private *dev_priv,
			     struct vmw_sw_context *sw_context,
			     uint32_t *sid)
{
	struct vmw_surface *srf;
	int ret;
	struct vmw_resource *res;

	if (*sid == SVGA3D_INVALID_ID)
		return 0;

	if (likely((sw_context->sid_valid  &&
		      *sid == sw_context->last_sid))) {
		*sid = sw_context->sid_translation;
		return 0;
	}

	ret = vmw_user_surface_lookup_handle(dev_priv,
					     sw_context->tfile,
					     *sid, &srf);
	if (unlikely(ret != 0)) {
		DRM_ERROR("Could ot find or use surface 0x%08x "
			  "address 0x%08lx\n",
			  (unsigned int) *sid,
			  (unsigned long) sid);
		return ret;
	}

	ret = vmw_surface_validate(dev_priv, srf);
	if (unlikely(ret != 0)) {
		if (ret != -ERESTARTSYS)
			DRM_ERROR("Could not validate surface.\n");
		vmw_surface_unreference(&srf);
		return ret;
	}

	sw_context->last_sid = *sid;
	sw_context->sid_valid = true;
	sw_context->sid_translation = srf->res.id;
	*sid = sw_context->sid_translation;

	res = &srf->res;
	vmw_resource_to_validate_list(sw_context, &res);

	return 0;
}


static int vmw_cmd_set_render_target_check(struct vmw_private *dev_priv,
					   struct vmw_sw_context *sw_context,
					   SVGA3dCmdHeader *header)
{
	struct vmw_sid_cmd {
		SVGA3dCmdHeader header;
		SVGA3dCmdSetRenderTarget body;
	} *cmd;
	int ret;

	ret = vmw_cmd_cid_check(dev_priv, sw_context, header);
	if (unlikely(ret != 0))
		return ret;

	cmd = container_of(header, struct vmw_sid_cmd, header);
	ret = vmw_cmd_sid_check(dev_priv, sw_context, &cmd->body.target.sid);
	return ret;
}

static int vmw_cmd_surface_copy_check(struct vmw_private *dev_priv,
				      struct vmw_sw_context *sw_context,
				      SVGA3dCmdHeader *header)
{
	struct vmw_sid_cmd {
		SVGA3dCmdHeader header;
		SVGA3dCmdSurfaceCopy body;
	} *cmd;
	int ret;

	cmd = container_of(header, struct vmw_sid_cmd, header);
	ret = vmw_cmd_sid_check(dev_priv, sw_context, &cmd->body.src.sid);
	if (unlikely(ret != 0))
		return ret;
	return vmw_cmd_sid_check(dev_priv, sw_context, &cmd->body.dest.sid);
}

static int vmw_cmd_stretch_blt_check(struct vmw_private *dev_priv,
				     struct vmw_sw_context *sw_context,
				     SVGA3dCmdHeader *header)
{
	struct vmw_sid_cmd {
		SVGA3dCmdHeader header;
		SVGA3dCmdSurfaceStretchBlt body;
	} *cmd;
	int ret;

	cmd = container_of(header, struct vmw_sid_cmd, header);
	ret = vmw_cmd_sid_check(dev_priv, sw_context, &cmd->body.src.sid);
	if (unlikely(ret != 0))
		return ret;
	return vmw_cmd_sid_check(dev_priv, sw_context, &cmd->body.dest.sid);
}

static int vmw_cmd_blt_surf_screen_check(struct vmw_private *dev_priv,
					 struct vmw_sw_context *sw_context,
					 SVGA3dCmdHeader *header)
{
	struct vmw_sid_cmd {
		SVGA3dCmdHeader header;
		SVGA3dCmdBlitSurfaceToScreen body;
	} *cmd;

	cmd = container_of(header, struct vmw_sid_cmd, header);

	if (unlikely(!sw_context->kernel)) {
		DRM_ERROR("Kernel only SVGA3d command: %u.\n", cmd->header.id);
		return -EPERM;
	}

	return vmw_cmd_sid_check(dev_priv, sw_context, &cmd->body.srcImage.sid);
}

static int vmw_cmd_present_check(struct vmw_private *dev_priv,
				 struct vmw_sw_context *sw_context,
				 SVGA3dCmdHeader *header)
{
	struct vmw_sid_cmd {
		SVGA3dCmdHeader header;
		SVGA3dCmdPresent body;
	} *cmd;


	cmd = container_of(header, struct vmw_sid_cmd, header);

	if (unlikely(!sw_context->kernel)) {
		DRM_ERROR("Kernel only SVGA3d command: %u.\n", cmd->header.id);
		return -EPERM;
	}

	return vmw_cmd_sid_check(dev_priv, sw_context, &cmd->body.sid);
}

static int vmw_query_bo_switch_prepare(struct vmw_private *dev_priv,
				       uint32_t cid,
				       struct ttm_buffer_object *new_query_bo,
				       struct vmw_sw_context *sw_context)
{
	int ret;
	bool add_cid = false;
	uint32_t cid_to_add;

	if (unlikely(new_query_bo != sw_context->cur_query_bo)) {

		if (unlikely(new_query_bo->num_pages > 4)) {
			DRM_ERROR("Query buffer too large.\n");
			return -EINVAL;
		}

		if (unlikely(sw_context->cur_query_bo != NULL)) {
			BUG_ON(!sw_context->query_cid_valid);
			add_cid = true;
			cid_to_add = sw_context->cur_query_cid;
			ret = vmw_bo_to_validate_list(sw_context,
						      sw_context->cur_query_bo,
						      DRM_VMW_FENCE_FLAG_EXEC,
						      NULL);
			if (unlikely(ret != 0))
				return ret;
		}
		sw_context->cur_query_bo = new_query_bo;

		ret = vmw_bo_to_validate_list(sw_context,
					      dev_priv->dummy_query_bo,
					      DRM_VMW_FENCE_FLAG_EXEC,
					      NULL);
		if (unlikely(ret != 0))
			return ret;

	}

	if (unlikely(cid != sw_context->cur_query_cid &&
		     sw_context->query_cid_valid)) {
		add_cid = true;
		cid_to_add = sw_context->cur_query_cid;
	}

	sw_context->cur_query_cid = cid;
	sw_context->query_cid_valid = true;

	if (add_cid) {
		struct vmw_resource *ctx = sw_context->cur_ctx;

		if (list_empty(&ctx->query_head))
			list_add_tail(&ctx->query_head,
				      &sw_context->query_list);
		ret = vmw_bo_to_validate_list(sw_context,
					      dev_priv->dummy_query_bo,
					      DRM_VMW_FENCE_FLAG_EXEC,
					      NULL);
		if (unlikely(ret != 0))
			return ret;
	}
	return 0;
}


static void vmw_query_bo_switch_commit(struct vmw_private *dev_priv,
				     struct vmw_sw_context *sw_context)
{

	struct vmw_resource *ctx, *next_ctx;
	int ret;


	list_for_each_entry_safe(ctx, next_ctx, &sw_context->query_list,
				 query_head) {
		list_del_init(&ctx->query_head);

		BUG_ON(list_empty(&ctx->validate_head));

		ret = vmw_fifo_emit_dummy_query(dev_priv, ctx->id);

		if (unlikely(ret != 0))
			DRM_ERROR("Out of fifo space for dummy query.\n");
	}

	if (dev_priv->pinned_bo != sw_context->cur_query_bo) {
		if (dev_priv->pinned_bo) {
			vmw_bo_pin(dev_priv->pinned_bo, false);
			ttm_bo_unref(&dev_priv->pinned_bo);
		}

		vmw_bo_pin(sw_context->cur_query_bo, true);


		vmw_bo_pin(dev_priv->dummy_query_bo, true);
		dev_priv->dummy_query_bo_pinned = true;

		dev_priv->query_cid = sw_context->cur_query_cid;
		dev_priv->pinned_bo =
			ttm_bo_reference(sw_context->cur_query_bo);
	}
}

static void vmw_query_switch_backoff(struct vmw_sw_context *sw_context)
{
	struct list_head *list, *next;

	list_for_each_safe(list, next, &sw_context->query_list) {
		list_del_init(list);
	}
}

static int vmw_translate_guest_ptr(struct vmw_private *dev_priv,
				   struct vmw_sw_context *sw_context,
				   SVGAGuestPtr *ptr,
				   struct vmw_dma_buffer **vmw_bo_p)
{
	struct vmw_dma_buffer *vmw_bo = NULL;
	struct ttm_buffer_object *bo;
	uint32_t handle = ptr->gmrId;
	struct vmw_relocation *reloc;
	int ret;

	ret = vmw_user_dmabuf_lookup(sw_context->tfile, handle, &vmw_bo);
	if (unlikely(ret != 0)) {
		DRM_ERROR("Could not find or use GMR region.\n");
		return -EINVAL;
	}
	bo = &vmw_bo->base;

	if (unlikely(sw_context->cur_reloc >= VMWGFX_MAX_RELOCATIONS)) {
		DRM_ERROR("Max number relocations per submission"
			  " exceeded\n");
		ret = -EINVAL;
		goto out_no_reloc;
	}

	reloc = &sw_context->relocs[sw_context->cur_reloc++];
	reloc->location = ptr;

	ret = vmw_bo_to_validate_list(sw_context, bo, DRM_VMW_FENCE_FLAG_EXEC,
				      &reloc->index);
	if (unlikely(ret != 0))
		goto out_no_reloc;

	*vmw_bo_p = vmw_bo;
	return 0;

out_no_reloc:
	vmw_dmabuf_unreference(&vmw_bo);
	vmw_bo_p = NULL;
	return ret;
}

static int vmw_cmd_end_query(struct vmw_private *dev_priv,
			     struct vmw_sw_context *sw_context,
			     SVGA3dCmdHeader *header)
{
	struct vmw_dma_buffer *vmw_bo;
	struct vmw_query_cmd {
		SVGA3dCmdHeader header;
		SVGA3dCmdEndQuery q;
	} *cmd;
	int ret;

	cmd = container_of(header, struct vmw_query_cmd, header);
	ret = vmw_cmd_cid_check(dev_priv, sw_context, header);
	if (unlikely(ret != 0))
		return ret;

	ret = vmw_translate_guest_ptr(dev_priv, sw_context,
				      &cmd->q.guestResult,
				      &vmw_bo);
	if (unlikely(ret != 0))
		return ret;

	ret = vmw_query_bo_switch_prepare(dev_priv, cmd->q.cid,
					  &vmw_bo->base, sw_context);

	vmw_dmabuf_unreference(&vmw_bo);
	return ret;
}

static int vmw_cmd_wait_query(struct vmw_private *dev_priv,
			      struct vmw_sw_context *sw_context,
			      SVGA3dCmdHeader *header)
{
	struct vmw_dma_buffer *vmw_bo;
	struct vmw_query_cmd {
		SVGA3dCmdHeader header;
		SVGA3dCmdWaitForQuery q;
	} *cmd;
	int ret;
	struct vmw_resource *ctx;

	cmd = container_of(header, struct vmw_query_cmd, header);
	ret = vmw_cmd_cid_check(dev_priv, sw_context, header);
	if (unlikely(ret != 0))
		return ret;

	ret = vmw_translate_guest_ptr(dev_priv, sw_context,
				      &cmd->q.guestResult,
				      &vmw_bo);
	if (unlikely(ret != 0))
		return ret;

	vmw_dmabuf_unreference(&vmw_bo);


	ctx = sw_context->cur_ctx;
	if (!list_empty(&ctx->query_head))
		list_del_init(&ctx->query_head);

	return 0;
}

static int vmw_cmd_dma(struct vmw_private *dev_priv,
		       struct vmw_sw_context *sw_context,
		       SVGA3dCmdHeader *header)
{
	struct vmw_dma_buffer *vmw_bo = NULL;
	struct ttm_buffer_object *bo;
	struct vmw_surface *srf = NULL;
	struct vmw_dma_cmd {
		SVGA3dCmdHeader header;
		SVGA3dCmdSurfaceDMA dma;
	} *cmd;
	int ret;
	struct vmw_resource *res;

	cmd = container_of(header, struct vmw_dma_cmd, header);
	ret = vmw_translate_guest_ptr(dev_priv, sw_context,
				      &cmd->dma.guest.ptr,
				      &vmw_bo);
	if (unlikely(ret != 0))
		return ret;

	bo = &vmw_bo->base;
	ret = vmw_user_surface_lookup_handle(dev_priv, sw_context->tfile,
					     cmd->dma.host.sid, &srf);
	if (ret) {
		DRM_ERROR("could not find surface\n");
		goto out_no_reloc;
	}

	ret = vmw_surface_validate(dev_priv, srf);
	if (unlikely(ret != 0)) {
		if (ret != -ERESTARTSYS)
			DRM_ERROR("Culd not validate surface.\n");
		goto out_no_validate;
	}

	cmd->dma.host.sid = srf->res.id;
	vmw_kms_cursor_snoop(srf, sw_context->tfile, bo, header);

	vmw_dmabuf_unreference(&vmw_bo);

	res = &srf->res;
	vmw_resource_to_validate_list(sw_context, &res);

	return 0;

out_no_validate:
	vmw_surface_unreference(&srf);
out_no_reloc:
	vmw_dmabuf_unreference(&vmw_bo);
	return ret;
}

static int vmw_cmd_draw(struct vmw_private *dev_priv,
			struct vmw_sw_context *sw_context,
			SVGA3dCmdHeader *header)
{
	struct vmw_draw_cmd {
		SVGA3dCmdHeader header;
		SVGA3dCmdDrawPrimitives body;
	} *cmd;
	SVGA3dVertexDecl *decl = (SVGA3dVertexDecl *)(
		(unsigned long)header + sizeof(*cmd));
	SVGA3dPrimitiveRange *range;
	uint32_t i;
	uint32_t maxnum;
	int ret;

	ret = vmw_cmd_cid_check(dev_priv, sw_context, header);
	if (unlikely(ret != 0))
		return ret;

	cmd = container_of(header, struct vmw_draw_cmd, header);
	maxnum = (header->size - sizeof(cmd->body)) / sizeof(*decl);

	if (unlikely(cmd->body.numVertexDecls > maxnum)) {
		DRM_ERROR("Illegal number of vertex declarations.\n");
		return -EINVAL;
	}

	for (i = 0; i < cmd->body.numVertexDecls; ++i, ++decl) {
		ret = vmw_cmd_sid_check(dev_priv, sw_context,
					&decl->array.surfaceId);
		if (unlikely(ret != 0))
			return ret;
	}

	maxnum = (header->size - sizeof(cmd->body) -
		  cmd->body.numVertexDecls * sizeof(*decl)) / sizeof(*range);
	if (unlikely(cmd->body.numRanges > maxnum)) {
		DRM_ERROR("Illegal number of index ranges.\n");
		return -EINVAL;
	}

	range = (SVGA3dPrimitiveRange *) decl;
	for (i = 0; i < cmd->body.numRanges; ++i, ++range) {
		ret = vmw_cmd_sid_check(dev_priv, sw_context,
					&range->indexArray.surfaceId);
		if (unlikely(ret != 0))
			return ret;
	}
	return 0;
}


static int vmw_cmd_tex_state(struct vmw_private *dev_priv,
			     struct vmw_sw_context *sw_context,
			     SVGA3dCmdHeader *header)
{
	struct vmw_tex_state_cmd {
		SVGA3dCmdHeader header;
		SVGA3dCmdSetTextureState state;
	};

	SVGA3dTextureState *last_state = (SVGA3dTextureState *)
	  ((unsigned long) header + header->size + sizeof(header));
	SVGA3dTextureState *cur_state = (SVGA3dTextureState *)
		((unsigned long) header + sizeof(struct vmw_tex_state_cmd));
	int ret;

	ret = vmw_cmd_cid_check(dev_priv, sw_context, header);
	if (unlikely(ret != 0))
		return ret;

	for (; cur_state < last_state; ++cur_state) {
		if (likely(cur_state->name != SVGA3D_TS_BIND_TEXTURE))
			continue;

		ret = vmw_cmd_sid_check(dev_priv, sw_context,
					&cur_state->value);
		if (unlikely(ret != 0))
			return ret;
	}

	return 0;
}

static int vmw_cmd_check_define_gmrfb(struct vmw_private *dev_priv,
				      struct vmw_sw_context *sw_context,
				      void *buf)
{
	struct vmw_dma_buffer *vmw_bo;
	int ret;

	struct {
		uint32_t header;
		SVGAFifoCmdDefineGMRFB body;
	} *cmd = buf;

	ret = vmw_translate_guest_ptr(dev_priv, sw_context,
				      &cmd->body.ptr,
				      &vmw_bo);
	if (unlikely(ret != 0))
		return ret;

	vmw_dmabuf_unreference(&vmw_bo);

	return ret;
}

static int vmw_cmd_check_not_3d(struct vmw_private *dev_priv,
				struct vmw_sw_context *sw_context,
				void *buf, uint32_t *size)
{
	uint32_t size_remaining = *size;
	uint32_t cmd_id;

	cmd_id = le32_to_cpu(((uint32_t *)buf)[0]);
	switch (cmd_id) {
	case SVGA_CMD_UPDATE:
		*size = sizeof(uint32_t) + sizeof(SVGAFifoCmdUpdate);
		break;
	case SVGA_CMD_DEFINE_GMRFB:
		*size = sizeof(uint32_t) + sizeof(SVGAFifoCmdDefineGMRFB);
		break;
	case SVGA_CMD_BLIT_GMRFB_TO_SCREEN:
		*size = sizeof(uint32_t) + sizeof(SVGAFifoCmdBlitGMRFBToScreen);
		break;
	case SVGA_CMD_BLIT_SCREEN_TO_GMRFB:
		*size = sizeof(uint32_t) + sizeof(SVGAFifoCmdBlitGMRFBToScreen);
		break;
	default:
		DRM_ERROR("Unsupported SVGA command: %u.\n", cmd_id);
		return -EINVAL;
	}

	if (*size > size_remaining) {
		DRM_ERROR("Invalid SVGA command (size mismatch):"
			  " %u.\n", cmd_id);
		return -EINVAL;
	}

	if (unlikely(!sw_context->kernel)) {
		DRM_ERROR("Kernel only SVGA command: %u.\n", cmd_id);
		return -EPERM;
	}

	if (cmd_id == SVGA_CMD_DEFINE_GMRFB)
		return vmw_cmd_check_define_gmrfb(dev_priv, sw_context, buf);

	return 0;
}

typedef int (*vmw_cmd_func) (struct vmw_private *,
			     struct vmw_sw_context *,
			     SVGA3dCmdHeader *);

#define VMW_CMD_DEF(cmd, func) \
	[cmd - SVGA_3D_CMD_BASE] = func

static vmw_cmd_func vmw_cmd_funcs[SVGA_3D_CMD_MAX] = {
	VMW_CMD_DEF(SVGA_3D_CMD_SURFACE_DEFINE, &vmw_cmd_invalid),
	VMW_CMD_DEF(SVGA_3D_CMD_SURFACE_DESTROY, &vmw_cmd_invalid),
	VMW_CMD_DEF(SVGA_3D_CMD_SURFACE_COPY, &vmw_cmd_surface_copy_check),
	VMW_CMD_DEF(SVGA_3D_CMD_SURFACE_STRETCHBLT, &vmw_cmd_stretch_blt_check),
	VMW_CMD_DEF(SVGA_3D_CMD_SURFACE_DMA, &vmw_cmd_dma),
	VMW_CMD_DEF(SVGA_3D_CMD_CONTEXT_DEFINE, &vmw_cmd_invalid),
	VMW_CMD_DEF(SVGA_3D_CMD_CONTEXT_DESTROY, &vmw_cmd_invalid),
	VMW_CMD_DEF(SVGA_3D_CMD_SETTRANSFORM, &vmw_cmd_cid_check),
	VMW_CMD_DEF(SVGA_3D_CMD_SETZRANGE, &vmw_cmd_cid_check),
	VMW_CMD_DEF(SVGA_3D_CMD_SETRENDERSTATE, &vmw_cmd_cid_check),
	VMW_CMD_DEF(SVGA_3D_CMD_SETRENDERTARGET,
		    &vmw_cmd_set_render_target_check),
	VMW_CMD_DEF(SVGA_3D_CMD_SETTEXTURESTATE, &vmw_cmd_tex_state),
	VMW_CMD_DEF(SVGA_3D_CMD_SETMATERIAL, &vmw_cmd_cid_check),
	VMW_CMD_DEF(SVGA_3D_CMD_SETLIGHTDATA, &vmw_cmd_cid_check),
	VMW_CMD_DEF(SVGA_3D_CMD_SETLIGHTENABLED, &vmw_cmd_cid_check),
	VMW_CMD_DEF(SVGA_3D_CMD_SETVIEWPORT, &vmw_cmd_cid_check),
	VMW_CMD_DEF(SVGA_3D_CMD_SETCLIPPLANE, &vmw_cmd_cid_check),
	VMW_CMD_DEF(SVGA_3D_CMD_CLEAR, &vmw_cmd_cid_check),
	VMW_CMD_DEF(SVGA_3D_CMD_PRESENT, &vmw_cmd_present_check),
	VMW_CMD_DEF(SVGA_3D_CMD_SHADER_DEFINE, &vmw_cmd_cid_check),
	VMW_CMD_DEF(SVGA_3D_CMD_SHADER_DESTROY, &vmw_cmd_cid_check),
	VMW_CMD_DEF(SVGA_3D_CMD_SET_SHADER, &vmw_cmd_cid_check),
	VMW_CMD_DEF(SVGA_3D_CMD_SET_SHADER_CONST, &vmw_cmd_cid_check),
	VMW_CMD_DEF(SVGA_3D_CMD_DRAW_PRIMITIVES, &vmw_cmd_draw),
	VMW_CMD_DEF(SVGA_3D_CMD_SETSCISSORRECT, &vmw_cmd_cid_check),
	VMW_CMD_DEF(SVGA_3D_CMD_BEGIN_QUERY, &vmw_cmd_cid_check),
	VMW_CMD_DEF(SVGA_3D_CMD_END_QUERY, &vmw_cmd_end_query),
	VMW_CMD_DEF(SVGA_3D_CMD_WAIT_FOR_QUERY, &vmw_cmd_wait_query),
	VMW_CMD_DEF(SVGA_3D_CMD_PRESENT_READBACK, &vmw_cmd_ok),
	VMW_CMD_DEF(SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN,
		    &vmw_cmd_blt_surf_screen_check)
};

static int vmw_cmd_check(struct vmw_private *dev_priv,
			 struct vmw_sw_context *sw_context,
			 void *buf, uint32_t *size)
{
	uint32_t cmd_id;
	uint32_t size_remaining = *size;
	SVGA3dCmdHeader *header = (SVGA3dCmdHeader *) buf;
	int ret;

	cmd_id = le32_to_cpu(((uint32_t *)buf)[0]);
	
	if (unlikely(cmd_id < SVGA_CMD_MAX))
		return vmw_cmd_check_not_3d(dev_priv, sw_context, buf, size);


	cmd_id = le32_to_cpu(header->id);
	*size = le32_to_cpu(header->size) + sizeof(SVGA3dCmdHeader);

	cmd_id -= SVGA_3D_CMD_BASE;
	if (unlikely(*size > size_remaining))
		goto out_err;

	if (unlikely(cmd_id >= SVGA_3D_CMD_MAX - SVGA_3D_CMD_BASE))
		goto out_err;

	ret = vmw_cmd_funcs[cmd_id](dev_priv, sw_context, header);
	if (unlikely(ret != 0))
		goto out_err;

	return 0;
out_err:
	DRM_ERROR("Illegal / Invalid SVGA3D command: %d\n",
		  cmd_id + SVGA_3D_CMD_BASE);
	return -EINVAL;
}

static int vmw_cmd_check_all(struct vmw_private *dev_priv,
			     struct vmw_sw_context *sw_context,
			     void *buf,
			     uint32_t size)
{
	int32_t cur_size = size;
	int ret;

	while (cur_size > 0) {
		size = cur_size;
		ret = vmw_cmd_check(dev_priv, sw_context, buf, &size);
		if (unlikely(ret != 0))
			return ret;
		buf = (void *)((unsigned long) buf + size);
		cur_size -= size;
	}

	if (unlikely(cur_size != 0)) {
		DRM_ERROR("Command verifier out of sync.\n");
		return -EINVAL;
	}

	return 0;
}

static void vmw_free_relocations(struct vmw_sw_context *sw_context)
{
	sw_context->cur_reloc = 0;
}

static void vmw_apply_relocations(struct vmw_sw_context *sw_context)
{
	uint32_t i;
	struct vmw_relocation *reloc;
	struct ttm_validate_buffer *validate;
	struct ttm_buffer_object *bo;

	for (i = 0; i < sw_context->cur_reloc; ++i) {
		reloc = &sw_context->relocs[i];
		validate = &sw_context->val_bufs[reloc->index];
		bo = validate->bo;
		if (bo->mem.mem_type == TTM_PL_VRAM) {
			reloc->location->offset += bo->offset;
			reloc->location->gmrId = SVGA_GMR_FRAMEBUFFER;
		} else
			reloc->location->gmrId = bo->mem.start;
	}
	vmw_free_relocations(sw_context);
}

static void vmw_clear_validations(struct vmw_sw_context *sw_context)
{
	struct ttm_validate_buffer *entry, *next;
	struct vmw_resource *res, *res_next;

	list_for_each_entry_safe(entry, next, &sw_context->validate_nodes,
				 head) {
		list_del(&entry->head);
		vmw_dmabuf_validate_clear(entry->bo);
		ttm_bo_unref(&entry->bo);
		sw_context->cur_val_buf--;
	}
	BUG_ON(sw_context->cur_val_buf != 0);

	vmw_resource_unreserve(&sw_context->resource_list);
	list_for_each_entry_safe(res, res_next, &sw_context->resource_list,
				 validate_head) {
		list_del_init(&res->validate_head);
		vmw_resource_unreference(&res);
	}
}

static int vmw_validate_single_buffer(struct vmw_private *dev_priv,
				      struct ttm_buffer_object *bo)
{
	int ret;



	if (bo == dev_priv->pinned_bo ||
	    (bo == dev_priv->dummy_query_bo &&
	     dev_priv->dummy_query_bo_pinned))
		return 0;


	ret = ttm_bo_validate(bo, &vmw_vram_gmr_placement, true, false, false);
	if (likely(ret == 0 || ret == -ERESTARTSYS))
		return ret;


	DRM_INFO("Falling through to VRAM.\n");
	ret = ttm_bo_validate(bo, &vmw_vram_placement, true, false, false);
	return ret;
}


static int vmw_validate_buffers(struct vmw_private *dev_priv,
				struct vmw_sw_context *sw_context)
{
	struct ttm_validate_buffer *entry;
	int ret;

	list_for_each_entry(entry, &sw_context->validate_nodes, head) {
		ret = vmw_validate_single_buffer(dev_priv, entry->bo);
		if (unlikely(ret != 0))
			return ret;
	}
	return 0;
}

static int vmw_resize_cmd_bounce(struct vmw_sw_context *sw_context,
				 uint32_t size)
{
	if (likely(sw_context->cmd_bounce_size >= size))
		return 0;

	if (sw_context->cmd_bounce_size == 0)
		sw_context->cmd_bounce_size = VMWGFX_CMD_BOUNCE_INIT_SIZE;

	while (sw_context->cmd_bounce_size < size) {
		sw_context->cmd_bounce_size =
			PAGE_ALIGN(sw_context->cmd_bounce_size +
				   (sw_context->cmd_bounce_size >> 1));
	}

	if (sw_context->cmd_bounce != NULL)
		vfree(sw_context->cmd_bounce);

	sw_context->cmd_bounce = vmalloc(sw_context->cmd_bounce_size);

	if (sw_context->cmd_bounce == NULL) {
		DRM_ERROR("Failed to allocate command bounce buffer.\n");
		sw_context->cmd_bounce_size = 0;
		return -ENOMEM;
	}

	return 0;
}


int vmw_execbuf_fence_commands(struct drm_file *file_priv,
			       struct vmw_private *dev_priv,
			       struct vmw_fence_obj **p_fence,
			       uint32_t *p_handle)
{
	uint32_t sequence;
	int ret;
	bool synced = false;

	
	BUG_ON(p_handle != NULL && file_priv == NULL);

	ret = vmw_fifo_send_fence(dev_priv, &sequence);
	if (unlikely(ret != 0)) {
		DRM_ERROR("Fence submission error. Syncing.\n");
		synced = true;
	}

	if (p_handle != NULL)
		ret = vmw_user_fence_create(file_priv, dev_priv->fman,
					    sequence,
					    DRM_VMW_FENCE_FLAG_EXEC,
					    p_fence, p_handle);
	else
		ret = vmw_fence_create(dev_priv->fman, sequence,
				       DRM_VMW_FENCE_FLAG_EXEC,
				       p_fence);

	if (unlikely(ret != 0 && !synced)) {
		(void) vmw_fallback_wait(dev_priv, false, false,
					 sequence, false,
					 VMW_FENCE_WAIT_TIMEOUT);
		*p_fence = NULL;
	}

	return 0;
}

void
vmw_execbuf_copy_fence_user(struct vmw_private *dev_priv,
			    struct vmw_fpriv *vmw_fp,
			    int ret,
			    struct drm_vmw_fence_rep __user *user_fence_rep,
			    struct vmw_fence_obj *fence,
			    uint32_t fence_handle)
{
	struct drm_vmw_fence_rep fence_rep;

	if (user_fence_rep == NULL)
		return;

	memset(&fence_rep, 0, sizeof(fence_rep));

	fence_rep.error = ret;
	if (ret == 0) {
		BUG_ON(fence == NULL);

		fence_rep.handle = fence_handle;
		fence_rep.seqno = fence->seqno;
		vmw_update_seqno(dev_priv, &dev_priv->fifo);
		fence_rep.passed_seqno = dev_priv->last_read_seqno;
	}

	ret = copy_to_user(user_fence_rep, &fence_rep,
			   sizeof(fence_rep));

	if (unlikely(ret != 0) && (fence_rep.error == 0)) {
		ttm_ref_object_base_unref(vmw_fp->tfile,
					  fence_handle, TTM_REF_USAGE);
		DRM_ERROR("Fence copy error. Syncing.\n");
		(void) vmw_fence_obj_wait(fence, fence->signal_mask,
					  false, false,
					  VMW_FENCE_WAIT_TIMEOUT);
	}
}

int vmw_execbuf_process(struct drm_file *file_priv,
			struct vmw_private *dev_priv,
			void __user *user_commands,
			void *kernel_commands,
			uint32_t command_size,
			uint64_t throttle_us,
			struct drm_vmw_fence_rep __user *user_fence_rep,
			struct vmw_fence_obj **out_fence)
{
	struct vmw_sw_context *sw_context = &dev_priv->ctx;
	struct vmw_fence_obj *fence = NULL;
	uint32_t handle;
	void *cmd;
	int ret;

	ret = mutex_lock_interruptible(&dev_priv->cmdbuf_mutex);
	if (unlikely(ret != 0))
		return -ERESTARTSYS;

	if (kernel_commands == NULL) {
		sw_context->kernel = false;

		ret = vmw_resize_cmd_bounce(sw_context, command_size);
		if (unlikely(ret != 0))
			goto out_unlock;


		ret = copy_from_user(sw_context->cmd_bounce,
				     user_commands, command_size);

		if (unlikely(ret != 0)) {
			ret = -EFAULT;
			DRM_ERROR("Failed copying commands.\n");
			goto out_unlock;
		}
		kernel_commands = sw_context->cmd_bounce;
	} else
		sw_context->kernel = true;

	sw_context->tfile = vmw_fpriv(file_priv)->tfile;
	sw_context->cid_valid = false;
	sw_context->sid_valid = false;
	sw_context->cur_reloc = 0;
	sw_context->cur_val_buf = 0;
	sw_context->fence_flags = 0;
	INIT_LIST_HEAD(&sw_context->query_list);
	INIT_LIST_HEAD(&sw_context->resource_list);
	sw_context->cur_query_bo = dev_priv->pinned_bo;
	sw_context->cur_query_cid = dev_priv->query_cid;
	sw_context->query_cid_valid = (dev_priv->pinned_bo != NULL);

	INIT_LIST_HEAD(&sw_context->validate_nodes);

	ret = vmw_cmd_check_all(dev_priv, sw_context, kernel_commands,
				command_size);
	if (unlikely(ret != 0))
		goto out_err;

	ret = ttm_eu_reserve_buffers(&sw_context->validate_nodes);
	if (unlikely(ret != 0))
		goto out_err;

	ret = vmw_validate_buffers(dev_priv, sw_context);
	if (unlikely(ret != 0))
		goto out_err;

	vmw_apply_relocations(sw_context);

	if (throttle_us) {
		ret = vmw_wait_lag(dev_priv, &dev_priv->fifo.marker_queue,
				   throttle_us);

		if (unlikely(ret != 0))
			goto out_throttle;
	}

	cmd = vmw_fifo_reserve(dev_priv, command_size);
	if (unlikely(cmd == NULL)) {
		DRM_ERROR("Failed reserving fifo space for commands.\n");
		ret = -ENOMEM;
		goto out_throttle;
	}

	memcpy(cmd, kernel_commands, command_size);
	vmw_fifo_commit(dev_priv, command_size);

	vmw_query_bo_switch_commit(dev_priv, sw_context);
	ret = vmw_execbuf_fence_commands(file_priv, dev_priv,
					 &fence,
					 (user_fence_rep) ? &handle : NULL);

	if (ret != 0)
		DRM_ERROR("Fence submission error. Syncing.\n");

	ttm_eu_fence_buffer_objects(&sw_context->validate_nodes,
				    (void *) fence);

	vmw_clear_validations(sw_context);
	vmw_execbuf_copy_fence_user(dev_priv, vmw_fpriv(file_priv), ret,
				    user_fence_rep, fence, handle);

	
	if (unlikely(out_fence != NULL)) {
		*out_fence = fence;
		fence = NULL;
	} else if (likely(fence != NULL)) {
		vmw_fence_obj_unreference(&fence);
	}

	mutex_unlock(&dev_priv->cmdbuf_mutex);
	return 0;

out_err:
	vmw_free_relocations(sw_context);
out_throttle:
	vmw_query_switch_backoff(sw_context);
	ttm_eu_backoff_reservation(&sw_context->validate_nodes);
	vmw_clear_validations(sw_context);
out_unlock:
	mutex_unlock(&dev_priv->cmdbuf_mutex);
	return ret;
}

static void vmw_execbuf_unpin_panic(struct vmw_private *dev_priv)
{
	DRM_ERROR("Can't unpin query buffer. Trying to recover.\n");

	(void) vmw_fallback_wait(dev_priv, false, true, 0, false, 10*HZ);
	vmw_bo_pin(dev_priv->pinned_bo, false);
	vmw_bo_pin(dev_priv->dummy_query_bo, false);
	dev_priv->dummy_query_bo_pinned = false;
}


void vmw_execbuf_release_pinned_bo(struct vmw_private *dev_priv,
				   bool only_on_cid_match, uint32_t cid)
{
	int ret = 0;
	struct list_head validate_list;
	struct ttm_validate_buffer pinned_val, query_val;
	struct vmw_fence_obj *fence;

	mutex_lock(&dev_priv->cmdbuf_mutex);

	if (dev_priv->pinned_bo == NULL)
		goto out_unlock;

	if (only_on_cid_match && cid != dev_priv->query_cid)
		goto out_unlock;

	INIT_LIST_HEAD(&validate_list);

	pinned_val.new_sync_obj_arg = (void *)(unsigned long)
		DRM_VMW_FENCE_FLAG_EXEC;
	pinned_val.bo = ttm_bo_reference(dev_priv->pinned_bo);
	list_add_tail(&pinned_val.head, &validate_list);

	query_val.new_sync_obj_arg = pinned_val.new_sync_obj_arg;
	query_val.bo = ttm_bo_reference(dev_priv->dummy_query_bo);
	list_add_tail(&query_val.head, &validate_list);

	do {
		ret = ttm_eu_reserve_buffers(&validate_list);
	} while (ret == -ERESTARTSYS);

	if (unlikely(ret != 0)) {
		vmw_execbuf_unpin_panic(dev_priv);
		goto out_no_reserve;
	}

	ret = vmw_fifo_emit_dummy_query(dev_priv, dev_priv->query_cid);
	if (unlikely(ret != 0)) {
		vmw_execbuf_unpin_panic(dev_priv);
		goto out_no_emit;
	}

	vmw_bo_pin(dev_priv->pinned_bo, false);
	vmw_bo_pin(dev_priv->dummy_query_bo, false);
	dev_priv->dummy_query_bo_pinned = false;

	(void) vmw_execbuf_fence_commands(NULL, dev_priv, &fence, NULL);
	ttm_eu_fence_buffer_objects(&validate_list, (void *) fence);

	ttm_bo_unref(&query_val.bo);
	ttm_bo_unref(&pinned_val.bo);
	ttm_bo_unref(&dev_priv->pinned_bo);

out_unlock:
	mutex_unlock(&dev_priv->cmdbuf_mutex);
	return;

out_no_emit:
	ttm_eu_backoff_reservation(&validate_list);
out_no_reserve:
	ttm_bo_unref(&query_val.bo);
	ttm_bo_unref(&pinned_val.bo);
	ttm_bo_unref(&dev_priv->pinned_bo);
	mutex_unlock(&dev_priv->cmdbuf_mutex);
}


int vmw_execbuf_ioctl(struct drm_device *dev, void *data,
		      struct drm_file *file_priv)
{
	struct vmw_private *dev_priv = vmw_priv(dev);
	struct drm_vmw_execbuf_arg *arg = (struct drm_vmw_execbuf_arg *)data;
	struct vmw_master *vmaster = vmw_master(file_priv->master);
	int ret;


	if (unlikely(arg->version != DRM_VMW_EXECBUF_VERSION)) {
		DRM_ERROR("Incorrect execbuf version.\n");
		DRM_ERROR("You're running outdated experimental "
			  "vmwgfx user-space drivers.");
		return -EINVAL;
	}

	ret = ttm_read_lock(&vmaster->lock, true);
	if (unlikely(ret != 0))
		return ret;

	ret = vmw_execbuf_process(file_priv, dev_priv,
				  (void __user *)(unsigned long)arg->commands,
				  NULL, arg->command_size, arg->throttle_us,
				  (void __user *)(unsigned long)arg->fence_rep,
				  NULL);

	if (unlikely(ret != 0))
		goto out_unlock;

	vmw_kms_cursor_post_execbuf(dev_priv);

out_unlock:
	ttm_read_unlock(&vmaster->lock);
	return ret;
}
