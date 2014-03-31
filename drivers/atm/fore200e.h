#ifndef _FORE200E_H
#define _FORE200E_H

#ifdef __KERNEL__


#define SMALL_BUFFER_SIZE    384     
#define LARGE_BUFFER_SIZE    4032    


#define RBD_BLK_SIZE	     32      


#define MAX_PDU_SIZE	     65535   


#define BUFFER_S1_SIZE       SMALL_BUFFER_SIZE    
#define BUFFER_L1_SIZE       LARGE_BUFFER_SIZE    

#define BUFFER_S2_SIZE       SMALL_BUFFER_SIZE    
#define BUFFER_L2_SIZE       LARGE_BUFFER_SIZE    

#define BUFFER_S1_NBR        (RBD_BLK_SIZE * 6)
#define BUFFER_L1_NBR        (RBD_BLK_SIZE * 4)

#define BUFFER_S2_NBR        (RBD_BLK_SIZE * 6)
#define BUFFER_L2_NBR        (RBD_BLK_SIZE * 4)


#define QUEUE_SIZE_CMD       16	     
#define QUEUE_SIZE_RX	     64	     
#define QUEUE_SIZE_TX	     256     
#define QUEUE_SIZE_BS        32	     

#define FORE200E_VPI_BITS     0
#define FORE200E_VCI_BITS    10
#define NBR_CONNECT          (1 << (FORE200E_VPI_BITS + FORE200E_VCI_BITS)) 


#define TSD_FIXED            2
#define TSD_EXTENSION        0
#define TSD_NBR              (TSD_FIXED + TSD_EXTENSION)



#define RSD_REQUIRED  (((MAX_PDU_SIZE - SMALL_BUFFER_SIZE + LARGE_BUFFER_SIZE) / LARGE_BUFFER_SIZE) + 1)

#define RSD_FIXED     3


#define RSD_EXTENSION  ((RSD_REQUIRED - RSD_FIXED) + 1)
#define RSD_NBR         (RSD_FIXED + RSD_EXTENSION)


#define FORE200E_DEV(d)          ((struct fore200e*)((d)->dev_data))
#define FORE200E_VCC(d)          ((struct fore200e_vcc*)((d)->dev_data))


#if defined(__LITTLE_ENDIAN_BITFIELD)
#define BITFIELD2(b1, b2)                    b1; b2;
#define BITFIELD3(b1, b2, b3)                b1; b2; b3;
#define BITFIELD4(b1, b2, b3, b4)            b1; b2; b3; b4;
#define BITFIELD5(b1, b2, b3, b4, b5)        b1; b2; b3; b4; b5;
#define BITFIELD6(b1, b2, b3, b4, b5, b6)    b1; b2; b3; b4; b5; b6;
#elif defined(__BIG_ENDIAN_BITFIELD)
#define BITFIELD2(b1, b2)                                    b2; b1;
#define BITFIELD3(b1, b2, b3)                            b3; b2; b1;
#define BITFIELD4(b1, b2, b3, b4)                    b4; b3; b2; b1;
#define BITFIELD5(b1, b2, b3, b4, b5)            b5; b4; b3; b2; b1;
#define BITFIELD6(b1, b2, b3, b4, b5, b6)    b6; b5; b4; b3; b2; b1;
#else
#error unknown bitfield endianess
#endif

 

typedef struct atm_header {
    BITFIELD5( 
        u32 clp :  1,    
        u32 plt :  3,    
        u32 vci : 16,    
        u32 vpi :  8,    
        u32 gfc :  4     
   )
} atm_header_t;



typedef enum fore200e_aal {
    FORE200E_AAL0  = 0,
    FORE200E_AAL34 = 4,
    FORE200E_AAL5  = 5,
} fore200e_aal_t;



typedef struct tpd_spec {
    BITFIELD4(
        u32               length : 16,    
        u32               nseg   :  8,    
        enum fore200e_aal aal    :  4,    
        u32               intr   :  4     
    )
} tpd_spec_t;



typedef struct tpd_rate
{
    BITFIELD2( 
        u32 idle_cells : 16,    
        u32 data_cells : 16     
    )
} tpd_rate_t;



typedef struct tsd {
    u32 buffer;    
    u32 length;    
} tsd_t;



typedef struct tpd {
    struct atm_header atm_header;        
    struct tpd_spec   spec;              
    struct tpd_rate   rate;              
    u32               pad;               
    struct tsd        tsd[ TSD_NBR ];    
} tpd_t;



