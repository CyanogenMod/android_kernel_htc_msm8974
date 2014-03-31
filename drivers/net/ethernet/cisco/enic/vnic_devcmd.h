/*
 * Copyright 2008-2010 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef _VNIC_DEVCMD_H_
#define _VNIC_DEVCMD_H_

#define _CMD_NBITS      14
#define _CMD_VTYPEBITS	10
#define _CMD_FLAGSBITS  6
#define _CMD_DIRBITS	2

#define _CMD_NMASK      ((1 << _CMD_NBITS)-1)
#define _CMD_VTYPEMASK  ((1 << _CMD_VTYPEBITS)-1)
#define _CMD_FLAGSMASK  ((1 << _CMD_FLAGSBITS)-1)
#define _CMD_DIRMASK    ((1 << _CMD_DIRBITS)-1)

#define _CMD_NSHIFT     0
#define _CMD_VTYPESHIFT (_CMD_NSHIFT+_CMD_NBITS)
#define _CMD_FLAGSSHIFT (_CMD_VTYPESHIFT+_CMD_VTYPEBITS)
#define _CMD_DIRSHIFT   (_CMD_FLAGSSHIFT+_CMD_FLAGSBITS)

#define _CMD_DIR_NONE   0U
#define _CMD_DIR_WRITE  1U
#define _CMD_DIR_READ   2U
#define _CMD_DIR_RW     (_CMD_DIR_WRITE | _CMD_DIR_READ)

#define _CMD_FLAGS_NONE 0U
#define _CMD_FLAGS_NOWAIT 1U

#define _CMD_VTYPE_NONE  0U
#define _CMD_VTYPE_ENET  1U
#define _CMD_VTYPE_FC    2U
#define _CMD_VTYPE_SCSI  4U
#define _CMD_VTYPE_ALL   (_CMD_VTYPE_ENET | _CMD_VTYPE_FC | _CMD_VTYPE_SCSI)

#define _CMDCF(dir, flags, vtype, nr)  \
	(((dir)   << _CMD_DIRSHIFT) | \
	((flags) << _CMD_FLAGSSHIFT) | \
	((vtype) << _CMD_VTYPESHIFT) | \
	((nr)    << _CMD_NSHIFT))
#define _CMDC(dir, vtype, nr)    _CMDCF(dir, 0, vtype, nr)
#define _CMDCNW(dir, vtype, nr)  _CMDCF(dir, _CMD_FLAGS_NOWAIT, vtype, nr)

#define _CMD_DIR(cmd)            (((cmd) >> _CMD_DIRSHIFT) & _CMD_DIRMASK)
#define _CMD_FLAGS(cmd)          (((cmd) >> _CMD_FLAGSSHIFT) & _CMD_FLAGSMASK)
#define _CMD_VTYPE(cmd)          (((cmd) >> _CMD_VTYPESHIFT) & _CMD_VTYPEMASK)
#define _CMD_N(cmd)              (((cmd) >> _CMD_NSHIFT) & _CMD_NMASK)

enum vnic_devcmd_cmd {
	CMD_NONE                = _CMDC(_CMD_DIR_NONE, _CMD_VTYPE_NONE, 0),

	CMD_MCPU_FW_INFO_OLD    = _CMDC(_CMD_DIR_WRITE, _CMD_VTYPE_ALL, 1),

	/*
	 * mcpu fw info in mem:
	 * in:
	 *   (u64)a0=paddr to struct vnic_devcmd_fw_info
	 *   (u16)a1=size of the structure
	 * out:
	 *	 (u16)a1=0                          for in:a1 = 0,
	 *	         data size actually written for other values.
	 * action:
	 *   Fills in first 128 bytes of vnic_devcmd_fw_info for in:a1 = 0,
	 *            first in:a1 bytes               for 0 < in:a1 <= 132,
	 *            132 bytes                       for other values of in:a1.
	 * note:
	 *   CMD_MCPU_FW_INFO and CMD_MCPU_FW_INFO_OLD have the same enum 1
	 *   for source compatibility.
	 */
	CMD_MCPU_FW_INFO        = _CMDC(_CMD_DIR_RW, _CMD_VTYPE_ALL, 1),

	CMD_DEV_SPEC            = _CMDC(_CMD_DIR_RW, _CMD_VTYPE_ALL, 2),

	
	CMD_STATS_CLEAR         = _CMDCNW(_CMD_DIR_NONE, _CMD_VTYPE_ALL, 3),

	CMD_STATS_DUMP          = _CMDC(_CMD_DIR_WRITE, _CMD_VTYPE_ALL, 4),

	
	CMD_PACKET_FILTER	= _CMDCNW(_CMD_DIR_WRITE, _CMD_VTYPE_ENET, 7),

	
	CMD_PACKET_FILTER_ALL   = _CMDCNW(_CMD_DIR_WRITE, _CMD_VTYPE_ALL, 7),

	
	CMD_HANG_NOTIFY         = _CMDC(_CMD_DIR_NONE, _CMD_VTYPE_ALL, 8),

	
	CMD_GET_MAC_ADDR        = _CMDC(_CMD_DIR_READ,
					_CMD_VTYPE_ENET | _CMD_VTYPE_FC, 9),

	
	CMD_ADDR_ADD            = _CMDCNW(_CMD_DIR_WRITE,
					_CMD_VTYPE_ENET | _CMD_VTYPE_FC, 12),

	
	CMD_ADDR_DEL            = _CMDCNW(_CMD_DIR_WRITE,
					_CMD_VTYPE_ENET | _CMD_VTYPE_FC, 13),

	
	CMD_VLAN_ADD            = _CMDCNW(_CMD_DIR_WRITE, _CMD_VTYPE_ENET, 14),

	
	CMD_VLAN_DEL            = _CMDCNW(_CMD_DIR_WRITE, _CMD_VTYPE_ENET, 15),

	
	CMD_NIC_CFG             = _CMDCNW(_CMD_DIR_WRITE, _CMD_VTYPE_ALL, 16),

	
	CMD_RSS_KEY             = _CMDC(_CMD_DIR_WRITE, _CMD_VTYPE_ENET, 17),

	
	CMD_RSS_CPU             = _CMDC(_CMD_DIR_WRITE, _CMD_VTYPE_ENET, 18),

	
	CMD_SOFT_RESET          = _CMDCNW(_CMD_DIR_NONE, _CMD_VTYPE_ALL, 19),

	CMD_SOFT_RESET_STATUS   = _CMDC(_CMD_DIR_READ, _CMD_VTYPE_ALL, 20),

	CMD_NOTIFY              = _CMDC(_CMD_DIR_RW, _CMD_VTYPE_ALL, 21),

	CMD_UNDI                = _CMDC(_CMD_DIR_WRITE, _CMD_VTYPE_ENET, 22),

	
	CMD_OPEN		= _CMDCNW(_CMD_DIR_WRITE, _CMD_VTYPE_ALL, 23),

	CMD_OPEN_STATUS		= _CMDC(_CMD_DIR_READ, _CMD_VTYPE_ALL, 24),

	
	CMD_CLOSE		= _CMDC(_CMD_DIR_NONE, _CMD_VTYPE_ALL, 25),

	
	CMD_INIT_v1		= _CMDCNW(_CMD_DIR_READ, _CMD_VTYPE_ALL, 26),

	CMD_INIT_PROV_INFO	= _CMDC(_CMD_DIR_WRITE, _CMD_VTYPE_ENET, 27),

	
	CMD_ENABLE		= _CMDCNW(_CMD_DIR_WRITE, _CMD_VTYPE_ALL, 28),

	
	CMD_ENABLE_WAIT		= _CMDC(_CMD_DIR_WRITE, _CMD_VTYPE_ALL, 28),

	
	CMD_DISABLE		= _CMDC(_CMD_DIR_NONE, _CMD_VTYPE_ALL, 29),

	CMD_STATS_DUMP_ALL	= _CMDC(_CMD_DIR_WRITE, _CMD_VTYPE_ALL, 30),

	CMD_INIT_STATUS		= _CMDC(_CMD_DIR_READ, _CMD_VTYPE_ALL, 31),

	CMD_INT13               = _CMDC(_CMD_DIR_WRITE, _CMD_VTYPE_FC, 32),

	
	CMD_LOGICAL_UPLINK      = _CMDCNW(_CMD_DIR_WRITE, _CMD_VTYPE_ENET, 33),

	
	CMD_DEINIT		= _CMDCNW(_CMD_DIR_NONE, _CMD_VTYPE_ALL, 34),

	
	CMD_INIT		= _CMDCNW(_CMD_DIR_WRITE, _CMD_VTYPE_ALL, 35),

	CMD_CAPABILITY		= _CMDC(_CMD_DIR_RW, _CMD_VTYPE_ALL, 36),

	CMD_PERBI		= _CMDC(_CMD_DIR_RW, _CMD_VTYPE_FC, 37),

	CMD_IAR			= _CMDCNW(_CMD_DIR_WRITE, _CMD_VTYPE_ALL, 38),

	
	CMD_HANG_RESET		= _CMDC(_CMD_DIR_NONE, _CMD_VTYPE_ALL, 39),

	CMD_HANG_RESET_STATUS   = _CMDC(_CMD_DIR_READ, _CMD_VTYPE_ALL, 40),

	CMD_IG_VLAN_REWRITE_MODE = _CMDC(_CMD_DIR_RW, _CMD_VTYPE_ENET, 41),

	CMD_PROXY_BY_BDF =	_CMDC(_CMD_DIR_RW, _CMD_VTYPE_ALL, 42),

	CMD_PROXY_BY_INDEX =    _CMDC(_CMD_DIR_RW, _CMD_VTYPE_ALL, 43),

	CMD_CONFIG_INFO_GET     = _CMDC(_CMD_DIR_RW, _CMD_VTYPE_ALL, 44),

	CMD_INIT_PROV_INFO2  = _CMDC(_CMD_DIR_WRITE, _CMD_VTYPE_ENET, 47),

	CMD_ENABLE2 = _CMDC(_CMD_DIR_WRITE, _CMD_VTYPE_ENET, 48),

	CMD_STATUS = _CMDC(_CMD_DIR_RW, _CMD_VTYPE_ALL, 49),

	CMD_INTR_COAL_CONVERT = _CMDC(_CMD_DIR_READ, _CMD_VTYPE_ALL, 50),

	CMD_SET_MAC_ADDR = _CMDC(_CMD_DIR_WRITE, _CMD_VTYPE_ENET, 55),
};

