#ifndef __QLOGICFAS408_H
#define __QLOGICFAS408_H



#define QL_TURBO_PDMA 1


#define QL_ENABLE_PARITY 1


#define QL_RESET_AT_START 0


#define XTALFREQ	40


#define SLOWCABLE 1

#define FASTSCSI 0

#define FASTCLK 0	

#define SYNCXFRPD 5	

#define SYNCOFFST 0


struct qlogicfas408_priv {
	int qbase;		
	int qinitid;		
	int qabort;		
	int qlirq;		
	int int_type;		
	char qinfo[80];		
	struct scsi_cmnd *qlcmd;	
	struct Scsi_Host *shost;	
	struct qlogicfas408_priv *next; 
};

#define REG0 ( outb( inb( qbase + 0xd ) & 0x7f , qbase + 0xd ), outb( 4 , qbase + 0xd ))
#define REG1 ( outb( inb( qbase + 0xd ) | 0x80 , qbase + 0xd ), outb( 0xb4 | int_type, qbase + 0xd ))

#define WATCHDOG 5000000


#define rtrc(i) {}

#define get_priv_by_cmd(x) (struct qlogicfas408_priv *)&((x)->device->host->hostdata[0])
#define get_priv_by_host(x) (struct qlogicfas408_priv *)&((x)->hostdata[0])

irqreturn_t qlogicfas408_ihandl(int irq, void *dev_id);
int qlogicfas408_queuecommand(struct Scsi_Host *h, struct scsi_cmnd * cmd);
int qlogicfas408_biosparam(struct scsi_device * disk,
			   struct block_device *dev,
			   sector_t capacity, int ip[]);
int qlogicfas408_abort(struct scsi_cmnd * cmd);
int qlogicfas408_bus_reset(struct scsi_cmnd * cmd);
const char *qlogicfas408_info(struct Scsi_Host *host);
int qlogicfas408_get_chip_type(int qbase, int int_type);
void qlogicfas408_setup(int qbase, int id, int int_type);
int qlogicfas408_detect(int qbase, int int_type);
void qlogicfas408_disable_ints(struct qlogicfas408_priv *priv);
#endif	

