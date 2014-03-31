#ifndef __LINUX_MTD_QINFO_H
#define __LINUX_MTD_QINFO_H

#include <linux/mtd/map.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/flashchip.h>
#include <linux/mtd/partitions.h>

struct lpddr_private {
	uint16_t ManufactId;
	uint16_t DevId;
	struct qinfo_chip *qinfo;
	int numchips;
	unsigned long chipshift;
	struct flchip chips[0];
};

struct qinfo_query_info {
	uint8_t	major;
	uint8_t	minor;
	char *id_str;
	char *desc;
};

struct qinfo_chip {
	
	uint16_t DevSizeShift;
	uint16_t BufSizeShift;
	
	uint16_t TotalBlocksNum;
	uint16_t UniformBlockSizeShift;
	
	uint16_t HWPartsNum;
	
	uint16_t SuspEraseSupp;
	
	uint16_t SingleWordProgTime;
	uint16_t ProgBufferTime;
	uint16_t BlockEraseTime;
};

#define LPDDR_MFR_ANY		0xffff
#define LPDDR_ID_ANY		0xffff
#define NUMONYX_MFGR_ID		0x0089
#define R18_DEVICE_ID_1G	0x893c

static inline map_word lpddr_build_cmd(u_long cmd, struct map_info *map)
{
	map_word val = { {0} };
	val.x[0] = cmd;
	return val;
}

#define CMD(x) lpddr_build_cmd(x, map)
#define CMDVAL(cmd) cmd.x[0]

struct mtd_info *lpddr_cmdset(struct map_info *);

#endif

