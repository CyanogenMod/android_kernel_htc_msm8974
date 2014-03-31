#ifndef _ASM_X86_MPSPEC_DEF_H
#define _ASM_X86_MPSPEC_DEF_H



#define SMP_MAGIC_IDENT	(('_'<<24) | ('P'<<16) | ('M'<<8) | '_')

#ifdef CONFIG_X86_32
# define MAX_MPC_ENTRY 1024
#endif

struct mpf_intel {
	char signature[4];		
	unsigned int physptr;		
	unsigned char length;		
	unsigned char specification;	
	unsigned char checksum;		
	unsigned char feature1;		
	unsigned char feature2;		
	unsigned char feature3;		
	unsigned char feature4;		
	unsigned char feature5;		
};

#define MPC_SIGNATURE "PCMP"

struct mpc_table {
	char signature[4];
	unsigned short length;		
	char spec;			
	char checksum;
	char oem[8];
	char productid[12];
	unsigned int oemptr;		
	unsigned short oemsize;		
	unsigned short oemcount;
	unsigned int lapic;		
	unsigned int reserved;
};


#define	MP_PROCESSOR		0
#define	MP_BUS			1
#define	MP_IOAPIC		2
#define	MP_INTSRC		3
#define	MP_LINTSRC		4
#define	MP_TRANSLATION		192

#define CPU_ENABLED		1	
#define CPU_BOOTPROCESSOR	2	

#define CPU_STEPPING_MASK	0x000F
#define CPU_MODEL_MASK		0x00F0
#define CPU_FAMILY_MASK		0x0F00

struct mpc_cpu {
	unsigned char type;
	unsigned char apicid;		
	unsigned char apicver;		
	unsigned char cpuflag;
	unsigned int cpufeature;
	unsigned int featureflag;	
	unsigned int reserved[2];
};

struct mpc_bus {
	unsigned char type;
	unsigned char busid;
	unsigned char bustype[6];
};

#define BUSTYPE_EISA	"EISA"
#define BUSTYPE_ISA	"ISA"
#define BUSTYPE_INTERN	"INTERN"	
#define BUSTYPE_MCA	"MCA"
#define BUSTYPE_VL	"VL"		
#define BUSTYPE_PCI	"PCI"
#define BUSTYPE_PCMCIA	"PCMCIA"
#define BUSTYPE_CBUS	"CBUS"
#define BUSTYPE_CBUSII	"CBUSII"
#define BUSTYPE_FUTURE	"FUTURE"
#define BUSTYPE_MBI	"MBI"
#define BUSTYPE_MBII	"MBII"
#define BUSTYPE_MPI	"MPI"
#define BUSTYPE_MPSA	"MPSA"
#define BUSTYPE_NUBUS	"NUBUS"
#define BUSTYPE_TC	"TC"
#define BUSTYPE_VME	"VME"
#define BUSTYPE_XPRESS	"XPRESS"

#define MPC_APIC_USABLE		0x01

struct mpc_ioapic {
	unsigned char type;
	unsigned char apicid;
	unsigned char apicver;
	unsigned char flags;
	unsigned int apicaddr;
};

struct mpc_intsrc {
	unsigned char type;
	unsigned char irqtype;
	unsigned short irqflag;
	unsigned char srcbus;
	unsigned char srcbusirq;
	unsigned char dstapic;
	unsigned char dstirq;
};

enum mp_irq_source_types {
	mp_INT = 0,
	mp_NMI = 1,
	mp_SMI = 2,
	mp_ExtINT = 3
};

#define MP_IRQDIR_DEFAULT	0
#define MP_IRQDIR_HIGH		1
#define MP_IRQDIR_LOW		3

#define MP_APIC_ALL	0xFF

struct mpc_lintsrc {
	unsigned char type;
	unsigned char irqtype;
	unsigned short irqflag;
	unsigned char srcbusid;
	unsigned char srcbusirq;
	unsigned char destapic;
	unsigned char destapiclint;
};

#define MPC_OEM_SIGNATURE "_OEM"

struct mpc_oemtable {
	char signature[4];
	unsigned short length;		
	char  rev;			
	char  checksum;
	char  mpc[8];
};


enum mp_bustype {
	MP_BUS_ISA = 1,
	MP_BUS_EISA,
	MP_BUS_PCI,
	MP_BUS_MCA,
};
#endif 