#define CMD_ENABLE2_ACTIVE  0x1

#define CMD_OPENF_OPROM		0x1	

#define CMD_INITF_DEFAULT_MAC	0x1	

#define CMD_PFILTER_DIRECTED		0x01
#define CMD_PFILTER_MULTICAST		0x02
#define CMD_PFILTER_BROADCAST		0x04
#define CMD_PFILTER_PROMISCUOUS		0x08
#define CMD_PFILTER_ALL_MULTICAST	0x10

#define IG_VLAN_REWRITE_MODE_DEFAULT_TRUNK              0
#define IG_VLAN_REWRITE_MODE_UNTAG_DEFAULT_VLAN         1
#define IG_VLAN_REWRITE_MODE_PRIORITY_TAG_DEFAULT_VLAN  2
#define IG_VLAN_REWRITE_MODE_PASS_THRU                  3

enum vnic_devcmd_status {
	STAT_NONE = 0,
	STAT_BUSY = 1 << 0,	
	STAT_ERROR = 1 << 1,	
};

enum vnic_devcmd_error {
	ERR_SUCCESS = 0,
	ERR_EINVAL = 1,
	ERR_EFAULT = 2,
	ERR_EPERM = 3,
	ERR_EBUSY = 4,
	ERR_ECMDUNKNOWN = 5,
	ERR_EBADSTATE = 6,
	ERR_ENOMEM = 7,
	ERR_ETIMEDOUT = 8,
	ERR_ELINKDOWN = 9,
	ERR_EMAXRES = 10,
	ERR_ENOTSUPPORTED = 11,
	ERR_EINPROGRESS = 12,
};

struct vnic_devcmd_fw_info {
	char fw_version[32];
	char fw_build[32];
	char hw_version[32];
	char hw_serial_number[32];
	u16 asic_type;
	u16 asic_rev;
};

struct vnic_devcmd_notify {
	u32 csum;		

	u32 link_state;		
	u32 port_speed;		
	u32 mtu;		
	u32 msglvl;		
	u32 uif;		
	u32 status;		
	u32 error;		
	u32 link_down_cnt;	
	u32 perbi_rebuild_cnt;	
};
#define VNIC_STF_FATAL_ERR	0x0001	
#define VNIC_STF_STD_PAUSE	0x0002	
#define VNIC_STF_PFC_PAUSE	0x0004	
#define VNIC_STF_ALL		(VNIC_STF_FATAL_ERR |\
				 VNIC_STF_STD_PAUSE |\
				 VNIC_STF_PFC_PAUSE |\
				 0)

struct vnic_devcmd_provinfo {
	u8 oui[3];
	u8 type;
	u8 data[0];
};


#define VNIC_DEVCMD_NARGS 15
struct vnic_devcmd {
	u32 status;			
	u32 cmd;			
	u64 args[VNIC_DEVCMD_NARGS];	
};

#endif 
