/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	r8185b_init.c

Abstract:
	Hardware Initialization and Hardware IO for RTL8185B

Major Change History:
	When		Who				What
	----------	---------------		-------------------------------
	2006-11-15    Xiong		Created

Notes:
	This file is ported from RTL8185B Windows driver.


--*/

#include <linux/spinlock.h>
#include "r8180_hw.h"
#include "r8180.h"
#include "r8180_rtl8225.h" 
#include "r8180_93cx6.h"   
#include "r8180_wx.h"

#include "ieee80211/dot11d.h"



#define TC_3W_POLL_MAX_TRY_CNT 5
static u8 MAC_REG_TABLE[][2] =	{
                        
						
						
						
						{0x08, 0xae}, {0x0a, 0x72}, {0x5b, 0x42},
						{0x84, 0x88}, {0x85, 0x24}, {0x88, 0x54}, {0x8b, 0xb8}, {0x8c, 0x03},
						{0x8d, 0x40}, {0x8e, 0x00}, {0x8f, 0x00}, {0x5b, 0x18}, {0x91, 0x03},
						{0x94, 0x0F}, {0x95, 0x32},
						{0x96, 0x00}, {0x97, 0x07}, {0xb4, 0x22}, {0xdb, 0x00},
						{0xf0, 0x32}, {0xf1, 0x32}, {0xf2, 0x00}, {0xf3, 0x00}, {0xf4, 0x32},
						{0xf5, 0x43}, {0xf6, 0x00}, {0xf7, 0x00}, {0xf8, 0x46}, {0xf9, 0xa4},
						{0xfa, 0x00}, {0xfb, 0x00}, {0xfc, 0x96}, {0xfd, 0xa4}, {0xfe, 0x00},
						{0xff, 0x00},

						
						
				
						{0x5e, 0x01},
						{0x58, 0x00}, {0x59, 0x00}, {0x5a, 0x04}, {0x5b, 0x00}, {0x60, 0x24},
						{0x61, 0x97}, {0x62, 0xF0}, {0x63, 0x09}, {0x80, 0x0F}, {0x81, 0xFF},
						{0x82, 0xFF}, {0x83, 0x03},
						{0xC4, 0x22}, {0xC5, 0x22}, {0xC6, 0x22}, {0xC7, 0x22}, {0xC8, 0x22}, 
						{0xC9, 0x22}, {0xCA, 0x22}, {0xCB, 0x22}, {0xCC, 0x22}, {0xCD, 0x22},
						{0xe2, 0x00},


						
						{0x5e, 0x02},
						{0x0c, 0x04}, {0x4c, 0x30}, {0x4d, 0x08}, {0x50, 0x05}, {0x51, 0xf5},
						{0x52, 0x04}, {0x53, 0xa0}, {0x54, 0xff}, {0x55, 0xff}, {0x56, 0xff},
						{0x57, 0xff}, {0x58, 0x08}, {0x59, 0x08}, {0x5a, 0x08}, {0x5b, 0x08},
						{0x60, 0x08}, {0x61, 0x08}, {0x62, 0x08}, {0x63, 0x08}, {0x64, 0x2f},
						{0x8c, 0x3f}, {0x8d, 0x3f}, {0x8e, 0x3f},
						{0x8f, 0x3f}, {0xc4, 0xff}, {0xc5, 0xff}, {0xc6, 0xff}, {0xc7, 0xff},
						{0xc8, 0x00}, {0xc9, 0x00}, {0xca, 0x80}, {0xcb, 0x00},

						
						{0x5e, 0x00}, {0x9f, 0x03}
			};


static u8  ZEBRA_AGC[]	=	{
			0,
			0x7E, 0x7E, 0x7E, 0x7E, 0x7D, 0x7C, 0x7B, 0x7A, 0x79, 0x78, 0x77, 0x76, 0x75, 0x74, 0x73, 0x72,
			0x71, 0x70, 0x6F, 0x6E, 0x6D, 0x6C, 0x6B, 0x6A, 0x69, 0x68, 0x67, 0x66, 0x65, 0x64, 0x63, 0x62,
			0x48, 0x47, 0x46, 0x45, 0x44, 0x29, 0x28, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21, 0x08, 0x07,
			0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x15, 0x16,
			0x17, 0x17, 0x18, 0x18, 0x19, 0x1a, 0x1a, 0x1b, 0x1b, 0x1c, 0x1c, 0x1d, 0x1d, 0x1d, 0x1e, 0x1e,
			0x1f, 0x1f, 0x1f, 0x20, 0x20, 0x20, 0x20, 0x21, 0x21, 0x21, 0x22, 0x22, 0x22, 0x23, 0x23, 0x24,
			0x24, 0x25, 0x25, 0x25, 0x26, 0x26, 0x27, 0x27, 0x2F, 0x2F, 0x2F, 0x2F, 0x2F, 0x2F, 0x2F, 0x2F
			};

static u32 ZEBRA_RF_RX_GAIN_TABLE[]	=	{
			0x0096, 0x0076, 0x0056, 0x0036, 0x0016, 0x01f6, 0x01d6, 0x01b6,
			0x0196, 0x0176, 0x00F7, 0x00D7, 0x00B7, 0x0097, 0x0077, 0x0057,
			0x0037, 0x00FB, 0x00DB, 0x00BB, 0x00FF, 0x00E3, 0x00C3, 0x00A3,
			0x0083, 0x0063, 0x0043, 0x0023, 0x0003, 0x01E3, 0x01C3, 0x01A3,
			0x0183, 0x0163, 0x0143, 0x0123, 0x0103
	};

static u8 OFDM_CONFIG[]	=	{
			
			
			

			
			0x10, 0x0F, 0x0A, 0x0C, 0x14, 0xFA, 0xFF, 0x50,
			0x00, 0x50, 0x00, 0x00, 0x00, 0x5C, 0x00, 0x00,
			
			0x40, 0x00, 0x40, 0x00, 0x00, 0x00, 0xA8, 0x26,
			0x32, 0x33, 0x06, 0xA5, 0x6F, 0x55, 0xC8, 0xBB,
			
			0x0A, 0xE1, 0x2C, 0x4A, 0x86, 0x83, 0x34, 0x00,
			0x4F, 0x24, 0x6F, 0xC2, 0x03, 0x40, 0x80, 0x00,
			
			0xC0, 0xC1, 0x58, 0xF1, 0x00, 0xC4, 0x90, 0x3e,
			0xD8, 0x3C, 0x7B, 0x10, 0x10
		};


