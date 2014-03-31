/*
 * Copyright 2007-2010 Analog Devices Inc.
 *
 * Licensed under the ADI BSD license or the GPL-2 (or later)
 */

#ifndef _DEF_BF544_H
#define _DEF_BF544_H

#include "defBF54x_base.h"



#define                    TIMER8_CONFIG  0xffc00600   
#define                   TIMER8_COUNTER  0xffc00604   
#define                    TIMER8_PERIOD  0xffc00608   
#define                     TIMER8_WIDTH  0xffc0060c   
#define                    TIMER9_CONFIG  0xffc00610   
#define                   TIMER9_COUNTER  0xffc00614   
#define                    TIMER9_PERIOD  0xffc00618   
#define                     TIMER9_WIDTH  0xffc0061c   
#define                   TIMER10_CONFIG  0xffc00620   
#define                  TIMER10_COUNTER  0xffc00624   
#define                   TIMER10_PERIOD  0xffc00628   
#define                    TIMER10_WIDTH  0xffc0062c   


#define                    TIMER_ENABLE1  0xffc00640   
#define                   TIMER_DISABLE1  0xffc00644   
#define                    TIMER_STATUS1  0xffc00648   


#define                     EPPI0_STATUS  0xffc01000   
#define                     EPPI0_HCOUNT  0xffc01004   
#define                     EPPI0_HDELAY  0xffc01008   
#define                     EPPI0_VCOUNT  0xffc0100c   
#define                     EPPI0_VDELAY  0xffc01010   
#define                      EPPI0_FRAME  0xffc01014   
#define                       EPPI0_LINE  0xffc01018   
#define                     EPPI0_CLKDIV  0xffc0101c   
#define                    EPPI0_CONTROL  0xffc01020   
#define                   EPPI0_FS1W_HBL  0xffc01024   
#define                  EPPI0_FS1P_AVPL  0xffc01028   
#define                   EPPI0_FS2W_LVB  0xffc0102c   
#define                  EPPI0_FS2P_LAVF  0xffc01030   
#define                       EPPI0_CLIP  0xffc01034   


#define                     TWI1_REGBASE  0xffc02200
#define                      TWI1_CLKDIV  0xffc02200   
#define                     TWI1_CONTROL  0xffc02204   
#define                   TWI1_SLAVE_CTL  0xffc02208   
#define                  TWI1_SLAVE_STAT  0xffc0220c   
#define                  TWI1_SLAVE_ADDR  0xffc02210   
#define                  TWI1_MASTER_CTL  0xffc02214   
#define                 TWI1_MASTER_STAT  0xffc02218   
#define                 TWI1_MASTER_ADDR  0xffc0221c   
#define                    TWI1_INT_STAT  0xffc02220   
#define                    TWI1_INT_MASK  0xffc02224   
#define                    TWI1_FIFO_CTL  0xffc02228   
#define                   TWI1_FIFO_STAT  0xffc0222c   
#define                   TWI1_XMT_DATA8  0xffc02280   
#define                  TWI1_XMT_DATA16  0xffc02284   
#define                   TWI1_RCV_DATA8  0xffc02288   
#define                  TWI1_RCV_DATA16  0xffc0228c   


#define                         CAN1_MC1  0xffc03200   
#define                         CAN1_MD1  0xffc03204   
#define                        CAN1_TRS1  0xffc03208   
#define                        CAN1_TRR1  0xffc0320c   
#define                         CAN1_TA1  0xffc03210   
#define                         CAN1_AA1  0xffc03214   
#define                        CAN1_RMP1  0xffc03218   
#define                        CAN1_RML1  0xffc0321c   
#define                      CAN1_MBTIF1  0xffc03220   
#define                      CAN1_MBRIF1  0xffc03224   
#define                       CAN1_MBIM1  0xffc03228   
#define                        CAN1_RFH1  0xffc0322c   
#define                       CAN1_OPSS1  0xffc03230   


