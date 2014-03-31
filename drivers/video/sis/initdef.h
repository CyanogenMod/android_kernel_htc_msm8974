/*
 * Global definitions for init.c and init301.c
 *
 * Copyright (C) 2001-2005 by Thomas Winischhofer, Vienna, Austria
 *
 * If distributed as part of the Linux kernel, the following license terms
 * apply:
 *
 * * This program is free software; you can redistribute it and/or modify
 * * it under the terms of the GNU General Public License as published by
 * * the Free Software Foundation; either version 2 of the named License,
 * * or any later version.
 * *
 * * This program is distributed in the hope that it will be useful,
 * * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * * GNU General Public License for more details.
 * *
 * * You should have received a copy of the GNU General Public License
 * * along with this program; if not, write to the Free Software
 * * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Otherwise, the following license terms apply:
 *
 * * Redistribution and use in source and binary forms, with or without
 * * modification, are permitted provided that the following conditions
 * * are met:
 * * 1) Redistributions of source code must retain the above copyright
 * *    notice, this list of conditions and the following disclaimer.
 * * 2) Redistributions in binary form must reproduce the above copyright
 * *    notice, this list of conditions and the following disclaimer in the
 * *    documentation and/or other materials provided with the distribution.
 * * 3) The name of the author may not be used to endorse or promote products
 * *    derived from this software without specific prior written permission.
 * *
 * * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: 	Thomas Winischhofer <thomas@winischhofer.net>
 *
 */

#ifndef _INITDEF_
#define _INITDEF_

#define IS_SIS330		(SiS_Pr->ChipType == SIS_330)
#define IS_SIS550		(SiS_Pr->ChipType == SIS_550)
#define IS_SIS650		(SiS_Pr->ChipType == SIS_650)  
#define IS_SIS740		(SiS_Pr->ChipType == SIS_740)
#define IS_SIS651	        (SiS_Pr->SiS_SysFlags & (SF_Is651 | SF_Is652))
#define IS_SISM650	        (SiS_Pr->SiS_SysFlags & (SF_IsM650 | SF_IsM652 | SF_IsM653))
#define IS_SIS65x               (IS_SIS651 || IS_SISM650)       
#define IS_SIS661		(SiS_Pr->ChipType == SIS_661)
#define IS_SIS741		(SiS_Pr->ChipType == SIS_741)
#define IS_SIS660		(SiS_Pr->ChipType == SIS_660)
#define IS_SIS760		(SiS_Pr->ChipType == SIS_760)
#define IS_SIS761		(SiS_Pr->ChipType == SIS_761)
#define IS_SIS661741660760	(IS_SIS661 || IS_SIS741 || IS_SIS660 || IS_SIS760 || IS_SIS761)
#define IS_SIS650740            ((SiS_Pr->ChipType >= SIS_650) && (SiS_Pr->ChipType < SIS_330))
#define IS_SIS550650740         (IS_SIS550 || IS_SIS650740)
#define IS_SIS650740660         (IS_SIS650 || IS_SIS740 || IS_SIS661741660760)
#define IS_SIS550650740660      (IS_SIS550 || IS_SIS650740660)

#define SISGETROMW(x)		(ROMAddr[(x)] | (ROMAddr[(x)+1] << 8))