void
PlatformIOWrite1Byte(
	struct net_device *dev,
	u32		offset,
	u8		data
	)
{
	write_nic_byte(dev, offset, data);
	read_nic_byte(dev, offset); 

}

void
PlatformIOWrite2Byte(
	struct net_device *dev,
	u32		offset,
	u16		data
	)
{
	write_nic_word(dev, offset, data);
	read_nic_word(dev, offset); 


}
u8 PlatformIORead1Byte(struct net_device *dev, u32 offset);

void
PlatformIOWrite4Byte(
	struct net_device *dev,
	u32		offset,
	u32		data
	)
{
if (offset == PhyAddr)	{
		unsigned char	cmdByte;
		unsigned long	dataBytes;
		unsigned char	idx;
		u8	u1bTmp;

		cmdByte = (u8)(data & 0x000000ff);
		dataBytes = data>>8;


		for (idx = 0; idx < 30; idx++)	{ 
		
			u1bTmp = PlatformIORead1Byte(dev, PhyAddr);
			if ((u1bTmp & BIT7) == 0)
				break;
			else
				mdelay(10);
		}

		for (idx = 0; idx < 3; idx++)
			PlatformIOWrite1Byte(dev, offset+1+idx, ((u8 *)&dataBytes)[idx]);

		write_nic_byte(dev, offset, cmdByte);

	}
	else	{
		write_nic_dword(dev, offset, data);
		read_nic_dword(dev, offset); 
	}
}

u8
PlatformIORead1Byte(
	struct net_device *dev,
	u32		offset
	)
{
	u8	data = 0;

	data = read_nic_byte(dev, offset);


	return data;
}

u16
PlatformIORead2Byte(
	struct net_device *dev,
	u32		offset
	)
{
	u16	data = 0;

	data = read_nic_word(dev, offset);


	return data;
}

u32
PlatformIORead4Byte(
	struct net_device *dev,
	u32		offset
	)
{
	u32	data = 0;

	data = read_nic_dword(dev, offset);


	return data;
}

void SetOutputEnableOfRfPins(struct net_device *dev)
{
	write_nic_word(dev, RFPinsEnable, 0x1bff);
}

static int
HwHSSIThreeWire(
	struct net_device *dev,
	u8			*pDataBuf,
	u8			nDataBufBitCnt,
	int			bSI,
	int			bWrite
	)
{
	int	bResult = 1;
	u8	TryCnt;
	u8	u1bTmp;

	do	{
		
		for (TryCnt = 0; TryCnt < TC_3W_POLL_MAX_TRY_CNT; TryCnt++)	{
			u1bTmp = read_nic_byte(dev, SW_3W_CMD1);
			if ((u1bTmp & (SW_3W_CMD1_RE|SW_3W_CMD1_WE)) == 0)
				break;

			udelay(10);
		}
		if (TryCnt == TC_3W_POLL_MAX_TRY_CNT) {
			printk(KERN_ERR "rtl8187se: HwThreeWire(): CmdReg:"
			       " %#X RE|WE bits are not clear!!\n", u1bTmp);
			dump_stack();
			return 0;
		}

		
		u1bTmp = read_nic_byte(dev, RF_SW_CONFIG);

		if (bSI)
			u1bTmp |=   RF_SW_CFG_SI;   

		else
			u1bTmp &= ~RF_SW_CFG_SI;  


		write_nic_byte(dev, RF_SW_CONFIG, u1bTmp);

		if (bSI)	{
			
			u1bTmp = read_nic_byte(dev, RFPinsSelect);
			u1bTmp &= ~BIT3;
			write_nic_byte(dev, RFPinsSelect, u1bTmp);
		}
		

		if (bWrite)	{
			if (nDataBufBitCnt == 16)	{
				write_nic_word(dev, SW_3W_DB0, *((u16 *)pDataBuf));
			}	else if (nDataBufBitCnt == 64)	{
			
				write_nic_dword(dev, SW_3W_DB0, *((u32 *)pDataBuf));
				write_nic_dword(dev, SW_3W_DB1, *((u32 *)(pDataBuf + 4)));
			}	else	{
				int idx;
				int ByteCnt = nDataBufBitCnt / 8;
								
				if ((nDataBufBitCnt % 8) != 0) {
					printk(KERN_ERR "rtl8187se: "
					       "HwThreeWire(): nDataBufBitCnt(%d)"
					       " should be multiple of 8!!!\n",
					       nDataBufBitCnt);
					dump_stack();
					nDataBufBitCnt += 8;
					nDataBufBitCnt &= ~7;
				}

			       if (nDataBufBitCnt > 64) {
					printk(KERN_ERR "rtl8187se: HwThreeWire():"
					       " nDataBufBitCnt(%d) should <= 64!!!\n",
					       nDataBufBitCnt);
					dump_stack();
					nDataBufBitCnt = 64;
				}

				for (idx = 0; idx < ByteCnt; idx++)
					write_nic_byte(dev, (SW_3W_DB0+idx), *(pDataBuf+idx));

			}
		}	else	{	
			if (bSI)	{
				
				write_nic_word(dev, SW_3W_DB0, *((u16 *)pDataBuf));
			}	else	{
				
				write_nic_word(dev, SW_3W_DB0, (*((u16 *)pDataBuf)) << 12);
			}
		}

		
		if (bWrite)
			write_nic_byte(dev, SW_3W_CMD1, SW_3W_CMD1_WE);

		else
			write_nic_byte(dev, SW_3W_CMD1, SW_3W_CMD1_RE);


		
		for (TryCnt = 0; TryCnt < TC_3W_POLL_MAX_TRY_CNT; TryCnt++)	{
			u1bTmp = read_nic_byte(dev, SW_3W_CMD1);
			if ((u1bTmp & SW_3W_CMD1_DONE) != 0)
				break;

			udelay(10);
		}

		write_nic_byte(dev, SW_3W_CMD1, 0);

		
		if (bWrite == 0)	{
			if (bSI)		{
				
				*((u16 *)pDataBuf) = read_nic_word(dev, SI_DATA_READ) ;
			}	else	{
				
				*((u16 *)pDataBuf) = read_nic_word(dev, PI_DATA_READ);
			}

			*((u16 *)pDataBuf) &= 0x0FFF;
		}

	}	while (0);

	return bResult;
}

void
RF_WriteReg(struct net_device *dev, u8 offset, u32 data)
{
	u32 data2Write;
	u8 len;

	
	data2Write = (data << 4) | (u32)(offset & 0x0f);
	len = 16;

	HwHSSIThreeWire(dev, (u8 *)(&data2Write), len, 1, 1);
}