#define                         CAN1_MC2  0xffc03240   
#define                         CAN1_MD2  0xffc03244   
#define                        CAN1_TRS2  0xffc03248   
#define                        CAN1_TRR2  0xffc0324c   
#define                         CAN1_TA2  0xffc03250   
#define                         CAN1_AA2  0xffc03254   
#define                        CAN1_RMP2  0xffc03258   
#define                        CAN1_RML2  0xffc0325c   
#define                      CAN1_MBTIF2  0xffc03260   
#define                      CAN1_MBRIF2  0xffc03264   
#define                       CAN1_MBIM2  0xffc03268   
#define                        CAN1_RFH2  0xffc0326c   
#define                       CAN1_OPSS2  0xffc03270   


#define                       CAN1_CLOCK  0xffc03280   
#define                      CAN1_TIMING  0xffc03284   
#define                       CAN1_DEBUG  0xffc03288   
#define                      CAN1_STATUS  0xffc0328c   
#define                         CAN1_CEC  0xffc03290   
#define                         CAN1_GIS  0xffc03294   
#define                         CAN1_GIM  0xffc03298   
#define                         CAN1_GIF  0xffc0329c   
#define                     CAN1_CONTROL  0xffc032a0   
#define                        CAN1_INTR  0xffc032a4   
#define                        CAN1_MBTD  0xffc032ac   
#define                         CAN1_EWR  0xffc032b0   
#define                         CAN1_ESR  0xffc032b4   
#define                       CAN1_UCCNT  0xffc032c4   
#define                        CAN1_UCRC  0xffc032c8   
#define                       CAN1_UCCNF  0xffc032cc   


#define                       CAN1_AM00L  0xffc03300   
#define                       CAN1_AM00H  0xffc03304   
#define                       CAN1_AM01L  0xffc03308   
#define                       CAN1_AM01H  0xffc0330c   
#define                       CAN1_AM02L  0xffc03310   
#define                       CAN1_AM02H  0xffc03314   
#define                       CAN1_AM03L  0xffc03318   
#define                       CAN1_AM03H  0xffc0331c   
#define                       CAN1_AM04L  0xffc03320   
#define                       CAN1_AM04H  0xffc03324   
#define                       CAN1_AM05L  0xffc03328   
#define                       CAN1_AM05H  0xffc0332c   
#define                       CAN1_AM06L  0xffc03330   
#define                       CAN1_AM06H  0xffc03334   
#define                       CAN1_AM07L  0xffc03338   
#define                       CAN1_AM07H  0xffc0333c   
#define                       CAN1_AM08L  0xffc03340   
#define                       CAN1_AM08H  0xffc03344   
#define                       CAN1_AM09L  0xffc03348   
#define                       CAN1_AM09H  0xffc0334c   
#define                       CAN1_AM10L  0xffc03350   
#define                       CAN1_AM10H  0xffc03354   
#define                       CAN1_AM11L  0xffc03358   
#define                       CAN1_AM11H  0xffc0335c   
#define                       CAN1_AM12L  0xffc03360   
#define                       CAN1_AM12H  0xffc03364   
#define                       CAN1_AM13L  0xffc03368   
#define                       CAN1_AM13H  0xffc0336c   
#define                       CAN1_AM14L  0xffc03370   
#define                       CAN1_AM14H  0xffc03374   
#define                       CAN1_AM15L  0xffc03378   
#define                       CAN1_AM15H  0xffc0337c   


