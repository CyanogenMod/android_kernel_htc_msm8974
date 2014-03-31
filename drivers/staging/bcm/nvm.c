#include "headers.h"

#define DWORD unsigned int

static INT BcmDoChipSelect(PMINI_ADAPTER Adapter, UINT offset);
static INT BcmGetActiveDSD(PMINI_ADAPTER Adapter);
static INT BcmGetActiveISO(PMINI_ADAPTER Adapter);
static UINT BcmGetEEPROMSize(PMINI_ADAPTER Adapter);
static INT BcmGetFlashCSInfo(PMINI_ADAPTER Adapter);
static UINT BcmGetFlashSectorSize(PMINI_ADAPTER Adapter, UINT FlashSectorSizeSig, UINT FlashSectorSize);

static VOID BcmValidateNvmType(PMINI_ADAPTER Adapter);
static INT BcmGetNvmSize(PMINI_ADAPTER Adapter);
static UINT BcmGetFlashSize(PMINI_ADAPTER Adapter);
static NVM_TYPE BcmGetNvmType(PMINI_ADAPTER Adapter);

static INT BcmGetSectionValEndOffset(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL eFlash2xSectionVal);

static B_UINT8 IsOffsetWritable(PMINI_ADAPTER Adapter, UINT uiOffset);
static INT IsSectionWritable(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL Section);
static INT IsSectionExistInVendorInfo(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL section);

static INT ReadDSDPriority(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL dsd);
static INT ReadDSDSignature(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL dsd);
static INT ReadISOPriority(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL iso);
static INT ReadISOSignature(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL iso);

static INT CorruptDSDSig(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL eFlash2xSectionVal);
static INT CorruptISOSig(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL eFlash2xSectionVal);
static INT SaveHeaderIfPresent(PMINI_ADAPTER Adapter, PUCHAR pBuff, UINT uiSectAlignAddr);
static INT WriteToFlashWithoutSectorErase(PMINI_ADAPTER Adapter, PUINT pBuff,
					  FLASH2X_SECTION_VAL eFlash2xSectionVal,
					  UINT uiOffset, UINT uiNumBytes);
static FLASH2X_SECTION_VAL getHighestPriDSD(PMINI_ADAPTER Adapter);
static FLASH2X_SECTION_VAL getHighestPriISO(PMINI_ADAPTER Adapter);

static INT BeceemFlashBulkRead(
	PMINI_ADAPTER Adapter,
	PUINT pBuffer,
	UINT uiOffset,
	UINT uiNumBytes);

static INT BeceemFlashBulkWrite(
	PMINI_ADAPTER Adapter,
	PUINT pBuffer,
	UINT uiOffset,
	UINT uiNumBytes,
	BOOLEAN bVerify);

static INT GetFlashBaseAddr(PMINI_ADAPTER Adapter);

static INT ReadBeceemEEPROMBulk(PMINI_ADAPTER Adapter,UINT dwAddress, UINT *pdwData, UINT dwNumData);


static UCHAR ReadEEPROMStatusRegister( PMINI_ADAPTER Adapter )
{
	UCHAR uiData = 0;
	DWORD dwRetries = MAX_EEPROM_RETRIES*RETRIES_PER_DELAY;
	UINT uiStatus = 0;
	UINT value = 0;
	UINT value1 = 0;

	
	value = EEPROM_READ_STATUS_REGISTER ;
	wrmalt( Adapter, EEPROM_CMDQ_SPI_REG, &value, sizeof(value));

	while ( dwRetries != 0 )
	{
		value=0;
		uiStatus = 0 ;
		rdmalt(Adapter, EEPROM_SPI_Q_STATUS1_REG, &uiStatus, sizeof(uiStatus));
		if(Adapter->device_removed == TRUE)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Modem has got removed hence exiting....");
			break;
		}

		
		if ( ( uiStatus & EEPROM_READ_DATA_AVAIL) != 0 )
		{
			
			value = uiStatus & (EEPROM_READ_DATA_AVAIL | EEPROM_READ_DATA_FULL);
			wrmalt( Adapter, EEPROM_SPI_Q_STATUS1_REG, &value, sizeof(value));

			value =0;
			rdmalt(Adapter, EEPROM_READ_DATAQ_REG, &value, sizeof(value));
			uiData = (UCHAR)value;

			break;
		}

		dwRetries-- ;
		if ( dwRetries == 0 )
		{
			 rdmalt(Adapter, EEPROM_SPI_Q_STATUS1_REG, &value, sizeof(value));
			 rdmalt(Adapter, EEPROM_SPI_Q_STATUS_REG, &value1, sizeof(value1));
			 BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"0x3004 = %x 0x3008 = %x, retries = %d failed.\n",value,value1,  MAX_EEPROM_RETRIES*RETRIES_PER_DELAY);
			return uiData;
		}
		if( !(dwRetries%RETRIES_PER_DELAY) )
			msleep(1);
		uiStatus = 0 ;
	}
	return uiData;
} 


INT ReadBeceemEEPROMBulk( PMINI_ADAPTER Adapter,
									   DWORD dwAddress,
									   DWORD *pdwData,
									   DWORD dwNumWords
									 )
{
	DWORD dwIndex = 0;
	DWORD dwRetries = MAX_EEPROM_RETRIES*RETRIES_PER_DELAY;
	UINT uiStatus  = 0;
	UINT value= 0;
	UINT value1 = 0;
	UCHAR *pvalue;

	
	value=( EEPROM_READ_QUEUE_FLUSH | EEPROM_CMD_QUEUE_FLUSH );
	wrmalt( Adapter, SPI_FLUSH_REG, &value, sizeof(value) );
	value=0;
	wrmalt( Adapter, SPI_FLUSH_REG, &value, sizeof(value));

	
	value=( EEPROM_READ_DATA_AVAIL | EEPROM_READ_DATA_FULL );
	wrmalt( Adapter, EEPROM_SPI_Q_STATUS1_REG,&value, sizeof(value));

	value= dwAddress | ( (dwNumWords == 4) ? EEPROM_16_BYTE_PAGE_READ : EEPROM_4_BYTE_PAGE_READ );
	wrmalt( Adapter, EEPROM_CMDQ_SPI_REG, &value, sizeof(value));

	while ( dwRetries != 0 )
		{

		uiStatus = 0;
		rdmalt(Adapter, EEPROM_SPI_Q_STATUS1_REG, &uiStatus, sizeof(uiStatus));
		if(Adapter->device_removed == TRUE)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Modem has got Removed.hence exiting from loop...");
			return -ENODEV;
		}

		if ( dwNumWords == 4 )
		{
			if ( ( uiStatus & EEPROM_READ_DATA_FULL ) != 0 )
			{
				
				value = ( uiStatus & (EEPROM_READ_DATA_AVAIL | EEPROM_READ_DATA_FULL) ) ;
				wrmalt( Adapter, EEPROM_SPI_Q_STATUS1_REG,&value, sizeof(value));
				break;
			}
		}
		else if ( dwNumWords == 1 )
		{

			if ( ( uiStatus & EEPROM_READ_DATA_AVAIL ) != 0 )
			{
				if (Adapter->chip_id == 0xBECE0210 )
	  					udelay(800);

				
				value=( uiStatus & (EEPROM_READ_DATA_AVAIL | EEPROM_READ_DATA_FULL) );
				wrmalt( Adapter, EEPROM_SPI_Q_STATUS1_REG,&value, sizeof(value));
				break;
			}
		}

		uiStatus = 0;

		dwRetries--;
		if(dwRetries == 0)
		{
			value=0;
			value1=0;
			rdmalt(Adapter, EEPROM_SPI_Q_STATUS1_REG, &value, sizeof(value));
			rdmalt(Adapter, EEPROM_SPI_Q_STATUS_REG, &value1, sizeof(value1));
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "dwNumWords %d 0x3004 = %x 0x3008 = %x  retries = %d failed.\n", dwNumWords, value,  value1,  MAX_EEPROM_RETRIES*RETRIES_PER_DELAY);
			return STATUS_FAILURE;
		}
		if( !(dwRetries%RETRIES_PER_DELAY) )
			msleep(1);
	}

	for ( dwIndex = 0; dwIndex < dwNumWords ; dwIndex++ )
	{
		
		pvalue = (PUCHAR)(pdwData + dwIndex);

		value =0;
		rdmalt(Adapter, EEPROM_READ_DATAQ_REG, &value, sizeof(value));

		pvalue[0] = value;

		value = 0;
		rdmalt(Adapter, EEPROM_READ_DATAQ_REG, &value, sizeof(value));

		pvalue[1] = value;

		value =0;
		rdmalt(Adapter, EEPROM_READ_DATAQ_REG, &value, sizeof(value));

		pvalue[2] = value;

		value = 0;
		rdmalt(Adapter, EEPROM_READ_DATAQ_REG, &value, sizeof(value));

		pvalue[3] = value;
	}

	return STATUS_SUCCESS;
} 


INT ReadBeceemEEPROM( PMINI_ADAPTER Adapter,
								   DWORD uiOffset,
								   DWORD *pBuffer
								 )
{
	UINT uiData[8]	 	= {0};
	UINT uiByteOffset	= 0;
	UINT uiTempOffset	= 0;

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL," ====> ");

	uiTempOffset = uiOffset - (uiOffset % MAX_RW_SIZE);
	uiByteOffset = uiOffset - uiTempOffset;

	ReadBeceemEEPROMBulk(Adapter, uiTempOffset, (PUINT)&uiData[0], 4);

	if ( uiByteOffset > 12 )
	{
		ReadBeceemEEPROMBulk(Adapter, uiTempOffset + MAX_RW_SIZE, (PUINT)&uiData[4], 4);
	}

	memcpy( (PUCHAR) pBuffer, ( ((PUCHAR)&uiData[0]) + uiByteOffset ), 4);

	return STATUS_SUCCESS;
} 



INT ReadMacAddressFromNVM(PMINI_ADAPTER Adapter)
{
	INT Status;
	unsigned char puMacAddr[6];

	Status = BeceemNVMRead(Adapter,
			(PUINT)&puMacAddr[0],
			INIT_PARAMS_1_MACADDRESS_ADDRESS,
			MAC_ADDRESS_SIZE);

	if(Status == STATUS_SUCCESS)
		memcpy(Adapter->dev->dev_addr, puMacAddr, MAC_ADDRESS_SIZE);

	return Status;
}


INT BeceemEEPROMBulkRead(
	PMINI_ADAPTER Adapter,
	PUINT pBuffer,
	UINT uiOffset,
	UINT uiNumBytes)
{
	UINT uiData[4]		  = {0};
	
	UINT uiBytesRemaining = uiNumBytes;
	UINT uiIndex 		  = 0;
	UINT uiTempOffset  	  = 0;
	UINT uiExtraBytes     = 0;
	UINT uiFailureRetries = 0;
	PUCHAR pcBuff = (PUCHAR)pBuffer;


	if(uiOffset%MAX_RW_SIZE&& uiBytesRemaining)
	{
		uiTempOffset = uiOffset - (uiOffset%MAX_RW_SIZE);
		uiExtraBytes = uiOffset-uiTempOffset;
		ReadBeceemEEPROMBulk(Adapter,uiTempOffset,(PUINT)&uiData[0],4);
		if(uiBytesRemaining >= (MAX_RW_SIZE - uiExtraBytes))
		{
			memcpy(pBuffer,(((PUCHAR)&uiData[0])+uiExtraBytes),MAX_RW_SIZE - uiExtraBytes);

			uiBytesRemaining -= (MAX_RW_SIZE - uiExtraBytes);
			uiIndex += (MAX_RW_SIZE - uiExtraBytes);
			uiOffset += (MAX_RW_SIZE - uiExtraBytes);
		}
		else
		{
			memcpy(pBuffer,(((PUCHAR)&uiData[0])+uiExtraBytes),uiBytesRemaining);
			uiIndex += uiBytesRemaining;
			uiOffset += uiBytesRemaining;
			uiBytesRemaining = 0;
		}


	}


	while(uiBytesRemaining && uiFailureRetries != 128)
	{
		if(Adapter->device_removed )
		{
			return -1;
		}

		if(uiBytesRemaining >= MAX_RW_SIZE)
		{
			if(0 == ReadBeceemEEPROMBulk(Adapter,uiOffset,&uiData[0],4))
			{
				memcpy(pcBuff+uiIndex,&uiData[0],MAX_RW_SIZE);
				uiOffset += MAX_RW_SIZE;
				uiBytesRemaining -= MAX_RW_SIZE;
				uiIndex += MAX_RW_SIZE;
			}
			else
			{
				uiFailureRetries++;
				mdelay(3);
			}
		}
		else if(uiBytesRemaining >= 4)
		{
			if(0 == ReadBeceemEEPROM(Adapter,uiOffset,&uiData[0]))
			{
				memcpy(pcBuff+uiIndex,&uiData[0],4);
				uiOffset += 4;
				uiBytesRemaining -= 4;
				uiIndex +=4;
			}
			else
			{
				uiFailureRetries++;
				mdelay(3);
			}
		}
		else
		{ 
			PUCHAR pCharBuff = (PUCHAR)pBuffer;
			pCharBuff += uiIndex;
			if(0 == ReadBeceemEEPROM(Adapter,uiOffset,&uiData[0]))
			{
				memcpy(pCharBuff,&uiData[0],uiBytesRemaining);
				uiBytesRemaining = 0;
			}
			else
			{
				uiFailureRetries++;
				mdelay(3);
			}
		}

	}

	return 0;
}


static INT BeceemFlashBulkRead(
	PMINI_ADAPTER Adapter,
	PUINT pBuffer,
	UINT uiOffset,
	UINT uiNumBytes)
{
	UINT uiIndex = 0;
	UINT uiBytesToRead = uiNumBytes;
	INT Status = 0;
	UINT uiPartOffset = 0;
	int bytes;

	if(Adapter->device_removed )
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Device Got Removed ");
		return -ENODEV;
	}

	
#if defined(BCM_SHM_INTERFACE) && !defined(FLASH_DIRECT_ACCESS)
  Status = bcmflash_raw_read((uiOffset/FLASH_PART_SIZE),(uiOffset % FLASH_PART_SIZE),( unsigned char *)pBuffer,uiNumBytes);
  return Status;
#endif

	Adapter->SelectedChip = RESET_CHIP_SELECT;

	if(uiOffset % MAX_RW_SIZE)
	{
		BcmDoChipSelect(Adapter,uiOffset);
		uiPartOffset = (uiOffset & (FLASH_PART_SIZE - 1)) + GetFlashBaseAddr(Adapter);

		uiBytesToRead = MAX_RW_SIZE - (uiOffset%MAX_RW_SIZE);
		uiBytesToRead = MIN(uiNumBytes,uiBytesToRead);

		bytes = rdm(Adapter, uiPartOffset, (PCHAR)pBuffer+uiIndex, uiBytesToRead);
		if (bytes < 0) {
			Status = bytes;
			Adapter->SelectedChip = RESET_CHIP_SELECT;
			return Status;
		}

		uiIndex += uiBytesToRead;
		uiOffset += uiBytesToRead;
		uiNumBytes -= uiBytesToRead;
	}

	while(uiNumBytes)
	{
		BcmDoChipSelect(Adapter,uiOffset);
		uiPartOffset = (uiOffset & (FLASH_PART_SIZE - 1)) + GetFlashBaseAddr(Adapter);

		uiBytesToRead = MIN(uiNumBytes,MAX_RW_SIZE);

		bytes = rdm(Adapter, uiPartOffset, (PCHAR)pBuffer+uiIndex, uiBytesToRead);
		if (bytes < 0) {
			Status = bytes;
			break;
		}


		uiIndex += uiBytesToRead;
		uiOffset += uiBytesToRead;
		uiNumBytes -= uiBytesToRead;

	}
	Adapter->SelectedChip = RESET_CHIP_SELECT;
	return Status;
}


static UINT BcmGetFlashSize(PMINI_ADAPTER Adapter)
{
	if(IsFlash2x(Adapter))
		return 	(Adapter->psFlash2xCSInfo->OffsetFromDSDStartForDSDHeader + sizeof(DSD_HEADER));
	else
		return 32*1024;


}


static UINT BcmGetEEPROMSize(PMINI_ADAPTER Adapter)
{
	UINT uiData = 0;
	UINT uiIndex = 0;

	BeceemEEPROMBulkRead(Adapter,&uiData,0x0,4);
	if(uiData == BECM)
	{
		for(uiIndex = 2;uiIndex <=256; uiIndex*=2)
		{
			BeceemEEPROMBulkRead(Adapter,&uiData,uiIndex*1024,4);
			if(uiData == BECM)
			{
				return uiIndex*1024;
			}
		}
	}
	else
	{

        uiData = 0xBABEFACE;
		if(0 == BeceemEEPROMBulkWrite(Adapter,(PUCHAR)&uiData,0,4,TRUE))
		{
			uiData = 0;
			for(uiIndex = 2;uiIndex <=256; uiIndex*=2)
			{
				BeceemEEPROMBulkRead(Adapter,&uiData,uiIndex*1024,4);
				if(uiData == 0xBABEFACE)
				{
					return uiIndex*1024;
				}
			}
		}

	}
	return 0;
}




static INT FlashSectorErase(PMINI_ADAPTER Adapter,
	UINT addr,
	UINT numOfSectors)
{
	UINT iIndex = 0, iRetries = 0;
	UINT uiStatus = 0;
	UINT value;
	int bytes;

	for(iIndex=0;iIndex<numOfSectors;iIndex++)
	{
		value = 0x06000000;
		wrmalt(Adapter, FLASH_SPI_CMDQ_REG, &value, sizeof(value));

		value = (0xd8000000 | (addr & 0xFFFFFF));
		wrmalt(Adapter, FLASH_SPI_CMDQ_REG, &value, sizeof(value));
		iRetries = 0;

		do
		{
			value = (FLASH_CMD_STATUS_REG_READ << 24);
			if(wrmalt(Adapter, FLASH_SPI_CMDQ_REG, &value, sizeof(value)) < 0)
			{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Programing of FLASH_SPI_CMDQ_REG fails");
				return STATUS_FAILURE;
			}

			bytes = rdmalt(Adapter, FLASH_SPI_READQ_REG, &uiStatus, sizeof(uiStatus));
			if (bytes < 0) {
				uiStatus = bytes;
				BCM_DEBUG_PRINT(Adapter, DBG_TYPE_PRINTK, 0, 0, "Reading status of FLASH_SPI_READQ_REG fails");
				return uiStatus;
			}
			iRetries++;
			
			
			
			msleep(10);
		}while((uiStatus & 0x1) && (iRetries < 400));

		if(uiStatus & 0x1)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"iRetries crossing the limit of 80000\n");
			return STATUS_FAILURE;
		}

		addr += Adapter->uiSectorSize;
	}
	return 0;
}
//		uiOffset   - Offset of the flash where data needs to be written to.
//		pData	- Address of Data to be written.

static INT flashByteWrite(
	PMINI_ADAPTER Adapter,
	UINT uiOffset,
	PVOID pData)
{

	UINT uiStatus = 0;
	INT  iRetries = MAX_FLASH_RETRIES * FLASH_PER_RETRIES_DELAY; 

	UINT value;
	ULONG ulData = *(PUCHAR)pData;
	int bytes;


	if(0xFF == ulData)
	{
		return STATUS_SUCCESS;
	}

	value = (FLASH_CMD_WRITE_ENABLE << 24);
	if(wrmalt(Adapter, FLASH_SPI_CMDQ_REG,&value, sizeof(value)) < 0)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Write enable in FLASH_SPI_CMDQ_REG register fails");
		return STATUS_FAILURE;
	}
	if(wrm(Adapter,FLASH_SPI_WRITEQ_REG, (PCHAR)&ulData, 4) < 0 )
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"DATA Write on FLASH_SPI_WRITEQ_REG fails");
		return STATUS_FAILURE;
	}
	value = (0x02000000 | (uiOffset & 0xFFFFFF));
	if(wrmalt(Adapter, FLASH_SPI_CMDQ_REG,&value, sizeof(value)) < 0 )
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Programming of FLASH_SPI_CMDQ_REG fails");
		return STATUS_FAILURE;
	}

	

	do
	{
		value = (FLASH_CMD_STATUS_REG_READ << 24);
	  	if(wrmalt(Adapter, FLASH_SPI_CMDQ_REG, &value, sizeof(value)) < 0)
	  	{
	  		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Programing of FLASH_SPI_CMDQ_REG fails");
			return STATUS_FAILURE;
	  	}
	  	
		bytes = rdmalt(Adapter, FLASH_SPI_READQ_REG, &uiStatus, sizeof(uiStatus));
		if (bytes < 0) {
			uiStatus = bytes;
			BCM_DEBUG_PRINT(Adapter, DBG_TYPE_PRINTK, 0, 0, "Reading status of FLASH_SPI_READQ_REG fails");
			return uiStatus;
		}
	  	iRetries--;
		if( iRetries && ((iRetries % FLASH_PER_RETRIES_DELAY) == 0))
			 msleep(1);

	}while((uiStatus & 0x1) && (iRetries  >0) );

	if(uiStatus & 0x1)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Flash Write fails even after checking status for 200 times.");
		return STATUS_FAILURE ;
	}

	return STATUS_SUCCESS;
}



//		uiOffset   - Offset of the flash where data needs to be written to.
//		pData	- Address of Data to be written.

static INT flashWrite(
	PMINI_ADAPTER Adapter,
	UINT uiOffset,
	PVOID pData)

{
	
	
	

	UINT uiStatus = 0;
	INT  iRetries = MAX_FLASH_RETRIES * FLASH_PER_RETRIES_DELAY; 

	UINT value;
	UINT uiErasePattern[4] = {0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF};
	int bytes;
	if (!memcmp(pData, uiErasePattern, MAX_RW_SIZE))
	{
		return 0;
	}

	value = (FLASH_CMD_WRITE_ENABLE << 24);

	if(wrmalt(Adapter, FLASH_SPI_CMDQ_REG,&value, sizeof(value)) < 0 )
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Write Enable of FLASH_SPI_CMDQ_REG fails");
		return STATUS_FAILURE;
	}
	if(wrm(Adapter, uiOffset, (PCHAR)pData, MAX_RW_SIZE) < 0)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Data write fails...");
		return STATUS_FAILURE;
	}

	
	do
	{
		value = (FLASH_CMD_STATUS_REG_READ << 24);
	  	if(wrmalt(Adapter, FLASH_SPI_CMDQ_REG, &value, sizeof(value)) < 0)
	  	{
	  		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Programing of FLASH_SPI_CMDQ_REG fails");
			return STATUS_FAILURE;
	  	}
	  	
		bytes = rdmalt(Adapter, FLASH_SPI_READQ_REG, &uiStatus, sizeof(uiStatus));
		if (bytes < 0) {
			uiStatus = bytes;
			BCM_DEBUG_PRINT(Adapter, DBG_TYPE_PRINTK, 0, 0, "Reading status of FLASH_SPI_READQ_REG fails");
			return uiStatus;
		}

		iRetries--;
		
		
		
		
		if(iRetries && ((iRetries % FLASH_PER_RETRIES_DELAY) == 0))
				msleep(1);
	}while((uiStatus & 0x1) && (iRetries > 0));

	if(uiStatus & 0x1)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Flash Write fails even after checking status for 200 times.");
		return STATUS_FAILURE ;
	}

	return STATUS_SUCCESS;
}