u32 RF_ReadReg(struct net_device *dev, u8 offset)
{
	u32 data2Write;
	u8 wlen;
	u32 dataRead;

	data2Write = ((u32)(offset & 0x0f));
	wlen = 16;
	HwHSSIThreeWire(dev, (u8 *)(&data2Write), wlen, 1, 0);
	dataRead = data2Write;

	return dataRead;
}


void
WriteBBPortUchar(
	struct net_device *dev,
	u32		Data
	)
{
	
	u8	RegisterContent;
	u8	UCharData;

	UCharData = (u8)((Data & 0x0000ff00) >> 8);
	PlatformIOWrite4Byte(dev, PhyAddr, Data);
	
	{
		PlatformIOWrite4Byte(dev, PhyAddr, Data & 0xffffff7f);
		RegisterContent = PlatformIORead1Byte(dev, PhyDataR);
		
		
	}
}

u8
ReadBBPortUchar(
	struct net_device *dev,
	u32		addr
	)
{
	
	u8	RegisterContent;

	PlatformIOWrite4Byte(dev, PhyAddr, addr & 0xffffff7f);
	RegisterContent = PlatformIORead1Byte(dev, PhyDataR);

	return RegisterContent;
}
bool
SetAntennaConfig87SE(
	struct net_device *dev,
	u8			DefaultAnt,		
	bool		bAntDiversity	
)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	bool   bAntennaSwitched = true;

	

	
	write_phy_cck(dev, 0x0c, 0x09); 

	if (bAntDiversity)	{	
		if (DefaultAnt == 1)	{	

			
			write_nic_byte(dev, ANTSEL, 0x00);

			
			write_phy_cck(dev, 0x11, 0xbb); 
			write_phy_cck(dev, 0x01, 0xc7); 

			
			write_phy_ofdm(dev, 0x0D, 0x54);	
			write_phy_ofdm(dev, 0x18, 0xb2);	
		}	else	{	
			
			write_nic_byte(dev, ANTSEL, 0x03);
			
			
			write_phy_cck(dev, 0x11, 0x9b); 
			write_phy_cck(dev, 0x01, 0xc7); 

			
			write_phy_ofdm(dev, 0x0d, 0x5c);  
			write_phy_ofdm(dev, 0x18, 0xb2);  
		}
	}	else	{
		
		if (DefaultAnt == 1)	{	
			
			write_nic_byte(dev, ANTSEL, 0x00);

			
			write_phy_cck(dev, 0x11, 0xbb); 
			write_phy_cck(dev, 0x01, 0x47); 

			
			write_phy_ofdm(dev, 0x0D, 0x54);	
			write_phy_ofdm(dev, 0x18, 0x32);	
		}	else	{	
			
			write_nic_byte(dev, ANTSEL, 0x03);

			
			write_phy_cck(dev, 0x11, 0x9b); 
			write_phy_cck(dev, 0x01, 0x47); 

			
			write_phy_ofdm(dev, 0x0D, 0x5c);	
			write_phy_ofdm(dev, 0x18, 0x32);	
		}
	}
	priv->CurrAntennaIndex = DefaultAnt; 
	return	bAntennaSwitched;
}

