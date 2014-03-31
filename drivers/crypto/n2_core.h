#ifndef _N2_CORE_H
#define _N2_CORE_H

#ifndef __ASSEMBLY__

struct ino_blob {
	u64			intr;
	u64			ino;
};

struct spu_mdesc_info {
	u64			cfg_handle;
	struct ino_blob		*ino_table;
	int			num_intrs;
};

struct n2_crypto {
	struct spu_mdesc_info	cwq_info;
	struct list_head	cwq_list;
};

struct n2_mau {
	struct spu_mdesc_info	mau_info;
	struct list_head	mau_list;
};

#define CWQ_ENTRY_SIZE		64
#define CWQ_NUM_ENTRIES		64

#define MAU_ENTRY_SIZE		64
#define MAU_NUM_ENTRIES		64

struct cwq_initial_entry {
	u64			control;
	u64			src_addr;
	u64			auth_key_addr;
	u64			auth_iv_addr;
	u64			final_auth_state_addr;
	u64			enc_key_addr;
	u64			enc_iv_addr;
	u64			dest_addr;
};

struct cwq_ext_entry {
	u64			len;
	u64			src_addr;
	u64			resv1;
	u64			resv2;
	u64			resv3;
	u64			resv4;
	u64			resv5;
	u64			resv6;
};

struct cwq_final_entry {
	u64			control;
	u64			src_addr;
	u64			resv1;
	u64			resv2;
	u64			resv3;
	u64			resv4;
	u64			resv5;
	u64			resv6;
};

#define CONTROL_LEN			0x000000000000ffffULL
#define CONTROL_LEN_SHIFT		0
#define CONTROL_HMAC_KEY_LEN		0x0000000000ff0000ULL
#define CONTROL_HMAC_KEY_LEN_SHIFT	16
#define CONTROL_ENC_TYPE		0x00000000ff000000ULL
#define CONTROL_ENC_TYPE_SHIFT		24
#define  ENC_TYPE_ALG_RC4_STREAM	0x00ULL
#define  ENC_TYPE_ALG_RC4_NOSTREAM	0x04ULL
#define  ENC_TYPE_ALG_DES		0x08ULL
#define  ENC_TYPE_ALG_3DES		0x0cULL
#define  ENC_TYPE_ALG_AES128		0x10ULL
#define  ENC_TYPE_ALG_AES192		0x14ULL
#define  ENC_TYPE_ALG_AES256		0x18ULL
#define  ENC_TYPE_ALG_RESERVED		0x1cULL
#define  ENC_TYPE_ALG_MASK		0x1cULL
#define  ENC_TYPE_CHAINING_ECB		0x00ULL
#define  ENC_TYPE_CHAINING_CBC		0x01ULL
#define  ENC_TYPE_CHAINING_CFB		0x02ULL
#define  ENC_TYPE_CHAINING_COUNTER	0x03ULL
#define  ENC_TYPE_CHAINING_MASK		0x03ULL
#define CONTROL_AUTH_TYPE		0x0000001f00000000ULL
#define CONTROL_AUTH_TYPE_SHIFT		32
#define  AUTH_TYPE_RESERVED		0x00ULL
#define  AUTH_TYPE_MD5			0x01ULL
#define  AUTH_TYPE_SHA1			0x02ULL
#define  AUTH_TYPE_SHA256		0x03ULL
#define  AUTH_TYPE_CRC32		0x04ULL
#define  AUTH_TYPE_HMAC_MD5		0x05ULL
#define  AUTH_TYPE_HMAC_SHA1		0x06ULL
#define  AUTH_TYPE_HMAC_SHA256		0x07ULL
#define  AUTH_TYPE_TCP_CHECKSUM		0x08ULL
#define  AUTH_TYPE_SSL_HMAC_MD5		0x09ULL
#define  AUTH_TYPE_SSL_HMAC_SHA1	0x0aULL
#define  AUTH_TYPE_SSL_HMAC_SHA256	0x0bULL
#define CONTROL_STRAND			0x000000e000000000ULL
#define CONTROL_STRAND_SHIFT		37
#define CONTROL_HASH_LEN		0x0000ff0000000000ULL
#define CONTROL_HASH_LEN_SHIFT		40
#define CONTROL_INTERRUPT		0x0001000000000000ULL
#define CONTROL_STORE_FINAL_AUTH_STATE	0x0002000000000000ULL
#define CONTROL_RESERVED		0x001c000000000000ULL
#define CONTROL_HV_DONE			0x0004000000000000ULL
#define CONTROL_HV_PROTOCOL_ERROR	0x0008000000000000ULL
#define CONTROL_HV_HARDWARE_ERROR	0x0010000000000000ULL
#define CONTROL_END_OF_BLOCK		0x0020000000000000ULL
#define CONTROL_START_OF_BLOCK		0x0040000000000000ULL
#define CONTROL_ENCRYPT			0x0080000000000000ULL
#define CONTROL_OPCODE			0xff00000000000000ULL
#define CONTROL_OPCODE_SHIFT		56
#define  OPCODE_INPLACE_BIT		0x80ULL
#define  OPCODE_SSL_KEYBLOCK		0x10ULL
#define  OPCODE_COPY			0x20ULL
#define  OPCODE_ENCRYPT			0x40ULL
#define  OPCODE_AUTH_MAC		0x41ULL

#endif 

#define HV_NCS_QTYPE_MAU		0x01
#define HV_NCS_QTYPE_CWQ		0x02

#define HV_FAST_NCS_QCONF		0x111

#define HV_FAST_NCS_QINFO		0x112

#define HV_FAST_NCS_GETHEAD		0x113

#define HV_FAST_NCS_GETTAIL		0x114

#define HV_FAST_NCS_SETTAIL		0x115

#define HV_FAST_NCS_QHANDLE_TO_DEVINO	0x116

#define HV_FAST_NCS_SETHEAD_MARKER	0x117

#ifndef __ASSEMBLY__
extern unsigned long sun4v_ncs_qconf(unsigned long queue_type,
				     unsigned long queue_ra,
				     unsigned long num_entries,
				     unsigned long *qhandle);
extern unsigned long sun4v_ncs_qinfo(unsigned long qhandle,
				     unsigned long *queue_type,
				     unsigned long *queue_ra,
				     unsigned long *num_entries);
extern unsigned long sun4v_ncs_gethead(unsigned long qhandle,
				       unsigned long *head);
extern unsigned long sun4v_ncs_gettail(unsigned long qhandle,
				       unsigned long *tail);
extern unsigned long sun4v_ncs_settail(unsigned long qhandle,
				       unsigned long tail);
extern unsigned long sun4v_ncs_qhandle_to_devino(unsigned long qhandle,
						 unsigned long *devino);
extern unsigned long sun4v_ncs_sethead_marker(unsigned long qhandle,
					      unsigned long head);
#endif 

#endif 
