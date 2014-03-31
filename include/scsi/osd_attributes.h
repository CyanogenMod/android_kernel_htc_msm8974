#ifndef __OSD_ATTRIBUTES_H__
#define __OSD_ATTRIBUTES_H__

#include "osd_protocol.h"


#define ATTR_SET(pg, id, l, ptr) \
	{ .attr_page = pg, .attr_id = id, .len = l, .val_ptr = ptr }

#define ATTR_DEF(pg, id, l) ATTR_SET(pg, id, l, NULL)

enum {
	OSD_APAGE_OBJECT_FIRST		= 0x0,
	OSD_APAGE_OBJECT_DIRECTORY	= 0,
	OSD_APAGE_OBJECT_INFORMATION	= 1,
	OSD_APAGE_OBJECT_QUOTAS		= 2,
	OSD_APAGE_OBJECT_TIMESTAMP	= 3,
	OSD_APAGE_OBJECT_COLLECTIONS	= 4,
	OSD_APAGE_OBJECT_SECURITY	= 5,
	OSD_APAGE_OBJECT_LAST		= 0x2fffffff,

	OSD_APAGE_PARTITION_FIRST	= 0x30000000,
	OSD_APAGE_PARTITION_DIRECTORY	= OSD_APAGE_PARTITION_FIRST + 0,
	OSD_APAGE_PARTITION_INFORMATION = OSD_APAGE_PARTITION_FIRST + 1,
	OSD_APAGE_PARTITION_QUOTAS	= OSD_APAGE_PARTITION_FIRST + 2,
	OSD_APAGE_PARTITION_TIMESTAMP	= OSD_APAGE_PARTITION_FIRST + 3,
	OSD_APAGE_PARTITION_ATTR_ACCESS = OSD_APAGE_PARTITION_FIRST + 4,
	OSD_APAGE_PARTITION_SECURITY	= OSD_APAGE_PARTITION_FIRST + 5,
	OSD_APAGE_PARTITION_LAST	= 0x5FFFFFFF,

	OSD_APAGE_COLLECTION_FIRST	= 0x60000000,
	OSD_APAGE_COLLECTION_DIRECTORY	= OSD_APAGE_COLLECTION_FIRST + 0,
	OSD_APAGE_COLLECTION_INFORMATION = OSD_APAGE_COLLECTION_FIRST + 1,
	OSD_APAGE_COLLECTION_TIMESTAMP	= OSD_APAGE_COLLECTION_FIRST + 3,
	OSD_APAGE_COLLECTION_SECURITY	= OSD_APAGE_COLLECTION_FIRST + 5,
	OSD_APAGE_COLLECTION_LAST	= 0x8FFFFFFF,

	OSD_APAGE_ROOT_FIRST		= 0x90000000,
	OSD_APAGE_ROOT_DIRECTORY	= OSD_APAGE_ROOT_FIRST + 0,
	OSD_APAGE_ROOT_INFORMATION	= OSD_APAGE_ROOT_FIRST + 1,
	OSD_APAGE_ROOT_QUOTAS		= OSD_APAGE_ROOT_FIRST + 2,
	OSD_APAGE_ROOT_TIMESTAMP	= OSD_APAGE_ROOT_FIRST + 3,
	OSD_APAGE_ROOT_SECURITY		= OSD_APAGE_ROOT_FIRST + 5,
	OSD_APAGE_ROOT_LAST		= 0xBFFFFFFF,

	OSD_APAGE_RESERVED_TYPE_FIRST	= 0xC0000000,
	OSD_APAGE_RESERVED_TYPE_LAST	= 0xEFFFFFFF,

	OSD_APAGE_COMMON_FIRST		= 0xF0000000,
	OSD_APAGE_COMMON_LAST		= 0xFFFFFFFD,

	OSD_APAGE_CURRENT_COMMAND	= 0xFFFFFFFE,