void
ZEBRA_Config_85BASIC_HardCode(
	struct net_device *dev
	)
{

	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	u32			i;
	u32	addr, data;
	u32	u4bRegOffset, u4bRegValue, u4bRF23, u4bRF24;
	u8			u1b24E;
	int d_cut = 0;




	
	RF_WriteReg(dev, 0x00, 0x013f);			mdelay(1); 
	u4bRF23 = RF_ReadReg(dev, 0x08);			mdelay(1);
	u4bRF24 = RF_ReadReg(dev, 0x09);			mdelay(1);

	if (u4bRF23 == 0x818 && u4bRF24 == 0x70C) {
		d_cut = 1;
		printk(KERN_INFO "rtl8187se: card type changed from C- to D-cut\n");
	}

	

	RF_WriteReg(dev, 0x00, 0x009f);			mdelay(1);

	RF_WriteReg(dev, 0x01, 0x06e0);			mdelay(1);

	RF_WriteReg(dev, 0x02, 0x004d);			mdelay(1);

	RF_WriteReg(dev, 0x03, 0x07f1);			mdelay(1);

	RF_WriteReg(dev, 0x04, 0x0975);			mdelay(1);
	RF_WriteReg(dev, 0x05, 0x0c72);			mdelay(1);
	RF_WriteReg(dev, 0x06, 0x0ae6);			mdelay(1);
	RF_WriteReg(dev, 0x07, 0x00ca);			mdelay(1);
	RF_WriteReg(dev, 0x08, 0x0e1c);			mdelay(1);
	RF_WriteReg(dev, 0x09, 0x02f0);			mdelay(1);
	RF_WriteReg(dev, 0x0a, 0x09d0);			mdelay(1);
	RF_WriteReg(dev, 0x0b, 0x01ba);			mdelay(1);
	RF_WriteReg(dev, 0x0c, 0x0640);			mdelay(1);
	RF_WriteReg(dev, 0x0d, 0x08df);			mdelay(1);
	RF_WriteReg(dev, 0x0e, 0x0020);			mdelay(1);
	RF_WriteReg(dev, 0x0f, 0x0990);			mdelay(1);


	
	RF_WriteReg(dev, 0x00, 0x013f);			mdelay(1);

	RF_WriteReg(dev, 0x03, 0x0806);			mdelay(1);

	RF_WriteReg(dev, 0x04, 0x03a7);			mdelay(1);
	RF_WriteReg(dev, 0x05, 0x059b);			mdelay(1);
	RF_WriteReg(dev, 0x06, 0x0081);			mdelay(1);


	RF_WriteReg(dev, 0x07, 0x01A0);			mdelay(1);
	RF_WriteReg(dev, 0x0a, 0x0001);			mdelay(1);
	RF_WriteReg(dev, 0x0b, 0x0418);			mdelay(1);

	if (d_cut) {
		RF_WriteReg(dev, 0x0c, 0x0fbe);			mdelay(1);
		RF_WriteReg(dev, 0x0d, 0x0008);			mdelay(1);
		RF_WriteReg(dev, 0x0e, 0x0807);			mdelay(1); 
	}	else	{
		RF_WriteReg(dev, 0x0c, 0x0fbe);			mdelay(1);
		RF_WriteReg(dev, 0x0d, 0x0008);			mdelay(1);
		RF_WriteReg(dev, 0x0e, 0x0806);			mdelay(1); 
	}

	RF_WriteReg(dev, 0x0f, 0x0acc);			mdelay(1);

	RF_WriteReg(dev, 0x00, 0x01d7);			mdelay(1); 

	RF_WriteReg(dev, 0x03, 0x0e00);			mdelay(1);
	RF_WriteReg(dev, 0x04, 0x0e50);			mdelay(1);
	for (i = 0; i <= 36; i++)	{
		RF_WriteReg(dev, 0x01, i);                     mdelay(1);
		RF_WriteReg(dev, 0x02, ZEBRA_RF_RX_GAIN_TABLE[i]); mdelay(1);
	}

	RF_WriteReg(dev, 0x05, 0x0203);			mdelay(1);	
	RF_WriteReg(dev, 0x06, 0x0200);			mdelay(1);	

	RF_WriteReg(dev, 0x00, 0x0137);			mdelay(1);	
	mdelay(10);			

	RF_WriteReg(dev, 0x0d, 0x0008);			mdelay(1);	
	mdelay(10);			

	RF_WriteReg(dev, 0x00, 0x0037);			mdelay(1);	
	mdelay(10);			

	RF_WriteReg(dev, 0x04, 0x0160);			mdelay(1);	
	mdelay(10);			

	RF_WriteReg(dev, 0x07, 0x0080);			mdelay(1);	
	mdelay(10);			

	RF_WriteReg(dev, 0x02, 0x088D);			mdelay(1);	
	mdelay(200);		
	mdelay(10);			
	mdelay(10);			

	RF_WriteReg(dev, 0x00, 0x0137);			mdelay(1);	
	mdelay(10);			

	RF_WriteReg(dev, 0x07, 0x0000);			mdelay(1);
	RF_WriteReg(dev, 0x07, 0x0180);			mdelay(1);
	RF_WriteReg(dev, 0x07, 0x0220);			mdelay(1);
	RF_WriteReg(dev, 0x07, 0x03E0);			mdelay(1);

	
	RF_WriteReg(dev, 0x06, 0x00c1);			mdelay(1);
	RF_WriteReg(dev, 0x0a, 0x0001);			mdelay(1);
	
	if (priv->bXtalCalibration)	{	
		RF_WriteReg(dev, 0x0f, (priv->XtalCal_Xin<<5) | (priv->XtalCal_Xout<<1) | BIT11 | BIT9);			mdelay(1);
		printk("ZEBRA_Config_85BASIC_HardCode(): (%02x)\n",
				(priv->XtalCal_Xin<<5) | (priv->XtalCal_Xout<<1) | BIT11 | BIT9);
	}	else	{
		
		RF_WriteReg(dev, 0x0f, 0x0acc);			mdelay(1);
	}

	RF_WriteReg(dev, 0x00, 0x00bf);			mdelay(1); 
	RF_WriteReg(dev, 0x0d, 0x08df);			mdelay(1); 
	RF_WriteReg(dev, 0x02, 0x004d);			mdelay(1); 
	RF_WriteReg(dev, 0x04, 0x0975);			mdelay(1); 
	mdelay(10);		
	mdelay(10);		
	mdelay(10);		
	RF_WriteReg(dev, 0x00, 0x0197);			mdelay(1); 	
	RF_WriteReg(dev, 0x05, 0x05ab);			mdelay(1); 	
	RF_WriteReg(dev, 0x00, 0x009f);			mdelay(1); 	

	RF_WriteReg(dev, 0x01, 0x0000);			mdelay(1); 	
	RF_WriteReg(dev, 0x02, 0x0000);			mdelay(1); 	
	
	u1b24E = read_nic_byte(dev, 0x24E);
	write_nic_byte(dev, 0x24E, (u1b24E & (~(BIT5|BIT6))));


	write_phy_cck(dev, 0x00, 0xc8);
	write_phy_cck(dev, 0x06, 0x1c);
	write_phy_cck(dev, 0x10, 0x78);
	write_phy_cck(dev, 0x2e, 0xd0);
	write_phy_cck(dev, 0x2f, 0x06);
	write_phy_cck(dev, 0x01, 0x46);

	
	write_nic_byte(dev, CCK_TXAGC, 0x10);
	write_nic_byte(dev, OFDM_TXAGC, 0x1B);
	write_nic_byte(dev, ANTSEL, 0x03);




	write_phy_ofdm(dev, 0x00, 0x12);

	for (i = 0; i < 128; i++)	{

		data = ZEBRA_AGC[i+1];
		data = data << 8;
		data = data | 0x0000008F;

		addr = i + 0x80; 
		addr = addr << 8;
		addr = addr | 0x0000008E;

		WriteBBPortUchar(dev, data);
		WriteBBPortUchar(dev, addr);
		WriteBBPortUchar(dev, 0x0000008E);
	}

	PlatformIOWrite4Byte(dev, PhyAddr, 0x00001080);	


	for (i = 0; i < 60; i++)	{
		u4bRegOffset = i;
		u4bRegValue = OFDM_CONFIG[i];

		WriteBBPortUchar(dev,
						(0x00000080 |
						(u4bRegOffset & 0x7f) |
						((u4bRegValue & 0xff) << 8)));
	}

	
	SetAntennaConfig87SE(dev, priv->bDefaultAntenna1, priv->bSwAntennaDiverity);
}


