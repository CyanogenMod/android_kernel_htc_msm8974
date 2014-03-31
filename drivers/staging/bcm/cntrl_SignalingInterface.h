#ifndef CNTRL_SIGNALING_INTERFACE_
#define CNTRL_SIGNALING_INTERFACE_




#define DSA_REQ 11
#define DSA_RSP 12
#define DSA_ACK 13
#define DSC_REQ 14
#define DSC_RSP 15
#define DSC_ACK 16
#define DSD_REQ 17
#define DSD_RSP 18
#define DSD_ACK 19
#define MAX_CLASSIFIERS_IN_SF  4


#define MAX_STRING_LEN 20
#define MAX_PHS_LENGTHS 255
#define VENDOR_PHS_PARAM_LENGTH 10
#define MAX_NUM_ACTIVE_BS 10
#define AUTH_TOKEN_LENGTH	10
#define NUM_HARQ_CHANNELS	16	
#define VENDOR_CLASSIFIER_PARAM_LENGTH 1 
#define  VENDOR_SPECIF_QOS_PARAM 1
#define VENDOR_PHS_PARAM_LENGTH	10
#define MBS_CONTENTS_ID_LENGTH	10
#define GLOBAL_SF_CLASSNAME_LENGTH 6

#define TYPE_OF_SERVICE_LENGTH				3
#define IP_MASKED_SRC_ADDRESS_LENGTH			32
#define IP_MASKED_DEST_ADDRESS_LENGTH		32
#define PROTOCOL_SRC_PORT_RANGE_LENGTH		4
#define PROTOCOL_DEST_PORT_RANGE_LENGTH		4
#define ETHERNET_DEST_MAC_ADDR_LENGTH		12
#define ETHERNET_SRC_MAC_ADDR_LENGTH		12
#define NUM_ETHERTYPE_BYTES  3
#define NUM_IPV6_FLOWLABLE_BYTES 3


struct _stCPacketClassificationRuleSI{

	
    B_UINT16                        u16UserPriority;
	
    B_UINT16                        u16VLANID;
	
    B_UINT16                        u16PacketClassificationRuleIndex;
	
    B_UINT8                         u8ClassifierRulePriority;
	
	B_UINT8                         u8IPTypeOfServiceLength;
	
    B_UINT8                         u8IPTypeOfService[TYPE_OF_SERVICE_LENGTH];
	
    B_UINT8                         u8Protocol;
	
    B_UINT8                         u8IPMaskedSourceAddressLength;
	
    B_UINT8                         u8IPMaskedSourceAddress[IP_MASKED_SRC_ADDRESS_LENGTH];
	
    B_UINT8                         u8IPDestinationAddressLength;
	
    B_UINT8                         u8IPDestinationAddress[IP_MASKED_DEST_ADDRESS_LENGTH];
	
    B_UINT8                         u8ProtocolSourcePortRangeLength;
	
    B_UINT8                         u8ProtocolSourcePortRange[PROTOCOL_SRC_PORT_RANGE_LENGTH];
	
    B_UINT8                         u8ProtocolDestPortRangeLength;
	
    B_UINT8                         u8ProtocolDestPortRange[PROTOCOL_DEST_PORT_RANGE_LENGTH];
	
    B_UINT8                         u8EthernetDestMacAddressLength;
	
    B_UINT8                         u8EthernetDestMacAddress[ETHERNET_DEST_MAC_ADDR_LENGTH];
	
    B_UINT8                         u8EthernetSourceMACAddressLength;
	
    B_UINT8                         u8EthernetSourceMACAddress[ETHERNET_SRC_MAC_ADDR_LENGTH];
	
	B_UINT8                         u8EthertypeLength;
	
    B_UINT8                         u8Ethertype[NUM_ETHERTYPE_BYTES];
	
    B_UINT8                         u8AssociatedPHSI;
	
    B_UINT8                         u8VendorSpecificClassifierParamLength;
	
    B_UINT8                         u8VendorSpecificClassifierParam[VENDOR_CLASSIFIER_PARAM_LENGTH];
    
    B_UINT8                         u8IPv6FlowLableLength;
	