#define VB_SIS301		0x0001
#define VB_SIS301B		0x0002
#define VB_SIS302B		0x0004
#define VB_SIS301LV		0x0008
#define VB_SIS302LV		0x0010
#define VB_SIS302ELV		0x0020
#define VB_SIS301C		0x0040
#define VB_SIS307T		0x0080
#define VB_SIS307LV		0x0100
#define VB_UMC			0x4000
#define VB_NoLCD        	0x8000
#define VB_SIS30xB		(VB_SIS301B | VB_SIS301C | VB_SIS302B | VB_SIS307T)
#define VB_SIS30xC		(VB_SIS301C | VB_SIS307T)
#define VB_SISTMDS		(VB_SIS301 | VB_SIS301B | VB_SIS301C | VB_SIS302B | VB_SIS307T)
#define VB_SISLVDS		(VB_SIS301LV | VB_SIS302LV | VB_SIS302ELV | VB_SIS307LV)
#define VB_SIS30xBLV		(VB_SIS30xB | VB_SISLVDS)
#define VB_SIS30xCLV		(VB_SIS30xC | VB_SIS302ELV | VB_SIS307LV)
#define VB_SISVB		(VB_SIS301 | VB_SIS30xBLV)
#define VB_SISLCDA		(VB_SIS302B | VB_SIS301C  | VB_SIS307T  | VB_SISLVDS)
#define VB_SISTMDSLCDA		(VB_SIS301C | VB_SIS307T)
#define VB_SISPART4SCALER	(VB_SIS301C | VB_SIS307T | VB_SIS302ELV | VB_SIS307LV)
#define VB_SISHIVISION		(VB_SIS301 | VB_SIS301B | VB_SIS302B)
#define VB_SISYPBPR		(VB_SIS301C | VB_SIS307T  | VB_SIS301LV | VB_SIS302LV | VB_SIS302ELV | VB_SIS307LV)
#define VB_SISTAP4SCALER	(VB_SIS301C | VB_SIS307T | VB_SIS302ELV | VB_SIS307LV)
#define VB_SISPART4OVERFLOW	(VB_SIS301C | VB_SIS307T | VB_SIS302LV | VB_SIS302ELV | VB_SIS307LV)
#define VB_SISPWD		(VB_SIS301C | VB_SIS307T | VB_SISLVDS)
#define VB_SISEMI		(VB_SIS302LV | VB_SIS302ELV | VB_SIS307LV)
#define VB_SISPOWER		(VB_SIS301C | VB_SIS307T | VB_SIS302LV | VB_SIS302ELV | VB_SIS307LV)
#define VB_SISDUALLINK		(VB_SIS302LV | VB_SIS302ELV | VB_SIS307T | VB_SIS307LV)
#define VB_SISVGA2		VB_SISTMDS
#define VB_SISRAMDAC202		(VB_SIS301C | VB_SIS307T)

#define SetSimuScanMode         0x0001   
#define SwitchCRT2              0x0002
#define SetCRT2ToAVIDEO         0x0004
#define SetCRT2ToSVIDEO         0x0008
#define SetCRT2ToSCART          0x0010
#define SetCRT2ToLCD            0x0020
#define SetCRT2ToRAMDAC         0x0040
#define SetCRT2ToHiVision       0x0080   		
#define SetCRT2ToCHYPbPr       	SetCRT2ToHiVision	
#define SetNTSCTV               0x0000   
#define SetPALTV                0x0100   		
#define SetInSlaveMode          0x0200
#define SetNotSimuMode          0x0400
#define SetNotSimuTVMode        SetNotSimuMode
#define SetDispDevSwitch        0x0800
#define SetCRT2ToYPbPr525750    0x0800
#define LoadDACFlag             0x1000
#define DisableCRT2Display      0x2000
#define DriverMode              0x4000
#define HotKeySwitch            0x8000
#define SetCRT2ToLCDA           0x8000

#define SetCRT2ToTV             (SetCRT2ToYPbPr525750|SetCRT2ToHiVision|SetCRT2ToSCART|SetCRT2ToSVIDEO|SetCRT2ToAVIDEO)
#define SetCRT2ToTVNoYPbPrHiVision (SetCRT2ToSCART | SetCRT2ToSVIDEO | SetCRT2ToAVIDEO)
#define SetCRT2ToTVNoHiVision  	(SetCRT2ToYPbPr525750 | SetCRT2ToSCART | SetCRT2ToSVIDEO | SetCRT2ToAVIDEO)

#define ModeText                0x00
#define ModeCGA                 0x01
#define ModeEGA                 0x02
#define ModeVGA                 0x03
#define Mode15Bpp               0x04
#define Mode16Bpp               0x05
#define Mode24Bpp               0x06
#define Mode32Bpp               0x07

#define ModeTypeMask            0x07
#define IsTextMode              0x07