#define                       CAN1_AM16L  0xffc03380   
#define                       CAN1_AM16H  0xffc03384   
#define                       CAN1_AM17L  0xffc03388   
#define                       CAN1_AM17H  0xffc0338c   
#define                       CAN1_AM18L  0xffc03390   
#define                       CAN1_AM18H  0xffc03394   
#define                       CAN1_AM19L  0xffc03398   
#define                       CAN1_AM19H  0xffc0339c   
#define                       CAN1_AM20L  0xffc033a0   
#define                       CAN1_AM20H  0xffc033a4   
#define                       CAN1_AM21L  0xffc033a8   
#define                       CAN1_AM21H  0xffc033ac   
#define                       CAN1_AM22L  0xffc033b0   
#define                       CAN1_AM22H  0xffc033b4   
#define                       CAN1_AM23L  0xffc033b8   
#define                       CAN1_AM23H  0xffc033bc   
#define                       CAN1_AM24L  0xffc033c0   
#define                       CAN1_AM24H  0xffc033c4   
#define                       CAN1_AM25L  0xffc033c8   
#define                       CAN1_AM25H  0xffc033cc   
#define                       CAN1_AM26L  0xffc033d0   
#define                       CAN1_AM26H  0xffc033d4   
#define                       CAN1_AM27L  0xffc033d8   
#define                       CAN1_AM27H  0xffc033dc   
#define                       CAN1_AM28L  0xffc033e0   
#define                       CAN1_AM28H  0xffc033e4   
#define                       CAN1_AM29L  0xffc033e8   
#define                       CAN1_AM29H  0xffc033ec   
#define                       CAN1_AM30L  0xffc033f0   
#define                       CAN1_AM30H  0xffc033f4   
#define                       CAN1_AM31L  0xffc033f8   
#define                       CAN1_AM31H  0xffc033fc   