typedef struct rsd {
    u32 handle;    
    u32 length;    
} rsd_t;



typedef struct rpd {
    struct atm_header atm_header;        
    u32               nseg;              
    struct rsd        rsd[ RSD_NBR ];    
} rpd_t;



typedef enum buffer_scheme {
    BUFFER_SCHEME_ONE,
    BUFFER_SCHEME_TWO,
    BUFFER_SCHEME_NBR    
} buffer_scheme_t;



typedef enum buffer_magn {
    BUFFER_MAGN_SMALL,
    BUFFER_MAGN_LARGE,
    BUFFER_MAGN_NBR    
} buffer_magn_t;



typedef struct rbd {
    u32 handle;          
    u32 buffer_haddr;    
} rbd_t;



typedef struct rbd_block {
    struct rbd rbd[ RBD_BLK_SIZE ];    
} rbd_block_t;



typedef struct tpd_haddr {
    BITFIELD3( 
        u32 size  :  4,    
        u32 pad   :  1,    
        u32 haddr : 27     
    )
} tpd_haddr_t;

#define TPD_HADDR_SHIFT 5  


typedef struct cp_txq_entry {
    struct tpd_haddr tpd_haddr;       
    u32              status_haddr;    
} cp_txq_entry_t;



typedef struct cp_rxq_entry {
    u32 rpd_haddr;       
    u32 status_haddr;    
} cp_rxq_entry_t;



typedef struct cp_bsq_entry {
    u32 rbd_block_haddr;    
    u32 status_haddr;       
} cp_bsq_entry_t;



typedef volatile enum status {
    STATUS_PENDING  = (1<<0),    /* initial status (written by host)  */
    STATUS_COMPLETE = (1<<1),    /* completion status (written by cp) */
    STATUS_FREE     = (1<<2),    /* initial status (written by host)  */
    STATUS_ERROR    = (1<<3)     /* completion status (written by cp) */
} status_t;



typedef enum opcode {
    OPCODE_INITIALIZE = 1,          
    OPCODE_ACTIVATE_VCIN,           
    OPCODE_ACTIVATE_VCOUT,          
    OPCODE_DEACTIVATE_VCIN,         
    OPCODE_DEACTIVATE_VCOUT,        
    OPCODE_GET_STATS,               
    OPCODE_SET_OC3,                 
    OPCODE_GET_OC3,                 
    OPCODE_RESET_STATS,             
    OPCODE_GET_PROM,                
    OPCODE_SET_VPI_BITS,            
    OPCODE_REQUEST_INTR = (1<<7)    
} opcode_t;



typedef struct vpvc {
    BITFIELD3(
        u32 vci : 16,    
        u32 vpi :  8,    
        u32 pad :  8     
    )
} vpvc_t;



typedef struct activate_opcode {
    BITFIELD4( 
        enum opcode        opcode : 8,    
        enum fore200e_aal  aal    : 8,    
        enum buffer_scheme scheme : 8,    
        u32  pad                  : 8     
   )
} activate_opcode_t;



typedef struct activate_block {
    struct activate_opcode  opcode;    
    struct vpvc             vpvc;      
    u32                     mtu;       

} activate_block_t;



typedef struct deactivate_opcode {
    BITFIELD2(
        enum opcode opcode :  8,    
        u32         pad    : 24     
    )
} deactivate_opcode_t;



typedef struct deactivate_block {
    struct deactivate_opcode opcode;    
    struct vpvc              vpvc;      
} deactivate_block_t;



typedef struct oc3_regs {
    u32 reg[ 128 ];    
} oc3_regs_t;



typedef struct oc3_opcode {
    BITFIELD4(
        enum opcode opcode : 8,    
	u32         reg    : 8,    
	u32         value  : 8,    
	u32         mask   : 8     
    )
} oc3_opcode_t;



typedef struct oc3_block {
    struct oc3_opcode opcode;        
    u32               regs_haddr;    
} oc3_block_t;



typedef struct stats_phy {
    __be32 crc_header_errors;    
    __be32 framing_errors;       
    __be32 pad[ 2 ];             
} stats_phy_t;



typedef struct stats_oc3 {
    __be32 section_bip8_errors;    
    __be32 path_bip8_errors;       
    __be32 line_bip24_errors;      
    __be32 line_febe_errors;       
    __be32 path_febe_errors;       
    __be32 corr_hcs_errors;        
    __be32 ucorr_hcs_errors;       
    __be32 pad[ 1 ];               
} stats_oc3_t;