#define DACInfoFlag             0x0018
#define MemoryInfoFlag          0x01E0
#define MemorySizeShift         5

#define Charx8Dot               0x0200
#define LineCompareOff          0x0400
#define CRT2Mode                0x0800
#define HalfDCLK                0x1000
#define NoSupportSimuTV         0x2000
#define NoSupportLCDScale	0x4000 
#define DoubleScanMode          0x8000

#define SupportTV               0x0008
#define SupportTV1024           0x0800
#define SupportCHTV 		0x0800
#define Support64048060Hz       0x0800  
#define SupportHiVision         0x0010
#define SupportYPbPr750p        0x1000
#define SupportLCD              0x0020
#define SupportRAMDAC2          0x0040	
#define SupportRAMDAC2_135      0x0100  
#define SupportRAMDAC2_162      0x0200  
#define SupportRAMDAC2_202      0x0400  
#define InterlaceMode           0x0080
#define SyncPP                  0x0000
#define HaveWideTiming		0x2000	
#define SyncPN                  0x4000
#define SyncNP                  0x8000
#define SyncNN                  0xc000

#define ProgrammingCRT2         0x0001
#define LowModeTests		0x0002
#define LCDVESATiming           0x0008
#define EnableLVDSDDA           0x0010
#define SetDispDevSwitchFlag    0x0020
#define CheckWinDos             0x0040
#define SetDOSMode              0x0080

#define TVSetPAL		0x0001
#define TVSetNTSCJ		0x0002
#define TVSetPALM		0x0004
#define TVSetPALN		0x0008
#define TVSetCHOverScan		0x0010
#define TVSetYPbPr525i		0x0020 
#define TVSetYPbPr525p		0x0040 
#define TVSetYPbPr750p		0x0080 
#define TVSetHiVision		0x0100 
#define TVSetTVSimuMode		0x0200 
#define TVRPLLDIV2XO		0x0400 
#define TVSetNTSC1024		0x0800 
#define TVSet525p1024		0x1000 
#define TVAspect43		0x2000
#define TVAspect169		0x4000
#define TVAspect43LB		0x8000

#define YPbPr525p               0x0001
#define YPbPr750p               0x0002
#define YPbPr525i               0x0004
#define YPbPrHiVision           0x0008
#define YPbPrModeMask           (YPbPr750p | YPbPr525p | YPbPr525i | YPbPrHiVision)

#define SF_Is651                0x0001
#define SF_IsM650               0x0002
#define SF_Is652		0x0004
#define SF_IsM652		0x0008
#define SF_IsM653		0x0010
#define SF_IsM661		0x0020
#define SF_IsM741		0x0040
#define SF_IsM760		0x0080
#define SF_760UMA		0x4000  
#define SF_760LFB		0x8000  


#define TVOverScan              0x10
#define TVOverScanShift         4



#define LCDRGB18Bit           0x0001
#define LCDNonExpanding       0x0010
#define LCDSync               0x0020
#define LCDPass11             0x0100   
#define LCDDualLink	      0x0200

#define DontExpandLCD	      LCDNonExpanding
#define LCDNonExpandingShift       4
#define DontExpandLCDShift    LCDNonExpandingShift
#define LCDSyncBit            0x00e0
#define LCDSyncShift               6

#define EnableDualEdge 		0x01
#define SetToLCDA		0x02   
#define EnableCHScart           0x04   
#define EnableCHYPbPr           0x08   
#define EnableSiSYPbPr          0x08   
#define EnableYPbPr525i         0x00   
#define EnableYPbPr525p         0x10   
#define EnableYPbPr750p         0x20   
#define EnableYPbPr1080i        0x30   
#define EnablePALM              0x40   
#define EnablePALN              0x80   
#define EnableNTSCJ             EnablePALM  


#define EnablePALMN             0x40   

#define LCDPass1_1		0x01   
#define Enable302LV_DualLink    0x04   






