/*
 *  linux/drivers/message/fusion/mptioctl.h
 *      Fusion MPT misc device (ioctl) driver.
 *      For use with PCI chip/adapter(s):
 *          LSIFC9xx/LSI409xx Fibre Channel
 *      running LSI Fusion MPT (Message Passing Technology) firmware.
 *
 *  Copyright (c) 1999-2008 LSI Corporation
 *  (mailto:DL-MPTFusionLinux@lsi.com)
 *
 */
/*
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    NO WARRANTY
    THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT
    LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is
    solely responsible for determining the appropriateness of using and
    distributing the Program and assumes all risks associated with its
    exercise of rights under this Agreement, including but not limited to
    the risks and costs of program errors, damage to or loss of data,
    programs or equipment, and unavailability or interruption of operations.

    DISCLAIMER OF LIABILITY
    NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
    USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
    HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef MPTCTL_H_INCLUDED
#define MPTCTL_H_INCLUDED



#define MPT_MISCDEV_BASENAME            "mptctl"
#define MPT_MISCDEV_PATHNAME            "/dev/" MPT_MISCDEV_BASENAME

#define MPT_PRODUCT_LENGTH              12

#define MPT_MAGIC_NUMBER	'm'

#define MPTRWPERF		_IOWR(MPT_MAGIC_NUMBER,0,struct mpt_raw_r_w)

#define MPTFWDOWNLOAD		_IOWR(MPT_MAGIC_NUMBER,15,struct mpt_fw_xfer)
#define MPTCOMMAND		_IOWR(MPT_MAGIC_NUMBER,20,struct mpt_ioctl_command)

#if defined(__KERNEL__) && defined(CONFIG_COMPAT)
#define MPTFWDOWNLOAD32		_IOWR(MPT_MAGIC_NUMBER,15,struct mpt_fw_xfer32)
#define MPTCOMMAND32		_IOWR(MPT_MAGIC_NUMBER,20,struct mpt_ioctl_command32)
#endif

#define MPTIOCINFO		_IOWR(MPT_MAGIC_NUMBER,17,struct mpt_ioctl_iocinfo)
#define MPTIOCINFO1		_IOWR(MPT_MAGIC_NUMBER,17,struct mpt_ioctl_iocinfo_rev0)
#define MPTIOCINFO2		_IOWR(MPT_MAGIC_NUMBER,17,struct mpt_ioctl_iocinfo_rev1)
#define MPTTARGETINFO		_IOWR(MPT_MAGIC_NUMBER,18,struct mpt_ioctl_targetinfo)
#define MPTTEST			_IOWR(MPT_MAGIC_NUMBER,19,struct mpt_ioctl_test)
#define MPTEVENTQUERY		_IOWR(MPT_MAGIC_NUMBER,21,struct mpt_ioctl_eventquery)
#define MPTEVENTENABLE		_IOWR(MPT_MAGIC_NUMBER,22,struct mpt_ioctl_eventenable)
#define MPTEVENTREPORT		_IOWR(MPT_MAGIC_NUMBER,23,struct mpt_ioctl_eventreport)
#define MPTHARDRESET		_IOWR(MPT_MAGIC_NUMBER,24,struct mpt_ioctl_diag_reset)
#define MPTFWREPLACE		_IOWR(MPT_MAGIC_NUMBER,25,struct mpt_ioctl_replace_fw)

struct mpt_fw_xfer {
	unsigned int	 iocnum;	
	unsigned int	 fwlen;
	void		__user *bufp;	
};

#if defined(__KERNEL__) && defined(CONFIG_COMPAT)
struct mpt_fw_xfer32 {
	unsigned int iocnum;
	unsigned int fwlen;
	u32 bufp;
};
#endif	

typedef struct _mpt_ioctl_header {
	unsigned int	 iocnum;	
	unsigned int	 port;		
	int		 maxDataSize;	
} mpt_ioctl_header;

struct mpt_ioctl_diag_reset {
	mpt_ioctl_header hdr;
};


struct mpt_ioctl_pci_info {
	union {
		struct {
			unsigned int  deviceNumber   :  5;
			unsigned int  functionNumber :  3;
			unsigned int  busNumber      : 24;
		} bits;
		unsigned int  asUlong;
	} u;
};

struct mpt_ioctl_pci_info2 {
	union {
		struct {
			unsigned int  deviceNumber   :  5;
			unsigned int  functionNumber :  3;
			unsigned int  busNumber      : 24;
		} bits;
		unsigned int  asUlong;
	} u;
  int segmentID;
};

#define MPT_IOCTL_INTERFACE_SCSI	(0x00)
#define MPT_IOCTL_INTERFACE_FC		(0x01)
#define MPT_IOCTL_INTERFACE_FC_IP	(0x02)
#define MPT_IOCTL_INTERFACE_SAS		(0x03)
#define MPT_IOCTL_VERSION_LENGTH	(32)

struct mpt_ioctl_iocinfo {
	mpt_ioctl_header hdr;
	int		 adapterType;	
	int		 port;		
	int		 pciId;		
	int		 hwRev;		
	int		 subSystemDevice;	
	int		 subSystemVendor;	
	int		 numDevices;		
	int		 FWVersion;		
	int		 BIOSVersion;		
	char		 driverVersion[MPT_IOCTL_VERSION_LENGTH];	
	char		 busChangeEvent;
	char		 hostId;
	char		 rsvd[2];
	struct mpt_ioctl_pci_info2  pciInfo; 
};

struct mpt_ioctl_iocinfo_rev1 {
	mpt_ioctl_header hdr;
	int		 adapterType;	
	int		 port;		
	int		 pciId;		
	int		 hwRev;		
	int		 subSystemDevice;	
	int		 subSystemVendor;	
	int		 numDevices;		
	int		 FWVersion;		
	int		 BIOSVersion;		
	char		 driverVersion[MPT_IOCTL_VERSION_LENGTH];	
	char		 busChangeEvent;
	char		 hostId;
	char		 rsvd[2];
	struct mpt_ioctl_pci_info  pciInfo; 
};

struct mpt_ioctl_iocinfo_rev0 {
	mpt_ioctl_header hdr;
	int		 adapterType;	
	int		 port;		
	int		 pciId;		
	int		 hwRev;		
	int		 subSystemDevice;	
	int		 subSystemVendor;	
	int		 numDevices;		
	int		 FWVersion;		
	int		 BIOSVersion;		
	char		 driverVersion[MPT_IOCTL_VERSION_LENGTH];	
	char		 busChangeEvent;
	char		 hostId;
	char		 rsvd[2];
};

struct mpt_ioctl_targetinfo {
	mpt_ioctl_header hdr;
	int		 numDevices;	
	int		 targetInfo[1];
};


struct mpt_ioctl_eventquery {
	mpt_ioctl_header hdr;
	unsigned short	 eventEntries;
	unsigned short	 reserved;
	unsigned int	 eventTypes;
};

struct mpt_ioctl_eventenable {
	mpt_ioctl_header hdr;
	unsigned int	 eventTypes;
};

#ifndef __KERNEL__
typedef struct {
	uint	event;
	uint	eventContext;
	uint	data[2];
} MPT_IOCTL_EVENTS;
#endif

struct mpt_ioctl_eventreport {
	mpt_ioctl_header	hdr;
	MPT_IOCTL_EVENTS	eventData[1];
};

#define MPT_MAX_NAME	32
struct mpt_ioctl_test {
	mpt_ioctl_header hdr;
	u8		 name[MPT_MAX_NAME];
	int		 chip_type;
	u8		 product [MPT_PRODUCT_LENGTH];
};

typedef struct mpt_ioctl_replace_fw {
	mpt_ioctl_header hdr;
	int		 newImageSize;
	u8		 newImage[1];
} mpt_ioctl_replace_fw_t;

struct mpt_ioctl_command {
	mpt_ioctl_header hdr;
	int		timeout;	
	char		__user *replyFrameBufPtr;
	char		__user *dataInBufPtr;
	char		__user *dataOutBufPtr;
	char		__user *senseDataPtr;
	int		maxReplyBytes;
	int		dataInSize;
	int		dataOutSize;
	int		maxSenseBytes;
	int		dataSgeOffset;
	char		MF[1];
};

#if defined(__KERNEL__) && defined(CONFIG_COMPAT)
struct mpt_ioctl_command32 {
	mpt_ioctl_header hdr;
	int	timeout;
	u32	replyFrameBufPtr;
	u32	dataInBufPtr;
	u32	dataOutBufPtr;
	u32	senseDataPtr;
	int	maxReplyBytes;
	int	dataInSize;
	int	dataOutSize;
	int	maxSenseBytes;
	int	dataSgeOffset;
	char	MF[1];
};
#endif	



#define CPQFCTS_IOC_MAGIC 'Z'
#define HP_IOC_MAGIC 'Z'
#define HP_GETHOSTINFO		_IOR(HP_IOC_MAGIC, 20, hp_host_info_t)
#define HP_GETHOSTINFO1		_IOR(HP_IOC_MAGIC, 20, hp_host_info_rev0_t)
#define HP_GETTARGETINFO	_IOR(HP_IOC_MAGIC, 21, hp_target_info_t)

typedef struct _hp_header {
	unsigned int iocnum;
	unsigned int host;
	unsigned int channel;
	unsigned int id;
	unsigned int lun;
} hp_header_t;

typedef struct _hp_host_info {
	hp_header_t	 hdr;
	u16		 vendor;
	u16		 device;
	u16		 subsystem_vendor;
	u16		 subsystem_id;
	u8		 devfn;
	u8		 bus;
	ushort		 host_no;		
	u8		 fw_version[16];	
	u8		 serial_number[24];	
	u32		 ioc_status;
	u32		 bus_phys_width;
	u32		 base_io_addr;
	u32		 rsvd;
	unsigned int	 hard_resets;		
	unsigned int	 soft_resets;		
	unsigned int	 timeouts;		
} hp_host_info_t;

typedef struct _hp_host_info_rev0 {
	hp_header_t	 hdr;
	u16		 vendor;
	u16		 device;
	u16		 subsystem_vendor;
	u16		 subsystem_id;
	u8		 devfn;
	u8		 bus;
	ushort		 host_no;		
	u8		 fw_version[16];	
	u8		 serial_number[24];	
	u32		 ioc_status;
	u32		 bus_phys_width;
	u32		 base_io_addr;
	u32		 rsvd;
	unsigned long	 hard_resets;		
	unsigned long	 soft_resets;		
	unsigned long	 timeouts;		
} hp_host_info_rev0_t;

typedef struct _hp_target_info {
	hp_header_t	 hdr;
	u32 parity_errors;
	u32 phase_errors;
	u32 select_timeouts;
	u32 message_rejects;
	u32 negotiated_speed;
	u8  negotiated_width;
	u8  rsvd[7];				
} hp_target_info_t;

#define HP_STATUS_OTHER		1
#define HP_STATUS_OK		2
#define HP_STATUS_FAILED	3

#define HP_BUS_WIDTH_UNK	1
#define HP_BUS_WIDTH_8		2
#define HP_BUS_WIDTH_16		3
#define HP_BUS_WIDTH_32		4

#define HP_DEV_SPEED_ASYNC	2
#define HP_DEV_SPEED_FAST	3
#define HP_DEV_SPEED_ULTRA	4
#define HP_DEV_SPEED_ULTRA2	5
#define HP_DEV_SPEED_ULTRA160	6
#define HP_DEV_SPEED_SCSI1	7
#define HP_DEV_SPEED_ULTRA320	8




#endif

