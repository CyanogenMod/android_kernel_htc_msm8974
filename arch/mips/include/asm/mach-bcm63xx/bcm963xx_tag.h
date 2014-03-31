#ifndef __BCM963XX_TAG_H
#define __BCM963XX_TAG_H

#define TAGVER_LEN		4	
#define TAGLAYOUT_LEN		4	
#define SIG1_LEN		20	
#define SIG2_LEN		14	
#define BOARDID_LEN		16	
#define ENDIANFLAG_LEN		2	
#define CHIPID_LEN		6	
#define IMAGE_LEN		10	
#define ADDRESS_LEN		12	
#define DUALFLAG_LEN		2	
#define INACTIVEFLAG_LEN	2	
#define RSASIG_LEN		20	
#define TAGINFO1_LEN		30	
#define FLASHLAYOUTVER_LEN	4	
#define TAGINFO2_LEN		16	
#define ALTTAGINFO_LEN		54	

#define NUM_PIRELLI		2
#define IMAGETAG_CRC_START	0xFFFFFFFF

#define PIRELLI_BOARDS { \
	"AGPF-S0", \
	"DWV-S0", \
}


struct bcm_tag {
	
	char tag_version[TAGVER_LEN];
	
	char sig_1[SIG1_LEN];
	
	char sig_2[SIG2_LEN];
	
	char chip_id[CHIPID_LEN];
	
	char board_id[BOARDID_LEN];
	
	char big_endian[ENDIANFLAG_LEN];
	
	char total_length[IMAGE_LEN];
	
	char cfe__address[ADDRESS_LEN];
	
	char cfe_length[IMAGE_LEN];
	char flash_image_start[ADDRESS_LEN];
	
	char root_length[IMAGE_LEN];
	
	char kernel_address[ADDRESS_LEN];
	
	char kernel_length[IMAGE_LEN];
	
	char dual_image[DUALFLAG_LEN];
	
	char inactive_flag[INACTIVEFLAG_LEN];
	
	char rsa_signature[RSASIG_LEN];
	
	char information1[TAGINFO1_LEN];
	
	char flash_layout_ver[FLASHLAYOUTVER_LEN];
	
	__u32 fskernel_crc;
	
	char information2[TAGINFO2_LEN];
	
	__u32 image_crc;
	
	__u32 rootfs_crc;
	
	__u32 kernel_crc;
	
	char reserved1[8];
	
	__u32 header_crc;
	
	char reserved2[16];
};

#endif 
