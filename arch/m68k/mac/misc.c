
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/mm.h>

#include <linux/adb.h>
#include <linux/cuda.h>
#include <linux/pmu.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/rtc.h>
#include <asm/segment.h>
#include <asm/setup.h>
#include <asm/macintosh.h>
#include <asm/mac_via.h>
#include <asm/mac_oss.h>

#define BOOTINFO_COMPAT_1_0
#include <asm/bootinfo.h>
#include <asm/machdep.h>


#define RTC_OFFSET 2082844800

static void (*rom_reset)(void);

#ifdef CONFIG_ADB_CUDA
static long cuda_read_time(void)
{
	struct adb_request req;
	long time;

	if (cuda_request(&req, NULL, 2, CUDA_PACKET, CUDA_GET_TIME) < 0)
		return 0;
	while (!req.complete)
		cuda_poll();

	time = (req.reply[3] << 24) | (req.reply[4] << 16)
		| (req.reply[5] << 8) | req.reply[6];
	return time - RTC_OFFSET;
}

static void cuda_write_time(long data)
{
	struct adb_request req;
	data += RTC_OFFSET;
	if (cuda_request(&req, NULL, 6, CUDA_PACKET, CUDA_SET_TIME,
			(data >> 24) & 0xFF, (data >> 16) & 0xFF,
			(data >> 8) & 0xFF, data & 0xFF) < 0)
		return;
	while (!req.complete)
		cuda_poll();
}

static __u8 cuda_read_pram(int offset)
{
	struct adb_request req;
	if (cuda_request(&req, NULL, 4, CUDA_PACKET, CUDA_GET_PRAM,
			(offset >> 8) & 0xFF, offset & 0xFF) < 0)
		return 0;
	while (!req.complete)
		cuda_poll();
	return req.reply[3];
}

static void cuda_write_pram(int offset, __u8 data)
{
	struct adb_request req;
	if (cuda_request(&req, NULL, 5, CUDA_PACKET, CUDA_SET_PRAM,
			(offset >> 8) & 0xFF, offset & 0xFF, data) < 0)
		return;
	while (!req.complete)
		cuda_poll();
}
#else
#define cuda_read_time() 0
#define cuda_write_time(n)
#define cuda_read_pram NULL
#define cuda_write_pram NULL
#endif

#ifdef CONFIG_ADB_PMU68K
static long pmu_read_time(void)
{
	struct adb_request req;
	long time;

	if (pmu_request(&req, NULL, 1, PMU_READ_RTC) < 0)
		return 0;
	while (!req.complete)
		pmu_poll();

	time = (req.reply[1] << 24) | (req.reply[2] << 16)
		| (req.reply[3] << 8) | req.reply[4];
	return time - RTC_OFFSET;
}

static void pmu_write_time(long data)
{
	struct adb_request req;
	data += RTC_OFFSET;
	if (pmu_request(&req, NULL, 5, PMU_SET_RTC,
			(data >> 24) & 0xFF, (data >> 16) & 0xFF,
			(data >> 8) & 0xFF, data & 0xFF) < 0)
		return;
	while (!req.complete)
		pmu_poll();
}

static __u8 pmu_read_pram(int offset)
{
	struct adb_request req;
	if (pmu_request(&req, NULL, 3, PMU_READ_NVRAM,
			(offset >> 8) & 0xFF, offset & 0xFF) < 0)
		return 0;
	while (!req.complete)
		pmu_poll();
	return req.reply[3];
}

static void pmu_write_pram(int offset, __u8 data)
{
	struct adb_request req;
	if (pmu_request(&req, NULL, 4, PMU_WRITE_NVRAM,
			(offset >> 8) & 0xFF, offset & 0xFF, data) < 0)
		return;
	while (!req.complete)
		pmu_poll();
}
#else
#define pmu_read_time() 0
#define pmu_write_time(n)
#define pmu_read_pram NULL
#define pmu_write_pram NULL
#endif