	OSD_APAGE_REQUEST_ALL		= 0xFFFFFFFF,
};

enum {
	OSD_APAGE_STD_FIRST		= 0x0,
	OSD_APAGE_STD_DIRECTORY		= 0,
	OSD_APAGE_STD_INFORMATION	= 1,
	OSD_APAGE_STD_QUOTAS		= 2,
	OSD_APAGE_STD_TIMESTAMP		= 3,
	OSD_APAGE_STD_COLLECTIONS	= 4,
	OSD_APAGE_STD_POLICY_SECURITY	= 5,
	OSD_APAGE_STD_LAST		= 0x0000007F,

	OSD_APAGE_RESERVED_FIRST	= 0x00000080,
	OSD_APAGE_RESERVED_LAST		= 0x00007FFF,

	OSD_APAGE_OTHER_STD_FIRST	= 0x00008000,
	OSD_APAGE_OTHER_STD_LAST	= 0x0000EFFF,

	OSD_APAGE_PUBLIC_FIRST		= 0x0000F000,
	OSD_APAGE_PUBLIC_LAST		= 0x0000FFFF,

	OSD_APAGE_APP_DEFINED_FIRST	= 0x00010000,
	OSD_APAGE_APP_DEFINED_LAST	= 0x1FFFFFFF,

	OSD_APAGE_VENDOR_SPECIFIC_FIRST	= 0x20000000,
	OSD_APAGE_VENDOR_SPECIFIC_LAST	= 0x2FFFFFFF,
};

enum {
	OSD_ATTR_PAGE_IDENTIFICATION = 0, 
};

struct page_identification {
	u8 vendor_identification[8];
	u8 page_identification[32];
}  __packed;

struct osd_attr_page_header {
	__be32 page_number;
	__be32 page_length;
} __packed;

enum {
	OSD_ATTR_RI_OSD_SYSTEM_ID            = 0x3,   
	OSD_ATTR_RI_VENDOR_IDENTIFICATION    = 0x4,   
	OSD_ATTR_RI_PRODUCT_IDENTIFICATION   = 0x5,   
	OSD_ATTR_RI_PRODUCT_MODEL            = 0x6,   
	OSD_ATTR_RI_PRODUCT_REVISION_LEVEL   = 0x7,   
	OSD_ATTR_RI_PRODUCT_SERIAL_NUMBER    = 0x8,   
	OSD_ATTR_RI_OSD_NAME                 = 0x9,   
	OSD_ATTR_RI_MAX_CDB_CONTINUATION_LEN = 0xA,   
	OSD_ATTR_RI_TOTAL_CAPACITY           = 0x80,  
	OSD_ATTR_RI_USED_CAPACITY            = 0x81,  
	OSD_ATTR_RI_NUMBER_OF_PARTITIONS     = 0xC0,  
	OSD_ATTR_RI_CLOCK                    = 0x100, 
	OARI_DEFAULT_ISOLATION_METHOD        = 0X110, 
	OARI_SUPPORTED_ISOLATION_METHODS     = 0X111, 

	OARI_DATA_ATOMICITY_GUARANTEE                   = 0X120,   
	OARI_DATA_ATOMICITY_ALIGNMENT                   = 0X121,   
	OARI_ATTRIBUTES_ATOMICITY_GUARANTEE             = 0X122,   
	OARI_DATA_ATTRIBUTES_ATOMICITY_MULTIPLIER       = 0X123,   