#define                  CAN1_MB00_DATA0  0xffc03400   
#define                  CAN1_MB00_DATA1  0xffc03404   
#define                  CAN1_MB00_DATA2  0xffc03408   
#define                  CAN1_MB00_DATA3  0xffc0340c   
#define                 CAN1_MB00_LENGTH  0xffc03410   
#define              CAN1_MB00_TIMESTAMP  0xffc03414   
#define                    CAN1_MB00_ID0  0xffc03418   
#define                    CAN1_MB00_ID1  0xffc0341c   
#define                  CAN1_MB01_DATA0  0xffc03420   
#define                  CAN1_MB01_DATA1  0xffc03424   
#define                  CAN1_MB01_DATA2  0xffc03428   
#define                  CAN1_MB01_DATA3  0xffc0342c   
#define                 CAN1_MB01_LENGTH  0xffc03430   
#define              CAN1_MB01_TIMESTAMP  0xffc03434   
#define                    CAN1_MB01_ID0  0xffc03438   
#define                    CAN1_MB01_ID1  0xffc0343c   
#define                  CAN1_MB02_DATA0  0xffc03440   
#define                  CAN1_MB02_DATA1  0xffc03444   
#define                  CAN1_MB02_DATA2  0xffc03448   
#define                  CAN1_MB02_DATA3  0xffc0344c   
#define                 CAN1_MB02_LENGTH  0xffc03450   
#define              CAN1_MB02_TIMESTAMP  0xffc03454   
#define                    CAN1_MB02_ID0  0xffc03458   
#define                    CAN1_MB02_ID1  0xffc0345c   
#define                  CAN1_MB03_DATA0  0xffc03460   
#define                  CAN1_MB03_DATA1  0xffc03464   
#define                  CAN1_MB03_DATA2  0xffc03468   
#define                  CAN1_MB03_DATA3  0xffc0346c   
#define                 CAN1_MB03_LENGTH  0xffc03470   
#define              CAN1_MB03_TIMESTAMP  0xffc03474   
#define                    CAN1_MB03_ID0  0xffc03478   
#define                    CAN1_MB03_ID1  0xffc0347c   
#define                  CAN1_MB04_DATA0  0xffc03480   
#define                  CAN1_MB04_DATA1  0xffc03484   
#define                  CAN1_MB04_DATA2  0xffc03488   
#define                  CAN1_MB04_DATA3  0xffc0348c   
#define                 CAN1_MB04_LENGTH  0xffc03490   
#define              CAN1_MB04_TIMESTAMP  0xffc03494   
#define                    CAN1_MB04_ID0  0xffc03498   
#define                    CAN1_MB04_ID1  0xffc0349c   
#define                  CAN1_MB05_DATA0  0xffc034a0   
#define                  CAN1_MB05_DATA1  0xffc034a4   
#define                  CAN1_MB05_DATA2  0xffc034a8   
#define                  CAN1_MB05_DATA3  0xffc034ac   
#define                 CAN1_MB05_LENGTH  0xffc034b0   
#define              CAN1_MB05_TIMESTAMP  0xffc034b4   
#define                    CAN1_MB05_ID0  0xffc034b8   
#define                    CAN1_MB05_ID1  0xffc034bc   
#define                  CAN1_MB06_DATA0  0xffc034c0   
#define                  CAN1_MB06_DATA1  0xffc034c4   
#define                  CAN1_MB06_DATA2  0xffc034c8   
#define                  CAN1_MB06_DATA3  0xffc034cc   
#define                 CAN1_MB06_LENGTH  0xffc034d0   
#define              CAN1_MB06_TIMESTAMP  0xffc034d4   
#define                    CAN1_MB06_ID0  0xffc034d8   
#define                    CAN1_MB06_ID1  0xffc034dc   
#define                  CAN1_MB07_DATA0  0xffc034e0   
#define                  CAN1_MB07_DATA1  0xffc034e4   
#define                  CAN1_MB07_DATA2  0xffc034e8   
#define                  CAN1_MB07_DATA3  0xffc034ec   
#define                 CAN1_MB07_LENGTH  0xffc034f0   
#define              CAN1_MB07_TIMESTAMP  0xffc034f4   
#define                    CAN1_MB07_ID0  0xffc034f8   
#define                    CAN1_MB07_ID1  0xffc034fc   
#define                  CAN1_MB08_DATA0  0xffc03500   
#define                  CAN1_MB08_DATA1  0xffc03504   
#define                  CAN1_MB08_DATA2  0xffc03508   
#define                  CAN1_MB08_DATA3  0xffc0350c   
#define                 CAN1_MB08_LENGTH  0xffc03510   
#define              CAN1_MB08_TIMESTAMP  0xffc03514   
#define                    CAN1_MB08_ID0  0xffc03518   
#define                    CAN1_MB08_ID1  0xffc0351c   
#define                  CAN1_MB09_DATA0  0xffc03520   
#define                  CAN1_MB09_DATA1  0xffc03524   
#define                  CAN1_MB09_DATA2  0xffc03528   
#define                  CAN1_MB09_DATA3  0xffc0352c   
#define                 CAN1_MB09_LENGTH  0xffc03530   
#define              CAN1_MB09_TIMESTAMP  0xffc03534   
#define                    CAN1_MB09_ID0  0xffc03538   
#define                    CAN1_MB09_ID1  0xffc0353c   
#define                  CAN1_MB10_DATA0  0xffc03540   
#define                  CAN1_MB10_DATA1  0xffc03544   
#define                  CAN1_MB10_DATA2  0xffc03548   
#define                  CAN1_MB10_DATA3  0xffc0354c   
#define                 CAN1_MB10_LENGTH  0xffc03550   
#define              CAN1_MB10_TIMESTAMP  0xffc03554   
#define                    CAN1_MB10_ID0  0xffc03558   
#define                    CAN1_MB10_ID1  0xffc0355c   
#define                  CAN1_MB11_DATA0  0xffc03560   
#define                  CAN1_MB11_DATA1  0xffc03564   
#define                  CAN1_MB11_DATA2  0xffc03568   
#define                  CAN1_MB11_DATA3  0xffc0356c   
#define                 CAN1_MB11_LENGTH  0xffc03570   
#define              CAN1_MB11_TIMESTAMP  0xffc03574   
#define                    CAN1_MB11_ID0  0xffc03578   
#define                    CAN1_MB11_ID1  0xffc0357c   
#define                  CAN1_MB12_DATA0  0xffc03580   
#define                  CAN1_MB12_DATA1  0xffc03584   
#define                  CAN1_MB12_DATA2  0xffc03588   
#define                  CAN1_MB12_DATA3  0xffc0358c   
#define                 CAN1_MB12_LENGTH  0xffc03590   
#define              CAN1_MB12_TIMESTAMP  0xffc03594   
#define                    CAN1_MB12_ID0  0xffc03598   
#define                    CAN1_MB12_ID1  0xffc0359c   
#define                  CAN1_MB13_DATA0  0xffc035a0   
#define                  CAN1_MB13_DATA1  0xffc035a4   
#define                  CAN1_MB13_DATA2  0xffc035a8   
#define                  CAN1_MB13_DATA3  0xffc035ac   
#define                 CAN1_MB13_LENGTH  0xffc035b0   
#define              CAN1_MB13_TIMESTAMP  0xffc035b4   
#define                    CAN1_MB13_ID0  0xffc035b8   
#define                    CAN1_MB13_ID1  0xffc035bc   
#define                  CAN1_MB14_DATA0  0xffc035c0   
#define                  CAN1_MB14_DATA1  0xffc035c4   
#define                  CAN1_MB14_DATA2  0xffc035c8   
#define                  CAN1_MB14_DATA3  0xffc035cc   
#define                 CAN1_MB14_LENGTH  0xffc035d0   
#define              CAN1_MB14_TIMESTAMP  0xffc035d4   
#define                    CAN1_MB14_ID0  0xffc035d8   
#define                    CAN1_MB14_ID1  0xffc035dc   
#define                  CAN1_MB15_DATA0  0xffc035e0   
#define                  CAN1_MB15_DATA1  0xffc035e4   
#define                  CAN1_MB15_DATA2  0xffc035e8   
#define                  CAN1_MB15_DATA3  0xffc035ec   
#define                 CAN1_MB15_LENGTH  0xffc035f0   
#define              CAN1_MB15_TIMESTAMP  0xffc035f4   
#define                    CAN1_MB15_ID0  0xffc035f8   
#define                    CAN1_MB15_ID1  0xffc035fc   


