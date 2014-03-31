/* Copyright (c) 2009-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <mach/debug_display.h>
#include "mdss_htc_util.h"
#include "mdss_dsi.h"
#include "mdss_mdp.h"

#define CABC_INDEX	 0
struct attribute_status htc_attr_status[] = {
	{"cabc_level_ctl", 1, 1, 1},
};

#define HUE_INDEX	 0
struct attribute_status htc_mdss_pp_pa[] = {
	{"mdss_pp_hue", 0, 0, 0},
};

static struct delayed_work dimming_work;

struct msm_fb_data_type *mfd_instance;
#define DEBUG_BUF   2048
#define MIN_COUNT   9
#define DCS_MAX_CNT   128

static char debug_buf[DEBUG_BUF];
struct mdss_dsi_ctrl_pdata *ctrl_instance = NULL;
static char dcs_cmds[DCS_MAX_CNT];
static char *tmp;
static struct dsi_cmd_desc debug_cmd = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 1, 1}, dcs_cmds
};
static ssize_t dsi_cmd_write(
	struct file *file,
	const char __user *buff,
	size_t count,
	loff_t *ppos)
{
	u32 type, value;
	int cnt, i;
	struct dcs_cmd_req cmdreq;

	if (count >= sizeof(debug_buf) || count < MIN_COUNT)
		return -EFAULT;

	if (copy_from_user(debug_buf, buff, count))
		return -EFAULT;

	if (!ctrl_instance)
		return count;

	
	debug_buf[count] = 0;

	
	cnt = (count) / 3 - 1;
	debug_cmd.dchdr.dlen = cnt;

	
	sscanf(debug_buf, "%x", &type);

	if (type == DTYPE_DCS_LWRITE)
		debug_cmd.dchdr.dtype = DTYPE_DCS_LWRITE;
	else if (type == DTYPE_GEN_LWRITE)
		debug_cmd.dchdr.dtype = DTYPE_GEN_LWRITE;
	else
		return -EFAULT;

	PR_DISP_INFO("%s: cnt=%d, type=0x%x\n", __func__, cnt, type);

	
	for (i = 0; i < cnt; i++) {
		if (i >= DCS_MAX_CNT) {
			PR_DISP_INFO("%s: DCS command count over DCS_MAX_CNT, Skip these commands.\n", __func__);
			break;
		}
		tmp = debug_buf + (3 * (i + 1));
		sscanf(tmp, "%x", &value);
		dcs_cmds[i] = value;
		PR_DISP_INFO("%s: value=0x%x\n", __func__, dcs_cmds[i]);
	}

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &debug_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl_instance, &cmdreq);
	PR_DISP_INFO("%s %d\n", __func__, count);
	return count;
}

static const struct file_operations dsi_cmd_fops = {
        .write = dsi_cmd_write,
};

void htc_debugfs_init(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_data *pdata;
	struct dentry *dent = debugfs_create_dir("htc_debug", NULL);

	pdata = dev_get_platdata(&mfd->pdev->dev);

	ctrl_instance = container_of(pdata, struct mdss_dsi_ctrl_pdata,
						panel_data);

	if (IS_ERR(dent)) {
		pr_err(KERN_ERR "%s(%d): debugfs_create_dir fail, error %ld\n",
			__FILE__, __LINE__, PTR_ERR(dent));
		return;
	}

	if (debugfs_create_file("dsi_cmd", 0644, dent, 0, &dsi_cmd_fops)
			== NULL) {
		pr_err(KERN_ERR "%s(%d): debugfs_create_file: index fail\n",
			__FILE__, __LINE__);
		return;
	}
	return;
}

static unsigned backlightvalue = 0;
static unsigned dua_backlightvalue = 0;
static ssize_t camera_bl_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	ssize_t ret =0;
	sprintf(buf,"%s%u\n%s%u\n", "BL_CAM_MIN=", backlightvalue, "BL_CAM_DUA_MIN=", dua_backlightvalue);
	ret = strlen(buf) + 1;
	return ret;
}

static ssize_t attrs_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int i;

	for (i = 0 ; i < ARRAY_SIZE(htc_attr_status); i++) {
		if (strcmp(attr->attr.name, htc_attr_status[i].title) == 0) {
			sprintf(buf,"%d\n", htc_attr_status[i].cur_value);
		        ret = strlen(buf) + 1;
			break;
		}
	}
	for (i = 0 ; i < ARRAY_SIZE(htc_mdss_pp_pa); i++) {
		if (strcmp(attr->attr.name, htc_mdss_pp_pa[i].title) == 0) {
			sprintf(buf,"%d\n", htc_mdss_pp_pa[i].cur_value);
		        ret = strlen(buf) + 1;
			break;
		}
	}

	return ret;
}

static ssize_t attr_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
	unsigned long res;
	int rc, i;

	rc = strict_strtoul(buf, 10, &res);
	if (rc) {
		pr_err("invalid parameter, %s %d\n", buf, rc);
		count = -EINVAL;
		goto err_out;
	}

	for (i = 0 ; i < ARRAY_SIZE(htc_attr_status); i++) {
		if (strcmp(attr->attr.name, htc_attr_status[i].title) == 0) {
			htc_attr_status[i].req_value = res;
			break;
		}
	}
	for (i = 0 ; i < ARRAY_SIZE(htc_mdss_pp_pa); i++) {
		if (strcmp(attr->attr.name, htc_mdss_pp_pa[i].title) == 0) {
			htc_mdss_pp_pa[i].req_value = res;
			break;
		}
	}

err_out:
	return count;
}

static DEVICE_ATTR(backlight_info, S_IRUGO, camera_bl_show, NULL);
static DEVICE_ATTR(cabc_level_ctl, S_IRUGO | S_IWUSR, attrs_show, attr_store);
static DEVICE_ATTR(mdss_pp_hue, S_IRUGO | S_IWUSR, attrs_show, attr_store);
static struct attribute *htc_extend_attrs[] = {
	&dev_attr_backlight_info.attr,
	&dev_attr_cabc_level_ctl.attr,
	&dev_attr_mdss_pp_hue.attr,
	NULL,
};

static struct attribute_group htc_extend_attr_group = {
	.attrs = htc_extend_attrs,
};

void htc_register_attrs(struct kobject *led_kobj, struct msm_fb_data_type *mfd)
{
	int rc;
	struct mdss_panel_info *panel_info = mfd->panel_info;

	rc = sysfs_create_group(led_kobj, &htc_extend_attr_group);
	if (rc)
		pr_err("sysfs group creation failed, rc=%d\n", rc);

	
	htc_mdss_pp_pa[HUE_INDEX].req_value = panel_info->mdss_pp_hue;

	return;
}

void htc_reset_status(void)
{
	int i;

	for (i = 0 ; i < ARRAY_SIZE(htc_attr_status); i++) {
		htc_attr_status[i].cur_value = htc_attr_status[i].def_value;
	}
	for (i = 0 ; i < ARRAY_SIZE(htc_mdss_pp_pa); i++) {
		htc_mdss_pp_pa[i].cur_value = htc_mdss_pp_pa[i].def_value;
	}


	
	cancel_delayed_work_sync(&dimming_work);

	return;
}

void htc_register_camera_bkl(int level, int dua_level)
{
	backlightvalue = level;
	dua_backlightvalue = dua_level;
}

void htc_set_cabc(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dcs_cmd_req cmdreq;

	pdata = dev_get_platdata(&mfd->pdev->dev);

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
					panel_data);

	if ((htc_attr_status[CABC_INDEX].req_value > 2) || (htc_attr_status[CABC_INDEX].req_value < 0))
		return;

	if (!ctrl_pdata->cabc_off_cmds.cmds)
		return;

	if (!ctrl_pdata->cabc_ui_cmds.cmds)
		return;

	if (!ctrl_pdata->cabc_video_cmds.cmds)
		return;

	if (htc_attr_status[CABC_INDEX].req_value == htc_attr_status[CABC_INDEX].cur_value)
		return;

	memset(&cmdreq, 0, sizeof(cmdreq));

	if (htc_attr_status[CABC_INDEX].req_value == 0) {
		cmdreq.cmds = ctrl_pdata->cabc_off_cmds.cmds;
		cmdreq.cmds_cnt = ctrl_pdata->cabc_off_cmds.cmd_cnt;
	} else if (htc_attr_status[CABC_INDEX].req_value == 1) {
		cmdreq.cmds = ctrl_pdata->cabc_ui_cmds.cmds;
		cmdreq.cmds_cnt = ctrl_pdata->cabc_ui_cmds.cmd_cnt;
	} else if (htc_attr_status[CABC_INDEX].req_value == 2) {
		cmdreq.cmds = ctrl_pdata->cabc_video_cmds.cmds;
		cmdreq.cmds_cnt = ctrl_pdata->cabc_video_cmds.cmd_cnt;
	} else {
		cmdreq.cmds = ctrl_pdata->cabc_ui_cmds.cmds;
		cmdreq.cmds_cnt = ctrl_pdata->cabc_ui_cmds.cmd_cnt;
	}

	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl_pdata, &cmdreq);

	htc_attr_status[CABC_INDEX].cur_value = htc_attr_status[CABC_INDEX].req_value;
	PR_DISP_INFO("%s cabc mode=%d\n", __func__, htc_attr_status[CABC_INDEX].cur_value);
	return;
}
static void dimming_do_work(struct work_struct *work)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dcs_cmd_req cmdreq;

	pdata = dev_get_platdata(&mfd_instance->pdev->dev);

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
					panel_data);

	memset(&cmdreq, 0, sizeof(cmdreq));

	cmdreq.cmds = ctrl_pdata->dimming_on_cmds.cmds;
	cmdreq.cmds_cnt = ctrl_pdata->dimming_on_cmds.cmd_cnt;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl_pdata, &cmdreq);

	PR_DISP_INFO("dimming on\n");
}

void htc_dimming_on(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	pdata = dev_get_platdata(&mfd->pdev->dev);

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
					panel_data);

	if (!ctrl_pdata->dimming_on_cmds.cmds)
		return;

	mfd_instance = mfd;

	INIT_DELAYED_WORK(&dimming_work, dimming_do_work);

	schedule_delayed_work(&dimming_work, msecs_to_jiffies(1000));
	return;
}

#define HUE_MAX   4096
void htc_set_pp_pa(struct mdss_mdp_ctl *ctl)
{
	struct mdss_data_type *mdata;
	struct mdss_mdp_mixer *mixer;
	u32 base = 0, opmode;
	char __iomem *basel;

	
	if (htc_mdss_pp_pa[HUE_INDEX].req_value == htc_mdss_pp_pa[HUE_INDEX].cur_value)
		return;

	if (htc_mdss_pp_pa[HUE_INDEX].req_value >= HUE_MAX)
		return;

	mdata = mdss_mdp_get_mdata();
	mixer = mdata->mixer_intf;

	base = MDSS_MDP_REG_DSPP_OFFSET(0);
	basel = mixer->dspp_base;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	MDSS_MDP_REG_WRITE(base + MDSS_MDP_REG_DSPP_PA_BASE, htc_mdss_pp_pa[HUE_INDEX].req_value);

	opmode = MDSS_MDP_REG_READ(base);
	opmode |= (1 << 20); 
	writel_relaxed(opmode, basel + MDSS_MDP_REG_DSPP_OP_MODE);

	ctl->flush_bits |= BIT(13);

	wmb();
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	htc_mdss_pp_pa[HUE_INDEX].cur_value = htc_mdss_pp_pa[HUE_INDEX].req_value;
	PR_DISP_INFO("%s pp_hue = 0x%x\n", __func__, htc_mdss_pp_pa[HUE_INDEX].req_value);
}
