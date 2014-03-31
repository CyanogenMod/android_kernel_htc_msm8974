#ifndef _INC_LIBSBEW_H_
#define _INC_LIBSBEW_H_

/*-----------------------------------------------------------------------------
 * libsbew.h - common library elements, charge across mulitple boards
 *
 *   This file contains common Ioctl structures and contents definitions.
 *
 * Copyright (C) 2004-2005  SBE, Inc.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 * For further information, contact via email: support@sbei.com
 * SBE, Inc.  San Ramon, California  U.S.A.
 *-----------------------------------------------------------------------------
 */



#define LOG_NONE          0
#define LOG_ERROR         1
#define LOG_SBEBUG3       3     
#define LOG_LSCHANGE      5     
#define LOG_LSIMMEDIATE   6     
#define LOG_WARN          8
#define LOG_MONITOR      10
#define LOG_SBEBUG12     12     
#define LOG_MONITOR2     14     
#define LOG_DEBUG        16

    
#define c4_LOG_NONE      LOG_NONE
#define c4_LOG_ERROR     LOG_ERROR
#define c4_LOG_WARN      LOG_WARN
#define c4_LOG_sTrace    LOG_MONITOR    
#define c4_LOG_DEBUG     LOG_DEBUG
#define c4_LOG_MAX       LOG_DEBUG





#define REL_STRLEN   80
    struct sbe_drv_info
    {
        int         rel_strlen;
        char        release[REL_STRLEN];
    };




#define CHNM_STRLEN   16
    struct sbe_brd_info
    {
        u_int32_t brd_id;       
        u_int32_t   brd_sn;
        int         brd_chan_cnt;       
        int         brd_port_cnt;       
        unsigned char brdno;    
        unsigned char brd_pci_speed;    
                    u_int8_t brd_mac_addr[6];
        char        first_iname[CHNM_STRLEN];   
        char        last_iname[CHNM_STRLEN];    
        u_int8_t    brd_hdw_id; 
        u_int8_t    reserved8[3];       
        u_int32_t   reserved32[3];      
    };


#define PCI_VENDOR_ID_SBE              0x1176
#define PCI_DEVICE_ID_WANPMC_C4T1E1    0x0701   
#define PCI_DEVICE_ID_WANPTMC_C4T1E1   0x0702   
#define PCI_DEVICE_ID_WANADAPT_HC4T1E1 0x0703   
#define PCI_DEVICE_ID_WANPTMC_256T3_T1 0x0704   
#define PCI_DEVICE_ID_WANPCI_C4T1E1    0x0705   
#define PCI_DEVICE_ID_WANPMC_C1T3      0x0706   
#define PCI_DEVICE_ID_WANPCI_C2T1E1    0x0707   
#define PCI_DEVICE_ID_WANPCI_C1T1E1    0x0708   
#define PCI_DEVICE_ID_WANPMC_C2T1E1    0x0709   
#define PCI_DEVICE_ID_WANPMC_C1T1E1    0x070A   
#define PCI_DEVICE_ID_WANPTMC_256T3_E1 0x070B   
#define PCI_DEVICE_ID_WANPTMC_C24TE1   0x070C   
#define PCI_DEVICE_ID_WANPMC_C4T1E1_L  0x070D   
#define PCI_DEVICE_ID_WANPMC_C2T1E1_L  0x070E   
#define PCI_DEVICE_ID_WANPMC_C1T1E1_L  0x070F   
#define PCI_DEVICE_ID_WANPMC_2SSI      0x0801
#define PCI_DEVICE_ID_WANPCI_4SSI      0x0802
#define PCI_DEVICE_ID_WANPMC_2T3E3     0x0900   
#define SBE_BOARD_ID(v,id)           ((v<<16) | id)

#define BINFO_PCI_SPEED_unk     0
#define BINFO_PCI_SPEED_33      1
#define BINFO_PCI_SPEED_66      2



    struct sbe_iid_info
    {
        u_int32_t   channum;    
        char        iname[CHNM_STRLEN]; 
    };



    struct sbe_brd_addr
    {
        unsigned char func;     
        unsigned char brdno;    
        unsigned char irq;
        unsigned char size;     
#define BRDADDR_SIZE_64  1
#define BRDADDR_SIZE_32  2
        int         reserved1;  

        union
        {
            unsigned long virt64;       
            u_int32_t   virt32[2];
        }           v;
        union
        {
            unsigned long phys64;       
            u_int32_t   phys32[2];
        }           p;
        int         reserved2[4];       
    };



    struct sbecom_wrt_vec
    {
        u_int32_t   reg;
        u_int32_t   data;
    };