#define Panel300_800x600        0x01	
#define Panel300_1024x768       0x02
#define Panel300_1280x1024      0x03
#define Panel300_1280x960       0x04
#define Panel300_640x480        0x05
#define Panel300_1024x600       0x06
#define Panel300_1152x768       0x07
#define Panel300_1280x768       0x0a
#define Panel300_Custom		0x0f
#define Panel300_Barco1366      0x10

#define Panel310_800x600        0x01
#define Panel310_1024x768       0x02
#define Panel310_1280x1024      0x03
#define Panel310_640x480        0x04
#define Panel310_1024x600       0x05
#define Panel310_1152x864       0x06
#define Panel310_1280x960       0x07
#define Panel310_1152x768       0x08	
#define Panel310_1400x1050      0x09
#define Panel310_1280x768       0x0a
#define Panel310_1600x1200      0x0b
#define Panel310_320x240_2      0x0c    
#define Panel310_320x240_3      0x0d    
#define Panel310_320x240_1      0x0e    
#define Panel310_Custom		0x0f

#define Panel661_800x600        0x01
#define Panel661_1024x768       0x02
#define Panel661_1280x1024      0x03
#define Panel661_640x480        0x04
#define Panel661_1024x600       0x05
#define Panel661_1152x864       0x06
#define Panel661_1280x960       0x07
#define Panel661_1280x854       0x08
#define Panel661_1400x1050      0x09
#define Panel661_1280x768       0x0a
#define Panel661_1600x1200      0x0b
#define Panel661_1280x800       0x0c
#define Panel661_1680x1050      0x0d
#define Panel661_1280x720       0x0e
#define Panel661_Custom		0x0f

#define Panel_800x600           0x01	
#define Panel_1024x768          0x02    
#define Panel_1280x1024         0x03
#define Panel_640x480           0x04
#define Panel_1024x600          0x05
#define Panel_1152x864          0x06
#define Panel_1280x960          0x07
#define Panel_1152x768          0x08	
#define Panel_1400x1050         0x09
#define Panel_1280x768          0x0a    
#define Panel_1600x1200         0x0b
#define Panel_1280x800		0x0c    
#define Panel_1680x1050         0x0d    
#define Panel_1280x720		0x0e    
#define Panel_Custom		0x0f	
#define Panel_320x240_1         0x10    
#define Panel_Barco1366         0x11
#define Panel_848x480		0x12
#define Panel_320x240_2		0x13    
#define Panel_320x240_3		0x14    
#define Panel_1280x768_2        0x15	
#define Panel_1280x768_3        0x16    
#define Panel_1280x800_2	0x17    
#define Panel_856x480		0x18
#define Panel_1280x854		0x19	

#define SIS_RI_320x200    0
#define SIS_RI_320x240    1
#define SIS_RI_320x400    2
#define SIS_RI_400x300    3
#define SIS_RI_512x384    4
#define SIS_RI_640x400    5
#define SIS_RI_640x480    6
#define SIS_RI_800x600    7
#define SIS_RI_1024x768   8
#define SIS_RI_1280x1024  9
#define SIS_RI_1600x1200 10
#define SIS_RI_1920x1440 11
#define SIS_RI_2048x1536 12
#define SIS_RI_720x480   13
#define SIS_RI_720x576   14
#define SIS_RI_1280x960  15
#define SIS_RI_800x480   16
#define SIS_RI_1024x576  17
#define SIS_RI_1280x720  18
#define SIS_RI_856x480   19
#define SIS_RI_1280x768  20
#define SIS_RI_1400x1050 21
#define SIS_RI_1152x864  22  
#define SIS_RI_848x480   23
#define SIS_RI_1360x768  24
#define SIS_RI_1024x600  25
#define SIS_RI_1152x768  26
#define SIS_RI_768x576   27
#define SIS_RI_1360x1024 28
#define SIS_RI_1680x1050 29
#define SIS_RI_1280x800  30
#define SIS_RI_1920x1080 31
#define SIS_RI_960x540   32
#define SIS_RI_960x600   33
#define SIS_RI_1280x854  34

#define IsM650                  0x80