typedef struct stats_atm {
    __be32	cells_transmitted;    
    __be32	cells_received;       
    __be32	vpi_bad_range;        
    __be32	vpi_no_conn;          
    __be32	vci_bad_range;        
    __be32	vci_no_conn;          
    __be32	pad[ 2 ];             
} stats_atm_t;


typedef struct stats_aal0 {
    __be32	cells_transmitted;    
    __be32	cells_received;       
    __be32	cells_dropped;        
    __be32	pad[ 1 ];             
} stats_aal0_t;



typedef struct stats_aal34 {
    __be32	cells_transmitted;         
    __be32	cells_received;            
    __be32	cells_crc_errors;          
    __be32	cells_protocol_errors;     
    __be32	cells_dropped;             
    __be32	cspdus_transmitted;        
    __be32	cspdus_received;           
    __be32	cspdus_protocol_errors;    
    __be32	cspdus_dropped;            
    __be32	pad[ 3 ];                  
} stats_aal34_t;



typedef struct stats_aal5 {
    __be32	cells_transmitted;         
    __be32	cells_received;		   
    __be32	cells_dropped;		   
    __be32	congestion_experienced;    
    __be32	cspdus_transmitted;        
    __be32	cspdus_received;           
    __be32	cspdus_crc_errors;         
    __be32	cspdus_protocol_errors;    
    __be32	cspdus_dropped;            
    __be32	pad[ 3 ];                  
} stats_aal5_t;



typedef struct stats_aux {
    __be32	small_b1_failed;     
    __be32	large_b1_failed;     
    __be32	small_b2_failed;     
    __be32	large_b2_failed;     
    __be32	rpd_alloc_failed;    
    __be32	receive_carrier;     
    __be32	pad[ 2 ];            
} stats_aux_t;



typedef struct stats {
    struct stats_phy   phy;      
    struct stats_oc3   oc3;      
    struct stats_atm   atm;      
    struct stats_aal0  aal0;     
    struct stats_aal34 aal34;    
    struct stats_aal5  aal5;     
    struct stats_aux   aux;      
} stats_t;



typedef struct stats_opcode {
    BITFIELD2(
        enum opcode opcode :  8,    
        u32         pad    : 24     
    )
} stats_opcode_t;



typedef struct stats_block {
    struct stats_opcode opcode;         
    u32                 stats_haddr;    
} stats_block_t;



typedef struct prom_data {
    u32 hw_revision;      
    u32 serial_number;    
    u8  mac_addr[ 8 ];    
} prom_data_t;



typedef struct prom_opcode {
    BITFIELD2(
        enum opcode opcode :  8,    
        u32         pad    : 24     
    )
} prom_opcode_t;



typedef struct prom_block {
    struct prom_opcode opcode;        
    u32                prom_haddr;    
} prom_block_t;



typedef union cmd {
    enum   opcode           opcode;           
    struct activate_block   activate_block;   
    struct deactivate_block deactivate_block; 
    struct stats_block      stats_block;      
    struct prom_block       prom_block;       
    struct oc3_block        oc3_block;        
    u32                     pad[ 4 ];         
} cmd_t;



typedef struct cp_cmdq_entry {
    union cmd cmd;             
    u32       status_haddr;    
    u32       pad[ 3 ];        
} cp_cmdq_entry_t;



typedef struct host_txq_entry {
    struct cp_txq_entry __iomem *cp_entry;    
    enum   status*          status;      
    struct tpd*             tpd;         
    u32                     tpd_dma;     
    struct sk_buff*         skb;         
    void*                   data;        
    unsigned long           incarn;      
    struct fore200e_vc_map* vc_map;

} host_txq_entry_t;



typedef struct host_rxq_entry {
    struct cp_rxq_entry __iomem *cp_entry;    
    enum   status*       status;      
    struct rpd*          rpd;         
    u32                  rpd_dma;     
} host_rxq_entry_t;



typedef struct host_bsq_entry {
    struct cp_bsq_entry __iomem *cp_entry;         
    enum   status*       status;           
    struct rbd_block*    rbd_block;        
    u32                  rbd_block_dma;    
} host_bsq_entry_t;



typedef struct host_cmdq_entry {
    struct cp_cmdq_entry __iomem *cp_entry;    
    enum status *status;	       
} host_cmdq_entry_t;