#if 0 
extern int maciisi_request(struct adb_request *req,
			void (*done)(struct adb_request *), int nbytes, ...);

static long maciisi_read_time(void)
{
	struct adb_request req;
	long time;

	if (maciisi_request(&req, NULL, 2, CUDA_PACKET, CUDA_GET_TIME))
		return 0;

	time = (req.reply[3] << 24) | (req.reply[4] << 16)
		| (req.reply[5] << 8) | req.reply[6];
	return time - RTC_OFFSET;
}

static void maciisi_write_time(long data)
{
	struct adb_request req;
	data += RTC_OFFSET;
	maciisi_request(&req, NULL, 6, CUDA_PACKET, CUDA_SET_TIME,
			(data >> 24) & 0xFF, (data >> 16) & 0xFF,
			(data >> 8) & 0xFF, data & 0xFF);
}

static __u8 maciisi_read_pram(int offset)
{
	struct adb_request req;
	if (maciisi_request(&req, NULL, 4, CUDA_PACKET, CUDA_GET_PRAM,
			(offset >> 8) & 0xFF, offset & 0xFF))
		return 0;
	return req.reply[3];
}

static void maciisi_write_pram(int offset, __u8 data)
{
	struct adb_request req;
	maciisi_request(&req, NULL, 5, CUDA_PACKET, CUDA_SET_PRAM,
			(offset >> 8) & 0xFF, offset & 0xFF, data);
}
#else
#define maciisi_read_time() 0
#define maciisi_write_time(n)
#define maciisi_read_pram NULL
#define maciisi_write_pram NULL
#endif


static __u8 via_pram_readbyte(void)
{
	int	i,reg;
	__u8	data;

	reg = via1[vBufB] & ~VIA1B_vRTCClk;

	

	via1[vDirB] &= ~VIA1B_vRTCData;

	

	data = 0;
	for (i = 0 ; i < 8 ; i++) {
		via1[vBufB] = reg;
		via1[vBufB] = reg | VIA1B_vRTCClk;
		data = (data << 1) | (via1[vBufB] & VIA1B_vRTCData);
	}

	

	via1[vDirB] |= VIA1B_vRTCData;

	return data;
}

static void via_pram_writebyte(__u8 data)
{
	int	i,reg,bit;

	reg = via1[vBufB] & ~(VIA1B_vRTCClk | VIA1B_vRTCData);

	

	for (i = 0 ; i < 8 ; i++) {
		bit = data & 0x80? 1 : 0;
		data <<= 1;
		via1[vBufB] = reg | bit;
		via1[vBufB] = reg | bit | VIA1B_vRTCClk;
	}
}


static void via_pram_command(int command, __u8 *data)
{
	unsigned long flags;
	int	is_read;

	local_irq_save(flags);

	

	via1[vBufB] = (via1[vBufB] | VIA1B_vRTCClk) & ~VIA1B_vRTCEnb;

	if (command & 0xFF00) {		
		via_pram_writebyte((command & 0xFF00) >> 8);
		via_pram_writebyte(command & 0xFF);
		is_read = command & 0x8000;
	} else {			
		via_pram_writebyte(command);
		is_read = command & 0x80;
	}
	if (is_read) {
		*data = via_pram_readbyte();
	} else {
		via_pram_writebyte(*data);
	}

	

	via1[vBufB] |= VIA1B_vRTCEnb;

	local_irq_restore(flags);
}

static __u8 via_read_pram(int offset)
{
	return 0;
}

static void via_write_pram(int offset, __u8 data)
{
}