void
UpdateInitialGain(
	struct net_device *dev
	)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	
	if (priv->eRFPowerState != eRfOn)	{
		priv->InitialGain = priv->InitialGainBackUp;
		return;
	}

	switch (priv->InitialGain) {
	case 1: 
		write_phy_ofdm(dev, 0x17, 0x26);	mdelay(1);
		write_phy_ofdm(dev, 0x24, 0x86);	mdelay(1);
		write_phy_ofdm(dev, 0x05, 0xfa);	mdelay(1);
		break;

	case 2: 
		write_phy_ofdm(dev, 0x17, 0x36);	mdelay(1);
		write_phy_ofdm(dev, 0x24, 0x86);	mdelay(1);
		write_phy_ofdm(dev, 0x05, 0xfa);	mdelay(1);
		break;

	case 3: 
		write_phy_ofdm(dev, 0x17, 0x36);	mdelay(1);
		write_phy_ofdm(dev, 0x24, 0x86);	mdelay(1);
		write_phy_ofdm(dev, 0x05, 0xfb);	mdelay(1);
		break;

	case 4: 
		write_phy_ofdm(dev, 0x17, 0x46);	mdelay(1);
		write_phy_ofdm(dev, 0x24, 0x86);	mdelay(1);
		write_phy_ofdm(dev, 0x05, 0xfb);	mdelay(1);
		break;

	case 5: 
		write_phy_ofdm(dev, 0x17, 0x46);	mdelay(1);
		write_phy_ofdm(dev, 0x24, 0x96);	mdelay(1);
		write_phy_ofdm(dev, 0x05, 0xfb);	mdelay(1);
		break;

	case 6: 
		write_phy_ofdm(dev, 0x17, 0x56);	mdelay(1);
		write_phy_ofdm(dev, 0x24, 0x96);	mdelay(1);
		write_phy_ofdm(dev, 0x05, 0xfc);	mdelay(1);
		break;

	case 7: 
		write_phy_ofdm(dev, 0x17, 0x56);	mdelay(1);
		write_phy_ofdm(dev, 0x24, 0xa6);	mdelay(1);
		write_phy_ofdm(dev, 0x05, 0xfc);	mdelay(1);
		break;

	case 8:
		write_phy_ofdm(dev, 0x17, 0x66);	mdelay(1);
		write_phy_ofdm(dev, 0x24, 0xb6);	mdelay(1);
		write_phy_ofdm(dev, 0x05, 0xfc);	mdelay(1);
		break;

	default:	
		write_phy_ofdm(dev, 0x17, 0x26);	mdelay(1);
		write_phy_ofdm(dev, 0x24, 0x86);	mdelay(1);
		write_phy_ofdm(dev, 0x05, 0xfa);	mdelay(1);
		break;
	}
}
void
InitTxPwrTracking87SE(
	struct net_device *dev
)
{
	u32	u4bRfReg;

	u4bRfReg = RF_ReadReg(dev, 0x02);

	
	RF_WriteReg(dev, 0x02, u4bRfReg|PWR_METER_EN);			mdelay(1);
}

void
PhyConfig8185(
	struct net_device *dev
	)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
		write_nic_dword(dev, RCR, priv->ReceiveConfig);
	   priv->RFProgType = read_nic_byte(dev, CONFIG4) & 0x03;
	
	ZEBRA_Config_85BASIC_HardCode(dev);
	
	if (priv->bDigMechanism)	{
		if (priv->InitialGain == 0)
			priv->InitialGain = 4;
	}

	if (priv->bTxPowerTrack)
		InitTxPwrTracking87SE(dev);

	priv->InitialGainBackUp = priv->InitialGain;
	UpdateInitialGain(dev);

	return;
}

void
HwConfigureRTL8185(
		struct net_device *dev
		)
{
	
	u8		bUNIVERSAL_CONTROL_RL = 0;
	u8		bUNIVERSAL_CONTROL_AGC = 1;
	u8		bUNIVERSAL_CONTROL_ANT = 1;
	u8		bAUTO_RATE_FALLBACK_CTL = 1;
	u8		val8;
	write_nic_word(dev, BRSR, 0x0fff);
	
	val8 = read_nic_byte(dev, CW_CONF);

	if (bUNIVERSAL_CONTROL_RL)
		val8 = val8 & 0xfd;
	else
		val8 = val8 | 0x02;

	write_nic_byte(dev, CW_CONF, val8);

	
	val8 = read_nic_byte(dev, TXAGC_CTL);
	if (bUNIVERSAL_CONTROL_AGC)	{
		write_nic_byte(dev, CCK_TXAGC, 128);
		write_nic_byte(dev, OFDM_TXAGC, 128);
		val8 = val8 & 0xfe;
	}	else	{
		val8 = val8 | 0x01 ;
	}


	write_nic_byte(dev, TXAGC_CTL, val8);

	
	val8 = read_nic_byte(dev, TXAGC_CTL);

	if (bUNIVERSAL_CONTROL_ANT)	{
		write_nic_byte(dev, ANTSEL, 0x00);
		val8 = val8 & 0xfd;
	}	else	{
		val8 = val8 & (val8|0x02); 
	}

	write_nic_byte(dev, TXAGC_CTL, val8);

	
	val8 = read_nic_byte(dev, RATE_FALLBACK);
	val8 &= 0x7c;
	if (bAUTO_RATE_FALLBACK_CTL)	{
		val8 |= RATE_FALLBACK_CTL_ENABLE | RATE_FALLBACK_CTL_AUTO_STEP1;

		
		PlatformIOWrite2Byte(dev, ARFR, 0x0fff); 
	}
	write_nic_byte(dev, RATE_FALLBACK, val8);
}

static void
MacConfig_85BASIC_HardCode(
	struct net_device *dev)
{
	int			nLinesRead = 0;

	u32	u4bRegOffset, u4bRegValue, u4bPageIndex = 0;
	int	i;

	nLinesRead = sizeof(MAC_REG_TABLE)/2;

	for (i = 0; i < nLinesRead; i++)	{	
		u4bRegOffset = MAC_REG_TABLE[i][0];
		u4bRegValue = MAC_REG_TABLE[i][1];

				if (u4bRegOffset == 0x5e)
					u4bPageIndex = u4bRegValue;

				else
						u4bRegOffset |= (u4bPageIndex << 8);

		write_nic_byte(dev, u4bRegOffset, (u8)u4bRegValue);
	}
	
}

static void
MacConfig_85BASIC(
	struct net_device *dev)
{

	u8			u1DA;
	MacConfig_85BASIC_HardCode(dev);

	

	
	write_nic_word(dev, TID_AC_MAP, 0xfa50);

	
	write_nic_word(dev, IntMig, 0x0000);

	
	PlatformIOWrite4Byte(dev, 0x1F0, 0x00000000);
	PlatformIOWrite4Byte(dev, 0x1F4, 0x00000000);
	PlatformIOWrite1Byte(dev, 0x1F8, 0x00);

	
	

	
	u1DA = read_nic_byte(dev, PHYPR);
	write_nic_byte(dev, PHYPR, (u1DA | BIT2));

	
	write_nic_word(dev, 0x360, 0x1000);
	write_nic_word(dev, 0x362, 0x1000);

	
	write_nic_word(dev, 0x370, 0x0560);
	write_nic_word(dev, 0x372, 0x0560);
	write_nic_word(dev, 0x374, 0x0DA4);
	write_nic_word(dev, 0x376, 0x0DA4);
	write_nic_word(dev, 0x378, 0x0560);
	write_nic_word(dev, 0x37A, 0x0560);
	write_nic_word(dev, 0x37C, 0x00EC);
	write_nic_word(dev, 0x37E, 0x00EC);	
	write_nic_byte(dev, 0x24E, 0x01);
}

u8
GetSupportedWirelessMode8185(
	struct net_device *dev
)
{
	u8			btSupportedWirelessMode = 0;

	btSupportedWirelessMode = (WIRELESS_MODE_B | WIRELESS_MODE_G);
	return btSupportedWirelessMode;
}

