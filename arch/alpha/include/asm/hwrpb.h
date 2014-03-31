#ifndef __ALPHA_HWRPB_H
#define __ALPHA_HWRPB_H

#define INIT_HWRPB ((struct hwrpb_struct *) 0x10000000)


#define EV3_CPU                 1       
#define EV4_CPU                 2       
#define LCA4_CPU                4       
#define EV5_CPU                 5       
#define EV45_CPU                6       
#define EV56_CPU		7	
#define EV6_CPU			8	
#define PCA56_CPU		9	
#define PCA57_CPU		10	
#define EV67_CPU		11	
#define EV68CB_CPU		12	
#define EV68AL_CPU		13	
#define EV68CX_CPU		14	
#define EV7_CPU			15	
#define EV79_CPU		16	
#define EV69_CPU		17	


#define ST_ADU			  1	
#define ST_DEC_4000		  2	
#define ST_DEC_7000		  3	
#define ST_DEC_3000_500		  4	
#define ST_DEC_2000_300		  6	
#define ST_DEC_3000_300		  7	
#define ST_DEC_2100_A500	  9	
#define ST_DEC_AXPVME_64	 10	
#define ST_DEC_AXPPCI_33	 11	
#define ST_DEC_TLASER		 12	
#define ST_DEC_2100_A50		 13	
#define ST_DEC_MUSTANG		 14	
#define ST_DEC_ALCOR		 15	
#define ST_DEC_1000		 17	
#define ST_DEC_EB64		 18	
#define ST_DEC_EB66		 19	
#define ST_DEC_EB64P		 20	
#define ST_DEC_BURNS		 21	
#define ST_DEC_RAWHIDE		 22	
#define ST_DEC_K2		 23	
#define ST_DEC_LYNX		 24	
#define ST_DEC_XL		 25	
#define ST_DEC_EB164		 26	
#define ST_DEC_NORITAKE		 27	
#define ST_DEC_CORTEX		 28	
#define ST_DEC_MIATA		 30	
#define ST_DEC_XXM		 31	
#define ST_DEC_TAKARA		 32	
#define ST_DEC_YUKON		 33	
#define ST_DEC_TSUNAMI		 34	
#define ST_DEC_WILDFIRE		 35	
#define ST_DEC_CUSCO		 36	
#define ST_DEC_EIGER		 37	
#define ST_DEC_TITAN		 38	
#define ST_DEC_MARVEL		 39	

#define ST_UNOFFICIAL_BIAS	100
#define ST_DTI_RUFFIAN		101	

#define ST_API_BIAS		200
#define ST_API_NAUTILUS		201	

struct pcb_struct {
	unsigned long ksp;
	unsigned long usp;
	unsigned long ptbr;
	unsigned int pcc;
	unsigned int asn;
	unsigned long unique;
	unsigned long flags;
	unsigned long res1, res2;
};

struct percpu_struct {
	unsigned long hwpcb[16];
	unsigned long flags;
	unsigned long pal_mem_size;
	unsigned long pal_scratch_size;
	unsigned long pal_mem_pa;
	unsigned long pal_scratch_pa;
	unsigned long pal_revision;
	unsigned long type;
	unsigned long variation;
	unsigned long revision;
	unsigned long serial_no[2];
	unsigned long logout_area_pa;
	unsigned long logout_area_len;
	unsigned long halt_PCBB;
	unsigned long halt_PC;
	unsigned long halt_PS;
	unsigned long halt_arg;
	unsigned long halt_ra;
	unsigned long halt_pv;
	unsigned long halt_reason;
	unsigned long res;
	unsigned long ipc_buffer[21];
	unsigned long palcode_avail[16];
	unsigned long compatibility;
	unsigned long console_data_log_pa;
	unsigned long console_data_log_length;
	unsigned long bcache_info;
};

struct procdesc_struct {
	unsigned long weird_vms_stuff;
	unsigned long address;
};

struct vf_map_struct {
	unsigned long va;
	unsigned long pa;
	unsigned long count;
};

struct crb_struct {
	struct procdesc_struct * dispatch_va;
	struct procdesc_struct * dispatch_pa;
	struct procdesc_struct * fixup_va;
	struct procdesc_struct * fixup_pa;
	
	unsigned long map_entries;
	unsigned long map_pages;
	struct vf_map_struct map[1];
};

struct memclust_struct {
	unsigned long start_pfn;
	unsigned long numpages;
	unsigned long numtested;
	unsigned long bitmap_va;
	unsigned long bitmap_pa;
	unsigned long bitmap_chksum;
	unsigned long usage;
};

struct memdesc_struct {
	unsigned long chksum;
	unsigned long optional_pa;
	unsigned long numclusters;
	struct memclust_struct cluster[0];
};

struct dsr_struct {
	long smm;			
	unsigned long  lurt_off;	
	unsigned long  sysname_off;	
};

struct hwrpb_struct {
	unsigned long phys_addr;	
	unsigned long id;		
	unsigned long revision;	
	unsigned long size;		
	unsigned long cpuid;
	unsigned long pagesize;		
	unsigned long pa_bits;		
	unsigned long max_asn;
	unsigned char ssn[16];		
	unsigned long sys_type;
	unsigned long sys_variation;
	unsigned long sys_revision;
	unsigned long intr_freq;	
	unsigned long cycle_freq;	
	unsigned long vptb;		
	unsigned long res1;
	unsigned long tbhb_offset;	
	unsigned long nr_processors;
	unsigned long processor_size;
	unsigned long processor_offset;
	unsigned long ctb_nr;
	unsigned long ctb_size;		
	unsigned long ctbt_offset;	
	unsigned long crb_offset;	
	unsigned long mddt_offset;	
	unsigned long cdb_offset;	
	unsigned long frut_offset;	
	void (*save_terminal)(unsigned long);
	unsigned long save_terminal_data;
	void (*restore_terminal)(unsigned long);
	unsigned long restore_terminal_data;
	void (*CPU_restart)(unsigned long);
	unsigned long CPU_restart_data;
	unsigned long res2;
	unsigned long res3;
	unsigned long chksum;
	unsigned long rxrdy;
	unsigned long txrdy;
	unsigned long dsr_offset;	
};

#ifdef __KERNEL__

extern struct hwrpb_struct *hwrpb;

static inline void
hwrpb_update_checksum(struct hwrpb_struct *h)
{
	unsigned long sum = 0, *l;
        for (l = (unsigned long *) h; l < (unsigned long *) &h->chksum; ++l)
                sum += *l;
        h->chksum = sum;
}

#endif 

#endif 
