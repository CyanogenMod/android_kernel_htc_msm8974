/*
 * Copyright © 2006 Eric Anholt
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifndef _INTEL_DVO_H
#define _INTEL_DVO_H

#include <linux/i2c.h>
#include "drmP.h"
#include "drm.h"
#include "drm_crtc.h"
#include "intel_drv.h"

struct intel_dvo_device {
	const char *name;
	int type;
	
	u32 dvo_reg;
	
	u32 gpio;
	int slave_addr;

	const struct intel_dvo_dev_ops *dev_ops;
	void *dev_priv;
	struct i2c_adapter *i2c_bus;
};

struct intel_dvo_dev_ops {
	bool (*init)(struct intel_dvo_device *dvo,
		     struct i2c_adapter *i2cbus);

	void (*create_resources)(struct intel_dvo_device *dvo);

	void (*dpms)(struct intel_dvo_device *dvo, int mode);

	int (*mode_valid)(struct intel_dvo_device *dvo,
			  struct drm_display_mode *mode);

	bool (*mode_fixup)(struct intel_dvo_device *dvo,
			   struct drm_display_mode *mode,
			   struct drm_display_mode *adjusted_mode);

	void (*prepare)(struct intel_dvo_device *dvo);

	void (*commit)(struct intel_dvo_device *dvo);

	void (*mode_set)(struct intel_dvo_device *dvo,
			 struct drm_display_mode *mode,
			 struct drm_display_mode *adjusted_mode);

	enum drm_connector_status (*detect)(struct intel_dvo_device *dvo);

	struct drm_display_mode *(*get_modes)(struct intel_dvo_device *dvo);

	void (*destroy) (struct intel_dvo_device *dvo);

	void (*dump_regs)(struct intel_dvo_device *dvo);
};

extern struct intel_dvo_dev_ops sil164_ops;
extern struct intel_dvo_dev_ops ch7xxx_ops;
extern struct intel_dvo_dev_ops ivch_ops;
extern struct intel_dvo_dev_ops tfp410_ops;
extern struct intel_dvo_dev_ops ch7017_ops;

#endif 
