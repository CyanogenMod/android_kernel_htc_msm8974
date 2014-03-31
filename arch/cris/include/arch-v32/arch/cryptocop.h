
#ifndef CRYPTOCOP_H
#define CRYPTOCOP_H

#include <linux/uio.h>


#define CRYPTOCOP_SESSION_ID_NONE (0)

typedef unsigned long long int cryptocop_session_id;

#define ETRAXCRYPTOCOP_IOCTYPE         (250)

#define CRYPTOCOP_IO_CREATE_SESSION    _IOWR(ETRAXCRYPTOCOP_IOCTYPE, 1, struct strcop_session_op)
#define CRYPTOCOP_IO_CLOSE_SESSION     _IOW(ETRAXCRYPTOCOP_IOCTYPE, 2, struct strcop_session_op)
#define CRYPTOCOP_IO_PROCESS_OP        _IOWR(ETRAXCRYPTOCOP_IOCTYPE, 3, struct strcop_crypto_op)
#define CRYPTOCOP_IO_MAXNR             (3)

typedef enum {
	cryptocop_cipher_des = 0,
	cryptocop_cipher_3des = 1,
	cryptocop_cipher_aes = 2,
	cryptocop_cipher_m2m = 3, 
	cryptocop_cipher_none
} cryptocop_cipher_type;

typedef enum {
	cryptocop_digest_sha1 = 0,
	cryptocop_digest_md5 = 1,
	cryptocop_digest_none
} cryptocop_digest_type;

typedef enum {
	cryptocop_csum_le = 0,
	cryptocop_csum_be = 1,
	cryptocop_csum_none
} cryptocop_csum_type;

typedef enum {
	cryptocop_cipher_mode_ecb = 0,
	cryptocop_cipher_mode_cbc,
	cryptocop_cipher_mode_none
} cryptocop_cipher_mode;

typedef enum {
	cryptocop_3des_eee = 0,
	cryptocop_3des_eed = 1,
	cryptocop_3des_ede = 2,
	cryptocop_3des_edd = 3,
	cryptocop_3des_dee = 4,
	cryptocop_3des_ded = 5,
	cryptocop_3des_dde = 6,
	cryptocop_3des_ddd = 7
} cryptocop_3des_mode;

struct strcop_session_op{
	cryptocop_session_id    ses_id;

	cryptocop_cipher_type   cipher; 

	cryptocop_cipher_mode   cmode; 
	cryptocop_3des_mode     des3_mode;

	cryptocop_digest_type   digest; 

	cryptocop_csum_type     csum;   

	unsigned char           *key;
	size_t                  keylen;
};

#define CRYPTOCOP_CSUM_LENGTH         (2)
#define CRYPTOCOP_MAX_DIGEST_LENGTH   (20)  
#define CRYPTOCOP_MAX_IV_LENGTH       (16)  
#define CRYPTOCOP_MAX_KEY_LENGTH      (32)

struct strcop_crypto_op{
	cryptocop_session_id ses_id;

	
	unsigned char            *indata;
	size_t                   inlen; 

	
	unsigned char            do_cipher:1;
	unsigned char            decrypt:1; 
	unsigned char            cipher_explicit:1;
	size_t                   cipher_start;
	size_t                   cipher_len;
	unsigned char            cipher_iv[CRYPTOCOP_MAX_IV_LENGTH];
	
	unsigned char            *cipher_outdata;
	size_t                   cipher_outlen;

	
	unsigned char            do_digest:1;
	size_t                   digest_start;
	size_t                   digest_len;
	
	unsigned char            digest[CRYPTOCOP_MAX_DIGEST_LENGTH];

	
	unsigned char            do_csum:1;
	size_t                   csum_start;
	size_t                   csum_len;
	
	unsigned char            csum[CRYPTOCOP_CSUM_LENGTH];
};



#ifdef __KERNEL__


#include <arch/hwregs/dma.h>

typedef enum {
	cryptocop_alg_csum = 0,
	cryptocop_alg_mem2mem,
	cryptocop_alg_md5,
	cryptocop_alg_sha1,
	cryptocop_alg_des,
	cryptocop_alg_3des,
	cryptocop_alg_aes,
	cryptocop_no_alg,
} cryptocop_algorithm;

typedef u8 cryptocop_tfrm_id;


struct cryptocop_operation;

typedef void (cryptocop_callback)(struct cryptocop_operation*, void*);

struct cryptocop_transform_init {
	cryptocop_algorithm    alg;
	
	unsigned char          key[CRYPTOCOP_MAX_KEY_LENGTH];
	unsigned int           keylen;
	cryptocop_cipher_mode  cipher_mode;
	cryptocop_3des_mode    tdes_mode;
	cryptocop_csum_type    csum_mode; 

	cryptocop_tfrm_id tid; 
	struct cryptocop_transform_init *next;
};


typedef enum {
	cryptocop_source_dma = 0,
	cryptocop_source_des,
	cryptocop_source_3des,
	cryptocop_source_aes,
	cryptocop_source_md5,
	cryptocop_source_sha1,
	cryptocop_source_csum,
	cryptocop_source_none,
} cryptocop_source;


struct cryptocop_desc_cfg {
	cryptocop_tfrm_id tid;
	cryptocop_source src;
	unsigned int last:1; 
	struct cryptocop_desc_cfg *next;
};

struct cryptocop_desc {
	size_t length;
	struct cryptocop_desc_cfg *cfg;
	struct cryptocop_desc *next;
};


#define CRYPTOCOP_NO_FLAG     (0x00)
#define CRYPTOCOP_ENCRYPT     (0x01)
#define CRYPTOCOP_DECRYPT     (0x02)
#define CRYPTOCOP_EXPLICIT_IV (0x04)

struct cryptocop_tfrm_cfg {
	cryptocop_tfrm_id tid;

	unsigned int flags; 

	
	u8 iv[CRYPTOCOP_MAX_IV_LENGTH];

	size_t inject_ix;

	struct cryptocop_tfrm_cfg *next;
};



struct cryptocop_dma_list_operation{
	dma_descr_data *outlist; 
	char           *out_data_buf;
	dma_descr_data *inlist; 
	char           *in_data_buf;

	cryptocop_3des_mode tdes_mode;
	cryptocop_csum_type csum_mode;
};


struct cryptocop_tfrm_operation{
	
	struct cryptocop_tfrm_cfg *tfrm_cfg;
	struct cryptocop_desc *desc;

	struct iovec *indata;
	size_t incount;
	size_t inlen; 

	struct iovec *outdata;
	size_t outcount;
	size_t outlen; 
};


struct cryptocop_operation {
	cryptocop_callback *cb;
	void *cb_data;

	cryptocop_session_id sid;

	
	int operation_status; 

	
	unsigned int use_dmalists:1;  
	unsigned int in_interrupt:1;  
	unsigned int fast_callback:1; 

	union{
		struct cryptocop_dma_list_operation list_op;
		struct cryptocop_tfrm_operation tfrm_op;
	};
};


int cryptocop_new_session(cryptocop_session_id *sid, struct cryptocop_transform_init *tinit, int alloc_flag);
int cryptocop_free_session(cryptocop_session_id sid);

int cryptocop_job_queue_insert_csum(struct cryptocop_operation *operation);

int cryptocop_job_queue_insert_crypto(struct cryptocop_operation *operation);

int cryptocop_job_queue_insert_user_job(struct cryptocop_operation *operation);

#endif 

#endif 