#define C1T3_CHIP_MSCC_32        0x01000000
#define C1T3_CHIP_TECT3_8        0x02000000
#define C1T3_CHIP_CPLD_8         0x03000000
#define C1T3_CHIP_EEPROM_8       0x04000000

#define W256T3_CHIP_MUSYCC_32    0x02000000
#define W256T3_CHIP_TEMUX_8      0x10000000
#define W256T3_CHIP_T8110_8      0x20000000
#define W256T3_CHIP_T8110_32     0x22000000
#define W256T3_CHIP_CPLD_8       0x30000000
#define W256T3_CHIP_EEPROM_8     0x40000000





struct sbecom_port_param
{
    u_int8_t    portnum;
    u_int8_t    port_mode;           
    u_int8_t    portStatus;
    u_int8_t    portP;          
                                
#ifdef SBE_PMCC4_ENABLE
	u_int32_t   hypersize;  
#endif
    int         reserved[3-1];    
    int    _res[4];
};

#define CFG_CLK_PORT_MASK      0x80     
#define CFG_CLK_PORT_INTERNAL  0x80     
#define CFG_CLK_PORT_EXTERNAL  0x00     

#define CFG_LBO_MASK      0x0F
#define CFG_LBO_unk       0     
#define CFG_LBO_LH0       1     
#define CFG_LBO_LH7_5     2     
#define CFG_LBO_LH15      3     
#define CFG_LBO_LH22_5    4     
#define CFG_LBO_SH110     5     
#define CFG_LBO_SH220     6     
#define CFG_LBO_SH330     7     
#define CFG_LBO_SH440     8     
#define CFG_LBO_SH550     9     
#define CFG_LBO_SH660     10    
#define CFG_LBO_E75       11    
#define CFG_LBO_E120      12    





    struct sbecom_chan_param
    {
        u_int32_t   channum;    
#ifdef SBE_PMCC4_ENABLE
	u_int32_t   card;  
	u_int32_t   port;  
	u_int8_t bitmask[32];
#endif
        u_int32_t   intr_mask;  
        u_int8_t    status;     
        u_int8_t    chan_mode;  
        u_int8_t    idlecode;   
        u_int8_t    pad_fill_count;     
        u_int8_t    data_inv;   
        u_int8_t    mode_56k;   
        u_int8_t    reserved[2 + 8];    
    };

#define SS7_INTR_SFILT      0x00000020
#define SS7_INTR_SDEC       0x00000040
#define SS7_INTR_SINC       0x00000080
#define SS7_INTR_SUERR      0x00000100
#define INTR_BUFF           0x00000002
#define INTR_EOM            0x00000004
#define INTR_MSG            0x00000008
#define INTR_IDLE           0x00000010

#define TX_ENABLED          0x01
#define RX_ENABLED          0x02

#define CFG_CH_PROTO_TRANS         0
#define CFG_CH_PROTO_SS7           1
#define CFG_CH_PROTO_HDLC_FCS16    2
#define CFG_CH_PROTO_HDLC_FCS32    3
#define CFG_CH_PROTO_ISLP_MODE     4

#define CFG_CH_FLAG_7E      0
#define CFG_CH_FLAG_FF      1
#define CFG_CH_FLAG_00      2

#define CFG_CH_DINV_NONE    0x00
#define CFG_CH_DINV_RX      0x01
#define CFG_CH_DINV_TX      0x02


