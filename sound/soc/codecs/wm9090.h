/*
 * ALSA SoC WM9090 driver
 *
 * Copyright 2009 Wolfson Microelectronics
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef __WM9090_H
#define __WM9090_H

#define WM9090_SOFTWARE_RESET                   0x00
#define WM9090_POWER_MANAGEMENT_1               0x01
#define WM9090_POWER_MANAGEMENT_2               0x02
#define WM9090_POWER_MANAGEMENT_3               0x03
#define WM9090_CLOCKING_1                       0x06
#define WM9090_IN1_LINE_CONTROL                 0x16
#define WM9090_IN2_LINE_CONTROL                 0x17
#define WM9090_IN1_LINE_INPUT_A_VOLUME          0x18
#define WM9090_IN1_LINE_INPUT_B_VOLUME          0x19
#define WM9090_IN2_LINE_INPUT_A_VOLUME          0x1A
#define WM9090_IN2_LINE_INPUT_B_VOLUME          0x1B
#define WM9090_LEFT_OUTPUT_VOLUME               0x1C
#define WM9090_RIGHT_OUTPUT_VOLUME              0x1D
#define WM9090_SPKMIXL_ATTENUATION              0x22
#define WM9090_SPKOUT_MIXERS                    0x24
#define WM9090_CLASSD3                          0x25
#define WM9090_SPEAKER_VOLUME_LEFT              0x26
#define WM9090_OUTPUT_MIXER1                    0x2D
#define WM9090_OUTPUT_MIXER2                    0x2E
#define WM9090_OUTPUT_MIXER3                    0x2F
#define WM9090_OUTPUT_MIXER4                    0x30
#define WM9090_SPEAKER_MIXER                    0x36
#define WM9090_ANTIPOP2                         0x39
#define WM9090_WRITE_SEQUENCER_0                0x46
#define WM9090_WRITE_SEQUENCER_1                0x47
#define WM9090_WRITE_SEQUENCER_2                0x48
#define WM9090_WRITE_SEQUENCER_3                0x49
#define WM9090_WRITE_SEQUENCER_4                0x4A
#define WM9090_WRITE_SEQUENCER_5                0x4B
#define WM9090_CHARGE_PUMP_1                    0x4C
#define WM9090_DC_SERVO_0                       0x54
#define WM9090_DC_SERVO_1                       0x55
#define WM9090_DC_SERVO_3                       0x57
#define WM9090_DC_SERVO_READBACK_0              0x58
#define WM9090_DC_SERVO_READBACK_1              0x59
#define WM9090_DC_SERVO_READBACK_2              0x5A
#define WM9090_ANALOGUE_HP_0                    0x60
#define WM9090_AGC_CONTROL_0                    0x62
#define WM9090_AGC_CONTROL_1                    0x63
#define WM9090_AGC_CONTROL_2                    0x64

#define WM9090_REGISTER_COUNT                   40
#define WM9090_MAX_REGISTER                     0x64


#define WM9090_SW_RESET_MASK                    0xFFFF  
#define WM9090_SW_RESET_SHIFT                        0  
#define WM9090_SW_RESET_WIDTH                       16  

#define WM9090_SPKOUTL_ENA                      0x1000  
#define WM9090_SPKOUTL_ENA_MASK                 0x1000  
#define WM9090_SPKOUTL_ENA_SHIFT                    12  
#define WM9090_SPKOUTL_ENA_WIDTH                     1  
#define WM9090_HPOUT1L_ENA                      0x0200  
#define WM9090_HPOUT1L_ENA_MASK                 0x0200  
#define WM9090_HPOUT1L_ENA_SHIFT                     9  
#define WM9090_HPOUT1L_ENA_WIDTH                     1  
#define WM9090_HPOUT1R_ENA                      0x0100  
#define WM9090_HPOUT1R_ENA_MASK                 0x0100  
#define WM9090_HPOUT1R_ENA_SHIFT                     8  
#define WM9090_HPOUT1R_ENA_WIDTH                     1  
#define WM9090_OSC_ENA                          0x0008  
#define WM9090_OSC_ENA_MASK                     0x0008  
#define WM9090_OSC_ENA_SHIFT                         3  
#define WM9090_OSC_ENA_WIDTH                         1  
#define WM9090_VMID_RES_MASK                    0x0006  
#define WM9090_VMID_RES_SHIFT                        1  
#define WM9090_VMID_RES_WIDTH                        2  
#define WM9090_BIAS_ENA                         0x0001  
#define WM9090_BIAS_ENA_MASK                    0x0001  
#define WM9090_BIAS_ENA_SHIFT                        0  
#define WM9090_BIAS_ENA_WIDTH                        1  

#define WM9090_TSHUT                            0x8000  
#define WM9090_TSHUT_MASK                       0x8000  
#define WM9090_TSHUT_SHIFT                          15  
#define WM9090_TSHUT_WIDTH                           1  
#define WM9090_TSHUT_ENA                        0x4000  
#define WM9090_TSHUT_ENA_MASK                   0x4000  
#define WM9090_TSHUT_ENA_SHIFT                      14  
#define WM9090_TSHUT_ENA_WIDTH                       1  
#define WM9090_TSHUT_OPDIS                      0x2000  
#define WM9090_TSHUT_OPDIS_MASK                 0x2000  
#define WM9090_TSHUT_OPDIS_SHIFT                    13  
#define WM9090_TSHUT_OPDIS_WIDTH                     1  
#define WM9090_IN1A_ENA                         0x0080  
#define WM9090_IN1A_ENA_MASK                    0x0080  
#define WM9090_IN1A_ENA_SHIFT                        7  
#define WM9090_IN1A_ENA_WIDTH                        1  
#define WM9090_IN1B_ENA                         0x0040  
#define WM9090_IN1B_ENA_MASK                    0x0040  
#define WM9090_IN1B_ENA_SHIFT                        6  
#define WM9090_IN1B_ENA_WIDTH                        1  
#define WM9090_IN2A_ENA                         0x0020  
#define WM9090_IN2A_ENA_MASK                    0x0020  
#define WM9090_IN2A_ENA_SHIFT                        5  
#define WM9090_IN2A_ENA_WIDTH                        1  
#define WM9090_IN2B_ENA                         0x0010  
#define WM9090_IN2B_ENA_MASK                    0x0010  
#define WM9090_IN2B_ENA_SHIFT                        4  
#define WM9090_IN2B_ENA_WIDTH                        1  

#define WM9090_AGC_ENA                          0x4000  
#define WM9090_AGC_ENA_MASK                     0x4000  
#define WM9090_AGC_ENA_SHIFT                        14  
#define WM9090_AGC_ENA_WIDTH                         1  
#define WM9090_SPKLVOL_ENA                      0x0100  
#define WM9090_SPKLVOL_ENA_MASK                 0x0100  
#define WM9090_SPKLVOL_ENA_SHIFT                     8  
#define WM9090_SPKLVOL_ENA_WIDTH                     1  
#define WM9090_MIXOUTL_ENA                      0x0020  
#define WM9090_MIXOUTL_ENA_MASK                 0x0020  
#define WM9090_MIXOUTL_ENA_SHIFT                     5  
#define WM9090_MIXOUTL_ENA_WIDTH                     1  
#define WM9090_MIXOUTR_ENA                      0x0010  
#define WM9090_MIXOUTR_ENA_MASK                 0x0010  
#define WM9090_MIXOUTR_ENA_SHIFT                     4  
#define WM9090_MIXOUTR_ENA_WIDTH                     1  
#define WM9090_SPKMIX_ENA                       0x0008  
#define WM9090_SPKMIX_ENA_MASK                  0x0008  
#define WM9090_SPKMIX_ENA_SHIFT                      3  
#define WM9090_SPKMIX_ENA_WIDTH                      1  

#define WM9090_TOCLK_RATE                       0x8000  
#define WM9090_TOCLK_RATE_MASK                  0x8000  
#define WM9090_TOCLK_RATE_SHIFT                     15  
#define WM9090_TOCLK_RATE_WIDTH                      1  
#define WM9090_TOCLK_ENA                        0x4000  
#define WM9090_TOCLK_ENA_MASK                   0x4000  
#define WM9090_TOCLK_ENA_SHIFT                      14  
#define WM9090_TOCLK_ENA_WIDTH                       1  

#define WM9090_IN1_DIFF                         0x0002  
#define WM9090_IN1_DIFF_MASK                    0x0002  
#define WM9090_IN1_DIFF_SHIFT                        1  
#define WM9090_IN1_DIFF_WIDTH                        1  
#define WM9090_IN1_CLAMP                        0x0001  
#define WM9090_IN1_CLAMP_MASK                   0x0001  
#define WM9090_IN1_CLAMP_SHIFT                       0  
#define WM9090_IN1_CLAMP_WIDTH                       1  

#define WM9090_IN2_DIFF                         0x0002  
#define WM9090_IN2_DIFF_MASK                    0x0002  
#define WM9090_IN2_DIFF_SHIFT                        1  
#define WM9090_IN2_DIFF_WIDTH                        1  
#define WM9090_IN2_CLAMP                        0x0001  
#define WM9090_IN2_CLAMP_MASK                   0x0001  
#define WM9090_IN2_CLAMP_SHIFT                       0  
#define WM9090_IN2_CLAMP_WIDTH                       1  

#define WM9090_IN1_VU                           0x0100  
#define WM9090_IN1_VU_MASK                      0x0100  
#define WM9090_IN1_VU_SHIFT                          8  
#define WM9090_IN1_VU_WIDTH                          1  
#define WM9090_IN1A_MUTE                        0x0080  
#define WM9090_IN1A_MUTE_MASK                   0x0080  
#define WM9090_IN1A_MUTE_SHIFT                       7  
#define WM9090_IN1A_MUTE_WIDTH                       1  
#define WM9090_IN1A_ZC                          0x0040  
#define WM9090_IN1A_ZC_MASK                     0x0040  
#define WM9090_IN1A_ZC_SHIFT                         6  
#define WM9090_IN1A_ZC_WIDTH                         1  
#define WM9090_IN1A_VOL_MASK                    0x0007  
#define WM9090_IN1A_VOL_SHIFT                        0  
#define WM9090_IN1A_VOL_WIDTH                        3  

#define WM9090_IN1_VU                           0x0100  
#define WM9090_IN1_VU_MASK                      0x0100  
#define WM9090_IN1_VU_SHIFT                          8  
#define WM9090_IN1_VU_WIDTH                          1  
#define WM9090_IN1B_MUTE                        0x0080  
#define WM9090_IN1B_MUTE_MASK                   0x0080  
#define WM9090_IN1B_MUTE_SHIFT                       7  
#define WM9090_IN1B_MUTE_WIDTH                       1  
#define WM9090_IN1B_ZC                          0x0040  
#define WM9090_IN1B_ZC_MASK                     0x0040  
#define WM9090_IN1B_ZC_SHIFT                         6  
#define WM9090_IN1B_ZC_WIDTH                         1  
#define WM9090_IN1B_VOL_MASK                    0x0007  
#define WM9090_IN1B_VOL_SHIFT                        0  
#define WM9090_IN1B_VOL_WIDTH                        3  

#define WM9090_IN2_VU                           0x0100  
#define WM9090_IN2_VU_MASK                      0x0100  
#define WM9090_IN2_VU_SHIFT                          8  
#define WM9090_IN2_VU_WIDTH                          1  
#define WM9090_IN2A_MUTE                        0x0080  
#define WM9090_IN2A_MUTE_MASK                   0x0080  
#define WM9090_IN2A_MUTE_SHIFT                       7  
#define WM9090_IN2A_MUTE_WIDTH                       1  
#define WM9090_IN2A_ZC                          0x0040  
#define WM9090_IN2A_ZC_MASK                     0x0040  
#define WM9090_IN2A_ZC_SHIFT                         6  
#define WM9090_IN2A_ZC_WIDTH                         1  
#define WM9090_IN2A_VOL_MASK                    0x0007  
#define WM9090_IN2A_VOL_SHIFT                        0  
#define WM9090_IN2A_VOL_WIDTH                        3  

#define WM9090_IN2_VU                           0x0100  
#define WM9090_IN2_VU_MASK                      0x0100  
#define WM9090_IN2_VU_SHIFT                          8  
#define WM9090_IN2_VU_WIDTH                          1  
#define WM9090_IN2B_MUTE                        0x0080  
#define WM9090_IN2B_MUTE_MASK                   0x0080  
#define WM9090_IN2B_MUTE_SHIFT                       7  
#define WM9090_IN2B_MUTE_WIDTH                       1  
#define WM9090_IN2B_ZC                          0x0040  
#define WM9090_IN2B_ZC_MASK                     0x0040  
#define WM9090_IN2B_ZC_SHIFT                         6  
#define WM9090_IN2B_ZC_WIDTH                         1  
#define WM9090_IN2B_VOL_MASK                    0x0007  
#define WM9090_IN2B_VOL_SHIFT                        0  
#define WM9090_IN2B_VOL_WIDTH                        3  

#define WM9090_HPOUT1_VU                        0x0100  
#define WM9090_HPOUT1_VU_MASK                   0x0100  
#define WM9090_HPOUT1_VU_SHIFT                       8  
#define WM9090_HPOUT1_VU_WIDTH                       1  
#define WM9090_HPOUT1L_ZC                       0x0080  
#define WM9090_HPOUT1L_ZC_MASK                  0x0080  
#define WM9090_HPOUT1L_ZC_SHIFT                      7  
#define WM9090_HPOUT1L_ZC_WIDTH                      1  
#define WM9090_HPOUT1L_MUTE                     0x0040  
#define WM9090_HPOUT1L_MUTE_MASK                0x0040  
#define WM9090_HPOUT1L_MUTE_SHIFT                    6  
#define WM9090_HPOUT1L_MUTE_WIDTH                    1  
#define WM9090_HPOUT1L_VOL_MASK                 0x003F  
#define WM9090_HPOUT1L_VOL_SHIFT                     0  
#define WM9090_HPOUT1L_VOL_WIDTH                     6  

#define WM9090_HPOUT1_VU                        0x0100  
#define WM9090_HPOUT1_VU_MASK                   0x0100  
#define WM9090_HPOUT1_VU_SHIFT                       8  
#define WM9090_HPOUT1_VU_WIDTH                       1  
#define WM9090_HPOUT1R_ZC                       0x0080  
#define WM9090_HPOUT1R_ZC_MASK                  0x0080  
#define WM9090_HPOUT1R_ZC_SHIFT                      7  
#define WM9090_HPOUT1R_ZC_WIDTH                      1  
#define WM9090_HPOUT1R_MUTE                     0x0040  
#define WM9090_HPOUT1R_MUTE_MASK                0x0040  
#define WM9090_HPOUT1R_MUTE_SHIFT                    6  
#define WM9090_HPOUT1R_MUTE_WIDTH                    1  
#define WM9090_HPOUT1R_VOL_MASK                 0x003F  
#define WM9090_HPOUT1R_VOL_SHIFT                     0  
#define WM9090_HPOUT1R_VOL_WIDTH                     6  

#define WM9090_SPKMIX_MUTE                      0x0100  
#define WM9090_SPKMIX_MUTE_MASK                 0x0100  
#define WM9090_SPKMIX_MUTE_SHIFT                     8  
#define WM9090_SPKMIX_MUTE_WIDTH                     1  
#define WM9090_IN1A_SPKMIX_VOL_MASK             0x00C0  
#define WM9090_IN1A_SPKMIX_VOL_SHIFT                 6  
#define WM9090_IN1A_SPKMIX_VOL_WIDTH                 2  
#define WM9090_IN1B_SPKMIX_VOL_MASK             0x0030  
#define WM9090_IN1B_SPKMIX_VOL_SHIFT                 4  
#define WM9090_IN1B_SPKMIX_VOL_WIDTH                 2  
#define WM9090_IN2A_SPKMIX_VOL_MASK             0x000C  
#define WM9090_IN2A_SPKMIX_VOL_SHIFT                 2  
#define WM9090_IN2A_SPKMIX_VOL_WIDTH                 2  
#define WM9090_IN2B_SPKMIX_VOL_MASK             0x0003  
#define WM9090_IN2B_SPKMIX_VOL_SHIFT                 0  
#define WM9090_IN2B_SPKMIX_VOL_WIDTH                 2  

#define WM9090_SPKMIXL_TO_SPKOUTL               0x0010  
#define WM9090_SPKMIXL_TO_SPKOUTL_MASK          0x0010  
#define WM9090_SPKMIXL_TO_SPKOUTL_SHIFT              4  
#define WM9090_SPKMIXL_TO_SPKOUTL_WIDTH              1  

#define WM9090_SPKOUTL_BOOST_MASK               0x0038  
#define WM9090_SPKOUTL_BOOST_SHIFT                   3  
#define WM9090_SPKOUTL_BOOST_WIDTH                   3  

#define WM9090_SPKOUT_VU                        0x0100  
#define WM9090_SPKOUT_VU_MASK                   0x0100  
#define WM9090_SPKOUT_VU_SHIFT                       8  
#define WM9090_SPKOUT_VU_WIDTH                       1  
#define WM9090_SPKOUTL_ZC                       0x0080  
#define WM9090_SPKOUTL_ZC_MASK                  0x0080  
#define WM9090_SPKOUTL_ZC_SHIFT                      7  
#define WM9090_SPKOUTL_ZC_WIDTH                      1  
#define WM9090_SPKOUTL_MUTE                     0x0040  
#define WM9090_SPKOUTL_MUTE_MASK                0x0040  
#define WM9090_SPKOUTL_MUTE_SHIFT                    6  
#define WM9090_SPKOUTL_MUTE_WIDTH                    1  
#define WM9090_SPKOUTL_VOL_MASK                 0x003F  
#define WM9090_SPKOUTL_VOL_SHIFT                     0  
#define WM9090_SPKOUTL_VOL_WIDTH                     6  

#define WM9090_IN1A_TO_MIXOUTL                  0x0040  
#define WM9090_IN1A_TO_MIXOUTL_MASK             0x0040  
#define WM9090_IN1A_TO_MIXOUTL_SHIFT                 6  
#define WM9090_IN1A_TO_MIXOUTL_WIDTH                 1  
#define WM9090_IN2A_TO_MIXOUTL                  0x0004  
#define WM9090_IN2A_TO_MIXOUTL_MASK             0x0004  
#define WM9090_IN2A_TO_MIXOUTL_SHIFT                 2  
#define WM9090_IN2A_TO_MIXOUTL_WIDTH                 1  

#define WM9090_IN1A_TO_MIXOUTR                  0x0040  
#define WM9090_IN1A_TO_MIXOUTR_MASK             0x0040  
#define WM9090_IN1A_TO_MIXOUTR_SHIFT                 6  
#define WM9090_IN1A_TO_MIXOUTR_WIDTH                 1  
#define WM9090_IN1B_TO_MIXOUTR                  0x0010  
#define WM9090_IN1B_TO_MIXOUTR_MASK             0x0010  
#define WM9090_IN1B_TO_MIXOUTR_SHIFT                 4  
#define WM9090_IN1B_TO_MIXOUTR_WIDTH                 1  
#define WM9090_IN2A_TO_MIXOUTR                  0x0004  
#define WM9090_IN2A_TO_MIXOUTR_MASK             0x0004  
#define WM9090_IN2A_TO_MIXOUTR_SHIFT                 2  
#define WM9090_IN2A_TO_MIXOUTR_WIDTH                 1  
#define WM9090_IN2B_TO_MIXOUTR                  0x0001  
#define WM9090_IN2B_TO_MIXOUTR_MASK             0x0001  
#define WM9090_IN2B_TO_MIXOUTR_SHIFT                 0  
#define WM9090_IN2B_TO_MIXOUTR_WIDTH                 1  

#define WM9090_MIXOUTL_MUTE                     0x0100  
#define WM9090_MIXOUTL_MUTE_MASK                0x0100  
#define WM9090_MIXOUTL_MUTE_SHIFT                    8  
#define WM9090_MIXOUTL_MUTE_WIDTH                    1  
#define WM9090_IN1A_MIXOUTL_VOL_MASK            0x00C0  
#define WM9090_IN1A_MIXOUTL_VOL_SHIFT                6  
#define WM9090_IN1A_MIXOUTL_VOL_WIDTH                2  
#define WM9090_IN2A_MIXOUTL_VOL_MASK            0x000C  
#define WM9090_IN2A_MIXOUTL_VOL_SHIFT                2  
#define WM9090_IN2A_MIXOUTL_VOL_WIDTH                2  

#define WM9090_MIXOUTR_MUTE                     0x0100  
#define WM9090_MIXOUTR_MUTE_MASK                0x0100  
#define WM9090_MIXOUTR_MUTE_SHIFT                    8  
#define WM9090_MIXOUTR_MUTE_WIDTH                    1  
#define WM9090_IN1A_MIXOUTR_VOL_MASK            0x00C0  
#define WM9090_IN1A_MIXOUTR_VOL_SHIFT                6  
#define WM9090_IN1A_MIXOUTR_VOL_WIDTH                2  
#define WM9090_IN1B_MIXOUTR_VOL_MASK            0x0030  
#define WM9090_IN1B_MIXOUTR_VOL_SHIFT                4  
#define WM9090_IN1B_MIXOUTR_VOL_WIDTH                2  
#define WM9090_IN2A_MIXOUTR_VOL_MASK            0x000C  
#define WM9090_IN2A_MIXOUTR_VOL_SHIFT                2  
#define WM9090_IN2A_MIXOUTR_VOL_WIDTH                2  
#define WM9090_IN2B_MIXOUTR_VOL_MASK            0x0003  
#define WM9090_IN2B_MIXOUTR_VOL_SHIFT                0  
#define WM9090_IN2B_MIXOUTR_VOL_WIDTH                2  

#define WM9090_IN1A_TO_SPKMIX                   0x0040  
#define WM9090_IN1A_TO_SPKMIX_MASK              0x0040  
#define WM9090_IN1A_TO_SPKMIX_SHIFT                  6  
#define WM9090_IN1A_TO_SPKMIX_WIDTH                  1  
#define WM9090_IN1B_TO_SPKMIX                   0x0010  
#define WM9090_IN1B_TO_SPKMIX_MASK              0x0010  
#define WM9090_IN1B_TO_SPKMIX_SHIFT                  4  
#define WM9090_IN1B_TO_SPKMIX_WIDTH                  1  
#define WM9090_IN2A_TO_SPKMIX                   0x0004  
#define WM9090_IN2A_TO_SPKMIX_MASK              0x0004  
#define WM9090_IN2A_TO_SPKMIX_SHIFT                  2  
#define WM9090_IN2A_TO_SPKMIX_WIDTH                  1  
#define WM9090_IN2B_TO_SPKMIX                   0x0001  
#define WM9090_IN2B_TO_SPKMIX_MASK              0x0001  
#define WM9090_IN2B_TO_SPKMIX_SHIFT                  0  
#define WM9090_IN2B_TO_SPKMIX_WIDTH                  1  

#define WM9090_VMID_BUF_ENA                     0x0008  
#define WM9090_VMID_BUF_ENA_MASK                0x0008  
#define WM9090_VMID_BUF_ENA_SHIFT                    3  
#define WM9090_VMID_BUF_ENA_WIDTH                    1  
#define WM9090_VMID_ENA                         0x0001  
#define WM9090_VMID_ENA_MASK                    0x0001  
#define WM9090_VMID_ENA_SHIFT                        0  
#define WM9090_VMID_ENA_WIDTH                        1  

#define WM9090_WSEQ_ENA                         0x0100  
#define WM9090_WSEQ_ENA_MASK                    0x0100  
#define WM9090_WSEQ_ENA_SHIFT                        8  
#define WM9090_WSEQ_ENA_WIDTH                        1  
#define WM9090_WSEQ_WRITE_INDEX_MASK            0x000F  
#define WM9090_WSEQ_WRITE_INDEX_SHIFT                0  
#define WM9090_WSEQ_WRITE_INDEX_WIDTH                4  

#define WM9090_WSEQ_DATA_WIDTH_MASK             0x7000  
#define WM9090_WSEQ_DATA_WIDTH_SHIFT                12  
#define WM9090_WSEQ_DATA_WIDTH_WIDTH                 3  
#define WM9090_WSEQ_DATA_START_MASK             0x0F00  
#define WM9090_WSEQ_DATA_START_SHIFT                 8  
#define WM9090_WSEQ_DATA_START_WIDTH                 4  
#define WM9090_WSEQ_ADDR_MASK                   0x00FF  
#define WM9090_WSEQ_ADDR_SHIFT                       0  
#define WM9090_WSEQ_ADDR_WIDTH                       8  

#define WM9090_WSEQ_EOS                         0x4000  
#define WM9090_WSEQ_EOS_MASK                    0x4000  
#define WM9090_WSEQ_EOS_SHIFT                       14  
#define WM9090_WSEQ_EOS_WIDTH                        1  
#define WM9090_WSEQ_DELAY_MASK                  0x0F00  
#define WM9090_WSEQ_DELAY_SHIFT                      8  
#define WM9090_WSEQ_DELAY_WIDTH                      4  
#define WM9090_WSEQ_DATA_MASK                   0x00FF  
#define WM9090_WSEQ_DATA_SHIFT                       0  
#define WM9090_WSEQ_DATA_WIDTH                       8  

#define WM9090_WSEQ_ABORT                       0x0200  
#define WM9090_WSEQ_ABORT_MASK                  0x0200  
#define WM9090_WSEQ_ABORT_SHIFT                      9  
#define WM9090_WSEQ_ABORT_WIDTH                      1  
#define WM9090_WSEQ_START                       0x0100  
#define WM9090_WSEQ_START_MASK                  0x0100  
#define WM9090_WSEQ_START_SHIFT                      8  
#define WM9090_WSEQ_START_WIDTH                      1  
#define WM9090_WSEQ_START_INDEX_MASK            0x003F  
#define WM9090_WSEQ_START_INDEX_SHIFT                0  
#define WM9090_WSEQ_START_INDEX_WIDTH                6  

#define WM9090_WSEQ_BUSY                        0x0001  
#define WM9090_WSEQ_BUSY_MASK                   0x0001  
#define WM9090_WSEQ_BUSY_SHIFT                       0  
#define WM9090_WSEQ_BUSY_WIDTH                       1  

#define WM9090_WSEQ_CURRENT_INDEX_MASK          0x003F  
#define WM9090_WSEQ_CURRENT_INDEX_SHIFT              0  
#define WM9090_WSEQ_CURRENT_INDEX_WIDTH              6  

#define WM9090_CP_ENA                           0x8000  
#define WM9090_CP_ENA_MASK                      0x8000  
#define WM9090_CP_ENA_SHIFT                         15  
#define WM9090_CP_ENA_WIDTH                          1  

#define WM9090_DCS_TRIG_SINGLE_1                0x2000  
#define WM9090_DCS_TRIG_SINGLE_1_MASK           0x2000  
#define WM9090_DCS_TRIG_SINGLE_1_SHIFT              13  
#define WM9090_DCS_TRIG_SINGLE_1_WIDTH               1  
#define WM9090_DCS_TRIG_SINGLE_0                0x1000  
#define WM9090_DCS_TRIG_SINGLE_0_MASK           0x1000  
#define WM9090_DCS_TRIG_SINGLE_0_SHIFT              12  
#define WM9090_DCS_TRIG_SINGLE_0_WIDTH               1  
#define WM9090_DCS_TRIG_SERIES_1                0x0200  
#define WM9090_DCS_TRIG_SERIES_1_MASK           0x0200  
#define WM9090_DCS_TRIG_SERIES_1_SHIFT               9  
#define WM9090_DCS_TRIG_SERIES_1_WIDTH               1  
#define WM9090_DCS_TRIG_SERIES_0                0x0100  
#define WM9090_DCS_TRIG_SERIES_0_MASK           0x0100  
#define WM9090_DCS_TRIG_SERIES_0_SHIFT               8  
#define WM9090_DCS_TRIG_SERIES_0_WIDTH               1  
#define WM9090_DCS_TRIG_STARTUP_1               0x0020  
#define WM9090_DCS_TRIG_STARTUP_1_MASK          0x0020  
#define WM9090_DCS_TRIG_STARTUP_1_SHIFT              5  
#define WM9090_DCS_TRIG_STARTUP_1_WIDTH              1  
#define WM9090_DCS_TRIG_STARTUP_0               0x0010  
#define WM9090_DCS_TRIG_STARTUP_0_MASK          0x0010  
#define WM9090_DCS_TRIG_STARTUP_0_SHIFT              4  
#define WM9090_DCS_TRIG_STARTUP_0_WIDTH              1  
#define WM9090_DCS_TRIG_DAC_WR_1                0x0008  
#define WM9090_DCS_TRIG_DAC_WR_1_MASK           0x0008  
#define WM9090_DCS_TRIG_DAC_WR_1_SHIFT               3  
#define WM9090_DCS_TRIG_DAC_WR_1_WIDTH               1  
#define WM9090_DCS_TRIG_DAC_WR_0                0x0004  
#define WM9090_DCS_TRIG_DAC_WR_0_MASK           0x0004  
#define WM9090_DCS_TRIG_DAC_WR_0_SHIFT               2  
#define WM9090_DCS_TRIG_DAC_WR_0_WIDTH               1  
#define WM9090_DCS_ENA_CHAN_1                   0x0002  
#define WM9090_DCS_ENA_CHAN_1_MASK              0x0002  
#define WM9090_DCS_ENA_CHAN_1_SHIFT                  1  
#define WM9090_DCS_ENA_CHAN_1_WIDTH                  1  
#define WM9090_DCS_ENA_CHAN_0                   0x0001  
#define WM9090_DCS_ENA_CHAN_0_MASK              0x0001  
#define WM9090_DCS_ENA_CHAN_0_SHIFT                  0  
#define WM9090_DCS_ENA_CHAN_0_WIDTH                  1  

#define WM9090_DCS_SERIES_NO_01_MASK            0x0FE0  
#define WM9090_DCS_SERIES_NO_01_SHIFT                5  
#define WM9090_DCS_SERIES_NO_01_WIDTH                7  
#define WM9090_DCS_TIMER_PERIOD_01_MASK         0x000F  
#define WM9090_DCS_TIMER_PERIOD_01_SHIFT             0  
#define WM9090_DCS_TIMER_PERIOD_01_WIDTH             4  

#define WM9090_DCS_DAC_WR_VAL_1_MASK            0xFF00  
#define WM9090_DCS_DAC_WR_VAL_1_SHIFT                8  
#define WM9090_DCS_DAC_WR_VAL_1_WIDTH                8  
#define WM9090_DCS_DAC_WR_VAL_0_MASK            0x00FF  
#define WM9090_DCS_DAC_WR_VAL_0_SHIFT                0  
#define WM9090_DCS_DAC_WR_VAL_0_WIDTH                8  

#define WM9090_DCS_CAL_COMPLETE_MASK            0x0300  
#define WM9090_DCS_CAL_COMPLETE_SHIFT                8  
#define WM9090_DCS_CAL_COMPLETE_WIDTH                2  
#define WM9090_DCS_DAC_WR_COMPLETE_MASK         0x0030  
#define WM9090_DCS_DAC_WR_COMPLETE_SHIFT             4  
#define WM9090_DCS_DAC_WR_COMPLETE_WIDTH             2  
#define WM9090_DCS_STARTUP_COMPLETE_MASK        0x0003  
#define WM9090_DCS_STARTUP_COMPLETE_SHIFT            0  
#define WM9090_DCS_STARTUP_COMPLETE_WIDTH            2  

#define WM9090_DCS_DAC_WR_VAL_1_RD_MASK         0x00FF  
#define WM9090_DCS_DAC_WR_VAL_1_RD_SHIFT             0  
#define WM9090_DCS_DAC_WR_VAL_1_RD_WIDTH             8  

#define WM9090_DCS_DAC_WR_VAL_0_RD_MASK         0x00FF  
#define WM9090_DCS_DAC_WR_VAL_0_RD_SHIFT             0  
#define WM9090_DCS_DAC_WR_VAL_0_RD_WIDTH             8  

#define WM9090_HPOUT1L_RMV_SHORT                0x0080  
#define WM9090_HPOUT1L_RMV_SHORT_MASK           0x0080  
#define WM9090_HPOUT1L_RMV_SHORT_SHIFT               7  
#define WM9090_HPOUT1L_RMV_SHORT_WIDTH               1  
#define WM9090_HPOUT1L_OUTP                     0x0040  
#define WM9090_HPOUT1L_OUTP_MASK                0x0040  
#define WM9090_HPOUT1L_OUTP_SHIFT                    6  
#define WM9090_HPOUT1L_OUTP_WIDTH                    1  
#define WM9090_HPOUT1L_DLY                      0x0020  
#define WM9090_HPOUT1L_DLY_MASK                 0x0020  
#define WM9090_HPOUT1L_DLY_SHIFT                     5  
#define WM9090_HPOUT1L_DLY_WIDTH                     1  
#define WM9090_HPOUT1R_RMV_SHORT                0x0008  
#define WM9090_HPOUT1R_RMV_SHORT_MASK           0x0008  
#define WM9090_HPOUT1R_RMV_SHORT_SHIFT               3  
#define WM9090_HPOUT1R_RMV_SHORT_WIDTH               1  
#define WM9090_HPOUT1R_OUTP                     0x0004  
#define WM9090_HPOUT1R_OUTP_MASK                0x0004  
#define WM9090_HPOUT1R_OUTP_SHIFT                    2  
#define WM9090_HPOUT1R_OUTP_WIDTH                    1  
#define WM9090_HPOUT1R_DLY                      0x0002  
#define WM9090_HPOUT1R_DLY_MASK                 0x0002  
#define WM9090_HPOUT1R_DLY_SHIFT                     1  
#define WM9090_HPOUT1R_DLY_WIDTH                     1  

#define WM9090_AGC_CLIP_ENA                     0x8000  
#define WM9090_AGC_CLIP_ENA_MASK                0x8000  
#define WM9090_AGC_CLIP_ENA_SHIFT                   15  
#define WM9090_AGC_CLIP_ENA_WIDTH                    1  
#define WM9090_AGC_CLIP_THR_MASK                0x0F00  
#define WM9090_AGC_CLIP_THR_SHIFT                    8  
#define WM9090_AGC_CLIP_THR_WIDTH                    4  
#define WM9090_AGC_CLIP_ATK_MASK                0x0070  
#define WM9090_AGC_CLIP_ATK_SHIFT                    4  
#define WM9090_AGC_CLIP_ATK_WIDTH                    3  
#define WM9090_AGC_CLIP_DCY_MASK                0x0007  
#define WM9090_AGC_CLIP_DCY_SHIFT                    0  
#define WM9090_AGC_CLIP_DCY_WIDTH                    3  

#define WM9090_AGC_PWR_ENA                      0x8000  
#define WM9090_AGC_PWR_ENA_MASK                 0x8000  
#define WM9090_AGC_PWR_ENA_SHIFT                    15  
#define WM9090_AGC_PWR_ENA_WIDTH                     1  
#define WM9090_AGC_PWR_AVG                      0x1000  
#define WM9090_AGC_PWR_AVG_MASK                 0x1000  
#define WM9090_AGC_PWR_AVG_SHIFT                    12  
#define WM9090_AGC_PWR_AVG_WIDTH                     1  
#define WM9090_AGC_PWR_THR_MASK                 0x0F00  
#define WM9090_AGC_PWR_THR_SHIFT                     8  
#define WM9090_AGC_PWR_THR_WIDTH                     4  
#define WM9090_AGC_PWR_ATK_MASK                 0x0070  
#define WM9090_AGC_PWR_ATK_SHIFT                     4  
#define WM9090_AGC_PWR_ATK_WIDTH                     3  
#define WM9090_AGC_PWR_DCY_MASK                 0x0007  
#define WM9090_AGC_PWR_DCY_SHIFT                     0  
#define WM9090_AGC_PWR_DCY_WIDTH                     3  

#define WM9090_AGC_RAMP                         0x0100  
#define WM9090_AGC_RAMP_MASK                    0x0100  
#define WM9090_AGC_RAMP_SHIFT                        8  
#define WM9090_AGC_RAMP_WIDTH                        1  
#define WM9090_AGC_MINGAIN_MASK                 0x003F  
#define WM9090_AGC_MINGAIN_SHIFT                     0  
#define WM9090_AGC_MINGAIN_WIDTH                     6  

#endif