//		uiOffset    - Offset of the flash where data needs to be written to.
//		pData	 - Address of the Data to be written.
static INT flashByteWriteStatus(
	PMINI_ADAPTER Adapter,
	UINT uiOffset,
	PVOID pData)
{
	UINT uiStatus = 0;
	INT  iRetries = MAX_FLASH_RETRIES * FLASH_PER_RETRIES_DELAY; 
	ULONG ulData  = *(PUCHAR)pData;
	UINT value;
	int bytes;


	if(0xFF == ulData)
	{
		return STATUS_SUCCESS;
	}

	

	value = (FLASH_CMD_WRITE_ENABLE << 24);
	if(wrmalt(Adapter, FLASH_SPI_CMDQ_REG,&value, sizeof(value)) < 0)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Write enable in FLASH_SPI_CMDQ_REG register fails");
		return STATUS_SUCCESS;
	}
	if(wrm(Adapter,FLASH_SPI_WRITEQ_REG, (PCHAR)&ulData, 4) < 0)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"DATA Write on FLASH_SPI_WRITEQ_REG fails");
		return STATUS_FAILURE;
	}
	value = (0x02000000 | (uiOffset & 0xFFFFFF));
	if(wrmalt(Adapter, FLASH_SPI_CMDQ_REG,&value, sizeof(value)) < 0)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Programming of FLASH_SPI_CMDQ_REG fails");
		return STATUS_FAILURE;
	}

    

	do
	{
		value = (FLASH_CMD_STATUS_REG_READ << 24);
		if(wrmalt(Adapter, FLASH_SPI_CMDQ_REG, &value, sizeof(value)) < 0)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Programing of FLASH_SPI_CMDQ_REG fails");
			return STATUS_FAILURE;
		}
		
		bytes = rdmalt(Adapter, FLASH_SPI_READQ_REG, &uiStatus, sizeof(uiStatus));
		if (bytes < 0) {
			uiStatus = bytes;
			BCM_DEBUG_PRINT(Adapter, DBG_TYPE_PRINTK, 0, 0, "Reading status of FLASH_SPI_READQ_REG fails");
			return uiStatus;
		}

		iRetries--;
		if( iRetries && ((iRetries % FLASH_PER_RETRIES_DELAY) == 0))
				msleep(1);
	}while((uiStatus & 0x1) && (iRetries > 0));

	if(uiStatus & 0x1)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Flash Write fails even after checking status for 200 times.");
		return STATUS_FAILURE ;
	}

	return STATUS_SUCCESS;

}
//		uiOffset    - Offset of the flash where data needs to be written to.
//		pData	 - Address of the Data to be written.

static INT flashWriteStatus(
	PMINI_ADAPTER Adapter,
	UINT uiOffset,
	PVOID pData)
{
	UINT uiStatus = 0;
	INT  iRetries = MAX_FLASH_RETRIES * FLASH_PER_RETRIES_DELAY; 
	
	UINT value;
	UINT uiErasePattern[4] = {0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF};
	int bytes;

	if (!memcmp(pData,uiErasePattern,MAX_RW_SIZE))
	{
		return 0;
	}

	value = (FLASH_CMD_WRITE_ENABLE << 24);
	if(wrmalt(Adapter, FLASH_SPI_CMDQ_REG,&value, sizeof(value)) < 0)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Write Enable of FLASH_SPI_CMDQ_REG fails");
		return STATUS_FAILURE;
	}
	if(wrm(Adapter, uiOffset, (PCHAR)pData, MAX_RW_SIZE) < 0)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Data write fails...");
		return STATUS_FAILURE;
	}
   

	do
	{
		value = (FLASH_CMD_STATUS_REG_READ << 24);
	  	if(wrmalt(Adapter, FLASH_SPI_CMDQ_REG, &value, sizeof(value)) < 0)
	  	{
	  		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Programing of FLASH_SPI_CMDQ_REG fails");
			return STATUS_FAILURE;
	  	}
	  	
		bytes = rdmalt(Adapter, FLASH_SPI_READQ_REG, &uiStatus, sizeof(uiStatus));
		if (bytes < 0) {
			uiStatus = bytes;
			BCM_DEBUG_PRINT(Adapter, DBG_TYPE_PRINTK, 0, 0, "Reading status of FLASH_SPI_READQ_REG fails");
			return uiStatus;
		}
	  	iRetries--;
		
		
		
		
		if(iRetries && ((iRetries % FLASH_PER_RETRIES_DELAY) == 0))
				msleep(1);
	}while((uiStatus & 0x1) && (iRetries >0));

	if(uiStatus & 0x1)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Flash Write fails even after checking status for 200 times.");
		return STATUS_FAILURE ;
	}

	return STATUS_SUCCESS;
}


static VOID BcmRestoreBlockProtectStatus(PMINI_ADAPTER Adapter,ULONG ulWriteStatus)
{
	UINT value;
	value = (FLASH_CMD_WRITE_ENABLE<< 24);
	wrmalt(Adapter, FLASH_SPI_CMDQ_REG, &value, sizeof(value));

	udelay(20);
	value = (FLASH_CMD_STATUS_REG_WRITE<<24)|(ulWriteStatus << 16);
	wrmalt(Adapter, FLASH_SPI_CMDQ_REG, &value, sizeof(value));
	udelay(20);
}
//		uiOffset   - Offset of the flash where data needs to be written to. This should be Sector aligned.
static ULONG BcmFlashUnProtectBlock(PMINI_ADAPTER Adapter,UINT uiOffset, UINT uiLength)
{
	ULONG ulStatus      = 0;
	ULONG ulWriteStatus = 0;
	UINT value;
	uiOffset = uiOffset&0x000FFFFF;

	if(FLASH_PART_SST25VF080B == Adapter->ulFlashID)
	{
	
	
	
		value = (FLASH_CMD_STATUS_REG_READ << 24);
		wrmalt(Adapter, FLASH_SPI_CMDQ_REG, &value, sizeof(value));
		udelay(10);
	
	
	
		rdmalt(Adapter, FLASH_SPI_READQ_REG, (PUINT)&ulStatus, sizeof(ulStatus));
		ulStatus >>= 24;
		ulWriteStatus = ulStatus;

	
	
	
	
	
	

		if(ulStatus)
		{
			if((uiOffset+uiLength) <= 0x80000)
			{
			
			
			
			
				ulWriteStatus |= (0x4<<2);
				ulWriteStatus &= ~(0x3<<2);
			}
			else if((uiOffset+uiLength) <= 0xC0000)
			{
			
			
			
			
				ulWriteStatus |= (0x3<<2);
				ulWriteStatus &= ~(0x1<<4);
			}
			else if((uiOffset+uiLength) <= 0xE0000)
		    {
		    
		    
		    
		    
		    	ulWriteStatus |= (0x1<<3);
		    	ulWriteStatus &= ~(0x5<<2);

		    }
		    else if((uiOffset+uiLength) <= 0xF0000)
		    {
		    
		    
		    
		    
		    	ulWriteStatus |= (0x1<<2);
		    	ulWriteStatus &= ~(0x3<<3);
		    }
		    else
		    {
		    
		    
		    
		    
		    	ulWriteStatus &= ~(0x7<<2);
		    }

			value = (FLASH_CMD_WRITE_ENABLE<< 24);
			wrmalt(Adapter, FLASH_SPI_CMDQ_REG, &value, sizeof(value));
			udelay(20);
			value = (FLASH_CMD_STATUS_REG_WRITE<<24)|(ulWriteStatus << 16);
			wrmalt(Adapter, FLASH_SPI_CMDQ_REG, &value, sizeof(value));
			udelay(20);

		}

	}
	return ulStatus;
}
//		pBuffer 	- Data to be written.
//		uiOffset   - Offset of the flash where data needs to be written to.
//		uiNumBytes - Number of bytes to be written.

static INT BeceemFlashBulkWrite(
	PMINI_ADAPTER Adapter,
	PUINT pBuffer,
	UINT uiOffset,
	UINT uiNumBytes,
	BOOLEAN bVerify)
{
	PCHAR  pTempBuff 			= NULL;
	PUCHAR pcBuffer             = (PUCHAR)pBuffer;
	UINT  uiIndex				= 0;
	UINT  uiOffsetFromSectStart = 0;
	UINT  uiSectAlignAddr		= 0;
	UINT  uiCurrSectOffsetAddr	= 0;
	UINT  uiSectBoundary		= 0;
	UINT  uiNumSectTobeRead 	= 0;
	UCHAR ucReadBk[16]       	= {0};
	ULONG ulStatus              = 0;
	INT Status 					= STATUS_SUCCESS;
	UINT uiTemp 				= 0;
	UINT index 					= 0;
	UINT uiPartOffset 			= 0;

#if defined(BCM_SHM_INTERFACE) && !defined(FLASH_DIRECT_ACCESS)
  Status = bcmflash_raw_write((uiOffset/FLASH_PART_SIZE),(uiOffset % FLASH_PART_SIZE),( unsigned char *)pBuffer,uiNumBytes);
  return Status;
#endif

	uiOffsetFromSectStart 	= uiOffset & ~(Adapter->uiSectorSize - 1);

	

	uiSectAlignAddr   		= uiOffset & ~(Adapter->uiSectorSize - 1);
	uiCurrSectOffsetAddr	= uiOffset & (Adapter->uiSectorSize - 1);
	uiSectBoundary	  		= uiSectAlignAddr + Adapter->uiSectorSize;

	pTempBuff = kmalloc(Adapter->uiSectorSize, GFP_KERNEL);
	if(NULL == pTempBuff)
		goto BeceemFlashBulkWrite_EXIT;
// check if the data to be written is overlapped across sectors
	if(uiOffset+uiNumBytes < uiSectBoundary)
	{
		uiNumSectTobeRead = 1;
	}
	else
	{
		
		uiNumSectTobeRead =  (uiCurrSectOffsetAddr+uiNumBytes)/Adapter->uiSectorSize;
		if((uiCurrSectOffsetAddr+uiNumBytes)%Adapter->uiSectorSize)
		{
			uiNumSectTobeRead++;
		}
	}
	
	

	if(IsFlash2x(Adapter) && (Adapter->bAllDSDWriteAllow == FALSE))
	{
		index = 0;
		uiTemp = uiNumSectTobeRead ;
		while(uiTemp)
		{
			 if(IsOffsetWritable(Adapter, uiOffsetFromSectStart + index * Adapter->uiSectorSize ) == FALSE)
			 {
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Sector Starting at offset <0X%X> is not writable",
											(uiOffsetFromSectStart + index * Adapter->uiSectorSize));
				Status = SECTOR_IS_NOT_WRITABLE;
				goto BeceemFlashBulkWrite_EXIT;
			 }
			 uiTemp = uiTemp - 1;
			 index = index + 1 ;
		}
	}
	Adapter->SelectedChip = RESET_CHIP_SELECT;
	while(uiNumSectTobeRead)
	{
		
		
		uiPartOffset = (uiSectAlignAddr & (FLASH_PART_SIZE - 1)) + GetFlashBaseAddr(Adapter);

		BcmDoChipSelect(Adapter,uiSectAlignAddr);

		if(0 != BeceemFlashBulkRead(Adapter,
						(PUINT)pTempBuff,
						uiOffsetFromSectStart,
						Adapter->uiSectorSize))
		{
			Status = -1;
			goto BeceemFlashBulkWrite_EXIT;
		}

		
		

		ulStatus = BcmFlashUnProtectBlock(Adapter,uiSectAlignAddr,Adapter->uiSectorSize);


		if(uiNumSectTobeRead > 1)
		{

			memcpy(&pTempBuff[uiCurrSectOffsetAddr],pcBuffer,uiSectBoundary-(uiSectAlignAddr+uiCurrSectOffsetAddr));
			pcBuffer += ((uiSectBoundary-(uiSectAlignAddr+uiCurrSectOffsetAddr)));
			uiNumBytes -= (uiSectBoundary-(uiSectAlignAddr+uiCurrSectOffsetAddr));
		}
		else
		{
				memcpy(&pTempBuff[uiCurrSectOffsetAddr],pcBuffer,uiNumBytes);
		}

		if(IsFlash2x(Adapter))
		{
			SaveHeaderIfPresent(Adapter,(PUCHAR)pTempBuff,uiOffsetFromSectStart);
		}

		FlashSectorErase(Adapter,uiPartOffset,1);
		
		

		for(uiIndex = 0; uiIndex < Adapter->uiSectorSize; uiIndex +=Adapter->ulFlashWriteSize)
		{
			if(Adapter->device_removed)
			{
				Status = -1;
				goto BeceemFlashBulkWrite_EXIT;
			}
			if(STATUS_SUCCESS != (*Adapter->fpFlashWrite)(Adapter,uiPartOffset+uiIndex,(&pTempBuff[uiIndex])))
			{
				Status = -1;
				goto BeceemFlashBulkWrite_EXIT;
			}
		}

		
		
		for(uiIndex = 0;uiIndex < Adapter->uiSectorSize;uiIndex += MAX_RW_SIZE)
		{
			if(STATUS_SUCCESS == BeceemFlashBulkRead(Adapter,(PUINT)ucReadBk,uiOffsetFromSectStart+uiIndex,MAX_RW_SIZE))
			{
				if(Adapter->ulFlashWriteSize == 1)
				{
					UINT uiReadIndex = 0;
					for(uiReadIndex = 0; uiReadIndex < 16; uiReadIndex++)
					{
						if(ucReadBk[uiReadIndex] != pTempBuff[uiIndex+uiReadIndex])
						{
							if(STATUS_SUCCESS != (*Adapter->fpFlashWriteWithStatusCheck)(Adapter,uiPartOffset+uiIndex+uiReadIndex,&pTempBuff[uiIndex+uiReadIndex]))
							{
								Status = STATUS_FAILURE;
								goto BeceemFlashBulkWrite_EXIT;
							}
						}
					}
				}
				else
				{
					if(memcmp(ucReadBk,&pTempBuff[uiIndex],MAX_RW_SIZE))
					{
						if(STATUS_SUCCESS != (*Adapter->fpFlashWriteWithStatusCheck)(Adapter,uiPartOffset+uiIndex,&pTempBuff[uiIndex]))
						{
							Status = STATUS_FAILURE;
							goto BeceemFlashBulkWrite_EXIT;
						}
					}
				}
			}
		}
		
		


		if(ulStatus)
		{
			BcmRestoreBlockProtectStatus(Adapter,ulStatus);
			ulStatus = 0;
		}

		uiCurrSectOffsetAddr = 0;
		uiSectAlignAddr = uiSectBoundary;
		uiSectBoundary += Adapter->uiSectorSize;
		uiOffsetFromSectStart += Adapter->uiSectorSize;
		uiNumSectTobeRead--;
	}
	
	
	
BeceemFlashBulkWrite_EXIT:
	if(ulStatus)
	{
		BcmRestoreBlockProtectStatus(Adapter,ulStatus);
	}
	
	kfree(pTempBuff);

	Adapter->SelectedChip = RESET_CHIP_SELECT;
	return Status;
}


//		pBuffer 	- Data to be written.
//		uiOffset   - Offset of the flash where data needs to be written to.
//		uiNumBytes - Number of bytes to be written.

static INT BeceemFlashBulkWriteStatus(
	PMINI_ADAPTER Adapter,
	PUINT pBuffer,
	UINT uiOffset,
	UINT uiNumBytes,
	BOOLEAN bVerify)
{
	PCHAR  pTempBuff 			= NULL;
	PUCHAR pcBuffer             = (PUCHAR)pBuffer;
	UINT  uiIndex				= 0;
	UINT  uiOffsetFromSectStart = 0;
	UINT  uiSectAlignAddr		= 0;
	UINT  uiCurrSectOffsetAddr	= 0;
	UINT  uiSectBoundary		= 0;
	UINT  uiNumSectTobeRead 	= 0;
	UCHAR ucReadBk[16]			= {0};
	ULONG ulStatus              = 0;
	UINT  Status				= STATUS_SUCCESS;
	UINT uiTemp 				= 0;
	UINT index 					= 0;
	UINT uiPartOffset			= 0;

	uiOffsetFromSectStart 	= uiOffset & ~(Adapter->uiSectorSize - 1);

	
	

	uiSectAlignAddr 		= uiOffset & ~(Adapter->uiSectorSize - 1);
	uiCurrSectOffsetAddr	= uiOffset & (Adapter->uiSectorSize - 1);
	uiSectBoundary			= uiSectAlignAddr + Adapter->uiSectorSize;

	pTempBuff = kmalloc(Adapter->uiSectorSize, GFP_KERNEL);
	if(NULL == pTempBuff)
		goto BeceemFlashBulkWriteStatus_EXIT;

// check if the data to be written is overlapped across sectors
	if(uiOffset+uiNumBytes < uiSectBoundary)
	{
		uiNumSectTobeRead = 1;
	}
	else
	{
		uiNumSectTobeRead =  (uiCurrSectOffsetAddr+uiNumBytes)/Adapter->uiSectorSize;
		if((uiCurrSectOffsetAddr+uiNumBytes)%Adapter->uiSectorSize)
		{
			uiNumSectTobeRead++;
		}
	}

	if(IsFlash2x(Adapter) && (Adapter->bAllDSDWriteAllow == FALSE))
	{
		index = 0;
		uiTemp = uiNumSectTobeRead ;
		while(uiTemp)
		{
			 if(IsOffsetWritable(Adapter,uiOffsetFromSectStart + index * Adapter->uiSectorSize ) == FALSE)
			 {
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Sector Starting at offset <0X%x> is not writable",
											(uiOffsetFromSectStart + index * Adapter->uiSectorSize));
				Status = SECTOR_IS_NOT_WRITABLE;
				goto BeceemFlashBulkWriteStatus_EXIT;
			 }
			 uiTemp = uiTemp - 1;
			 index = index + 1 ;
		}
	}

	Adapter->SelectedChip = RESET_CHIP_SELECT;
	while(uiNumSectTobeRead)
	{
		uiPartOffset = (uiSectAlignAddr & (FLASH_PART_SIZE - 1)) + GetFlashBaseAddr(Adapter);

		BcmDoChipSelect(Adapter,uiSectAlignAddr);
		if(0 != BeceemFlashBulkRead(Adapter,
						(PUINT)pTempBuff,
						uiOffsetFromSectStart,
						Adapter->uiSectorSize))
		{
			Status = -1;
			goto BeceemFlashBulkWriteStatus_EXIT;
		}

		ulStatus = BcmFlashUnProtectBlock(Adapter,uiOffsetFromSectStart,Adapter->uiSectorSize);

		if(uiNumSectTobeRead > 1)
		{

			memcpy(&pTempBuff[uiCurrSectOffsetAddr],pcBuffer,uiSectBoundary-(uiSectAlignAddr+uiCurrSectOffsetAddr));
			pcBuffer += ((uiSectBoundary-(uiSectAlignAddr+uiCurrSectOffsetAddr)));
			uiNumBytes -= (uiSectBoundary-(uiSectAlignAddr+uiCurrSectOffsetAddr));
		}
		else
		{
			memcpy(&pTempBuff[uiCurrSectOffsetAddr],pcBuffer,uiNumBytes);
		}

		if(IsFlash2x(Adapter))
		{
			SaveHeaderIfPresent(Adapter,(PUCHAR)pTempBuff,uiOffsetFromSectStart);
		}

		FlashSectorErase(Adapter,uiPartOffset,1);

		for(uiIndex = 0; uiIndex < Adapter->uiSectorSize; uiIndex +=Adapter->ulFlashWriteSize)

		{
			if(Adapter->device_removed)
			{
				Status = -1;
				goto BeceemFlashBulkWriteStatus_EXIT;
			}

			if(STATUS_SUCCESS != (*Adapter->fpFlashWriteWithStatusCheck)(Adapter,uiPartOffset+uiIndex,&pTempBuff[uiIndex]))
			{
				Status = -1;
				goto BeceemFlashBulkWriteStatus_EXIT;
			}
		}

		if(bVerify)
		{
			for(uiIndex = 0;uiIndex < Adapter->uiSectorSize;uiIndex += MAX_RW_SIZE)
			{

				if(STATUS_SUCCESS == BeceemFlashBulkRead(Adapter,(PUINT)ucReadBk,uiOffsetFromSectStart+uiIndex,MAX_RW_SIZE))
				{
					if(memcmp(ucReadBk,&pTempBuff[uiIndex],MAX_RW_SIZE))
					{
						Status = STATUS_FAILURE;
						goto BeceemFlashBulkWriteStatus_EXIT;
					}

				}

			}
		}

		if(ulStatus)
		{
			BcmRestoreBlockProtectStatus(Adapter,ulStatus);
			ulStatus = 0;
		}

		uiCurrSectOffsetAddr = 0;
		uiSectAlignAddr = uiSectBoundary;
		uiSectBoundary += Adapter->uiSectorSize;
		uiOffsetFromSectStart += Adapter->uiSectorSize;
		uiNumSectTobeRead--;
	}
BeceemFlashBulkWriteStatus_EXIT:
	if(ulStatus)
	{
		BcmRestoreBlockProtectStatus(Adapter,ulStatus);
	}

	kfree(pTempBuff);
	Adapter->SelectedChip = RESET_CHIP_SELECT;
	return Status;

}