#define                  CAN1_MB16_DATA0  0xffc03600   
#define                  CAN1_MB16_DATA1  0xffc03604   
#define                  CAN1_MB16_DATA2  0xffc03608   
#define                  CAN1_MB16_DATA3  0xffc0360c   
#define                 CAN1_MB16_LENGTH  0xffc03610   
#define              CAN1_MB16_TIMESTAMP  0xffc03614   
#define                    CAN1_MB16_ID0  0xffc03618   
#define                    CAN1_MB16_ID1  0xffc0361c   
#define                  CAN1_MB17_DATA0  0xffc03620   
#define                  CAN1_MB17_DATA1  0xffc03624   
#define                  CAN1_MB17_DATA2  0xffc03628   
#define                  CAN1_MB17_DATA3  0xffc0362c   
#define                 CAN1_MB17_LENGTH  0xffc03630   
#define              CAN1_MB17_TIMESTAMP  0xffc03634   
#define                    CAN1_MB17_ID0  0xffc03638   
#define                    CAN1_MB17_ID1  0xffc0363c   
#define                  CAN1_MB18_DATA0  0xffc03640   
#define                  CAN1_MB18_DATA1  0xffc03644   
#define                  CAN1_MB18_DATA2  0xffc03648   
#define                  CAN1_MB18_DATA3  0xffc0364c   
#define                 CAN1_MB18_LENGTH  0xffc03650   
#define              CAN1_MB18_TIMESTAMP  0xffc03654   
#define                    CAN1_MB18_ID0  0xffc03658   
#define                    CAN1_MB18_ID1  0xffc0365c   
#define                  CAN1_MB19_DATA0  0xffc03660   
#define                  CAN1_MB19_DATA1  0xffc03664   
#define                  CAN1_MB19_DATA2  0xffc03668   
#define                  CAN1_MB19_DATA3  0xffc0366c   
#define                 CAN1_MB19_LENGTH  0xffc03670   
#define              CAN1_MB19_TIMESTAMP  0xffc03674   
#define                    CAN1_MB19_ID0  0xffc03678   
#define                    CAN1_MB19_ID1  0xffc0367c   
#define                  CAN1_MB20_DATA0  0xffc03680   
#define                  CAN1_MB20_DATA1  0xffc03684   
#define                  CAN1_MB20_DATA2  0xffc03688   
#define                  CAN1_MB20_DATA3  0xffc0368c   
#define                 CAN1_MB20_LENGTH  0xffc03690   
#define              CAN1_MB20_TIMESTAMP  0xffc03694   
#define                    CAN1_MB20_ID0  0xffc03698   
#define                    CAN1_MB20_ID1  0xffc0369c   
#define                  CAN1_MB21_DATA0  0xffc036a0   
#define                  CAN1_MB21_DATA1  0xffc036a4   
#define                  CAN1_MB21_DATA2  0xffc036a8   
#define                  CAN1_MB21_DATA3  0xffc036ac   
#define                 CAN1_MB21_LENGTH  0xffc036b0   
#define              CAN1_MB21_TIMESTAMP  0xffc036b4   
#define                    CAN1_MB21_ID0  0xffc036b8   
#define                    CAN1_MB21_ID1  0xffc036bc   
#define                  CAN1_MB22_DATA0  0xffc036c0   
#define                  CAN1_MB22_DATA1  0xffc036c4   
#define                  CAN1_MB22_DATA2  0xffc036c8   
#define                  CAN1_MB22_DATA3  0xffc036cc   
#define                 CAN1_MB22_LENGTH  0xffc036d0   
#define              CAN1_MB22_TIMESTAMP  0xffc036d4   
#define                    CAN1_MB22_ID0  0xffc036d8   
#define                    CAN1_MB22_ID1  0xffc036dc   
#define                  CAN1_MB23_DATA0  0xffc036e0   
#define                  CAN1_MB23_DATA1  0xffc036e4   
#define                  CAN1_MB23_DATA2  0xffc036e8   
#define                  CAN1_MB23_DATA3  0xffc036ec   
#define                 CAN1_MB23_LENGTH  0xffc036f0   
#define              CAN1_MB23_TIMESTAMP  0xffc036f4   
#define                    CAN1_MB23_ID0  0xffc036f8   
#define                    CAN1_MB23_ID1  0xffc036fc   
#define                  CAN1_MB24_DATA0  0xffc03700   
#define                  CAN1_MB24_DATA1  0xffc03704   
#define                  CAN1_MB24_DATA2  0xffc03708   
#define                  CAN1_MB24_DATA3  0xffc0370c   
#define                 CAN1_MB24_LENGTH  0xffc03710   
#define              CAN1_MB24_TIMESTAMP  0xffc03714   
#define                    CAN1_MB24_ID0  0xffc03718   
#define                    CAN1_MB24_ID1  0xffc0371c   
#define                  CAN1_MB25_DATA0  0xffc03720   
#define                  CAN1_MB25_DATA1  0xffc03724   
#define                  CAN1_MB25_DATA2  0xffc03728   
#define                  CAN1_MB25_DATA3  0xffc0372c   
#define                 CAN1_MB25_LENGTH  0xffc03730   
#define              CAN1_MB25_TIMESTAMP  0xffc03734   
#define                    CAN1_MB25_ID0  0xffc03738   
#define                    CAN1_MB25_ID1  0xffc0373c   
#define                  CAN1_MB26_DATA0  0xffc03740   
#define                  CAN1_MB26_DATA1  0xffc03744   
#define                  CAN1_MB26_DATA2  0xffc03748   
#define                  CAN1_MB26_DATA3  0xffc0374c   
#define                 CAN1_MB26_LENGTH  0xffc03750   
#define              CAN1_MB26_TIMESTAMP  0xffc03754   
#define                    CAN1_MB26_ID0  0xffc03758   
#define                    CAN1_MB26_ID1  0xffc0375c   
#define                  CAN1_MB27_DATA0  0xffc03760   
#define                  CAN1_MB27_DATA1  0xffc03764   
#define                  CAN1_MB27_DATA2  0xffc03768   
#define                  CAN1_MB27_DATA3  0xffc0376c   
#define                 CAN1_MB27_LENGTH  0xffc03770   
#define              CAN1_MB27_TIMESTAMP  0xffc03774   
#define                    CAN1_MB27_ID0  0xffc03778   
#define                    CAN1_MB27_ID1  0xffc0377c   
#define                  CAN1_MB28_DATA0  0xffc03780   
#define                  CAN1_MB28_DATA1  0xffc03784   
#define                  CAN1_MB28_DATA2  0xffc03788   
#define                  CAN1_MB28_DATA3  0xffc0378c   
#define                 CAN1_MB28_LENGTH  0xffc03790   
#define              CAN1_MB28_TIMESTAMP  0xffc03794   
#define                    CAN1_MB28_ID0  0xffc03798   
#define                    CAN1_MB28_ID1  0xffc0379c   
#define                  CAN1_MB29_DATA0  0xffc037a0   
#define                  CAN1_MB29_DATA1  0xffc037a4   
#define                  CAN1_MB29_DATA2  0xffc037a8   
#define                  CAN1_MB29_DATA3  0xffc037ac   
#define                 CAN1_MB29_LENGTH  0xffc037b0   
#define              CAN1_MB29_TIMESTAMP  0xffc037b4   
#define                    CAN1_MB29_ID0  0xffc037b8   
#define                    CAN1_MB29_ID1  0xffc037bc   
#define                  CAN1_MB30_DATA0  0xffc037c0   
#define                  CAN1_MB30_DATA1  0xffc037c4   
#define                  CAN1_MB30_DATA2  0xffc037c8   
#define                  CAN1_MB30_DATA3  0xffc037cc   
#define                 CAN1_MB30_LENGTH  0xffc037d0   
#define              CAN1_MB30_TIMESTAMP  0xffc037d4   
#define                    CAN1_MB30_ID0  0xffc037d8   
#define                    CAN1_MB30_ID1  0xffc037dc   
#define                  CAN1_MB31_DATA0  0xffc037e0   
#define                  CAN1_MB31_DATA1  0xffc037e4   
#define                  CAN1_MB31_DATA2  0xffc037e8   
#define                  CAN1_MB31_DATA3  0xffc037ec   
#define                 CAN1_MB31_LENGTH  0xffc037f0   
#define              CAN1_MB31_TIMESTAMP  0xffc037f4   
#define                    CAN1_MB31_ID0  0xffc037f8   
#define                    CAN1_MB31_ID1  0xffc037fc   


