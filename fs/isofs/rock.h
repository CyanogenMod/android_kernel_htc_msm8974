
struct SU_SP_s {
	unsigned char magic[2];
	unsigned char skip;
} __attribute__ ((packed));

struct SU_CE_s {
	char extent[8];
	char offset[8];
	char size[8];
};

struct SU_ER_s {
	unsigned char len_id;
	unsigned char len_des;
	unsigned char len_src;
	unsigned char ext_ver;
	char data[0];
} __attribute__ ((packed));

struct RR_RR_s {
	char flags[1];
} __attribute__ ((packed));

struct RR_PX_s {
	char mode[8];
	char n_links[8];
	char uid[8];
	char gid[8];
};

struct RR_PN_s {
	char dev_high[8];
	char dev_low[8];
};

struct SL_component {
	unsigned char flags;
	unsigned char len;
	char text[0];
} __attribute__ ((packed));

struct RR_SL_s {
	unsigned char flags;
	struct SL_component link;
} __attribute__ ((packed));

struct RR_NM_s {
	unsigned char flags;
	char name[0];
} __attribute__ ((packed));

struct RR_CL_s {
	char location[8];
};

struct RR_PL_s {
	char location[8];
};

struct stamp {
	char time[7];
} __attribute__ ((packed));

struct RR_TF_s {
	char flags;
	struct stamp times[0];	
} __attribute__ ((packed));

struct RR_ZF_s {
	char algorithm[2];
	char parms[2];
	char real_size[8];
};

#define TF_CREATE 1
#define TF_MODIFY 2
#define TF_ACCESS 4
#define TF_ATTRIBUTES 8
#define TF_BACKUP 16
#define TF_EXPIRATION 32
#define TF_EFFECTIVE 64
#define TF_LONG_FORM 128

struct rock_ridge {
	char signature[2];
	unsigned char len;
	unsigned char version;
	union {
		struct SU_SP_s SP;
		struct SU_CE_s CE;
		struct SU_ER_s ER;
		struct RR_RR_s RR;
		struct RR_PX_s PX;
		struct RR_PN_s PN;
		struct RR_SL_s SL;
		struct RR_NM_s NM;
		struct RR_CL_s CL;
		struct RR_PL_s PL;
		struct RR_TF_s TF;
		struct RR_ZF_s ZF;
	} u;
};

#define RR_PX 1			
#define RR_PN 2			
#define RR_SL 4			
#define RR_NM 8			
#define RR_CL 16		
#define RR_PL 32		
#define RR_RE 64		
#define RR_TF 128		