INT PropagateCalParamsFromEEPROMToMemory(PMINI_ADAPTER Adapter)
{
	PCHAR pBuff = kmalloc(BUFFER_4K, GFP_KERNEL);
	UINT uiEepromSize = 0;
	UINT uiIndex = 0;
	UINT uiBytesToCopy = 0;
	UINT uiCalStartAddr = EEPROM_CALPARAM_START;
	UINT uiMemoryLoc = EEPROM_CAL_DATA_INTERNAL_LOC;
	UINT value;
	INT Status = 0;
	if(pBuff == NULL)
	{
		return -1;
	}

	if(0 != BeceemEEPROMBulkRead(Adapter,&uiEepromSize,EEPROM_SIZE_OFFSET,4))
	{

		kfree(pBuff);
		return -1;
	}

	uiEepromSize >>= 16;
	if(uiEepromSize > 1024*1024)
	{
		kfree(pBuff);
		return -1;
	}


	uiBytesToCopy = MIN(BUFFER_4K,uiEepromSize);

	while(uiBytesToCopy)
	{
		if(0 != BeceemEEPROMBulkRead(Adapter,(PUINT)pBuff,uiCalStartAddr,uiBytesToCopy))
		{
			Status = -1;
			break;
		}
		wrm(Adapter,uiMemoryLoc,(PCHAR)(((PULONG)pBuff)+uiIndex),uiBytesToCopy);
		uiMemoryLoc += uiBytesToCopy;
		uiEepromSize -= uiBytesToCopy;
		uiCalStartAddr += uiBytesToCopy;
		uiIndex += uiBytesToCopy/4;
		uiBytesToCopy = MIN(BUFFER_4K,uiEepromSize);

	}
	value = 0xbeadbead;
	wrmalt(Adapter, EEPROM_CAL_DATA_INTERNAL_LOC-4,&value, sizeof(value));
	value = 0xbeadbead;
	wrmalt(Adapter, EEPROM_CAL_DATA_INTERNAL_LOC-8,&value, sizeof(value));
	kfree(pBuff);

	return Status;

}


INT PropagateCalParamsFromFlashToMemory(PMINI_ADAPTER Adapter)
{
	PCHAR pBuff, pPtr;
	UINT uiEepromSize = 0;
	UINT uiBytesToCopy = 0;
	
	UINT uiCalStartAddr = EEPROM_CALPARAM_START;
	UINT uiMemoryLoc = EEPROM_CAL_DATA_INTERNAL_LOC;
	UINT value;
	INT Status = 0;
	value = 0xbeadbead;
	wrmalt(Adapter, EEPROM_CAL_DATA_INTERNAL_LOC - 4, &value, sizeof(value));
	value = 0xbeadbead;
	wrmalt(Adapter, EEPROM_CAL_DATA_INTERNAL_LOC - 8, &value, sizeof(value));

	if(0 != BeceemNVMRead(Adapter,&uiEepromSize,EEPROM_SIZE_OFFSET, 4))
	{
		return -1;
	}
	uiEepromSize = ntohl(uiEepromSize);
	uiEepromSize >>= 16;

	uiEepromSize -= EEPROM_CALPARAM_START;

	if(uiEepromSize > 1024*1024)
	{
		return -1;
	}

	pBuff = kmalloc(uiEepromSize, GFP_KERNEL);
	if ( pBuff == NULL )
		return -1;

	if(0 != BeceemNVMRead(Adapter,(PUINT)pBuff,uiCalStartAddr, uiEepromSize))
	{
		kfree(pBuff);
		return -1;
	}

	pPtr = pBuff;

	uiBytesToCopy = MIN(BUFFER_4K,uiEepromSize);

	while(uiBytesToCopy)
	{
		Status = wrm(Adapter,uiMemoryLoc,(PCHAR)pPtr,uiBytesToCopy);
		if(Status)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"wrm failed with status :%d",Status);
			break;
		}

		pPtr += uiBytesToCopy;
		uiEepromSize -= uiBytesToCopy;
		uiMemoryLoc += uiBytesToCopy;
		uiBytesToCopy = MIN(BUFFER_4K,uiEepromSize);
	}

	kfree(pBuff);
	return Status;

}

// Description: Read back the data written and verifies.
//		pBuffer 	    - Data to be written.
//		uiOffset       - Offset of the flash where data needs to be written to.
//		uiNumBytes - Number of bytes to be written.

static INT BeceemEEPROMReadBackandVerify(
	PMINI_ADAPTER Adapter,
	PUINT pBuffer,
	UINT uiOffset,
	UINT uiNumBytes)
{
	UINT uiRdbk  	= 0;
	UINT uiIndex 	= 0;
	UINT uiData  	= 0;
	UINT auiData[4] = {0};

	while(uiNumBytes)
	{
		if(Adapter->device_removed )
		{
			return -1;
		}

		if(uiNumBytes >= MAX_RW_SIZE)
		{
			BeceemEEPROMBulkRead(Adapter,&auiData[0],uiOffset,MAX_RW_SIZE);

			if(memcmp(&pBuffer[uiIndex],&auiData[0],MAX_RW_SIZE))
			{
				
				BeceemEEPROMBulkWrite(Adapter,(PUCHAR)(pBuffer+uiIndex),uiOffset,MAX_RW_SIZE,FALSE);
				mdelay(3);
				BeceemEEPROMBulkRead(Adapter,&auiData[0],uiOffset,MAX_RW_SIZE);

				if(memcmp(&pBuffer[uiIndex],&auiData[0],MAX_RW_SIZE))
				{
					return -1;
				}
			}
			uiOffset += MAX_RW_SIZE;
			uiNumBytes -= MAX_RW_SIZE;
			uiIndex += 4;

		}
		else if(uiNumBytes >= 4)
		{
			BeceemEEPROMBulkRead(Adapter,&uiData,uiOffset,4);
			if(uiData != pBuffer[uiIndex])
			{
				
				BeceemEEPROMBulkWrite(Adapter,(PUCHAR)(pBuffer+uiIndex),uiOffset,4,FALSE);
				mdelay(3);
				BeceemEEPROMBulkRead(Adapter,&uiData,uiOffset,4);
				if(uiData != pBuffer[uiIndex])
				{
					return -1;
				}
			}
			uiOffset += 4;
			uiNumBytes -= 4;
			uiIndex++;

		}
		else
		{ 
			uiData = 0;
			memcpy(&uiData,((PUCHAR)pBuffer)+(uiIndex*sizeof(UINT)),uiNumBytes);
			BeceemEEPROMBulkRead(Adapter,&uiRdbk,uiOffset,4);

			if(memcmp(&uiData, &uiRdbk, uiNumBytes))
				return -1;

			uiNumBytes = 0;
		}

	}

	return 0;
}

static VOID BcmSwapWord(UINT *ptr1) {

	UINT  tempval = (UINT)*ptr1;
	char *ptr2 = (char *)&tempval;
	char *ptr = (char *)ptr1;

	ptr[0] = ptr2[3];
	ptr[1] = ptr2[2];
	ptr[2] = ptr2[1];
	ptr[3] = ptr2[0];
}

//		uiData 	  	  - Data to be written.
//		uiOffset      - Offset of the EEPROM where data needs to be written to.
static INT BeceemEEPROMWritePage( PMINI_ADAPTER Adapter, UINT uiData[], UINT uiOffset )
{
	UINT uiRetries = MAX_EEPROM_RETRIES*RETRIES_PER_DELAY;
	UINT uiStatus = 0;
	UCHAR uiEpromStatus = 0;
	UINT value =0 ;

	
	value = ( EEPROM_WRITE_QUEUE_FLUSH | EEPROM_CMD_QUEUE_FLUSH | EEPROM_READ_QUEUE_FLUSH );
	wrmalt( Adapter, SPI_FLUSH_REG, &value, sizeof(value));
	value = 0 ;
	wrmalt( Adapter, SPI_FLUSH_REG, &value, sizeof(value) );

	value = ( EEPROM_WRITE_QUEUE_EMPTY | EEPROM_WRITE_QUEUE_AVAIL | EEPROM_WRITE_QUEUE_FULL | EEPROM_READ_DATA_AVAIL | EEPROM_READ_DATA_FULL ) ;
	wrmalt( Adapter, EEPROM_SPI_Q_STATUS1_REG,&value, sizeof(value));

	
	value = EEPROM_WRITE_ENABLE ;
	wrmalt( Adapter, EEPROM_CMDQ_SPI_REG,&value, sizeof(value) );


	value = uiData[0];
	BcmSwapWord(&value);
	wrm( Adapter, EEPROM_WRITE_DATAQ_REG, (PUCHAR)&value, 4);

	value = uiData[1];
	BcmSwapWord(&value);
	wrm( Adapter, EEPROM_WRITE_DATAQ_REG, (PUCHAR)&value, 4);

	value = uiData[2];
	BcmSwapWord(&value);
	wrm( Adapter, EEPROM_WRITE_DATAQ_REG, (PUCHAR)&value, 4);

	value = uiData[3];
	BcmSwapWord(&value);
	wrm( Adapter, EEPROM_WRITE_DATAQ_REG, (PUCHAR)&value, 4);

	value =  EEPROM_16_BYTE_PAGE_WRITE | uiOffset ;
	wrmalt( Adapter, EEPROM_CMDQ_SPI_REG, &value, sizeof(value) );

	uiStatus = 0;
	rdmalt(Adapter, EEPROM_SPI_Q_STATUS1_REG, &uiStatus, sizeof(uiStatus));
	while ( ( uiStatus & EEPROM_WRITE_QUEUE_EMPTY ) == 0 )
	{
		uiRetries--;
		if ( uiRetries == 0 )
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "0x0f003004 = %x, %d retries failed.\n", uiStatus, MAX_EEPROM_RETRIES *RETRIES_PER_DELAY);
			return STATUS_FAILURE ;
		}

		if( !(uiRetries%RETRIES_PER_DELAY) )
					msleep(1);

		uiStatus = 0;
		rdmalt(Adapter, EEPROM_SPI_Q_STATUS1_REG, &uiStatus, sizeof(uiStatus));
		if(Adapter->device_removed == TRUE)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Modem got removed hence exiting from loop....");
			return -ENODEV;
		}

	}

	if ( uiRetries != 0 )
	{
		
		value = ( uiStatus & ( EEPROM_WRITE_QUEUE_EMPTY | EEPROM_WRITE_QUEUE_AVAIL | EEPROM_WRITE_QUEUE_FULL ) );
		wrmalt( Adapter, EEPROM_SPI_Q_STATUS1_REG, &value, sizeof(value));
	}

	uiRetries = MAX_EEPROM_RETRIES*RETRIES_PER_DELAY;
	uiEpromStatus = 0;
	while ( uiRetries != 0 )
	{
		uiEpromStatus = ReadEEPROMStatusRegister( Adapter) ;
		if(Adapter->device_removed == TRUE)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Modem has got removed hence exiting from loop...");
			return -ENODEV;
		}
		if ( ( EEPROM_STATUS_REG_WRITE_BUSY & uiEpromStatus ) == 0 )
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "EEPROM status register = %x tries = %d\n", uiEpromStatus, (MAX_EEPROM_RETRIES * RETRIES_PER_DELAY- uiRetries) );
			return STATUS_SUCCESS ;
		}
		uiRetries--;
		if ( uiRetries == 0 )
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "0x0f003004 = %x, for EEPROM status read %d retries failed.\n", uiEpromStatus, MAX_EEPROM_RETRIES *RETRIES_PER_DELAY);
			return STATUS_FAILURE ;
		}
		uiEpromStatus = 0;
		if( !(uiRetries%RETRIES_PER_DELAY) )
				msleep(1);
	}

	return STATUS_SUCCESS ;
} 


//		pBuffer 	    - Data to be written.
//		uiOffset       - Offset of the EEPROM where data needs to be written to.
//		uiNumBytes - Number of bytes to be written.

INT BeceemEEPROMBulkWrite(
	PMINI_ADAPTER Adapter,
	PUCHAR pBuffer,
	UINT uiOffset,
	UINT uiNumBytes,
	BOOLEAN bVerify)
{
	UINT  uiBytesToCopy = uiNumBytes;
	
	UINT  uiData[4] 	= {0};
	UINT  uiIndex 		= 0;
	UINT  uiTempOffset  = 0;
	UINT  uiExtraBytes  = 0;
	
	

	if(uiOffset%MAX_RW_SIZE && uiBytesToCopy)
	{
		uiTempOffset = uiOffset - (uiOffset%MAX_RW_SIZE);
		uiExtraBytes = uiOffset-uiTempOffset;


		BeceemEEPROMBulkRead(Adapter,&uiData[0],uiTempOffset,MAX_RW_SIZE);

		if(uiBytesToCopy >= (16 -uiExtraBytes))
		{
			memcpy((((PUCHAR)&uiData[0])+uiExtraBytes),pBuffer,MAX_RW_SIZE- uiExtraBytes);

			if ( STATUS_FAILURE == BeceemEEPROMWritePage( Adapter, uiData, uiTempOffset ) )
					return STATUS_FAILURE;

			uiBytesToCopy -= (MAX_RW_SIZE - uiExtraBytes);
			uiIndex += (MAX_RW_SIZE - uiExtraBytes);
			uiOffset += (MAX_RW_SIZE - uiExtraBytes);
		}
		else
		{
			memcpy((((PUCHAR)&uiData[0])+uiExtraBytes),pBuffer,uiBytesToCopy);

			if ( STATUS_FAILURE == BeceemEEPROMWritePage( Adapter, uiData, uiTempOffset ) )
					return STATUS_FAILURE;

			uiIndex += uiBytesToCopy;
			uiOffset += uiBytesToCopy;
			uiBytesToCopy = 0;
		}


	}

	while(uiBytesToCopy)
	{
		if(Adapter->device_removed)
		{
			return -1;
		}

		if(uiBytesToCopy >= MAX_RW_SIZE)
		{

			if (STATUS_FAILURE == BeceemEEPROMWritePage( Adapter, (PUINT) &pBuffer[uiIndex], uiOffset ) )
						return STATUS_FAILURE;

			uiIndex += MAX_RW_SIZE;
			uiOffset += MAX_RW_SIZE;
			uiBytesToCopy	-= MAX_RW_SIZE;
		}
		else
		{
	
	
	
			BeceemEEPROMBulkRead(Adapter,&uiData[0],uiOffset,16);
			memcpy(&uiData[0],pBuffer+uiIndex,uiBytesToCopy);


			if ( STATUS_FAILURE == BeceemEEPROMWritePage( Adapter, uiData, uiOffset ) )
					return STATUS_FAILURE;
			uiBytesToCopy = 0;
		}

	}

	return 0;
}


INT BeceemNVMRead(
	PMINI_ADAPTER Adapter,
	PUINT pBuffer,
	UINT uiOffset,
	UINT uiNumBytes)
{
	INT Status = 0;
#if !defined(BCM_SHM_INTERFACE) || defined(FLASH_DIRECT_ACCESS)
	UINT uiTemp = 0, value;
#endif

	if(Adapter->eNVMType == NVM_FLASH)
	{
		if(Adapter->bFlashRawRead == FALSE)
		{
			if (IsSectionExistInVendorInfo(Adapter,Adapter->eActiveDSD))
				return vendorextnReadSection(Adapter,(PUCHAR)pBuffer,Adapter->eActiveDSD,uiOffset,uiNumBytes);
			uiOffset = uiOffset+ Adapter->ulFlashCalStart ;
		}
#if defined(BCM_SHM_INTERFACE) && !defined(FLASH_DIRECT_ACCESS)
		Status = bcmflash_raw_read((uiOffset/FLASH_PART_SIZE),(uiOffset % FLASH_PART_SIZE),( unsigned char *)pBuffer,uiNumBytes);
#else

		rdmalt(Adapter, 0x0f000C80, &uiTemp, sizeof(uiTemp));
		value = 0;
		wrmalt(Adapter, 0x0f000C80,&value, sizeof(value));
		Status = BeceemFlashBulkRead(Adapter,
						pBuffer,
						uiOffset,
						uiNumBytes);
		wrmalt(Adapter, 0x0f000C80, &uiTemp, sizeof(uiTemp));
#endif
	}
	else if(Adapter->eNVMType == NVM_EEPROM)
	{
		Status = BeceemEEPROMBulkRead(Adapter,
					pBuffer,
					uiOffset,
					uiNumBytes);
	}
	else
	{
		Status = -1;
	}
	return Status;
}

//		pBuffer       - Buffer contains the data to be written.
//		uiOffset       - Offset of NVM where data to be written to.
//		uiNumBytes - Number of bytes to be written..

INT BeceemNVMWrite(
	PMINI_ADAPTER Adapter,
	PUINT pBuffer,
	UINT uiOffset,
	UINT uiNumBytes,
	BOOLEAN bVerify)
{
	INT Status = 0;
	UINT uiTemp = 0;
	UINT uiMemoryLoc = EEPROM_CAL_DATA_INTERNAL_LOC;
	UINT uiIndex = 0;
#if !defined(BCM_SHM_INTERFACE) || defined(FLASH_DIRECT_ACCESS)
	UINT value;
#endif
	UINT uiFlashOffset = 0;

	if(Adapter->eNVMType == NVM_FLASH)
	{
		if (IsSectionExistInVendorInfo(Adapter,Adapter->eActiveDSD))
			Status = vendorextnWriteSection(Adapter,(PUCHAR)pBuffer,Adapter->eActiveDSD,uiOffset,uiNumBytes,bVerify);
		else
		{
			uiFlashOffset = uiOffset + Adapter->ulFlashCalStart;

#if defined(BCM_SHM_INTERFACE) && !defined(FLASH_DIRECT_ACCESS)
			Status = bcmflash_raw_write((uiFlashOffset/FLASH_PART_SIZE), (uiFlashOffset % FLASH_PART_SIZE), (unsigned char *)pBuffer,uiNumBytes);
#else
			rdmalt(Adapter, 0x0f000C80, &uiTemp, sizeof(uiTemp));
			value = 0;
			wrmalt(Adapter, 0x0f000C80, &value, sizeof(value));

			if(Adapter->bStatusWrite == TRUE)
			{
				Status = BeceemFlashBulkWriteStatus(Adapter,
							pBuffer,
							uiFlashOffset,
							uiNumBytes ,
							bVerify);
			}
			else
			{

				Status = BeceemFlashBulkWrite(Adapter,
							pBuffer,
							uiFlashOffset,
							uiNumBytes,
							bVerify);
			}
#endif
		}


		if(uiOffset >= EEPROM_CALPARAM_START)
		{
			uiMemoryLoc += (uiOffset - EEPROM_CALPARAM_START);
			while(uiNumBytes)
			{
				if(uiNumBytes > BUFFER_4K)
				{
					wrm(Adapter,(uiMemoryLoc+uiIndex),(PCHAR)(pBuffer+(uiIndex/4)),BUFFER_4K);
					uiNumBytes -= BUFFER_4K;
					uiIndex += BUFFER_4K;
				}
				else
				{
					wrm(Adapter,uiMemoryLoc+uiIndex,(PCHAR)(pBuffer+(uiIndex/4)),uiNumBytes);
					uiNumBytes = 0;
					break;
				}
			}
		}
		else
		{
			if((uiOffset+uiNumBytes) > EEPROM_CALPARAM_START)
			{
				ULONG ulBytesTobeSkipped = 0;
				PUCHAR pcBuffer = (PUCHAR)pBuffer;
				uiNumBytes -= (EEPROM_CALPARAM_START - uiOffset);
				ulBytesTobeSkipped += (EEPROM_CALPARAM_START - uiOffset);
				uiOffset += (EEPROM_CALPARAM_START - uiOffset);
				while(uiNumBytes)
				{
					if(uiNumBytes > BUFFER_4K)
					{
						wrm(Adapter,uiMemoryLoc+uiIndex,(PCHAR )&pcBuffer[ulBytesTobeSkipped+uiIndex],BUFFER_4K);
						uiNumBytes -= BUFFER_4K;
						uiIndex += BUFFER_4K;
					}
					else
					{
						wrm(Adapter,uiMemoryLoc+uiIndex,(PCHAR)&pcBuffer[ulBytesTobeSkipped+uiIndex],uiNumBytes);
						uiNumBytes = 0;
						break;
					}
				}

			}
		}

	
		wrmalt(Adapter,0x0f000C80,&uiTemp, sizeof(uiTemp));
	}
	else if(Adapter->eNVMType == NVM_EEPROM)
	{
		Status = BeceemEEPROMBulkWrite(Adapter,
					(PUCHAR)pBuffer,
					uiOffset,
					uiNumBytes,
					bVerify);
		if(bVerify)
		{
			Status = BeceemEEPROMReadBackandVerify(Adapter,(PUINT)pBuffer,uiOffset,uiNumBytes);
		}
	}
	else
	{
		Status = -1;
	}
	return Status;
}


INT BcmUpdateSectorSize(PMINI_ADAPTER Adapter,UINT uiSectorSize)
{
	INT Status = -1;
	FLASH_CS_INFO sFlashCsInfo = {0};
	UINT uiTemp = 0;

	UINT uiSectorSig = 0;
	UINT uiCurrentSectorSize = 0;

	UINT value;



	rdmalt(Adapter, 0x0f000C80, &uiTemp, sizeof(uiTemp));
	value = 0;
	wrmalt(Adapter, 0x0f000C80,&value, sizeof(value));

	BeceemFlashBulkRead(Adapter,(PUINT)&sFlashCsInfo,Adapter->ulFlashControlSectionStart,sizeof(sFlashCsInfo));
	uiSectorSig = ntohl(sFlashCsInfo.FlashSectorSizeSig);
	uiCurrentSectorSize = ntohl(sFlashCsInfo.FlashSectorSize);

	if(uiSectorSig == FLASH_SECTOR_SIZE_SIG)
	{

		if((uiCurrentSectorSize <= MAX_SECTOR_SIZE) && (uiCurrentSectorSize >= MIN_SECTOR_SIZE))
		{
			if(uiSectorSize == uiCurrentSectorSize)
			{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Provided sector size is same as programmed in Flash");
				Status = STATUS_SUCCESS;
				goto Restore ;
			}
		}
	}

	if((uiSectorSize <= MAX_SECTOR_SIZE) && (uiSectorSize >= MIN_SECTOR_SIZE))
	{

		sFlashCsInfo.FlashSectorSize = htonl(uiSectorSize);
		sFlashCsInfo.FlashSectorSizeSig = htonl(FLASH_SECTOR_SIZE_SIG);

		Status = BeceemFlashBulkWrite(Adapter,
					(PUINT)&sFlashCsInfo,
					Adapter->ulFlashControlSectionStart,
					sizeof(sFlashCsInfo),
					TRUE);


	}

	Restore :
	
	wrmalt(Adapter, 0x0f000C80,&uiTemp, sizeof(uiTemp));


	return Status;

}