#define NTSCHT                  1716
#define NTSC2HT                 1920
#define NTSCVT                  525
#define PALHT                   1728
#define PALVT                   625
#define StHiTVHT                892
#define StHiTVVT                1126
#define StHiTextTVHT            1000
#define StHiTextTVVT            1126
#define ExtHiTVHT               2100
#define ExtHiTVVT               1125


#define VCLK28                  0x00   
#define VCLK40                  0x04   
#define VCLK65_300              0x09   
#define VCLK108_2_300           0x14   
#define VCLK81_300		0x3f   
#define VCLK108_3_300           0x42   
#define VCLK100_300             0x43   
#define VCLK34_300              0x3d   
#define VCLK_CUSTOM_300		0x47

#define VCLK65_315              0x0b   
#define VCLK108_2_315           0x19
#define VCLK81_315		0x5b
#define VCLK162_315             0x5e
#define VCLK108_3_315           0x45
#define VCLK100_315             0x46
#define VCLK34_315              0x55
#define VCLK68_315		0x0d
#define VCLK_1280x800_315_2	0x5c
#define VCLK121_315		0x5d
#define VCLK130_315		0x72
#define VCLK_1280x720		0x5f
#define VCLK_1280x768_2		0x60
#define VCLK_1280x768_3		0x61   
#define VCLK_CUSTOM_315		0x62
#define VCLK_1280x720_2		0x63
#define VCLK_720x480		0x67
#define VCLK_720x576		0x68
#define VCLK_768x576		0x68
#define VCLK_848x480		0x65
#define VCLK_856x480		0x66
#define VCLK_800x480		0x65
#define VCLK_1024x576		0x51
#define VCLK_1152x864		0x64
#define VCLK_1360x768		0x58
#define VCLK_1280x800_315	0x6c
#define VCLK_1280x854		0x76

#define TVCLKBASE_300		0x21   
#define TVCLKBASE_315	        0x3a   
#define TVVCLKDIV2              0x00   
#define TVVCLK                  0x01   
#define HiTVVCLKDIV2            0x02   
#define HiTVVCLK                0x03   
#define HiTVSimuVCLK            0x04   
#define HiTVTextVCLK            0x05   
#define YPbPr750pVCLK		0x25   


#define SetSCARTOutput          0x01

#define HotPlugFunction         0x08

#define StStructSize            0x06

#define SIS_VIDEO_CAPTURE       0x00 - 0x30
#define SIS_VIDEO_PLAYBACK      0x02 - 0x30
#define SIS_CRT2_PORT_04        0x04 - 0x30
#define SIS_CRT2_PORT_10        0x10 - 0x30
#define SIS_CRT2_PORT_12        0x12 - 0x30
#define SIS_CRT2_PORT_14        0x14 - 0x30

#define ADR_CRT2PtrData         0x20E
#define offset_Zurac            0x210   
#define ADR_LVDSDesPtrData      0x212
#define ADR_LVDSCRT1DataPtr     0x214
#define ADR_CHTVVCLKPtr         0x216
#define ADR_CHTVRegDataPtr      0x218

#define LCDDataLen              8
#define HiTVDataLen             12
#define TVDataLen               16

#define LVDSDataLen             6
#define LVDSDesDataLen          3
#define ActiveNonExpanding      0x40
#define ActiveNonExpandingShift 6
#define ActivePAL               0x20
#define ActivePALShift          5
#define ModeSwitchStatus        0x0F
#define SoftTVType              0x40
#define SoftSettingAddr         0x52
#define ModeSettingAddr         0x53

#define _PanelType00             0x00
#define _PanelType01             0x08
#define _PanelType02             0x10
#define _PanelType03             0x18
#define _PanelType04             0x20
#define _PanelType05             0x28
#define _PanelType06             0x30
#define _PanelType07             0x38
#define _PanelType08             0x40
#define _PanelType09             0x48
#define _PanelType0A             0x50
#define _PanelType0B             0x58
#define _PanelType0C             0x60
#define _PanelType0D             0x68
#define _PanelType0E             0x70
#define _PanelType0F             0x78

#define PRIMARY_VGA       	0     