typedef struct chunk {
    void* alloc_addr;    
    void* align_addr;    
    dma_addr_t dma_addr; 
    int   direction;     
    u32   alloc_size;    
    u32   align_size;    
} chunk_t;

#define dma_size align_size             



typedef struct buffer {
    struct buffer*       next;        
    enum   buffer_scheme scheme;      
    enum   buffer_magn   magn;        
    struct chunk         data;        
#ifdef FORE200E_BSQ_DEBUG
    unsigned long        index;       
    int                  supplied;    
#endif
} buffer_t;


#if (BITS_PER_LONG == 32)
#define FORE200E_BUF2HDL(buffer)    ((u32)(buffer))
#define FORE200E_HDL2BUF(handle)    ((struct buffer*)(handle))
#else   
#define FORE200E_BUF2HDL(buffer)    ((u32)((u64)(buffer)))
#define FORE200E_HDL2BUF(handle)    ((struct buffer*)(((u64)(handle)) | PAGE_OFFSET))
#endif



typedef struct host_cmdq {
    struct host_cmdq_entry host_entry[ QUEUE_SIZE_CMD ];    
    int                    head;                            
    struct chunk           status;                          
} host_cmdq_t;



typedef struct host_txq {
    struct host_txq_entry host_entry[ QUEUE_SIZE_TX ];    
    int                   head;                           
    int                   tail;                           
    struct chunk          tpd;                            
    struct chunk          status;                         
    int                   txing;                          
} host_txq_t;



typedef struct host_rxq {
    struct host_rxq_entry  host_entry[ QUEUE_SIZE_RX ];    
    int                    head;                           
    struct chunk           rpd;                            
    struct chunk           status;                         
} host_rxq_t;



typedef struct host_bsq {
    struct host_bsq_entry host_entry[ QUEUE_SIZE_BS ];    
    int                   head;                           
    struct chunk          rbd_block;                      
    struct chunk          status;                         
    struct buffer*        buffer;                         
    struct buffer*        freebuf;                        
    volatile int          freebuf_count;                  
} host_bsq_t;



typedef struct fw_header {
    __le32 magic;           
    __le32 version;         
    __le32 load_offset;     
    __le32 start_offset;    
} fw_header_t;

#define FW_HEADER_MAGIC  0x65726f66    



typedef struct bs_spec {
    u32	queue_length;      
    u32	buffer_size;	   
    u32	pool_size;	   
    u32	supply_blksize;    
} bs_spec_t;



typedef struct init_block {
    enum opcode  opcode;               
    enum status	 status;	       
    u32          receive_threshold;    
    u32          num_connect;          
    u32          cmd_queue_len;        
    u32          tx_queue_len;         
    u32          rx_queue_len;         
    u32          rsd_extension;        
    u32          tsd_extension;        
    u32          conless_vpvc;         
    u32          pad[ 2 ];             
    struct bs_spec bs_spec[ BUFFER_SCHEME_NBR ][ BUFFER_MAGN_NBR ];      
} init_block_t;


typedef enum media_type {
    MEDIA_TYPE_CAT5_UTP  = 0x06,    
    MEDIA_TYPE_MM_OC3_ST = 0x16,    
    MEDIA_TYPE_MM_OC3_SC = 0x26,    
    MEDIA_TYPE_SM_OC3_ST = 0x36,    
    MEDIA_TYPE_SM_OC3_SC = 0x46     
} media_type_t;

#define FORE200E_MEDIA_INDEX(media_type)   ((media_type)>>4)



typedef struct cp_queues {
    u32	              cp_cmdq;         
    u32	              cp_txq;          
    u32	              cp_rxq;          
    u32               cp_bsq[ BUFFER_SCHEME_NBR ][ BUFFER_MAGN_NBR ];        
    u32	              imask;             
    u32	              istat;             
    u32	              heap_base;         
    u32	              heap_size;         
    u32	              hlogger;           
    u32               heartbeat;         
    u32	              fw_release;        
    u32	              mon960_release;    
    u32	              tq_plen;           
    
    struct init_block init;              
    enum   media_type media_type;        
    u32               oc3_revision;      
} cp_queues_t;