static UINT BcmGetFlashSectorSize(PMINI_ADAPTER Adapter, UINT FlashSectorSizeSig, UINT FlashSectorSize)
{
	UINT uiSectorSize = 0;
	UINT uiSectorSig = 0;

	if(Adapter->bSectorSizeOverride &&
		(Adapter->uiSectorSizeInCFG <= MAX_SECTOR_SIZE &&
		Adapter->uiSectorSizeInCFG >= MIN_SECTOR_SIZE))
	{
		Adapter->uiSectorSize = Adapter->uiSectorSizeInCFG;
	}
	else
	{

		uiSectorSig = FlashSectorSizeSig;

		if(uiSectorSig == FLASH_SECTOR_SIZE_SIG)
		{
			uiSectorSize = FlashSectorSize;
	
	
	
			if(uiSectorSize <= MAX_SECTOR_SIZE && uiSectorSize >= MIN_SECTOR_SIZE)
			{
				Adapter->uiSectorSize = uiSectorSize;
			}
	
			else if(Adapter->uiSectorSizeInCFG <= MAX_SECTOR_SIZE &&
					Adapter->uiSectorSizeInCFG >= MIN_SECTOR_SIZE)
			{
				Adapter->uiSectorSize = Adapter->uiSectorSizeInCFG;
			}
	
			else
			{
				Adapter->uiSectorSize = DEFAULT_SECTOR_SIZE;
			}

		}
		else
		{
			if(Adapter->uiSectorSizeInCFG <= MAX_SECTOR_SIZE &&
					Adapter->uiSectorSizeInCFG >= MIN_SECTOR_SIZE)
			{
				Adapter->uiSectorSize = Adapter->uiSectorSizeInCFG;
			}
			else
			{
				Adapter->uiSectorSize = DEFAULT_SECTOR_SIZE;
			}
		}
	}

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "Sector size  :%x \n", Adapter->uiSectorSize);
	return Adapter->uiSectorSize;
}


static INT BcmInitEEPROMQueues(PMINI_ADAPTER Adapter)
{
	UINT value = 0;
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Fixing reset value on 0x0f003004 register\n" );
	value = EEPROM_READ_DATA_AVAIL;
	wrmalt( Adapter, EEPROM_SPI_Q_STATUS1_REG, &value, sizeof(value));

	
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, " Flushing the queues\n");
	value =EEPROM_ALL_QUEUE_FLUSH ;
	wrmalt( Adapter, SPI_FLUSH_REG, &value, sizeof(value));

	value = 0;
	wrmalt( Adapter, SPI_FLUSH_REG, &value, sizeof(value) );

	
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "EEPROM Status register value = %x\n", ReadEEPROMStatusRegister(Adapter) );

	return STATUS_SUCCESS;
} 


INT BcmInitNVM(PMINI_ADAPTER ps_adapter)
{
	BcmValidateNvmType(ps_adapter);
	BcmInitEEPROMQueues(ps_adapter);

	if(ps_adapter->eNVMType == NVM_AUTODETECT)
	{
		ps_adapter->eNVMType = BcmGetNvmType(ps_adapter);
		if(ps_adapter->eNVMType == NVM_UNKNOWN)
		{
			BCM_DEBUG_PRINT(ps_adapter,DBG_TYPE_PRINTK, 0, 0, "NVM Type is unknown!!\n");
		}
	}
	else if(ps_adapter->eNVMType == NVM_FLASH)
	{
		BcmGetFlashCSInfo(ps_adapter);
	}

	BcmGetNvmSize(ps_adapter);

	return STATUS_SUCCESS;
}

static INT BcmGetNvmSize(PMINI_ADAPTER Adapter)
{
	if(Adapter->eNVMType == NVM_EEPROM)
	{
		Adapter->uiNVMDSDSize = BcmGetEEPROMSize(Adapter);
	}
	else if(Adapter->eNVMType == NVM_FLASH)
	{
		Adapter->uiNVMDSDSize = BcmGetFlashSize(Adapter);
	}
	return 0;
}

static VOID BcmValidateNvmType(PMINI_ADAPTER Adapter)
{

	
	
	
	
	

	if(Adapter->eNVMType == NVM_FLASH &&
		Adapter->chip_id < 0xBECE3300)
	{
		Adapter->eNVMType = NVM_AUTODETECT;
	}
}
static ULONG BcmReadFlashRDID(PMINI_ADAPTER Adapter)
{
	ULONG ulRDID = 0;
	UINT value;
	value = (FLASH_CMD_READ_ID<<24);
	wrmalt(Adapter, FLASH_SPI_CMDQ_REG,&value, sizeof(value));

	udelay(10);
	rdmalt(Adapter, FLASH_SPI_READQ_REG, (PUINT)&ulRDID, sizeof(ulRDID));

	return (ulRDID >>8);


}

INT BcmAllocFlashCSStructure(PMINI_ADAPTER psAdapter)
{
	if(psAdapter == NULL)
	{
		BCM_DEBUG_PRINT(psAdapter,DBG_TYPE_PRINTK, 0, 0, "Adapter structure point is NULL");
		return -EINVAL;
	}
	psAdapter->psFlashCSInfo = (PFLASH_CS_INFO)kzalloc(sizeof(FLASH_CS_INFO), GFP_KERNEL);
	if(psAdapter->psFlashCSInfo == NULL)
	{
		BCM_DEBUG_PRINT(psAdapter,DBG_TYPE_PRINTK, 0, 0,"Can't Allocate memory for Flash 1.x");
		return -ENOMEM;
	}

	psAdapter->psFlash2xCSInfo = (PFLASH2X_CS_INFO)kzalloc(sizeof(FLASH2X_CS_INFO), GFP_KERNEL);
	if(psAdapter->psFlash2xCSInfo == NULL)
	{
		BCM_DEBUG_PRINT(psAdapter,DBG_TYPE_PRINTK, 0, 0,"Can't Allocate memory for Flash 2.x");
		kfree(psAdapter->psFlashCSInfo);
		return -ENOMEM;
	}

	psAdapter->psFlash2xVendorInfo = (PFLASH2X_VENDORSPECIFIC_INFO)kzalloc(sizeof(FLASH2X_VENDORSPECIFIC_INFO), GFP_KERNEL);
	if(psAdapter->psFlash2xVendorInfo == NULL)
	{
		BCM_DEBUG_PRINT(psAdapter,DBG_TYPE_PRINTK, 0, 0,"Can't Allocate Vendor Info Memory for Flash 2.x");
		kfree(psAdapter->psFlashCSInfo);
		kfree(psAdapter->psFlash2xCSInfo);
		return -ENOMEM;
	}

	return STATUS_SUCCESS;
}

INT BcmDeAllocFlashCSStructure(PMINI_ADAPTER psAdapter)
{
	if(psAdapter == NULL)
	{
		BCM_DEBUG_PRINT(psAdapter,DBG_TYPE_PRINTK, 0, 0," Adapter structure point is NULL");
		return -EINVAL;
	}
	kfree(psAdapter->psFlashCSInfo);
	kfree(psAdapter->psFlash2xCSInfo);
	kfree(psAdapter->psFlash2xVendorInfo);
	return STATUS_SUCCESS ;
}

static INT	BcmDumpFlash2XCSStructure(PFLASH2X_CS_INFO psFlash2xCSInfo,PMINI_ADAPTER Adapter)
{
	UINT Index = 0;
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "**********************FLASH2X CS Structure *******************");
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "Signature is  :%x", (psFlash2xCSInfo->MagicNumber));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "Flash Major Version :%d", MAJOR_VERSION(psFlash2xCSInfo->FlashLayoutVersion));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "Flash Minor Version :%d", MINOR_VERSION(psFlash2xCSInfo->FlashLayoutVersion));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, " ISOImageMajorVersion:0x%x", (psFlash2xCSInfo->ISOImageVersion));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "SCSIFirmwareMajorVersion :0x%x", (psFlash2xCSInfo->SCSIFirmwareVersion));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetFromZeroForPart1ISOImage :0x%x", (psFlash2xCSInfo->OffsetFromZeroForPart1ISOImage));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetFromZeroForScsiFirmware :0x%x", (psFlash2xCSInfo->OffsetFromZeroForScsiFirmware));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "SizeOfScsiFirmware  :0x%x", (psFlash2xCSInfo->SizeOfScsiFirmware ));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetFromZeroForPart2ISOImage :0x%x", (psFlash2xCSInfo->OffsetFromZeroForPart2ISOImage));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetFromZeroForDSDStart :0x%x", (psFlash2xCSInfo->OffsetFromZeroForDSDStart));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetFromZeroForDSDEnd :0x%x", (psFlash2xCSInfo->OffsetFromZeroForDSDEnd));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetFromZeroForVSAStart :0x%x", (psFlash2xCSInfo->OffsetFromZeroForVSAStart));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetFromZeroForVSAEnd :0x%x", (psFlash2xCSInfo->OffsetFromZeroForVSAEnd));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetFromZeroForControlSectionStart :0x%x", (psFlash2xCSInfo->OffsetFromZeroForControlSectionStart));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetFromZeroForControlSectionData :0x%x", (psFlash2xCSInfo->OffsetFromZeroForControlSectionData));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "CDLessInactivityTimeout :0x%x", (psFlash2xCSInfo->CDLessInactivityTimeout));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "NewImageSignature :0x%x", (psFlash2xCSInfo->NewImageSignature));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "FlashSectorSizeSig :0x%x", (psFlash2xCSInfo->FlashSectorSizeSig));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "FlashSectorSize :0x%x", (psFlash2xCSInfo->FlashSectorSize));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "FlashWriteSupportSize :0x%x", (psFlash2xCSInfo->FlashWriteSupportSize));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "TotalFlashSize :0x%X", (psFlash2xCSInfo->TotalFlashSize));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "FlashBaseAddr :0x%x", (psFlash2xCSInfo->FlashBaseAddr));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "FlashPartMaxSize :0x%x", (psFlash2xCSInfo->FlashPartMaxSize));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "IsCDLessDeviceBootSig :0x%x", (psFlash2xCSInfo->IsCDLessDeviceBootSig));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "MassStorageTimeout :0x%x", (psFlash2xCSInfo->MassStorageTimeout));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetISOImage1Part1Start :0x%x", (psFlash2xCSInfo->OffsetISOImage1Part1Start));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetISOImage1Part1End :0x%x", (psFlash2xCSInfo->OffsetISOImage1Part1End));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetISOImage1Part2Start :0x%x", (psFlash2xCSInfo->OffsetISOImage1Part2Start));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetISOImage1Part2End :0x%x", (psFlash2xCSInfo->OffsetISOImage1Part2End));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetISOImage1Part3Start :0x%x", (psFlash2xCSInfo->OffsetISOImage1Part3Start));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetISOImage1Part3End :0x%x", (psFlash2xCSInfo->OffsetISOImage1Part3End));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetISOImage2Part1Start :0x%x", (psFlash2xCSInfo->OffsetISOImage2Part1Start));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetISOImage2Part1End	:0x%x", (psFlash2xCSInfo->OffsetISOImage2Part1End));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetISOImage2Part2Start :0x%x", (psFlash2xCSInfo->OffsetISOImage2Part2Start));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetISOImage2Part2End :0x%x", (psFlash2xCSInfo->OffsetISOImage2Part2End));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetISOImage2Part3Start :0x%x", (psFlash2xCSInfo->OffsetISOImage2Part3Start));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetISOImage2Part3End :0x%x", (psFlash2xCSInfo->OffsetISOImage2Part3End));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetFromDSDStartForDSDHeader :0x%x", (psFlash2xCSInfo->OffsetFromDSDStartForDSDHeader));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetFromZeroForDSD1Start :0x%x", (psFlash2xCSInfo->OffsetFromZeroForDSD1Start));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetFromZeroForDSD1End :0x%x", (psFlash2xCSInfo->OffsetFromZeroForDSD1End));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetFromZeroForDSD2Start :0x%x", (psFlash2xCSInfo->OffsetFromZeroForDSD2Start));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetFromZeroForDSD2End :0x%x", (psFlash2xCSInfo->OffsetFromZeroForDSD2End));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetFromZeroForVSA1Start :0x%x", (psFlash2xCSInfo->OffsetFromZeroForVSA1Start));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetFromZeroForVSA1End :0x%x", (psFlash2xCSInfo->OffsetFromZeroForVSA1End));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetFromZeroForVSA2Start :0x%x", (psFlash2xCSInfo->OffsetFromZeroForVSA2Start));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "OffsetFromZeroForVSA2End :0x%x", (psFlash2xCSInfo->OffsetFromZeroForVSA2End));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "Sector Access Bit Map is Defined as :");
	for(Index =0; Index <(FLASH2X_TOTAL_SIZE/(DEFAULT_SECTOR_SIZE *16)); Index++)
	{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "SectorAccessBitMap[%d] :0x%x", Index,
				(psFlash2xCSInfo->SectorAccessBitMap[Index]));
	}

	return STATUS_SUCCESS;
}


static INT	ConvertEndianOf2XCSStructure(PFLASH2X_CS_INFO psFlash2xCSInfo)
{
	UINT Index = 0;
	psFlash2xCSInfo->MagicNumber = ntohl(psFlash2xCSInfo->MagicNumber);
	psFlash2xCSInfo->FlashLayoutVersion= ntohl(psFlash2xCSInfo->FlashLayoutVersion);
	
	psFlash2xCSInfo->ISOImageVersion = ntohl(psFlash2xCSInfo->ISOImageVersion);
	psFlash2xCSInfo->SCSIFirmwareVersion =ntohl(psFlash2xCSInfo->SCSIFirmwareVersion);
	psFlash2xCSInfo->OffsetFromZeroForPart1ISOImage = ntohl(psFlash2xCSInfo->OffsetFromZeroForPart1ISOImage);
	psFlash2xCSInfo->OffsetFromZeroForScsiFirmware = ntohl(psFlash2xCSInfo->OffsetFromZeroForScsiFirmware);
	psFlash2xCSInfo->SizeOfScsiFirmware = ntohl(psFlash2xCSInfo->SizeOfScsiFirmware );
	psFlash2xCSInfo->OffsetFromZeroForPart2ISOImage = ntohl(psFlash2xCSInfo->OffsetFromZeroForPart2ISOImage);
	psFlash2xCSInfo->OffsetFromZeroForDSDStart = ntohl(psFlash2xCSInfo->OffsetFromZeroForDSDStart);
	psFlash2xCSInfo->OffsetFromZeroForDSDEnd = ntohl(psFlash2xCSInfo->OffsetFromZeroForDSDEnd);
	psFlash2xCSInfo->OffsetFromZeroForVSAStart = ntohl(psFlash2xCSInfo->OffsetFromZeroForVSAStart);
	psFlash2xCSInfo->OffsetFromZeroForVSAEnd = ntohl(psFlash2xCSInfo->OffsetFromZeroForVSAEnd);
	psFlash2xCSInfo->OffsetFromZeroForControlSectionStart = ntohl(psFlash2xCSInfo->OffsetFromZeroForControlSectionStart);
	psFlash2xCSInfo->OffsetFromZeroForControlSectionData = ntohl(psFlash2xCSInfo->OffsetFromZeroForControlSectionData);
	psFlash2xCSInfo->CDLessInactivityTimeout = ntohl(psFlash2xCSInfo->CDLessInactivityTimeout);
	psFlash2xCSInfo->NewImageSignature = ntohl(psFlash2xCSInfo->NewImageSignature);
	psFlash2xCSInfo->FlashSectorSizeSig = ntohl(psFlash2xCSInfo->FlashSectorSizeSig);
	psFlash2xCSInfo->FlashSectorSize = ntohl(psFlash2xCSInfo->FlashSectorSize);
	psFlash2xCSInfo->FlashWriteSupportSize = ntohl(psFlash2xCSInfo->FlashWriteSupportSize);
	psFlash2xCSInfo->TotalFlashSize = ntohl(psFlash2xCSInfo->TotalFlashSize);
	psFlash2xCSInfo->FlashBaseAddr = ntohl(psFlash2xCSInfo->FlashBaseAddr);
	psFlash2xCSInfo->FlashPartMaxSize = ntohl(psFlash2xCSInfo->FlashPartMaxSize);
	psFlash2xCSInfo->IsCDLessDeviceBootSig = ntohl(psFlash2xCSInfo->IsCDLessDeviceBootSig);
	psFlash2xCSInfo->MassStorageTimeout = ntohl(psFlash2xCSInfo->MassStorageTimeout);
	psFlash2xCSInfo->OffsetISOImage1Part1Start = ntohl(psFlash2xCSInfo->OffsetISOImage1Part1Start);
	psFlash2xCSInfo->OffsetISOImage1Part1End = ntohl(psFlash2xCSInfo->OffsetISOImage1Part1End);
	psFlash2xCSInfo->OffsetISOImage1Part2Start = ntohl(psFlash2xCSInfo->OffsetISOImage1Part2Start);
	psFlash2xCSInfo->OffsetISOImage1Part2End = ntohl(psFlash2xCSInfo->OffsetISOImage1Part2End);
	psFlash2xCSInfo->OffsetISOImage1Part3Start = ntohl(psFlash2xCSInfo->OffsetISOImage1Part3Start);
	psFlash2xCSInfo->OffsetISOImage1Part3End = ntohl(psFlash2xCSInfo->OffsetISOImage1Part3End);
	psFlash2xCSInfo->OffsetISOImage2Part1Start = ntohl(psFlash2xCSInfo->OffsetISOImage2Part1Start);
	psFlash2xCSInfo->OffsetISOImage2Part1End = ntohl(psFlash2xCSInfo->OffsetISOImage2Part1End);
	psFlash2xCSInfo->OffsetISOImage2Part2Start = ntohl(psFlash2xCSInfo->OffsetISOImage2Part2Start);
	psFlash2xCSInfo->OffsetISOImage2Part2End = ntohl(psFlash2xCSInfo->OffsetISOImage2Part2End);
	psFlash2xCSInfo->OffsetISOImage2Part3Start = ntohl(psFlash2xCSInfo->OffsetISOImage2Part3Start);
	psFlash2xCSInfo->OffsetISOImage2Part3End = ntohl(psFlash2xCSInfo->OffsetISOImage2Part3End);
	psFlash2xCSInfo->OffsetFromDSDStartForDSDHeader = ntohl(psFlash2xCSInfo->OffsetFromDSDStartForDSDHeader);
	psFlash2xCSInfo->OffsetFromZeroForDSD1Start = ntohl(psFlash2xCSInfo->OffsetFromZeroForDSD1Start);
	psFlash2xCSInfo->OffsetFromZeroForDSD1End = ntohl(psFlash2xCSInfo->OffsetFromZeroForDSD1End);
	psFlash2xCSInfo->OffsetFromZeroForDSD2Start = ntohl(psFlash2xCSInfo->OffsetFromZeroForDSD2Start);
	psFlash2xCSInfo->OffsetFromZeroForDSD2End = ntohl(psFlash2xCSInfo->OffsetFromZeroForDSD2End);
	psFlash2xCSInfo->OffsetFromZeroForVSA1Start = ntohl(psFlash2xCSInfo->OffsetFromZeroForVSA1Start);
	psFlash2xCSInfo->OffsetFromZeroForVSA1End = ntohl(psFlash2xCSInfo->OffsetFromZeroForVSA1End);
	psFlash2xCSInfo->OffsetFromZeroForVSA2Start = ntohl(psFlash2xCSInfo->OffsetFromZeroForVSA2Start);
	psFlash2xCSInfo->OffsetFromZeroForVSA2End = ntohl(psFlash2xCSInfo->OffsetFromZeroForVSA2End);
	for(Index =0; Index <(FLASH2X_TOTAL_SIZE/(DEFAULT_SECTOR_SIZE *16)); Index++)
	{
			psFlash2xCSInfo->SectorAccessBitMap[Index] = ntohl(psFlash2xCSInfo->SectorAccessBitMap[Index]);
	}
	return STATUS_SUCCESS;
}

static INT	ConvertEndianOfCSStructure(PFLASH_CS_INFO psFlashCSInfo)
{
	
	psFlashCSInfo->MagicNumber					 		=ntohl(psFlashCSInfo->MagicNumber);
	psFlashCSInfo->FlashLayoutVersion					=ntohl(psFlashCSInfo->FlashLayoutVersion);
	psFlashCSInfo->ISOImageVersion 						= ntohl(psFlashCSInfo->ISOImageVersion);
	
	psFlashCSInfo->SCSIFirmwareVersion =(psFlashCSInfo->SCSIFirmwareVersion);

	psFlashCSInfo->OffsetFromZeroForPart1ISOImage  		= ntohl(psFlashCSInfo->OffsetFromZeroForPart1ISOImage);
	psFlashCSInfo->OffsetFromZeroForScsiFirmware        = ntohl(psFlashCSInfo->OffsetFromZeroForScsiFirmware);
	psFlashCSInfo->SizeOfScsiFirmware                   = ntohl(psFlashCSInfo->SizeOfScsiFirmware );
	psFlashCSInfo->OffsetFromZeroForPart2ISOImage       = ntohl(psFlashCSInfo->OffsetFromZeroForPart2ISOImage);
	psFlashCSInfo->OffsetFromZeroForCalibrationStart    = ntohl(psFlashCSInfo->OffsetFromZeroForCalibrationStart);
	psFlashCSInfo->OffsetFromZeroForCalibrationEnd      = ntohl(psFlashCSInfo->OffsetFromZeroForCalibrationEnd);
	psFlashCSInfo->OffsetFromZeroForVSAStart            = ntohl(psFlashCSInfo->OffsetFromZeroForVSAStart);
	psFlashCSInfo->OffsetFromZeroForVSAEnd              = ntohl(psFlashCSInfo->OffsetFromZeroForVSAEnd);
	psFlashCSInfo->OffsetFromZeroForControlSectionStart = ntohl(psFlashCSInfo->OffsetFromZeroForControlSectionStart);
	psFlashCSInfo->OffsetFromZeroForControlSectionData  = ntohl(psFlashCSInfo->OffsetFromZeroForControlSectionData);
	psFlashCSInfo->CDLessInactivityTimeout 				= ntohl(psFlashCSInfo->CDLessInactivityTimeout);
	psFlashCSInfo->NewImageSignature                    = ntohl(psFlashCSInfo->NewImageSignature);
	psFlashCSInfo->FlashSectorSizeSig                   = ntohl(psFlashCSInfo->FlashSectorSizeSig);
	psFlashCSInfo->FlashSectorSize                      = ntohl(psFlashCSInfo->FlashSectorSize);
	psFlashCSInfo->FlashWriteSupportSize                = ntohl(psFlashCSInfo->FlashWriteSupportSize);
	psFlashCSInfo->TotalFlashSize        				= ntohl(psFlashCSInfo->TotalFlashSize);
 	psFlashCSInfo->FlashBaseAddr         				= ntohl(psFlashCSInfo->FlashBaseAddr);
	psFlashCSInfo->FlashPartMaxSize      				= ntohl(psFlashCSInfo->FlashPartMaxSize);
 	psFlashCSInfo->IsCDLessDeviceBootSig 				= ntohl(psFlashCSInfo->IsCDLessDeviceBootSig);
	psFlashCSInfo->MassStorageTimeout    				= ntohl(psFlashCSInfo->MassStorageTimeout);

	return STATUS_SUCCESS;
}