    B_UINT8                         u8IPv6FlowLable[NUM_IPV6_FLOWLABLE_BYTES];
	
    B_UINT8							u8ClassifierActionRule;
    B_UINT16							u16ValidityBitMap;
};
typedef struct _stCPacketClassificationRuleSI CCPacketClassificationRuleSI,stCPacketClassificationRuleSI, *pstCPacketClassificationRuleSI;

typedef struct _stPhsRuleSI {
	
    B_UINT8                         u8PHSI;
	
    B_UINT8                         u8PHSFLength;
    
    B_UINT8                         u8PHSF[MAX_PHS_LENGTHS];
	
    B_UINT8                         u8PHSMLength;
	
    B_UINT8                         u8PHSM[MAX_PHS_LENGTHS];
	
    B_UINT8                         u8PHSS;
	
    B_UINT8                         u8PHSV;
	
    B_UINT8                         u8VendorSpecificPHSParamsLength;
	
    B_UINT8                         u8VendorSpecificPHSParams[VENDOR_PHS_PARAM_LENGTH];

	B_UINT8                         u8Padding[2];
}stPhsRuleSI,*pstPhsRuleSI;
typedef stPhsRuleSI CPhsRuleSI;

struct _stConvergenceSLTypes{
	
    B_UINT8                         u8ClassfierDSCAction;
	
    B_UINT8                         u8PhsDSCAction;
	
    B_UINT8                         u8Padding[2];
    
    stCPacketClassificationRuleSI     cCPacketClassificationRule;
    
     struct _stPhsRuleSI		cPhsRule;
};
typedef struct _stConvergenceSLTypes stConvergenceSLTypes,CConvergenceSLTypes, *pstConvergenceSLTypes;


typedef struct _stServiceFlowParamSI{

     
    B_UINT32                        u32SFID;

     
    B_UINT32                        u32MaxSustainedTrafficRate;

     
    B_UINT32                        u32MaxTrafficBurst;

    
    B_UINT32                        u32MinReservedTrafficRate;

	
    	B_UINT32                        u32ToleratedJitter;

   
    B_UINT32                        u32MaximumLatency;

	
    B_UINT16                        u16CID;

     
    B_UINT16                        u16TargetSAID;

	
    B_UINT16                        u16ARQWindowSize;

     
    B_UINT16                        u16ARQRetryTxTimeOut;

	
    B_UINT16                        u16ARQRetryRxTimeOut;

	
    B_UINT16                        u16ARQBlockLifeTime;

	
    B_UINT16                        u16ARQSyncLossTimeOut;

	 
    B_UINT16                        u16ARQRxPurgeTimeOut;
    
    B_UINT16                        u16ARQBlockSize;

	
	B_UINT16                        u16SDUInterArrivalTime;

	
	B_UINT16                        u16TimeBase;

	 
	B_UINT16                        u16UnsolicitedGrantInterval;

	
	B_UINT16						u16UnsolicitedPollingInterval;

	 
	B_UINT16						u16MacOverhead;

	 
	B_UINT16						u16MBSContentsID[MBS_CONTENTS_ID_LENGTH];

	
	B_UINT8							u8MBSContentsIDLength;

	
    B_UINT8                         u8ServiceClassNameLength;

	
    B_UINT8                         u8ServiceClassName[32];

	
	B_UINT8							u8MBSService;

    
    B_UINT8                         u8QosParamSet;

   
    B_UINT8                         u8TrafficPriority;

   
    B_UINT8                         u8ServiceFlowSchedulingType;

  
    B_UINT8							u8RequesttransmissionPolicy;

	
    B_UINT8                         u8FixedLengthVSVariableLengthSDUIndicator;

	
	B_UINT8                         u8SDUSize;

	 
       B_UINT8                         u8ARQEnable;

	
       B_UINT8                         u8ARQDeliverInOrder;

	
	B_UINT8                         u8RxARQAckProcessingTime;

	
       B_UINT8                         u8CSSpecification;

	 
	B_UINT8                         u8TypeOfDataDeliveryService;

	
	B_UINT8                         u8PagingPreference;

	
       B_UINT8                         u8MBSZoneIdentifierassignment;

       
	B_UINT8                         u8TrafficIndicationPreference;

	
	B_UINT8                         u8GlobalServicesClassNameLength;

	 
	B_UINT8                         u8GlobalServicesClassName[GLOBAL_SF_CLASSNAME_LENGTH];

	 
	B_UINT8                         u8SNFeedbackEnabled;

	 
	B_UINT8                         u8FSNSize;

	
	B_UINT8							u8CIDAllocation4activeBSsLength;

	
	B_UINT8							u8CIDAllocation4activeBSs[MAX_NUM_ACTIVE_BS];

	 
	B_UINT8                         u8PDUSNExtendedSubheader4HarqReordering;

	 
	B_UINT8                         u8HARQServiceFlows;

	
	B_UINT8							u8AuthTokenLength;

	
	B_UINT8							u8AuthToken[AUTH_TOKEN_LENGTH];

	
	B_UINT8							u8HarqChannelMappingLength;

	 
	B_UINT8							u8HARQChannelMapping[NUM_HARQ_CHANNELS];

	
    B_UINT8                         u8VendorSpecificQoSParamLength;

	
    B_UINT8                          u8VendorSpecificQoSParam[VENDOR_SPECIF_QOS_PARAM];

	
	B_UINT8                         u8TotalClassifiers;  
	B_UINT8							bValid;	
	B_UINT8				u8Padding;	 

	stConvergenceSLTypes		cConvergenceSLTypes[MAX_CLASSIFIERS_IN_SF];

} stServiceFlowParamSI, *pstServiceFlowParamSI;
typedef stServiceFlowParamSI CServiceFlowParamSI;