typedef enum boot_status {
    BSTAT_COLD_START    = (u32) 0xc01dc01d,    
    BSTAT_SELFTEST_OK   = (u32) 0x02201958,    
    BSTAT_SELFTEST_FAIL = (u32) 0xadbadbad,    
    BSTAT_CP_RUNNING    = (u32) 0xce11feed,    
    BSTAT_MON_TOO_BIG   = (u32) 0x10aded00     
} boot_status_t;



typedef struct soft_uart {
    u32 send;    
    u32 recv;    
} soft_uart_t;

#define FORE200E_CP_MONITOR_UART_FREE     0x00000000
#define FORE200E_CP_MONITOR_UART_AVAIL    0x01000000



typedef struct cp_monitor {
    struct soft_uart    soft_uart;      
    enum boot_status	bstat;          
    u32			app_base;       
    u32			mon_version;    
} cp_monitor_t;



typedef enum fore200e_state {
    FORE200E_STATE_BLANK,         
    FORE200E_STATE_REGISTER,      
    FORE200E_STATE_CONFIGURE,     
    FORE200E_STATE_MAP,           
    FORE200E_STATE_RESET,         
    FORE200E_STATE_START_FW,      
    FORE200E_STATE_INITIALIZE,    
    FORE200E_STATE_INIT_CMDQ,     
    FORE200E_STATE_INIT_TXQ,      
    FORE200E_STATE_INIT_RXQ,      
    FORE200E_STATE_INIT_BSQ,      
    FORE200E_STATE_ALLOC_BUF,     
    FORE200E_STATE_IRQ,           
    FORE200E_STATE_COMPLETE       
} fore200e_state;



typedef struct fore200e_pca_regs {
    volatile u32 __iomem * hcr;    
    volatile u32 __iomem * imr;    
    volatile u32 __iomem * psr;    
} fore200e_pca_regs_t;



typedef struct fore200e_sba_regs {
    u32 __iomem *hcr;    
    u32 __iomem *bsr;    
    u32 __iomem *isr;    
} fore200e_sba_regs_t;



typedef union fore200e_regs {
    struct fore200e_pca_regs pca;    
    struct fore200e_sba_regs sba;    
} fore200e_regs;


struct fore200e;


typedef struct fore200e_bus {
    char*                model_name;          
    char*                proc_name;           
    int                  descr_alignment;     
    int                  buffer_alignment;    
    int                  status_alignment;    
    u32                  (*read)(volatile u32 __iomem *);
    void                 (*write)(u32, volatile u32 __iomem *);
    u32                  (*dma_map)(struct fore200e*, void*, int, int);
    void                 (*dma_unmap)(struct fore200e*, u32, int, int);
    void                 (*dma_sync_for_cpu)(struct fore200e*, u32, int, int);
    void                 (*dma_sync_for_device)(struct fore200e*, u32, int, int);
    int                  (*dma_chunk_alloc)(struct fore200e*, struct chunk*, int, int, int);
    void                 (*dma_chunk_free)(struct fore200e*, struct chunk*);
    int                  (*configure)(struct fore200e*); 
    int                  (*map)(struct fore200e*); 
    void                 (*reset)(struct fore200e*);
    int                  (*prom_read)(struct fore200e*, struct prom_data*);
    void                 (*unmap)(struct fore200e*);
    void                 (*irq_enable)(struct fore200e*);
    int                  (*irq_check)(struct fore200e*);
    void                 (*irq_ack)(struct fore200e*);
    int                  (*proc_read)(struct fore200e*, char*);
} fore200e_bus_t;


typedef struct fore200e_vc_map {
    struct atm_vcc* vcc;       
    unsigned long   incarn;    
} fore200e_vc_map_t;

#define FORE200E_VC_MAP(fore200e, vpi, vci)  \
        (& (fore200e)->vc_map[ ((vpi) << FORE200E_VCI_BITS) | (vci) ])



typedef struct fore200e {
    struct       list_head     entry;                  
    const struct fore200e_bus* bus;                    
    union        fore200e_regs regs;                   
    struct       atm_dev*      atm_dev;                

    enum fore200e_state        state;                  

    char                       name[16];               
    void*                      bus_dev;                
    int                        irq;                    
    unsigned long              phys_base;              
    void __iomem *             virt_base;              
    
    unsigned char              esi[ ESI_LEN ];         

    struct cp_monitor __iomem *         cp_monitor;    
    struct cp_queues __iomem *          cp_queues;              
    struct host_cmdq           host_cmdq;              
    struct host_txq            host_txq;               
    struct host_rxq            host_rxq;               
                                                       
    struct host_bsq            host_bsq[ BUFFER_SCHEME_NBR ][ BUFFER_MAGN_NBR ];       

    u32                        available_cell_rate;    

    int                        loop_mode;              

    struct stats*              stats;                  
    
    struct mutex               rate_mtx;               
    spinlock_t                 q_lock;                 
#ifdef FORE200E_USE_TASKLET
    struct tasklet_struct      tx_tasklet;             
    struct tasklet_struct      rx_tasklet;             
#endif
    unsigned long              tx_sat;                 

    unsigned long              incarn_count;
    struct fore200e_vc_map     vc_map[ NBR_CONNECT ];  
} fore200e_t;