#define RESET_DEV_TEMUX     1
#define RESET_DEV_TECT3     RESET_DEV_TEMUX
#define RESET_DEV_PLL       2




    struct sbecom_chan_stats
    {
        unsigned long rx_packets;       
        unsigned long tx_packets;       
        unsigned long rx_bytes; 
        unsigned long tx_bytes; 
        unsigned long rx_errors;
        unsigned long tx_errors;
        unsigned long rx_dropped;       
        unsigned long tx_dropped;       

        
        unsigned long rx_length_errors;
        unsigned long rx_over_errors;   
        unsigned long rx_crc_errors;    
        unsigned long rx_frame_errors;  
        unsigned long rx_fifo_errors;   
        unsigned long rx_missed_errors; 

        
        unsigned long tx_aborted_errors;
        unsigned long tx_fifo_errors;
        unsigned long tx_pending;
    };



 

    struct sbecom_card_param
    {
        u_int8_t    framing_type;       
        u_int8_t    loopback;   
        u_int8_t    line_build_out;     
        u_int8_t    receive_eq; 
        u_int8_t    transmit_ones;      
        u_int8_t    clock;      
        u_int8_t    h110enable; 
        u_int8_t    disable_leds;       
        u_int8_t    reserved1;  
        u_int8_t    rear_io;    
        u_int8_t    disable_tx; 
        u_int8_t    mute_los;   
        u_int8_t    los_threshold;      
        u_int8_t    ds1_mode;   
        u_int8_t    ds3_unchan; 
        u_int8_t    reserved[1 + 16];   
    };

#define FRAMING_M13                0
#define FRAMING_CBP                1

#define CFG_CARD_LOOPBACK_NONE     0x00
#define CFG_CARD_LOOPBACK_DIAG     0x01
#define CFG_CARD_LOOPBACK_LINE     0x02
#define CFG_CARD_LOOPBACK_PAYLOAD  0x03

#define CFG_LIU_LOOPBACK_NONE      0x00
#define CFG_LIU_LOOPBACK_ANALOG    0x10
#define CFG_LIU_LOOPBACK_DIGITAL   0x11
#define CFG_LIU_LOOPBACK_REMOTE    0x12

#define CFG_CLK_INTERNAL           0x00
#define CFG_CLK_EXTERNAL           0x01

#define LOOPBACK_NONE              0
#define LOOPBACK_LIU_ANALOG        1
#define LOOPBACK_LIU_DIGITAL       2
#define LOOPBACK_FRAMER_DS3        3
#define LOOPBACK_FRAMER_T1         4
#define LOOPBACK_LIU_REMOTE        5

#define CFG_DS1_MODE_MASK          0x0f
#define CFG_DS1_MODE_T1            0x00
#define CFG_DS1_MODE_E1            0x01
#define CFG_DS1_MODE_CHANGE        0x80

#define CFG_DS3_UNCHAN_MASK        0x01
#define CFG_DS3_UNCHAN_OFF         0x00
#define CFG_DS3_UNCHAN_ON          0x01




    struct sbecom_framer_param
    {
        u_int8_t    framer_num;
        u_int8_t    frame_type; 
        u_int8_t    loopback_type;      
        u_int8_t    auto_alarms;
        u_int8_t    reserved[12];       
    };

#define CFG_FRAME_NONE             0
#define CFG_FRAME_SF               1    
#define CFG_FRAME_ESF              2    
#define CFG_FRAME_E1PLAIN          3    
#define CFG_FRAME_E1CAS            4    
#define CFG_FRAME_E1CRC            5    
#define CFG_FRAME_E1CRC_CAS        6    
#define CFG_FRAME_SF_AMI           7    
#define CFG_FRAME_ESF_AMI          8    
#define CFG_FRAME_E1PLAIN_AMI      9    
#define CFG_FRAME_E1CAS_AMI       10    
#define CFG_FRAME_E1CRC_AMI       11    
#define CFG_FRAME_E1CRC_CAS_AMI   12    

#define IS_FRAME_ANY_T1(field) \
                    (((field) == CFG_FRAME_NONE) || \
                     ((field) == CFG_FRAME_SF)   || \
                     ((field) == CFG_FRAME_ESF)  || \
                     ((field) == CFG_FRAME_SF_AMI) || \
                     ((field) == CFG_FRAME_ESF_AMI))

#define IS_FRAME_ANY_T1ESF(field) \
                    (((field) == CFG_FRAME_ESF) || \
                     ((field) == CFG_FRAME_ESF_AMI))

#define IS_FRAME_ANY_E1(field) \
                    (((field) == CFG_FRAME_E1PLAIN) || \
                     ((field) == CFG_FRAME_E1CAS)   || \
                     ((field) == CFG_FRAME_E1CRC)   || \
                     ((field) == CFG_FRAME_E1CRC_CAS) || \
                     ((field) == CFG_FRAME_E1PLAIN_AMI) || \
                     ((field) == CFG_FRAME_E1CAS_AMI) || \
                     ((field) == CFG_FRAME_E1CRC_AMI) || \
                     ((field) == CFG_FRAME_E1CRC_CAS_AMI))