	OARI_MAXIMUM_SNAPSHOTS_COUNT                    = 0X1C1,    
	OARI_MAXIMUM_CLONES_COUNT                       = 0X1C2,    
	OARI_MAXIMUM_BRANCH_DEPTH                       = 0X1CC,    
	OARI_SUPPORTED_OBJECT_DUPLICATION_METHOD_FIRST  = 0X200,    
	OARI_SUPPORTED_OBJECT_DUPLICATION_METHOD_LAST   = 0X2ff,    
	OARI_SUPPORTED_TIME_OF_DUPLICATION_METHOD_FIRST = 0X300,    
	OARI_SUPPORTED_TIME_OF_DUPLICATION_METHOD_LAST  = 0X30F,    
	OARI_SUPPORT_FOR_DUPLICATED_OBJECT_FREEZING     = 0X310,    
	OARI_SUPPORT_FOR_SNAPSHOT_REFRESHING            = 0X311,    
	OARI_SUPPORTED_CDB_CONTINUATION_DESC_TYPE_FIRST = 0X7000001,
	OARI_SUPPORTED_CDB_CONTINUATION_DESC_TYPE_LAST  = 0X700FFFF,
};

enum {
	OSD_ATTR_PI_PARTITION_ID            = 0x1,     
	OSD_ATTR_PI_USERNAME                = 0x9,     
	OSD_ATTR_PI_USED_CAPACITY           = 0x81,    
	OSD_ATTR_PI_USED_CAPACITY_INCREMENT = 0x84,    
	OSD_ATTR_PI_NUMBER_OF_OBJECTS       = 0xC1,    

	OSD_ATTR_PI_ACTUAL_DATA_SPACE                      = 0xD1, 
	OSD_ATTR_PI_RESERVED_DATA_SPACE                    = 0xD2, 
	OSD_ATTR_PI_DEFAULT_SNAPSHOT_DUPLICATION_METHOD    = 0x200,
	OSD_ATTR_PI_DEFAULT_CLONE_DUPLICATION_METHOD       = 0x201,
	OSD_ATTR_PI_DEFAULT_SP_TIME_OF_DUPLICATION         = 0x300,
	OSD_ATTR_PI_DEFAULT_CLONE_TIME_OF_DUPLICATION      = 0x301,
};

enum {
	OSD_ATTR_CI_PARTITION_ID           = 0x1,       
	OSD_ATTR_CI_COLLECTION_OBJECT_ID   = 0x2,       
	OSD_ATTR_CI_USERNAME               = 0x9,       
	OSD_ATTR_CI_COLLECTION_TYPE        = 0xA,       
	OSD_ATTR_CI_USED_CAPACITY          = 0x81,      
};

enum {
	OSD_ATTR_OI_PARTITION_ID         = 0x1,       
	OSD_ATTR_OI_OBJECT_ID            = 0x2,       
	OSD_ATTR_OI_USERNAME             = 0x9,       
	OSD_ATTR_OI_USED_CAPACITY        = 0x81,      
	OSD_ATTR_OI_LOGICAL_LENGTH       = 0x82,      
	SD_ATTR_OI_ACTUAL_DATA_SPACE     = 0XD1,      
	SD_ATTR_OI_RESERVED_DATA_SPACE   = 0XD2,      
};

enum {
	OSD_ATTR_RQ_DEFAULT_MAXIMUM_USER_OBJECT_LENGTH     = 0x1,      
	OSD_ATTR_RQ_PARTITION_CAPACITY_QUOTA               = 0x10001,  
	OSD_ATTR_RQ_PARTITION_OBJECT_COUNT                 = 0x10002,  
	OSD_ATTR_RQ_PARTITION_COLLECTIONS_PER_USER_OBJECT  = 0x10081,  
	OSD_ATTR_RQ_PARTITION_COUNT                        = 0x20002,  
};

struct Root_Quotas_attributes_page {
	struct osd_attr_page_header hdr; 
	__be64 default_maximum_user_object_length;
	__be64 partition_capacity_quota;
	__be64 partition_object_count;
	__be64 partition_collections_per_user_object;
	__be64 partition_count;
}  __packed;