static INT IsSectionExistInVendorInfo(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL section)
{
 	return ( Adapter->uiVendorExtnFlag &&
 		(Adapter->psFlash2xVendorInfo->VendorSection[section].AccessFlags & FLASH2X_SECTION_PRESENT) &&
 		(Adapter->psFlash2xVendorInfo->VendorSection[section].OffsetFromZeroForSectionStart != UNINIT_PTR_IN_CS) );
}

static VOID UpdateVendorInfo(PMINI_ADAPTER Adapter)
{
	B_UINT32 i = 0;
	UINT uiSizeSection = 0;

	Adapter->uiVendorExtnFlag = FALSE;

	for(i = 0;i < TOTAL_SECTIONS;i++)
		Adapter->psFlash2xVendorInfo->VendorSection[i].OffsetFromZeroForSectionStart = UNINIT_PTR_IN_CS;

	if(STATUS_SUCCESS != vendorextnGetSectionInfo(Adapter, Adapter->psFlash2xVendorInfo))
		return;

	i = 0;
	while(i < TOTAL_SECTIONS)
	{
		if(!(Adapter->psFlash2xVendorInfo->VendorSection[i].AccessFlags & FLASH2X_SECTION_PRESENT))
		{
			i++;
			continue;
		}

		Adapter->uiVendorExtnFlag = TRUE;
		uiSizeSection = (Adapter->psFlash2xVendorInfo->VendorSection[i].OffsetFromZeroForSectionEnd -
						Adapter->psFlash2xVendorInfo->VendorSection[i].OffsetFromZeroForSectionStart);

		switch(i)
		{
		case DSD0:
			if(( uiSizeSection >= (Adapter->psFlash2xCSInfo->OffsetFromDSDStartForDSDHeader + sizeof(DSD_HEADER))) &&
			(UNINIT_PTR_IN_CS != Adapter->psFlash2xVendorInfo->VendorSection[i].OffsetFromZeroForSectionStart))
				Adapter->psFlash2xCSInfo->OffsetFromZeroForDSDStart = Adapter->psFlash2xCSInfo->OffsetFromZeroForDSDEnd = VENDOR_PTR_IN_CS;
			else
				Adapter->psFlash2xCSInfo->OffsetFromZeroForDSDStart = Adapter->psFlash2xCSInfo->OffsetFromZeroForDSDEnd = UNINIT_PTR_IN_CS;
			break;

		case DSD1:
			if(( uiSizeSection >= (Adapter->psFlash2xCSInfo->OffsetFromDSDStartForDSDHeader + sizeof(DSD_HEADER))) &&
			(UNINIT_PTR_IN_CS != Adapter->psFlash2xVendorInfo->VendorSection[i].OffsetFromZeroForSectionStart))
				Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD1Start = Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD1End = VENDOR_PTR_IN_CS;
			else
				Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD1Start = Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD1End = UNINIT_PTR_IN_CS;
			break;

		case DSD2:
			if(( uiSizeSection >= (Adapter->psFlash2xCSInfo->OffsetFromDSDStartForDSDHeader + sizeof(DSD_HEADER))) &&
			(UNINIT_PTR_IN_CS != Adapter->psFlash2xVendorInfo->VendorSection[i].OffsetFromZeroForSectionStart))
				Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD2Start = Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD2End = VENDOR_PTR_IN_CS;
			else
				Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD2Start = Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD2End = UNINIT_PTR_IN_CS;
			break;
		case VSA0:
			if(UNINIT_PTR_IN_CS != Adapter->psFlash2xVendorInfo->VendorSection[i].OffsetFromZeroForSectionStart)
				Adapter->psFlash2xCSInfo->OffsetFromZeroForVSAStart = Adapter->psFlash2xCSInfo->OffsetFromZeroForVSAEnd = VENDOR_PTR_IN_CS;
			else
				Adapter->psFlash2xCSInfo->OffsetFromZeroForVSAStart = Adapter->psFlash2xCSInfo->OffsetFromZeroForVSAEnd = UNINIT_PTR_IN_CS;
			break;

		case VSA1:
			if(UNINIT_PTR_IN_CS != Adapter->psFlash2xVendorInfo->VendorSection[i].OffsetFromZeroForSectionStart)
				Adapter->psFlash2xCSInfo->OffsetFromZeroForVSA1Start = Adapter->psFlash2xCSInfo->OffsetFromZeroForVSA1End = VENDOR_PTR_IN_CS;
			else
				Adapter->psFlash2xCSInfo->OffsetFromZeroForVSA1Start = Adapter->psFlash2xCSInfo->OffsetFromZeroForVSA1End = UNINIT_PTR_IN_CS;
			break;
		case VSA2:
			if(UNINIT_PTR_IN_CS != Adapter->psFlash2xVendorInfo->VendorSection[i].OffsetFromZeroForSectionStart)
				Adapter->psFlash2xCSInfo->OffsetFromZeroForVSA2Start = Adapter->psFlash2xCSInfo->OffsetFromZeroForVSA2End = VENDOR_PTR_IN_CS;
			else
				Adapter->psFlash2xCSInfo->OffsetFromZeroForVSA2Start = Adapter->psFlash2xCSInfo->OffsetFromZeroForVSA2End = UNINIT_PTR_IN_CS;
			break;

		default:
			break;
		}
		i++;
	}

}


static INT BcmGetFlashCSInfo(PMINI_ADAPTER Adapter)
{
	

#if !defined(BCM_SHM_INTERFACE) || defined(FLASH_DIRECT_ACCESS)
	UINT value;
#endif
	UINT uiFlashLayoutMajorVersion;
	Adapter->uiFlashLayoutMinorVersion = 0;
	Adapter->uiFlashLayoutMajorVersion = 0;
	Adapter->ulFlashControlSectionStart = FLASH_CS_INFO_START_ADDR;


	Adapter->uiFlashBaseAdd = 0;
	Adapter->ulFlashCalStart = 0;
	memset(Adapter->psFlashCSInfo, 0 ,sizeof(FLASH_CS_INFO));
	memset(Adapter->psFlash2xCSInfo, 0 ,sizeof(FLASH2X_CS_INFO));

	if(!Adapter->bDDRInitDone)
	{
		{
			value = FLASH_CONTIGIOUS_START_ADDR_BEFORE_INIT;
			wrmalt(Adapter, 0xAF00A080, &value, sizeof(value));
		}
	}


	
	
	BeceemFlashBulkRead(Adapter,(PUINT)Adapter->psFlashCSInfo,Adapter->ulFlashControlSectionStart,8);

	Adapter->psFlashCSInfo->FlashLayoutVersion =  ntohl(Adapter->psFlashCSInfo->FlashLayoutVersion);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "Flash Layout Version :%X", (Adapter->psFlashCSInfo->FlashLayoutVersion));
	
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "Signature is  :%x\n", ntohl(Adapter->psFlashCSInfo->MagicNumber));

	if(FLASH_CONTROL_STRUCT_SIGNATURE == ntohl(Adapter->psFlashCSInfo->MagicNumber))
	{
		uiFlashLayoutMajorVersion = MAJOR_VERSION((Adapter->psFlashCSInfo->FlashLayoutVersion));
		Adapter->uiFlashLayoutMinorVersion = MINOR_VERSION((Adapter->psFlashCSInfo->FlashLayoutVersion));
	}
	else
	{
		Adapter->uiFlashLayoutMinorVersion = 0;
		uiFlashLayoutMajorVersion = 0;
	}

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"FLASH LAYOUT MAJOR VERSION :%X", uiFlashLayoutMajorVersion);

	if(uiFlashLayoutMajorVersion < FLASH_2X_MAJOR_NUMBER)
	{
		BeceemFlashBulkRead(Adapter,(PUINT)Adapter->psFlashCSInfo,Adapter->ulFlashControlSectionStart,sizeof(FLASH_CS_INFO));
		ConvertEndianOfCSStructure(Adapter->psFlashCSInfo);
		Adapter->ulFlashCalStart = (Adapter->psFlashCSInfo->OffsetFromZeroForCalibrationStart);

		if(!((Adapter->uiFlashLayoutMajorVersion == 1) && (Adapter->uiFlashLayoutMinorVersion == 1)))
		{
			Adapter->ulFlashControlSectionStart = Adapter->psFlashCSInfo->OffsetFromZeroForControlSectionStart;
		}

		if((FLASH_CONTROL_STRUCT_SIGNATURE == (Adapter->psFlashCSInfo->MagicNumber)) &&
		   (SCSI_FIRMWARE_MINOR_VERSION <= MINOR_VERSION(Adapter->psFlashCSInfo->SCSIFirmwareVersion)) &&
		   (FLASH_SECTOR_SIZE_SIG == (Adapter->psFlashCSInfo->FlashSectorSizeSig)) &&
		   (BYTE_WRITE_SUPPORT == (Adapter->psFlashCSInfo->FlashWriteSupportSize)))
		{
			Adapter->ulFlashWriteSize = (Adapter->psFlashCSInfo->FlashWriteSupportSize);
		   	Adapter->fpFlashWrite = flashByteWrite;
		   	Adapter->fpFlashWriteWithStatusCheck = flashByteWriteStatus;
		}
		else
		{
			Adapter->ulFlashWriteSize = MAX_RW_SIZE;
			Adapter->fpFlashWrite = flashWrite;
		   	Adapter->fpFlashWriteWithStatusCheck = flashWriteStatus;
		}

		BcmGetFlashSectorSize(Adapter, (Adapter->psFlashCSInfo->FlashSectorSizeSig),
					 (Adapter->psFlashCSInfo->FlashSectorSize));


		Adapter->uiFlashBaseAdd = Adapter->psFlashCSInfo->FlashBaseAddr & 0xFCFFFFFF;


	}
	else
	{
		if(BcmFlash2xBulkRead(Adapter,(PUINT)Adapter->psFlash2xCSInfo,NO_SECTION_VAL,
				Adapter->ulFlashControlSectionStart,sizeof(FLASH2X_CS_INFO)))
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Unable to read CS structure \n");
			return STATUS_FAILURE;
		}
		ConvertEndianOf2XCSStructure(Adapter->psFlash2xCSInfo);
		BcmDumpFlash2XCSStructure(Adapter->psFlash2xCSInfo,Adapter);
		if((FLASH_CONTROL_STRUCT_SIGNATURE == Adapter->psFlash2xCSInfo->MagicNumber) &&
		   (SCSI_FIRMWARE_MINOR_VERSION <= MINOR_VERSION(Adapter->psFlash2xCSInfo->SCSIFirmwareVersion)) &&
		   (FLASH_SECTOR_SIZE_SIG == Adapter->psFlash2xCSInfo->FlashSectorSizeSig) &&
		   (BYTE_WRITE_SUPPORT == Adapter->psFlash2xCSInfo->FlashWriteSupportSize))
		{
			Adapter->ulFlashWriteSize = Adapter->psFlash2xCSInfo->FlashWriteSupportSize;
		   	Adapter->fpFlashWrite = flashByteWrite;
		   	Adapter->fpFlashWriteWithStatusCheck = flashByteWriteStatus;
		}
		else
		{
			Adapter->ulFlashWriteSize = MAX_RW_SIZE;
			Adapter->fpFlashWrite = flashWrite;
		   	Adapter->fpFlashWriteWithStatusCheck = flashWriteStatus;
		}

		BcmGetFlashSectorSize(Adapter, Adapter->psFlash2xCSInfo->FlashSectorSizeSig,
					Adapter->psFlash2xCSInfo->FlashSectorSize);

		UpdateVendorInfo(Adapter);

		BcmGetActiveDSD(Adapter);
		BcmGetActiveISO(Adapter);
		Adapter->uiFlashBaseAdd = Adapter->psFlash2xCSInfo->FlashBaseAddr & 0xFCFFFFFF;
		Adapter->ulFlashControlSectionStart = Adapter->psFlash2xCSInfo->OffsetFromZeroForControlSectionStart;

	}
	Adapter->ulFlashID = BcmReadFlashRDID(Adapter);

	Adapter->uiFlashLayoutMajorVersion = uiFlashLayoutMajorVersion;


	return STATUS_SUCCESS ;
}



static NVM_TYPE BcmGetNvmType(PMINI_ADAPTER Adapter)
{
	UINT uiData = 0;

	BeceemEEPROMBulkRead(Adapter,&uiData,0x0,4);
	if(uiData == BECM)
	{
		return NVM_EEPROM;
	}
	
	
	
	BcmGetFlashCSInfo(Adapter);

	BeceemFlashBulkRead(Adapter,&uiData,0x0 + Adapter->ulFlashCalStart,4);
	if(uiData == BECM)
	{
		return NVM_FLASH;
	}
	if(BcmGetEEPROMSize(Adapter))
	{
		return NVM_EEPROM;
	}



	return NVM_UNKNOWN;
}


INT BcmGetSectionValStartOffset(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL eFlashSectionVal)
{

	INT SectStartOffset = 0 ;

	SectStartOffset = INVALID_OFFSET ;

	if(IsSectionExistInVendorInfo(Adapter,eFlashSectionVal))
	{
		return Adapter->psFlash2xVendorInfo->VendorSection[eFlashSectionVal].OffsetFromZeroForSectionStart;
	}

	switch(eFlashSectionVal)
	{
		case ISO_IMAGE1 :
			  if((Adapter->psFlash2xCSInfo->OffsetISOImage1Part1Start != UNINIT_PTR_IN_CS) &&
			  	(IsNonCDLessDevice(Adapter) == FALSE))
				  SectStartOffset = (Adapter->psFlash2xCSInfo->OffsetISOImage1Part1Start);
			   break;
		case ISO_IMAGE2 :
				if((Adapter->psFlash2xCSInfo->OffsetISOImage2Part1Start != UNINIT_PTR_IN_CS) &&
					(IsNonCDLessDevice(Adapter) == FALSE))
			  		SectStartOffset = (Adapter->psFlash2xCSInfo->OffsetISOImage2Part1Start);
			  break;
		case DSD0 :
				if(Adapter->psFlash2xCSInfo->OffsetFromZeroForDSDStart != UNINIT_PTR_IN_CS)
					SectStartOffset = (Adapter->psFlash2xCSInfo->OffsetFromZeroForDSDStart);
				break;
		case DSD1 :
				if(Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD1Start != UNINIT_PTR_IN_CS)
					SectStartOffset =  (Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD1Start);
				break;
		case DSD2 :
				if(Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD2Start != UNINIT_PTR_IN_CS)
					SectStartOffset =  (Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD2Start);
				break;
		case VSA0 :
				if(Adapter->psFlash2xCSInfo->OffsetFromZeroForVSAStart != UNINIT_PTR_IN_CS)
					SectStartOffset =  (Adapter->psFlash2xCSInfo->OffsetFromZeroForVSAStart);
				break;
		case VSA1 :
				if(Adapter->psFlash2xCSInfo->OffsetFromZeroForVSA1Start != UNINIT_PTR_IN_CS)
					SectStartOffset =  (Adapter->psFlash2xCSInfo->OffsetFromZeroForVSA1Start);
				break;
		case VSA2 :
				if(Adapter->psFlash2xCSInfo->OffsetFromZeroForVSA2Start != UNINIT_PTR_IN_CS)
					SectStartOffset =  (Adapter->psFlash2xCSInfo->OffsetFromZeroForVSA2Start);
				break;
		case SCSI :
				if(Adapter->psFlash2xCSInfo->OffsetFromZeroForScsiFirmware != UNINIT_PTR_IN_CS)
					SectStartOffset =  (Adapter->psFlash2xCSInfo->OffsetFromZeroForScsiFirmware);
				break;
		case CONTROL_SECTION :
				if(Adapter->psFlash2xCSInfo->OffsetFromZeroForControlSectionStart != UNINIT_PTR_IN_CS)
					SectStartOffset =  (Adapter->psFlash2xCSInfo->OffsetFromZeroForControlSectionStart);
				break;
		case ISO_IMAGE1_PART2 :
				if(Adapter->psFlash2xCSInfo->OffsetISOImage1Part2Start != UNINIT_PTR_IN_CS)
				 	 SectStartOffset =  (Adapter->psFlash2xCSInfo->OffsetISOImage1Part2Start);
				 break;
		case ISO_IMAGE1_PART3 :
				if(Adapter->psFlash2xCSInfo->OffsetISOImage1Part3Start != UNINIT_PTR_IN_CS)
				  SectStartOffset =  (Adapter->psFlash2xCSInfo->OffsetISOImage1Part3Start);
			  	break;
		case ISO_IMAGE2_PART2 :
				if(Adapter->psFlash2xCSInfo->OffsetISOImage2Part2Start != UNINIT_PTR_IN_CS)
			 		 SectStartOffset =  (Adapter->psFlash2xCSInfo->OffsetISOImage2Part2Start);
			    break;
		case ISO_IMAGE2_PART3 :
  			  if(Adapter->psFlash2xCSInfo->OffsetISOImage2Part3Start != UNINIT_PTR_IN_CS)
  				  SectStartOffset =  (Adapter->psFlash2xCSInfo->OffsetISOImage2Part3Start);
  			  break;
		default :
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Section Does not exist in Flash 2.x");
			SectStartOffset =  INVALID_OFFSET;
	}
	return SectStartOffset;
}


INT BcmGetSectionValEndOffset(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL eFlash2xSectionVal)
{
	INT SectEndOffset = 0 ;
	SectEndOffset = INVALID_OFFSET;

	if(IsSectionExistInVendorInfo(Adapter,eFlash2xSectionVal))
	{
		return Adapter->psFlash2xVendorInfo->VendorSection[eFlash2xSectionVal].OffsetFromZeroForSectionEnd;
	}

	switch(eFlash2xSectionVal)
	{
		case ISO_IMAGE1 :
			 if((Adapter->psFlash2xCSInfo->OffsetISOImage1Part1End!= UNINIT_PTR_IN_CS) &&
			 	 (IsNonCDLessDevice(Adapter) == FALSE))
				  SectEndOffset = (Adapter->psFlash2xCSInfo->OffsetISOImage1Part1End);
			   break;
		case ISO_IMAGE2 :
			if((Adapter->psFlash2xCSInfo->OffsetISOImage2Part1End!= UNINIT_PTR_IN_CS) &&
				(IsNonCDLessDevice(Adapter) == FALSE))
					SectEndOffset = (Adapter->psFlash2xCSInfo->OffsetISOImage2Part1End);
			 break;
		case DSD0 :
			if(Adapter->psFlash2xCSInfo->OffsetFromZeroForDSDEnd != UNINIT_PTR_IN_CS)
				SectEndOffset = (Adapter->psFlash2xCSInfo->OffsetFromZeroForDSDEnd);
			break;
		case DSD1 :
			if(Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD1End != UNINIT_PTR_IN_CS)
				SectEndOffset = (Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD1End);
			break;
		case DSD2 :
			if(Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD2End != UNINIT_PTR_IN_CS)
				SectEndOffset = (Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD2End);
			break;
		case VSA0 :
			if(Adapter->psFlash2xCSInfo->OffsetFromZeroForVSAEnd != UNINIT_PTR_IN_CS)
				SectEndOffset = (Adapter->psFlash2xCSInfo->OffsetFromZeroForVSAEnd);
			break;
		case VSA1 :
			if(Adapter->psFlash2xCSInfo->OffsetFromZeroForVSA1End != UNINIT_PTR_IN_CS)
				SectEndOffset = (Adapter->psFlash2xCSInfo->OffsetFromZeroForVSA1End);
			break;
		case VSA2 :
			if(Adapter->psFlash2xCSInfo->OffsetFromZeroForVSA2End != UNINIT_PTR_IN_CS)
				SectEndOffset = (Adapter->psFlash2xCSInfo->OffsetFromZeroForVSA2End);
			break;
		case SCSI :
			if(Adapter->psFlash2xCSInfo->OffsetFromZeroForScsiFirmware != UNINIT_PTR_IN_CS)
				SectEndOffset = ((Adapter->psFlash2xCSInfo->OffsetFromZeroForScsiFirmware) +
					(Adapter->psFlash2xCSInfo->SizeOfScsiFirmware));
			break;
		case CONTROL_SECTION :
				
				SectEndOffset = STATUS_FAILURE;
		case ISO_IMAGE1_PART2 :
				if(Adapter->psFlash2xCSInfo->OffsetISOImage1Part2End!= UNINIT_PTR_IN_CS)
				 	 SectEndOffset =  (Adapter->psFlash2xCSInfo->OffsetISOImage1Part2End);
				 break;
		case ISO_IMAGE1_PART3 :
				if(Adapter->psFlash2xCSInfo->OffsetISOImage1Part3End!= UNINIT_PTR_IN_CS)
				  SectEndOffset =  (Adapter->psFlash2xCSInfo->OffsetISOImage1Part3End);
			  	break;
		case ISO_IMAGE2_PART2 :
				if(Adapter->psFlash2xCSInfo->OffsetISOImage2Part2End != UNINIT_PTR_IN_CS)
			 		 SectEndOffset =  (Adapter->psFlash2xCSInfo->OffsetISOImage2Part2End);
			    break;
		case ISO_IMAGE2_PART3 :
  			  if(Adapter->psFlash2xCSInfo->OffsetISOImage2Part3End!= UNINIT_PTR_IN_CS)
  				  SectEndOffset =  (Adapter->psFlash2xCSInfo->OffsetISOImage2Part3End);
  			  break;

		default :
			SectEndOffset = INVALID_OFFSET;
	}
	return SectEndOffset ;
}


INT BcmFlash2xBulkRead(
	PMINI_ADAPTER Adapter,
	PUINT pBuffer,
	FLASH2X_SECTION_VAL eFlash2xSectionVal,
	UINT uiOffsetWithinSectionVal,
	UINT uiNumBytes)
{

	INT Status = STATUS_SUCCESS;
	INT SectionStartOffset = 0;
	UINT uiAbsoluteOffset = 0 ;
	UINT uiTemp =0, value =0 ;
	if(Adapter == NULL)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Adapter structure is NULL");
		return -EINVAL;
	}
	if(Adapter->device_removed )
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Device has been removed");
		return -ENODEV;
	}

	
	if(eFlash2xSectionVal == NO_SECTION_VAL)
		SectionStartOffset = 0;
	else
		SectionStartOffset = BcmGetSectionValStartOffset(Adapter,eFlash2xSectionVal);

	if(SectionStartOffset == STATUS_FAILURE )
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"This Section<%d> does not exixt in Flash 2.x Map ",eFlash2xSectionVal);
		return -EINVAL;
	}

	if(IsSectionExistInVendorInfo(Adapter,eFlash2xSectionVal))
		return vendorextnReadSection(Adapter,(PUCHAR)pBuffer, eFlash2xSectionVal, uiOffsetWithinSectionVal, uiNumBytes);

	
	uiAbsoluteOffset = uiOffsetWithinSectionVal + SectionStartOffset;
	rdmalt(Adapter, 0x0f000C80, &uiTemp, sizeof(uiTemp));
	value = 0;
	wrmalt(Adapter, 0x0f000C80,&value, sizeof(value));

	Status= BeceemFlashBulkRead(Adapter, pBuffer,uiAbsoluteOffset,uiNumBytes) ;

	wrmalt(Adapter, 0x0f000C80, &uiTemp, sizeof(uiTemp));
	if(Status)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Flash Read Failed with Status :%d", Status);
		return Status ;
	}

	return Status;
}


