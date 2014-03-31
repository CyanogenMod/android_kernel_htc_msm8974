/*
 * 802.1x EAPOL definitions
 *
 * See
 * IEEE Std 802.1X-2001
 * IEEE 802.1X RADIUS Usage Guidelines
 *
 * Copyright (C) 2002 Broadcom Corporation
 *
 * $Id: eapol.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _eapol_h_
#define _eapol_h_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

#include <packed_section_start.h>

#include <bcmcrypto/aeskeywrap.h>

typedef BWL_PRE_PACKED_STRUCT struct {
	struct ether_header eth;	
	unsigned char version;		
	unsigned char type;		
	unsigned short length;		
	unsigned char body[1];		
} BWL_POST_PACKED_STRUCT eapol_header_t;

#define EAPOL_HEADER_LEN 18

typedef struct {
	unsigned char version;		
	unsigned char type;		
	unsigned short length;		
} eapol_hdr_t;

#define EAPOL_HDR_LEN 4

#define WPA2_EAPOL_VERSION	2
#define WPA_EAPOL_VERSION	1
#define LEAP_EAPOL_VERSION	1
#define SES_EAPOL_VERSION	1

#define EAP_PACKET		0
#define EAPOL_START		1
#define EAPOL_LOGOFF		2
#define EAPOL_KEY		3
#define EAPOL_ASF		4

#define EAPOL_RC4_KEY		1
#define EAPOL_WPA2_KEY		2	
#define EAPOL_WPA_KEY		254	

#define EAPOL_KEY_REPLAY_LEN	8
#define EAPOL_KEY_IV_LEN	16
#define EAPOL_KEY_SIG_LEN	16

typedef BWL_PRE_PACKED_STRUCT struct {
	unsigned char type;			
	unsigned short length;			
	unsigned char replay[EAPOL_KEY_REPLAY_LEN];	
	unsigned char iv[EAPOL_KEY_IV_LEN];		
	unsigned char index;				
	unsigned char signature[EAPOL_KEY_SIG_LEN];	
	unsigned char key[1];				
} BWL_POST_PACKED_STRUCT eapol_key_header_t;

#define EAPOL_KEY_HEADER_LEN 	44

#define EAPOL_KEY_FLAGS_MASK	0x80
#define EAPOL_KEY_BROADCAST	0
#define EAPOL_KEY_UNICAST	0x80

#define EAPOL_KEY_INDEX_MASK	0x7f

#define EAPOL_WPA_KEY_REPLAY_LEN	8
#define EAPOL_WPA_KEY_NONCE_LEN		32
#define EAPOL_WPA_KEY_IV_LEN		16
#define EAPOL_WPA_KEY_RSC_LEN		8
#define EAPOL_WPA_KEY_ID_LEN		8
#define EAPOL_WPA_KEY_MIC_LEN		16
#define EAPOL_WPA_KEY_DATA_LEN		(EAPOL_WPA_MAX_KEY_SIZE + AKW_BLOCK_LEN)
#define EAPOL_WPA_MAX_KEY_SIZE		32

typedef BWL_PRE_PACKED_STRUCT struct {
	unsigned char type;		
	unsigned short key_info;	
	unsigned short key_len;		
	unsigned char replay[EAPOL_WPA_KEY_REPLAY_LEN];	
	unsigned char nonce[EAPOL_WPA_KEY_NONCE_LEN];	
	unsigned char iv[EAPOL_WPA_KEY_IV_LEN];		
	unsigned char rsc[EAPOL_WPA_KEY_RSC_LEN];	
	unsigned char id[EAPOL_WPA_KEY_ID_LEN];		
	unsigned char mic[EAPOL_WPA_KEY_MIC_LEN];	
	unsigned short data_len;			
	unsigned char data[EAPOL_WPA_KEY_DATA_LEN];	
} BWL_POST_PACKED_STRUCT eapol_wpa_key_header_t;

#define EAPOL_WPA_KEY_LEN 		95

#define WPA_KEY_DESC_V1		0x01
#define WPA_KEY_DESC_V2		0x02
#define WPA_KEY_DESC_V3		0x03
#define WPA_KEY_PAIRWISE	0x08
#define WPA_KEY_INSTALL		0x40
#define WPA_KEY_ACK		0x80
#define WPA_KEY_MIC		0x100
#define WPA_KEY_SECURE		0x200
#define WPA_KEY_ERROR		0x400
#define WPA_KEY_REQ		0x800

#define WPA_KEY_DESC_V2_OR_V3 WPA_KEY_DESC_V2

#define WPA_KEY_INDEX_0		0x00
#define WPA_KEY_INDEX_1		0x10
#define WPA_KEY_INDEX_2		0x20
#define WPA_KEY_INDEX_3		0x30
#define WPA_KEY_INDEX_MASK	0x30
#define WPA_KEY_INDEX_SHIFT	0x04

#define WPA_KEY_ENCRYPTED_DATA	0x1000

typedef BWL_PRE_PACKED_STRUCT struct {
	uint8 type;
	uint8 length;
	uint8 oui[3];
	uint8 subtype;
	uint8 data[1];
} BWL_POST_PACKED_STRUCT eapol_wpa2_encap_data_t;

#define EAPOL_WPA2_ENCAP_DATA_HDR_LEN 	6

#define WPA2_KEY_DATA_SUBTYPE_GTK	1
#define WPA2_KEY_DATA_SUBTYPE_STAKEY	2
#define WPA2_KEY_DATA_SUBTYPE_MAC	3
#define WPA2_KEY_DATA_SUBTYPE_PMKID	4
#define WPA2_KEY_DATA_SUBTYPE_IGTK	9

typedef BWL_PRE_PACKED_STRUCT struct {
	uint8	flags;
	uint8	reserved;
	uint8	gtk[EAPOL_WPA_MAX_KEY_SIZE];
} BWL_POST_PACKED_STRUCT eapol_wpa2_key_gtk_encap_t;

#define EAPOL_WPA2_KEY_GTK_ENCAP_HDR_LEN 	2

#define WPA2_GTK_INDEX_MASK	0x03
#define WPA2_GTK_INDEX_SHIFT	0x00

#define WPA2_GTK_TRANSMIT	0x04

typedef BWL_PRE_PACKED_STRUCT struct {
	uint16	key_id;
	uint8	ipn[6];
	uint8	key[EAPOL_WPA_MAX_KEY_SIZE];
} BWL_POST_PACKED_STRUCT eapol_wpa2_key_igtk_encap_t;

#define EAPOL_WPA2_KEY_IGTK_ENCAP_HDR_LEN 	8

typedef BWL_PRE_PACKED_STRUCT struct {
	uint8	reserved[2];
	uint8	mac[ETHER_ADDR_LEN];
	uint8	stakey[EAPOL_WPA_MAX_KEY_SIZE];
} BWL_POST_PACKED_STRUCT eapol_wpa2_key_stakey_encap_t;

#define WPA2_KEY_DATA_PAD	0xdd


#include <packed_section_end.h>

#endif 
