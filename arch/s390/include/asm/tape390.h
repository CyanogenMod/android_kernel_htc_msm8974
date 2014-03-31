/*************************************************************************
 *
 * tape390.h
 *	   enables user programs to display messages and control encryption
 *	   on s390 tape devices
 *
 *	   Copyright IBM Corp. 2001,2006
 *	   Author(s): Michael Holzheu <holzheu@de.ibm.com>
 *
 *************************************************************************/

#ifndef _TAPE390_H
#define _TAPE390_H

#define TAPE390_DISPLAY _IOW('d', 1, struct display_struct)


typedef struct display_struct {
        char cntrl;
        char message1[8];
        char message2[8];
} display_struct;


struct tape390_crypt_info {
	char capability;
	char status;
	char medium_status;
} __attribute__ ((packed));


#define TAPE390_CRYPT_SUPPORTED_MASK 0x01
#define TAPE390_CRYPT_SUPPORTED(x) \
	((x.capability & TAPE390_CRYPT_SUPPORTED_MASK))

#define TAPE390_CRYPT_ON_MASK 0x01
#define TAPE390_CRYPT_ON(x) (((x.status) & TAPE390_CRYPT_ON_MASK))

#define TAPE390_MEDIUM_LOADED_MASK 0x01
#define TAPE390_MEDIUM_ENCRYPTED_MASK 0x02
#define TAPE390_MEDIUM_ENCRYPTED(x) \
	(((x.medium_status) & TAPE390_MEDIUM_ENCRYPTED_MASK))
#define TAPE390_MEDIUM_LOADED(x) \
	(((x.medium_status) & TAPE390_MEDIUM_LOADED_MASK))

#define TAPE390_CRYPT_SET _IOW('d', 2, struct tape390_crypt_info)

#define TAPE390_CRYPT_QUERY _IOR('d', 3, struct tape390_crypt_info)

#define TAPE390_KEKL_TYPE_NONE 0
#define TAPE390_KEKL_TYPE_LABEL 1
#define TAPE390_KEKL_TYPE_HASH 2

struct tape390_kekl {
	unsigned char type;
	unsigned char type_on_tape;
	char label[65];
} __attribute__ ((packed));

struct tape390_kekl_pair {
	struct tape390_kekl kekl[2];
} __attribute__ ((packed));

#define TAPE390_KEKL_SET _IOW('d', 4, struct tape390_kekl_pair)

#define TAPE390_KEKL_QUERY _IOR('d', 5, struct tape390_kekl_pair)

#endif 