void
ActUpdateChannelAccessSetting(
	struct net_device *dev,
	WIRELESS_MODE			WirelessMode,
	PCHANNEL_ACCESS_SETTING	ChnlAccessSetting
	)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee80211;
	AC_CODING	eACI;
	AC_PARAM	AcParam;
	u8	bFollowLegacySetting = 0;
	u8   u1bAIFS;

	ChnlAccessSetting->SIFS_Timer = 0x22; 
	ChnlAccessSetting->DIFS_Timer = 0x1C; 
	ChnlAccessSetting->SlotTimeTimer = 9; 
	ChnlAccessSetting->EIFS_Timer = 0x5B; 
	ChnlAccessSetting->CWminIndex = 3; 
	ChnlAccessSetting->CWmaxIndex = 7; 

	write_nic_byte(dev, SIFS, ChnlAccessSetting->SIFS_Timer);
	write_nic_byte(dev, SLOT, ChnlAccessSetting->SlotTimeTimer);	

	u1bAIFS = aSifsTime + (2 * ChnlAccessSetting->SlotTimeTimer);

	write_nic_byte(dev, EIFS, ChnlAccessSetting->EIFS_Timer);

	write_nic_byte(dev, AckTimeOutReg, 0x5B); 

	{ 
		bFollowLegacySetting = 1;

	}

	
	if (bFollowLegacySetting)	{

		AcParam.longData = 0;
		AcParam.f.AciAifsn.f.AIFSN = 2; 
		AcParam.f.AciAifsn.f.ACM = 0;
		AcParam.f.Ecw.f.ECWmin = ChnlAccessSetting->CWminIndex; 
		AcParam.f.Ecw.f.ECWmax = ChnlAccessSetting->CWmaxIndex; 
		AcParam.f.TXOPLimit = 0;

		
		
		if (ieee->current_network.Turbo_Enable == 1)
			AcParam.f.TXOPLimit = 0x01FF;
		
		if (ieee->iw_mode == IW_MODE_ADHOC)
			AcParam.f.TXOPLimit = 0x0020;

		for (eACI = 0; eACI < AC_MAX; eACI++)	{
			AcParam.f.AciAifsn.f.ACI = (u8)eACI;
			{
				PAC_PARAM	pAcParam = (PAC_PARAM)(&AcParam);
				AC_CODING	eACI;
				u8		u1bAIFS;
				u32		u4bAcParam;

				
				eACI = pAcParam->f.AciAifsn.f.ACI;
				u1bAIFS = pAcParam->f.AciAifsn.f.AIFSN * ChnlAccessSetting->SlotTimeTimer + aSifsTime;
				u4bAcParam = ((((u32)(pAcParam->f.TXOPLimit)) << AC_PARAM_TXOP_LIMIT_OFFSET)	|
						(((u32)(pAcParam->f.Ecw.f.ECWmax)) << AC_PARAM_ECW_MAX_OFFSET)	|
						(((u32)(pAcParam->f.Ecw.f.ECWmin)) << AC_PARAM_ECW_MIN_OFFSET)	|
						(((u32)u1bAIFS) << AC_PARAM_AIFS_OFFSET));

				switch (eACI)	{
				case AC1_BK:
					
					break;

				case AC0_BE:
					
					break;

				case AC2_VI:
					
					break;

				case AC3_VO:
					
					break;

				default:
					DMESGW("SetHwReg8185(): invalid ACI: %d !\n", eACI);
					break;
				}

				
				
				{
					PACI_AIFSN	pAciAifsn = (PACI_AIFSN)(&pAcParam->f.AciAifsn);
					AC_CODING	eACI = pAciAifsn->f.ACI;

					
					
					u8	AcmCtrl = 0;
					if (pAciAifsn->f.ACM)	{
						
						switch (eACI)	{
						case AC0_BE:
							AcmCtrl |= (BEQ_ACM_EN|BEQ_ACM_CTL|ACM_HW_EN);	
							break;

						case AC2_VI:
							AcmCtrl |= (VIQ_ACM_EN|VIQ_ACM_CTL|ACM_HW_EN);	
							break;

						case AC3_VO:
							AcmCtrl |= (VOQ_ACM_EN|VOQ_ACM_CTL|ACM_HW_EN);	
							break;

						default:
							DMESGW("SetHwReg8185(): [HW_VAR_ACM_CTRL] ACM set failed: eACI is %d\n", eACI);
							break;
						}
					}	else	{
						
						switch (eACI)	{
						case AC0_BE:
							AcmCtrl &= ((~BEQ_ACM_EN) & (~BEQ_ACM_CTL) & (~ACM_HW_EN));	
							break;

						case AC2_VI:
							AcmCtrl &= ((~VIQ_ACM_EN) & (~VIQ_ACM_CTL) & (~ACM_HW_EN));	
							break;

						case AC3_VO:
							AcmCtrl &= ((~VOQ_ACM_EN) & (~VOQ_ACM_CTL) & (~ACM_HW_EN));	
							break;

						default:
							break;
						}
					}
					write_nic_byte(dev, ACM_CONTROL, 0);
				}
			}
		}
	}
}

void
ActSetWirelessMode8185(
	struct net_device *dev,
	u8				btWirelessMode
	)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee80211;
	u8	btSupportedWirelessMode = GetSupportedWirelessMode8185(dev);

	if ((btWirelessMode & btSupportedWirelessMode) == 0)	{
		
		DMESGW("ActSetWirelessMode8185(): WirelessMode(%d) is not supported (%d)!\n",
			btWirelessMode, btSupportedWirelessMode);
		return;
	}

	
	if (btWirelessMode == WIRELESS_MODE_AUTO)	{
		if ((btSupportedWirelessMode & WIRELESS_MODE_A))	{
			btWirelessMode = WIRELESS_MODE_A;
		}	else if (btSupportedWirelessMode & WIRELESS_MODE_G)	{
				btWirelessMode = WIRELESS_MODE_G;

		}	else if ((btSupportedWirelessMode & WIRELESS_MODE_B))	{
				btWirelessMode = WIRELESS_MODE_B;
		}	else	{
				DMESGW("ActSetWirelessMode8185(): No valid wireless mode supported, btSupportedWirelessMode(%x)!!!\n",
						btSupportedWirelessMode);
				btWirelessMode = WIRELESS_MODE_B;
		}
	}


	ieee->mode = (WIRELESS_MODE)btWirelessMode;

	
	if( ieee->mode == WIRELESS_MODE_A )	{
		DMESG("WIRELESS_MODE_A\n");
	}	else if( ieee->mode == WIRELESS_MODE_B )	{
			DMESG("WIRELESS_MODE_B\n");
	}	else if( ieee->mode == WIRELESS_MODE_G )	{
			DMESG("WIRELESS_MODE_G\n");
	}
	ActUpdateChannelAccessSetting( dev, ieee->mode, &priv->ChannelAccessSetting);
}