static long via_read_time(void)
{
	union {
		__u8 cdata[4];
		long idata;
	} result, last_result;
	int count = 1;

	via_pram_command(0x81, &last_result.cdata[3]);
	via_pram_command(0x85, &last_result.cdata[2]);
	via_pram_command(0x89, &last_result.cdata[1]);
	via_pram_command(0x8D, &last_result.cdata[0]);


	while (1) {
		via_pram_command(0x81, &result.cdata[3]);
		via_pram_command(0x85, &result.cdata[2]);
		via_pram_command(0x89, &result.cdata[1]);
		via_pram_command(0x8D, &result.cdata[0]);

		if (result.idata == last_result.idata)
			return result.idata - RTC_OFFSET;

		if (++count > 10)
			break;

		last_result.idata = result.idata;
	}

	pr_err("via_read_time: failed to read a stable value; "
	       "got 0x%08lx then 0x%08lx\n",
	       last_result.idata, result.idata);

	return 0;
}


static void via_write_time(long time)
{
	union {
		__u8  cdata[4];
		long  idata;
	} data;
	__u8	temp;

	

	temp = 0x55;
	via_pram_command(0x35, &temp);

	data.idata = time + RTC_OFFSET;
	via_pram_command(0x01, &data.cdata[3]);
	via_pram_command(0x05, &data.cdata[2]);
	via_pram_command(0x09, &data.cdata[1]);
	via_pram_command(0x0D, &data.cdata[0]);

	

	temp = 0xD5;
	via_pram_command(0x35, &temp);
}

static void via_shutdown(void)
{
	if (rbv_present) {
		via2[rBufB] &= ~0x04;
	} else {
		
		via2[vDirB] |= 0x04;
		
		via2[vBufB] &= ~0x04;
		mdelay(1000);
	}
}


static void oss_shutdown(void)
{
	oss->rom_ctrl = OSS_POWEROFF;
}

#ifdef CONFIG_ADB_CUDA

static void cuda_restart(void)
{
	struct adb_request req;
	if (cuda_request(&req, NULL, 2, CUDA_PACKET, CUDA_RESET_SYSTEM) < 0)
		return;
	while (!req.complete)
		cuda_poll();
}

static void cuda_shutdown(void)
{
	struct adb_request req;
	if (cuda_request(&req, NULL, 2, CUDA_PACKET, CUDA_POWERDOWN) < 0)
		return;
	while (!req.complete)
		cuda_poll();
}

#endif 

#ifdef CONFIG_ADB_PMU68K

void pmu_restart(void)
{
	struct adb_request req;
	if (pmu_request(&req, NULL,
			2, PMU_SET_INTR_MASK, PMU_INT_ADB|PMU_INT_TICK) < 0)
		return;
	while (!req.complete)
		pmu_poll();
	if (pmu_request(&req, NULL, 1, PMU_RESET) < 0)
		return;
	while (!req.complete)
		pmu_poll();
}

void pmu_shutdown(void)
{
	struct adb_request req;
	if (pmu_request(&req, NULL,
			2, PMU_SET_INTR_MASK, PMU_INT_ADB|PMU_INT_TICK) < 0)
		return;
	while (!req.complete)
		pmu_poll();
	if (pmu_request(&req, NULL, 5, PMU_SHUTDOWN, 'M', 'A', 'T', 'T') < 0)
		return;
	while (!req.complete)
		pmu_poll();
}

#endif


void mac_pram_read(int offset, __u8 *buffer, int len)
{
	__u8 (*func)(int);
	int i;

	switch(macintosh_config->adb_type) {
	case MAC_ADB_IISI:
		func = maciisi_read_pram; break;
	case MAC_ADB_PB1:
	case MAC_ADB_PB2:
		func = pmu_read_pram; break;
	case MAC_ADB_CUDA:
		func = cuda_read_pram; break;
	default:
		func = via_read_pram;
	}
	if (!func)
		return;
	for (i = 0 ; i < len ; i++) {
		buffer[i] = (*func)(offset++);
	}
}