#define BIOSIDCodeAddr          0x235  
#define OEMUtilIDCodeAddr       0x237
#define VBModeIDTableAddr       0x239
#define OEMTVPtrAddr            0x241
#define PhaseTableAddr          0x243
#define NTSCFilterTableAddr     0x245
#define PALFilterTableAddr      0x247
#define OEMLCDPtr_1Addr         0x249
#define OEMLCDPtr_2Addr         0x24B
#define LCDHPosTable_1Addr      0x24D
#define LCDHPosTable_2Addr      0x24F
#define LCDVPosTable_1Addr      0x251
#define LCDVPosTable_2Addr      0x253
#define OEMLCDPIDTableAddr      0x255

#define VBModeStructSize        5
#define PhaseTableSize          4
#define FilterTableSize         4
#define LCDHPosTableSize        7
#define LCDVPosTableSize        5
#define OEMLVDSPIDTableSize     4
#define LVDSHPosTableSize       4
#define LVDSVPosTableSize       6

#define VB_ModeID               0
#define VB_TVTableIndex         1
#define VB_LCDTableIndex        2
#define VB_LCDHIndex            3
#define VB_LCDVIndex            4

#define OEMLCDEnable            0x0001
#define OEMLCDDelayEnable       0x0002
#define OEMLCDPOSEnable         0x0004
#define OEMTVEnable             0x0100
#define OEMTVDelayEnable        0x0200
#define OEMTVFlickerEnable      0x0400
#define OEMTVPhaseEnable        0x0800
#define OEMTVFilterEnable       0x1000

#define OEMLCDPanelIDSupport    0x0080

#define SoftDRAMType        0x80
#define SoftSetting_OFFSET  0x52
#define SR07_OFFSET  0x7C
#define SR15_OFFSET  0x7D
#define SR16_OFFSET  0x81
#define SR17_OFFSET  0x85
#define SR19_OFFSET  0x8D
#define SR1F_OFFSET  0x99
#define SR21_OFFSET  0x9A
#define SR22_OFFSET  0x9B
#define SR23_OFFSET  0x9C
#define SR24_OFFSET  0x9D
#define SR25_OFFSET  0x9E
#define SR31_OFFSET  0x9F
#define SR32_OFFSET  0xA0
#define SR33_OFFSET  0xA1

#define CR40_OFFSET  0xA2
#define SR25_1_OFFSET  0xF6
#define CR49_OFFSET  0xF7

#define VB310Data_1_2_Offset  0xB6
#define VB310Data_4_D_Offset  0xB7
#define VB310Data_4_E_Offset  0xB8
#define VB310Data_4_10_Offset 0xBB

#define RGBSenseDataOffset    0xBD
#define YCSenseDataOffset     0xBF
#define VideoSenseDataOffset  0xC1
#define OutputSelectOffset    0xF3

#define ECLK_MCLK_DISTANCE  0x14
#define VBIOSTablePointerStart    0x100
#define StandTablePtrOffset       VBIOSTablePointerStart+0x02
#define EModeIDTablePtrOffset     VBIOSTablePointerStart+0x04
#define CRT1TablePtrOffset        VBIOSTablePointerStart+0x06
#define ScreenOffsetPtrOffset     VBIOSTablePointerStart+0x08
#define VCLKDataPtrOffset         VBIOSTablePointerStart+0x0A
#define MCLKDataPtrOffset         VBIOSTablePointerStart+0x0E
#define CRT2PtrDataPtrOffset      VBIOSTablePointerStart+0x10
#define TVAntiFlickPtrOffset      VBIOSTablePointerStart+0x12
#define TVDelayPtr1Offset         VBIOSTablePointerStart+0x14
#define TVPhaseIncrPtr1Offset     VBIOSTablePointerStart+0x16
#define TVYFilterPtr1Offset       VBIOSTablePointerStart+0x18
#define LCDDelayPtr1Offset        VBIOSTablePointerStart+0x20
#define TVEdgePtr1Offset          VBIOSTablePointerStart+0x24
#define CRT2Delay1Offset          VBIOSTablePointerStart+0x28

#endif