INT BcmFlash2xBulkWrite(
	PMINI_ADAPTER Adapter,
	PUINT pBuffer,
	FLASH2X_SECTION_VAL eFlash2xSectVal,
	UINT uiOffset,
	UINT uiNumBytes,
	UINT bVerify)
{

	INT Status 	= STATUS_SUCCESS;
	UINT FlashSectValStartOffset = 0;
	UINT uiTemp = 0, value = 0;
	if(Adapter == NULL)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Adapter structure is NULL");
		return -EINVAL;
	}
	if(Adapter->device_removed )
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Device has been removed");
		return -ENODEV;
	}

	
	if(eFlash2xSectVal == NO_SECTION_VAL)
		FlashSectValStartOffset = 0;
	else
		FlashSectValStartOffset = BcmGetSectionValStartOffset(Adapter,eFlash2xSectVal);

	if(FlashSectValStartOffset == STATUS_FAILURE )
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"This Section<%d> does not exixt in Flash Map 2.x",eFlash2xSectVal);
		return -EINVAL;
	}

	if(IsSectionExistInVendorInfo(Adapter,eFlash2xSectVal))
		return vendorextnWriteSection(Adapter, (PUCHAR)pBuffer, eFlash2xSectVal, uiOffset, uiNumBytes, bVerify);

	
	uiOffset = uiOffset + FlashSectValStartOffset;

	rdmalt(Adapter, 0x0f000C80, &uiTemp, sizeof(uiTemp));
	value = 0;
	wrmalt(Adapter, 0x0f000C80,&value, sizeof(value));

	Status = BeceemFlashBulkWrite(Adapter, pBuffer,uiOffset,uiNumBytes,bVerify);

	wrmalt(Adapter, 0x0f000C80, &uiTemp, sizeof(uiTemp));
	if(Status)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Flash Write failed with Status :%d", Status);
		return Status ;
	}

	return Status;

}

static INT BcmGetActiveDSD(PMINI_ADAPTER Adapter)
{
	FLASH2X_SECTION_VAL uiHighestPriDSD = 0 ;

	uiHighestPriDSD = getHighestPriDSD(Adapter);
	Adapter->eActiveDSD = uiHighestPriDSD;

	if(DSD0  == uiHighestPriDSD)
		Adapter->ulFlashCalStart = Adapter->psFlash2xCSInfo->OffsetFromZeroForDSDStart;
	if(DSD1 == uiHighestPriDSD)
		Adapter->ulFlashCalStart = Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD1Start;
	if(DSD2 == uiHighestPriDSD)
		Adapter->ulFlashCalStart = Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD2Start;
	if(Adapter->eActiveDSD)
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "Active DSD :%d", Adapter->eActiveDSD);
	if(Adapter->eActiveDSD == 0)
	{
		
		if(IsSectionWritable(Adapter,DSD2))
		{
			Adapter->eActiveDSD = DSD2;
			Adapter->ulFlashCalStart = Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD2Start;
		}
		else if(IsSectionWritable(Adapter,DSD1))
		{
			Adapter->eActiveDSD = DSD1;
			Adapter->ulFlashCalStart = Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD1Start;
		}
		else if(IsSectionWritable(Adapter,DSD0))
		{
			Adapter->eActiveDSD = DSD0;
			Adapter->ulFlashCalStart = Adapter->psFlash2xCSInfo->OffsetFromZeroForDSDStart;
		}
	}

	return STATUS_SUCCESS;
}



static INT BcmGetActiveISO(PMINI_ADAPTER Adapter)
{

	INT HighestPriISO = 0 ;
	HighestPriISO = getHighestPriISO(Adapter);

	Adapter->eActiveISO = HighestPriISO ;
	if(Adapter->eActiveISO == ISO_IMAGE2)
		Adapter->uiActiveISOOffset = (Adapter->psFlash2xCSInfo->OffsetISOImage2Part1Start);
	else if(Adapter->eActiveISO == ISO_IMAGE1)
		Adapter->uiActiveISOOffset = (Adapter->psFlash2xCSInfo->OffsetISOImage1Part1Start);

	if(Adapter->eActiveISO)
	 	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Active ISO :%x", Adapter->eActiveISO);

	return STATUS_SUCCESS;
}

B_UINT8 IsOffsetWritable(PMINI_ADAPTER Adapter, UINT uiOffset)
{
	UINT uiSectorNum = 0;
	UINT uiWordOfSectorPermission =0;
	UINT uiBitofSectorePermission = 0;
	B_UINT32 permissionBits = 0;
	uiSectorNum = uiOffset/Adapter->uiSectorSize;

	
	uiWordOfSectorPermission = Adapter->psFlash2xCSInfo->SectorAccessBitMap[uiSectorNum /16];

	
	uiBitofSectorePermission = 2*(15 - uiSectorNum %16);

	
	permissionBits = uiWordOfSectorPermission & (0x3 << uiBitofSectorePermission) ;
	permissionBits = (permissionBits >> uiBitofSectorePermission) & 0x3;
	if(permissionBits == SECTOR_READWRITE_PERMISSION)
		return 	TRUE;
	else
		return FALSE;
}

static INT BcmDumpFlash2xSectionBitMap(PFLASH2X_BITMAP psFlash2xBitMap)
{
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "***************Flash 2.x Section Bitmap***************");
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"ISO_IMAGE1  :0X%x", psFlash2xBitMap->ISO_IMAGE1);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"ISO_IMAGE2  :0X%x", psFlash2xBitMap->ISO_IMAGE2);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"DSD0  :0X%x", psFlash2xBitMap->DSD0);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"DSD1  :0X%x", psFlash2xBitMap->DSD1);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"DSD2  :0X%x", psFlash2xBitMap->DSD2);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"VSA0  :0X%x", psFlash2xBitMap->VSA0);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"VSA1  :0X%x", psFlash2xBitMap->VSA1);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"VSA2  :0X%x", psFlash2xBitMap->VSA2);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"SCSI  :0X%x", psFlash2xBitMap->SCSI);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"CONTROL_SECTION  :0X%x", psFlash2xBitMap->CONTROL_SECTION);

	return STATUS_SUCCESS;
}


INT BcmGetFlash2xSectionalBitMap(PMINI_ADAPTER Adapter, PFLASH2X_BITMAP psFlash2xBitMap)
{


	PFLASH2X_CS_INFO psFlash2xCSInfo = Adapter->psFlash2xCSInfo;
	FLASH2X_SECTION_VAL uiHighestPriDSD = 0 ;
	FLASH2X_SECTION_VAL uiHighestPriISO= 0 ;
	BOOLEAN SetActiveDSDDone = FALSE ;
	BOOLEAN SetActiveISODone = FALSE ;

	
	
	if(IsFlash2x(Adapter) == FALSE)
	{
		psFlash2xBitMap->ISO_IMAGE2 = 0;
		psFlash2xBitMap->ISO_IMAGE1 = 0;
		psFlash2xBitMap->DSD0 = FLASH2X_SECTION_VALID | FLASH2X_SECTION_ACT | FLASH2X_SECTION_PRESENT; 
		psFlash2xBitMap->DSD1  = 0 ;
		psFlash2xBitMap->DSD2 = 0 ;
		psFlash2xBitMap->VSA0 = 0 ;
		psFlash2xBitMap->VSA1 = 0 ;
		psFlash2xBitMap->VSA2 = 0 ;
		psFlash2xBitMap->CONTROL_SECTION = 0 ;
		psFlash2xBitMap->SCSI= 0 ;
		psFlash2xBitMap->Reserved0 = 0 ;
		psFlash2xBitMap->Reserved1 = 0 ;
		psFlash2xBitMap->Reserved2 = 0 ;
		return STATUS_SUCCESS ;

	}

	uiHighestPriDSD = getHighestPriDSD(Adapter);
	uiHighestPriISO = getHighestPriISO(Adapter);

	
	
	
	if((psFlash2xCSInfo->OffsetISOImage2Part1Start) != UNINIT_PTR_IN_CS)
	{
		
		psFlash2xBitMap->ISO_IMAGE2= psFlash2xBitMap->ISO_IMAGE2 | FLASH2X_SECTION_PRESENT;


		if(ReadISOSignature(Adapter,ISO_IMAGE2)== ISO_IMAGE_MAGIC_NUMBER)
			psFlash2xBitMap->ISO_IMAGE2 |= FLASH2X_SECTION_VALID;


		
		if(IsSectionWritable(Adapter, ISO_IMAGE2) == FALSE)
			psFlash2xBitMap->ISO_IMAGE2 |= FLASH2X_SECTION_RO;

		if(SetActiveISODone == FALSE && uiHighestPriISO == ISO_IMAGE2)
		{
			psFlash2xBitMap->ISO_IMAGE2 |= FLASH2X_SECTION_ACT ;
			SetActiveISODone = TRUE;
		}

	}

	
	
	
	if((psFlash2xCSInfo->OffsetISOImage1Part1Start) != UNINIT_PTR_IN_CS)
	{
		
		psFlash2xBitMap->ISO_IMAGE1 = psFlash2xBitMap->ISO_IMAGE1 | FLASH2X_SECTION_PRESENT;

		if(ReadISOSignature(Adapter,ISO_IMAGE1) == ISO_IMAGE_MAGIC_NUMBER)
			psFlash2xBitMap->ISO_IMAGE1 |= FLASH2X_SECTION_VALID;

		
		if(IsSectionWritable(Adapter, ISO_IMAGE1) == FALSE)
			psFlash2xBitMap->ISO_IMAGE1 |= FLASH2X_SECTION_RO;

		if(SetActiveISODone == FALSE && uiHighestPriISO == ISO_IMAGE1)
		{
			psFlash2xBitMap->ISO_IMAGE1 |= FLASH2X_SECTION_ACT ;
			SetActiveISODone = TRUE;
		}
	}



	
	
	
	if((psFlash2xCSInfo->OffsetFromZeroForDSD2Start) != UNINIT_PTR_IN_CS)
	{
		
		psFlash2xBitMap->DSD2= psFlash2xBitMap->DSD2 | FLASH2X_SECTION_PRESENT;

		if(ReadDSDSignature(Adapter,DSD2)== DSD_IMAGE_MAGIC_NUMBER)
			psFlash2xBitMap->DSD2 |= FLASH2X_SECTION_VALID;

		
		if(IsSectionWritable(Adapter, DSD2) == FALSE)
		{
			psFlash2xBitMap->DSD2 |= FLASH2X_SECTION_RO;

		}
		else
		{
			
			if((SetActiveDSDDone == FALSE) && (uiHighestPriDSD == DSD2))
			{
				psFlash2xBitMap->DSD2 |= FLASH2X_SECTION_ACT ;
				SetActiveDSDDone =TRUE ;
			}
		}
	}

	
	
	
	if((psFlash2xCSInfo->OffsetFromZeroForDSD1Start) != UNINIT_PTR_IN_CS)
	{
		
		psFlash2xBitMap->DSD1= psFlash2xBitMap->DSD1 | FLASH2X_SECTION_PRESENT;


		if(ReadDSDSignature(Adapter,DSD1)== DSD_IMAGE_MAGIC_NUMBER)
			psFlash2xBitMap->DSD1 |= FLASH2X_SECTION_VALID;

		
		if(IsSectionWritable(Adapter, DSD1) == FALSE)
		{
			psFlash2xBitMap->DSD1 |= FLASH2X_SECTION_RO;
		}
		else
		{
			
			if((SetActiveDSDDone == FALSE) && (uiHighestPriDSD == DSD1))
			{
					psFlash2xBitMap->DSD1 |= FLASH2X_SECTION_ACT ;
					SetActiveDSDDone =TRUE ;
			}
		}

	}

	
	
	
	if((psFlash2xCSInfo->OffsetFromZeroForDSDStart) != UNINIT_PTR_IN_CS)
	{
		
		psFlash2xBitMap->DSD0 = psFlash2xBitMap->DSD0 | FLASH2X_SECTION_PRESENT;

		if(ReadDSDSignature(Adapter,DSD0) == DSD_IMAGE_MAGIC_NUMBER)
			psFlash2xBitMap->DSD0 |= FLASH2X_SECTION_VALID;

		
		if(IsSectionWritable(Adapter, DSD0) == FALSE)
		{
			psFlash2xBitMap->DSD0 |= FLASH2X_SECTION_RO;
		}
		else
		{
			
			if((SetActiveDSDDone == FALSE) &&(uiHighestPriDSD == DSD0))
			{
					psFlash2xBitMap->DSD0 |= FLASH2X_SECTION_ACT ;
					SetActiveDSDDone =TRUE ;
			}
		}
	}

	
	
	
	if((psFlash2xCSInfo->OffsetFromZeroForVSAStart) != UNINIT_PTR_IN_CS)
	{
		
		psFlash2xBitMap->VSA0= psFlash2xBitMap->VSA0 | FLASH2X_SECTION_PRESENT;

		
		psFlash2xBitMap->VSA0 |= FLASH2X_SECTION_VALID;

		
		if(IsSectionWritable(Adapter, VSA0) == FALSE)
			psFlash2xBitMap->VSA0 |=  FLASH2X_SECTION_RO;

		
		psFlash2xBitMap->VSA0 |= FLASH2X_SECTION_ACT ;

	}


	
	
	

	if((psFlash2xCSInfo->OffsetFromZeroForVSA1Start) != UNINIT_PTR_IN_CS)
	{
		
		psFlash2xBitMap->VSA1= psFlash2xBitMap->VSA1 | FLASH2X_SECTION_PRESENT;

		
		psFlash2xBitMap->VSA1|= FLASH2X_SECTION_VALID;

		
		if(IsSectionWritable(Adapter, VSA1) == FALSE)
			psFlash2xBitMap->VSA1 |= FLASH2X_SECTION_RO;

		
		psFlash2xBitMap->VSA1 |= FLASH2X_SECTION_ACT ;

	}


	
	
	

	if((psFlash2xCSInfo->OffsetFromZeroForVSA2Start) != UNINIT_PTR_IN_CS)
	{
		
		psFlash2xBitMap->VSA2= psFlash2xBitMap->VSA2 | FLASH2X_SECTION_PRESENT;


		
		psFlash2xBitMap->VSA2 |= FLASH2X_SECTION_VALID;

		
		if(IsSectionWritable(Adapter, VSA2) == FALSE)
			psFlash2xBitMap->VSA2 |= FLASH2X_SECTION_RO;

		
		psFlash2xBitMap->VSA2 |= FLASH2X_SECTION_ACT ;
	}

	
	
	
	if((psFlash2xCSInfo->OffsetFromZeroForScsiFirmware) != UNINIT_PTR_IN_CS)
	{
		
		psFlash2xBitMap->SCSI= psFlash2xBitMap->SCSI | FLASH2X_SECTION_PRESENT;


		
		psFlash2xBitMap->SCSI|= FLASH2X_SECTION_VALID;

		
		if(IsSectionWritable(Adapter, SCSI) == FALSE)
			psFlash2xBitMap->SCSI |= FLASH2X_SECTION_RO;

		
		psFlash2xBitMap->SCSI |= FLASH2X_SECTION_ACT ;

	}


	
	
	
	if((psFlash2xCSInfo->OffsetFromZeroForControlSectionStart) != UNINIT_PTR_IN_CS)
	{
		
		psFlash2xBitMap->CONTROL_SECTION = psFlash2xBitMap->CONTROL_SECTION | (FLASH2X_SECTION_PRESENT);


		
		psFlash2xBitMap->CONTROL_SECTION |= FLASH2X_SECTION_VALID;

		
		if(IsSectionWritable(Adapter, CONTROL_SECTION) == FALSE)
			psFlash2xBitMap->CONTROL_SECTION |= FLASH2X_SECTION_RO;

		
		psFlash2xBitMap->CONTROL_SECTION |= FLASH2X_SECTION_ACT ;

	}

	
	
	
	psFlash2xBitMap->Reserved0 = 0;
	psFlash2xBitMap->Reserved0 = 0;
	psFlash2xBitMap->Reserved0 = 0;

	BcmDumpFlash2xSectionBitMap(psFlash2xBitMap);

	return STATUS_SUCCESS ;

}
INT BcmSetActiveSection(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL eFlash2xSectVal)
{
	unsigned int SectImagePriority = 0;
	INT Status =STATUS_SUCCESS;

	
	
	INT HighestPriDSD = 0 ;
	INT HighestPriISO = 0;



	Status = IsSectionWritable(Adapter,eFlash2xSectVal) ;
	if(Status != TRUE )
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Provided Section <%d> is not writable",eFlash2xSectVal);
		return STATUS_FAILURE;
	}

	Adapter->bHeaderChangeAllowed = TRUE ;
	switch(eFlash2xSectVal)
	{
		case ISO_IMAGE1 :
		case ISO_IMAGE2	:
			if(ReadISOSignature(Adapter,eFlash2xSectVal)== ISO_IMAGE_MAGIC_NUMBER )
			{
				HighestPriISO = getHighestPriISO(Adapter);

				if(HighestPriISO == eFlash2xSectVal	)
				{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Given ISO<%x> already  has highest priority",eFlash2xSectVal );
					Status = STATUS_SUCCESS ;
					break;
				}

				SectImagePriority = ReadISOPriority(Adapter, HighestPriISO) + 1;

				if((SectImagePriority <= 0) && IsSectionWritable(Adapter,HighestPriISO))
				{
					
					
					
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "SectImagePriority wraparound happened, eFlash2xSectVal: 0x%x\n",eFlash2xSectVal);
					SectImagePriority = htonl(0x1);
					Status = BcmFlash2xBulkWrite(Adapter,
								&SectImagePriority,
								HighestPriISO,
								0 + FIELD_OFFSET_IN_HEADER(PISO_HEADER, ISOImagePriority),
								SIGNATURE_SIZE,
								TRUE);

					if(Status)
					{
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Priority has not been written properly");
						Status = STATUS_FAILURE;
						break ;
					}

					HighestPriISO = getHighestPriISO(Adapter);

					if(HighestPriISO == eFlash2xSectVal	)
					{
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Given ISO<%x> already  has highest priority",eFlash2xSectVal );
						Status = STATUS_SUCCESS ;
						break;
					}

					SectImagePriority = 2;
				 }


				SectImagePriority = htonl(SectImagePriority);

				Status = BcmFlash2xBulkWrite(Adapter,
								&SectImagePriority,
								eFlash2xSectVal,
								0 + FIELD_OFFSET_IN_HEADER(PISO_HEADER, ISOImagePriority),
								SIGNATURE_SIZE,
								TRUE);
				if(Status)
				{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Priority has not been written properly");
					break ;
				}
			}
			else
			{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Signature is currupted. Hence can't increase the priority");
				Status = STATUS_FAILURE ;
				break;
			}
			break;
		case DSD0 :
		case DSD1 :
		case DSD2 :
			if(ReadDSDSignature(Adapter,eFlash2xSectVal)== DSD_IMAGE_MAGIC_NUMBER)
			{
				HighestPriDSD = getHighestPriDSD(Adapter);

				if((HighestPriDSD == eFlash2xSectVal))
				{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Given DSD<%x> already  has highest priority", eFlash2xSectVal);
					Status = STATUS_SUCCESS ;
					break;
				}

				SectImagePriority = ReadDSDPriority(Adapter, HighestPriDSD) + 1 ;
				if(SectImagePriority <= 0)
				{
					
					
					
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, NVM_RW, DBG_LVL_ALL, "SectImagePriority wraparound happened, eFlash2xSectVal: 0x%x\n",eFlash2xSectVal);
					SectImagePriority = htonl(0x1);

					Status = BcmFlash2xBulkWrite(Adapter,
									&SectImagePriority,
									HighestPriDSD,
									Adapter->psFlash2xCSInfo->OffsetFromDSDStartForDSDHeader + FIELD_OFFSET_IN_HEADER(PDSD_HEADER, DSDImagePriority),
									SIGNATURE_SIZE,
									TRUE);

					if(Status)
					{
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Priority has not been written properly");
						break ;
					}

					HighestPriDSD = getHighestPriDSD(Adapter);

					if((HighestPriDSD == eFlash2xSectVal))
					{
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Made the DSD: %x highest by reducing priority of other\n", eFlash2xSectVal);
						Status = STATUS_SUCCESS ;
						break;
					}

					SectImagePriority = htonl(0x2);
					Status = BcmFlash2xBulkWrite(Adapter,
									&SectImagePriority,
									HighestPriDSD,
									Adapter->psFlash2xCSInfo->OffsetFromDSDStartForDSDHeader + FIELD_OFFSET_IN_HEADER(PDSD_HEADER, DSDImagePriority),
									SIGNATURE_SIZE,
									TRUE);

					if(Status)
					{
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Priority has not been written properly");
						break ;
					}

					HighestPriDSD = getHighestPriDSD(Adapter);

					if((HighestPriDSD == eFlash2xSectVal))
					{
						Status = STATUS_SUCCESS ;
						break;
					}
					SectImagePriority = 3 ;

				}
				SectImagePriority = htonl(SectImagePriority);
				Status = BcmFlash2xBulkWrite(Adapter,
								&SectImagePriority,
								eFlash2xSectVal,
								Adapter->psFlash2xCSInfo->OffsetFromDSDStartForDSDHeader + FIELD_OFFSET_IN_HEADER(PDSD_HEADER, DSDImagePriority),
								SIGNATURE_SIZE ,
								TRUE);
				if(Status)
				{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Priority has not been written properly");
					Status = STATUS_FAILURE ;
					break ;
				}
			}
			else
			{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Signature is currupted. Hence can't increase the priority");
				Status = STATUS_FAILURE ;
				break;
			}
			break;
		case VSA0 :
		case VSA1 :
		case VSA2 :
			
			break ;
		default :
				Status = STATUS_FAILURE ;
				break;

	}

	Adapter->bHeaderChangeAllowed = FALSE ;
	return Status;

}

