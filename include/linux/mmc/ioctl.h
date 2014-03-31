#ifndef LINUX_MMC_IOCTL_H
#define LINUX_MMC_IOCTL_H

#include <linux/types.h>

struct mmc_ioc_cmd {
	
	int write_flag;

	
	int is_acmd;

	__u32 opcode;
	__u32 arg;
	__u32 response[4];  
	unsigned int flags;
	unsigned int blksz;
	unsigned int blocks;

	unsigned int postsleep_min_us;
	unsigned int postsleep_max_us;

	unsigned int data_timeout_ns;
	unsigned int cmd_timeout_ms;

	__u32 __pad;

	
	__u64 data_ptr;
};
#define mmc_ioc_cmd_set_data(ic, ptr) ic.data_ptr = (__u64)(unsigned long) ptr

#define MMC_IOC_CMD _IOWR(MMC_BLOCK_MAJOR, 0, struct mmc_ioc_cmd)

#define MMC_IOC_MAX_RPMB_CMD	3
struct mmc_ioc_rpmb {
	struct mmc_ioc_cmd cmds[MMC_IOC_MAX_RPMB_CMD];
};

#define MMC_IOC_RPMB_CMD _IOWR(MMC_BLOCK_MAJOR, 0, struct mmc_ioc_rpmb)

#define MMC_IOC_MAX_BYTES  (512L * 256)
#endif 
