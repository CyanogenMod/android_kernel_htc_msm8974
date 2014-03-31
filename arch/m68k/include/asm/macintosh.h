#ifndef __ASM_MACINTOSH_H
#define __ASM_MACINTOSH_H

#include <linux/seq_file.h>
#include <linux/interrupt.h>


extern void mac_reset(void);
extern void mac_poweroff(void);
extern void mac_init_IRQ(void);

extern void mac_irq_enable(struct irq_data *data);
extern void mac_irq_disable(struct irq_data *data);


struct mac_model
{
	short ident;
	char *name;
	char adb_type;
	char via_type;
	char scsi_type;
	char ide_type;
	char scc_type;
	char ether_type;
	char nubus_type;
	char floppy_type;
};

#define MAC_ADB_NONE		0
#define MAC_ADB_II		1
#define MAC_ADB_IISI		2
#define MAC_ADB_CUDA		3
#define MAC_ADB_PB1		4
#define MAC_ADB_PB2		5
#define MAC_ADB_IOP		6

#define MAC_VIA_II		1
#define MAC_VIA_IICI		2
#define MAC_VIA_QUADRA		3

#define MAC_SCSI_NONE		0
#define MAC_SCSI_OLD		1
#define MAC_SCSI_QUADRA		2
#define MAC_SCSI_QUADRA2	3
#define MAC_SCSI_QUADRA3	4

#define MAC_IDE_NONE		0
#define MAC_IDE_QUADRA		1
#define MAC_IDE_PB		2
#define MAC_IDE_BABOON		3

#define MAC_SCC_II		1
#define MAC_SCC_IOP		2
#define MAC_SCC_QUADRA		3
#define MAC_SCC_PSC		4

#define MAC_ETHER_NONE		0
#define MAC_ETHER_SONIC		1
#define MAC_ETHER_MACE		2

#define MAC_NO_NUBUS		0
#define MAC_NUBUS		1

#define MAC_FLOPPY_IWM		0
#define MAC_FLOPPY_SWIM_ADDR1	1
#define MAC_FLOPPY_SWIM_ADDR2	2
#define MAC_FLOPPY_SWIM_IOP	3
#define MAC_FLOPPY_AV		4


#define MAC_MODEL_II		6
#define MAC_MODEL_IIX		7
#define MAC_MODEL_IICX		8
#define MAC_MODEL_SE30		9
#define MAC_MODEL_IICI		11
#define MAC_MODEL_IIFX		13	
#define MAC_MODEL_IISI		18
#define MAC_MODEL_LC		19
#define MAC_MODEL_Q900		20
#define MAC_MODEL_PB170		21
#define MAC_MODEL_Q700		22
#define MAC_MODEL_CLII		23	
#define MAC_MODEL_PB140		25
#define MAC_MODEL_Q950		26	
#define MAC_MODEL_LCIII		27	
#define MAC_MODEL_PB210		29
#define MAC_MODEL_C650		30
#define MAC_MODEL_PB230		32
#define MAC_MODEL_PB180		33
#define MAC_MODEL_PB160		34
#define MAC_MODEL_Q800		35	
#define MAC_MODEL_Q650		36
#define MAC_MODEL_LCII		37	
#define MAC_MODEL_PB250		38
#define MAC_MODEL_IIVI		44
#define MAC_MODEL_P600		45	
#define MAC_MODEL_IIVX		48
#define MAC_MODEL_CCL		49	
#define MAC_MODEL_PB165C	50
#define MAC_MODEL_C610		52	
#define MAC_MODEL_Q610		53
#define MAC_MODEL_PB145		54	
#define MAC_MODEL_P520		56	
#define MAC_MODEL_C660		60
#define MAC_MODEL_P460		62	
#define MAC_MODEL_PB180C	71
#define MAC_MODEL_PB520		72	
#define MAC_MODEL_PB270C	77
#define MAC_MODEL_Q840		78
#define MAC_MODEL_P550		80	
#define MAC_MODEL_CCLII		83	
#define MAC_MODEL_PB165		84
#define MAC_MODEL_PB190		85	
#define MAC_MODEL_TV		88
#define MAC_MODEL_P475		89	
#define MAC_MODEL_P475F		90	
#define MAC_MODEL_P575		92	
#define MAC_MODEL_Q605		94
#define MAC_MODEL_Q605_ACC	95	
#define MAC_MODEL_Q630		98	
#define MAC_MODEL_P588		99	
#define MAC_MODEL_PB280		102
#define MAC_MODEL_PB280C	103
#define MAC_MODEL_PB150		115

extern struct mac_model *macintosh_config;

#endif