void mac_pram_write(int offset, __u8 *buffer, int len)
{
	void (*func)(int, __u8);
	int i;

	switch(macintosh_config->adb_type) {
	case MAC_ADB_IISI:
		func = maciisi_write_pram; break;
	case MAC_ADB_PB1:
	case MAC_ADB_PB2:
		func = pmu_write_pram; break;
	case MAC_ADB_CUDA:
		func = cuda_write_pram; break;
	default:
		func = via_write_pram;
	}
	if (!func)
		return;
	for (i = 0 ; i < len ; i++) {
		(*func)(offset++, buffer[i]);
	}
}

void mac_poweroff(void)
{

	if (oss_present) {
		oss_shutdown();
	} else if (macintosh_config->adb_type == MAC_ADB_II) {
		via_shutdown();
#ifdef CONFIG_ADB_CUDA
	} else if (macintosh_config->adb_type == MAC_ADB_CUDA) {
		cuda_shutdown();
#endif
#ifdef CONFIG_ADB_PMU68K
	} else if (macintosh_config->adb_type == MAC_ADB_PB1
		|| macintosh_config->adb_type == MAC_ADB_PB2) {
		pmu_shutdown();
#endif
	}
	local_irq_enable();
	printk("It is now safe to turn off your Macintosh.\n");
	while(1);
}

void mac_reset(void)
{
	if (macintosh_config->adb_type == MAC_ADB_II) {
		unsigned long flags;

		
		

		if (mac_bi_data.rombase == 0)
			mac_bi_data.rombase = 0x40800000;

		
		rom_reset = (void *) (mac_bi_data.rombase + 0xa);

		if (macintosh_config->ident == MAC_MODEL_SE30) {
		} else {
			local_irq_save(flags);

			rom_reset();

			local_irq_restore(flags);
		}
#ifdef CONFIG_ADB_CUDA
	} else if (macintosh_config->adb_type == MAC_ADB_CUDA) {
		cuda_restart();
#endif
#ifdef CONFIG_ADB_PMU68K
	} else if (macintosh_config->adb_type == MAC_ADB_PB1
		|| macintosh_config->adb_type == MAC_ADB_PB2) {
		pmu_restart();
#endif
	} else if (CPU_IS_030) {


		unsigned long rombase = 0x40000000;

		
		unsigned long virt = (unsigned long) mac_reset;
		unsigned long phys = virt_to_phys(mac_reset);
		unsigned long addr = (phys&0xFF000000)|0x8777;
		unsigned long offset = phys-virt;
		local_irq_disable(); 
		__asm__ __volatile__(".chip 68030\n\t"
				     "pmove %0,%/tt0\n\t"
				     ".chip 68k"
				     : : "m" (addr));
		
		__asm__ __volatile__(
                    ".chip 68030\n\t"
		    "lea %/pc@(1f),%/a0\n\t"
		    "addl %0,%/a0\n\t"
		    "addl %0,%/sp\n\t"
		    "pflusha\n\t"
		    "jmp %/a0@\n\t" 
		    "0:.long 0\n\t" 
		    
		    "1:\n\t"
		    "lea %/pc@(0b),%/a0\n\t"
		    "pmove %/a0@, %/tc\n\t" 
		    "pmove %/a0@, %/tt0\n\t" 
		    "pmove %/a0@, %/tt1\n\t" 
		    "movel #0, %/a0\n\t"
		    "movec %/a0, %/vbr\n\t" 
		    "movec %/a0, %/cacr\n\t" 
		    "movel #0x0808,%/a0\n\t"
		    "movec %/a0, %/cacr\n\t" 
		    "movew #0x2700,%/sr\n\t" 
		    "movel %1@(0x0),%/a0\n\t"
		    "movec %/a0, %/isp\n\t"
		    "movel %1@(0x4),%/a0\n\t" 
		    "reset\n\t" 
		    "jmp %/a0@\n\t" 
		    ".chip 68k"
		    : : "r" (offset), "a" (rombase) : "a0");
	}

	
	local_irq_enable();
	printk ("Restart failed.  Please restart manually.\n");
	while(1);
}

#define SECS_PER_MINUTE (60)
#define SECS_PER_HOUR  (SECS_PER_MINUTE * 60)
#define SECS_PER_DAY   (SECS_PER_HOUR * 24)