typedef struct fore200e_vcc {
    enum buffer_scheme     scheme;             
    struct tpd_rate        rate;               
    int                    rx_min_pdu;         
    int                    rx_max_pdu;         
    int                    tx_min_pdu;         
    int                    tx_max_pdu;         
    unsigned long          tx_pdu;             
    unsigned long          rx_pdu;             
} fore200e_vcc_t;




#define FORE200E_CP_MONITOR_OFFSET	0x00000400    
#define FORE200E_CP_QUEUES_OFFSET	0x00004d40    



#define PCA200E_IOSPACE_LENGTH	        0x00200000

#define PCA200E_HCR_OFFSET		0x00100000    
#define PCA200E_IMR_OFFSET		0x00100004    
#define PCA200E_PSR_OFFSET		0x00100008    



#define PCA200E_HCR_RESET     (1<<0)    
#define PCA200E_HCR_HOLD_LOCK (1<<1)    
#define PCA200E_HCR_I960FAIL  (1<<2)    
#define PCA200E_HCR_INTRB     (1<<2)    
#define PCA200E_HCR_HOLD_ACK  (1<<3)    
#define PCA200E_HCR_INTRA     (1<<3)    
#define PCA200E_HCR_OUTFULL   (1<<4)    
#define PCA200E_HCR_CLRINTR   (1<<4)    
#define PCA200E_HCR_ESPHOLD   (1<<5)    
#define PCA200E_HCR_INFULL    (1<<6)    
#define PCA200E_HCR_TESTMODE  (1<<7)    



#define PCA200E_PCI_LATENCY      0x40    
#define PCA200E_PCI_MASTER_CTRL  0x41    
#define PCA200E_PCI_THRESHOLD    0x42    


#define PCA200E_CTRL_DIS_CACHE_RD      (1<<0)    
#define PCA200E_CTRL_DIS_WRT_INVAL     (1<<1)    
#define PCA200E_CTRL_2_CACHE_WRT_INVAL (1<<2)    
#define PCA200E_CTRL_IGN_LAT_TIMER     (1<<3)    
#define PCA200E_CTRL_ENA_CONT_REQ_MODE (1<<4)    
#define PCA200E_CTRL_LARGE_PCI_BURSTS  (1<<5)    
#define PCA200E_CTRL_CONVERT_ENDIAN    (1<<6)    



#define SBA200E_PROM_NAME  "FORE,sba-200e"    



#define SBA200E_HCR_LENGTH        4
#define SBA200E_BSR_LENGTH        4
#define SBA200E_ISR_LENGTH        4
#define SBA200E_RAM_LENGTH  0x40000



#define SBA200E_BSR_BURST4   0x04
#define SBA200E_BSR_BURST8   0x08
#define SBA200E_BSR_BURST16  0x10



#define SBA200E_HCR_RESET        (1<<0)    
#define SBA200E_HCR_HOLD_LOCK    (1<<1)    
#define SBA200E_HCR_I960FAIL     (1<<2)    
#define SBA200E_HCR_I960SETINTR  (1<<2)    
#define SBA200E_HCR_OUTFULL      (1<<3)    
#define SBA200E_HCR_INTR_CLR     (1<<3)    
#define SBA200E_HCR_INTR_ENA     (1<<4)    
#define SBA200E_HCR_ESPHOLD      (1<<5)    
#define SBA200E_HCR_INFULL       (1<<6)    
#define SBA200E_HCR_TESTMODE     (1<<7)    
#define SBA200E_HCR_INTR_REQ     (1<<8)    

#define SBA200E_HCR_STICKY       (SBA200E_HCR_RESET | SBA200E_HCR_HOLD_LOCK | SBA200E_HCR_INTR_ENA)


#endif 
#endif 