#define IS_FRAME_ANY_AMI(field) \
                    (((field) == CFG_FRAME_SF_AMI) || \
                     ((field) == CFG_FRAME_ESF_AMI) || \
                     ((field) == CFG_FRAME_E1PLAIN_AMI) || \
                     ((field) == CFG_FRAME_E1CAS_AMI) || \
                     ((field) == CFG_FRAME_E1CRC_AMI) || \
                     ((field) == CFG_FRAME_E1CRC_CAS_AMI))

#define CFG_FRMR_LOOPBACK_NONE     0
#define CFG_FRMR_LOOPBACK_DIAG     1
#define CFG_FRMR_LOOPBACK_LINE     2
#define CFG_FRMR_LOOPBACK_PAYLOAD  3




    struct temux_card_stats
    {
        struct temux_stats
        {
            
            u_int32_t   lcv;
            u_int32_t   err_framing;
            u_int32_t   febe;
            u_int32_t   err_cpbit;
            u_int32_t   err_parity;
            
            u_int8_t    los;
            u_int8_t    oof;
            u_int8_t    red;
            u_int8_t    yellow;
            u_int8_t    idle;
            u_int8_t    ais;
            u_int8_t    cbit;
            
            u_int8_t    feac;
            u_int8_t    feac_last;
        }           t;
        u_int32_t   tx_pending; 
    };


    struct wancfg
    {
        int         cs, ds;
        char       *p;
    };
    typedef struct wancfg wcfg_t;

    extern wcfg_t *wancfg_init (char *, char *);
    extern int  wancfg_card_blink (wcfg_t *, int);
    extern int  wancfg_ctl (wcfg_t *, int, void *, int, void *, int);
    extern int  wancfg_del_card_stats (wcfg_t *);
    extern int  wancfg_del_chan_stats (wcfg_t *, int);
    extern int  wancfg_enable_ports (wcfg_t *, int);
    extern int  wancfg_free (wcfg_t *);
    extern int  wancfg_get_brdaddr (wcfg_t *, struct sbe_brd_addr *);
    extern int  wancfg_get_brdinfo (wcfg_t *, struct sbe_brd_info *);
    extern int  wancfg_get_card (wcfg_t *, struct sbecom_card_param *);
    extern int  wancfg_get_card_chan_stats (wcfg_t *, struct sbecom_chan_stats *);
    extern int  wancfg_get_card_sn (wcfg_t *);
    extern int  wancfg_get_card_stats (wcfg_t *, struct temux_card_stats *);
    extern int  wancfg_get_chan (wcfg_t *, int, struct sbecom_chan_param *);
    extern int  wancfg_get_chan_stats (wcfg_t *, int, struct sbecom_chan_stats *);
    extern int  wancfg_get_drvinfo (wcfg_t *, int, struct sbe_drv_info *);
    extern int  wancfg_get_framer (wcfg_t *, int, struct sbecom_framer_param *);
    extern int  wancfg_get_iid (wcfg_t *, int, struct sbe_iid_info *);
    extern int  wancfg_get_sn (wcfg_t *, unsigned int *);
    extern int  wancfg_read (wcfg_t *, int, struct sbecom_wrt_vec *);
    extern int  wancfg_reset_device (wcfg_t *, int);
    extern int  wancfg_set_card (wcfg_t *, struct sbecom_card_param *);
    extern int  wancfg_set_chan (wcfg_t *, int, struct sbecom_chan_param *);
    extern int  wancfg_set_framer (wcfg_t *, int, struct sbecom_framer_param *);
    extern int  wancfg_set_loglevel (wcfg_t *, uint);
    extern int  wancfg_write (wcfg_t *, int, struct sbecom_wrt_vec *);

#ifdef NOT_YET_COMMON
    extern int  wancfg_get_tsioc (wcfg_t *, struct wanc1t3_ts_hdr *, struct wanc1t3_ts_param *);
    extern int  wancfg_set_tsioc (wcfg_t *, struct wanc1t3_ts_param *);
#endif

#endif                          
