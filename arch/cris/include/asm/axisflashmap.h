#ifndef __ASM_AXISFLASHMAP_H
#define __ASM_AXISFLASHMAP_H

#define FLASH_BOOT_MAGIC 0xbeefcace
#define BOOTPARAM_OFFSET 0xc000

#define PARTITION_TABLE_OFFSET 10
#define PARTITION_TABLE_MAGIC 0xbeef	

struct partitiontable_head {
	__u16 magic;	
	__u16 size;	
	__u32 checksum;	
};

struct partitiontable_entry {
	__u32 offset;		
	__u32 size;		
	__u32 checksum;		
	__u16 type;		
	__u16 flags;		
	__u32 future0;		
	__u32 future1;
	__u32 future2;
	__u32 future3;
};
#define PARTITIONTABLE_END_MARKER 0xFFFFFFFF
#define PARTITIONTABLE_END_MARKER_SIZE 4

#define PARTITIONTABLE_END_PAD	10

struct partitiontable {
	__u8 skip[PARTITION_TABLE_OFFSET];
	struct partitiontable_head head;
	struct partitiontable_entry entries[];
};

#define PARTITION_TYPE_PARAM  0x0001
#define PARTITION_TYPE_KERNEL 0x0002
#define PARTITION_TYPE_JFFS   0x0003
#define PARTITION_TYPE_JFFS2  0x0000

#define	PARTITION_FLAGS_READONLY_MASK	0x0001
#define	PARTITION_FLAGS_READONLY	0x0001

extern struct mtd_info *axisflash_mtd;

#endif