enum {
	OSD_ATTR_PQ_DEFAULT_MAXIMUM_USER_OBJECT_LENGTH  = 0x1,        
	OSD_ATTR_PQ_CAPACITY_QUOTA                      = 0x10001,    
	OSD_ATTR_PQ_OBJECT_COUNT                        = 0x10002,    
	OSD_ATTR_PQ_COLLECTIONS_PER_USER_OBJECT         = 0x10081,    
};

struct Partition_Quotas_attributes_page {
	struct osd_attr_page_header hdr; 
	__be64 default_maximum_user_object_length;
	__be64 capacity_quota;
	__be64 object_count;
	__be64 collections_per_user_object;
}  __packed;

enum {
	OSD_ATTR_OQ_MAXIMUM_LENGTH  = 0x1,        
};

struct Object_Quotas_attributes_page {
	struct osd_attr_page_header hdr; 
	__be64 maximum_length;
}  __packed;

enum {
	OSD_ATTR_RT_ATTRIBUTES_ACCESSED_TIME  = 0x2,        
	OSD_ATTR_RT_ATTRIBUTES_MODIFIED_TIME  = 0x3,        
	OSD_ATTR_RT_TIMESTAMP_BYPASS          = 0xFFFFFFFE, 
};

struct root_timestamps_attributes_page {
	struct osd_attr_page_header hdr; 
	struct osd_timestamp attributes_accessed_time;
	struct osd_timestamp attributes_modified_time;
	u8 timestamp_bypass;
}  __packed;

enum {
	OSD_ATTR_PT_CREATED_TIME              = 0x1,        
	OSD_ATTR_PT_ATTRIBUTES_ACCESSED_TIME  = 0x2,        
	OSD_ATTR_PT_ATTRIBUTES_MODIFIED_TIME  = 0x3,        
	OSD_ATTR_PT_DATA_ACCESSED_TIME        = 0x4,        
	OSD_ATTR_PT_DATA_MODIFIED_TIME        = 0x5,        
	OSD_ATTR_PT_TIMESTAMP_BYPASS          = 0xFFFFFFFE, 
};

struct partition_timestamps_attributes_page {
	struct osd_attr_page_header hdr; 
	struct osd_timestamp created_time;
	struct osd_timestamp attributes_accessed_time;
	struct osd_timestamp attributes_modified_time;
	struct osd_timestamp data_accessed_time;
	struct osd_timestamp data_modified_time;
	u8 timestamp_bypass;
}  __packed;

enum {
	OSD_ATTR_OT_CREATED_TIME              = 0x1,        
	OSD_ATTR_OT_ATTRIBUTES_ACCESSED_TIME  = 0x2,        
	OSD_ATTR_OT_ATTRIBUTES_MODIFIED_TIME  = 0x3,        
	OSD_ATTR_OT_DATA_ACCESSED_TIME        = 0x4,        
	OSD_ATTR_OT_DATA_MODIFIED_TIME        = 0x5,        
};

struct object_timestamps_attributes_page {
	struct osd_attr_page_header hdr; 
	struct osd_timestamp created_time;
	struct osd_timestamp attributes_accessed_time;
	struct osd_timestamp attributes_modified_time;
	struct osd_timestamp data_accessed_time;
	struct osd_timestamp data_modified_time;
}  __packed;

struct attributes_access_attr {
	struct osd_attributes_list_attrid attr_list[0];
} __packed;


enum {
	OSD_ATTR_RS_DEFAULT_SECURITY_METHOD           = 0x1,       
	OSD_ATTR_RS_OLDEST_VALID_NONCE_LIMIT          = 0x2,       
	OSD_ATTR_RS_NEWEST_VALID_NONCE_LIMIT          = 0x3,       
	OSD_ATTR_RS_PARTITION_DEFAULT_SECURITY_METHOD = 0x6,       
	OSD_ATTR_RS_SUPPORTED_SECURITY_METHODS        = 0x7,       
	OSD_ATTR_RS_ADJUSTABLE_CLOCK                  = 0x9,       
	OSD_ATTR_RS_MASTER_KEY_IDENTIFIER             = 0x7FFD,    
	OSD_ATTR_RS_ROOT_KEY_IDENTIFIER               = 0x7FFE,    
	OSD_ATTR_RS_SUPPORTED_INTEGRITY_ALGORITHM_0   = 0x80000000,
	OSD_ATTR_RS_SUPPORTED_DH_GROUP_0              = 0x80000010,
};

