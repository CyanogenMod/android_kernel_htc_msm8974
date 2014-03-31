#ifndef _CM_HOST_H
#define _CM_HOST_H

#pragma once
#pragma pack (push,4)

#define  DSX_MESSAGE_EXCHANGE_BUFFER        0xBF60AC84 
#define  DSX_MESSAGE_EXCHANGE_BUFFER_SIZE   72000 

typedef struct stLocalSFAddRequestAlt{
	B_UINT8                         u8Type;
	B_UINT8      u8Direction;

	B_UINT16                        u16TID;
   
    B_UINT16                        u16CID;
    
    B_UINT16                        u16VCID;


	
    stServiceFlowParamSI              sfParameterSet;

    
}stLocalSFAddRequestAlt;

typedef struct stLocalSFAddIndicationAlt{
    B_UINT8                         u8Type;
	B_UINT8      u8Direction;
	B_UINT16                         u16TID;
    
    B_UINT16                        u16CID;
    
    B_UINT16                        u16VCID;
	
    stServiceFlowParamSI              sfAuthorizedSet;
    
    stServiceFlowParamSI              sfAdmittedSet;
	
    stServiceFlowParamSI              sfActiveSet;

	B_UINT8 						u8CC;	
	B_UINT8 						u8Padd; 	
	B_UINT16						u16Padd;	
}stLocalSFAddIndicationAlt;

typedef struct stLocalSFAddConfirmationAlt{
	B_UINT8                     u8Type;
	B_UINT8      				u8Direction;
	B_UINT16					u16TID;
    
    B_UINT16                        u16CID;
    
    B_UINT16                        u16VCID;
    
    stServiceFlowParamSI              sfAuthorizedSet;
    
    stServiceFlowParamSI              sfAdmittedSet;
    
    stServiceFlowParamSI              sfActiveSet;
}stLocalSFAddConfirmationAlt;


typedef struct stLocalSFChangeRequestAlt{
    B_UINT8                         u8Type;
	B_UINT8      u8Direction;
	B_UINT16					u16TID;
    
    B_UINT16                        u16CID;
    
    B_UINT16                        u16VCID;
    
    stServiceFlowParamSI              sfAuthorizedSet;
    
    stServiceFlowParamSI              sfAdmittedSet;
    
    stServiceFlowParamSI              sfActiveSet;

	B_UINT8 						u8CC;	
	B_UINT8 						u8Padd; 	
	B_UINT16						u16Padd;	

}stLocalSFChangeRequestAlt;

typedef struct stLocalSFChangeConfirmationAlt{
	B_UINT8                         u8Type;
	B_UINT8      					u8Direction;
	B_UINT16						u16TID;
    
    B_UINT16                        u16CID;
    
    B_UINT16                        u16VCID;
    
    stServiceFlowParamSI              sfAuthorizedSet;
    
    stServiceFlowParamSI              sfAdmittedSet;
    
    stServiceFlowParamSI              sfActiveSet;

}stLocalSFChangeConfirmationAlt;

typedef struct stLocalSFChangeIndicationAlt{
	B_UINT8                         u8Type;
		B_UINT8      u8Direction;
	B_UINT16						u16TID;
    
    B_UINT16                        u16CID;
    
    B_UINT16                        u16VCID;
    
    stServiceFlowParamSI              sfAuthorizedSet;
    
    stServiceFlowParamSI              sfAdmittedSet;
    
    stServiceFlowParamSI              sfActiveSet;

	B_UINT8 						u8CC;	
	B_UINT8 						u8Padd; 	
	B_UINT16						u16Padd;	

}stLocalSFChangeIndicationAlt;

ULONG StoreCmControlResponseMessage(PMINI_ADAPTER Adapter,PVOID pvBuffer,UINT *puBufferLength);

INT AllocAdapterDsxBuffer(PMINI_ADAPTER Adapter);

INT FreeAdapterDsxBuffer(PMINI_ADAPTER Adapter);
ULONG SetUpTargetDsxBuffers(PMINI_ADAPTER Adapter);

BOOLEAN CmControlResponseMessage(PMINI_ADAPTER Adapter,PVOID pvBuffer);


#pragma pack (pop)

#endif
