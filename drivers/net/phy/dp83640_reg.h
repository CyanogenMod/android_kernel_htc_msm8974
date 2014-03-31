#ifndef HAVE_DP83640_REGISTERS
#define HAVE_DP83640_REGISTERS

#define PAGE0                     0x0000
#define PHYCR2                    0x001c 

#define PAGE4                     0x0004
#define PTP_CTL                   0x0014 
#define PTP_TDR                   0x0015 
#define PTP_STS                   0x0016 
#define PTP_TSTS                  0x0017 
#define PTP_RATEL                 0x0018 
#define PTP_RATEH                 0x0019 
#define PTP_RDCKSUM               0x001a 
#define PTP_WRCKSUM               0x001b 
#define PTP_TXTS                  0x001c 
#define PTP_RXTS                  0x001d 
#define PTP_ESTS                  0x001e 
#define PTP_EDATA                 0x001f 

#define PAGE5                     0x0005
#define PTP_TRIG                  0x0014 
#define PTP_EVNT                  0x0015 
#define PTP_TXCFG0                0x0016 
#define PTP_TXCFG1                0x0017 
#define PSF_CFG0                  0x0018 
#define PTP_RXCFG0                0x0019 
#define PTP_RXCFG1                0x001a 
#define PTP_RXCFG2                0x001b 
#define PTP_RXCFG3                0x001c 
#define PTP_RXCFG4                0x001d 
#define PTP_TRDL                  0x001e 
#define PTP_TRDH                  0x001f 

#define PAGE6                     0x0006
#define PTP_COC                   0x0014 
#define PSF_CFG1                  0x0015 
#define PSF_CFG2                  0x0016 
#define PSF_CFG3                  0x0017 
#define PSF_CFG4                  0x0018 
#define PTP_SFDCFG                0x0019 
#define PTP_INTCTL                0x001a 
#define PTP_CLKSRC                0x001b 
#define PTP_ETR                   0x001c 
#define PTP_OFF                   0x001d 
#define PTP_GPIOMON               0x001e 
#define PTP_RXHASH                0x001f 

#define BC_WRITE                  (1<<11) 

#define TRIG_SEL_SHIFT            (10)    
#define TRIG_SEL_MASK             (0x7)
#define TRIG_DIS                  (1<<9)  
#define TRIG_EN                   (1<<8)  
#define TRIG_READ                 (1<<7)  
#define TRIG_LOAD                 (1<<6)  
#define PTP_RD_CLK                (1<<5)  
#define PTP_LOAD_CLK              (1<<4)  
#define PTP_STEP_CLK              (1<<3)  
#define PTP_ENABLE                (1<<2)  
#define PTP_DISABLE               (1<<1)  
#define PTP_RESET                 (1<<0)  

#define TXTS_RDY                  (1<<11) 
#define RXTS_RDY                  (1<<10) 
#define TRIG_DONE                 (1<<9)  
#define EVENT_RDY                 (1<<8)  
#define TXTS_IE                   (1<<3)  
#define RXTS_IE                   (1<<2)  
#define TRIG_IE                   (1<<1)  
#define EVENT_IE                  (1<<0)  

#define TRIG7_ERROR               (1<<15) 
#define TRIG7_ACTIVE              (1<<14) 
#define TRIG6_ERROR               (1<<13) 
#define TRIG6_ACTIVE              (1<<12) 
#define TRIG5_ERROR               (1<<11) 
#define TRIG5_ACTIVE              (1<<10) 
#define TRIG4_ERROR               (1<<9)  
#define TRIG4_ACTIVE              (1<<8)  
#define TRIG3_ERROR               (1<<7)  
#define TRIG3_ACTIVE              (1<<6)  
#define TRIG2_ERROR               (1<<5)  
#define TRIG2_ACTIVE              (1<<4)  
#define TRIG1_ERROR               (1<<3)  
#define TRIG1_ACTIVE              (1<<2)  
#define TRIG0_ERROR               (1<<1)  
#define TRIG0_ACTIVE              (1<<0)  

#define PTP_RATE_DIR              (1<<15) 
#define PTP_TMP_RATE              (1<<14) 
#define PTP_RATE_HI_SHIFT         (0)     
#define PTP_RATE_HI_MASK          (0x3ff)

#define EVNTS_MISSED_SHIFT        (8)     
#define EVNTS_MISSED_MASK         (0x7)
#define EVNT_TS_LEN_SHIFT         (6)     
#define EVNT_TS_LEN_MASK          (0x3)
#define EVNT_RF                   (1<<5)  
#define EVNT_NUM_SHIFT            (2)     
#define EVNT_NUM_MASK             (0x7)
#define MULT_EVNT                 (1<<1)  
#define EVENT_DET                 (1<<0)  

#define E7_RISE                   (1<<15) 
#define E7_DET                    (1<<14) 
#define E6_RISE                   (1<<13) 
#define E6_DET                    (1<<12) 
#define E5_RISE                   (1<<11) 
#define E5_DET                    (1<<10) 
#define E4_RISE                   (1<<9)  
#define E4_DET                    (1<<8)  
#define E3_RISE                   (1<<7)  
#define E3_DET                    (1<<6)  
#define E2_RISE                   (1<<5)  
#define E2_DET                    (1<<4)  
#define E1_RISE                   (1<<3)  
#define E1_DET                    (1<<2)  
#define E0_RISE                   (1<<1)  
#define E0_DET                    (1<<0)  