#define                     HOST_CONTROL  0xffc03a00   
#define                      HOST_STATUS  0xffc03a04   
#define                     HOST_TIMEOUT  0xffc03a08   


#define                         PIXC_CTL  0xffc04400   
#define                         PIXC_PPL  0xffc04404   
#define                         PIXC_LPF  0xffc04408   
#define                     PIXC_AHSTART  0xffc0440c   
#define                       PIXC_AHEND  0xffc04410   
#define                     PIXC_AVSTART  0xffc04414   
#define                       PIXC_AVEND  0xffc04418   
#define                     PIXC_ATRANSP  0xffc0441c   
#define                     PIXC_BHSTART  0xffc04420   
#define                       PIXC_BHEND  0xffc04424   
#define                     PIXC_BVSTART  0xffc04428   
#define                       PIXC_BVEND  0xffc0442c   
#define                     PIXC_BTRANSP  0xffc04430   
#define                    PIXC_INTRSTAT  0xffc0443c   
#define                       PIXC_RYCON  0xffc04440   
#define                       PIXC_GUCON  0xffc04444   
#define                       PIXC_BVCON  0xffc04448   
#define                      PIXC_CCBIAS  0xffc0444c   
#define                          PIXC_TC  0xffc04450   


#define                   HMDMA0_CONTROL  0xffc04500   
#define                    HMDMA0_ECINIT  0xffc04504   
#define                    HMDMA0_BCINIT  0xffc04508   
#define                  HMDMA0_ECURGENT  0xffc0450c   
#define                HMDMA0_ECOVERFLOW  0xffc04510   
#define                    HMDMA0_ECOUNT  0xffc04514   
#define                    HMDMA0_BCOUNT  0xffc04518   


