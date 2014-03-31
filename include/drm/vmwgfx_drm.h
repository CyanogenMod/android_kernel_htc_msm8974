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

#ifndef __VMWGFX_DRM_H__
#define __VMWGFX_DRM_H__

#define DRM_VMW_MAX_SURFACE_FACES 6
#define DRM_VMW_MAX_MIP_LEVELS 24


#define DRM_VMW_GET_PARAM            0
#define DRM_VMW_ALLOC_DMABUF         1
#define DRM_VMW_UNREF_DMABUF         2
#define DRM_VMW_CURSOR_BYPASS        3
#define DRM_VMW_CONTROL_STREAM       4
#define DRM_VMW_CLAIM_STREAM         5
#define DRM_VMW_UNREF_STREAM         6
#define DRM_VMW_CREATE_CONTEXT       7
#define DRM_VMW_UNREF_CONTEXT        8
#define DRM_VMW_CREATE_SURFACE       9
#define DRM_VMW_UNREF_SURFACE        10
#define DRM_VMW_REF_SURFACE          11
#define DRM_VMW_EXECBUF              12
#define DRM_VMW_GET_3D_CAP           13
#define DRM_VMW_FENCE_WAIT           14
#define DRM_VMW_FENCE_SIGNALED       15
#define DRM_VMW_FENCE_UNREF          16
#define DRM_VMW_FENCE_EVENT          17
#define DRM_VMW_PRESENT              18
#define DRM_VMW_PRESENT_READBACK     19
#define DRM_VMW_UPDATE_LAYOUT        20


#define DRM_VMW_PARAM_NUM_STREAMS      0
#define DRM_VMW_PARAM_NUM_FREE_STREAMS 1
#define DRM_VMW_PARAM_3D               2
#define DRM_VMW_PARAM_HW_CAPS          3
#define DRM_VMW_PARAM_FIFO_CAPS        4
#define DRM_VMW_PARAM_MAX_FB_SIZE      5
#define DRM_VMW_PARAM_FIFO_HW_VERSION  6


struct drm_vmw_getparam_arg {
	uint64_t value;
	uint32_t param;
	uint32_t pad64;
};



struct drm_vmw_context_arg {
	int32_t cid;
	uint32_t pad64;
};




struct drm_vmw_surface_create_req {
	uint32_t flags;
	uint32_t format;
	uint32_t mip_levels[DRM_VMW_MAX_SURFACE_FACES];
	uint64_t size_addr;
	int32_t shareable;
	int32_t scanout;
};


struct drm_vmw_surface_arg {
	int32_t sid;
	uint32_t pad64;
};


struct drm_vmw_size {
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t pad64;
};


union drm_vmw_surface_create_arg {
	struct drm_vmw_surface_arg rep;
	struct drm_vmw_surface_create_req req;
};



union drm_vmw_surface_reference_arg {
	struct drm_vmw_surface_create_req rep;
	struct drm_vmw_surface_arg req;
};




#define DRM_VMW_EXECBUF_VERSION 1

struct drm_vmw_execbuf_arg {
	uint64_t commands;
	uint32_t command_size;
	uint32_t throttle_us;
	uint64_t fence_rep;
	uint32_t version;
	uint32_t flags;
};


struct drm_vmw_fence_rep {
	uint32_t handle;
	uint32_t mask;
	uint32_t seqno;
	uint32_t passed_seqno;
	uint32_t pad64;
	int32_t error;
};



struct drm_vmw_alloc_dmabuf_req {
	uint32_t size;
	uint32_t pad64;
};


struct drm_vmw_dmabuf_rep {
	uint64_t map_handle;
	uint32_t handle;
	uint32_t cur_gmr_id;
	uint32_t cur_gmr_offset;
	uint32_t pad64;
};


union drm_vmw_alloc_dmabuf_arg {
	struct drm_vmw_alloc_dmabuf_req req;
	struct drm_vmw_dmabuf_rep rep;
};



struct drm_vmw_unref_dmabuf_arg {
	uint32_t handle;
	uint32_t pad64;
};



struct drm_vmw_rect {
	int32_t x;
	int32_t y;
	uint32_t w;
	uint32_t h;
};


struct drm_vmw_control_stream_arg {
	uint32_t stream_id;
	uint32_t enabled;

	uint32_t flags;
	uint32_t color_key;

	uint32_t handle;
	uint32_t offset;
	int32_t format;
	uint32_t size;
	uint32_t width;
	uint32_t height;
	uint32_t pitch[3];

	uint32_t pad64;
	struct drm_vmw_rect src;
	struct drm_vmw_rect dst;
};


#define DRM_VMW_CURSOR_BYPASS_ALL    (1 << 0)
#define DRM_VMW_CURSOR_BYPASS_FLAGS       (1)


struct drm_vmw_cursor_bypass_arg {
	uint32_t flags;
	uint32_t crtc_id;
	int32_t xpos;
	int32_t ypos;
	int32_t xhot;
	int32_t yhot;
};



struct drm_vmw_stream_arg {
	uint32_t stream_id;
	uint32_t pad64;
};




struct drm_vmw_get_3d_cap_arg {
	uint64_t buffer;
	uint32_t max_size;
	uint32_t pad64;
};


#define DRM_VMW_FENCE_FLAG_EXEC   (1 << 0)
#define DRM_VMW_FENCE_FLAG_QUERY  (1 << 1)

#define DRM_VMW_WAIT_OPTION_UNREF (1 << 0)


struct drm_vmw_fence_wait_arg {
	uint32_t handle;
	int32_t  cookie_valid;
	uint64_t kernel_cookie;
	uint64_t timeout_us;
	int32_t lazy;
	int32_t flags;
	int32_t wait_options;
	int32_t pad64;
};



struct drm_vmw_fence_signaled_arg {
	 uint32_t handle;
	 uint32_t flags;
	 int32_t signaled;
	 uint32_t passed_seqno;
	 uint32_t signaled_flags;
	 uint32_t pad64;
};



struct drm_vmw_fence_arg {
	 uint32_t handle;
	 uint32_t pad64;
};



#define DRM_VMW_EVENT_FENCE_SIGNALED 0x80000000

struct drm_vmw_event_fence {
	struct drm_event base;
	uint64_t user_data;
	uint32_t tv_sec;
	uint32_t tv_usec;
};

#define DRM_VMW_FE_FLAG_REQ_TIME (1 << 0)

struct drm_vmw_fence_event_arg {
	uint64_t fence_rep;
	uint64_t user_data;
	uint32_t handle;
	uint32_t flags;
};




struct drm_vmw_present_arg {
	uint32_t fb_id;
	uint32_t sid;
	int32_t dest_x;
	int32_t dest_y;
	uint64_t clips_ptr;
	uint32_t num_clips;
	uint32_t pad64;
};




struct drm_vmw_present_readback_arg {
	 uint32_t fb_id;
	 uint32_t num_clips;
	 uint64_t clips_ptr;
	 uint64_t fence_rep;
};


struct drm_vmw_update_layout_arg {
	uint32_t num_outputs;
	uint32_t pad64;
	uint64_t rects;
};

#endif