void rtl8185b_irq_enable(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	priv->irq_enabled = 1;
	write_nic_dword(dev, IMR, priv->IntrMask);
}
void
DrvIFIndicateDisassociation(
	struct net_device *dev,
	u16			reason
	)
{
		
	}
void
MgntDisconnectIBSS(
	struct net_device *dev
)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	u8			i;

	DrvIFIndicateDisassociation(dev, unspec_reason);

	for (i = 0; i < 6 ; i++)
		priv->ieee80211->current_network.bssid[i] = 0x55;



	priv->ieee80211->state = IEEE80211_NOLINK;
	ieee80211_stop_send_beacons(priv->ieee80211);

	priv->ieee80211->link_change(dev);
	notify_wx_assoc_event(priv->ieee80211);
}
void
MlmeDisassociateRequest(
	struct net_device *dev,
	u8 *asSta,
	u8	asRsn
	)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	u8 i;

	SendDisassociation(priv->ieee80211, asSta, asRsn);

	if (memcmp(priv->ieee80211->current_network.bssid, asSta, 6) == 0)	{
		
		
		DrvIFIndicateDisassociation(dev, unspec_reason);



		for (i = 0; i < 6; i++)
			priv->ieee80211->current_network.bssid[i] = 0x22;

		ieee80211_disassociate(priv->ieee80211);
	}

}

void
MgntDisconnectAP(
	struct net_device *dev,
	u8			asRsn
)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	MlmeDisassociateRequest(dev, priv->ieee80211->current_network.bssid, asRsn);

	priv->ieee80211->state = IEEE80211_NOLINK;
}
bool
MgntDisconnect(
	struct net_device *dev,
	u8			asRsn
)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	if (IS_DOT11D_ENABLE(priv->ieee80211))
		Dot11d_Reset(priv->ieee80211);
	
	if (priv->ieee80211->state == IEEE80211_LINKED)	{
		if (priv->ieee80211->iw_mode == IW_MODE_ADHOC)
			MgntDisconnectIBSS(dev);

		if (priv->ieee80211->iw_mode == IW_MODE_INFRA)	{
			MgntDisconnectAP(dev, asRsn);
		}
		
	}
	return true;
}
bool
SetRFPowerState(
	struct net_device *dev,
	RT_RF_POWER_STATE	eRFPowerState
	)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	bool			bResult = false;

	if (eRFPowerState == priv->eRFPowerState)
		return bResult;

	 bResult = SetZebraRFPowerState8185(dev, eRFPowerState);

	return bResult;
}
void
HalEnableRx8185Dummy(
	struct net_device *dev
	)
{
}
void
HalDisableRx8185Dummy(
	struct net_device *dev
	)
{
}

bool
MgntActSet_RF_State(
	struct net_device *dev,
	RT_RF_POWER_STATE	StateToSet,
	u32	ChangeSource
	)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	bool				bActionAllowed = false;
	bool				bConnectBySSID = false;
	RT_RF_POWER_STATE	rtState;
	u16				RFWaitCounter = 0;
	unsigned long flag;
	while (true)	{
		spin_lock_irqsave(&priv->rf_ps_lock, flag);
		if (priv->RFChangeInProgress)	{
			spin_unlock_irqrestore(&priv->rf_ps_lock, flag);
			
			while (priv->RFChangeInProgress)	{
				RFWaitCounter++;
				udelay(1000); 

				
				if (RFWaitCounter > 1000)	{	
					printk("MgntActSet_RF_State(): Wait too long to set RF\n");
					
					return false;
				}
			}
		}	else	{
			priv->RFChangeInProgress = true;
			spin_unlock_irqrestore(&priv->rf_ps_lock, flag);
			break;
		}
	}
	rtState = priv->eRFPowerState;

	switch (StateToSet)	{
	case eRfOn:
		priv->RfOffReason &= (~ChangeSource);

		if (!priv->RfOffReason)	{
			priv->RfOffReason = 0;
			bActionAllowed = true;

			if (rtState == eRfOff && ChangeSource >= RF_CHANGE_BY_HW && !priv->bInHctTest)
				bConnectBySSID = true;

		}	else
				;
		break;

	case eRfOff:
		 

			if (priv->RfOffReason > RF_CHANGE_BY_IPS)	{
				MgntDisconnect(dev, disas_lv_ss);

				
			}

		priv->RfOffReason |= ChangeSource;
		bActionAllowed = true;
		break;
	case eRfSleep:
		priv->RfOffReason |= ChangeSource;
		bActionAllowed = true;
		break;
	default:
		break;
	}

	if (bActionAllowed)	{
				
		SetRFPowerState(dev, StateToSet);

		
		if (StateToSet == eRfOn)		{
			HalEnableRx8185Dummy(dev);
			if (bConnectBySSID)	{
				
			}
		}
		
		else if (StateToSet == eRfOff)
			HalDisableRx8185Dummy(dev);

	}

	
	spin_lock_irqsave(&priv->rf_ps_lock, flag);
	priv->RFChangeInProgress = false;
	spin_unlock_irqrestore(&priv->rf_ps_lock, flag);
	return bActionAllowed;
}
void
InactivePowerSave(
	struct net_device *dev
	)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	priv->bSwRfProcessing = true;

	MgntActSet_RF_State(dev, priv->eInactivePowerState, RF_CHANGE_BY_IPS);


	priv->bSwRfProcessing = false;
}

void
IPSEnter(
	struct net_device *dev
	)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	RT_RF_POWER_STATE rtState;
	if (priv->bInactivePs)	{
		rtState = priv->eRFPowerState;

		if (rtState == eRfOn && !priv->bSwRfProcessing
			&& (priv->ieee80211->state != IEEE80211_LINKED))	{
			priv->eInactivePowerState = eRfOff;
			InactivePowerSave(dev);
		}
	}
}
void
IPSLeave(
	struct net_device *dev
	)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	RT_RF_POWER_STATE rtState;
	if (priv->bInactivePs)	{
		rtState = priv->eRFPowerState;
		if ((rtState == eRfOff || rtState == eRfSleep) && (!priv->bSwRfProcessing) && priv->RfOffReason <= RF_CHANGE_BY_IPS)	{
			priv->eInactivePowerState = eRfOn;
			InactivePowerSave(dev);
		}
	}
}

