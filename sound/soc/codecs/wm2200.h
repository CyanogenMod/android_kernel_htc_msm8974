/*
 * wm2200.h - WM2200 audio codec interface
 *
 * Copyright 2012 Wolfson Microelectronics PLC.
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#ifndef _WM2200_H
#define _WM2200_H

#define WM2200_CLK_SYSCLK 1

#define WM2200_CLKSRC_MCLK1  0
#define WM2200_CLKSRC_MCLK2  1
#define WM2200_CLKSRC_FLL    4
#define WM2200_CLKSRC_BCLK1  8

#define WM2200_FLL_SRC_MCLK1 0
#define WM2200_FLL_SRC_MCLK2 1
#define WM2200_FLL_SRC_BCLK  2

#define WM2200_SOFTWARE_RESET                   0x00
#define WM2200_DEVICE_REVISION                  0x01
#define WM2200_TONE_GENERATOR_1                 0x0B
#define WM2200_CLOCKING_3                       0x102
#define WM2200_CLOCKING_4                       0x103
#define WM2200_FLL_CONTROL_1                    0x111
#define WM2200_FLL_CONTROL_2                    0x112
#define WM2200_FLL_CONTROL_3                    0x113
#define WM2200_FLL_CONTROL_4                    0x114
#define WM2200_FLL_CONTROL_6                    0x116
#define WM2200_FLL_CONTROL_7                    0x117
#define WM2200_FLL_EFS_1                        0x119
#define WM2200_FLL_EFS_2                        0x11A
#define WM2200_MIC_CHARGE_PUMP_1                0x200
#define WM2200_MIC_CHARGE_PUMP_2                0x201
#define WM2200_DM_CHARGE_PUMP_1                 0x202
#define WM2200_MIC_BIAS_CTRL_1                  0x20C
#define WM2200_MIC_BIAS_CTRL_2                  0x20D
#define WM2200_EAR_PIECE_CTRL_1                 0x20F
#define WM2200_EAR_PIECE_CTRL_2                 0x210
#define WM2200_INPUT_ENABLES                    0x301
#define WM2200_IN1L_CONTROL                     0x302
#define WM2200_IN1R_CONTROL                     0x303
#define WM2200_IN2L_CONTROL                     0x304
#define WM2200_IN2R_CONTROL                     0x305
#define WM2200_IN3L_CONTROL                     0x306
#define WM2200_IN3R_CONTROL                     0x307
#define WM2200_RXANC_SRC                        0x30A
#define WM2200_INPUT_VOLUME_RAMP                0x30B
#define WM2200_ADC_DIGITAL_VOLUME_1L            0x30C
#define WM2200_ADC_DIGITAL_VOLUME_1R            0x30D
#define WM2200_ADC_DIGITAL_VOLUME_2L            0x30E
#define WM2200_ADC_DIGITAL_VOLUME_2R            0x30F
#define WM2200_ADC_DIGITAL_VOLUME_3L            0x310
#define WM2200_ADC_DIGITAL_VOLUME_3R            0x311
#define WM2200_OUTPUT_ENABLES                   0x400
#define WM2200_DAC_VOLUME_LIMIT_1L              0x401
#define WM2200_DAC_VOLUME_LIMIT_1R              0x402
#define WM2200_DAC_VOLUME_LIMIT_2L              0x403
#define WM2200_DAC_VOLUME_LIMIT_2R              0x404
#define WM2200_DAC_AEC_CONTROL_1                0x409
#define WM2200_OUTPUT_VOLUME_RAMP               0x40A
#define WM2200_DAC_DIGITAL_VOLUME_1L            0x40B
#define WM2200_DAC_DIGITAL_VOLUME_1R            0x40C
#define WM2200_DAC_DIGITAL_VOLUME_2L            0x40D
#define WM2200_DAC_DIGITAL_VOLUME_2R            0x40E
#define WM2200_PDM_1                            0x417
#define WM2200_PDM_2                            0x418
#define WM2200_AUDIO_IF_1_1                     0x500
#define WM2200_AUDIO_IF_1_2                     0x501
#define WM2200_AUDIO_IF_1_3                     0x502
#define WM2200_AUDIO_IF_1_4                     0x503
#define WM2200_AUDIO_IF_1_5                     0x504
#define WM2200_AUDIO_IF_1_6                     0x505
#define WM2200_AUDIO_IF_1_7                     0x506
#define WM2200_AUDIO_IF_1_8                     0x507
#define WM2200_AUDIO_IF_1_9                     0x508
#define WM2200_AUDIO_IF_1_10                    0x509
#define WM2200_AUDIO_IF_1_11                    0x50A
#define WM2200_AUDIO_IF_1_12                    0x50B
#define WM2200_AUDIO_IF_1_13                    0x50C
#define WM2200_AUDIO_IF_1_14                    0x50D
#define WM2200_AUDIO_IF_1_15                    0x50E
#define WM2200_AUDIO_IF_1_16                    0x50F
#define WM2200_AUDIO_IF_1_17                    0x510
#define WM2200_AUDIO_IF_1_18                    0x511
#define WM2200_AUDIO_IF_1_19                    0x512
#define WM2200_AUDIO_IF_1_20                    0x513
#define WM2200_AUDIO_IF_1_21                    0x514
#define WM2200_AUDIO_IF_1_22                    0x515
#define WM2200_OUT1LMIX_INPUT_1_SOURCE          0x600
#define WM2200_OUT1LMIX_INPUT_1_VOLUME          0x601
#define WM2200_OUT1LMIX_INPUT_2_SOURCE          0x602
#define WM2200_OUT1LMIX_INPUT_2_VOLUME          0x603
#define WM2200_OUT1LMIX_INPUT_3_SOURCE          0x604
#define WM2200_OUT1LMIX_INPUT_3_VOLUME          0x605
#define WM2200_OUT1LMIX_INPUT_4_SOURCE          0x606
#define WM2200_OUT1LMIX_INPUT_4_VOLUME          0x607
#define WM2200_OUT1RMIX_INPUT_1_SOURCE          0x608
#define WM2200_OUT1RMIX_INPUT_1_VOLUME          0x609
#define WM2200_OUT1RMIX_INPUT_2_SOURCE          0x60A
#define WM2200_OUT1RMIX_INPUT_2_VOLUME          0x60B
#define WM2200_OUT1RMIX_INPUT_3_SOURCE          0x60C
#define WM2200_OUT1RMIX_INPUT_3_VOLUME          0x60D
#define WM2200_OUT1RMIX_INPUT_4_SOURCE          0x60E
#define WM2200_OUT1RMIX_INPUT_4_VOLUME          0x60F
#define WM2200_OUT2LMIX_INPUT_1_SOURCE          0x610
#define WM2200_OUT2LMIX_INPUT_1_VOLUME          0x611
#define WM2200_OUT2LMIX_INPUT_2_SOURCE          0x612
#define WM2200_OUT2LMIX_INPUT_2_VOLUME          0x613
#define WM2200_OUT2LMIX_INPUT_3_SOURCE          0x614
#define WM2200_OUT2LMIX_INPUT_3_VOLUME          0x615
#define WM2200_OUT2LMIX_INPUT_4_SOURCE          0x616
#define WM2200_OUT2LMIX_INPUT_4_VOLUME          0x617
#define WM2200_OUT2RMIX_INPUT_1_SOURCE          0x618
#define WM2200_OUT2RMIX_INPUT_1_VOLUME          0x619
#define WM2200_OUT2RMIX_INPUT_2_SOURCE          0x61A
#define WM2200_OUT2RMIX_INPUT_2_VOLUME          0x61B
#define WM2200_OUT2RMIX_INPUT_3_SOURCE          0x61C
#define WM2200_OUT2RMIX_INPUT_3_VOLUME          0x61D
#define WM2200_OUT2RMIX_INPUT_4_SOURCE          0x61E
#define WM2200_OUT2RMIX_INPUT_4_VOLUME          0x61F
#define WM2200_AIF1TX1MIX_INPUT_1_SOURCE        0x620
#define WM2200_AIF1TX1MIX_INPUT_1_VOLUME        0x621
#define WM2200_AIF1TX1MIX_INPUT_2_SOURCE        0x622
#define WM2200_AIF1TX1MIX_INPUT_2_VOLUME        0x623
#define WM2200_AIF1TX1MIX_INPUT_3_SOURCE        0x624
#define WM2200_AIF1TX1MIX_INPUT_3_VOLUME        0x625
#define WM2200_AIF1TX1MIX_INPUT_4_SOURCE        0x626
#define WM2200_AIF1TX1MIX_INPUT_4_VOLUME        0x627
#define WM2200_AIF1TX2MIX_INPUT_1_SOURCE        0x628
#define WM2200_AIF1TX2MIX_INPUT_1_VOLUME        0x629
#define WM2200_AIF1TX2MIX_INPUT_2_SOURCE        0x62A
#define WM2200_AIF1TX2MIX_INPUT_2_VOLUME        0x62B
#define WM2200_AIF1TX2MIX_INPUT_3_SOURCE        0x62C
#define WM2200_AIF1TX2MIX_INPUT_3_VOLUME        0x62D
#define WM2200_AIF1TX2MIX_INPUT_4_SOURCE        0x62E
#define WM2200_AIF1TX2MIX_INPUT_4_VOLUME        0x62F
#define WM2200_AIF1TX3MIX_INPUT_1_SOURCE        0x630
#define WM2200_AIF1TX3MIX_INPUT_1_VOLUME        0x631
#define WM2200_AIF1TX3MIX_INPUT_2_SOURCE        0x632
#define WM2200_AIF1TX3MIX_INPUT_2_VOLUME        0x633
#define WM2200_AIF1TX3MIX_INPUT_3_SOURCE        0x634
#define WM2200_AIF1TX3MIX_INPUT_3_VOLUME        0x635
#define WM2200_AIF1TX3MIX_INPUT_4_SOURCE        0x636
#define WM2200_AIF1TX3MIX_INPUT_4_VOLUME        0x637
#define WM2200_AIF1TX4MIX_INPUT_1_SOURCE        0x638
#define WM2200_AIF1TX4MIX_INPUT_1_VOLUME        0x639
#define WM2200_AIF1TX4MIX_INPUT_2_SOURCE        0x63A
#define WM2200_AIF1TX4MIX_INPUT_2_VOLUME        0x63B
#define WM2200_AIF1TX4MIX_INPUT_3_SOURCE        0x63C
#define WM2200_AIF1TX4MIX_INPUT_3_VOLUME        0x63D
#define WM2200_AIF1TX4MIX_INPUT_4_SOURCE        0x63E
#define WM2200_AIF1TX4MIX_INPUT_4_VOLUME        0x63F
#define WM2200_AIF1TX5MIX_INPUT_1_SOURCE        0x640
#define WM2200_AIF1TX5MIX_INPUT_1_VOLUME        0x641
#define WM2200_AIF1TX5MIX_INPUT_2_SOURCE        0x642
#define WM2200_AIF1TX5MIX_INPUT_2_VOLUME        0x643
#define WM2200_AIF1TX5MIX_INPUT_3_SOURCE        0x644
#define WM2200_AIF1TX5MIX_INPUT_3_VOLUME        0x645
#define WM2200_AIF1TX5MIX_INPUT_4_SOURCE        0x646
#define WM2200_AIF1TX5MIX_INPUT_4_VOLUME        0x647
#define WM2200_AIF1TX6MIX_INPUT_1_SOURCE        0x648
#define WM2200_AIF1TX6MIX_INPUT_1_VOLUME        0x649
#define WM2200_AIF1TX6MIX_INPUT_2_SOURCE        0x64A
#define WM2200_AIF1TX6MIX_INPUT_2_VOLUME        0x64B
#define WM2200_AIF1TX6MIX_INPUT_3_SOURCE        0x64C
#define WM2200_AIF1TX6MIX_INPUT_3_VOLUME        0x64D
#define WM2200_AIF1TX6MIX_INPUT_4_SOURCE        0x64E
#define WM2200_AIF1TX6MIX_INPUT_4_VOLUME        0x64F
#define WM2200_EQLMIX_INPUT_1_SOURCE            0x650
#define WM2200_EQLMIX_INPUT_1_VOLUME            0x651
#define WM2200_EQLMIX_INPUT_2_SOURCE            0x652
#define WM2200_EQLMIX_INPUT_2_VOLUME            0x653
#define WM2200_EQLMIX_INPUT_3_SOURCE            0x654
#define WM2200_EQLMIX_INPUT_3_VOLUME            0x655
#define WM2200_EQLMIX_INPUT_4_SOURCE            0x656
#define WM2200_EQLMIX_INPUT_4_VOLUME            0x657
#define WM2200_EQRMIX_INPUT_1_SOURCE            0x658
#define WM2200_EQRMIX_INPUT_1_VOLUME            0x659
#define WM2200_EQRMIX_INPUT_2_SOURCE            0x65A
#define WM2200_EQRMIX_INPUT_2_VOLUME            0x65B
#define WM2200_EQRMIX_INPUT_3_SOURCE            0x65C
#define WM2200_EQRMIX_INPUT_3_VOLUME            0x65D
#define WM2200_EQRMIX_INPUT_4_SOURCE            0x65E
#define WM2200_EQRMIX_INPUT_4_VOLUME            0x65F
#define WM2200_LHPF1MIX_INPUT_1_SOURCE          0x660
#define WM2200_LHPF1MIX_INPUT_1_VOLUME          0x661
#define WM2200_LHPF1MIX_INPUT_2_SOURCE          0x662
#define WM2200_LHPF1MIX_INPUT_2_VOLUME          0x663
#define WM2200_LHPF1MIX_INPUT_3_SOURCE          0x664
#define WM2200_LHPF1MIX_INPUT_3_VOLUME          0x665
#define WM2200_LHPF1MIX_INPUT_4_SOURCE          0x666
#define WM2200_LHPF1MIX_INPUT_4_VOLUME          0x667
#define WM2200_LHPF2MIX_INPUT_1_SOURCE          0x668
#define WM2200_LHPF2MIX_INPUT_1_VOLUME          0x669
#define WM2200_LHPF2MIX_INPUT_2_SOURCE          0x66A
#define WM2200_LHPF2MIX_INPUT_2_VOLUME          0x66B
#define WM2200_LHPF2MIX_INPUT_3_SOURCE          0x66C
#define WM2200_LHPF2MIX_INPUT_3_VOLUME          0x66D
#define WM2200_LHPF2MIX_INPUT_4_SOURCE          0x66E
#define WM2200_LHPF2MIX_INPUT_4_VOLUME          0x66F
#define WM2200_DSP1LMIX_INPUT_1_SOURCE          0x670
#define WM2200_DSP1LMIX_INPUT_1_VOLUME          0x671
#define WM2200_DSP1LMIX_INPUT_2_SOURCE          0x672
#define WM2200_DSP1LMIX_INPUT_2_VOLUME          0x673
#define WM2200_DSP1LMIX_INPUT_3_SOURCE          0x674
#define WM2200_DSP1LMIX_INPUT_3_VOLUME          0x675
#define WM2200_DSP1LMIX_INPUT_4_SOURCE          0x676
#define WM2200_DSP1LMIX_INPUT_4_VOLUME          0x677
#define WM2200_DSP1RMIX_INPUT_1_SOURCE          0x678
#define WM2200_DSP1RMIX_INPUT_1_VOLUME          0x679
#define WM2200_DSP1RMIX_INPUT_2_SOURCE          0x67A
#define WM2200_DSP1RMIX_INPUT_2_VOLUME          0x67B
#define WM2200_DSP1RMIX_INPUT_3_SOURCE          0x67C
#define WM2200_DSP1RMIX_INPUT_3_VOLUME          0x67D
#define WM2200_DSP1RMIX_INPUT_4_SOURCE          0x67E
#define WM2200_DSP1RMIX_INPUT_4_VOLUME          0x67F
#define WM2200_DSP1AUX1MIX_INPUT_1_SOURCE       0x680
#define WM2200_DSP1AUX2MIX_INPUT_1_SOURCE       0x681
#define WM2200_DSP1AUX3MIX_INPUT_1_SOURCE       0x682
#define WM2200_DSP1AUX4MIX_INPUT_1_SOURCE       0x683
#define WM2200_DSP1AUX5MIX_INPUT_1_SOURCE       0x684
#define WM2200_DSP1AUX6MIX_INPUT_1_SOURCE       0x685
#define WM2200_DSP2LMIX_INPUT_1_SOURCE          0x686
#define WM2200_DSP2LMIX_INPUT_1_VOLUME          0x687
#define WM2200_DSP2LMIX_INPUT_2_SOURCE          0x688
#define WM2200_DSP2LMIX_INPUT_2_VOLUME          0x689
#define WM2200_DSP2LMIX_INPUT_3_SOURCE          0x68A
#define WM2200_DSP2LMIX_INPUT_3_VOLUME          0x68B
#define WM2200_DSP2LMIX_INPUT_4_SOURCE          0x68C
#define WM2200_DSP2LMIX_INPUT_4_VOLUME          0x68D
#define WM2200_DSP2RMIX_INPUT_1_SOURCE          0x68E
#define WM2200_DSP2RMIX_INPUT_1_VOLUME          0x68F
#define WM2200_DSP2RMIX_INPUT_2_SOURCE          0x690
#define WM2200_DSP2RMIX_INPUT_2_VOLUME          0x691
#define WM2200_DSP2RMIX_INPUT_3_SOURCE          0x692
#define WM2200_DSP2RMIX_INPUT_3_VOLUME          0x693
#define WM2200_DSP2RMIX_INPUT_4_SOURCE          0x694
#define WM2200_DSP2RMIX_INPUT_4_VOLUME          0x695
#define WM2200_DSP2AUX1MIX_INPUT_1_SOURCE       0x696
#define WM2200_DSP2AUX2MIX_INPUT_1_SOURCE       0x697
#define WM2200_DSP2AUX3MIX_INPUT_1_SOURCE       0x698
#define WM2200_DSP2AUX4MIX_INPUT_1_SOURCE       0x699
#define WM2200_DSP2AUX5MIX_INPUT_1_SOURCE       0x69A
#define WM2200_DSP2AUX6MIX_INPUT_1_SOURCE       0x69B
#define WM2200_GPIO_CTRL_1                      0x700
#define WM2200_GPIO_CTRL_2                      0x701
#define WM2200_GPIO_CTRL_3                      0x702
#define WM2200_GPIO_CTRL_4                      0x703
#define WM2200_ADPS1_IRQ0                       0x707
#define WM2200_ADPS1_IRQ1                       0x708
#define WM2200_MISC_PAD_CTRL_1                  0x709
#define WM2200_INTERRUPT_STATUS_1               0x800
#define WM2200_INTERRUPT_STATUS_1_MASK          0x801
#define WM2200_INTERRUPT_STATUS_2               0x802
#define WM2200_INTERRUPT_RAW_STATUS_2           0x803
#define WM2200_INTERRUPT_STATUS_2_MASK          0x804
#define WM2200_INTERRUPT_CONTROL                0x808
#define WM2200_EQL_1                            0x900
#define WM2200_EQL_2                            0x901
#define WM2200_EQL_3                            0x902
#define WM2200_EQL_4                            0x903
#define WM2200_EQL_5                            0x904
#define WM2200_EQL_6                            0x905
#define WM2200_EQL_7                            0x906
#define WM2200_EQL_8                            0x907
#define WM2200_EQL_9                            0x908
#define WM2200_EQL_10                           0x909
#define WM2200_EQL_11                           0x90A
#define WM2200_EQL_12                           0x90B
#define WM2200_EQL_13                           0x90C
#define WM2200_EQL_14                           0x90D
#define WM2200_EQL_15                           0x90E
#define WM2200_EQL_16                           0x90F
#define WM2200_EQL_17                           0x910
#define WM2200_EQL_18                           0x911
#define WM2200_EQL_19                           0x912
#define WM2200_EQL_20                           0x913
#define WM2200_EQR_1                            0x916
#define WM2200_EQR_2                            0x917
#define WM2200_EQR_3                            0x918
#define WM2200_EQR_4                            0x919
#define WM2200_EQR_5                            0x91A
#define WM2200_EQR_6                            0x91B
#define WM2200_EQR_7                            0x91C
#define WM2200_EQR_8                            0x91D
#define WM2200_EQR_9                            0x91E
#define WM2200_EQR_10                           0x91F
#define WM2200_EQR_11                           0x920
#define WM2200_EQR_12                           0x921
#define WM2200_EQR_13                           0x922
#define WM2200_EQR_14                           0x923
#define WM2200_EQR_15                           0x924
#define WM2200_EQR_16                           0x925
#define WM2200_EQR_17                           0x926
#define WM2200_EQR_18                           0x927
#define WM2200_EQR_19                           0x928
#define WM2200_EQR_20                           0x929
#define WM2200_HPLPF1_1                         0x93E
#define WM2200_HPLPF1_2                         0x93F
#define WM2200_HPLPF2_1                         0x942
#define WM2200_HPLPF2_2                         0x943
#define WM2200_DSP1_CONTROL_1                   0xA00
#define WM2200_DSP1_CONTROL_2                   0xA02
#define WM2200_DSP1_CONTROL_3                   0xA03
#define WM2200_DSP1_CONTROL_4                   0xA04
#define WM2200_DSP1_CONTROL_5                   0xA06
#define WM2200_DSP1_CONTROL_6                   0xA07
#define WM2200_DSP1_CONTROL_7                   0xA08
#define WM2200_DSP1_CONTROL_8                   0xA09
#define WM2200_DSP1_CONTROL_9                   0xA0A
#define WM2200_DSP1_CONTROL_10                  0xA0B
#define WM2200_DSP1_CONTROL_11                  0xA0C
#define WM2200_DSP1_CONTROL_12                  0xA0D
#define WM2200_DSP1_CONTROL_13                  0xA0F
#define WM2200_DSP1_CONTROL_14                  0xA10
#define WM2200_DSP1_CONTROL_15                  0xA11
#define WM2200_DSP1_CONTROL_16                  0xA12
#define WM2200_DSP1_CONTROL_17                  0xA13
#define WM2200_DSP1_CONTROL_18                  0xA14
#define WM2200_DSP1_CONTROL_19                  0xA16
#define WM2200_DSP1_CONTROL_20                  0xA17
#define WM2200_DSP1_CONTROL_21                  0xA18
#define WM2200_DSP1_CONTROL_22                  0xA1A
#define WM2200_DSP1_CONTROL_23                  0xA1B
#define WM2200_DSP1_CONTROL_24                  0xA1C
#define WM2200_DSP1_CONTROL_25                  0xA1E
#define WM2200_DSP1_CONTROL_26                  0xA20
#define WM2200_DSP1_CONTROL_27                  0xA21
#define WM2200_DSP1_CONTROL_28                  0xA22
#define WM2200_DSP1_CONTROL_29                  0xA23
#define WM2200_DSP1_CONTROL_30                  0xA24
#define WM2200_DSP1_CONTROL_31                  0xA26
#define WM2200_DSP2_CONTROL_1                   0xB00
#define WM2200_DSP2_CONTROL_2                   0xB02
#define WM2200_DSP2_CONTROL_3                   0xB03
#define WM2200_DSP2_CONTROL_4                   0xB04
#define WM2200_DSP2_CONTROL_5                   0xB06
#define WM2200_DSP2_CONTROL_6                   0xB07
#define WM2200_DSP2_CONTROL_7                   0xB08
#define WM2200_DSP2_CONTROL_8                   0xB09
#define WM2200_DSP2_CONTROL_9                   0xB0A
#define WM2200_DSP2_CONTROL_10                  0xB0B
#define WM2200_DSP2_CONTROL_11                  0xB0C
#define WM2200_DSP2_CONTROL_12                  0xB0D
#define WM2200_DSP2_CONTROL_13                  0xB0F
#define WM2200_DSP2_CONTROL_14                  0xB10
#define WM2200_DSP2_CONTROL_15                  0xB11
#define WM2200_DSP2_CONTROL_16                  0xB12
#define WM2200_DSP2_CONTROL_17                  0xB13
#define WM2200_DSP2_CONTROL_18                  0xB14
#define WM2200_DSP2_CONTROL_19                  0xB16
#define WM2200_DSP2_CONTROL_20                  0xB17
#define WM2200_DSP2_CONTROL_21                  0xB18
#define WM2200_DSP2_CONTROL_22                  0xB1A
#define WM2200_DSP2_CONTROL_23                  0xB1B
#define WM2200_DSP2_CONTROL_24                  0xB1C
#define WM2200_DSP2_CONTROL_25                  0xB1E
#define WM2200_DSP2_CONTROL_26                  0xB20
#define WM2200_DSP2_CONTROL_27                  0xB21
#define WM2200_DSP2_CONTROL_28                  0xB22
#define WM2200_DSP2_CONTROL_29                  0xB23
#define WM2200_DSP2_CONTROL_30                  0xB24
#define WM2200_DSP2_CONTROL_31                  0xB26
#define WM2200_ANC_CTRL1                        0xD00
#define WM2200_ANC_CTRL2                        0xD01
#define WM2200_ANC_CTRL3                        0xD02
#define WM2200_ANC_CTRL7                        0xD08
#define WM2200_ANC_CTRL8                        0xD09
#define WM2200_ANC_CTRL9                        0xD0A
#define WM2200_ANC_CTRL10                       0xD0B
#define WM2200_ANC_CTRL11                       0xD0C
#define WM2200_ANC_CTRL12                       0xD0D
#define WM2200_ANC_CTRL13                       0xD0E
#define WM2200_ANC_CTRL14                       0xD0F
#define WM2200_ANC_CTRL15                       0xD10
#define WM2200_ANC_CTRL16                       0xD11
#define WM2200_ANC_CTRL17                       0xD12
#define WM2200_ANC_CTRL18                       0xD15
#define WM2200_ANC_CTRL19                       0xD16
#define WM2200_ANC_CTRL20                       0xD17
#define WM2200_ANC_CTRL21                       0xD18
#define WM2200_ANC_CTRL22                       0xD19
#define WM2200_ANC_CTRL23                       0xD1A
#define WM2200_ANC_CTRL24                       0xD1B
#define WM2200_ANC_CTRL25                       0xD1C
#define WM2200_ANC_CTRL26                       0xD1D
#define WM2200_ANC_CTRL27                       0xD1E
#define WM2200_ANC_CTRL28                       0xD1F
#define WM2200_ANC_CTRL29                       0xD20
#define WM2200_ANC_CTRL30                       0xD21
#define WM2200_ANC_CTRL31                       0xD23
#define WM2200_ANC_CTRL32                       0xD24
#define WM2200_ANC_CTRL33                       0xD25
#define WM2200_ANC_CTRL34                       0xD27
#define WM2200_ANC_CTRL35                       0xD28
#define WM2200_ANC_CTRL36                       0xD29
#define WM2200_ANC_CTRL37                       0xD2A
#define WM2200_ANC_CTRL38                       0xD2B
#define WM2200_ANC_CTRL39                       0xD2C
#define WM2200_ANC_CTRL40                       0xD2D
#define WM2200_ANC_CTRL41                       0xD2E
#define WM2200_ANC_CTRL42                       0xD2F
#define WM2200_ANC_CTRL43                       0xD30
#define WM2200_ANC_CTRL44                       0xD31
#define WM2200_ANC_CTRL45                       0xD32
#define WM2200_ANC_CTRL46                       0xD33
#define WM2200_ANC_CTRL47                       0xD34
#define WM2200_ANC_CTRL48                       0xD35
#define WM2200_ANC_CTRL49                       0xD36
#define WM2200_ANC_CTRL50                       0xD37
#define WM2200_ANC_CTRL51                       0xD38
#define WM2200_ANC_CTRL52                       0xD39
#define WM2200_ANC_CTRL53                       0xD3A
#define WM2200_ANC_CTRL54                       0xD3B
#define WM2200_ANC_CTRL55                       0xD3C
#define WM2200_ANC_CTRL56                       0xD3D
#define WM2200_ANC_CTRL57                       0xD3E
#define WM2200_ANC_CTRL58                       0xD3F
#define WM2200_ANC_CTRL59                       0xD40
#define WM2200_ANC_CTRL60                       0xD41
#define WM2200_ANC_CTRL61                       0xD42
#define WM2200_ANC_CTRL62                       0xD43
#define WM2200_ANC_CTRL63                       0xD44
#define WM2200_ANC_CTRL64                       0xD45
#define WM2200_ANC_CTRL65                       0xD46
#define WM2200_ANC_CTRL66                       0xD47
#define WM2200_ANC_CTRL67                       0xD48
#define WM2200_ANC_CTRL68                       0xD49
#define WM2200_ANC_CTRL69                       0xD4A
#define WM2200_ANC_CTRL70                       0xD4B
#define WM2200_ANC_CTRL71                       0xD4C
#define WM2200_ANC_CTRL72                       0xD4D
#define WM2200_ANC_CTRL73                       0xD4E
#define WM2200_ANC_CTRL74                       0xD4F
#define WM2200_ANC_CTRL75                       0xD50
#define WM2200_ANC_CTRL76                       0xD51
#define WM2200_ANC_CTRL77                       0xD52
#define WM2200_ANC_CTRL78                       0xD53
#define WM2200_ANC_CTRL79                       0xD54
#define WM2200_ANC_CTRL80                       0xD55
#define WM2200_ANC_CTRL81                       0xD56
#define WM2200_ANC_CTRL82                       0xD57
#define WM2200_ANC_CTRL83                       0xD58
#define WM2200_ANC_CTRL84                       0xD5B
#define WM2200_ANC_CTRL85                       0xD5C
#define WM2200_ANC_CTRL86                       0xD5F
#define WM2200_ANC_CTRL87                       0xD60
#define WM2200_ANC_CTRL88                       0xD61
#define WM2200_ANC_CTRL89                       0xD62
#define WM2200_ANC_CTRL90                       0xD63
#define WM2200_ANC_CTRL91                       0xD64
#define WM2200_ANC_CTRL92                       0xD65
#define WM2200_ANC_CTRL93                       0xD66
#define WM2200_ANC_CTRL94                       0xD67
#define WM2200_ANC_CTRL95                       0xD68
#define WM2200_ANC_CTRL96                       0xD69
#define WM2200_DSP1_DM_0                        0x3000
#define WM2200_DSP1_DM_1                        0x3001
#define WM2200_DSP1_DM_2                        0x3002
#define WM2200_DSP1_DM_3                        0x3003
#define WM2200_DSP1_DM_2044                     0x37FC
#define WM2200_DSP1_DM_2045                     0x37FD
#define WM2200_DSP1_DM_2046                     0x37FE
#define WM2200_DSP1_DM_2047                     0x37FF
#define WM2200_DSP1_PM_0                        0x3800
#define WM2200_DSP1_PM_1                        0x3801
#define WM2200_DSP1_PM_2                        0x3802
#define WM2200_DSP1_PM_3                        0x3803
#define WM2200_DSP1_PM_4                        0x3804
#define WM2200_DSP1_PM_5                        0x3805
#define WM2200_DSP1_PM_762                      0x3AFA
#define WM2200_DSP1_PM_763                      0x3AFB
#define WM2200_DSP1_PM_764                      0x3AFC
#define WM2200_DSP1_PM_765                      0x3AFD
#define WM2200_DSP1_PM_766                      0x3AFE
#define WM2200_DSP1_PM_767                      0x3AFF
#define WM2200_DSP1_ZM_0                        0x3C00
#define WM2200_DSP1_ZM_1                        0x3C01
#define WM2200_DSP1_ZM_2                        0x3C02
#define WM2200_DSP1_ZM_3                        0x3C03
#define WM2200_DSP1_ZM_1020                     0x3FFC
#define WM2200_DSP1_ZM_1021                     0x3FFD
#define WM2200_DSP1_ZM_1022                     0x3FFE
#define WM2200_DSP1_ZM_1023                     0x3FFF
#define WM2200_DSP2_DM_0                        0x4000
#define WM2200_DSP2_DM_1                        0x4001
#define WM2200_DSP2_DM_2                        0x4002
#define WM2200_DSP2_DM_3                        0x4003
#define WM2200_DSP2_DM_2044                     0x47FC
#define WM2200_DSP2_DM_2045                     0x47FD
#define WM2200_DSP2_DM_2046                     0x47FE
#define WM2200_DSP2_DM_2047                     0x47FF
#define WM2200_DSP2_PM_0                        0x4800
#define WM2200_DSP2_PM_1                        0x4801
#define WM2200_DSP2_PM_2                        0x4802
#define WM2200_DSP2_PM_3                        0x4803
#define WM2200_DSP2_PM_4                        0x4804
#define WM2200_DSP2_PM_5                        0x4805
#define WM2200_DSP2_PM_762                      0x4AFA
#define WM2200_DSP2_PM_763                      0x4AFB
#define WM2200_DSP2_PM_764                      0x4AFC
#define WM2200_DSP2_PM_765                      0x4AFD
#define WM2200_DSP2_PM_766                      0x4AFE
#define WM2200_DSP2_PM_767                      0x4AFF
#define WM2200_DSP2_ZM_0                        0x4C00
#define WM2200_DSP2_ZM_1                        0x4C01
#define WM2200_DSP2_ZM_2                        0x4C02
#define WM2200_DSP2_ZM_3                        0x4C03
#define WM2200_DSP2_ZM_1020                     0x4FFC
#define WM2200_DSP2_ZM_1021                     0x4FFD
#define WM2200_DSP2_ZM_1022                     0x4FFE
#define WM2200_DSP2_ZM_1023                     0x4FFF

#define WM2200_REGISTER_COUNT                   494
#define WM2200_MAX_REGISTER                     0x4FFF


#define WM2200_SW_RESET_CHIP_ID1_MASK           0xFFFF  
#define WM2200_SW_RESET_CHIP_ID1_SHIFT               0  
#define WM2200_SW_RESET_CHIP_ID1_WIDTH              16  

#define WM2200_DEVICE_REVISION_MASK             0x000F  
#define WM2200_DEVICE_REVISION_SHIFT                 0  
#define WM2200_DEVICE_REVISION_WIDTH                 4  

#define WM2200_TONE_ENA                         0x0001  
#define WM2200_TONE_ENA_MASK                    0x0001  
#define WM2200_TONE_ENA_SHIFT                        0  
#define WM2200_TONE_ENA_WIDTH                        1  

#define WM2200_SYSCLK_FREQ_MASK                 0x0700  
#define WM2200_SYSCLK_FREQ_SHIFT                     8  
#define WM2200_SYSCLK_FREQ_WIDTH                     3  
#define WM2200_SYSCLK_ENA                       0x0040  
#define WM2200_SYSCLK_ENA_MASK                  0x0040  
#define WM2200_SYSCLK_ENA_SHIFT                      6  
#define WM2200_SYSCLK_ENA_WIDTH                      1  
#define WM2200_SYSCLK_SRC_MASK                  0x000F  
#define WM2200_SYSCLK_SRC_SHIFT                      0  
#define WM2200_SYSCLK_SRC_WIDTH                      4  

#define WM2200_SAMPLE_RATE_1_MASK               0x001F  
#define WM2200_SAMPLE_RATE_1_SHIFT                   0  
#define WM2200_SAMPLE_RATE_1_WIDTH                   5  

#define WM2200_FLL_ENA                          0x0001  
#define WM2200_FLL_ENA_MASK                     0x0001  
#define WM2200_FLL_ENA_SHIFT                         0  
#define WM2200_FLL_ENA_WIDTH                         1  

#define WM2200_FLL_OUTDIV_MASK                  0x3F00  
#define WM2200_FLL_OUTDIV_SHIFT                      8  
#define WM2200_FLL_OUTDIV_WIDTH                      6  
#define WM2200_FLL_FRATIO_MASK                  0x0007  
#define WM2200_FLL_FRATIO_SHIFT                      0  
#define WM2200_FLL_FRATIO_WIDTH                      3  

#define WM2200_FLL_FRACN_ENA                    0x0001  
#define WM2200_FLL_FRACN_ENA_MASK               0x0001  
#define WM2200_FLL_FRACN_ENA_SHIFT                   0  
#define WM2200_FLL_FRACN_ENA_WIDTH                   1  

#define WM2200_FLL_THETA_MASK                   0xFFFF  
#define WM2200_FLL_THETA_SHIFT                       0  
#define WM2200_FLL_THETA_WIDTH                      16  

#define WM2200_FLL_N_MASK                       0x03FF  
#define WM2200_FLL_N_SHIFT                           0  
#define WM2200_FLL_N_WIDTH                          10  

#define WM2200_FLL_CLK_REF_DIV_MASK             0x0030  
#define WM2200_FLL_CLK_REF_DIV_SHIFT                 4  
#define WM2200_FLL_CLK_REF_DIV_WIDTH                 2  
#define WM2200_FLL_CLK_REF_SRC_MASK             0x0003  
#define WM2200_FLL_CLK_REF_SRC_SHIFT                 0  
#define WM2200_FLL_CLK_REF_SRC_WIDTH                 2  

#define WM2200_FLL_LAMBDA_MASK                  0xFFFF  
#define WM2200_FLL_LAMBDA_SHIFT                      0  
#define WM2200_FLL_LAMBDA_WIDTH                     16  

#define WM2200_FLL_EFS_ENA                      0x0001  
#define WM2200_FLL_EFS_ENA_MASK                 0x0001  
#define WM2200_FLL_EFS_ENA_SHIFT                     0  
#define WM2200_FLL_EFS_ENA_WIDTH                     1  

#define WM2200_CPMIC_BYPASS_MODE                0x0020  
#define WM2200_CPMIC_BYPASS_MODE_MASK           0x0020  
#define WM2200_CPMIC_BYPASS_MODE_SHIFT               5  
#define WM2200_CPMIC_BYPASS_MODE_WIDTH               1  
#define WM2200_CPMIC_ENA                        0x0001  
#define WM2200_CPMIC_ENA_MASK                   0x0001  
#define WM2200_CPMIC_ENA_SHIFT                       0  
#define WM2200_CPMIC_ENA_WIDTH                       1  

#define WM2200_CPMIC_LDO_VSEL_OVERRIDE_MASK     0xF800  
#define WM2200_CPMIC_LDO_VSEL_OVERRIDE_SHIFT        11  
#define WM2200_CPMIC_LDO_VSEL_OVERRIDE_WIDTH         5  

#define WM2200_CPDM_ENA                         0x0001  
#define WM2200_CPDM_ENA_MASK                    0x0001  
#define WM2200_CPDM_ENA_SHIFT                        0  
#define WM2200_CPDM_ENA_WIDTH                        1  

#define WM2200_MICB1_DISCH                      0x0040  
#define WM2200_MICB1_DISCH_MASK                 0x0040  
#define WM2200_MICB1_DISCH_SHIFT                     6  
#define WM2200_MICB1_DISCH_WIDTH                     1  
#define WM2200_MICB1_RATE                       0x0020  
#define WM2200_MICB1_RATE_MASK                  0x0020  
#define WM2200_MICB1_RATE_SHIFT                      5  
#define WM2200_MICB1_RATE_WIDTH                      1  
#define WM2200_MICB1_LVL_MASK                   0x001C  
#define WM2200_MICB1_LVL_SHIFT                       2  
#define WM2200_MICB1_LVL_WIDTH                       3  
#define WM2200_MICB1_MODE                       0x0002  
#define WM2200_MICB1_MODE_MASK                  0x0002  
#define WM2200_MICB1_MODE_SHIFT                      1  
#define WM2200_MICB1_MODE_WIDTH                      1  
#define WM2200_MICB1_ENA                        0x0001  
#define WM2200_MICB1_ENA_MASK                   0x0001  
#define WM2200_MICB1_ENA_SHIFT                       0  
#define WM2200_MICB1_ENA_WIDTH                       1  

#define WM2200_MICB2_DISCH                      0x0040  
#define WM2200_MICB2_DISCH_MASK                 0x0040  
#define WM2200_MICB2_DISCH_SHIFT                     6  
#define WM2200_MICB2_DISCH_WIDTH                     1  
#define WM2200_MICB2_RATE                       0x0020  
#define WM2200_MICB2_RATE_MASK                  0x0020  
#define WM2200_MICB2_RATE_SHIFT                      5  
#define WM2200_MICB2_RATE_WIDTH                      1  
#define WM2200_MICB2_LVL_MASK                   0x001C  
#define WM2200_MICB2_LVL_SHIFT                       2  
#define WM2200_MICB2_LVL_WIDTH                       3  
#define WM2200_MICB2_MODE                       0x0002  
#define WM2200_MICB2_MODE_MASK                  0x0002  
#define WM2200_MICB2_MODE_SHIFT                      1  
#define WM2200_MICB2_MODE_WIDTH                      1  
#define WM2200_MICB2_ENA                        0x0001  
#define WM2200_MICB2_ENA_MASK                   0x0001  
#define WM2200_MICB2_ENA_SHIFT                       0  
#define WM2200_MICB2_ENA_WIDTH                       1  

#define WM2200_EPD_LP_ENA                       0x4000  
#define WM2200_EPD_LP_ENA_MASK                  0x4000  
#define WM2200_EPD_LP_ENA_SHIFT                     14  
#define WM2200_EPD_LP_ENA_WIDTH                      1  
#define WM2200_EPD_OUTP_LP_ENA                  0x2000  
#define WM2200_EPD_OUTP_LP_ENA_MASK             0x2000  
#define WM2200_EPD_OUTP_LP_ENA_SHIFT                13  
#define WM2200_EPD_OUTP_LP_ENA_WIDTH                 1  
#define WM2200_EPD_RMV_SHRT_LP                  0x1000  
#define WM2200_EPD_RMV_SHRT_LP_MASK             0x1000  
#define WM2200_EPD_RMV_SHRT_LP_SHIFT                12  
#define WM2200_EPD_RMV_SHRT_LP_WIDTH                 1  
#define WM2200_EPD_LN_ENA                       0x0800  
#define WM2200_EPD_LN_ENA_MASK                  0x0800  
#define WM2200_EPD_LN_ENA_SHIFT                     11  
#define WM2200_EPD_LN_ENA_WIDTH                      1  
#define WM2200_EPD_OUTP_LN_ENA                  0x0400  
#define WM2200_EPD_OUTP_LN_ENA_MASK             0x0400  
#define WM2200_EPD_OUTP_LN_ENA_SHIFT                10  
#define WM2200_EPD_OUTP_LN_ENA_WIDTH                 1  
#define WM2200_EPD_RMV_SHRT_LN                  0x0200  
#define WM2200_EPD_RMV_SHRT_LN_MASK             0x0200  
#define WM2200_EPD_RMV_SHRT_LN_SHIFT                 9  
#define WM2200_EPD_RMV_SHRT_LN_WIDTH                 1  

#define WM2200_EPD_RP_ENA                       0x4000  
#define WM2200_EPD_RP_ENA_MASK                  0x4000  
#define WM2200_EPD_RP_ENA_SHIFT                     14  
#define WM2200_EPD_RP_ENA_WIDTH                      1  
#define WM2200_EPD_OUTP_RP_ENA                  0x2000  
#define WM2200_EPD_OUTP_RP_ENA_MASK             0x2000  
#define WM2200_EPD_OUTP_RP_ENA_SHIFT                13  
#define WM2200_EPD_OUTP_RP_ENA_WIDTH                 1  
#define WM2200_EPD_RMV_SHRT_RP                  0x1000  
#define WM2200_EPD_RMV_SHRT_RP_MASK             0x1000  
#define WM2200_EPD_RMV_SHRT_RP_SHIFT                12  
#define WM2200_EPD_RMV_SHRT_RP_WIDTH                 1  
#define WM2200_EPD_RN_ENA                       0x0800  
#define WM2200_EPD_RN_ENA_MASK                  0x0800  
#define WM2200_EPD_RN_ENA_SHIFT                     11  
#define WM2200_EPD_RN_ENA_WIDTH                      1  
#define WM2200_EPD_OUTP_RN_ENA                  0x0400  
#define WM2200_EPD_OUTP_RN_ENA_MASK             0x0400  
#define WM2200_EPD_OUTP_RN_ENA_SHIFT                10  
#define WM2200_EPD_OUTP_RN_ENA_WIDTH                 1  
#define WM2200_EPD_RMV_SHRT_RN                  0x0200  
#define WM2200_EPD_RMV_SHRT_RN_MASK             0x0200  
#define WM2200_EPD_RMV_SHRT_RN_SHIFT                 9  
#define WM2200_EPD_RMV_SHRT_RN_WIDTH                 1  

#define WM2200_IN3L_ENA                         0x0020  
#define WM2200_IN3L_ENA_MASK                    0x0020  
#define WM2200_IN3L_ENA_SHIFT                        5  
#define WM2200_IN3L_ENA_WIDTH                        1  
#define WM2200_IN3R_ENA                         0x0010  
#define WM2200_IN3R_ENA_MASK                    0x0010  
#define WM2200_IN3R_ENA_SHIFT                        4  
#define WM2200_IN3R_ENA_WIDTH                        1  
#define WM2200_IN2L_ENA                         0x0008  
#define WM2200_IN2L_ENA_MASK                    0x0008  
#define WM2200_IN2L_ENA_SHIFT                        3  
#define WM2200_IN2L_ENA_WIDTH                        1  
#define WM2200_IN2R_ENA                         0x0004  
#define WM2200_IN2R_ENA_MASK                    0x0004  
#define WM2200_IN2R_ENA_SHIFT                        2  
#define WM2200_IN2R_ENA_WIDTH                        1  
#define WM2200_IN1L_ENA                         0x0002  
#define WM2200_IN1L_ENA_MASK                    0x0002  
#define WM2200_IN1L_ENA_SHIFT                        1  
#define WM2200_IN1L_ENA_WIDTH                        1  
#define WM2200_IN1R_ENA                         0x0001  
#define WM2200_IN1R_ENA_MASK                    0x0001  
#define WM2200_IN1R_ENA_SHIFT                        0  
#define WM2200_IN1R_ENA_WIDTH                        1  

#define WM2200_IN1_OSR                          0x2000  
#define WM2200_IN1_OSR_MASK                     0x2000  
#define WM2200_IN1_OSR_SHIFT                        13  
#define WM2200_IN1_OSR_WIDTH                         1  
#define WM2200_IN1_DMIC_SUP_MASK                0x1800  
#define WM2200_IN1_DMIC_SUP_SHIFT                   11  
#define WM2200_IN1_DMIC_SUP_WIDTH                    2  
#define WM2200_IN1_MODE_MASK                    0x0600  
#define WM2200_IN1_MODE_SHIFT                        9  
#define WM2200_IN1_MODE_WIDTH                        2  
#define WM2200_IN1L_PGA_VOL_MASK                0x00FE  
#define WM2200_IN1L_PGA_VOL_SHIFT                    1  
#define WM2200_IN1L_PGA_VOL_WIDTH                    7  

#define WM2200_IN1R_PGA_VOL_MASK                0x00FE  
#define WM2200_IN1R_PGA_VOL_SHIFT                    1  
#define WM2200_IN1R_PGA_VOL_WIDTH                    7  

#define WM2200_IN2_OSR                          0x2000  
#define WM2200_IN2_OSR_MASK                     0x2000  
#define WM2200_IN2_OSR_SHIFT                        13  
#define WM2200_IN2_OSR_WIDTH                         1  
#define WM2200_IN2_DMIC_SUP_MASK                0x1800  
#define WM2200_IN2_DMIC_SUP_SHIFT                   11  
#define WM2200_IN2_DMIC_SUP_WIDTH                    2  
#define WM2200_IN2_MODE_MASK                    0x0600  
#define WM2200_IN2_MODE_SHIFT                        9  
#define WM2200_IN2_MODE_WIDTH                        2  
#define WM2200_IN2L_PGA_VOL_MASK                0x00FE  
#define WM2200_IN2L_PGA_VOL_SHIFT                    1  
#define WM2200_IN2L_PGA_VOL_WIDTH                    7  

#define WM2200_IN2R_PGA_VOL_MASK                0x00FE  
#define WM2200_IN2R_PGA_VOL_SHIFT                    1  
#define WM2200_IN2R_PGA_VOL_WIDTH                    7  

#define WM2200_IN3_OSR                          0x2000  
#define WM2200_IN3_OSR_MASK                     0x2000  
#define WM2200_IN3_OSR_SHIFT                        13  
#define WM2200_IN3_OSR_WIDTH                         1  
#define WM2200_IN3_DMIC_SUP_MASK                0x1800  
#define WM2200_IN3_DMIC_SUP_SHIFT                   11  
#define WM2200_IN3_DMIC_SUP_WIDTH                    2  
#define WM2200_IN3_MODE_MASK                    0x0600  
#define WM2200_IN3_MODE_SHIFT                        9  
#define WM2200_IN3_MODE_WIDTH                        2  
#define WM2200_IN3L_PGA_VOL_MASK                0x00FE  
#define WM2200_IN3L_PGA_VOL_SHIFT                    1  
#define WM2200_IN3L_PGA_VOL_WIDTH                    7  

#define WM2200_IN3R_PGA_VOL_MASK                0x00FE  
#define WM2200_IN3R_PGA_VOL_SHIFT                    1  
#define WM2200_IN3R_PGA_VOL_WIDTH                    7  

#define WM2200_IN_RXANC_SEL_MASK                0x0007  
#define WM2200_IN_RXANC_SEL_SHIFT                    0  
#define WM2200_IN_RXANC_SEL_WIDTH                    3  

#define WM2200_IN_VD_RAMP_MASK                  0x0070  
#define WM2200_IN_VD_RAMP_SHIFT                      4  
#define WM2200_IN_VD_RAMP_WIDTH                      3  
#define WM2200_IN_VI_RAMP_MASK                  0x0007  
#define WM2200_IN_VI_RAMP_SHIFT                      0  
#define WM2200_IN_VI_RAMP_WIDTH                      3  

#define WM2200_IN_VU                            0x0200  
#define WM2200_IN_VU_MASK                       0x0200  
#define WM2200_IN_VU_SHIFT                           9  
#define WM2200_IN_VU_WIDTH                           1  
#define WM2200_IN1L_MUTE                        0x0100  
#define WM2200_IN1L_MUTE_MASK                   0x0100  
#define WM2200_IN1L_MUTE_SHIFT                       8  
#define WM2200_IN1L_MUTE_WIDTH                       1  
#define WM2200_IN1L_DIG_VOL_MASK                0x00FF  
#define WM2200_IN1L_DIG_VOL_SHIFT                    0  
#define WM2200_IN1L_DIG_VOL_WIDTH                    8  

#define WM2200_IN_VU                            0x0200  
#define WM2200_IN_VU_MASK                       0x0200  
#define WM2200_IN_VU_SHIFT                           9  
#define WM2200_IN_VU_WIDTH                           1  
#define WM2200_IN1R_MUTE                        0x0100  
#define WM2200_IN1R_MUTE_MASK                   0x0100  
#define WM2200_IN1R_MUTE_SHIFT                       8  
#define WM2200_IN1R_MUTE_WIDTH                       1  
#define WM2200_IN1R_DIG_VOL_MASK                0x00FF  
#define WM2200_IN1R_DIG_VOL_SHIFT                    0  
#define WM2200_IN1R_DIG_VOL_WIDTH                    8  

#define WM2200_IN_VU                            0x0200  
#define WM2200_IN_VU_MASK                       0x0200  
#define WM2200_IN_VU_SHIFT                           9  
#define WM2200_IN_VU_WIDTH                           1  
#define WM2200_IN2L_MUTE                        0x0100  
#define WM2200_IN2L_MUTE_MASK                   0x0100  
#define WM2200_IN2L_MUTE_SHIFT                       8  
#define WM2200_IN2L_MUTE_WIDTH                       1  
#define WM2200_IN2L_DIG_VOL_MASK                0x00FF  
#define WM2200_IN2L_DIG_VOL_SHIFT                    0  
#define WM2200_IN2L_DIG_VOL_WIDTH                    8  

#define WM2200_IN_VU                            0x0200  
#define WM2200_IN_VU_MASK                       0x0200  
#define WM2200_IN_VU_SHIFT                           9  
#define WM2200_IN_VU_WIDTH                           1  
#define WM2200_IN2R_MUTE                        0x0100  
#define WM2200_IN2R_MUTE_MASK                   0x0100  
#define WM2200_IN2R_MUTE_SHIFT                       8  
#define WM2200_IN2R_MUTE_WIDTH                       1  
#define WM2200_IN2R_DIG_VOL_MASK                0x00FF  
#define WM2200_IN2R_DIG_VOL_SHIFT                    0  
#define WM2200_IN2R_DIG_VOL_WIDTH                    8  

#define WM2200_IN_VU                            0x0200  
#define WM2200_IN_VU_MASK                       0x0200  
#define WM2200_IN_VU_SHIFT                           9  
#define WM2200_IN_VU_WIDTH                           1  
#define WM2200_IN3L_MUTE                        0x0100  
#define WM2200_IN3L_MUTE_MASK                   0x0100  
#define WM2200_IN3L_MUTE_SHIFT                       8  
#define WM2200_IN3L_MUTE_WIDTH                       1  
#define WM2200_IN3L_DIG_VOL_MASK                0x00FF  
#define WM2200_IN3L_DIG_VOL_SHIFT                    0  
#define WM2200_IN3L_DIG_VOL_WIDTH                    8  

#define WM2200_IN_VU                            0x0200  
#define WM2200_IN_VU_MASK                       0x0200  
#define WM2200_IN_VU_SHIFT                           9  
#define WM2200_IN_VU_WIDTH                           1  
#define WM2200_IN3R_MUTE                        0x0100  
#define WM2200_IN3R_MUTE_MASK                   0x0100  
#define WM2200_IN3R_MUTE_SHIFT                       8  
#define WM2200_IN3R_MUTE_WIDTH                       1  
#define WM2200_IN3R_DIG_VOL_MASK                0x00FF  
#define WM2200_IN3R_DIG_VOL_SHIFT                    0  
#define WM2200_IN3R_DIG_VOL_WIDTH                    8  

#define WM2200_OUT2L_ENA                        0x0008  
#define WM2200_OUT2L_ENA_MASK                   0x0008  
#define WM2200_OUT2L_ENA_SHIFT                       3  
#define WM2200_OUT2L_ENA_WIDTH                       1  
#define WM2200_OUT2R_ENA                        0x0004  
#define WM2200_OUT2R_ENA_MASK                   0x0004  
#define WM2200_OUT2R_ENA_SHIFT                       2  
#define WM2200_OUT2R_ENA_WIDTH                       1  
#define WM2200_OUT1L_ENA                        0x0002  
#define WM2200_OUT1L_ENA_MASK                   0x0002  
#define WM2200_OUT1L_ENA_SHIFT                       1  
#define WM2200_OUT1L_ENA_WIDTH                       1  
#define WM2200_OUT1R_ENA                        0x0001  
#define WM2200_OUT1R_ENA_MASK                   0x0001  
#define WM2200_OUT1R_ENA_SHIFT                       0  
#define WM2200_OUT1R_ENA_WIDTH                       1  

#define WM2200_OUT1_OSR                         0x2000  
#define WM2200_OUT1_OSR_MASK                    0x2000  
#define WM2200_OUT1_OSR_SHIFT                       13  
#define WM2200_OUT1_OSR_WIDTH                        1  
#define WM2200_OUT1L_ANC_SRC                    0x0800  
#define WM2200_OUT1L_ANC_SRC_MASK               0x0800  
#define WM2200_OUT1L_ANC_SRC_SHIFT                  11  
#define WM2200_OUT1L_ANC_SRC_WIDTH                   1  
#define WM2200_OUT1L_PGA_VOL_MASK               0x00FE  
#define WM2200_OUT1L_PGA_VOL_SHIFT                   1  
#define WM2200_OUT1L_PGA_VOL_WIDTH                   7  

#define WM2200_OUT1R_ANC_SRC                    0x0800  
#define WM2200_OUT1R_ANC_SRC_MASK               0x0800  
#define WM2200_OUT1R_ANC_SRC_SHIFT                  11  
#define WM2200_OUT1R_ANC_SRC_WIDTH                   1  
#define WM2200_OUT1R_PGA_VOL_MASK               0x00FE  
#define WM2200_OUT1R_PGA_VOL_SHIFT                   1  
#define WM2200_OUT1R_PGA_VOL_WIDTH                   7  

#define WM2200_OUT2_OSR                         0x2000  
#define WM2200_OUT2_OSR_MASK                    0x2000  
#define WM2200_OUT2_OSR_SHIFT                       13  
#define WM2200_OUT2_OSR_WIDTH                        1  
#define WM2200_OUT2L_ANC_SRC                    0x0800  
#define WM2200_OUT2L_ANC_SRC_MASK               0x0800  
#define WM2200_OUT2L_ANC_SRC_SHIFT                  11  
#define WM2200_OUT2L_ANC_SRC_WIDTH                   1  

#define WM2200_OUT2R_ANC_SRC                    0x0800  
#define WM2200_OUT2R_ANC_SRC_MASK               0x0800  
#define WM2200_OUT2R_ANC_SRC_SHIFT                  11  
#define WM2200_OUT2R_ANC_SRC_WIDTH                   1  

#define WM2200_AEC_LOOPBACK_ENA                 0x0004  
#define WM2200_AEC_LOOPBACK_ENA_MASK            0x0004  
#define WM2200_AEC_LOOPBACK_ENA_SHIFT                2  
#define WM2200_AEC_LOOPBACK_ENA_WIDTH                1  
#define WM2200_AEC_LOOPBACK_SRC_MASK            0x0003  
#define WM2200_AEC_LOOPBACK_SRC_SHIFT                0  
#define WM2200_AEC_LOOPBACK_SRC_WIDTH                2  

#define WM2200_OUT_VD_RAMP_MASK                 0x0070  
#define WM2200_OUT_VD_RAMP_SHIFT                     4  
#define WM2200_OUT_VD_RAMP_WIDTH                     3  
#define WM2200_OUT_VI_RAMP_MASK                 0x0007  
#define WM2200_OUT_VI_RAMP_SHIFT                     0  
#define WM2200_OUT_VI_RAMP_WIDTH                     3  

#define WM2200_OUT_VU                           0x0200  
#define WM2200_OUT_VU_MASK                      0x0200  
#define WM2200_OUT_VU_SHIFT                          9  
#define WM2200_OUT_VU_WIDTH                          1  
#define WM2200_OUT1L_MUTE                       0x0100  
#define WM2200_OUT1L_MUTE_MASK                  0x0100  
#define WM2200_OUT1L_MUTE_SHIFT                      8  
#define WM2200_OUT1L_MUTE_WIDTH                      1  
#define WM2200_OUT1L_VOL_MASK                   0x00FF  
#define WM2200_OUT1L_VOL_SHIFT                       0  
#define WM2200_OUT1L_VOL_WIDTH                       8  

#define WM2200_OUT_VU                           0x0200  
#define WM2200_OUT_VU_MASK                      0x0200  
#define WM2200_OUT_VU_SHIFT                          9  
#define WM2200_OUT_VU_WIDTH                          1  
#define WM2200_OUT1R_MUTE                       0x0100  
#define WM2200_OUT1R_MUTE_MASK                  0x0100  
#define WM2200_OUT1R_MUTE_SHIFT                      8  
#define WM2200_OUT1R_MUTE_WIDTH                      1  
#define WM2200_OUT1R_VOL_MASK                   0x00FF  
#define WM2200_OUT1R_VOL_SHIFT                       0  
#define WM2200_OUT1R_VOL_WIDTH                       8  

#define WM2200_OUT_VU                           0x0200  
#define WM2200_OUT_VU_MASK                      0x0200  
#define WM2200_OUT_VU_SHIFT                          9  
#define WM2200_OUT_VU_WIDTH                          1  
#define WM2200_OUT2L_MUTE                       0x0100  
#define WM2200_OUT2L_MUTE_MASK                  0x0100  
#define WM2200_OUT2L_MUTE_SHIFT                      8  
#define WM2200_OUT2L_MUTE_WIDTH                      1  
#define WM2200_OUT2L_VOL_MASK                   0x00FF  
#define WM2200_OUT2L_VOL_SHIFT                       0  
#define WM2200_OUT2L_VOL_WIDTH                       8  

#define WM2200_OUT_VU                           0x0200  
#define WM2200_OUT_VU_MASK                      0x0200  
#define WM2200_OUT_VU_SHIFT                          9  
#define WM2200_OUT_VU_WIDTH                          1  
#define WM2200_OUT2R_MUTE                       0x0100  
#define WM2200_OUT2R_MUTE_MASK                  0x0100  
#define WM2200_OUT2R_MUTE_SHIFT                      8  
#define WM2200_OUT2R_MUTE_WIDTH                      1  
#define WM2200_OUT2R_VOL_MASK                   0x00FF  
#define WM2200_OUT2R_VOL_SHIFT                       0  
#define WM2200_OUT2R_VOL_WIDTH                       8  

#define WM2200_SPK1R_MUTE                       0x2000  
#define WM2200_SPK1R_MUTE_MASK                  0x2000  
#define WM2200_SPK1R_MUTE_SHIFT                     13  
#define WM2200_SPK1R_MUTE_WIDTH                      1  
#define WM2200_SPK1L_MUTE                       0x1000  
#define WM2200_SPK1L_MUTE_MASK                  0x1000  
#define WM2200_SPK1L_MUTE_SHIFT                     12  
#define WM2200_SPK1L_MUTE_WIDTH                      1  
#define WM2200_SPK1_MUTE_ENDIAN                 0x0100  
#define WM2200_SPK1_MUTE_ENDIAN_MASK            0x0100  
#define WM2200_SPK1_MUTE_ENDIAN_SHIFT                8  
#define WM2200_SPK1_MUTE_ENDIAN_WIDTH                1  
#define WM2200_SPK1_MUTE_SEQL_MASK              0x00FF  
#define WM2200_SPK1_MUTE_SEQL_SHIFT                  0  
#define WM2200_SPK1_MUTE_SEQL_WIDTH                  8  

#define WM2200_SPK1_FMT                         0x0001  
#define WM2200_SPK1_FMT_MASK                    0x0001  
#define WM2200_SPK1_FMT_SHIFT                        0  
#define WM2200_SPK1_FMT_WIDTH                        1  

#define WM2200_AIF1_BCLK_INV                    0x0040  
#define WM2200_AIF1_BCLK_INV_MASK               0x0040  
#define WM2200_AIF1_BCLK_INV_SHIFT                   6  
#define WM2200_AIF1_BCLK_INV_WIDTH                   1  
#define WM2200_AIF1_BCLK_FRC                    0x0020  
#define WM2200_AIF1_BCLK_FRC_MASK               0x0020  
#define WM2200_AIF1_BCLK_FRC_SHIFT                   5  
#define WM2200_AIF1_BCLK_FRC_WIDTH                   1  
#define WM2200_AIF1_BCLK_MSTR                   0x0010  
#define WM2200_AIF1_BCLK_MSTR_MASK              0x0010  
#define WM2200_AIF1_BCLK_MSTR_SHIFT                  4  
#define WM2200_AIF1_BCLK_MSTR_WIDTH                  1  
#define WM2200_AIF1_BCLK_DIV_MASK               0x000F  
#define WM2200_AIF1_BCLK_DIV_SHIFT                   0  
#define WM2200_AIF1_BCLK_DIV_WIDTH                   4  

#define WM2200_AIF1TX_DAT_TRI                   0x0020  
#define WM2200_AIF1TX_DAT_TRI_MASK              0x0020  
#define WM2200_AIF1TX_DAT_TRI_SHIFT                  5  
#define WM2200_AIF1TX_DAT_TRI_WIDTH                  1  
#define WM2200_AIF1TX_LRCLK_SRC                 0x0008  
#define WM2200_AIF1TX_LRCLK_SRC_MASK            0x0008  
#define WM2200_AIF1TX_LRCLK_SRC_SHIFT                3  
#define WM2200_AIF1TX_LRCLK_SRC_WIDTH                1  
#define WM2200_AIF1TX_LRCLK_INV                 0x0004  
#define WM2200_AIF1TX_LRCLK_INV_MASK            0x0004  
#define WM2200_AIF1TX_LRCLK_INV_SHIFT                2  
#define WM2200_AIF1TX_LRCLK_INV_WIDTH                1  
#define WM2200_AIF1TX_LRCLK_FRC                 0x0002  
#define WM2200_AIF1TX_LRCLK_FRC_MASK            0x0002  
#define WM2200_AIF1TX_LRCLK_FRC_SHIFT                1  
#define WM2200_AIF1TX_LRCLK_FRC_WIDTH                1  
#define WM2200_AIF1TX_LRCLK_MSTR                0x0001  
#define WM2200_AIF1TX_LRCLK_MSTR_MASK           0x0001  
#define WM2200_AIF1TX_LRCLK_MSTR_SHIFT               0  
#define WM2200_AIF1TX_LRCLK_MSTR_WIDTH               1  

#define WM2200_AIF1RX_LRCLK_INV                 0x0004  
#define WM2200_AIF1RX_LRCLK_INV_MASK            0x0004  
#define WM2200_AIF1RX_LRCLK_INV_SHIFT                2  
#define WM2200_AIF1RX_LRCLK_INV_WIDTH                1  
#define WM2200_AIF1RX_LRCLK_FRC                 0x0002  
#define WM2200_AIF1RX_LRCLK_FRC_MASK            0x0002  
#define WM2200_AIF1RX_LRCLK_FRC_SHIFT                1  
#define WM2200_AIF1RX_LRCLK_FRC_WIDTH                1  
#define WM2200_AIF1RX_LRCLK_MSTR                0x0001  
#define WM2200_AIF1RX_LRCLK_MSTR_MASK           0x0001  
#define WM2200_AIF1RX_LRCLK_MSTR_SHIFT               0  
#define WM2200_AIF1RX_LRCLK_MSTR_WIDTH               1  

#define WM2200_AIF1_TRI                         0x0040  
#define WM2200_AIF1_TRI_MASK                    0x0040  
#define WM2200_AIF1_TRI_SHIFT                        6  
#define WM2200_AIF1_TRI_WIDTH                        1  

#define WM2200_AIF1_FMT_MASK                    0x0007  
#define WM2200_AIF1_FMT_SHIFT                        0  
#define WM2200_AIF1_FMT_WIDTH                        3  

#define WM2200_AIF1TX_BCPF_MASK                 0x07FF  
#define WM2200_AIF1TX_BCPF_SHIFT                     0  
#define WM2200_AIF1TX_BCPF_WIDTH                    11  

#define WM2200_AIF1RX_BCPF_MASK                 0x07FF  
#define WM2200_AIF1RX_BCPF_SHIFT                     0  
#define WM2200_AIF1RX_BCPF_WIDTH                    11  

#define WM2200_AIF1TX_WL_MASK                   0x3F00  
#define WM2200_AIF1TX_WL_SHIFT                       8  
#define WM2200_AIF1TX_WL_WIDTH                       6  
#define WM2200_AIF1TX_SLOT_LEN_MASK             0x00FF  
#define WM2200_AIF1TX_SLOT_LEN_SHIFT                 0  
#define WM2200_AIF1TX_SLOT_LEN_WIDTH                 8  

#define WM2200_AIF1RX_WL_MASK                   0x3F00  
#define WM2200_AIF1RX_WL_SHIFT                       8  
#define WM2200_AIF1RX_WL_WIDTH                       6  
#define WM2200_AIF1RX_SLOT_LEN_MASK             0x00FF  
#define WM2200_AIF1RX_SLOT_LEN_SHIFT                 0  
#define WM2200_AIF1RX_SLOT_LEN_WIDTH                 8  

#define WM2200_AIF1TX1_SLOT_MASK                0x003F  
#define WM2200_AIF1TX1_SLOT_SHIFT                    0  
#define WM2200_AIF1TX1_SLOT_WIDTH                    6  

#define WM2200_AIF1TX2_SLOT_MASK                0x003F  
#define WM2200_AIF1TX2_SLOT_SHIFT                    0  
#define WM2200_AIF1TX2_SLOT_WIDTH                    6  

#define WM2200_AIF1TX3_SLOT_MASK                0x003F  
#define WM2200_AIF1TX3_SLOT_SHIFT                    0  
#define WM2200_AIF1TX3_SLOT_WIDTH                    6  

#define WM2200_AIF1TX4_SLOT_MASK                0x003F  
#define WM2200_AIF1TX4_SLOT_SHIFT                    0  
#define WM2200_AIF1TX4_SLOT_WIDTH                    6  

#define WM2200_AIF1TX5_SLOT_MASK                0x003F  
#define WM2200_AIF1TX5_SLOT_SHIFT                    0  
#define WM2200_AIF1TX5_SLOT_WIDTH                    6  

#define WM2200_AIF1TX6_SLOT_MASK                0x003F  
#define WM2200_AIF1TX6_SLOT_SHIFT                    0  
#define WM2200_AIF1TX6_SLOT_WIDTH                    6  

#define WM2200_AIF1RX1_SLOT_MASK                0x003F  
#define WM2200_AIF1RX1_SLOT_SHIFT                    0  
#define WM2200_AIF1RX1_SLOT_WIDTH                    6  

#define WM2200_AIF1RX2_SLOT_MASK                0x003F  
#define WM2200_AIF1RX2_SLOT_SHIFT                    0  
#define WM2200_AIF1RX2_SLOT_WIDTH                    6  

#define WM2200_AIF1RX3_SLOT_MASK                0x003F  
#define WM2200_AIF1RX3_SLOT_SHIFT                    0  
#define WM2200_AIF1RX3_SLOT_WIDTH                    6  

#define WM2200_AIF1RX4_SLOT_MASK                0x003F  
#define WM2200_AIF1RX4_SLOT_SHIFT                    0  
#define WM2200_AIF1RX4_SLOT_WIDTH                    6  

#define WM2200_AIF1RX5_SLOT_MASK                0x003F  
#define WM2200_AIF1RX5_SLOT_SHIFT                    0  
#define WM2200_AIF1RX5_SLOT_WIDTH                    6  

#define WM2200_AIF1RX6_SLOT_MASK                0x003F  
#define WM2200_AIF1RX6_SLOT_SHIFT                    0  
#define WM2200_AIF1RX6_SLOT_WIDTH                    6  

#define WM2200_AIF1RX6_ENA                      0x0800  
#define WM2200_AIF1RX6_ENA_MASK                 0x0800  
#define WM2200_AIF1RX6_ENA_SHIFT                    11  
#define WM2200_AIF1RX6_ENA_WIDTH                     1  
#define WM2200_AIF1RX5_ENA                      0x0400  
#define WM2200_AIF1RX5_ENA_MASK                 0x0400  
#define WM2200_AIF1RX5_ENA_SHIFT                    10  
#define WM2200_AIF1RX5_ENA_WIDTH                     1  
#define WM2200_AIF1RX4_ENA                      0x0200  
#define WM2200_AIF1RX4_ENA_MASK                 0x0200  
#define WM2200_AIF1RX4_ENA_SHIFT                     9  
#define WM2200_AIF1RX4_ENA_WIDTH                     1  
#define WM2200_AIF1RX3_ENA                      0x0100  
#define WM2200_AIF1RX3_ENA_MASK                 0x0100  
#define WM2200_AIF1RX3_ENA_SHIFT                     8  
#define WM2200_AIF1RX3_ENA_WIDTH                     1  
#define WM2200_AIF1RX2_ENA                      0x0080  
#define WM2200_AIF1RX2_ENA_MASK                 0x0080  
#define WM2200_AIF1RX2_ENA_SHIFT                     7  
#define WM2200_AIF1RX2_ENA_WIDTH                     1  
#define WM2200_AIF1RX1_ENA                      0x0040  
#define WM2200_AIF1RX1_ENA_MASK                 0x0040  
#define WM2200_AIF1RX1_ENA_SHIFT                     6  
#define WM2200_AIF1RX1_ENA_WIDTH                     1  
#define WM2200_AIF1TX6_ENA                      0x0020  
#define WM2200_AIF1TX6_ENA_MASK                 0x0020  
#define WM2200_AIF1TX6_ENA_SHIFT                     5  
#define WM2200_AIF1TX6_ENA_WIDTH                     1  
#define WM2200_AIF1TX5_ENA                      0x0010  
#define WM2200_AIF1TX5_ENA_MASK                 0x0010  
#define WM2200_AIF1TX5_ENA_SHIFT                     4  
#define WM2200_AIF1TX5_ENA_WIDTH                     1  
#define WM2200_AIF1TX4_ENA                      0x0008  
#define WM2200_AIF1TX4_ENA_MASK                 0x0008  
#define WM2200_AIF1TX4_ENA_SHIFT                     3  
#define WM2200_AIF1TX4_ENA_WIDTH                     1  
#define WM2200_AIF1TX3_ENA                      0x0004  
#define WM2200_AIF1TX3_ENA_MASK                 0x0004  
#define WM2200_AIF1TX3_ENA_SHIFT                     2  
#define WM2200_AIF1TX3_ENA_WIDTH                     1  
#define WM2200_AIF1TX2_ENA                      0x0002  
#define WM2200_AIF1TX2_ENA_MASK                 0x0002  
#define WM2200_AIF1TX2_ENA_SHIFT                     1  
#define WM2200_AIF1TX2_ENA_WIDTH                     1  
#define WM2200_AIF1TX1_ENA                      0x0001  
#define WM2200_AIF1TX1_ENA_MASK                 0x0001  
#define WM2200_AIF1TX1_ENA_SHIFT                     0  
#define WM2200_AIF1TX1_ENA_WIDTH                     1  

#define WM2200_OUT1LMIX_SRC1_MASK               0x007F  
#define WM2200_OUT1LMIX_SRC1_SHIFT                   0  
#define WM2200_OUT1LMIX_SRC1_WIDTH                   7  

#define WM2200_OUT1LMIX_VOL1_MASK               0x00FE  
#define WM2200_OUT1LMIX_VOL1_SHIFT                   1  
#define WM2200_OUT1LMIX_VOL1_WIDTH                   7  

#define WM2200_OUT1LMIX_SRC2_MASK               0x007F  
#define WM2200_OUT1LMIX_SRC2_SHIFT                   0  
#define WM2200_OUT1LMIX_SRC2_WIDTH                   7  

#define WM2200_OUT1LMIX_VOL2_MASK               0x00FE  
#define WM2200_OUT1LMIX_VOL2_SHIFT                   1  
#define WM2200_OUT1LMIX_VOL2_WIDTH                   7  

#define WM2200_OUT1LMIX_SRC3_MASK               0x007F  
#define WM2200_OUT1LMIX_SRC3_SHIFT                   0  
#define WM2200_OUT1LMIX_SRC3_WIDTH                   7  

#define WM2200_OUT1LMIX_VOL3_MASK               0x00FE  
#define WM2200_OUT1LMIX_VOL3_SHIFT                   1  
#define WM2200_OUT1LMIX_VOL3_WIDTH                   7  

#define WM2200_OUT1LMIX_SRC4_MASK               0x007F  
#define WM2200_OUT1LMIX_SRC4_SHIFT                   0  
#define WM2200_OUT1LMIX_SRC4_WIDTH                   7  

#define WM2200_OUT1LMIX_VOL4_MASK               0x00FE  
#define WM2200_OUT1LMIX_VOL4_SHIFT                   1  
#define WM2200_OUT1LMIX_VOL4_WIDTH                   7  

#define WM2200_OUT1RMIX_SRC1_MASK               0x007F  
#define WM2200_OUT1RMIX_SRC1_SHIFT                   0  
#define WM2200_OUT1RMIX_SRC1_WIDTH                   7  

#define WM2200_OUT1RMIX_VOL1_MASK               0x00FE  
#define WM2200_OUT1RMIX_VOL1_SHIFT                   1  
#define WM2200_OUT1RMIX_VOL1_WIDTH                   7  

#define WM2200_OUT1RMIX_SRC2_MASK               0x007F  
#define WM2200_OUT1RMIX_SRC2_SHIFT                   0  
#define WM2200_OUT1RMIX_SRC2_WIDTH                   7  

#define WM2200_OUT1RMIX_VOL2_MASK               0x00FE  
#define WM2200_OUT1RMIX_VOL2_SHIFT                   1  
#define WM2200_OUT1RMIX_VOL2_WIDTH                   7  

#define WM2200_OUT1RMIX_SRC3_MASK               0x007F  
#define WM2200_OUT1RMIX_SRC3_SHIFT                   0  
#define WM2200_OUT1RMIX_SRC3_WIDTH                   7  

#define WM2200_OUT1RMIX_VOL3_MASK               0x00FE  
#define WM2200_OUT1RMIX_VOL3_SHIFT                   1  
#define WM2200_OUT1RMIX_VOL3_WIDTH                   7  

#define WM2200_OUT1RMIX_SRC4_MASK               0x007F  
#define WM2200_OUT1RMIX_SRC4_SHIFT                   0  
#define WM2200_OUT1RMIX_SRC4_WIDTH                   7  

#define WM2200_OUT1RMIX_VOL4_MASK               0x00FE  
#define WM2200_OUT1RMIX_VOL4_SHIFT                   1  
#define WM2200_OUT1RMIX_VOL4_WIDTH                   7  

#define WM2200_OUT2LMIX_SRC1_MASK               0x007F  
#define WM2200_OUT2LMIX_SRC1_SHIFT                   0  
#define WM2200_OUT2LMIX_SRC1_WIDTH                   7  

#define WM2200_OUT2LMIX_VOL1_MASK               0x00FE  
#define WM2200_OUT2LMIX_VOL1_SHIFT                   1  
#define WM2200_OUT2LMIX_VOL1_WIDTH                   7  

#define WM2200_OUT2LMIX_SRC2_MASK               0x007F  
#define WM2200_OUT2LMIX_SRC2_SHIFT                   0  
#define WM2200_OUT2LMIX_SRC2_WIDTH                   7  

#define WM2200_OUT2LMIX_VOL2_MASK               0x00FE  
#define WM2200_OUT2LMIX_VOL2_SHIFT                   1  
#define WM2200_OUT2LMIX_VOL2_WIDTH                   7  

#define WM2200_OUT2LMIX_SRC3_MASK               0x007F  
#define WM2200_OUT2LMIX_SRC3_SHIFT                   0  
#define WM2200_OUT2LMIX_SRC3_WIDTH                   7  

#define WM2200_OUT2LMIX_VOL3_MASK               0x00FE  
#define WM2200_OUT2LMIX_VOL3_SHIFT                   1  
#define WM2200_OUT2LMIX_VOL3_WIDTH                   7  

#define WM2200_OUT2LMIX_SRC4_MASK               0x007F  
#define WM2200_OUT2LMIX_SRC4_SHIFT                   0  
#define WM2200_OUT2LMIX_SRC4_WIDTH                   7  

#define WM2200_OUT2LMIX_VOL4_MASK               0x00FE  
#define WM2200_OUT2LMIX_VOL4_SHIFT                   1  
#define WM2200_OUT2LMIX_VOL4_WIDTH                   7  

#define WM2200_OUT2RMIX_SRC1_MASK               0x007F  
#define WM2200_OUT2RMIX_SRC1_SHIFT                   0  
#define WM2200_OUT2RMIX_SRC1_WIDTH                   7  

#define WM2200_OUT2RMIX_VOL1_MASK               0x00FE  
#define WM2200_OUT2RMIX_VOL1_SHIFT                   1  
#define WM2200_OUT2RMIX_VOL1_WIDTH                   7  

#define WM2200_OUT2RMIX_SRC2_MASK               0x007F  
#define WM2200_OUT2RMIX_SRC2_SHIFT                   0  
#define WM2200_OUT2RMIX_SRC2_WIDTH                   7  

#define WM2200_OUT2RMIX_VOL2_MASK               0x00FE  
#define WM2200_OUT2RMIX_VOL2_SHIFT                   1  
#define WM2200_OUT2RMIX_VOL2_WIDTH                   7  

#define WM2200_OUT2RMIX_SRC3_MASK               0x007F  
#define WM2200_OUT2RMIX_SRC3_SHIFT                   0  
#define WM2200_OUT2RMIX_SRC3_WIDTH                   7  

#define WM2200_OUT2RMIX_VOL3_MASK               0x00FE  
#define WM2200_OUT2RMIX_VOL3_SHIFT                   1  
#define WM2200_OUT2RMIX_VOL3_WIDTH                   7  

#define WM2200_OUT2RMIX_SRC4_MASK               0x007F  
#define WM2200_OUT2RMIX_SRC4_SHIFT                   0  
#define WM2200_OUT2RMIX_SRC4_WIDTH                   7  

#define WM2200_OUT2RMIX_VOL4_MASK               0x00FE  
#define WM2200_OUT2RMIX_VOL4_SHIFT                   1  
#define WM2200_OUT2RMIX_VOL4_WIDTH                   7  

#define WM2200_AIF1TX1MIX_SRC1_MASK             0x007F  
#define WM2200_AIF1TX1MIX_SRC1_SHIFT                 0  
#define WM2200_AIF1TX1MIX_SRC1_WIDTH                 7  

#define WM2200_AIF1TX1MIX_VOL1_MASK             0x00FE  
#define WM2200_AIF1TX1MIX_VOL1_SHIFT                 1  
#define WM2200_AIF1TX1MIX_VOL1_WIDTH                 7  

#define WM2200_AIF1TX1MIX_SRC2_MASK             0x007F  
#define WM2200_AIF1TX1MIX_SRC2_SHIFT                 0  
#define WM2200_AIF1TX1MIX_SRC2_WIDTH                 7  

#define WM2200_AIF1TX1MIX_VOL2_MASK             0x00FE  
#define WM2200_AIF1TX1MIX_VOL2_SHIFT                 1  
#define WM2200_AIF1TX1MIX_VOL2_WIDTH                 7  

#define WM2200_AIF1TX1MIX_SRC3_MASK             0x007F  
#define WM2200_AIF1TX1MIX_SRC3_SHIFT                 0  
#define WM2200_AIF1TX1MIX_SRC3_WIDTH                 7  

#define WM2200_AIF1TX1MIX_VOL3_MASK             0x00FE  
#define WM2200_AIF1TX1MIX_VOL3_SHIFT                 1  
#define WM2200_AIF1TX1MIX_VOL3_WIDTH                 7  

#define WM2200_AIF1TX1MIX_SRC4_MASK             0x007F  
#define WM2200_AIF1TX1MIX_SRC4_SHIFT                 0  
#define WM2200_AIF1TX1MIX_SRC4_WIDTH                 7  

#define WM2200_AIF1TX1MIX_VOL4_MASK             0x00FE  
#define WM2200_AIF1TX1MIX_VOL4_SHIFT                 1  
#define WM2200_AIF1TX1MIX_VOL4_WIDTH                 7  

#define WM2200_AIF1TX2MIX_SRC1_MASK             0x007F  
#define WM2200_AIF1TX2MIX_SRC1_SHIFT                 0  
#define WM2200_AIF1TX2MIX_SRC1_WIDTH                 7  

#define WM2200_AIF1TX2MIX_VOL1_MASK             0x00FE  
#define WM2200_AIF1TX2MIX_VOL1_SHIFT                 1  
#define WM2200_AIF1TX2MIX_VOL1_WIDTH                 7  

#define WM2200_AIF1TX2MIX_SRC2_MASK             0x007F  
#define WM2200_AIF1TX2MIX_SRC2_SHIFT                 0  
#define WM2200_AIF1TX2MIX_SRC2_WIDTH                 7  

#define WM2200_AIF1TX2MIX_VOL2_MASK             0x00FE  
#define WM2200_AIF1TX2MIX_VOL2_SHIFT                 1  
#define WM2200_AIF1TX2MIX_VOL2_WIDTH                 7  

#define WM2200_AIF1TX2MIX_SRC3_MASK             0x007F  
#define WM2200_AIF1TX2MIX_SRC3_SHIFT                 0  
#define WM2200_AIF1TX2MIX_SRC3_WIDTH                 7  

#define WM2200_AIF1TX2MIX_VOL3_MASK             0x00FE  
#define WM2200_AIF1TX2MIX_VOL3_SHIFT                 1  
#define WM2200_AIF1TX2MIX_VOL3_WIDTH                 7  

#define WM2200_AIF1TX2MIX_SRC4_MASK             0x007F  
#define WM2200_AIF1TX2MIX_SRC4_SHIFT                 0  
#define WM2200_AIF1TX2MIX_SRC4_WIDTH                 7  

#define WM2200_AIF1TX2MIX_VOL4_MASK             0x00FE  
#define WM2200_AIF1TX2MIX_VOL4_SHIFT                 1  
#define WM2200_AIF1TX2MIX_VOL4_WIDTH                 7  

#define WM2200_AIF1TX3MIX_SRC1_MASK             0x007F  
#define WM2200_AIF1TX3MIX_SRC1_SHIFT                 0  
#define WM2200_AIF1TX3MIX_SRC1_WIDTH                 7  

#define WM2200_AIF1TX3MIX_VOL1_MASK             0x00FE  
#define WM2200_AIF1TX3MIX_VOL1_SHIFT                 1  
#define WM2200_AIF1TX3MIX_VOL1_WIDTH                 7  

#define WM2200_AIF1TX3MIX_SRC2_MASK             0x007F  
#define WM2200_AIF1TX3MIX_SRC2_SHIFT                 0  
#define WM2200_AIF1TX3MIX_SRC2_WIDTH                 7  

#define WM2200_AIF1TX3MIX_VOL2_MASK             0x00FE  
#define WM2200_AIF1TX3MIX_VOL2_SHIFT                 1  
#define WM2200_AIF1TX3MIX_VOL2_WIDTH                 7  

#define WM2200_AIF1TX3MIX_SRC3_MASK             0x007F  
#define WM2200_AIF1TX3MIX_SRC3_SHIFT                 0  
#define WM2200_AIF1TX3MIX_SRC3_WIDTH                 7  

#define WM2200_AIF1TX3MIX_VOL3_MASK             0x00FE  
#define WM2200_AIF1TX3MIX_VOL3_SHIFT                 1  
#define WM2200_AIF1TX3MIX_VOL3_WIDTH                 7  

#define WM2200_AIF1TX3MIX_SRC4_MASK             0x007F  
#define WM2200_AIF1TX3MIX_SRC4_SHIFT                 0  
#define WM2200_AIF1TX3MIX_SRC4_WIDTH                 7  

#define WM2200_AIF1TX3MIX_VOL4_MASK             0x00FE  
#define WM2200_AIF1TX3MIX_VOL4_SHIFT                 1  
#define WM2200_AIF1TX3MIX_VOL4_WIDTH                 7  

#define WM2200_AIF1TX4MIX_SRC1_MASK             0x007F  
#define WM2200_AIF1TX4MIX_SRC1_SHIFT                 0  
#define WM2200_AIF1TX4MIX_SRC1_WIDTH                 7  

#define WM2200_AIF1TX4MIX_VOL1_MASK             0x00FE  
#define WM2200_AIF1TX4MIX_VOL1_SHIFT                 1  
#define WM2200_AIF1TX4MIX_VOL1_WIDTH                 7  

#define WM2200_AIF1TX4MIX_SRC2_MASK             0x007F  
#define WM2200_AIF1TX4MIX_SRC2_SHIFT                 0  
#define WM2200_AIF1TX4MIX_SRC2_WIDTH                 7  

#define WM2200_AIF1TX4MIX_VOL2_MASK             0x00FE  
#define WM2200_AIF1TX4MIX_VOL2_SHIFT                 1  
#define WM2200_AIF1TX4MIX_VOL2_WIDTH                 7  

#define WM2200_AIF1TX4MIX_SRC3_MASK             0x007F  
#define WM2200_AIF1TX4MIX_SRC3_SHIFT                 0  
#define WM2200_AIF1TX4MIX_SRC3_WIDTH                 7  

#define WM2200_AIF1TX4MIX_VOL3_MASK             0x00FE  
#define WM2200_AIF1TX4MIX_VOL3_SHIFT                 1  
#define WM2200_AIF1TX4MIX_VOL3_WIDTH                 7  

#define WM2200_AIF1TX4MIX_SRC4_MASK             0x007F  
#define WM2200_AIF1TX4MIX_SRC4_SHIFT                 0  
#define WM2200_AIF1TX4MIX_SRC4_WIDTH                 7  

#define WM2200_AIF1TX4MIX_VOL4_MASK             0x00FE  
#define WM2200_AIF1TX4MIX_VOL4_SHIFT                 1  
#define WM2200_AIF1TX4MIX_VOL4_WIDTH                 7  

#define WM2200_AIF1TX5MIX_SRC1_MASK             0x007F  
#define WM2200_AIF1TX5MIX_SRC1_SHIFT                 0  
#define WM2200_AIF1TX5MIX_SRC1_WIDTH                 7  

#define WM2200_AIF1TX5MIX_VOL1_MASK             0x00FE  
#define WM2200_AIF1TX5MIX_VOL1_SHIFT                 1  
#define WM2200_AIF1TX5MIX_VOL1_WIDTH                 7  

#define WM2200_AIF1TX5MIX_SRC2_MASK             0x007F  
#define WM2200_AIF1TX5MIX_SRC2_SHIFT                 0  
#define WM2200_AIF1TX5MIX_SRC2_WIDTH                 7  

#define WM2200_AIF1TX5MIX_VOL2_MASK             0x00FE  
#define WM2200_AIF1TX5MIX_VOL2_SHIFT                 1  
#define WM2200_AIF1TX5MIX_VOL2_WIDTH                 7  

#define WM2200_AIF1TX5MIX_SRC3_MASK             0x007F  
#define WM2200_AIF1TX5MIX_SRC3_SHIFT                 0  
#define WM2200_AIF1TX5MIX_SRC3_WIDTH                 7  

#define WM2200_AIF1TX5MIX_VOL3_MASK             0x00FE  
#define WM2200_AIF1TX5MIX_VOL3_SHIFT                 1  
#define WM2200_AIF1TX5MIX_VOL3_WIDTH                 7  

#define WM2200_AIF1TX5MIX_SRC4_MASK             0x007F  
#define WM2200_AIF1TX5MIX_SRC4_SHIFT                 0  
#define WM2200_AIF1TX5MIX_SRC4_WIDTH                 7  

#define WM2200_AIF1TX5MIX_VOL4_MASK             0x00FE  
#define WM2200_AIF1TX5MIX_VOL4_SHIFT                 1  
#define WM2200_AIF1TX5MIX_VOL4_WIDTH                 7  

#define WM2200_AIF1TX6MIX_SRC1_MASK             0x007F  
#define WM2200_AIF1TX6MIX_SRC1_SHIFT                 0  
#define WM2200_AIF1TX6MIX_SRC1_WIDTH                 7  

#define WM2200_AIF1TX6MIX_VOL1_MASK             0x00FE  
#define WM2200_AIF1TX6MIX_VOL1_SHIFT                 1  
#define WM2200_AIF1TX6MIX_VOL1_WIDTH                 7  

#define WM2200_AIF1TX6MIX_SRC2_MASK             0x007F  
#define WM2200_AIF1TX6MIX_SRC2_SHIFT                 0  
#define WM2200_AIF1TX6MIX_SRC2_WIDTH                 7  

#define WM2200_AIF1TX6MIX_VOL2_MASK             0x00FE  
#define WM2200_AIF1TX6MIX_VOL2_SHIFT                 1  
#define WM2200_AIF1TX6MIX_VOL2_WIDTH                 7  

#define WM2200_AIF1TX6MIX_SRC3_MASK             0x007F  
#define WM2200_AIF1TX6MIX_SRC3_SHIFT                 0  
#define WM2200_AIF1TX6MIX_SRC3_WIDTH                 7  

#define WM2200_AIF1TX6MIX_VOL3_MASK             0x00FE  
#define WM2200_AIF1TX6MIX_VOL3_SHIFT                 1  
#define WM2200_AIF1TX6MIX_VOL3_WIDTH                 7  

#define WM2200_AIF1TX6MIX_SRC4_MASK             0x007F  
#define WM2200_AIF1TX6MIX_SRC4_SHIFT                 0  
#define WM2200_AIF1TX6MIX_SRC4_WIDTH                 7  

#define WM2200_AIF1TX6MIX_VOL4_MASK             0x00FE  
#define WM2200_AIF1TX6MIX_VOL4_SHIFT                 1  
#define WM2200_AIF1TX6MIX_VOL4_WIDTH                 7  

#define WM2200_EQLMIX_SRC1_MASK                 0x007F  
#define WM2200_EQLMIX_SRC1_SHIFT                     0  
#define WM2200_EQLMIX_SRC1_WIDTH                     7  

#define WM2200_EQLMIX_VOL1_MASK                 0x00FE  
#define WM2200_EQLMIX_VOL1_SHIFT                     1  
#define WM2200_EQLMIX_VOL1_WIDTH                     7  

#define WM2200_EQLMIX_SRC2_MASK                 0x007F  
#define WM2200_EQLMIX_SRC2_SHIFT                     0  
#define WM2200_EQLMIX_SRC2_WIDTH                     7  

#define WM2200_EQLMIX_VOL2_MASK                 0x00FE  
#define WM2200_EQLMIX_VOL2_SHIFT                     1  
#define WM2200_EQLMIX_VOL2_WIDTH                     7  

#define WM2200_EQLMIX_SRC3_MASK                 0x007F  
#define WM2200_EQLMIX_SRC3_SHIFT                     0  
#define WM2200_EQLMIX_SRC3_WIDTH                     7  

#define WM2200_EQLMIX_VOL3_MASK                 0x00FE  
#define WM2200_EQLMIX_VOL3_SHIFT                     1  
#define WM2200_EQLMIX_VOL3_WIDTH                     7  

#define WM2200_EQLMIX_SRC4_MASK                 0x007F  
#define WM2200_EQLMIX_SRC4_SHIFT                     0  
#define WM2200_EQLMIX_SRC4_WIDTH                     7  

#define WM2200_EQLMIX_VOL4_MASK                 0x00FE  
#define WM2200_EQLMIX_VOL4_SHIFT                     1  
#define WM2200_EQLMIX_VOL4_WIDTH                     7  

#define WM2200_EQRMIX_SRC1_MASK                 0x007F  
#define WM2200_EQRMIX_SRC1_SHIFT                     0  
#define WM2200_EQRMIX_SRC1_WIDTH                     7  

#define WM2200_EQRMIX_VOL1_MASK                 0x00FE  
#define WM2200_EQRMIX_VOL1_SHIFT                     1  
#define WM2200_EQRMIX_VOL1_WIDTH                     7  

#define WM2200_EQRMIX_SRC2_MASK                 0x007F  
#define WM2200_EQRMIX_SRC2_SHIFT                     0  
#define WM2200_EQRMIX_SRC2_WIDTH                     7  

#define WM2200_EQRMIX_VOL2_MASK                 0x00FE  
#define WM2200_EQRMIX_VOL2_SHIFT                     1  
#define WM2200_EQRMIX_VOL2_WIDTH                     7  

#define WM2200_EQRMIX_SRC3_MASK                 0x007F  
#define WM2200_EQRMIX_SRC3_SHIFT                     0  
#define WM2200_EQRMIX_SRC3_WIDTH                     7  

#define WM2200_EQRMIX_VOL3_MASK                 0x00FE  
#define WM2200_EQRMIX_VOL3_SHIFT                     1  
#define WM2200_EQRMIX_VOL3_WIDTH                     7  

#define WM2200_EQRMIX_SRC4_MASK                 0x007F  
#define WM2200_EQRMIX_SRC4_SHIFT                     0  
#define WM2200_EQRMIX_SRC4_WIDTH                     7  

#define WM2200_EQRMIX_VOL4_MASK                 0x00FE  
#define WM2200_EQRMIX_VOL4_SHIFT                     1  
#define WM2200_EQRMIX_VOL4_WIDTH                     7  

#define WM2200_LHPF1MIX_SRC1_MASK               0x007F  
#define WM2200_LHPF1MIX_SRC1_SHIFT                   0  
#define WM2200_LHPF1MIX_SRC1_WIDTH                   7  

#define WM2200_LHPF1MIX_VOL1_MASK               0x00FE  
#define WM2200_LHPF1MIX_VOL1_SHIFT                   1  
#define WM2200_LHPF1MIX_VOL1_WIDTH                   7  

#define WM2200_LHPF1MIX_SRC2_MASK               0x007F  
#define WM2200_LHPF1MIX_SRC2_SHIFT                   0  
#define WM2200_LHPF1MIX_SRC2_WIDTH                   7  

#define WM2200_LHPF1MIX_VOL2_MASK               0x00FE  
#define WM2200_LHPF1MIX_VOL2_SHIFT                   1  
#define WM2200_LHPF1MIX_VOL2_WIDTH                   7  

#define WM2200_LHPF1MIX_SRC3_MASK               0x007F  
#define WM2200_LHPF1MIX_SRC3_SHIFT                   0  
#define WM2200_LHPF1MIX_SRC3_WIDTH                   7  

#define WM2200_LHPF1MIX_VOL3_MASK               0x00FE  
#define WM2200_LHPF1MIX_VOL3_SHIFT                   1  
#define WM2200_LHPF1MIX_VOL3_WIDTH                   7  

#define WM2200_LHPF1MIX_SRC4_MASK               0x007F  
#define WM2200_LHPF1MIX_SRC4_SHIFT                   0  
#define WM2200_LHPF1MIX_SRC4_WIDTH                   7  

#define WM2200_LHPF1MIX_VOL4_MASK               0x00FE  
#define WM2200_LHPF1MIX_VOL4_SHIFT                   1  
#define WM2200_LHPF1MIX_VOL4_WIDTH                   7  

#define WM2200_LHPF2MIX_SRC1_MASK               0x007F  
#define WM2200_LHPF2MIX_SRC1_SHIFT                   0  
#define WM2200_LHPF2MIX_SRC1_WIDTH                   7  

#define WM2200_LHPF2MIX_VOL1_MASK               0x00FE  
#define WM2200_LHPF2MIX_VOL1_SHIFT                   1  
#define WM2200_LHPF2MIX_VOL1_WIDTH                   7  

#define WM2200_LHPF2MIX_SRC2_MASK               0x007F  
#define WM2200_LHPF2MIX_SRC2_SHIFT                   0  
#define WM2200_LHPF2MIX_SRC2_WIDTH                   7  

#define WM2200_LHPF2MIX_VOL2_MASK               0x00FE  
#define WM2200_LHPF2MIX_VOL2_SHIFT                   1  
#define WM2200_LHPF2MIX_VOL2_WIDTH                   7  

#define WM2200_LHPF2MIX_SRC3_MASK               0x007F  
#define WM2200_LHPF2MIX_SRC3_SHIFT                   0  
#define WM2200_LHPF2MIX_SRC3_WIDTH                   7  

#define WM2200_LHPF2MIX_VOL3_MASK               0x00FE  
#define WM2200_LHPF2MIX_VOL3_SHIFT                   1  
#define WM2200_LHPF2MIX_VOL3_WIDTH                   7  

#define WM2200_LHPF2MIX_SRC4_MASK               0x007F  
#define WM2200_LHPF2MIX_SRC4_SHIFT                   0  
#define WM2200_LHPF2MIX_SRC4_WIDTH                   7  

#define WM2200_LHPF2MIX_VOL4_MASK               0x00FE  
#define WM2200_LHPF2MIX_VOL4_SHIFT                   1  
#define WM2200_LHPF2MIX_VOL4_WIDTH                   7  

#define WM2200_DSP1LMIX_SRC1_MASK               0x007F  
#define WM2200_DSP1LMIX_SRC1_SHIFT                   0  
#define WM2200_DSP1LMIX_SRC1_WIDTH                   7  

#define WM2200_DSP1LMIX_VOL1_MASK               0x00FE  
#define WM2200_DSP1LMIX_VOL1_SHIFT                   1  
#define WM2200_DSP1LMIX_VOL1_WIDTH                   7  

#define WM2200_DSP1LMIX_SRC2_MASK               0x007F  
#define WM2200_DSP1LMIX_SRC2_SHIFT                   0  
#define WM2200_DSP1LMIX_SRC2_WIDTH                   7  

#define WM2200_DSP1LMIX_VOL2_MASK               0x00FE  
#define WM2200_DSP1LMIX_VOL2_SHIFT                   1  
#define WM2200_DSP1LMIX_VOL2_WIDTH                   7  

#define WM2200_DSP1LMIX_SRC3_MASK               0x007F  
#define WM2200_DSP1LMIX_SRC3_SHIFT                   0  
#define WM2200_DSP1LMIX_SRC3_WIDTH                   7  

#define WM2200_DSP1LMIX_VOL3_MASK               0x00FE  
#define WM2200_DSP1LMIX_VOL3_SHIFT                   1  
#define WM2200_DSP1LMIX_VOL3_WIDTH                   7  

#define WM2200_DSP1LMIX_SRC4_MASK               0x007F  
#define WM2200_DSP1LMIX_SRC4_SHIFT                   0  
#define WM2200_DSP1LMIX_SRC4_WIDTH                   7  

#define WM2200_DSP1LMIX_VOL4_MASK               0x00FE  
#define WM2200_DSP1LMIX_VOL4_SHIFT                   1  
#define WM2200_DSP1LMIX_VOL4_WIDTH                   7  

#define WM2200_DSP1RMIX_SRC1_MASK               0x007F  
#define WM2200_DSP1RMIX_SRC1_SHIFT                   0  
#define WM2200_DSP1RMIX_SRC1_WIDTH                   7  

#define WM2200_DSP1RMIX_VOL1_MASK               0x00FE  
#define WM2200_DSP1RMIX_VOL1_SHIFT                   1  
#define WM2200_DSP1RMIX_VOL1_WIDTH                   7  

#define WM2200_DSP1RMIX_SRC2_MASK               0x007F  
#define WM2200_DSP1RMIX_SRC2_SHIFT                   0  
#define WM2200_DSP1RMIX_SRC2_WIDTH                   7  

#define WM2200_DSP1RMIX_VOL2_MASK               0x00FE  
#define WM2200_DSP1RMIX_VOL2_SHIFT                   1  
#define WM2200_DSP1RMIX_VOL2_WIDTH                   7  

#define WM2200_DSP1RMIX_SRC3_MASK               0x007F  
#define WM2200_DSP1RMIX_SRC3_SHIFT                   0  
#define WM2200_DSP1RMIX_SRC3_WIDTH                   7  

#define WM2200_DSP1RMIX_VOL3_MASK               0x00FE  
#define WM2200_DSP1RMIX_VOL3_SHIFT                   1  
#define WM2200_DSP1RMIX_VOL3_WIDTH                   7  

#define WM2200_DSP1RMIX_SRC4_MASK               0x007F  
#define WM2200_DSP1RMIX_SRC4_SHIFT                   0  
#define WM2200_DSP1RMIX_SRC4_WIDTH                   7  

#define WM2200_DSP1RMIX_VOL4_MASK               0x00FE  
#define WM2200_DSP1RMIX_VOL4_SHIFT                   1  
#define WM2200_DSP1RMIX_VOL4_WIDTH                   7  

#define WM2200_DSP1AUX1MIX_SRC1_MASK            0x007F  
#define WM2200_DSP1AUX1MIX_SRC1_SHIFT                0  
#define WM2200_DSP1AUX1MIX_SRC1_WIDTH                7  

#define WM2200_DSP1AUX2MIX_SRC1_MASK            0x007F  
#define WM2200_DSP1AUX2MIX_SRC1_SHIFT                0  
#define WM2200_DSP1AUX2MIX_SRC1_WIDTH                7  

#define WM2200_DSP1AUX3MIX_SRC1_MASK            0x007F  
#define WM2200_DSP1AUX3MIX_SRC1_SHIFT                0  
#define WM2200_DSP1AUX3MIX_SRC1_WIDTH                7  

#define WM2200_DSP1AUX4MIX_SRC1_MASK            0x007F  
#define WM2200_DSP1AUX4MIX_SRC1_SHIFT                0  
#define WM2200_DSP1AUX4MIX_SRC1_WIDTH                7  

#define WM2200_DSP1AUX5MIX_SRC1_MASK            0x007F  
#define WM2200_DSP1AUX5MIX_SRC1_SHIFT                0  
#define WM2200_DSP1AUX5MIX_SRC1_WIDTH                7  

#define WM2200_DSP1AUX6MIX_SRC1_MASK            0x007F  
#define WM2200_DSP1AUX6MIX_SRC1_SHIFT                0  
#define WM2200_DSP1AUX6MIX_SRC1_WIDTH                7  

#define WM2200_DSP2LMIX_SRC1_MASK               0x007F  
#define WM2200_DSP2LMIX_SRC1_SHIFT                   0  
#define WM2200_DSP2LMIX_SRC1_WIDTH                   7  

#define WM2200_DSP2LMIX_VOL1_MASK               0x00FE  
#define WM2200_DSP2LMIX_VOL1_SHIFT                   1  
#define WM2200_DSP2LMIX_VOL1_WIDTH                   7  

#define WM2200_DSP2LMIX_SRC2_MASK               0x007F  
#define WM2200_DSP2LMIX_SRC2_SHIFT                   0  
#define WM2200_DSP2LMIX_SRC2_WIDTH                   7  

#define WM2200_DSP2LMIX_VOL2_MASK               0x00FE  
#define WM2200_DSP2LMIX_VOL2_SHIFT                   1  
#define WM2200_DSP2LMIX_VOL2_WIDTH                   7  

#define WM2200_DSP2LMIX_SRC3_MASK               0x007F  
#define WM2200_DSP2LMIX_SRC3_SHIFT                   0  
#define WM2200_DSP2LMIX_SRC3_WIDTH                   7  

#define WM2200_DSP2LMIX_VOL3_MASK               0x00FE  
#define WM2200_DSP2LMIX_VOL3_SHIFT                   1  
#define WM2200_DSP2LMIX_VOL3_WIDTH                   7  

#define WM2200_DSP2LMIX_SRC4_MASK               0x007F  
#define WM2200_DSP2LMIX_SRC4_SHIFT                   0  
#define WM2200_DSP2LMIX_SRC4_WIDTH                   7  

#define WM2200_DSP2LMIX_VOL4_MASK               0x00FE  
#define WM2200_DSP2LMIX_VOL4_SHIFT                   1  
#define WM2200_DSP2LMIX_VOL4_WIDTH                   7  

#define WM2200_DSP2RMIX_SRC1_MASK               0x007F  
#define WM2200_DSP2RMIX_SRC1_SHIFT                   0  
#define WM2200_DSP2RMIX_SRC1_WIDTH                   7  

#define WM2200_DSP2RMIX_VOL1_MASK               0x00FE  
#define WM2200_DSP2RMIX_VOL1_SHIFT                   1  
#define WM2200_DSP2RMIX_VOL1_WIDTH                   7  

#define WM2200_DSP2RMIX_SRC2_MASK               0x007F  
#define WM2200_DSP2RMIX_SRC2_SHIFT                   0  
#define WM2200_DSP2RMIX_SRC2_WIDTH                   7  

#define WM2200_DSP2RMIX_VOL2_MASK               0x00FE  
#define WM2200_DSP2RMIX_VOL2_SHIFT                   1  
#define WM2200_DSP2RMIX_VOL2_WIDTH                   7  

#define WM2200_DSP2RMIX_SRC3_MASK               0x007F  
#define WM2200_DSP2RMIX_SRC3_SHIFT                   0  
#define WM2200_DSP2RMIX_SRC3_WIDTH                   7  

#define WM2200_DSP2RMIX_VOL3_MASK               0x00FE  
#define WM2200_DSP2RMIX_VOL3_SHIFT                   1  
#define WM2200_DSP2RMIX_VOL3_WIDTH                   7  

#define WM2200_DSP2RMIX_SRC4_MASK               0x007F  
#define WM2200_DSP2RMIX_SRC4_SHIFT                   0  
#define WM2200_DSP2RMIX_SRC4_WIDTH                   7  

#define WM2200_DSP2RMIX_VOL4_MASK               0x00FE  
#define WM2200_DSP2RMIX_VOL4_SHIFT                   1  
#define WM2200_DSP2RMIX_VOL4_WIDTH                   7  

#define WM2200_DSP2AUX1MIX_SRC1_MASK            0x007F  
#define WM2200_DSP2AUX1MIX_SRC1_SHIFT                0  
#define WM2200_DSP2AUX1MIX_SRC1_WIDTH                7  

#define WM2200_DSP2AUX2MIX_SRC1_MASK            0x007F  
#define WM2200_DSP2AUX2MIX_SRC1_SHIFT                0  
#define WM2200_DSP2AUX2MIX_SRC1_WIDTH                7  

#define WM2200_DSP2AUX3MIX_SRC1_MASK            0x007F  
#define WM2200_DSP2AUX3MIX_SRC1_SHIFT                0  
#define WM2200_DSP2AUX3MIX_SRC1_WIDTH                7  

#define WM2200_DSP2AUX4MIX_SRC1_MASK            0x007F  
#define WM2200_DSP2AUX4MIX_SRC1_SHIFT                0  
#define WM2200_DSP2AUX4MIX_SRC1_WIDTH                7  

#define WM2200_DSP2AUX5MIX_SRC1_MASK            0x007F  
#define WM2200_DSP2AUX5MIX_SRC1_SHIFT                0  
#define WM2200_DSP2AUX5MIX_SRC1_WIDTH                7  

#define WM2200_DSP2AUX6MIX_SRC1_MASK            0x007F  
#define WM2200_DSP2AUX6MIX_SRC1_SHIFT                0  
#define WM2200_DSP2AUX6MIX_SRC1_WIDTH                7  

#define WM2200_GP1_DIR                          0x8000  
#define WM2200_GP1_DIR_MASK                     0x8000  
#define WM2200_GP1_DIR_SHIFT                        15  
#define WM2200_GP1_DIR_WIDTH                         1  
#define WM2200_GP1_PU                           0x4000  
#define WM2200_GP1_PU_MASK                      0x4000  
#define WM2200_GP1_PU_SHIFT                         14  
#define WM2200_GP1_PU_WIDTH                          1  
#define WM2200_GP1_PD                           0x2000  
#define WM2200_GP1_PD_MASK                      0x2000  
#define WM2200_GP1_PD_SHIFT                         13  
#define WM2200_GP1_PD_WIDTH                          1  
#define WM2200_GP1_POL                          0x0400  
#define WM2200_GP1_POL_MASK                     0x0400  
#define WM2200_GP1_POL_SHIFT                        10  
#define WM2200_GP1_POL_WIDTH                         1  
#define WM2200_GP1_OP_CFG                       0x0200  
#define WM2200_GP1_OP_CFG_MASK                  0x0200  
#define WM2200_GP1_OP_CFG_SHIFT                      9  
#define WM2200_GP1_OP_CFG_WIDTH                      1  
#define WM2200_GP1_DB                           0x0100  
#define WM2200_GP1_DB_MASK                      0x0100  
#define WM2200_GP1_DB_SHIFT                          8  
#define WM2200_GP1_DB_WIDTH                          1  
#define WM2200_GP1_LVL                          0x0040  
#define WM2200_GP1_LVL_MASK                     0x0040  
#define WM2200_GP1_LVL_SHIFT                         6  
#define WM2200_GP1_LVL_WIDTH                         1  
#define WM2200_GP1_FN_MASK                      0x003F  
#define WM2200_GP1_FN_SHIFT                          0  
#define WM2200_GP1_FN_WIDTH                          6  

#define WM2200_GP2_DIR                          0x8000  
#define WM2200_GP2_DIR_MASK                     0x8000  
#define WM2200_GP2_DIR_SHIFT                        15  
#define WM2200_GP2_DIR_WIDTH                         1  
#define WM2200_GP2_PU                           0x4000  
#define WM2200_GP2_PU_MASK                      0x4000  
#define WM2200_GP2_PU_SHIFT                         14  
#define WM2200_GP2_PU_WIDTH                          1  
#define WM2200_GP2_PD                           0x2000  
#define WM2200_GP2_PD_MASK                      0x2000  
#define WM2200_GP2_PD_SHIFT                         13  
#define WM2200_GP2_PD_WIDTH                          1  
#define WM2200_GP2_POL                          0x0400  
#define WM2200_GP2_POL_MASK                     0x0400  
#define WM2200_GP2_POL_SHIFT                        10  
#define WM2200_GP2_POL_WIDTH                         1  
#define WM2200_GP2_OP_CFG                       0x0200  
#define WM2200_GP2_OP_CFG_MASK                  0x0200  
#define WM2200_GP2_OP_CFG_SHIFT                      9  
#define WM2200_GP2_OP_CFG_WIDTH                      1  
#define WM2200_GP2_DB                           0x0100  
#define WM2200_GP2_DB_MASK                      0x0100  
#define WM2200_GP2_DB_SHIFT                          8  
#define WM2200_GP2_DB_WIDTH                          1  
#define WM2200_GP2_LVL                          0x0040  
#define WM2200_GP2_LVL_MASK                     0x0040  
#define WM2200_GP2_LVL_SHIFT                         6  
#define WM2200_GP2_LVL_WIDTH                         1  
#define WM2200_GP2_FN_MASK                      0x003F  
#define WM2200_GP2_FN_SHIFT                          0  
#define WM2200_GP2_FN_WIDTH                          6  

#define WM2200_GP3_DIR                          0x8000  
#define WM2200_GP3_DIR_MASK                     0x8000  
#define WM2200_GP3_DIR_SHIFT                        15  
#define WM2200_GP3_DIR_WIDTH                         1  
#define WM2200_GP3_PU                           0x4000  
#define WM2200_GP3_PU_MASK                      0x4000  
#define WM2200_GP3_PU_SHIFT                         14  
#define WM2200_GP3_PU_WIDTH                          1  
#define WM2200_GP3_PD                           0x2000  
#define WM2200_GP3_PD_MASK                      0x2000  
#define WM2200_GP3_PD_SHIFT                         13  
#define WM2200_GP3_PD_WIDTH                          1  
#define WM2200_GP3_POL                          0x0400  
#define WM2200_GP3_POL_MASK                     0x0400  
#define WM2200_GP3_POL_SHIFT                        10  
#define WM2200_GP3_POL_WIDTH                         1  
#define WM2200_GP3_OP_CFG                       0x0200  
#define WM2200_GP3_OP_CFG_MASK                  0x0200  
#define WM2200_GP3_OP_CFG_SHIFT                      9  
#define WM2200_GP3_OP_CFG_WIDTH                      1  
#define WM2200_GP3_DB                           0x0100  
#define WM2200_GP3_DB_MASK                      0x0100  
#define WM2200_GP3_DB_SHIFT                          8  
#define WM2200_GP3_DB_WIDTH                          1  
#define WM2200_GP3_LVL                          0x0040  
#define WM2200_GP3_LVL_MASK                     0x0040  
#define WM2200_GP3_LVL_SHIFT                         6  
#define WM2200_GP3_LVL_WIDTH                         1  
#define WM2200_GP3_FN_MASK                      0x003F  
#define WM2200_GP3_FN_SHIFT                          0  
#define WM2200_GP3_FN_WIDTH                          6  

#define WM2200_GP4_DIR                          0x8000  
#define WM2200_GP4_DIR_MASK                     0x8000  
#define WM2200_GP4_DIR_SHIFT                        15  
#define WM2200_GP4_DIR_WIDTH                         1  
#define WM2200_GP4_PU                           0x4000  
#define WM2200_GP4_PU_MASK                      0x4000  
#define WM2200_GP4_PU_SHIFT                         14  
#define WM2200_GP4_PU_WIDTH                          1  
#define WM2200_GP4_PD                           0x2000  
#define WM2200_GP4_PD_MASK                      0x2000  
#define WM2200_GP4_PD_SHIFT                         13  
#define WM2200_GP4_PD_WIDTH                          1  
#define WM2200_GP4_POL                          0x0400  
#define WM2200_GP4_POL_MASK                     0x0400  
#define WM2200_GP4_POL_SHIFT                        10  
#define WM2200_GP4_POL_WIDTH                         1  
#define WM2200_GP4_OP_CFG                       0x0200  
#define WM2200_GP4_OP_CFG_MASK                  0x0200  
#define WM2200_GP4_OP_CFG_SHIFT                      9  
#define WM2200_GP4_OP_CFG_WIDTH                      1  
#define WM2200_GP4_DB                           0x0100  
#define WM2200_GP4_DB_MASK                      0x0100  
#define WM2200_GP4_DB_SHIFT                          8  
#define WM2200_GP4_DB_WIDTH                          1  
#define WM2200_GP4_LVL                          0x0040  
#define WM2200_GP4_LVL_MASK                     0x0040  
#define WM2200_GP4_LVL_SHIFT                         6  
#define WM2200_GP4_LVL_WIDTH                         1  
#define WM2200_GP4_FN_MASK                      0x003F  
#define WM2200_GP4_FN_SHIFT                          0  
#define WM2200_GP4_FN_WIDTH                          6  

#define WM2200_DSP_IRQ1                         0x0002  
#define WM2200_DSP_IRQ1_MASK                    0x0002  
#define WM2200_DSP_IRQ1_SHIFT                        1  
#define WM2200_DSP_IRQ1_WIDTH                        1  
#define WM2200_DSP_IRQ0                         0x0001  
#define WM2200_DSP_IRQ0_MASK                    0x0001  
#define WM2200_DSP_IRQ0_SHIFT                        0  
#define WM2200_DSP_IRQ0_WIDTH                        1  

#define WM2200_DSP_IRQ3                         0x0002  
#define WM2200_DSP_IRQ3_MASK                    0x0002  
#define WM2200_DSP_IRQ3_SHIFT                        1  
#define WM2200_DSP_IRQ3_WIDTH                        1  
#define WM2200_DSP_IRQ2                         0x0001  
#define WM2200_DSP_IRQ2_MASK                    0x0001  
#define WM2200_DSP_IRQ2_SHIFT                        0  
#define WM2200_DSP_IRQ2_WIDTH                        1  

#define WM2200_LDO1ENA_PD                       0x8000  
#define WM2200_LDO1ENA_PD_MASK                  0x8000  
#define WM2200_LDO1ENA_PD_SHIFT                     15  
#define WM2200_LDO1ENA_PD_WIDTH                      1  
#define WM2200_MCLK2_PD                         0x2000  
#define WM2200_MCLK2_PD_MASK                    0x2000  
#define WM2200_MCLK2_PD_SHIFT                       13  
#define WM2200_MCLK2_PD_WIDTH                        1  
#define WM2200_MCLK1_PD                         0x1000  
#define WM2200_MCLK1_PD_MASK                    0x1000  
#define WM2200_MCLK1_PD_SHIFT                       12  
#define WM2200_MCLK1_PD_WIDTH                        1  
#define WM2200_DACLRCLK1_PU                     0x0400  
#define WM2200_DACLRCLK1_PU_MASK                0x0400  
#define WM2200_DACLRCLK1_PU_SHIFT                   10  
#define WM2200_DACLRCLK1_PU_WIDTH                    1  
#define WM2200_DACLRCLK1_PD                     0x0200  
#define WM2200_DACLRCLK1_PD_MASK                0x0200  
#define WM2200_DACLRCLK1_PD_SHIFT                    9  
#define WM2200_DACLRCLK1_PD_WIDTH                    1  
#define WM2200_BCLK1_PU                         0x0100  
#define WM2200_BCLK1_PU_MASK                    0x0100  
#define WM2200_BCLK1_PU_SHIFT                        8  
#define WM2200_BCLK1_PU_WIDTH                        1  
#define WM2200_BCLK1_PD                         0x0080  
#define WM2200_BCLK1_PD_MASK                    0x0080  
#define WM2200_BCLK1_PD_SHIFT                        7  
#define WM2200_BCLK1_PD_WIDTH                        1  
#define WM2200_DACDAT1_PU                       0x0040  
#define WM2200_DACDAT1_PU_MASK                  0x0040  
#define WM2200_DACDAT1_PU_SHIFT                      6  
#define WM2200_DACDAT1_PU_WIDTH                      1  
#define WM2200_DACDAT1_PD                       0x0020  
#define WM2200_DACDAT1_PD_MASK                  0x0020  
#define WM2200_DACDAT1_PD_SHIFT                      5  
#define WM2200_DACDAT1_PD_WIDTH                      1  
#define WM2200_DMICDAT3_PD                      0x0010  
#define WM2200_DMICDAT3_PD_MASK                 0x0010  
#define WM2200_DMICDAT3_PD_SHIFT                     4  
#define WM2200_DMICDAT3_PD_WIDTH                     1  
#define WM2200_DMICDAT2_PD                      0x0008  
#define WM2200_DMICDAT2_PD_MASK                 0x0008  
#define WM2200_DMICDAT2_PD_SHIFT                     3  
#define WM2200_DMICDAT2_PD_WIDTH                     1  
#define WM2200_DMICDAT1_PD                      0x0004  
#define WM2200_DMICDAT1_PD_MASK                 0x0004  
#define WM2200_DMICDAT1_PD_SHIFT                     2  
#define WM2200_DMICDAT1_PD_WIDTH                     1  
#define WM2200_RSTB_PU                          0x0002  
#define WM2200_RSTB_PU_MASK                     0x0002  
#define WM2200_RSTB_PU_SHIFT                         1  
#define WM2200_RSTB_PU_WIDTH                         1  
#define WM2200_ADDR_PD                          0x0001  
#define WM2200_ADDR_PD_MASK                     0x0001  
#define WM2200_ADDR_PD_SHIFT                         0  
#define WM2200_ADDR_PD_WIDTH                         1  

#define WM2200_DSP_IRQ0_EINT                    0x0080  
#define WM2200_DSP_IRQ0_EINT_MASK               0x0080  
#define WM2200_DSP_IRQ0_EINT_SHIFT                   7  
#define WM2200_DSP_IRQ0_EINT_WIDTH                   1  
#define WM2200_DSP_IRQ1_EINT                    0x0040  
#define WM2200_DSP_IRQ1_EINT_MASK               0x0040  
#define WM2200_DSP_IRQ1_EINT_SHIFT                   6  
#define WM2200_DSP_IRQ1_EINT_WIDTH                   1  
#define WM2200_DSP_IRQ2_EINT                    0x0020  
#define WM2200_DSP_IRQ2_EINT_MASK               0x0020  
#define WM2200_DSP_IRQ2_EINT_SHIFT                   5  
#define WM2200_DSP_IRQ2_EINT_WIDTH                   1  
#define WM2200_DSP_IRQ3_EINT                    0x0010  
#define WM2200_DSP_IRQ3_EINT_MASK               0x0010  
#define WM2200_DSP_IRQ3_EINT_SHIFT                   4  
#define WM2200_DSP_IRQ3_EINT_WIDTH                   1  
#define WM2200_GP4_EINT                         0x0008  
#define WM2200_GP4_EINT_MASK                    0x0008  
#define WM2200_GP4_EINT_SHIFT                        3  
#define WM2200_GP4_EINT_WIDTH                        1  
#define WM2200_GP3_EINT                         0x0004  
#define WM2200_GP3_EINT_MASK                    0x0004  
#define WM2200_GP3_EINT_SHIFT                        2  
#define WM2200_GP3_EINT_WIDTH                        1  
#define WM2200_GP2_EINT                         0x0002  
#define WM2200_GP2_EINT_MASK                    0x0002  
#define WM2200_GP2_EINT_SHIFT                        1  
#define WM2200_GP2_EINT_WIDTH                        1  
#define WM2200_GP1_EINT                         0x0001  
#define WM2200_GP1_EINT_MASK                    0x0001  
#define WM2200_GP1_EINT_SHIFT                        0  
#define WM2200_GP1_EINT_WIDTH                        1  

#define WM2200_IM_DSP_IRQ0_EINT                 0x0080  
#define WM2200_IM_DSP_IRQ0_EINT_MASK            0x0080  
#define WM2200_IM_DSP_IRQ0_EINT_SHIFT                7  
#define WM2200_IM_DSP_IRQ0_EINT_WIDTH                1  
#define WM2200_IM_DSP_IRQ1_EINT                 0x0040  
#define WM2200_IM_DSP_IRQ1_EINT_MASK            0x0040  
#define WM2200_IM_DSP_IRQ1_EINT_SHIFT                6  
#define WM2200_IM_DSP_IRQ1_EINT_WIDTH                1  
#define WM2200_IM_DSP_IRQ2_EINT                 0x0020  
#define WM2200_IM_DSP_IRQ2_EINT_MASK            0x0020  
#define WM2200_IM_DSP_IRQ2_EINT_SHIFT                5  
#define WM2200_IM_DSP_IRQ2_EINT_WIDTH                1  
#define WM2200_IM_DSP_IRQ3_EINT                 0x0010  
#define WM2200_IM_DSP_IRQ3_EINT_MASK            0x0010  
#define WM2200_IM_DSP_IRQ3_EINT_SHIFT                4  
#define WM2200_IM_DSP_IRQ3_EINT_WIDTH                1  
#define WM2200_IM_GP4_EINT                      0x0008  
#define WM2200_IM_GP4_EINT_MASK                 0x0008  
#define WM2200_IM_GP4_EINT_SHIFT                     3  
#define WM2200_IM_GP4_EINT_WIDTH                     1  
#define WM2200_IM_GP3_EINT                      0x0004  
#define WM2200_IM_GP3_EINT_MASK                 0x0004  
#define WM2200_IM_GP3_EINT_SHIFT                     2  
#define WM2200_IM_GP3_EINT_WIDTH                     1  
#define WM2200_IM_GP2_EINT                      0x0002  
#define WM2200_IM_GP2_EINT_MASK                 0x0002  
#define WM2200_IM_GP2_EINT_SHIFT                     1  
#define WM2200_IM_GP2_EINT_WIDTH                     1  
#define WM2200_IM_GP1_EINT                      0x0001  
#define WM2200_IM_GP1_EINT_MASK                 0x0001  
#define WM2200_IM_GP1_EINT_SHIFT                     0  
#define WM2200_IM_GP1_EINT_WIDTH                     1  

#define WM2200_WSEQ_BUSY_EINT                   0x0100  
#define WM2200_WSEQ_BUSY_EINT_MASK              0x0100  
#define WM2200_WSEQ_BUSY_EINT_SHIFT                  8  
#define WM2200_WSEQ_BUSY_EINT_WIDTH                  1  
#define WM2200_FLL_LOCK_EINT                    0x0002  
#define WM2200_FLL_LOCK_EINT_MASK               0x0002  
#define WM2200_FLL_LOCK_EINT_SHIFT                   1  
#define WM2200_FLL_LOCK_EINT_WIDTH                   1  
#define WM2200_CLKGEN_EINT                      0x0001  
#define WM2200_CLKGEN_EINT_MASK                 0x0001  
#define WM2200_CLKGEN_EINT_SHIFT                     0  
#define WM2200_CLKGEN_EINT_WIDTH                     1  

#define WM2200_WSEQ_BUSY_STS                    0x0100  
#define WM2200_WSEQ_BUSY_STS_MASK               0x0100  
#define WM2200_WSEQ_BUSY_STS_SHIFT                   8  
#define WM2200_WSEQ_BUSY_STS_WIDTH                   1  
#define WM2200_FLL_LOCK_STS                     0x0002  
#define WM2200_FLL_LOCK_STS_MASK                0x0002  
#define WM2200_FLL_LOCK_STS_SHIFT                    1  
#define WM2200_FLL_LOCK_STS_WIDTH                    1  
#define WM2200_CLKGEN_STS                       0x0001  
#define WM2200_CLKGEN_STS_MASK                  0x0001  
#define WM2200_CLKGEN_STS_SHIFT                      0  
#define WM2200_CLKGEN_STS_WIDTH                      1  

#define WM2200_IM_WSEQ_BUSY_EINT                0x0100  
#define WM2200_IM_WSEQ_BUSY_EINT_MASK           0x0100  
#define WM2200_IM_WSEQ_BUSY_EINT_SHIFT               8  
#define WM2200_IM_WSEQ_BUSY_EINT_WIDTH               1  
#define WM2200_IM_FLL_LOCK_EINT                 0x0002  
#define WM2200_IM_FLL_LOCK_EINT_MASK            0x0002  
#define WM2200_IM_FLL_LOCK_EINT_SHIFT                1  
#define WM2200_IM_FLL_LOCK_EINT_WIDTH                1  
#define WM2200_IM_CLKGEN_EINT                   0x0001  
#define WM2200_IM_CLKGEN_EINT_MASK              0x0001  
#define WM2200_IM_CLKGEN_EINT_SHIFT                  0  
#define WM2200_IM_CLKGEN_EINT_WIDTH                  1  

#define WM2200_IM_IRQ                           0x0001  
#define WM2200_IM_IRQ_MASK                      0x0001  
#define WM2200_IM_IRQ_SHIFT                          0  
#define WM2200_IM_IRQ_WIDTH                          1  

#define WM2200_EQL_B1_GAIN_MASK                 0xF800  
#define WM2200_EQL_B1_GAIN_SHIFT                    11  
#define WM2200_EQL_B1_GAIN_WIDTH                     5  
#define WM2200_EQL_B2_GAIN_MASK                 0x07C0  
#define WM2200_EQL_B2_GAIN_SHIFT                     6  
#define WM2200_EQL_B2_GAIN_WIDTH                     5  
#define WM2200_EQL_B3_GAIN_MASK                 0x003E  
#define WM2200_EQL_B3_GAIN_SHIFT                     1  
#define WM2200_EQL_B3_GAIN_WIDTH                     5  
#define WM2200_EQL_ENA                          0x0001  
#define WM2200_EQL_ENA_MASK                     0x0001  
#define WM2200_EQL_ENA_SHIFT                         0  
#define WM2200_EQL_ENA_WIDTH                         1  

#define WM2200_EQL_B4_GAIN_MASK                 0xF800  
#define WM2200_EQL_B4_GAIN_SHIFT                    11  
#define WM2200_EQL_B4_GAIN_WIDTH                     5  
#define WM2200_EQL_B5_GAIN_MASK                 0x07C0  
#define WM2200_EQL_B5_GAIN_SHIFT                     6  
#define WM2200_EQL_B5_GAIN_WIDTH                     5  

#define WM2200_EQL_B1_A_MASK                    0xFFFF  
#define WM2200_EQL_B1_A_SHIFT                        0  
#define WM2200_EQL_B1_A_WIDTH                       16  

#define WM2200_EQL_B1_B_MASK                    0xFFFF  
#define WM2200_EQL_B1_B_SHIFT                        0  
#define WM2200_EQL_B1_B_WIDTH                       16  

#define WM2200_EQL_B1_PG_MASK                   0xFFFF  
#define WM2200_EQL_B1_PG_SHIFT                       0  
#define WM2200_EQL_B1_PG_WIDTH                      16  

#define WM2200_EQL_B2_A_MASK                    0xFFFF  
#define WM2200_EQL_B2_A_SHIFT                        0  
#define WM2200_EQL_B2_A_WIDTH                       16  

#define WM2200_EQL_B2_B_MASK                    0xFFFF  
#define WM2200_EQL_B2_B_SHIFT                        0  
#define WM2200_EQL_B2_B_WIDTH                       16  

#define WM2200_EQL_B2_C_MASK                    0xFFFF  
#define WM2200_EQL_B2_C_SHIFT                        0  
#define WM2200_EQL_B2_C_WIDTH                       16  

#define WM2200_EQL_B2_PG_MASK                   0xFFFF  
#define WM2200_EQL_B2_PG_SHIFT                       0  
#define WM2200_EQL_B2_PG_WIDTH                      16  

#define WM2200_EQL_B3_A_MASK                    0xFFFF  
#define WM2200_EQL_B3_A_SHIFT                        0  
#define WM2200_EQL_B3_A_WIDTH                       16  

#define WM2200_EQL_B3_B_MASK                    0xFFFF  
#define WM2200_EQL_B3_B_SHIFT                        0  
#define WM2200_EQL_B3_B_WIDTH                       16  

#define WM2200_EQL_B3_C_MASK                    0xFFFF  
#define WM2200_EQL_B3_C_SHIFT                        0  
#define WM2200_EQL_B3_C_WIDTH                       16  

#define WM2200_EQL_B3_PG_MASK                   0xFFFF  
#define WM2200_EQL_B3_PG_SHIFT                       0  
#define WM2200_EQL_B3_PG_WIDTH                      16  

#define WM2200_EQL_B4_A_MASK                    0xFFFF  
#define WM2200_EQL_B4_A_SHIFT                        0  
#define WM2200_EQL_B4_A_WIDTH                       16  

#define WM2200_EQL_B4_B_MASK                    0xFFFF  
#define WM2200_EQL_B4_B_SHIFT                        0  
#define WM2200_EQL_B4_B_WIDTH                       16  

#define WM2200_EQL_B4_C_MASK                    0xFFFF  
#define WM2200_EQL_B4_C_SHIFT                        0  
#define WM2200_EQL_B4_C_WIDTH                       16  

#define WM2200_EQL_B4_PG_MASK                   0xFFFF  
#define WM2200_EQL_B4_PG_SHIFT                       0  
#define WM2200_EQL_B4_PG_WIDTH                      16  

#define WM2200_EQL_B5_A_MASK                    0xFFFF  
#define WM2200_EQL_B5_A_SHIFT                        0  
#define WM2200_EQL_B5_A_WIDTH                       16  

#define WM2200_EQL_B5_B_MASK                    0xFFFF  
#define WM2200_EQL_B5_B_SHIFT                        0  
#define WM2200_EQL_B5_B_WIDTH                       16  

#define WM2200_EQL_B5_PG_MASK                   0xFFFF  
#define WM2200_EQL_B5_PG_SHIFT                       0  
#define WM2200_EQL_B5_PG_WIDTH                      16  

#define WM2200_EQR_B1_GAIN_MASK                 0xF800  
#define WM2200_EQR_B1_GAIN_SHIFT                    11  
#define WM2200_EQR_B1_GAIN_WIDTH                     5  
#define WM2200_EQR_B2_GAIN_MASK                 0x07C0  
#define WM2200_EQR_B2_GAIN_SHIFT                     6  
#define WM2200_EQR_B2_GAIN_WIDTH                     5  
#define WM2200_EQR_B3_GAIN_MASK                 0x003E  
#define WM2200_EQR_B3_GAIN_SHIFT                     1  
#define WM2200_EQR_B3_GAIN_WIDTH                     5  
#define WM2200_EQR_ENA                          0x0001  
#define WM2200_EQR_ENA_MASK                     0x0001  
#define WM2200_EQR_ENA_SHIFT                         0  
#define WM2200_EQR_ENA_WIDTH                         1  

#define WM2200_EQR_B4_GAIN_MASK                 0xF800  
#define WM2200_EQR_B4_GAIN_SHIFT                    11  
#define WM2200_EQR_B4_GAIN_WIDTH                     5  
#define WM2200_EQR_B5_GAIN_MASK                 0x07C0  
#define WM2200_EQR_B5_GAIN_SHIFT                     6  
#define WM2200_EQR_B5_GAIN_WIDTH                     5  

#define WM2200_EQR_B1_A_MASK                    0xFFFF  
#define WM2200_EQR_B1_A_SHIFT                        0  
#define WM2200_EQR_B1_A_WIDTH                       16  

#define WM2200_EQR_B1_B_MASK                    0xFFFF  
#define WM2200_EQR_B1_B_SHIFT                        0  
#define WM2200_EQR_B1_B_WIDTH                       16  

#define WM2200_EQR_B1_PG_MASK                   0xFFFF  
#define WM2200_EQR_B1_PG_SHIFT                       0  
#define WM2200_EQR_B1_PG_WIDTH                      16  

#define WM2200_EQR_B2_A_MASK                    0xFFFF  
#define WM2200_EQR_B2_A_SHIFT                        0  
#define WM2200_EQR_B2_A_WIDTH                       16  

#define WM2200_EQR_B2_B_MASK                    0xFFFF  
#define WM2200_EQR_B2_B_SHIFT                        0  
#define WM2200_EQR_B2_B_WIDTH                       16  

#define WM2200_EQR_B2_C_MASK                    0xFFFF  
#define WM2200_EQR_B2_C_SHIFT                        0  
#define WM2200_EQR_B2_C_WIDTH                       16  

#define WM2200_EQR_B2_PG_MASK                   0xFFFF  
#define WM2200_EQR_B2_PG_SHIFT                       0  
#define WM2200_EQR_B2_PG_WIDTH                      16  

#define WM2200_EQR_B3_A_MASK                    0xFFFF  
#define WM2200_EQR_B3_A_SHIFT                        0  
#define WM2200_EQR_B3_A_WIDTH                       16  

#define WM2200_EQR_B3_B_MASK                    0xFFFF  
#define WM2200_EQR_B3_B_SHIFT                        0  
#define WM2200_EQR_B3_B_WIDTH                       16  

#define WM2200_EQR_B3_C_MASK                    0xFFFF  
#define WM2200_EQR_B3_C_SHIFT                        0  
#define WM2200_EQR_B3_C_WIDTH                       16  

#define WM2200_EQR_B3_PG_MASK                   0xFFFF  
#define WM2200_EQR_B3_PG_SHIFT                       0  
#define WM2200_EQR_B3_PG_WIDTH                      16  

#define WM2200_EQR_B4_A_MASK                    0xFFFF  
#define WM2200_EQR_B4_A_SHIFT                        0  
#define WM2200_EQR_B4_A_WIDTH                       16  

#define WM2200_EQR_B4_B_MASK                    0xFFFF  
#define WM2200_EQR_B4_B_SHIFT                        0  
#define WM2200_EQR_B4_B_WIDTH                       16  

#define WM2200_EQR_B4_C_MASK                    0xFFFF  
#define WM2200_EQR_B4_C_SHIFT                        0  
#define WM2200_EQR_B4_C_WIDTH                       16  

#define WM2200_EQR_B4_PG_MASK                   0xFFFF  
#define WM2200_EQR_B4_PG_SHIFT                       0  
#define WM2200_EQR_B4_PG_WIDTH                      16  

#define WM2200_EQR_B5_A_MASK                    0xFFFF  
#define WM2200_EQR_B5_A_SHIFT                        0  
#define WM2200_EQR_B5_A_WIDTH                       16  

#define WM2200_EQR_B5_B_MASK                    0xFFFF  
#define WM2200_EQR_B5_B_SHIFT                        0  
#define WM2200_EQR_B5_B_WIDTH                       16  

#define WM2200_EQR_B5_PG_MASK                   0xFFFF  
#define WM2200_EQR_B5_PG_SHIFT                       0  
#define WM2200_EQR_B5_PG_WIDTH                      16  

#define WM2200_LHPF1_MODE                       0x0002  
#define WM2200_LHPF1_MODE_MASK                  0x0002  
#define WM2200_LHPF1_MODE_SHIFT                      1  
#define WM2200_LHPF1_MODE_WIDTH                      1  
#define WM2200_LHPF1_ENA                        0x0001  
#define WM2200_LHPF1_ENA_MASK                   0x0001  
#define WM2200_LHPF1_ENA_SHIFT                       0  
#define WM2200_LHPF1_ENA_WIDTH                       1  

#define WM2200_LHPF1_COEFF_MASK                 0xFFFF  
#define WM2200_LHPF1_COEFF_SHIFT                     0  
#define WM2200_LHPF1_COEFF_WIDTH                    16  

#define WM2200_LHPF2_MODE                       0x0002  
#define WM2200_LHPF2_MODE_MASK                  0x0002  
#define WM2200_LHPF2_MODE_SHIFT                      1  
#define WM2200_LHPF2_MODE_WIDTH                      1  
#define WM2200_LHPF2_ENA                        0x0001  
#define WM2200_LHPF2_ENA_MASK                   0x0001  
#define WM2200_LHPF2_ENA_SHIFT                       0  
#define WM2200_LHPF2_ENA_WIDTH                       1  

#define WM2200_LHPF2_COEFF_MASK                 0xFFFF  
#define WM2200_LHPF2_COEFF_SHIFT                     0  
#define WM2200_LHPF2_COEFF_WIDTH                    16  

#define WM2200_DSP1_RW_SEQUENCE_ENA             0x0001  
#define WM2200_DSP1_RW_SEQUENCE_ENA_MASK        0x0001  
#define WM2200_DSP1_RW_SEQUENCE_ENA_SHIFT            0  
#define WM2200_DSP1_RW_SEQUENCE_ENA_WIDTH            1  

#define WM2200_DSP1_PAGE_BASE_PM_0_MASK         0xFF00  
#define WM2200_DSP1_PAGE_BASE_PM_0_SHIFT             8  
#define WM2200_DSP1_PAGE_BASE_PM_0_WIDTH             8  

#define WM2200_DSP1_PAGE_BASE_DM_0_MASK         0xFF00  
#define WM2200_DSP1_PAGE_BASE_DM_0_SHIFT             8  
#define WM2200_DSP1_PAGE_BASE_DM_0_WIDTH             8  

#define WM2200_DSP1_PAGE_BASE_ZM_0_MASK         0xFF00  
#define WM2200_DSP1_PAGE_BASE_ZM_0_SHIFT             8  
#define WM2200_DSP1_PAGE_BASE_ZM_0_WIDTH             8  

#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_0_MASK 0x3FFF  
#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_0_SHIFT      0  
#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_0_WIDTH     14  

#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_1_MASK 0x3FFF  
#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_1_SHIFT      0  
#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_1_WIDTH     14  

#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_2_MASK 0x3FFF  
#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_2_SHIFT      0  
#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_2_WIDTH     14  

#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_3_MASK 0x3FFF  
#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_3_SHIFT      0  
#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_3_WIDTH     14  

#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_4_MASK 0x3FFF  
#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_4_SHIFT      0  
#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_4_WIDTH     14  

#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_5_MASK 0x3FFF  
#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_5_SHIFT      0  
#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_5_WIDTH     14  

#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_6_MASK 0x3FFF  
#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_6_SHIFT      0  
#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_6_WIDTH     14  

#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_7_MASK 0x3FFF  
#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_7_SHIFT      0  
#define WM2200_DSP1_START_ADDRESS_WDMA_BUFFER_7_WIDTH     14  

#define WM2200_DSP1_START_ADDRESS_RDMA_BUFFER_0_MASK 0x3FFF  
#define WM2200_DSP1_START_ADDRESS_RDMA_BUFFER_0_SHIFT      0  
#define WM2200_DSP1_START_ADDRESS_RDMA_BUFFER_0_WIDTH     14  

#define WM2200_DSP1_START_ADDRESS_RDMA_BUFFER_1_MASK 0x3FFF  
#define WM2200_DSP1_START_ADDRESS_RDMA_BUFFER_1_SHIFT      0  
#define WM2200_DSP1_START_ADDRESS_RDMA_BUFFER_1_WIDTH     14  

#define WM2200_DSP1_START_ADDRESS_RDMA_BUFFER_2_MASK 0x3FFF  
#define WM2200_DSP1_START_ADDRESS_RDMA_BUFFER_2_SHIFT      0  
#define WM2200_DSP1_START_ADDRESS_RDMA_BUFFER_2_WIDTH     14  

#define WM2200_DSP1_START_ADDRESS_RDMA_BUFFER_3_MASK 0x3FFF  
#define WM2200_DSP1_START_ADDRESS_RDMA_BUFFER_3_SHIFT      0  
#define WM2200_DSP1_START_ADDRESS_RDMA_BUFFER_3_WIDTH     14  

#define WM2200_DSP1_START_ADDRESS_RDMA_BUFFER_4_MASK 0x3FFF  
#define WM2200_DSP1_START_ADDRESS_RDMA_BUFFER_4_SHIFT      0  
#define WM2200_DSP1_START_ADDRESS_RDMA_BUFFER_4_WIDTH     14  

#define WM2200_DSP1_START_ADDRESS_RDMA_BUFFER_5_MASK 0x3FFF  
#define WM2200_DSP1_START_ADDRESS_RDMA_BUFFER_5_SHIFT      0  
#define WM2200_DSP1_START_ADDRESS_RDMA_BUFFER_5_WIDTH     14  

#define WM2200_DSP1_WDMA_BUFFER_LENGTH_MASK     0x00FF  
#define WM2200_DSP1_WDMA_BUFFER_LENGTH_SHIFT         0  
#define WM2200_DSP1_WDMA_BUFFER_LENGTH_WIDTH         8  

#define WM2200_DSP1_WDMA_CHANNEL_ENABLE_MASK    0x00FF  
#define WM2200_DSP1_WDMA_CHANNEL_ENABLE_SHIFT        0  
#define WM2200_DSP1_WDMA_CHANNEL_ENABLE_WIDTH        8  

#define WM2200_DSP1_RDMA_CHANNEL_ENABLE_MASK    0x003F  
#define WM2200_DSP1_RDMA_CHANNEL_ENABLE_SHIFT        0  
#define WM2200_DSP1_RDMA_CHANNEL_ENABLE_WIDTH        6  

#define WM2200_DSP1_DM_SIZE_MASK                0xFFFF  
#define WM2200_DSP1_DM_SIZE_SHIFT                    0  
#define WM2200_DSP1_DM_SIZE_WIDTH                   16  

#define WM2200_DSP1_PM_SIZE_MASK                0xFFFF  
#define WM2200_DSP1_PM_SIZE_SHIFT                    0  
#define WM2200_DSP1_PM_SIZE_WIDTH                   16  

#define WM2200_DSP1_ZM_SIZE_MASK                0xFFFF  
#define WM2200_DSP1_ZM_SIZE_SHIFT                    0  
#define WM2200_DSP1_ZM_SIZE_WIDTH                   16  

#define WM2200_DSP1_PING_FULL                   0x8000  
#define WM2200_DSP1_PING_FULL_MASK              0x8000  
#define WM2200_DSP1_PING_FULL_SHIFT                 15  
#define WM2200_DSP1_PING_FULL_WIDTH                  1  
#define WM2200_DSP1_PONG_FULL                   0x4000  
#define WM2200_DSP1_PONG_FULL_MASK              0x4000  
#define WM2200_DSP1_PONG_FULL_SHIFT                 14  
#define WM2200_DSP1_PONG_FULL_WIDTH                  1  
#define WM2200_DSP1_WDMA_ACTIVE_CHANNELS_MASK   0x00FF  
#define WM2200_DSP1_WDMA_ACTIVE_CHANNELS_SHIFT       0  
#define WM2200_DSP1_WDMA_ACTIVE_CHANNELS_WIDTH       8  

#define WM2200_DSP1_SCRATCH_0_MASK              0xFFFF  
#define WM2200_DSP1_SCRATCH_0_SHIFT                  0  
#define WM2200_DSP1_SCRATCH_0_WIDTH                 16  

#define WM2200_DSP1_SCRATCH_1_MASK              0xFFFF  
#define WM2200_DSP1_SCRATCH_1_SHIFT                  0  
#define WM2200_DSP1_SCRATCH_1_WIDTH                 16  

#define WM2200_DSP1_SCRATCH_2_MASK              0xFFFF  
#define WM2200_DSP1_SCRATCH_2_SHIFT                  0  
#define WM2200_DSP1_SCRATCH_2_WIDTH                 16  

#define WM2200_DSP1_SCRATCH_3_MASK              0xFFFF  
#define WM2200_DSP1_SCRATCH_3_SHIFT                  0  
#define WM2200_DSP1_SCRATCH_3_WIDTH                 16  

#define WM2200_DSP1_DBG_CLK_ENA                 0x0008  
#define WM2200_DSP1_DBG_CLK_ENA_MASK            0x0008  
#define WM2200_DSP1_DBG_CLK_ENA_SHIFT                3  
#define WM2200_DSP1_DBG_CLK_ENA_WIDTH                1  
#define WM2200_DSP1_SYS_ENA                     0x0004  
#define WM2200_DSP1_SYS_ENA_MASK                0x0004  
#define WM2200_DSP1_SYS_ENA_SHIFT                    2  
#define WM2200_DSP1_SYS_ENA_WIDTH                    1  
#define WM2200_DSP1_CORE_ENA                    0x0002  
#define WM2200_DSP1_CORE_ENA_MASK               0x0002  
#define WM2200_DSP1_CORE_ENA_SHIFT                   1  
#define WM2200_DSP1_CORE_ENA_WIDTH                   1  
#define WM2200_DSP1_START                       0x0001  
#define WM2200_DSP1_START_MASK                  0x0001  
#define WM2200_DSP1_START_SHIFT                      0  
#define WM2200_DSP1_START_WIDTH                      1  

#define WM2200_DSP1_CLK_RATE_MASK               0x0018  
#define WM2200_DSP1_CLK_RATE_SHIFT                   3  
#define WM2200_DSP1_CLK_RATE_WIDTH                   2  
#define WM2200_DSP1_CLK_AVAIL                   0x0004  
#define WM2200_DSP1_CLK_AVAIL_MASK              0x0004  
#define WM2200_DSP1_CLK_AVAIL_SHIFT                  2  
#define WM2200_DSP1_CLK_AVAIL_WIDTH                  1  
#define WM2200_DSP1_CLK_REQ_MASK                0x0003  
#define WM2200_DSP1_CLK_REQ_SHIFT                    0  
#define WM2200_DSP1_CLK_REQ_WIDTH                    2  

#define WM2200_DSP2_RW_SEQUENCE_ENA             0x0001  
#define WM2200_DSP2_RW_SEQUENCE_ENA_MASK        0x0001  
#define WM2200_DSP2_RW_SEQUENCE_ENA_SHIFT            0  
#define WM2200_DSP2_RW_SEQUENCE_ENA_WIDTH            1  

#define WM2200_DSP2_PAGE_BASE_PM_0_MASK         0xFF00  
#define WM2200_DSP2_PAGE_BASE_PM_0_SHIFT             8  
#define WM2200_DSP2_PAGE_BASE_PM_0_WIDTH             8  

#define WM2200_DSP2_PAGE_BASE_DM_0_MASK         0xFF00  
#define WM2200_DSP2_PAGE_BASE_DM_0_SHIFT             8  
#define WM2200_DSP2_PAGE_BASE_DM_0_WIDTH             8  

#define WM2200_DSP2_PAGE_BASE_ZM_0_MASK         0xFF00  
#define WM2200_DSP2_PAGE_BASE_ZM_0_SHIFT             8  
#define WM2200_DSP2_PAGE_BASE_ZM_0_WIDTH             8  

#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_0_MASK 0x3FFF  
#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_0_SHIFT      0  
#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_0_WIDTH     14  

#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_1_MASK 0x3FFF  
#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_1_SHIFT      0  
#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_1_WIDTH     14  

#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_2_MASK 0x3FFF  
#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_2_SHIFT      0  
#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_2_WIDTH     14  

#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_3_MASK 0x3FFF  
#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_3_SHIFT      0  
#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_3_WIDTH     14  

#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_4_MASK 0x3FFF  
#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_4_SHIFT      0  
#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_4_WIDTH     14  

#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_5_MASK 0x3FFF  
#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_5_SHIFT      0  
#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_5_WIDTH     14  

#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_6_MASK 0x3FFF  
#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_6_SHIFT      0  
#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_6_WIDTH     14  

#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_7_MASK 0x3FFF  
#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_7_SHIFT      0  
#define WM2200_DSP2_START_ADDRESS_WDMA_BUFFER_7_WIDTH     14  

#define WM2200_DSP2_START_ADDRESS_RDMA_BUFFER_0_MASK 0x3FFF  
#define WM2200_DSP2_START_ADDRESS_RDMA_BUFFER_0_SHIFT      0  
#define WM2200_DSP2_START_ADDRESS_RDMA_BUFFER_0_WIDTH     14  

#define WM2200_DSP2_START_ADDRESS_RDMA_BUFFER_1_MASK 0x3FFF  
#define WM2200_DSP2_START_ADDRESS_RDMA_BUFFER_1_SHIFT      0  
#define WM2200_DSP2_START_ADDRESS_RDMA_BUFFER_1_WIDTH     14  

#define WM2200_DSP2_START_ADDRESS_RDMA_BUFFER_2_MASK 0x3FFF  
#define WM2200_DSP2_START_ADDRESS_RDMA_BUFFER_2_SHIFT      0  
#define WM2200_DSP2_START_ADDRESS_RDMA_BUFFER_2_WIDTH     14  

#define WM2200_DSP2_START_ADDRESS_RDMA_BUFFER_3_MASK 0x3FFF  
#define WM2200_DSP2_START_ADDRESS_RDMA_BUFFER_3_SHIFT      0  
#define WM2200_DSP2_START_ADDRESS_RDMA_BUFFER_3_WIDTH     14  

#define WM2200_DSP2_START_ADDRESS_RDMA_BUFFER_4_MASK 0x3FFF  
#define WM2200_DSP2_START_ADDRESS_RDMA_BUFFER_4_SHIFT      0  
#define WM2200_DSP2_START_ADDRESS_RDMA_BUFFER_4_WIDTH     14  

#define WM2200_DSP2_START_ADDRESS_RDMA_BUFFER_5_MASK 0x3FFF  
#define WM2200_DSP2_START_ADDRESS_RDMA_BUFFER_5_SHIFT      0  
#define WM2200_DSP2_START_ADDRESS_RDMA_BUFFER_5_WIDTH     14  

#define WM2200_DSP2_WDMA_BUFFER_LENGTH_MASK     0x00FF  
#define WM2200_DSP2_WDMA_BUFFER_LENGTH_SHIFT         0  
#define WM2200_DSP2_WDMA_BUFFER_LENGTH_WIDTH         8  

#define WM2200_DSP2_WDMA_CHANNEL_ENABLE_MASK    0x00FF  
#define WM2200_DSP2_WDMA_CHANNEL_ENABLE_SHIFT        0  
#define WM2200_DSP2_WDMA_CHANNEL_ENABLE_WIDTH        8  

#define WM2200_DSP2_RDMA_CHANNEL_ENABLE_MASK    0x003F  
#define WM2200_DSP2_RDMA_CHANNEL_ENABLE_SHIFT        0  
#define WM2200_DSP2_RDMA_CHANNEL_ENABLE_WIDTH        6  

#define WM2200_DSP2_DM_SIZE_MASK                0xFFFF  
#define WM2200_DSP2_DM_SIZE_SHIFT                    0  
#define WM2200_DSP2_DM_SIZE_WIDTH                   16  

#define WM2200_DSP2_PM_SIZE_MASK                0xFFFF  
#define WM2200_DSP2_PM_SIZE_SHIFT                    0  
#define WM2200_DSP2_PM_SIZE_WIDTH                   16  

#define WM2200_DSP2_ZM_SIZE_MASK                0xFFFF  
#define WM2200_DSP2_ZM_SIZE_SHIFT                    0  
#define WM2200_DSP2_ZM_SIZE_WIDTH                   16  

#define WM2200_DSP2_PING_FULL                   0x8000  
#define WM2200_DSP2_PING_FULL_MASK              0x8000  
#define WM2200_DSP2_PING_FULL_SHIFT                 15  
#define WM2200_DSP2_PING_FULL_WIDTH                  1  
#define WM2200_DSP2_PONG_FULL                   0x4000  
#define WM2200_DSP2_PONG_FULL_MASK              0x4000  
#define WM2200_DSP2_PONG_FULL_SHIFT                 14  
#define WM2200_DSP2_PONG_FULL_WIDTH                  1  
#define WM2200_DSP2_WDMA_ACTIVE_CHANNELS_MASK   0x00FF  
#define WM2200_DSP2_WDMA_ACTIVE_CHANNELS_SHIFT       0  
#define WM2200_DSP2_WDMA_ACTIVE_CHANNELS_WIDTH       8  

#define WM2200_DSP2_SCRATCH_0_MASK              0xFFFF  
#define WM2200_DSP2_SCRATCH_0_SHIFT                  0  
#define WM2200_DSP2_SCRATCH_0_WIDTH                 16  

#define WM2200_DSP2_SCRATCH_1_MASK              0xFFFF  
#define WM2200_DSP2_SCRATCH_1_SHIFT                  0  
#define WM2200_DSP2_SCRATCH_1_WIDTH                 16  

#define WM2200_DSP2_SCRATCH_2_MASK              0xFFFF  
#define WM2200_DSP2_SCRATCH_2_SHIFT                  0  
#define WM2200_DSP2_SCRATCH_2_WIDTH                 16  

#define WM2200_DSP2_SCRATCH_3_MASK              0xFFFF  
#define WM2200_DSP2_SCRATCH_3_SHIFT                  0  
#define WM2200_DSP2_SCRATCH_3_WIDTH                 16  

#define WM2200_DSP2_DBG_CLK_ENA                 0x0008  
#define WM2200_DSP2_DBG_CLK_ENA_MASK            0x0008  
#define WM2200_DSP2_DBG_CLK_ENA_SHIFT                3  
#define WM2200_DSP2_DBG_CLK_ENA_WIDTH                1  
#define WM2200_DSP2_SYS_ENA                     0x0004  
#define WM2200_DSP2_SYS_ENA_MASK                0x0004  
#define WM2200_DSP2_SYS_ENA_SHIFT                    2  
#define WM2200_DSP2_SYS_ENA_WIDTH                    1  
#define WM2200_DSP2_CORE_ENA                    0x0002  
#define WM2200_DSP2_CORE_ENA_MASK               0x0002  
#define WM2200_DSP2_CORE_ENA_SHIFT                   1  
#define WM2200_DSP2_CORE_ENA_WIDTH                   1  
#define WM2200_DSP2_START                       0x0001  
#define WM2200_DSP2_START_MASK                  0x0001  
#define WM2200_DSP2_START_SHIFT                      0  
#define WM2200_DSP2_START_WIDTH                      1  

#define WM2200_DSP2_CLK_RATE_MASK               0x0018  
#define WM2200_DSP2_CLK_RATE_SHIFT                   3  
#define WM2200_DSP2_CLK_RATE_WIDTH                   2  
#define WM2200_DSP2_CLK_AVAIL                   0x0004  
#define WM2200_DSP2_CLK_AVAIL_MASK              0x0004  
#define WM2200_DSP2_CLK_AVAIL_SHIFT                  2  
#define WM2200_DSP2_CLK_AVAIL_WIDTH                  1  
#define WM2200_DSP2_CLK_REQ_MASK                0x0003  
#define WM2200_DSP2_CLK_REQ_SHIFT                    0  
#define WM2200_DSP2_CLK_REQ_WIDTH                    2  

#endif
