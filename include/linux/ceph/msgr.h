#ifndef CEPH_MSGR_H
#define CEPH_MSGR_H


#define CEPH_MON_PORT    6789  

#define CEPH_PORT_FIRST  6789
#define CEPH_PORT_START  6800  
#define CEPH_PORT_LAST   6900

#define CEPH_BANNER "ceph v027"
#define CEPH_BANNER_MAX_LEN 30


typedef __u32 ceph_seq_t;

static inline __s32 ceph_seq_cmp(__u32 a, __u32 b)
{
       return (__s32)a - (__s32)b;
}


struct ceph_entity_name {
	__u8 type;      
	__le64 num;
} __attribute__ ((packed));

#define CEPH_ENTITY_TYPE_MON    0x01
#define CEPH_ENTITY_TYPE_MDS    0x02
#define CEPH_ENTITY_TYPE_OSD    0x04
#define CEPH_ENTITY_TYPE_CLIENT 0x08
#define CEPH_ENTITY_TYPE_AUTH   0x20

#define CEPH_ENTITY_TYPE_ANY    0xFF

extern const char *ceph_entity_type_name(int type);

struct ceph_entity_addr {
	__le32 type;
	__le32 nonce;  
	struct sockaddr_storage in_addr;
} __attribute__ ((packed));

struct ceph_entity_inst {
	struct ceph_entity_name name;
	struct ceph_entity_addr addr;
} __attribute__ ((packed));


#define CEPH_MSGR_TAG_READY         1  
#define CEPH_MSGR_TAG_RESETSESSION  2  
#define CEPH_MSGR_TAG_WAIT          3  
#define CEPH_MSGR_TAG_RETRY_SESSION 4  
#define CEPH_MSGR_TAG_RETRY_GLOBAL  5  
#define CEPH_MSGR_TAG_CLOSE         6  
#define CEPH_MSGR_TAG_MSG           7  
#define CEPH_MSGR_TAG_ACK           8  
#define CEPH_MSGR_TAG_KEEPALIVE     9  
#define CEPH_MSGR_TAG_BADPROTOVER  10  
#define CEPH_MSGR_TAG_BADAUTHORIZER 11 
#define CEPH_MSGR_TAG_FEATURES      12 


struct ceph_msg_connect {
	__le64 features;     
	__le32 host_type;    
	__le32 global_seq;   
	__le32 connect_seq;  
	__le32 protocol_version;
	__le32 authorizer_protocol;
	__le32 authorizer_len;
	__u8  flags;         
} __attribute__ ((packed));

struct ceph_msg_connect_reply {
	__u8 tag;
	__le64 features;     
	__le32 global_seq;
	__le32 connect_seq;
	__le32 protocol_version;
	__le32 authorizer_len;
	__u8 flags;
} __attribute__ ((packed));

#define CEPH_MSG_CONNECT_LOSSY  1  


struct ceph_msg_header_old {
	__le64 seq;       
	__le64 tid;       
	__le16 type;      
	__le16 priority;  
	__le16 version;   

	__le32 front_len; 
	__le32 middle_len;
	__le32 data_len;  
	__le16 data_off;  

	struct ceph_entity_inst src, orig_src;
	__le32 reserved;
	__le32 crc;       
} __attribute__ ((packed));

struct ceph_msg_header {
	__le64 seq;       
	__le64 tid;       
	__le16 type;      
	__le16 priority;  
	__le16 version;   

	__le32 front_len; 
	__le32 middle_len;
	__le32 data_len;  
	__le16 data_off;  

	struct ceph_entity_name src;
	__le32 reserved;
	__le32 crc;       
} __attribute__ ((packed));

#define CEPH_MSG_PRIO_LOW     64
#define CEPH_MSG_PRIO_DEFAULT 127
#define CEPH_MSG_PRIO_HIGH    196
#define CEPH_MSG_PRIO_HIGHEST 255

struct ceph_msg_footer {
	__le32 front_crc, middle_crc, data_crc;
	__u8 flags;
} __attribute__ ((packed));

#define CEPH_MSG_FOOTER_COMPLETE  (1<<0)   
#define CEPH_MSG_FOOTER_NOCRC     (1<<1)   


#endif
