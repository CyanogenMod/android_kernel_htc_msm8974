/**************************************************************************
 *
 * Copyright 2010 Pauli Nieminen.
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
 *
 **************************************************************************/

#ifndef _DRM_BUFFER_H_
#define _DRM_BUFFER_H_

#include "drmP.h"

struct drm_buffer {
	int iterator;
	int size;
	char *data[];
};


static inline int drm_buffer_page(struct drm_buffer *buf)
{
	return buf->iterator / PAGE_SIZE;
}
static inline int drm_buffer_index(struct drm_buffer *buf)
{
	return buf->iterator & (PAGE_SIZE - 1);
}
static inline int drm_buffer_unprocessed(struct drm_buffer *buf)
{
	return buf->size - buf->iterator;
}

static inline void drm_buffer_advance(struct drm_buffer *buf, int bytes)
{
	buf->iterator += bytes;
}

extern int drm_buffer_alloc(struct drm_buffer **buf, int size);

extern int drm_buffer_copy_from_user(struct drm_buffer *buf,
		void __user *user_data, int size);

extern void drm_buffer_free(struct drm_buffer *buf);

extern void *drm_buffer_read_object(struct drm_buffer *buf,
		int objsize, void *stack_obj);

static inline void *drm_buffer_pointer_to_dword(struct drm_buffer *buffer,
		int offset)
{
	int iter = buffer->iterator + offset * 4;
	return &buffer->data[iter / PAGE_SIZE][iter & (PAGE_SIZE - 1)];
}
static inline void *drm_buffer_pointer_to_byte(struct drm_buffer *buffer,
		int offset)
{
	int iter = buffer->iterator + offset;
	return &buffer->data[iter / PAGE_SIZE][iter & (PAGE_SIZE - 1)];
}

#endif