static void unmktime(unsigned long time, long offset,
		     int *yearp, int *monp, int *dayp,
		     int *hourp, int *minp, int *secp)
{
        
	static const unsigned short int __mon_yday[2][13] =
	{
		
		{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
		
		{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
	};
	long int days, rem, y, wday, yday;
	const unsigned short int *ip;

	days = time / SECS_PER_DAY;
	rem = time % SECS_PER_DAY;
	rem += offset;
	while (rem < 0) {
		rem += SECS_PER_DAY;
		--days;
	}
	while (rem >= SECS_PER_DAY) {
		rem -= SECS_PER_DAY;
		++days;
	}
	*hourp = rem / SECS_PER_HOUR;
	rem %= SECS_PER_HOUR;
	*minp = rem / SECS_PER_MINUTE;
	*secp = rem % SECS_PER_MINUTE;
	
	wday = (4 + days) % 7; 
	if (wday < 0) wday += 7;
	y = 1970;

#define DIV(a, b) ((a) / (b) - ((a) % (b) < 0))
#define LEAPS_THRU_END_OF(y) (DIV (y, 4) - DIV (y, 100) + DIV (y, 400))
#define __isleap(year)	\
  ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))

	while (days < 0 || days >= (__isleap (y) ? 366 : 365))
	{
		
		long int yg = y + days / 365 - (days % 365 < 0);

		
		days -= ((yg - y) * 365
			 + LEAPS_THRU_END_OF (yg - 1)
			 - LEAPS_THRU_END_OF (y - 1));
		y = yg;
	}
	*yearp = y - 1900;
	yday = days; 
	ip = __mon_yday[__isleap(y)];
	for (y = 11; days < (long int) ip[y]; --y)
		continue;
	days -= ip[y];
	*monp = y;
	*dayp = days + 1; 
	return;
}


int mac_hwclk(int op, struct rtc_time *t)
{
	unsigned long now;

	if (!op) { 
		switch (macintosh_config->adb_type) {
		case MAC_ADB_II:
		case MAC_ADB_IOP:
			now = via_read_time();
			break;
		case MAC_ADB_IISI:
			now = maciisi_read_time();
			break;
		case MAC_ADB_PB1:
		case MAC_ADB_PB2:
			now = pmu_read_time();
			break;
		case MAC_ADB_CUDA:
			now = cuda_read_time();
			break;
		default:
			now = 0;
		}

		t->tm_wday = 0;
		unmktime(now, 0,
			 &t->tm_year, &t->tm_mon, &t->tm_mday,
			 &t->tm_hour, &t->tm_min, &t->tm_sec);
#if 0
		printk("mac_hwclk: read %04d-%02d-%-2d %02d:%02d:%02d\n",
			t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
			t->tm_hour, t->tm_min, t->tm_sec);
#endif
	} else { 
#if 0
		printk("mac_hwclk: tried to write %04d-%02d-%-2d %02d:%02d:%02d\n",
			t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
			t->tm_hour, t->tm_min, t->tm_sec);
#endif

		now = mktime(t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
			     t->tm_hour, t->tm_min, t->tm_sec);

		switch (macintosh_config->adb_type) {
		case MAC_ADB_II:
		case MAC_ADB_IOP:
			via_write_time(now);
			break;
		case MAC_ADB_CUDA:
			cuda_write_time(now);
			break;
		case MAC_ADB_PB1:
		case MAC_ADB_PB2:
			pmu_write_time(now);
			break;
		case MAC_ADB_IISI:
			maciisi_write_time(now);
		}
	}
	return 0;
}


int mac_set_clock_mmss (unsigned long nowtime)
{
	struct rtc_time now;

	mac_hwclk(0, &now);
	now.tm_sec = nowtime % 60;
	now.tm_min = (nowtime / 60) % 60;
	mac_hwclk(1, &now);

	return 0;
}