INT BcmCopyISO(PMINI_ADAPTER Adapter, FLASH2X_COPY_SECTION sCopySectStrut)
{

	PCHAR Buff = NULL;
	FLASH2X_SECTION_VAL eISOReadPart = 0,eISOWritePart = 0;
	UINT uiReadOffsetWithinPart = 0, uiWriteOffsetWithinPart = 0;
	UINT uiTotalDataToCopy = 0;
	BOOLEAN IsThisHeaderSector = FALSE ;
	UINT sigOffset = 0;
	UINT ISOLength = 0;
	UINT Status = STATUS_SUCCESS;
	UINT SigBuff[MAX_RW_SIZE];
	UINT i = 0;

	if(ReadISOSignature(Adapter,sCopySectStrut.SrcSection) != ISO_IMAGE_MAGIC_NUMBER)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "error as Source ISO Section does not have valid signature");
		return STATUS_FAILURE;
	}

	Status = BcmFlash2xBulkRead(Adapter,
					   &ISOLength,
					   sCopySectStrut.SrcSection,
					   0 + FIELD_OFFSET_IN_HEADER(PISO_HEADER,ISOImageSize),
					   4);

	if(Status)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Read failed while copying ISO\n");
		return Status;
	}

	ISOLength = htonl(ISOLength);

	if(ISOLength % Adapter->uiSectorSize)
	{
		ISOLength = Adapter->uiSectorSize*(1 + ISOLength/Adapter->uiSectorSize);
	}

	sigOffset = FIELD_OFFSET_IN_HEADER(PISO_HEADER, ISOImageMagicNumber);

	Buff = kzalloc(Adapter->uiSectorSize, GFP_KERNEL);

	if(Buff == NULL)
	{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Memory allocation failed for section size");
			return -ENOMEM;
	}

	if(sCopySectStrut.SrcSection ==ISO_IMAGE1 && sCopySectStrut.DstSection ==ISO_IMAGE2)
	{
		eISOReadPart = ISO_IMAGE1 ;
		eISOWritePart = ISO_IMAGE2 ;
		uiReadOffsetWithinPart =  0;
		uiWriteOffsetWithinPart = 0 ;

		uiTotalDataToCopy =(Adapter->psFlash2xCSInfo->OffsetISOImage1Part1End) -
						   (Adapter->psFlash2xCSInfo->OffsetISOImage1Part1Start)+
						   (Adapter->psFlash2xCSInfo->OffsetISOImage1Part2End) -
						   (Adapter->psFlash2xCSInfo->OffsetISOImage1Part2Start)+
						   (Adapter->psFlash2xCSInfo->OffsetISOImage1Part3End) -
						   (Adapter->psFlash2xCSInfo->OffsetISOImage1Part3Start);

		if(uiTotalDataToCopy < ISOLength)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"error as Source ISO Section does not have valid signature");
			Status = STATUS_FAILURE;
			goto out;
		}

		uiTotalDataToCopy =(Adapter->psFlash2xCSInfo->OffsetISOImage2Part1End) -
						   (Adapter->psFlash2xCSInfo->OffsetISOImage2Part1Start)+
						   (Adapter->psFlash2xCSInfo->OffsetISOImage2Part2End) -
						   (Adapter->psFlash2xCSInfo->OffsetISOImage2Part2Start)+
						   (Adapter->psFlash2xCSInfo->OffsetISOImage2Part3End) -
						   (Adapter->psFlash2xCSInfo->OffsetISOImage2Part3Start);

		if(uiTotalDataToCopy < ISOLength)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"error as Dest ISO Section does not have enough section size");
			Status = STATUS_FAILURE;
			goto out;
		}

		uiTotalDataToCopy = ISOLength;

		CorruptISOSig(Adapter,ISO_IMAGE2);

		while(uiTotalDataToCopy)
		{
			if(uiTotalDataToCopy == Adapter->uiSectorSize)
			{
				//Setting for write of first sector. First sector is assumed to be written in last
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Writing the signature sector");
				eISOReadPart = ISO_IMAGE1 ;
				uiReadOffsetWithinPart = 0;
				eISOWritePart = ISO_IMAGE2;
				uiWriteOffsetWithinPart = 0 ;
				IsThisHeaderSector = TRUE ;

			}
			else
			{
				uiReadOffsetWithinPart = uiReadOffsetWithinPart + Adapter->uiSectorSize ;
				uiWriteOffsetWithinPart = uiWriteOffsetWithinPart + Adapter->uiSectorSize ;

				if((eISOReadPart == ISO_IMAGE1) && (uiReadOffsetWithinPart == (Adapter->psFlash2xCSInfo->OffsetISOImage1Part1End - Adapter->psFlash2xCSInfo->OffsetISOImage1Part1Start) ))
				{
					eISOReadPart = ISO_IMAGE1_PART2 ;
					uiReadOffsetWithinPart = 0;
				}
				if((eISOReadPart == ISO_IMAGE1_PART2) && (uiReadOffsetWithinPart == (Adapter->psFlash2xCSInfo->OffsetISOImage1Part2End - Adapter->psFlash2xCSInfo->OffsetISOImage1Part2Start)))
				{
					eISOReadPart = ISO_IMAGE1_PART3 ;
					uiReadOffsetWithinPart = 0;
				}
				if((eISOWritePart == ISO_IMAGE2) && (uiWriteOffsetWithinPart == (Adapter->psFlash2xCSInfo->OffsetISOImage2Part1End - Adapter->psFlash2xCSInfo->OffsetISOImage2Part1Start)))
				{
					eISOWritePart = ISO_IMAGE2_PART2 ;
					uiWriteOffsetWithinPart = 0;
				}
				if((eISOWritePart == ISO_IMAGE2_PART2) && (uiWriteOffsetWithinPart == (Adapter->psFlash2xCSInfo->OffsetISOImage2Part2End - Adapter->psFlash2xCSInfo->OffsetISOImage2Part2Start)))
				{
					eISOWritePart = ISO_IMAGE2_PART3 ;
					uiWriteOffsetWithinPart = 0;
				}
			}

			Status = BcmFlash2xBulkRead(Adapter,
								   (PUINT)Buff,
								   eISOReadPart,
								   uiReadOffsetWithinPart,
								   Adapter->uiSectorSize
								);

			if(Status)
			{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Read failed while copying ISO: Part: %x, OffsetWithinPart: %x\n", eISOReadPart, uiReadOffsetWithinPart);
				break;
			}

			if(IsThisHeaderSector == TRUE)
			{
				
				memcpy(SigBuff, Buff + sigOffset, MAX_RW_SIZE);

				for(i = 0; i < MAX_RW_SIZE;i++)
					*(Buff + sigOffset + i) = 0xFF;
			}
			Adapter->bHeaderChangeAllowed = TRUE ;

			Status = BcmFlash2xBulkWrite(Adapter,
								 (PUINT)Buff,
								 eISOWritePart,
								 uiWriteOffsetWithinPart,
								 Adapter->uiSectorSize,
								 TRUE);
			if(Status)
			{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Write failed while copying ISO: Part: %x, OffsetWithinPart: %x\n", eISOWritePart, uiWriteOffsetWithinPart);
				break;
			}

			Adapter->bHeaderChangeAllowed = FALSE;

			if(IsThisHeaderSector == TRUE)
			{
				WriteToFlashWithoutSectorErase(Adapter,
												SigBuff,
												eISOWritePart,
												sigOffset,
												MAX_RW_SIZE);
				IsThisHeaderSector = FALSE ;
			}
			//subtracting the written Data
			uiTotalDataToCopy = uiTotalDataToCopy - Adapter->uiSectorSize ;
		}


	}

	if(sCopySectStrut.SrcSection ==ISO_IMAGE2 && sCopySectStrut.DstSection ==ISO_IMAGE1)
	{
		eISOReadPart = ISO_IMAGE2 ;
		eISOWritePart = ISO_IMAGE1 ;
		uiReadOffsetWithinPart =	0;
		uiWriteOffsetWithinPart = 0 ;

		uiTotalDataToCopy =(Adapter->psFlash2xCSInfo->OffsetISOImage2Part1End) -
						   (Adapter->psFlash2xCSInfo->OffsetISOImage2Part1Start)+
						   (Adapter->psFlash2xCSInfo->OffsetISOImage2Part2End) -
						   (Adapter->psFlash2xCSInfo->OffsetISOImage2Part2Start)+
						   (Adapter->psFlash2xCSInfo->OffsetISOImage2Part3End) -
						   (Adapter->psFlash2xCSInfo->OffsetISOImage2Part3Start);

		if(uiTotalDataToCopy < ISOLength)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"error as Source ISO Section does not have valid signature");
			Status = STATUS_FAILURE;
			goto out;
		}

		uiTotalDataToCopy =(Adapter->psFlash2xCSInfo->OffsetISOImage1Part1End) -
						   (Adapter->psFlash2xCSInfo->OffsetISOImage1Part1Start)+
						   (Adapter->psFlash2xCSInfo->OffsetISOImage1Part2End) -
						   (Adapter->psFlash2xCSInfo->OffsetISOImage1Part2Start)+
						   (Adapter->psFlash2xCSInfo->OffsetISOImage1Part3End) -
						   (Adapter->psFlash2xCSInfo->OffsetISOImage1Part3Start);

		if(uiTotalDataToCopy < ISOLength)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"error as Dest ISO Section does not have enough section size");
			Status = STATUS_FAILURE;
			goto out;
		}

		uiTotalDataToCopy = ISOLength;

		CorruptISOSig(Adapter,ISO_IMAGE1);

		while(uiTotalDataToCopy)
		{
			if(uiTotalDataToCopy == Adapter->uiSectorSize)
			{
				//Setting for write of first sector. First sector is assumed to be written in last
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Writing the signature sector");
				eISOReadPart = ISO_IMAGE2 ;
				uiReadOffsetWithinPart = 0;
				eISOWritePart = ISO_IMAGE1;
				uiWriteOffsetWithinPart = 0 ;
				IsThisHeaderSector = TRUE;

			}
			else
			{
				uiReadOffsetWithinPart = uiReadOffsetWithinPart + Adapter->uiSectorSize ;
				uiWriteOffsetWithinPart = uiWriteOffsetWithinPart + Adapter->uiSectorSize ;

				if((eISOReadPart == ISO_IMAGE2) && (uiReadOffsetWithinPart == (Adapter->psFlash2xCSInfo->OffsetISOImage2Part1End - Adapter->psFlash2xCSInfo->OffsetISOImage2Part1Start) ))
				{
					eISOReadPart = ISO_IMAGE2_PART2 ;
					uiReadOffsetWithinPart = 0;
				}
				if((eISOReadPart == ISO_IMAGE2_PART2) && (uiReadOffsetWithinPart == (Adapter->psFlash2xCSInfo->OffsetISOImage2Part2End - Adapter->psFlash2xCSInfo->OffsetISOImage2Part2Start)))
				{
					eISOReadPart = ISO_IMAGE2_PART3 ;
					uiReadOffsetWithinPart = 0;
				}
				if((eISOWritePart == ISO_IMAGE1) && (uiWriteOffsetWithinPart == (Adapter->psFlash2xCSInfo->OffsetISOImage1Part1End - Adapter->psFlash2xCSInfo->OffsetISOImage1Part1Start)))
				{
					eISOWritePart = ISO_IMAGE1_PART2 ;
					uiWriteOffsetWithinPart = 0;
				}
				if((eISOWritePart == ISO_IMAGE1_PART2) && (uiWriteOffsetWithinPart == (Adapter->psFlash2xCSInfo->OffsetISOImage1Part2End - Adapter->psFlash2xCSInfo->OffsetISOImage1Part2Start)))
				{
					eISOWritePart = ISO_IMAGE1_PART3 ;
					uiWriteOffsetWithinPart = 0;
				}
			}

			Status = BcmFlash2xBulkRead(Adapter,
								   (PUINT)Buff,
								   eISOReadPart,
								   uiReadOffsetWithinPart,
								   Adapter->uiSectorSize
								);
			if(Status)
			{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Read failed while copying ISO: Part: %x, OffsetWithinPart: %x\n", eISOReadPart, uiReadOffsetWithinPart);
				break;
			}

			if(IsThisHeaderSector == TRUE)
			{
				
				memcpy(SigBuff, Buff + sigOffset, MAX_RW_SIZE);

				for(i = 0; i < MAX_RW_SIZE;i++)
					*(Buff + sigOffset + i) = 0xFF;

			}
			Adapter->bHeaderChangeAllowed = TRUE ;
			Status = BcmFlash2xBulkWrite(Adapter,
								 (PUINT)Buff,
								 eISOWritePart,
								 uiWriteOffsetWithinPart,
								 Adapter->uiSectorSize,
								 TRUE);

			if(Status)
			{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Write failed while copying ISO: Part: %x, OffsetWithinPart: %x\n", eISOWritePart, uiWriteOffsetWithinPart);
				break;
			}

			Adapter->bHeaderChangeAllowed = FALSE ;

			if(IsThisHeaderSector == TRUE)
			{
				WriteToFlashWithoutSectorErase(Adapter,
												SigBuff,
												eISOWritePart,
												sigOffset,
												MAX_RW_SIZE);
				IsThisHeaderSector = FALSE ;
			}

			//subtracting the written Data
			uiTotalDataToCopy = uiTotalDataToCopy - Adapter->uiSectorSize ;
		}


	}

out:
	kfree(Buff);

	return Status;
}
/**
BcmFlash2xCorruptSig : this API is used to corrupt the written sig in Bcm Header present in flash section.
					     It will corrupt the sig, if Section is writable, by making first bytes as zero.
@Adapater :- Bcm Driver Private Data Structure
@eFlash2xSectionVal :- Flash section val which has header

Return Value :-
	Success :- If Section is present and writable, corrupt the sig and return STATUS_SUCCESS
	Failure :-Return negative error code


**/
INT BcmFlash2xCorruptSig(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL eFlash2xSectionVal)
{

	INT Status = STATUS_SUCCESS ;
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Section Value :%x \n", eFlash2xSectionVal);

	if((eFlash2xSectionVal == DSD0) || (eFlash2xSectionVal == DSD1) || (eFlash2xSectionVal == DSD2))
	{
		Status = CorruptDSDSig(Adapter, eFlash2xSectionVal);
	}
	else if(eFlash2xSectionVal == ISO_IMAGE1 || eFlash2xSectionVal == ISO_IMAGE2)
	{
		Status = CorruptISOSig(Adapter, eFlash2xSectionVal);
	}
	else
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Given Section <%d>does not have Header",eFlash2xSectionVal);
		return STATUS_SUCCESS;
	}
	return Status;
}
INT BcmFlash2xWriteSig(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL eFlashSectionVal)
{

	UINT uiSignature = 0 ;
	UINT uiOffset = 0;
	

	if(Adapter->bSigCorrupted == FALSE)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Signature is not corrupted by driver, hence not restoring\n");
		return STATUS_SUCCESS;
	}
	if(Adapter->bAllDSDWriteAllow == FALSE)
	{
		if(IsSectionWritable(Adapter,eFlashSectionVal) == FALSE)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Section is not Writable...Hence can't Write signature");
			return SECTOR_IS_NOT_WRITABLE;
		}
	}
	if((eFlashSectionVal == DSD0) ||(eFlashSectionVal == DSD1) || (eFlashSectionVal == DSD2))
	{
		uiSignature = htonl(DSD_IMAGE_MAGIC_NUMBER) ;
		uiOffset = Adapter->psFlash2xCSInfo->OffsetFromDSDStartForDSDHeader ;

		uiOffset += FIELD_OFFSET_IN_HEADER(PDSD_HEADER,DSDImageMagicNumber);

		if((ReadDSDSignature(Adapter,eFlashSectionVal) & 0xFF000000) != CORRUPTED_PATTERN)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Corrupted Pattern is not there. Hence won't write sig");
			return STATUS_FAILURE;
		}

	}
	else if((eFlashSectionVal == ISO_IMAGE1) || (eFlashSectionVal == ISO_IMAGE2))
	{
		uiSignature = htonl(ISO_IMAGE_MAGIC_NUMBER);
		
		uiOffset = FIELD_OFFSET_IN_HEADER(PISO_HEADER,ISOImageMagicNumber);
		if((ReadISOSignature(Adapter,eFlashSectionVal) & 0xFF000000) != CORRUPTED_PATTERN)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Currupted Pattern is not there. Hence won't write sig");
			return STATUS_FAILURE;
		}
	}
	else
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"GIVEN SECTION< %d > IS NOT VALID FOR SIG WRITE...", eFlashSectionVal);
		return STATUS_FAILURE;
	}

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Restoring the signature");


	Adapter->bHeaderChangeAllowed = TRUE;
	Adapter->bSigCorrupted = FALSE;
	BcmFlash2xBulkWrite(Adapter, &uiSignature,eFlashSectionVal,uiOffset,SIGNATURE_SIZE,TRUE);
	Adapter->bHeaderChangeAllowed = FALSE;



	return STATUS_SUCCESS;
}
INT	validateFlash2xReadWrite(PMINI_ADAPTER Adapter, PFLASH2X_READWRITE psFlash2xReadWrite)
{
	UINT uiNumOfBytes = 0 ;
	UINT uiSectStartOffset = 0 ;
	UINT uiSectEndOffset = 0;
	uiNumOfBytes = psFlash2xReadWrite->numOfBytes;

	if(IsSectionExistInFlash(Adapter,psFlash2xReadWrite->Section) != TRUE)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Section<%x> does not exixt in Flash",psFlash2xReadWrite->Section);
		return FALSE;
	}
	uiSectStartOffset = BcmGetSectionValStartOffset(Adapter,psFlash2xReadWrite->Section);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "Start offset :%x ,section :%d\n",uiSectStartOffset,psFlash2xReadWrite->Section);
	if((psFlash2xReadWrite->Section == ISO_IMAGE1) ||(psFlash2xReadWrite->Section == ISO_IMAGE2))
	{
		if(psFlash2xReadWrite->Section == ISO_IMAGE1)
		{
			uiSectEndOffset = BcmGetSectionValEndOffset(Adapter,ISO_IMAGE1) -
							  BcmGetSectionValStartOffset(Adapter,ISO_IMAGE1)+
							  BcmGetSectionValEndOffset(Adapter,ISO_IMAGE1_PART2) -
							  BcmGetSectionValStartOffset(Adapter,ISO_IMAGE1_PART2)+
							  BcmGetSectionValEndOffset(Adapter,ISO_IMAGE1_PART3) -
							  BcmGetSectionValStartOffset(Adapter,ISO_IMAGE1_PART3);
		}
		else if(psFlash2xReadWrite->Section == ISO_IMAGE2)
		{
			uiSectEndOffset = BcmGetSectionValEndOffset(Adapter,ISO_IMAGE2) -
							  BcmGetSectionValStartOffset(Adapter,ISO_IMAGE2)+
							  BcmGetSectionValEndOffset(Adapter,ISO_IMAGE2_PART2) -
							  BcmGetSectionValStartOffset(Adapter,ISO_IMAGE2_PART2)+
							  BcmGetSectionValEndOffset(Adapter,ISO_IMAGE2_PART3) -
							  BcmGetSectionValStartOffset(Adapter,ISO_IMAGE2_PART3);

		}

		
		
		uiSectEndOffset = uiSectStartOffset + uiSectEndOffset ;

		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Total size of the ISO Image :%x",uiSectEndOffset);
	}
	else
		uiSectEndOffset   = BcmGetSectionValEndOffset(Adapter,psFlash2xReadWrite->Section);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL, "End offset :%x \n",uiSectEndOffset);

	
	if((uiSectStartOffset + psFlash2xReadWrite->offset + uiNumOfBytes) <= uiSectEndOffset)
		return TRUE;
	else
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Invalid Request....");
		return FALSE;
	}

}


INT IsFlash2x(PMINI_ADAPTER Adapter)
{
	if(Adapter->uiFlashLayoutMajorVersion >= FLASH_2X_MAJOR_NUMBER)
		return TRUE ;
	else
		return FALSE;
}

static INT GetFlashBaseAddr(PMINI_ADAPTER Adapter)
{

	UINT uiBaseAddr = 0;

	if(Adapter->bDDRInitDone)
	{
		if(Adapter->uiFlashLayoutMajorVersion && (Adapter->bFlashRawRead == FALSE) &&
			!((Adapter->uiFlashLayoutMajorVersion == 1) && (Adapter->uiFlashLayoutMinorVersion == 1))
			)
			uiBaseAddr = Adapter->uiFlashBaseAdd ;
		else
			uiBaseAddr = FLASH_CONTIGIOUS_START_ADDR_AFTER_INIT;
	}
	else
	{
		if(Adapter->uiFlashLayoutMajorVersion && (Adapter->bFlashRawRead == FALSE) &&
			!((Adapter->uiFlashLayoutMajorVersion == 1) && (Adapter->uiFlashLayoutMinorVersion == 1))
			)
			uiBaseAddr = Adapter->uiFlashBaseAdd | FLASH_CONTIGIOUS_START_ADDR_BEFORE_INIT;
		else
			uiBaseAddr = FLASH_CONTIGIOUS_START_ADDR_BEFORE_INIT;
	}

	return uiBaseAddr ;
}

INT	BcmCopySection(PMINI_ADAPTER Adapter,
						FLASH2X_SECTION_VAL SrcSection,
						FLASH2X_SECTION_VAL DstSection,
						UINT offset,
						UINT numOfBytes)
{
	UINT BuffSize = 0 ;
	UINT BytesToBeCopied = 0;
	PUCHAR pBuff = NULL ;
	INT Status = STATUS_SUCCESS ;
	if(SrcSection == DstSection)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Source and Destination should be different ...try again");
		return -EINVAL;
	}
	if((SrcSection != DSD0) && (SrcSection != DSD1) && (SrcSection != DSD2))
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Source should be DSD subsection");
		return  -EINVAL;
	}
	if((DstSection != DSD0) && (DstSection != DSD1) && (DstSection != DSD2))
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Destination should be DSD subsection");
		return  -EINVAL;
	}

	

	if(numOfBytes == 0)
	{
		numOfBytes = BcmGetSectionValEndOffset(Adapter,SrcSection)
				  - BcmGetSectionValStartOffset(Adapter,SrcSection);

		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL," Section Size :0x%x",numOfBytes);
	}

	if((offset + numOfBytes) > BcmGetSectionValEndOffset(Adapter,SrcSection)
				  - BcmGetSectionValStartOffset(Adapter,SrcSection))
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0," Input parameters going beyond the section offS: %x numB: %x of Source Section\n",
						offset, numOfBytes);
		return -EINVAL;
	}

	if((offset + numOfBytes) > BcmGetSectionValEndOffset(Adapter,DstSection)
				  - BcmGetSectionValStartOffset(Adapter,DstSection))
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0," Input parameters going beyond the section offS: %x numB: %x of Destination Section\n",
						offset, numOfBytes);
		return -EINVAL;
	}


	if(numOfBytes > Adapter->uiSectorSize )
		BuffSize = Adapter->uiSectorSize;
	else
		BuffSize = numOfBytes ;

	pBuff = (PCHAR)kzalloc(BuffSize, GFP_KERNEL);
	if(pBuff == NULL)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Memory allocation failed.. ");
		return -ENOMEM;
	}


	BytesToBeCopied = Adapter->uiSectorSize ;
	if(offset % Adapter->uiSectorSize)
		BytesToBeCopied = Adapter->uiSectorSize - (offset % Adapter->uiSectorSize);
	if(BytesToBeCopied > numOfBytes)
		BytesToBeCopied = numOfBytes ;



	Adapter->bHeaderChangeAllowed = TRUE;

	do
	{
		Status = BcmFlash2xBulkRead(Adapter, (PUINT)pBuff, SrcSection , offset,BytesToBeCopied);
		if(Status)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Read failed at offset :%d for NOB :%d", SrcSection,BytesToBeCopied);
			break;
		}
		Status = BcmFlash2xBulkWrite(Adapter,(PUINT)pBuff,DstSection,offset,BytesToBeCopied,FALSE);
		if(Status)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Write failed at offset :%d for NOB :%d", DstSection,BytesToBeCopied);
			break;
		}
		offset = offset + BytesToBeCopied;
		numOfBytes = numOfBytes - BytesToBeCopied ;
		if(numOfBytes)
		{
			if(numOfBytes > Adapter->uiSectorSize )
				BytesToBeCopied = Adapter->uiSectorSize;
			else
				BytesToBeCopied = numOfBytes;
		}
	}while(numOfBytes > 0) ;
	kfree(pBuff);
	Adapter->bHeaderChangeAllowed = FALSE ;
	return Status;
}