struct root_security_attributes_page {
	struct osd_attr_page_header hdr; 
	u8 default_security_method;
	u8 partition_default_security_method;
	__be16 supported_security_methods;
	u8 mki_valid_rki_valid;
	struct osd_timestamp oldest_valid_nonce_limit;
	struct osd_timestamp newest_valid_nonce_limit;
	struct osd_timestamp adjustable_clock;
	u8 master_key_identifier[32-25];
	u8 root_key_identifier[39-32];
	u8 supported_integrity_algorithm[16];
	u8 supported_dh_group[16];
}  __packed;

enum {
	OSD_ATTR_PS_DEFAULT_SECURITY_METHOD        = 0x1,        
	OSD_ATTR_PS_OLDEST_VALID_NONCE             = 0x2,        
	OSD_ATTR_PS_NEWEST_VALID_NONCE             = 0x3,        
	OSD_ATTR_PS_REQUEST_NONCE_LIST_DEPTH       = 0x4,        
	OSD_ATTR_PS_FROZEN_WORKING_KEY_BIT_MASK    = 0x5,        
	OSD_ATTR_PS_PARTITION_KEY_IDENTIFIER       = 0x7FFF,     
	OSD_ATTR_PS_WORKING_KEY_IDENTIFIER_FIRST   = 0x8000,     
	OSD_ATTR_PS_WORKING_KEY_IDENTIFIER_LAST    = 0x800F,     
	OSD_ATTR_PS_POLICY_ACCESS_TAG              = 0x40000001, 
	OSD_ATTR_PS_USER_OBJECT_POLICY_ACCESS_TAG  = 0x40000002, 
};

struct partition_security_attributes_page {
	struct osd_attr_page_header hdr; 
	u8 reserved[3];
	u8 default_security_method;
	struct osd_timestamp oldest_valid_nonce;
	struct osd_timestamp newest_valid_nonce;
	__be16 request_nonce_list_depth;
	__be16 frozen_working_key_bit_mask;
	__be32 policy_access_tag;
	__be32 user_object_policy_access_tag;
	u8 pki_valid;
	__be16 wki_00_0f_vld;
	struct osd_key_identifier partition_key_identifier;
	struct osd_key_identifier working_key_identifiers[16];
}  __packed;

enum {
	OSD_ATTR_OS_POLICY_ACCESS_TAG              = 0x40000001, 
};

struct object_security_attributes_page {
	struct osd_attr_page_header hdr; 
	__be32 policy_access_tag;
}  __packed;

enum {
	OSD_ATTR_CC_RESPONSE_INTEGRITY_CHECK_VALUE     = 0x1, 
	OSD_ATTR_CC_OBJECT_TYPE                        = 0x2, 
	OSD_ATTR_CC_PARTITION_ID                       = 0x3, 
	OSD_ATTR_CC_OBJECT_ID                          = 0x4, 
	OSD_ATTR_CC_STARTING_BYTE_ADDRESS_OF_APPEND    = 0x5, 
	OSD_ATTR_CC_CHANGE_IN_USED_CAPACITY            = 0x6, 
};


struct osdv2_current_command_attributes_page {
	struct osd_attr_page_header hdr;  
	u8 response_integrity_check_value[OSD_CRYPTO_KEYID_SIZE];
	u8 object_type;
	u8 reserved[3];
	__be64 partition_id;
	__be64 object_id;
	__be64 starting_byte_address_of_append;
	__be64 change_in_used_capacity;
};

#endif 