#define                   HMDMA1_CONTROL  0xffc04540   
#define                    HMDMA1_ECINIT  0xffc04544   
#define                    HMDMA1_BCINIT  0xffc04548   
#define                  HMDMA1_ECURGENT  0xffc0454c   
#define                HMDMA1_ECOVERFLOW  0xffc04550   
#define                    HMDMA1_ECOUNT  0xffc04554   
#define                    HMDMA1_BCOUNT  0xffc04558   




#define                   PIXC_EN  0x1        
#define                  OVR_A_EN  0x2        
#define                  OVR_B_EN  0x4        
#define                  IMG_FORM  0x8        
#define                  OVR_FORM  0x10       
#define                  OUT_FORM  0x20       
#define                   UDS_MOD  0x40       
#define                     TC_EN  0x80       
#define                  IMG_STAT  0x300      
#define                  OVR_STAT  0xc00      
#define                    WM_LVL  0x3000     


#define                  A_HSTART  0xfff      


#define                    A_HEND  0xfff      


#define                  A_VSTART  0x3ff      


#define                    A_VEND  0x3ff      


#define                  A_TRANSP  0xf        


#define                  B_HSTART  0xfff      


#define                    B_HEND  0xfff      


#define                  B_VSTART  0x3ff      


#define                    B_VEND  0x3ff      


