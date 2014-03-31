/*
 * Copyright (C) 2010 Texas Instruments Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/err.h>

#include <media/v4l2-device.h>
#include <media/davinci/vpbe_types.h>
#include <media/davinci/vpbe.h>
#include <media/davinci/vpss.h>
#include <media/davinci/vpbe_venc.h>

#define VPBE_DEFAULT_OUTPUT	"Composite"
#define VPBE_DEFAULT_MODE	"ntsc"

static char *def_output = VPBE_DEFAULT_OUTPUT;
static char *def_mode = VPBE_DEFAULT_MODE;
static int debug;

module_param(def_output, charp, S_IRUGO);
module_param(def_mode, charp, S_IRUGO);
module_param(debug, int, 0644);

MODULE_PARM_DESC(def_output, "vpbe output name (default:Composite)");
MODULE_PARM_DESC(def_mode, "vpbe output mode name (default:ntsc");
MODULE_PARM_DESC(debug, "Debug level 0-1");

MODULE_DESCRIPTION("TI DMXXX VPBE Display controller");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Texas Instruments");

static struct encoder_config_info*
vpbe_current_encoder_info(struct vpbe_device *vpbe_dev)
{
	struct vpbe_config *cfg = vpbe_dev->cfg;
	int index = vpbe_dev->current_sd_index;

	return ((index == 0) ? &cfg->venc :
				&cfg->ext_encoders[index-1]);
}

static int vpbe_find_encoder_sd_index(struct vpbe_config *cfg,
			     int index)
{
	char *encoder_name = cfg->outputs[index].subdev_name;
	int i;

	
	if (!strcmp(encoder_name, cfg->venc.module_name))
		return 0;

	for (i = 0; i < cfg->num_ext_encoders; i++) {
		if (!strcmp(encoder_name,
		     cfg->ext_encoders[i].module_name))
			return i+1;
	}

	return -EINVAL;
}

static int vpbe_g_cropcap(struct vpbe_device *vpbe_dev,
			  struct v4l2_cropcap *cropcap)
{
	if (NULL == cropcap)
		return -EINVAL;
	cropcap->bounds.left = 0;
	cropcap->bounds.top = 0;
	cropcap->bounds.width = vpbe_dev->current_timings.xres;
	cropcap->bounds.height = vpbe_dev->current_timings.yres;
	cropcap->defrect = cropcap->bounds;

	return 0;
}

static int vpbe_enum_outputs(struct vpbe_device *vpbe_dev,
			     struct v4l2_output *output)
{
	struct vpbe_config *cfg = vpbe_dev->cfg;
	int temp_index = output->index;

	if (temp_index >= cfg->num_outputs)
		return -EINVAL;

	*output = cfg->outputs[temp_index].output;
	output->index = temp_index;

	return 0;
}

static int vpbe_get_mode_info(struct vpbe_device *vpbe_dev, char *mode,
			      int output_index)
{
	struct vpbe_config *cfg = vpbe_dev->cfg;
	struct vpbe_enc_mode_info var;
	int curr_output = output_index;
	int i;

	if (NULL == mode)
		return -EINVAL;

	for (i = 0; i < cfg->outputs[curr_output].num_modes; i++) {
		var = cfg->outputs[curr_output].modes[i];
		if (!strcmp(mode, var.name)) {
			vpbe_dev->current_timings = var;
			return 0;
		}
	}

	return -EINVAL;
}

static int vpbe_get_current_mode_info(struct vpbe_device *vpbe_dev,
				      struct vpbe_enc_mode_info *mode_info)
{
	if (NULL == mode_info)
		return -EINVAL;

	*mode_info = vpbe_dev->current_timings;

	return 0;
}

static int vpbe_get_dv_preset_info(struct vpbe_device *vpbe_dev,
				   unsigned int dv_preset)
{
	struct vpbe_config *cfg = vpbe_dev->cfg;
	struct vpbe_enc_mode_info var;
	int curr_output = vpbe_dev->current_out_index;
	int i;

	for (i = 0; i < vpbe_dev->cfg->outputs[curr_output].num_modes; i++) {
		var = cfg->outputs[curr_output].modes[i];
		if ((var.timings_type & VPBE_ENC_DV_PRESET) &&
		  (var.timings.dv_preset == dv_preset)) {
			vpbe_dev->current_timings = var;
			return 0;
		}
	}

	return -EINVAL;
}

static int vpbe_get_std_info(struct vpbe_device *vpbe_dev,
			     v4l2_std_id std_id)
{
	struct vpbe_config *cfg = vpbe_dev->cfg;
	struct vpbe_enc_mode_info var;
	int curr_output = vpbe_dev->current_out_index;
	int i;

	for (i = 0; i < vpbe_dev->cfg->outputs[curr_output].num_modes; i++) {
		var = cfg->outputs[curr_output].modes[i];
		if ((var.timings_type & VPBE_ENC_STD) &&
		  (var.timings.std_id & std_id)) {
			vpbe_dev->current_timings = var;
			return 0;
		}
	}

	return -EINVAL;
}

static int vpbe_get_std_info_by_name(struct vpbe_device *vpbe_dev,
				char *std_name)
{
	struct vpbe_config *cfg = vpbe_dev->cfg;
	struct vpbe_enc_mode_info var;
	int curr_output = vpbe_dev->current_out_index;
	int i;

	for (i = 0; i < vpbe_dev->cfg->outputs[curr_output].num_modes; i++) {
		var = cfg->outputs[curr_output].modes[i];
		if (!strcmp(var.name, std_name)) {
			vpbe_dev->current_timings = var;
			return 0;
		}
	}

	return -EINVAL;
}

static int vpbe_set_output(struct vpbe_device *vpbe_dev, int index)
{
	struct encoder_config_info *curr_enc_info =
			vpbe_current_encoder_info(vpbe_dev);
	struct vpbe_config *cfg = vpbe_dev->cfg;
	struct venc_platform_data *venc_device = vpbe_dev->venc_device;
	enum v4l2_mbus_pixelcode if_params;
	int enc_out_index;
	int sd_index;
	int ret = 0;

	if (index >= cfg->num_outputs)
		return -EINVAL;

	mutex_lock(&vpbe_dev->lock);

	sd_index = vpbe_dev->current_sd_index;
	enc_out_index = cfg->outputs[index].output.index;
	if (strcmp(curr_enc_info->module_name,
		   cfg->outputs[index].subdev_name)) {
		
		sd_index = vpbe_find_encoder_sd_index(cfg, index);
		if (sd_index < 0) {
			ret = -EINVAL;
			goto out;
		}

		if_params = cfg->outputs[index].if_params;
		venc_device->setup_if_config(if_params);
		if (ret)
			goto out;
	}

	
	ret = v4l2_subdev_call(vpbe_dev->encoders[sd_index], video,
				       s_routing, 0, enc_out_index, 0);
	if (ret)
		goto out;

	ret = vpbe_get_mode_info(vpbe_dev,
				 cfg->outputs[index].default_mode, index);
	if (!ret) {
		struct osd_state *osd_device = vpbe_dev->osd_device;

		osd_device->ops.set_left_margin(osd_device,
			vpbe_dev->current_timings.left_margin);
		osd_device->ops.set_top_margin(osd_device,
		vpbe_dev->current_timings.upper_margin);
		vpbe_dev->current_sd_index = sd_index;
		vpbe_dev->current_out_index = index;
	}
out:
	mutex_unlock(&vpbe_dev->lock);
	return ret;
}

static int vpbe_set_default_output(struct vpbe_device *vpbe_dev)
{
	struct vpbe_config *cfg = vpbe_dev->cfg;
	int ret = 0;
	int i;

	for (i = 0; i < cfg->num_outputs; i++) {
		if (!strcmp(def_output,
			    cfg->outputs[i].output.name)) {
			ret = vpbe_set_output(vpbe_dev, i);
			if (!ret)
				vpbe_dev->current_out_index = i;
			return ret;
		}
	}
	return ret;
}

static unsigned int vpbe_get_output(struct vpbe_device *vpbe_dev)
{
	return vpbe_dev->current_out_index;
}

static int vpbe_s_dv_preset(struct vpbe_device *vpbe_dev,
		     struct v4l2_dv_preset *dv_preset)
{
	struct vpbe_config *cfg = vpbe_dev->cfg;
	int out_index = vpbe_dev->current_out_index;
	int sd_index = vpbe_dev->current_sd_index;
	int ret;


	if (!(cfg->outputs[out_index].output.capabilities &
	    V4L2_OUT_CAP_PRESETS))
		return -EINVAL;

	ret = vpbe_get_dv_preset_info(vpbe_dev, dv_preset->preset);

	if (ret)
		return ret;

	mutex_lock(&vpbe_dev->lock);


	ret = v4l2_subdev_call(vpbe_dev->encoders[sd_index], video,
					s_dv_preset, dv_preset);
	if (!ret && (vpbe_dev->amp != NULL)) {
		
		ret = v4l2_subdev_call(vpbe_dev->amp, video,
				s_dv_preset, dv_preset);
	}
	
	if (!ret) {
		struct osd_state *osd_device = vpbe_dev->osd_device;

		osd_device->ops.set_left_margin(osd_device,
		vpbe_dev->current_timings.left_margin);
		osd_device->ops.set_top_margin(osd_device,
		vpbe_dev->current_timings.upper_margin);
	}
	mutex_unlock(&vpbe_dev->lock);

	return ret;
}

static int vpbe_g_dv_preset(struct vpbe_device *vpbe_dev,
		     struct v4l2_dv_preset *dv_preset)
{
	if (vpbe_dev->current_timings.timings_type &
	  VPBE_ENC_DV_PRESET) {
		dv_preset->preset = vpbe_dev->current_timings.timings.dv_preset;
		return 0;
	}

	return -EINVAL;
}

static int vpbe_enum_dv_presets(struct vpbe_device *vpbe_dev,
			 struct v4l2_dv_enum_preset *preset_info)
{
	struct vpbe_config *cfg = vpbe_dev->cfg;
	int out_index = vpbe_dev->current_out_index;
	struct vpbe_output *output = &cfg->outputs[out_index];
	int j = 0;
	int i;

	if (!(output->output.capabilities & V4L2_OUT_CAP_PRESETS))
		return -EINVAL;

	for (i = 0; i < output->num_modes; i++) {
		if (output->modes[i].timings_type == VPBE_ENC_DV_PRESET) {
			if (j == preset_info->index)
				break;
			j++;
		}
	}

	if (i == output->num_modes)
		return -EINVAL;

	return v4l_fill_dv_preset_info(output->modes[i].timings.dv_preset,
					preset_info);
}

static int vpbe_s_std(struct vpbe_device *vpbe_dev, v4l2_std_id *std_id)
{
	struct vpbe_config *cfg = vpbe_dev->cfg;
	int out_index = vpbe_dev->current_out_index;
	int sd_index = vpbe_dev->current_sd_index;
	int ret;

	if (!(cfg->outputs[out_index].output.capabilities &
		V4L2_OUT_CAP_STD))
		return -EINVAL;

	ret = vpbe_get_std_info(vpbe_dev, *std_id);
	if (ret)
		return ret;

	mutex_lock(&vpbe_dev->lock);

	ret = v4l2_subdev_call(vpbe_dev->encoders[sd_index], video,
			       s_std_output, *std_id);
	
	if (!ret) {
		struct osd_state *osd_device = vpbe_dev->osd_device;

		osd_device->ops.set_left_margin(osd_device,
		vpbe_dev->current_timings.left_margin);
		osd_device->ops.set_top_margin(osd_device,
		vpbe_dev->current_timings.upper_margin);
	}
	mutex_unlock(&vpbe_dev->lock);

	return ret;
}

static int vpbe_g_std(struct vpbe_device *vpbe_dev, v4l2_std_id *std_id)
{
	struct vpbe_enc_mode_info cur_timings = vpbe_dev->current_timings;

	if (cur_timings.timings_type & VPBE_ENC_STD) {
		*std_id = cur_timings.timings.std_id;
		return 0;
	}

	return -EINVAL;
}

static int vpbe_set_mode(struct vpbe_device *vpbe_dev,
			 struct vpbe_enc_mode_info *mode_info)
{
	struct vpbe_enc_mode_info *preset_mode = NULL;
	struct vpbe_config *cfg = vpbe_dev->cfg;
	struct v4l2_dv_preset dv_preset;
	struct osd_state *osd_device;
	int out_index = vpbe_dev->current_out_index;
	int ret = 0;
	int i;

	if ((NULL == mode_info) || (NULL == mode_info->name))
		return -EINVAL;

	for (i = 0; i < cfg->outputs[out_index].num_modes; i++) {
		if (!strcmp(mode_info->name,
		     cfg->outputs[out_index].modes[i].name)) {
			preset_mode = &cfg->outputs[out_index].modes[i];
			if (preset_mode->timings_type & VPBE_ENC_STD)
				return vpbe_s_std(vpbe_dev,
						 &preset_mode->timings.std_id);
			if (preset_mode->timings_type & VPBE_ENC_DV_PRESET) {
				dv_preset.preset =
					preset_mode->timings.dv_preset;
				return vpbe_s_dv_preset(vpbe_dev, &dv_preset);
			}
		}
	}

	
	if (preset_mode == NULL)
		return -EINVAL;

	mutex_lock(&vpbe_dev->lock);

	osd_device = vpbe_dev->osd_device;
	vpbe_dev->current_timings = *preset_mode;
	osd_device->ops.set_left_margin(osd_device,
		vpbe_dev->current_timings.left_margin);
	osd_device->ops.set_top_margin(osd_device,
		vpbe_dev->current_timings.upper_margin);

	mutex_unlock(&vpbe_dev->lock);

	return ret;
}

static int vpbe_set_default_mode(struct vpbe_device *vpbe_dev)
{
	int ret;

	ret = vpbe_get_std_info_by_name(vpbe_dev, def_mode);
	if (ret)
		return ret;

	
	return vpbe_set_mode(vpbe_dev, &vpbe_dev->current_timings);
}

static int platform_device_get(struct device *dev, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct vpbe_device *vpbe_dev = data;

	if (strcmp("vpbe-osd", pdev->name) == 0)
		vpbe_dev->osd_device = platform_get_drvdata(pdev);
	if (strcmp("vpbe-venc", pdev->name) == 0)
		vpbe_dev->venc_device = dev_get_platdata(&pdev->dev);

	return 0;
}

static int vpbe_initialize(struct device *dev, struct vpbe_device *vpbe_dev)
{
	struct encoder_config_info *enc_info;
	struct amp_config_info *amp_info;
	struct v4l2_subdev **enc_subdev;
	struct osd_state *osd_device;
	struct i2c_adapter *i2c_adap;
	int output_index;
	int num_encoders;
	int ret = 0;
	int err;
	int i;

	if (NULL == vpbe_dev || NULL == dev) {
		printk(KERN_ERR "Null device pointers.\n");
		return -ENODEV;
	}

	if (vpbe_dev->initialized)
		return 0;

	mutex_lock(&vpbe_dev->lock);

	if (strcmp(vpbe_dev->cfg->module_name, "dm644x-vpbe-display") != 0) {
		
		vpbe_dev->dac_clk = clk_get(vpbe_dev->pdev, "vpss_dac");
		if (IS_ERR(vpbe_dev->dac_clk)) {
			ret =  PTR_ERR(vpbe_dev->dac_clk);
			goto vpbe_unlock;
		}
		if (clk_enable(vpbe_dev->dac_clk)) {
			ret =  -ENODEV;
			goto vpbe_unlock;
		}
	}

	
	vpss_enable_clock(VPSS_VPBE_CLOCK, 1);

	
	ret = v4l2_device_register(dev, &vpbe_dev->v4l2_dev);
	if (ret) {
		v4l2_err(dev->driver,
			"Unable to register v4l2 device.\n");
		goto vpbe_fail_clock;
	}
	v4l2_info(&vpbe_dev->v4l2_dev, "vpbe v4l2 device registered\n");

	err = bus_for_each_dev(&platform_bus_type, NULL, vpbe_dev,
			       platform_device_get);
	if (err < 0)
		return err;

	vpbe_dev->venc = venc_sub_dev_init(&vpbe_dev->v4l2_dev,
					   vpbe_dev->cfg->venc.module_name);
	
	if (vpbe_dev->venc == NULL) {
		v4l2_err(&vpbe_dev->v4l2_dev,
			"vpbe unable to init venc sub device\n");
		ret = -ENODEV;
		goto vpbe_fail_v4l2_device;
	}
	
	osd_device = vpbe_dev->osd_device;

	if (NULL != osd_device->ops.initialize) {
		err = osd_device->ops.initialize(osd_device);
		if (err) {
			v4l2_err(&vpbe_dev->v4l2_dev,
				 "unable to initialize the OSD device");
			err = -ENOMEM;
			goto vpbe_fail_v4l2_device;
		}
	}

	num_encoders = vpbe_dev->cfg->num_ext_encoders + 1;
	vpbe_dev->encoders = kmalloc(
				sizeof(struct v4l2_subdev *)*num_encoders,
				GFP_KERNEL);
	if (NULL == vpbe_dev->encoders) {
		v4l2_err(&vpbe_dev->v4l2_dev,
			"unable to allocate memory for encoders sub devices");
		ret = -ENOMEM;
		goto vpbe_fail_v4l2_device;
	}

	i2c_adap = i2c_get_adapter(vpbe_dev->cfg->i2c_adapter_id);
	for (i = 0; i < (vpbe_dev->cfg->num_ext_encoders + 1); i++) {
		if (i == 0) {
			
			enc_subdev = &vpbe_dev->encoders[i];
			*enc_subdev = vpbe_dev->venc;
			continue;
		}
		enc_info = &vpbe_dev->cfg->ext_encoders[i];
		if (enc_info->is_i2c) {
			enc_subdev = &vpbe_dev->encoders[i];
			*enc_subdev = v4l2_i2c_new_subdev_board(
						&vpbe_dev->v4l2_dev, i2c_adap,
						&enc_info->board_info, NULL);
			if (*enc_subdev)
				v4l2_info(&vpbe_dev->v4l2_dev,
					  "v4l2 sub device %s registered\n",
					  enc_info->module_name);
			else {
				v4l2_err(&vpbe_dev->v4l2_dev, "encoder %s"
					 " failed to register",
					 enc_info->module_name);
				ret = -ENODEV;
				goto vpbe_fail_sd_register;
			}
		} else
			v4l2_warn(&vpbe_dev->v4l2_dev, "non-i2c encoders"
				 " currently not supported");
	}
	
	if ((strcmp(vpbe_dev->cfg->module_name, "dm365-vpbe-display") == 0) &&
			vpbe_dev->cfg->amp != NULL) {
		amp_info = vpbe_dev->cfg->amp;
		if (amp_info->is_i2c) {
			vpbe_dev->amp = v4l2_i2c_new_subdev_board(
			&vpbe_dev->v4l2_dev, i2c_adap,
			&amp_info->board_info, NULL);
			if (!vpbe_dev->amp) {
				v4l2_err(&vpbe_dev->v4l2_dev,
					 "amplifier %s failed to register",
					 amp_info->module_name);
				ret = -ENODEV;
				goto vpbe_fail_amp_register;
			}
			v4l2_info(&vpbe_dev->v4l2_dev,
					  "v4l2 sub device %s registered\n",
					  amp_info->module_name);
		} else {
			    vpbe_dev->amp = NULL;
			    v4l2_warn(&vpbe_dev->v4l2_dev, "non-i2c amplifiers"
			    " currently not supported");
		}
	} else {
	    vpbe_dev->amp = NULL;
	}

	
	vpbe_dev->current_sd_index = 0;
	vpbe_dev->current_out_index = 0;
	output_index = 0;

	mutex_unlock(&vpbe_dev->lock);

	printk(KERN_NOTICE "Setting default output to %s\n", def_output);
	ret = vpbe_set_default_output(vpbe_dev);
	if (ret) {
		v4l2_err(&vpbe_dev->v4l2_dev, "Failed to set default output %s",
			 def_output);
		return ret;
	}

	printk(KERN_NOTICE "Setting default mode to %s\n", def_mode);
	ret = vpbe_set_default_mode(vpbe_dev);
	if (ret) {
		v4l2_err(&vpbe_dev->v4l2_dev, "Failed to set default mode %s",
			 def_mode);
		return ret;
	}
	vpbe_dev->initialized = 1;
	
	return 0;

vpbe_fail_amp_register:
	kfree(vpbe_dev->amp);
vpbe_fail_sd_register:
	kfree(vpbe_dev->encoders);
vpbe_fail_v4l2_device:
	v4l2_device_unregister(&vpbe_dev->v4l2_dev);
vpbe_fail_clock:
	if (strcmp(vpbe_dev->cfg->module_name, "dm644x-vpbe-display") != 0)
		clk_put(vpbe_dev->dac_clk);
vpbe_unlock:
	mutex_unlock(&vpbe_dev->lock);
	return ret;
}

static void vpbe_deinitialize(struct device *dev, struct vpbe_device *vpbe_dev)
{
	v4l2_device_unregister(&vpbe_dev->v4l2_dev);
	if (strcmp(vpbe_dev->cfg->module_name, "dm644x-vpbe-display") != 0)
		clk_put(vpbe_dev->dac_clk);

	kfree(vpbe_dev->amp);
	kfree(vpbe_dev->encoders);
	vpbe_dev->initialized = 0;
	
	vpss_enable_clock(VPSS_VPBE_CLOCK, 0);
}

static struct vpbe_device_ops vpbe_dev_ops = {
	.g_cropcap = vpbe_g_cropcap,
	.enum_outputs = vpbe_enum_outputs,
	.set_output = vpbe_set_output,
	.get_output = vpbe_get_output,
	.s_dv_preset = vpbe_s_dv_preset,
	.g_dv_preset = vpbe_g_dv_preset,
	.enum_dv_presets = vpbe_enum_dv_presets,
	.s_std = vpbe_s_std,
	.g_std = vpbe_g_std,
	.initialize = vpbe_initialize,
	.deinitialize = vpbe_deinitialize,
	.get_mode_info = vpbe_get_current_mode_info,
	.set_mode = vpbe_set_mode,
};

static __devinit int vpbe_probe(struct platform_device *pdev)
{
	struct vpbe_device *vpbe_dev;
	struct vpbe_config *cfg;
	int ret = -EINVAL;

	if (pdev->dev.platform_data == NULL) {
		v4l2_err(pdev->dev.driver, "No platform data\n");
		return -ENODEV;
	}
	cfg = pdev->dev.platform_data;

	if (!cfg->module_name[0] ||
	    !cfg->osd.module_name[0] ||
	    !cfg->venc.module_name[0]) {
		v4l2_err(pdev->dev.driver, "vpbe display module names not"
			 " defined\n");
		return ret;
	}

	vpbe_dev = kzalloc(sizeof(*vpbe_dev), GFP_KERNEL);
	if (vpbe_dev == NULL) {
		v4l2_err(pdev->dev.driver, "Unable to allocate memory"
			 " for vpbe_device\n");
		return -ENOMEM;
	}
	vpbe_dev->cfg = cfg;
	vpbe_dev->ops = vpbe_dev_ops;
	vpbe_dev->pdev = &pdev->dev;

	if (cfg->outputs->num_modes > 0)
		vpbe_dev->current_timings = vpbe_dev->cfg->outputs[0].modes[0];
	else {
		kfree(vpbe_dev);
		return -ENODEV;
	}

	
	platform_set_drvdata(pdev, vpbe_dev);
	mutex_init(&vpbe_dev->lock);

	return 0;
}

static int vpbe_remove(struct platform_device *device)
{
	struct vpbe_device *vpbe_dev = platform_get_drvdata(device);

	kfree(vpbe_dev);

	return 0;
}

static struct platform_driver vpbe_driver = {
	.driver	= {
		.name	= "vpbe_controller",
		.owner	= THIS_MODULE,
	},
	.probe = vpbe_probe,
	.remove = vpbe_remove,
};

module_platform_driver(vpbe_driver);
