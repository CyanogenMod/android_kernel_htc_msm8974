#include "headers.h"
INT vendorextnGetSectionInfo(PVOID  pContext,PFLASH2X_VENDORSPECIFIC_INFO pVendorInfo)
{
	return STATUS_FAILURE;
}


INT vendorextnInit(PMINI_ADAPTER Adapter)
{
	return STATUS_SUCCESS;
}


INT vendorextnExit(PMINI_ADAPTER Adapter)
{
	return STATUS_SUCCESS;
}

INT vendorextnIoctl(PMINI_ADAPTER Adapter, UINT cmd, ULONG arg)
{
	return CONTINUE_COMMON_PATH;
}




INT vendorextnReadSection(PVOID  pContext, PUCHAR pBuffer, FLASH2X_SECTION_VAL SectionVal,
			UINT offset, UINT numOfBytes)
{
	return STATUS_FAILURE;
}



//		bVerify - the Buffer Written should be verified.
INT vendorextnWriteSection(PVOID  pContext, PUCHAR pBuffer, FLASH2X_SECTION_VAL SectionVal,
			UINT offset, UINT numOfBytes, BOOLEAN bVerify)
{
	return STATUS_FAILURE;
}



INT vendorextnWriteSectionWithoutErase(PVOID  pContext, PUCHAR pBuffer, FLASH2X_SECTION_VAL SectionVal,
			UINT offset, UINT numOfBytes)
{
	return STATUS_FAILURE;
}