#define                  B_TRANSP  0xf        


#define                OVR_INT_EN  0x1        
#define                FRM_INT_EN  0x2        
#define              OVR_INT_STAT  0x4        
#define              FRM_INT_STAT  0x8        


#define                       A11  0x3ff      
#define                       A12  0xffc00    
#define                       A13  0x3ff00000 
#define                  RY_MULT4  0x40000000 


#define                       A21  0x3ff      
#define                       A22  0xffc00    
#define                       A23  0x3ff00000 
#define                  GU_MULT4  0x40000000 


#define                       A31  0x3ff      
#define                       A32  0xffc00    
#define                       A33  0x3ff00000 
#define                  BV_MULT4  0x40000000 


#define                       A14  0x3ff      
#define                       A24  0xffc00    
#define                       A34  0x3ff00000 


#define                  RY_TRANS  0xff       
#define                  GU_TRANS  0xff00     
#define                  BV_TRANS  0xff0000   


#define                   HOST_EN  0x1        
#define                  HOST_END  0x2        
#define                 DATA_SIZE  0x4        
#define                  HOST_RST  0x8        
#define                  HRDY_OVR  0x20       
#define                  INT_MODE  0x40       
#define                     BT_EN  0x80       
#define                       EHW  0x100      
#define                       EHR  0x200      
#define                       BDR  0x400      


#define                 DMA_READY  0x1        
#define                  FIFOFULL  0x2        
#define                 FIFOEMPTY  0x4        
#define              DMA_COMPLETE  0x8        
#define                      HSHK  0x10       
#define                 HSTIMEOUT  0x20       
#define                      HIRQ  0x40       
#define                ALLOW_CNFG  0x80       
#define                   DMA_DIR  0x100      
#define                       BTE  0x200      


#define             COUNT_TIMEOUT  0x7ff      


#define                    TIMEN8  0x1        
#define                    TIMEN9  0x2        
#define                   TIMEN10  0x4        


#define                   TIMDIS8  0x1        
#define                   TIMDIS9  0x2        
#define                  TIMDIS10  0x4        


#define                    TIMIL8  0x1        
#define                    TIMIL9  0x2        
#define                   TIMIL10  0x4        
#define                 TOVF_ERR8  0x10       
#define                 TOVF_ERR9  0x20       
#define                TOVF_ERR10  0x40       
#define                     TRUN8  0x1000     
#define                     TRUN9  0x2000     
#define                    TRUN10  0x4000     


#endif 