typedef struct _stLocalSFAddRequest{

	B_UINT8                         u8Type;	
	B_UINT8      eConnectionDir;		
	
	B_UINT16                        u16TID;	
	
    	B_UINT16                        u16CID;	
	
	B_UINT16                        u16VCID;	
    

	stServiceFlowParamSI	*psfParameterSet;	

}stLocalSFAddRequest, *pstLocalSFAddRequest;


typedef struct _stLocalSFAddIndication{

	B_UINT8                         u8Type;	
	B_UINT8      eConnectionDir;	
	
	B_UINT16                         u16TID;	
    
    B_UINT16                        u16CID;		
    
    B_UINT16                        u16VCID;	 


    
    
    stServiceFlowParamSI              *psfAuthorizedSet;	
    
    stServiceFlowParamSI              *psfAdmittedSet;	
    
    stServiceFlowParamSI              *psfActiveSet;	
	B_UINT8				   u8CC;	
	B_UINT8				   u8Padd;		

    B_UINT16               u16Padd;	

}stLocalSFAddIndication;


typedef struct _stLocalSFAddIndication *pstLocalSFAddIndication;
typedef struct _stLocalSFAddIndication stLocalSFChangeRequest, *pstLocalSFChangeRequest;
typedef struct _stLocalSFAddIndication stLocalSFChangeIndication, *pstLocalSFChangeIndication;

typedef struct _stLocalSFDeleteRequest{
	B_UINT8                         u8Type;	 
	B_UINT8                         u8Padding;	 
	B_UINT16			u16TID;		 
    
    B_UINT32                        u32SFID;	 
}stLocalSFDeleteRequest, *pstLocalSFDeleteRequest;

typedef struct stLocalSFDeleteIndication{
	B_UINT8                         u8Type;	
	B_UINT8                         u8Padding;	
	B_UINT16			u16TID;			
       
    B_UINT16                        u16CID;			
    
    B_UINT16                        u16VCID;		
    
    B_UINT32                        u32SFID; 		
	
	B_UINT8                         u8ConfirmationCode;	
	B_UINT8                         u8Padding1[3];		
}stLocalSFDeleteIndication;

typedef struct _stIM_SFHostNotify
{
	B_UINT32 	SFID;      
	B_UINT16 	newCID;   
	B_UINT16 	VCID;             
	B_UINT8  	RetainSF;        
	B_UINT8 	QoSParamSet; 
	B_UINT16 	u16reserved;  

} stIM_SFHostNotify;

#endif