/**
SaveHeaderIfPresent :- This API is use to Protect the Header in case of Header Sector write
@Adapater :- Bcm Driver Private Data Structure
@pBuff :- Data buffer that has to be written in sector having the header map.
@uiOffset :- Flash offset that has to be written.

Return value :-
	Success :- On success return STATUS_SUCCESS
	Faillure :- Return negative error code

**/

INT SaveHeaderIfPresent(PMINI_ADAPTER Adapter, PUCHAR pBuff, UINT uiOffset)
{
	UINT offsetToProtect = 0,HeaderSizeToProtect =0;
	BOOLEAN bHasHeader = FALSE ;
	PUCHAR pTempBuff =NULL;
	UINT uiSectAlignAddr = 0;
	UINT sig = 0;

	
	uiSectAlignAddr = uiOffset & ~(Adapter->uiSectorSize - 1);


	if((uiSectAlignAddr == BcmGetSectionValEndOffset(Adapter,DSD2)- Adapter->uiSectorSize)||
	(uiSectAlignAddr == BcmGetSectionValEndOffset(Adapter,DSD1)- Adapter->uiSectorSize)||
	(uiSectAlignAddr == BcmGetSectionValEndOffset(Adapter,DSD0)- Adapter->uiSectorSize))
	{

		
		offsetToProtect = Adapter->psFlash2xCSInfo->OffsetFromDSDStartForDSDHeader % Adapter->uiSectorSize;
		HeaderSizeToProtect = sizeof(DSD_HEADER);
		bHasHeader = TRUE ;
	}

	if(uiSectAlignAddr == BcmGetSectionValStartOffset(Adapter,ISO_IMAGE1) ||
		uiSectAlignAddr == BcmGetSectionValStartOffset(Adapter,ISO_IMAGE2))
	{
		offsetToProtect = 0;
		HeaderSizeToProtect = sizeof(ISO_HEADER);
		bHasHeader = TRUE;
	}
	
	if(bHasHeader && (Adapter->bHeaderChangeAllowed == FALSE))
	{
		pTempBuff = (PUCHAR)kzalloc(HeaderSizeToProtect, GFP_KERNEL);
		if(pTempBuff == NULL)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Memory allocation failed ");
			return -ENOMEM;
		}
		
		BeceemFlashBulkRead(Adapter,(PUINT)pTempBuff,(uiSectAlignAddr + offsetToProtect),HeaderSizeToProtect);
		BCM_DEBUG_PRINT_BUFFER(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,pTempBuff ,HeaderSizeToProtect);
		
		memcpy(pBuff +offsetToProtect,pTempBuff,HeaderSizeToProtect);

		kfree(pTempBuff);
	}
	if(bHasHeader && Adapter->bSigCorrupted)
	{
		sig = *((PUINT)(pBuff + offsetToProtect + FIELD_OFFSET_IN_HEADER(PDSD_HEADER,DSDImageMagicNumber)));
		sig = ntohl(sig);
		if((sig & 0xFF000000) != CORRUPTED_PATTERN)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Desired pattern is not at sig offset. Hence won't restore");
			Adapter->bSigCorrupted = FALSE;
			return STATUS_SUCCESS;
		}
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL," Corrupted sig is :%X", sig);
		*((PUINT)(pBuff + offsetToProtect + FIELD_OFFSET_IN_HEADER(PDSD_HEADER,DSDImageMagicNumber)))= htonl(DSD_IMAGE_MAGIC_NUMBER);
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Restoring the signature in Header Write only");
		Adapter->bSigCorrupted = FALSE;
	}

	return STATUS_SUCCESS ;
}

static INT BcmDoChipSelect(PMINI_ADAPTER Adapter, UINT offset)
{
	UINT FlashConfig = 0;
	INT ChipNum = 0;
	UINT GPIOConfig = 0;
	UINT PartNum = 0;

	ChipNum = offset / FLASH_PART_SIZE ;

	
	
	
	
	
	
	


	if(Adapter->SelectedChip == ChipNum)
    		return STATUS_SUCCESS;

	
	Adapter->SelectedChip = ChipNum ;

	
	rdmalt(Adapter, FLASH_CONFIG_REG, &FlashConfig, 4);
	rdmalt(Adapter, FLASH_GPIO_CONFIG_REG, &GPIOConfig, 4);

	{
		switch(ChipNum)
		{
		case 0:
			PartNum = 0;
			break;
		case 1:
			PartNum = 3;
			GPIOConfig |= (0x4 << CHIP_SELECT_BIT12);
			break;
		case 2:
			PartNum = 1;
			GPIOConfig |= (0x1 << CHIP_SELECT_BIT12);
			break;
		case 3:
			PartNum = 2;
			GPIOConfig |= (0x2 << CHIP_SELECT_BIT12);
			break;
		}
	}
	/* In case the bits already written in the FLASH_CONFIG_REG is same as what the user desired,
	    nothing to do... can return immediately.
	    ASSUMPTION: FLASH_GPIO_CONFIG_REG will be in sync with FLASH_CONFIG_REG.
	    Even if the chip goes to low power mode, it should wake with values in each register in sync with each other.
	    These values are not written by host other than during CHIP_SELECT.
	*/
	if(PartNum == ((FlashConfig >> CHIP_SELECT_BIT12) & 0x3))
		return STATUS_SUCCESS;

	
	FlashConfig &= 0xFFFFCFFF;
	FlashConfig = (FlashConfig | (PartNum<<CHIP_SELECT_BIT12)); 

	wrmalt(Adapter,FLASH_GPIO_CONFIG_REG, &GPIOConfig, 4);
	udelay(100);

	wrmalt(Adapter,FLASH_CONFIG_REG, &FlashConfig, 4);
	udelay(100);

	return STATUS_SUCCESS;

}
INT ReadDSDSignature(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL dsd)
{
		UINT uiDSDsig = 0;
		
		


		

		if(dsd != DSD0 && dsd != DSD1 && dsd != DSD2)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"passed section value is not for DSDs");
			return STATUS_FAILURE;
		}
		BcmFlash2xBulkRead(Adapter,
						   &uiDSDsig,
						   dsd,
						   Adapter->psFlash2xCSInfo->OffsetFromDSDStartForDSDHeader + FIELD_OFFSET_IN_HEADER(PDSD_HEADER,DSDImageMagicNumber),
						   SIGNATURE_SIZE);

		uiDSDsig = ntohl(uiDSDsig);
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"DSD SIG :%x", uiDSDsig);

		return uiDSDsig ;
}
INT ReadDSDPriority(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL dsd)
{
	
	unsigned int uiDSDPri = STATUS_FAILURE;
	
	
	if(IsSectionWritable(Adapter,dsd))
	{
		if(ReadDSDSignature(Adapter,dsd)== DSD_IMAGE_MAGIC_NUMBER)
		{
			BcmFlash2xBulkRead(Adapter,
							   &uiDSDPri,
							   dsd,
							   Adapter->psFlash2xCSInfo->OffsetFromDSDStartForDSDHeader +FIELD_OFFSET_IN_HEADER(PDSD_HEADER, DSDImagePriority),
							   4);

			uiDSDPri = ntohl(uiDSDPri);
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"DSD<%x> Priority :%x", dsd, uiDSDPri);

		}
	}
	return uiDSDPri;
}
FLASH2X_SECTION_VAL getHighestPriDSD(PMINI_ADAPTER Adapter)
{
	INT DSDHighestPri = STATUS_FAILURE;
	INT  DsdPri= 0 ;
	FLASH2X_SECTION_VAL HighestPriDSD = 0 ;

	if(IsSectionWritable(Adapter,DSD2))
	{
		DSDHighestPri = ReadDSDPriority(Adapter,DSD2);
		HighestPriDSD = DSD2 ;
	}
	if(IsSectionWritable(Adapter,DSD1))
	{
		 DsdPri = ReadDSDPriority(Adapter,DSD1);
		 if(DSDHighestPri  < DsdPri)
		 {
		 	DSDHighestPri = DsdPri ;
			HighestPriDSD = DSD1;
		 }
	}
	if(IsSectionWritable(Adapter,DSD0))
	{
		 DsdPri = ReadDSDPriority(Adapter,DSD0);
		 if(DSDHighestPri  < DsdPri)
		 {
		 	DSDHighestPri = DsdPri ;
			HighestPriDSD = DSD0;
		 }
	}
	if(HighestPriDSD)
	 	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Highest DSD :%x , and its  Pri :%x", HighestPriDSD, DSDHighestPri);
	return  HighestPriDSD ;
}

INT ReadISOSignature(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL iso)
{
		UINT uiISOsig = 0;
		
		


		

		if(iso != ISO_IMAGE1 && iso != ISO_IMAGE2)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"passed section value is not for ISOs");
			return STATUS_FAILURE;
		}
		BcmFlash2xBulkRead(Adapter,
						   &uiISOsig,
						   iso,
						   0 + FIELD_OFFSET_IN_HEADER(PISO_HEADER,ISOImageMagicNumber),
						   SIGNATURE_SIZE);

		uiISOsig = ntohl(uiISOsig);
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"ISO SIG :%x", uiISOsig);

		return uiISOsig ;
}
INT ReadISOPriority(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL iso)
{

	unsigned int ISOPri = STATUS_FAILURE;
	if(IsSectionWritable(Adapter,iso))
	{
		if(ReadISOSignature(Adapter,iso)== ISO_IMAGE_MAGIC_NUMBER)
		{
			BcmFlash2xBulkRead(Adapter,
							   &ISOPri,
							   iso,
							   0 + FIELD_OFFSET_IN_HEADER(PISO_HEADER, ISOImagePriority),
							   4);

			ISOPri = ntohl(ISOPri);
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"ISO<%x> Priority :%x", iso, ISOPri);

		}
	}
	return ISOPri;
}
FLASH2X_SECTION_VAL getHighestPriISO(PMINI_ADAPTER Adapter)
{
	INT ISOHighestPri = STATUS_FAILURE;
	INT  ISOPri= 0 ;
	FLASH2X_SECTION_VAL HighestPriISO = NO_SECTION_VAL ;

	if(IsSectionWritable(Adapter,ISO_IMAGE2))
	{
		ISOHighestPri = ReadISOPriority(Adapter,ISO_IMAGE2);
		HighestPriISO = ISO_IMAGE2 ;
	}
	if(IsSectionWritable(Adapter,ISO_IMAGE1))
	{
		 ISOPri = ReadISOPriority(Adapter,ISO_IMAGE1);
		 if(ISOHighestPri  < ISOPri)
		 {
			ISOHighestPri = ISOPri ;
			HighestPriISO = ISO_IMAGE1;
		 }
	}
	if(HighestPriISO)
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Highest ISO :%x and its Pri :%x",HighestPriISO,ISOHighestPri);
	return	HighestPriISO ;
}
INT WriteToFlashWithoutSectorErase(PMINI_ADAPTER Adapter,
										PUINT pBuff,
										FLASH2X_SECTION_VAL eFlash2xSectionVal,
										UINT uiOffset,
										UINT uiNumBytes
										)
{
#if !defined(BCM_SHM_INTERFACE) || defined(FLASH_DIRECT_ACCESS)
	UINT uiTemp = 0, value = 0 ;
	UINT i = 0;
	UINT uiPartOffset = 0;
#endif
	UINT uiStartOffset = 0;
	
	INT Status = STATUS_SUCCESS;
	PUCHAR pcBuff = (PUCHAR)pBuff;

	if(uiNumBytes % Adapter->ulFlashWriteSize)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Writing without Sector Erase for non-FlashWriteSize number of bytes 0x%x\n", uiNumBytes);
		return STATUS_FAILURE;
	}

	uiStartOffset = BcmGetSectionValStartOffset(Adapter,eFlash2xSectionVal);

	if(IsSectionExistInVendorInfo(Adapter,eFlash2xSectionVal))
	{
		return vendorextnWriteSectionWithoutErase(Adapter, pcBuff, eFlash2xSectionVal, uiOffset, uiNumBytes);
	}

	uiOffset = uiOffset + uiStartOffset;

#if defined(BCM_SHM_INTERFACE) && !defined(FLASH_DIRECT_ACCESS)
  Status = bcmflash_raw_writenoerase((uiOffset/FLASH_PART_SIZE),(uiOffset % FLASH_PART_SIZE), pcBuff,uiNumBytes);
#else
	rdmalt(Adapter, 0x0f000C80, &uiTemp, sizeof(uiTemp));
	value = 0;
	wrmalt(Adapter, 0x0f000C80,&value, sizeof(value));

	Adapter->SelectedChip = RESET_CHIP_SELECT;
	BcmDoChipSelect(Adapter,uiOffset);
	uiPartOffset = (uiOffset & (FLASH_PART_SIZE - 1)) + GetFlashBaseAddr(Adapter);

	for(i = 0 ; i< uiNumBytes; i += Adapter->ulFlashWriteSize)
	{
		if(Adapter->ulFlashWriteSize == BYTE_WRITE_SUPPORT)
			Status = flashByteWrite(Adapter,uiPartOffset, pcBuff);
		else
			Status = flashWrite(Adapter,uiPartOffset, pcBuff);

		if(Status != STATUS_SUCCESS)
			break;

		pcBuff = pcBuff + Adapter->ulFlashWriteSize;
		uiPartOffset = uiPartOffset +  Adapter->ulFlashWriteSize;
	}
	wrmalt(Adapter, 0x0f000C80, &uiTemp, sizeof(uiTemp));
	Adapter->SelectedChip = RESET_CHIP_SELECT;
#endif

	return Status;
}

BOOLEAN IsSectionExistInFlash(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL section)
{

	BOOLEAN SectionPresent = FALSE ;

	switch(section)
	{

		case ISO_IMAGE1 :
			  if((Adapter->psFlash2xCSInfo->OffsetISOImage1Part1Start != UNINIT_PTR_IN_CS) &&
			  		(IsNonCDLessDevice(Adapter) == FALSE))
				  SectionPresent = TRUE ;
			   break;
		case ISO_IMAGE2 :
				if((Adapter->psFlash2xCSInfo->OffsetISOImage2Part1Start != UNINIT_PTR_IN_CS) &&
					(IsNonCDLessDevice(Adapter) == FALSE))
					 SectionPresent = TRUE ;
			  break;
		case DSD0 :
				if(Adapter->psFlash2xCSInfo->OffsetFromZeroForDSDStart != UNINIT_PTR_IN_CS)
					 SectionPresent = TRUE ;
				break;
		case DSD1 :
				if(Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD1Start != UNINIT_PTR_IN_CS)
					 SectionPresent = TRUE ;
				break;
		case DSD2 :
				if(Adapter->psFlash2xCSInfo->OffsetFromZeroForDSD2Start != UNINIT_PTR_IN_CS)
					 SectionPresent = TRUE ;
				break;
		case VSA0 :
				if(Adapter->psFlash2xCSInfo->OffsetFromZeroForVSAStart != UNINIT_PTR_IN_CS)
					 SectionPresent = TRUE ;
				break;
		case VSA1 :
				if(Adapter->psFlash2xCSInfo->OffsetFromZeroForVSA1Start != UNINIT_PTR_IN_CS)
					 SectionPresent = TRUE ;
				break;
		case VSA2 :
				if(Adapter->psFlash2xCSInfo->OffsetFromZeroForVSA2Start != UNINIT_PTR_IN_CS)
					 SectionPresent = TRUE ;
				break;
		case SCSI :
				if(Adapter->psFlash2xCSInfo->OffsetFromZeroForScsiFirmware != UNINIT_PTR_IN_CS)
					 SectionPresent = TRUE ;
				break;
		case CONTROL_SECTION :
				if(Adapter->psFlash2xCSInfo->OffsetFromZeroForControlSectionStart != UNINIT_PTR_IN_CS)
					 SectionPresent = TRUE ;
				break;
		default :
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Section Does not exist in Flash 2.x");
			SectionPresent =  FALSE;
	}
	return SectionPresent ;
}
INT IsSectionWritable(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL Section)
{
		INT offset = STATUS_FAILURE;
		INT Status = FALSE;
		if(IsSectionExistInFlash(Adapter,Section) == FALSE)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Section <%d> does not exixt", Section);
			return FALSE;
		}
		offset = BcmGetSectionValStartOffset(Adapter,Section);
		if(offset == INVALID_OFFSET)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Section<%d> does not exixt", Section);
			return FALSE;
		}

		if(IsSectionExistInVendorInfo(Adapter,Section))
		{
			return !(Adapter->psFlash2xVendorInfo->VendorSection[Section].AccessFlags & FLASH2X_SECTION_RO);
		}

		Status = IsOffsetWritable(Adapter,offset);
		return Status ;
}

static INT CorruptDSDSig(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL eFlash2xSectionVal)
{

	PUCHAR pBuff = NULL;
	UINT sig = 0;
	UINT uiOffset = 0;
	UINT BlockStatus = 0;
	UINT uiSectAlignAddr = 0;

	Adapter->bSigCorrupted = FALSE;

	if(Adapter->bAllDSDWriteAllow == FALSE)
	{
		if(IsSectionWritable(Adapter,eFlash2xSectionVal) != TRUE)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Section is not Writable...Hence can't Corrupt signature");
			return SECTOR_IS_NOT_WRITABLE;
		}
	}

	pBuff = (PUCHAR)kzalloc(MAX_RW_SIZE, GFP_KERNEL);
	if(pBuff == NULL)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "Can't allocate memorey");
		return -ENOMEM ;
	}

	uiOffset = Adapter->psFlash2xCSInfo->OffsetFromDSDStartForDSDHeader + sizeof(DSD_HEADER);
	uiOffset -= MAX_RW_SIZE ;

	BcmFlash2xBulkRead(Adapter, (PUINT)pBuff,eFlash2xSectionVal,uiOffset,MAX_RW_SIZE);


	sig = *((PUINT)(pBuff +12));
	sig =ntohl(sig);
	BCM_DEBUG_PRINT_BUFFER(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,pBuff,MAX_RW_SIZE);
	
	*(pBuff + 12) = 0;

	if(sig == DSD_IMAGE_MAGIC_NUMBER)
	{
		Adapter->bSigCorrupted = TRUE;
		if(Adapter->ulFlashWriteSize == BYTE_WRITE_SUPPORT)
		{
			uiSectAlignAddr = uiOffset & ~(Adapter->uiSectorSize -1);
			BlockStatus = BcmFlashUnProtectBlock(Adapter,uiSectAlignAddr,Adapter->uiSectorSize);

			WriteToFlashWithoutSectorErase(Adapter,(PUINT)(pBuff + 12),eFlash2xSectionVal,
												(uiOffset + 12),BYTE_WRITE_SUPPORT);
			if(BlockStatus)
			{
				BcmRestoreBlockProtectStatus(Adapter,BlockStatus);
				BlockStatus = 0;
			}
		}
		else
		{
			WriteToFlashWithoutSectorErase(Adapter,(PUINT)pBuff,eFlash2xSectionVal,
												uiOffset ,MAX_RW_SIZE);
		}
	}
	else
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"BCM Signature is not present in header");
		kfree(pBuff);
		return STATUS_FAILURE;
	}

	kfree(pBuff);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Corrupted the signature");
	return STATUS_SUCCESS ;
}

static INT CorruptISOSig(PMINI_ADAPTER Adapter, FLASH2X_SECTION_VAL eFlash2xSectionVal)
{

	PUCHAR pBuff = NULL;
	UINT sig = 0;
	UINT uiOffset = 0;

	Adapter->bSigCorrupted = FALSE;

	if(IsSectionWritable(Adapter,eFlash2xSectionVal) != TRUE)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Section is not Writable...Hence can't Corrupt signature");
		return SECTOR_IS_NOT_WRITABLE;
	}

	pBuff = (PUCHAR)kzalloc(MAX_RW_SIZE, GFP_KERNEL);
	if(pBuff == NULL)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Can't allocate memorey");
		return -ENOMEM ;
	}

	uiOffset = 0;

	BcmFlash2xBulkRead(Adapter, (PUINT)pBuff,eFlash2xSectionVal,uiOffset, MAX_RW_SIZE);

	sig = *((PUINT)pBuff);
	sig =ntohl(sig);

	
	*pBuff = 0;

	if(sig == ISO_IMAGE_MAGIC_NUMBER)
	{
		Adapter->bSigCorrupted = TRUE;
		WriteToFlashWithoutSectorErase(Adapter,(PUINT)pBuff,eFlash2xSectionVal,
											uiOffset ,Adapter->ulFlashWriteSize);
	}
	else
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"BCM Signature is not present in header");
		kfree(pBuff);
		return STATUS_FAILURE;
	}

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,"Corrupted the signature");
	BCM_DEBUG_PRINT_BUFFER(Adapter,DBG_TYPE_OTHERS, NVM_RW, DBG_LVL_ALL,pBuff,MAX_RW_SIZE);

	kfree(pBuff);
	return STATUS_SUCCESS ;
}

BOOLEAN IsNonCDLessDevice(PMINI_ADAPTER Adapter)
{
	if(Adapter->psFlash2xCSInfo->IsCDLessDeviceBootSig == NON_CDLESS_DEVICE_BOOT_SIG)
		return TRUE;
	else
		return FALSE ;
}