#define TRIG_PULSE                (1<<15) 
#define TRIG_PER                  (1<<14) 
#define TRIG_IF_LATE              (1<<13) 
#define TRIG_NOTIFY               (1<<12) 
#define TRIG_GPIO_SHIFT           (8)     
#define TRIG_GPIO_MASK            (0xf)
#define TRIG_TOGGLE               (1<<7)  
#define TRIG_CSEL_SHIFT           (1)     
#define TRIG_CSEL_MASK            (0x7)
#define TRIG_WR                   (1<<0)  

#define EVNT_RISE                 (1<<14) 
#define EVNT_FALL                 (1<<13) 
#define EVNT_SINGLE               (1<<12) 
#define EVNT_GPIO_SHIFT           (8)     
#define EVNT_GPIO_MASK            (0xf)
#define EVNT_SEL_SHIFT            (1)     
#define EVNT_SEL_MASK             (0x7)
#define EVNT_WR                   (1<<0)  

#define SYNC_1STEP                (1<<15) 
#define DR_INSERT                 (1<<13) 
#define NTP_TS_EN                 (1<<12) 
#define IGNORE_2STEP              (1<<11) 
#define CRC_1STEP                 (1<<10) 
#define CHK_1STEP                 (1<<9)  
#define IP1588_EN                 (1<<8)  
#define TX_L2_EN                  (1<<7)  
#define TX_IPV6_EN                (1<<6)  
#define TX_IPV4_EN                (1<<5)  
#define TX_PTP_VER_SHIFT          (1)     
#define TX_PTP_VER_MASK           (0xf)
#define TX_TS_EN                  (1<<0)  

#define BYTE0_MASK_SHIFT          (8)     
#define BYTE0_MASK_MASK           (0xff)
#define BYTE0_DATA_SHIFT          (0)     
#define BYTE0_DATA_MASK           (0xff)

#define MAC_SRC_ADD_SHIFT         (11)    
#define MAC_SRC_ADD_MASK          (0x3)
#define MIN_PRE_SHIFT             (8)     
#define MIN_PRE_MASK              (0x7)
#define PSF_ENDIAN                (1<<7)  
#define PSF_IPV4                  (1<<6)  
#define PSF_PCF_RD                (1<<5)  
#define PSF_ERR_EN                (1<<4)  
#define PSF_TXTS_EN               (1<<3)  
#define PSF_RXTS_EN               (1<<2)  
#define PSF_TRIG_EN               (1<<1)  
#define PSF_EVNT_EN               (1<<0)  

#define DOMAIN_EN                 (1<<15) 
#define ALT_MAST_DIS              (1<<14) 
#define USER_IP_SEL               (1<<13) 
#define USER_IP_EN                (1<<12) 
#define RX_SLAVE                  (1<<11) 
#define IP1588_EN_SHIFT           (8)     
#define IP1588_EN_MASK            (0xf)
#define RX_L2_EN                  (1<<7)  
#define RX_IPV6_EN                (1<<6)  
#define RX_IPV4_EN                (1<<5)  
#define RX_PTP_VER_SHIFT          (1)     
#define RX_PTP_VER_MASK           (0xf)
#define RX_TS_EN                  (1<<0)  

#define BYTE0_MASK_SHIFT          (8)     
#define BYTE0_MASK_MASK           (0xff)
#define BYTE0_DATA_SHIFT          (0)     
#define BYTE0_DATA_MASK           (0xff)

#define TS_MIN_IFG_SHIFT          (12)    
#define TS_MIN_IFG_MASK           (0xf)
#define ACC_UDP                   (1<<11) 
#define ACC_CRC                   (1<<10) 
#define TS_APPEND                 (1<<9)  
#define TS_INSERT                 (1<<8)  
#define PTP_DOMAIN_SHIFT          (0)     
#define PTP_DOMAIN_MASK           (0xff)

#define IPV4_UDP_MOD              (1<<15) 
#define TS_SEC_EN                 (1<<14) 
#define TS_SEC_LEN_SHIFT          (12)    
#define TS_SEC_LEN_MASK           (0x3)
#define RXTS_NS_OFF_SHIFT         (6)     
#define RXTS_NS_OFF_MASK          (0x3f)
#define RXTS_SEC_OFF_SHIFT        (0)     
#define RXTS_SEC_OFF_MASK         (0x3f)

#define PTP_CLKOUT_EN             (1<<15) 
#define PTP_CLKOUT_SEL            (1<<14) 
#define PTP_CLKOUT_SPEEDSEL       (1<<13) 
#define PTP_CLKDIV_SHIFT          (0)     
#define PTP_CLKDIV_MASK           (0xff)

#define PTPRESERVED_SHIFT         (12)    
#define PTPRESERVED_MASK          (0xf)
#define VERSIONPTP_SHIFT          (8)     
#define VERSIONPTP_MASK           (0xf)
#define TRANSPORT_SPECIFIC_SHIFT  (4)     
#define TRANSPORT_SPECIFIC_MASK   (0xf)
#define MESSAGETYPE_SHIFT         (0)     
#define MESSAGETYPE_MASK          (0xf)

#define TX_SFD_GPIO_SHIFT         (4)     
#define TX_SFD_GPIO_MASK          (0xf)
#define RX_SFD_GPIO_SHIFT         (0)     
#define RX_SFD_GPIO_MASK          (0xf)

#define PTP_INT_GPIO_SHIFT        (0)     
#define PTP_INT_GPIO_MASK         (0xf)

#define CLK_SRC_SHIFT             (14)    
#define CLK_SRC_MASK              (0x3)
#define CLK_SRC_PER_SHIFT         (0)     
#define CLK_SRC_PER_MASK          (0x7f)

#define PTP_OFFSET_SHIFT          (0)     
#define PTP_OFFSET_MASK           (0xff)

#endif
