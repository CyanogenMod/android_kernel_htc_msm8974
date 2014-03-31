/*
 * Copyright (c) 2006, 2007, 2008, 2009 QLogic Corporation. All rights reserved.
 * Copyright (c) 2003, 2004, 2005, 2006 PathScale, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define QSFP_DEV 0xA0
#define QSFP_PWR_LAG_MSEC 2000
#define QSFP_MODPRS_LAG_MSEC 20

#define QSFP_GPIO_MOD_SEL_N (4)
#define QSFP_GPIO_MOD_PRS_N (8)
#define QSFP_GPIO_INT_N (0x10)
#define QSFP_GPIO_MOD_RST_N (0x20)
#define QSFP_GPIO_LP_MODE (0x40)
#define QSFP_GPIO_PORT2_SHIFT 5

#define QSFP_PAGESIZE 128
#define QSFP_MOD_ID_OFFS 128
#define QSFP_MOD_PWR_OFFS 129
#define QSFP_MOD_LEN_OFFS 146
#define QSFP_MOD_TECH_OFFS 147
extern const char *const qib_qsfp_devtech[16];
#define QSFP_IS_ACTIVE(tech) ((0xA2FF >> ((tech) >> 4)) & 1)
#define QSFP_IS_ACTIVE_FAR(tech) ((0x32FF >> ((tech) >> 4)) & 1)
#define QSFP_HAS_ATTEN(tech) ((0x4D00 >> ((tech) >> 4)) & 1)
#define QSFP_IS_CU(tech) ((0xED00 >> ((tech) >> 4)) & 1)
#define QSFP_TECH_1490 9

#define QSFP_OUI(oui) (((unsigned)oui[0] << 16) | ((unsigned)oui[1] << 8) | \
			oui[2])
#define QSFP_OUI_AMPHENOL 0x415048
#define QSFP_OUI_FINISAR  0x009065
#define QSFP_OUI_GORE     0x002177

#define QSFP_VEND_OFFS 148
#define QSFP_VEND_LEN 16
#define QSFP_IBXCV_OFFS 164
#define QSFP_VOUI_OFFS 165
#define QSFP_VOUI_LEN 3
#define QSFP_PN_OFFS 168
#define QSFP_PN_LEN 16
#define QSFP_REV_OFFS 184
#define QSFP_REV_LEN 2
#define QSFP_ATTEN_OFFS 186
#define QSFP_ATTEN_LEN 2
#define QSFP_CC_OFFS 191
#define QSFP_SN_OFFS 196
#define QSFP_SN_LEN 16
#define QSFP_DATE_OFFS 212
#define QSFP_DATE_LEN 6
#define QSFP_LOT_OFFS 218
#define QSFP_LOT_LEN 2
#define QSFP_CC_EXT_OFFS 223


struct qib_qsfp_cache {
	u8 id;	
	u8 pwr; 
	u8 len;	
	u8 tech;
	char vendor[QSFP_VEND_LEN];
	u8 xt_xcv; 
	u8 oui[QSFP_VOUI_LEN];
	u8 partnum[QSFP_PN_LEN];
	u8 rev[QSFP_REV_LEN];
	u8 atten[QSFP_ATTEN_LEN];
	u8 cks1;	
	u8 serial[QSFP_SN_LEN];
	u8 date[QSFP_DATE_LEN];
	u8 lot[QSFP_LOT_LEN];
	u8 cks2;	
};

#define QSFP_PWR(pbyte) (((pbyte) >> 6) & 3)
#define QSFP_ATTEN_SDR(attenarray) (attenarray[0])
#define QSFP_ATTEN_DDR(attenarray) (attenarray[1])

struct qib_qsfp_data {
	
	struct qib_pportdata *ppd;
	struct work_struct work;
	struct qib_qsfp_cache cache;
	unsigned long t_insert;
	u8 modpresent;
};

extern int qib_refresh_qsfp_cache(struct qib_pportdata *ppd,
				  struct qib_qsfp_cache *cp);
extern int qib_qsfp_mod_present(struct qib_pportdata *ppd);
extern void qib_qsfp_init(struct qib_qsfp_data *qd,
			  void (*fevent)(struct work_struct *));
extern void qib_qsfp_deinit(struct qib_qsfp_data *qd);