void rtl8185b_adapter_start(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee80211;

	u8 SupportedWirelessMode;
	u8			InitWirelessMode;
	u8			bInvalidWirelessMode = 0;
	u8 tmpu8;
	u8 btCR9346;
	u8 TmpU1b;
	u8 btPSR;

	write_nic_byte(dev, 0x24e, (BIT5|BIT6|BIT0));
	rtl8180_reset(dev);

	priv->dma_poll_mask = 0;
	priv->dma_poll_stop_mask = 0;

	HwConfigureRTL8185(dev);
	write_nic_dword(dev, MAC0, ((u32 *)dev->dev_addr)[0]);
	write_nic_word(dev, MAC4, ((u32 *)dev->dev_addr)[1] & 0xffff);
	write_nic_byte(dev, MSR, read_nic_byte(dev, MSR) & 0xf3);	
	write_nic_word(dev, BcnItv, 100);
	write_nic_word(dev, AtimWnd, 2);
	PlatformIOWrite2Byte(dev, FEMR, 0xFFFF);
	write_nic_byte(dev, WPA_CONFIG, 0);
	MacConfig_85BASIC(dev);
	
	
	PlatformIOWrite2Byte(dev, RFSW_CTRL, 0x569a);

	
	write_nic_byte(dev, CR9346, 0xc0);	
	tmpu8 = read_nic_byte(dev, CONFIG3);
	write_nic_byte(dev, CONFIG3, (tmpu8 | CONFIG3_PARM_En));
	
	
	write_nic_dword(dev, ANAPARAM2, ANAPARM2_ASIC_ON);
	write_nic_dword(dev, ANAPARAM, ANAPARM_ASIC_ON);
	write_nic_word(dev, ANAPARAM3, 0x0010);

	write_nic_byte(dev, CONFIG3, tmpu8);
	write_nic_byte(dev, CR9346, 0x00);
	
	btCR9346 = read_nic_byte(dev, CR9346);
	write_nic_byte(dev, CR9346, (btCR9346 | 0xC0));

	
	TmpU1b = read_nic_byte(dev, CONFIG5);
	TmpU1b = TmpU1b & ~BIT3;
	write_nic_byte(dev, CONFIG5, TmpU1b);

	
	btCR9346 &= ~(0xC0);
	write_nic_byte(dev, CR9346, btCR9346);

	
	
	btPSR = read_nic_byte(dev, PSR);
	write_nic_byte(dev, PSR, (btPSR | BIT3));
	
	write_nic_word(dev, RFPinsOutput, 0x0480);
	SetOutputEnableOfRfPins(dev);
	write_nic_word(dev, RFPinsSelect, 0x2488);

	
	PhyConfig8185(dev);

	SupportedWirelessMode = GetSupportedWirelessMode8185(dev);
	if ((ieee->mode != WIRELESS_MODE_B) &&
		(ieee->mode != WIRELESS_MODE_G) &&
		(ieee->mode != WIRELESS_MODE_A) &&
		(ieee->mode != WIRELESS_MODE_AUTO))	{
		
		bInvalidWirelessMode = 1;
	}	else	{
	
		
		if	((ieee->mode != WIRELESS_MODE_AUTO) &&
			(ieee->mode & SupportedWirelessMode) == 0)	{
			bInvalidWirelessMode = 1;
		}
	}

	if (bInvalidWirelessMode || ieee->mode == WIRELESS_MODE_AUTO)	{
		
		
		if ((SupportedWirelessMode & WIRELESS_MODE_A))	{
			InitWirelessMode = WIRELESS_MODE_A;
		}	else if ((SupportedWirelessMode & WIRELESS_MODE_G))	{
			InitWirelessMode = WIRELESS_MODE_G;
		}	else if ((SupportedWirelessMode & WIRELESS_MODE_B))	{
			InitWirelessMode = WIRELESS_MODE_B;
		}	else	{
			DMESGW("InitializeAdapter8185(): No valid wireless mode supported, SupportedWirelessMode(%x)!!!\n",
				 SupportedWirelessMode);
			InitWirelessMode = WIRELESS_MODE_B;
		}

		
		if (bInvalidWirelessMode)
			ieee->mode = (WIRELESS_MODE)InitWirelessMode;

	}	else	{
	
		InitWirelessMode = ieee->mode;
	}
	priv->eRFPowerState = eRfOff;
	priv->RfOffReason = 0;
	{
		MgntActSet_RF_State(dev, eRfOn, 0);
	}
	if (priv->bInactivePs)
		MgntActSet_RF_State(dev , eRfOff, RF_CHANGE_BY_IPS);


	ActSetWirelessMode8185(dev, (u8)(InitWirelessMode));

	

	rtl8185b_irq_enable(dev);

	netif_start_queue(dev);
	}

void rtl8185b_rx_enable(struct net_device *dev)
{
	u8 cmd;
	
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);


	if (dev->flags & IFF_PROMISC)
		DMESG("NIC in promisc mode");

	if (priv->ieee80211->iw_mode == IW_MODE_MONITOR || \
	   dev->flags & IFF_PROMISC)	{
		priv->ReceiveConfig = priv->ReceiveConfig & (~RCR_APM);
		priv->ReceiveConfig = priv->ReceiveConfig | RCR_AAP;
	}

	if (priv->ieee80211->iw_mode == IW_MODE_MONITOR)
		priv->ReceiveConfig = priv->ReceiveConfig | RCR_ACF | RCR_APWRMGT | RCR_AICV;


	if (priv->crcmon == 1 && priv->ieee80211->iw_mode == IW_MODE_MONITOR)
		priv->ReceiveConfig = priv->ReceiveConfig | RCR_ACRC32;

	write_nic_dword(dev, RCR, priv->ReceiveConfig);

	fix_rx_fifo(dev);

	cmd = read_nic_byte(dev, CMD);
	write_nic_byte(dev, CMD, cmd | (1<<CMD_RX_ENABLE_SHIFT));

}

void rtl8185b_tx_enable(struct net_device *dev)
{
	u8 cmd;
	u8 byte;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	write_nic_dword(dev, TCR, priv->TransmitConfig);
	byte = read_nic_byte(dev, MSR);
	byte |= MSR_LINK_ENEDCA;
	write_nic_byte(dev, MSR, byte);

	fix_tx_fifo(dev);

	cmd = read_nic_byte(dev, CMD);
	write_nic_byte(dev, CMD, cmd | (1<<CMD_TX_ENABLE_SHIFT));
}

