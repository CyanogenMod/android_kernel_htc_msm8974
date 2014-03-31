/*
 * Public Include File for DRV6000 users
 * (ie. NxtWave Communications - NXT6000 demodulator driver)
 *
 * Copyright (C) 2001 NxtWave Communications, Inc.
 *
 */


#define MAXNXT6000REG          (0x9A)

#define A_VIT_BER_0            (0x1B)

#define A_VIT_BER_TIMER_0      (0x1D)

#define RS_COR_STAT            (0x21)
#define RSCORESTATUS           (0x03)

#define RS_COR_INTEN           (0x22)

#define RS_COR_INSTAT          (0x23)
#define INSTAT_ERROR           (0x04)
#define LOCK_LOSS_BITS         (0x03)

#define RS_COR_SYNC_PARAM      (0x24)
#define SYNC_PARAM             (0x03)

#define BER_CTRL               (0x25)
#define BER_ENABLE             (0x02)
#define BER_RESET              (0x01)

#define BER_PAY                (0x26)

#define BER_PKT_L              (0x27)
#define BER_PKTOVERFLOW        (0x80)

#define VIT_COR_CTL            (0x30)
#define BER_CONTROL            (0x02)
#define VIT_COR_MASK           (0x82)
#define VIT_COR_RESYNC         (0x80)


#define VIT_SYNC_STATUS        (0x32)
#define VITINSYNC              (0x80)

#define VIT_COR_INTEN          (0x33)
#define GLOBAL_ENABLE          (0x80)

#define VIT_COR_INTSTAT        (0x34)
#define BER_DONE               (0x08)
#define BER_OVERFLOW           (0x10)

#define VIT_BERTIME_2      (0x38)

#define VIT_BERTIME_1      (0x39)

#define VIT_BERTIME_0      (0x3a)

			     
#define A_VIT_BER_TIMER_0      (0x1D)

			     
#define A_VIT_BER_0            (0x1B)

#define VIT_BER_1              (0x3b)

#define VIT_BER_0              (0x3c)

#define OFDM_COR_CTL           (0x40)
#define COREACT                (0x20)
#define HOLDSM                 (0x10)
#define WAIT_AGC               (0x02)
#define WAIT_SYR               (0x03)

#define OFDM_COR_STAT          (0x41)
#define COR_STATUS             (0x0F)
#define MONITOR_TPS            (0x06)
#define TPSLOCKED              (0x40)
#define AGCLOCKED              (0x10)

#define OFDM_COR_INTEN         (0x42)
#define TPSRCVBAD              (0x04)
#define TPSRCVCHANGED         (0x02)
#define TPSRCVUPDATE           (0x01)

#define OFDM_COR_INSTAT        (0x43)

#define OFDM_COR_MODEGUARD     (0x44)
#define FORCEMODE              (0x08)
#define FORCEMODE8K			   (0x04)

#define OFDM_AGC_CTL           (0x45)
#define INITIAL_AGC_BW		   (0x08)
#define AGCNEG                 (0x02)
#define AGCLAST				   (0x10)

#define OFDM_AGC_TARGET		   (0x48)
#define OFDM_AGC_TARGET_DEFAULT (0x28)
#define OFDM_AGC_TARGET_IMPULSE (0x38)

#define OFDM_AGC_GAIN_1        (0x49)

#define OFDM_ITB_CTL           (0x4B)
#define ITBINV                 (0x01)

#define AGC_GAIN_1             (0x49)

#define AGC_GAIN_2             (0x4A)

#define OFDM_ITB_FREQ_1        (0x4C)

#define OFDM_ITB_FREQ_2        (0x4D)

#define OFDM_CAS_CTL           (0x4E)
#define ACSDIS                 (0x40)
#define CCSEN                  (0x80)

#define CAS_FREQ               (0x4F)

#define OFDM_SYR_CTL           (0x51)
#define SIXTH_ENABLE           (0x80)
#define SYR_TRACKING_DISABLE   (0x01)

#define OFDM_SYR_STAT		   (0x52)
#define GI14_2K_SYR_LOCK	   (0x13)
#define GI14_8K_SYR_LOCK	   (0x17)
#define GI14_SYR_LOCK		   (0x10)

#define OFDM_SYR_OFFSET_1      (0x55)

#define OFDM_SYR_OFFSET_2      (0x56)

#define OFDM_SCR_CTL           (0x58)
#define SYR_ADJ_DECAY_MASK     (0x70)
#define SYR_ADJ_DECAY          (0x30)

#define OFDM_PPM_CTL_1         (0x59)
#define PPMMAX_MASK            (0x30)
#define PPM256				   (0x30)

#define OFDM_TRL_NOMINALRATE_1 (0x5B)

#define OFDM_TRL_NOMINALRATE_2 (0x5C)

#define OFDM_TRL_TIME_1        (0x5D)

#define OFDM_CRL_FREQ_1        (0x60)

#define OFDM_CHC_CTL_1         (0x63)
#define MANMEAN1               (0xF0);
#define CHCFIR                 (0x01)

#define OFDM_CHC_SNR           (0x64)

#define OFDM_BDI_CTL           (0x65)
#define LP_SELECT              (0x02)

#define OFDM_TPS_RCVD_1        (0x67)
#define TPSFRAME               (0x03)

#define OFDM_TPS_RCVD_2        (0x68)

#define OFDM_TPS_RCVD_3        (0x69)

#define OFDM_TPS_RCVD_4        (0x6A)

#define OFDM_TPS_RESERVED_1    (0x6B)

#define OFDM_TPS_RESERVED_2    (0x6C)

#define OFDM_MSC_REV           (0x73)

#define OFDM_SNR_CARRIER_2     (0x76)
#define MEAN_MASK              (0x80)
#define MEANBIT                (0x80)

#define ANALOG_CONTROL_0       (0x80)
#define POWER_DOWN_ADC         (0x40)

#define ENABLE_TUNER_IIC       (0x81)
#define ENABLE_TUNER_BIT       (0x01)

#define EN_DMD_RACQ            (0x82)
#define EN_DMD_RACQ_REG_VAL    (0x81)
#define EN_DMD_RACQ_REG_VAL_14 (0x01)

#define SNR_COMMAND            (0x84)
#define SNRStat                (0x80)

#define SNRCARRIERNUMBER_LSB   (0x85)

#define SNRMINTHRESHOLD_LSB    (0x87)

#define SNR_PER_CARRIER_LSB    (0x89)

#define SNRBELOWTHRESHOLD_LSB  (0x8B)

#define RF_AGC_VAL_1           (0x91)

#define RF_AGC_STATUS          (0x92)

#define DIAG_CONFIG            (0x98)
#define DIAG_MASK              (0x70)
#define TB_SET                 (0x10)
#define TRAN_SELECT            (0x07)
#define SERIAL_SELECT          (0x01)

#define SUB_DIAG_MODE_SEL      (0x99)
#define CLKINVERSION           (0x01)

#define TS_FORMAT              (0x9A)
#define ERROR_SENSE            (0x08)
#define VALID_SENSE            (0x04)
#define SYNC_SENSE             (0x02)
#define GATED_CLOCK            (0x01)

#define NXT6000ASICDEVICE      (0x0b)
