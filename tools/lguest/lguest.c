#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <stdint.h>
#include <stdlib.h>
#include <elf.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <linux/if_tun.h>
#include <sys/uio.h>
#include <termios.h>
#include <getopt.h>
#include <assert.h>
#include <sched.h>
#include <limits.h>
#include <stddef.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>

#include <linux/virtio_config.h>
#include <linux/virtio_net.h>
#include <linux/virtio_blk.h>
#include <linux/virtio_console.h>
#include <linux/virtio_rng.h>
#include <linux/virtio_ring.h>
#include <asm/bootparam.h>
#include "../../include/linux/lguest_launcher.h"
typedef unsigned long long u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

#define BRIDGE_PFX "bridge:"
#ifndef SIOCBRADDIF
#define SIOCBRADDIF	0x89a2		
#endif
#define DEVICE_PAGES 256
#define VIRTQUEUE_NUM 256

static bool verbose;
#define verbose(args...) \
	do { if (verbose) printf(args); } while(0)

static void *guest_base;
static unsigned long guest_limit, guest_max;
static int lguest_fd;

static unsigned int __thread cpu_id;

struct device_list {
	
	unsigned int next_irq;

	
	unsigned int device_num;

	
	u8 *descpage;

	
	struct device *dev;
	
	struct device *lastdev;
};

static struct device_list devices;

struct device {
	
	struct device *next;

	
	struct lguest_device_desc *desc;

	
	unsigned int feature_len;
	unsigned int num_vq;

	
	const char *name;

	
	struct virtqueue *vq;

	
	bool running;

	
	void *priv;
};

struct virtqueue {
	struct virtqueue *next;

	
	struct device *dev;

	
	struct lguest_vqconfig config;

	
	struct vring vring;

	
	u16 last_avail_idx;

	
	unsigned int pending_used;

	
	int eventfd;

	
	void (*service)(struct virtqueue *vq);
	pid_t thread;
};

static char **main_args;

static struct termios orig_term;

#define wmb() __asm__ __volatile__("" : : : "memory")
#define mb() __asm__ __volatile__("" : : : "memory")

#define convert(iov, type) \
	((type *)_convert((iov), sizeof(type), __alignof__(type), #type))

static void *_convert(struct iovec *iov, size_t size, size_t align,
		      const char *name)
{
	if (iov->iov_len != size)
		errx(1, "Bad iovec size %zu for %s", iov->iov_len, name);
	if ((unsigned long)iov->iov_base % align != 0)
		errx(1, "Bad alignment %p for %s", iov->iov_base, name);
	return iov->iov_base;
}

#define lg_last_avail(vq)	((vq)->last_avail_idx)

#define cpu_to_le16(v16) (v16)
#define cpu_to_le32(v32) (v32)
#define cpu_to_le64(v64) (v64)
#define le16_to_cpu(v16) (v16)
#define le32_to_cpu(v32) (v32)
#define le64_to_cpu(v64) (v64)

static bool iov_empty(const struct iovec iov[], unsigned int num_iov)
{
	unsigned int i;

	for (i = 0; i < num_iov; i++)
		if (iov[i].iov_len)
			return false;
	return true;
}

static void iov_consume(struct iovec iov[], unsigned num_iov, unsigned len)
{
	unsigned int i;

	for (i = 0; i < num_iov; i++) {
		unsigned int used;

		used = iov[i].iov_len < len ? iov[i].iov_len : len;
		iov[i].iov_base += used;
		iov[i].iov_len -= used;
		len -= used;
	}
	assert(len == 0);
}

static u8 *get_feature_bits(struct device *dev)
{
	return (u8 *)(dev->desc + 1)
		+ dev->num_vq * sizeof(struct lguest_vqconfig);
}

static void *from_guest_phys(unsigned long addr)
{
	return guest_base + addr;
}

static unsigned long to_guest_phys(const void *addr)
{
	return (addr - guest_base);
}

static int open_or_die(const char *name, int flags)
{
	int fd = open(name, flags);
	if (fd < 0)
		err(1, "Failed to open %s", name);
	return fd;
}

static void *map_zeroed_pages(unsigned int num)
{
	int fd = open_or_die("/dev/zero", O_RDONLY);
	void *addr;

	addr = mmap(NULL, getpagesize() * (num+2),
		    PROT_NONE, MAP_PRIVATE, fd, 0);

	if (addr == MAP_FAILED)
		err(1, "Mmapping %u pages of /dev/zero", num);

	if (mprotect(addr + getpagesize(), getpagesize() * num,
		     PROT_READ|PROT_WRITE) == -1)
		err(1, "mprotect rw %u pages failed", num);

	close(fd);

	
	return addr + getpagesize();
}

static void *get_pages(unsigned int num)
{
	void *addr = from_guest_phys(guest_limit);

	guest_limit += num * getpagesize();
	if (guest_limit > guest_max)
		errx(1, "Not enough memory for devices");
	return addr;
}

static void map_at(int fd, void *addr, unsigned long offset, unsigned long len)
{
	ssize_t r;

	if (mmap(addr, len, PROT_READ|PROT_WRITE,
		 MAP_FIXED|MAP_PRIVATE, fd, offset) != MAP_FAILED)
		return;

	
	r = pread(fd, addr, len, offset);
	if (r != len)
		err(1, "Reading offset %lu len %lu gave %zi", offset, len, r);
}

static unsigned long map_elf(int elf_fd, const Elf32_Ehdr *ehdr)
{
	Elf32_Phdr phdr[ehdr->e_phnum];
	unsigned int i;

	if (ehdr->e_type != ET_EXEC
	    || ehdr->e_machine != EM_386
	    || ehdr->e_phentsize != sizeof(Elf32_Phdr)
	    || ehdr->e_phnum < 1 || ehdr->e_phnum > 65536U/sizeof(Elf32_Phdr))
		errx(1, "Malformed elf header");


	
	if (lseek(elf_fd, ehdr->e_phoff, SEEK_SET) < 0)
		err(1, "Seeking to program headers");
	if (read(elf_fd, phdr, sizeof(phdr)) != sizeof(phdr))
		err(1, "Reading program headers");

	for (i = 0; i < ehdr->e_phnum; i++) {
		
		if (phdr[i].p_type != PT_LOAD)
			continue;

		verbose("Section %i: size %i addr %p\n",
			i, phdr[i].p_memsz, (void *)phdr[i].p_paddr);

		
		map_at(elf_fd, from_guest_phys(phdr[i].p_paddr),
		       phdr[i].p_offset, phdr[i].p_filesz);
	}

	
	return ehdr->e_entry;
}

static unsigned long load_bzimage(int fd)
{
	struct boot_params boot;
	int r;
	
	void *p = from_guest_phys(0x100000);

	lseek(fd, 0, SEEK_SET);
	read(fd, &boot, sizeof(boot));

	
	if (memcmp(&boot.hdr.header, "HdrS", 4) != 0)
		errx(1, "This doesn't look like a bzImage to me");

	
	lseek(fd, (boot.hdr.setup_sects+1) * 512, SEEK_SET);

	
	while ((r = read(fd, p, 65536)) > 0)
		p += r;

	
	return boot.hdr.code32_start;
}

static unsigned long load_kernel(int fd)
{
	Elf32_Ehdr hdr;

	
	if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr))
		err(1, "Reading kernel");

	
	if (memcmp(hdr.e_ident, ELFMAG, SELFMAG) == 0)
		return map_elf(fd, &hdr);

	
	return load_bzimage(fd);
}

static inline unsigned long page_align(unsigned long addr)
{
	
	return ((addr + getpagesize()-1) & ~(getpagesize()-1));
}

static unsigned long load_initrd(const char *name, unsigned long mem)
{
	int ifd;
	struct stat st;
	unsigned long len;

	ifd = open_or_die(name, O_RDONLY);
	
	if (fstat(ifd, &st) < 0)
		err(1, "fstat() on initrd '%s'", name);

	len = page_align(st.st_size);
	map_at(ifd, from_guest_phys(mem - len), 0, st.st_size);
	close(ifd);
	verbose("mapped initrd %s size=%lu @ %p\n", name, len, (void*)mem-len);

	
	return len;
}

static void concat(char *dst, char *args[])
{
	unsigned int i, len = 0;

	for (i = 0; args[i]; i++) {
		if (i) {
			strcat(dst+len, " ");
			len++;
		}
		strcpy(dst+len, args[i]);
		len += strlen(args[i]);
	}
	
	dst[len] = '\0';
}

static void tell_kernel(unsigned long start)
{
	unsigned long args[] = { LHREQ_INITIALIZE,
				 (unsigned long)guest_base,
				 guest_limit / getpagesize(), start };
	verbose("Guest: %p - %p (%#lx)\n",
		guest_base, guest_base + guest_limit, guest_limit);
	lguest_fd = open_or_die("/dev/lguest", O_RDWR);
	if (write(lguest_fd, args, sizeof(args)) < 0)
		err(1, "Writing to /dev/lguest");
}

static void *_check_pointer(unsigned long addr, unsigned int size,
			    unsigned int line)
{
	if ((addr + size) > guest_limit || (addr + size) < addr)
		errx(1, "%s:%i: Invalid address %#lx", __FILE__, line, addr);
	return from_guest_phys(addr);
}
#define check_pointer(addr,size) _check_pointer(addr, size, __LINE__)

static unsigned next_desc(struct vring_desc *desc,
			  unsigned int i, unsigned int max)
{
	unsigned int next;

	
	if (!(desc[i].flags & VRING_DESC_F_NEXT))
		return max;

	
	next = desc[i].next;
	
	wmb();

	if (next >= max)
		errx(1, "Desc next is %u", next);

	return next;
}

static void trigger_irq(struct virtqueue *vq)
{
	unsigned long buf[] = { LHREQ_IRQ, vq->config.irq };

	
	if (!vq->pending_used)
		return;
	vq->pending_used = 0;

	
	if (vq->vring.avail->flags & VRING_AVAIL_F_NO_INTERRUPT) {
		return;
	}

	
	if (write(lguest_fd, buf, sizeof(buf)) != 0)
		err(1, "Triggering irq %i", vq->config.irq);
}

static unsigned wait_for_vq_desc(struct virtqueue *vq,
				 struct iovec iov[],
				 unsigned int *out_num, unsigned int *in_num)
{
	unsigned int i, head, max;
	struct vring_desc *desc;
	u16 last_avail = lg_last_avail(vq);

	
	while (last_avail == vq->vring.avail->idx) {
		u64 event;

		trigger_irq(vq);

		
		vq->vring.used->flags &= ~VRING_USED_F_NO_NOTIFY;

		/*
		 * They could have slipped one in as we were doing that: make
		 * sure it's written, then check again.
		 */
		mb();
		if (last_avail != vq->vring.avail->idx) {
			vq->vring.used->flags |= VRING_USED_F_NO_NOTIFY;
			break;
		}

		
		if (read(vq->eventfd, &event, sizeof(event)) != sizeof(event))
			errx(1, "Event read failed?");

		
		vq->vring.used->flags |= VRING_USED_F_NO_NOTIFY;
	}

	
	if ((u16)(vq->vring.avail->idx - last_avail) > vq->vring.num)
		errx(1, "Guest moved used index from %u to %u",
		     last_avail, vq->vring.avail->idx);

	head = vq->vring.avail->ring[last_avail % vq->vring.num];
	lg_last_avail(vq)++;

	
	if (head >= vq->vring.num)
		errx(1, "Guest says index %u is available", head);

	
	*out_num = *in_num = 0;

	max = vq->vring.num;
	desc = vq->vring.desc;
	i = head;

	if (desc[i].flags & VRING_DESC_F_INDIRECT) {
		if (desc[i].len % sizeof(struct vring_desc))
			errx(1, "Invalid size for indirect buffer table");

		max = desc[i].len / sizeof(struct vring_desc);
		desc = check_pointer(desc[i].addr, desc[i].len);
		i = 0;
	}

	do {
		
		iov[*out_num + *in_num].iov_len = desc[i].len;
		iov[*out_num + *in_num].iov_base
			= check_pointer(desc[i].addr, desc[i].len);
		
		if (desc[i].flags & VRING_DESC_F_WRITE)
			(*in_num)++;
		else {
			if (*in_num)
				errx(1, "Descriptor has out after in");
			(*out_num)++;
		}

		
		if (*out_num + *in_num > max)
			errx(1, "Looped descriptor");
	} while ((i = next_desc(desc, i, max)) != max);

	return head;
}

static void add_used(struct virtqueue *vq, unsigned int head, int len)
{
	struct vring_used_elem *used;

	used = &vq->vring.used->ring[vq->vring.used->idx % vq->vring.num];
	used->id = head;
	used->len = len;
	/* Make sure buffer is written before we update index. */
	wmb();
	vq->vring.used->idx++;
	vq->pending_used++;
}

static void add_used_and_trigger(struct virtqueue *vq, unsigned head, int len)
{
	add_used(vq, head, len);
	trigger_irq(vq);
}

struct console_abort {
	
	int count;
	
	struct timeval start;
};

static void console_input(struct virtqueue *vq)
{
	int len;
	unsigned int head, in_num, out_num;
	struct console_abort *abort = vq->dev->priv;
	struct iovec iov[vq->vring.num];

	
	head = wait_for_vq_desc(vq, iov, &out_num, &in_num);
	if (out_num)
		errx(1, "Output buffers in console in queue?");

	
	len = readv(STDIN_FILENO, iov, in_num);
	if (len <= 0) {
		
		warnx("Failed to get console input, ignoring console.");
		for (;;)
			pause();
	}

	
	add_used_and_trigger(vq, head, len);

	if (len != 1 || ((char *)iov[0].iov_base)[0] != 3) {
		abort->count = 0;
		return;
	}

	abort->count++;
	if (abort->count == 1)
		gettimeofday(&abort->start, NULL);
	else if (abort->count == 3) {
		struct timeval now;
		gettimeofday(&now, NULL);
		
		if (now.tv_sec <= abort->start.tv_sec+1)
			kill(0, SIGINT);
		abort->count = 0;
	}
}

static void console_output(struct virtqueue *vq)
{
	unsigned int head, out, in;
	struct iovec iov[vq->vring.num];

	
	head = wait_for_vq_desc(vq, iov, &out, &in);
	if (in)
		errx(1, "Input buffers in console output queue?");

	
	while (!iov_empty(iov, out)) {
		int len = writev(STDOUT_FILENO, iov, out);
		if (len <= 0) {
			warn("Write to stdout gave %i (%d)", len, errno);
			break;
		}
		iov_consume(iov, out, len);
	}

	add_used(vq, head, 0);
}

struct net_info {
	int tunfd;
};

static void net_output(struct virtqueue *vq)
{
	struct net_info *net_info = vq->dev->priv;
	unsigned int head, out, in;
	struct iovec iov[vq->vring.num];

	
	head = wait_for_vq_desc(vq, iov, &out, &in);
	if (in)
		errx(1, "Input buffers in net output queue?");
	if (writev(net_info->tunfd, iov, out) < 0)
		warnx("Write to tun failed (%d)?", errno);

	add_used(vq, head, 0);
}

static bool will_block(int fd)
{
	fd_set fdset;
	struct timeval zero = { 0, 0 };
	FD_ZERO(&fdset);
	FD_SET(fd, &fdset);
	return select(fd+1, &fdset, NULL, NULL, &zero) != 1;
}

static void net_input(struct virtqueue *vq)
{
	int len;
	unsigned int head, out, in;
	struct iovec iov[vq->vring.num];
	struct net_info *net_info = vq->dev->priv;

	head = wait_for_vq_desc(vq, iov, &out, &in);
	if (out)
		errx(1, "Output buffers in net input queue?");

	if (vq->pending_used && will_block(net_info->tunfd))
		trigger_irq(vq);

	len = readv(net_info->tunfd, iov, in);
	if (len <= 0)
		warn("Failed to read from tun (%d).", errno);

	add_used(vq, head, len);
}

static int do_thread(void *_vq)
{
	struct virtqueue *vq = _vq;

	for (;;)
		vq->service(vq);
	return 0;
}

static void kill_launcher(int signal)
{
	kill(0, SIGTERM);
}

static void reset_device(struct device *dev)
{
	struct virtqueue *vq;

	verbose("Resetting device %s\n", dev->name);

	
	memset(get_feature_bits(dev) + dev->feature_len, 0, dev->feature_len);

	
	signal(SIGCHLD, SIG_IGN);

	
	for (vq = dev->vq; vq; vq = vq->next) {
		if (vq->thread != (pid_t)-1) {
			kill(vq->thread, SIGTERM);
			waitpid(vq->thread, NULL, 0);
			vq->thread = (pid_t)-1;
		}
		memset(vq->vring.desc, 0,
		       vring_size(vq->config.num, LGUEST_VRING_ALIGN));
		lg_last_avail(vq) = 0;
	}
	dev->running = false;

	
	signal(SIGCHLD, (void *)kill_launcher);
}

static void create_thread(struct virtqueue *vq)
{
	char *stack = malloc(32768);
	unsigned long args[] = { LHREQ_EVENTFD,
				 vq->config.pfn*getpagesize(), 0 };

	
	vq->eventfd = eventfd(0, 0);
	if (vq->eventfd < 0)
		err(1, "Creating eventfd");
	args[2] = vq->eventfd;

	if (write(lguest_fd, &args, sizeof(args)) != 0)
		err(1, "Attaching eventfd");

	vq->thread = clone(do_thread, stack + 32768, CLONE_VM | SIGCHLD, vq);
	if (vq->thread == (pid_t)-1)
		err(1, "Creating clone");

	
	close(vq->eventfd);
}

static void start_device(struct device *dev)
{
	unsigned int i;
	struct virtqueue *vq;

	verbose("Device %s OK: offered", dev->name);
	for (i = 0; i < dev->feature_len; i++)
		verbose(" %02x", get_feature_bits(dev)[i]);
	verbose(", accepted");
	for (i = 0; i < dev->feature_len; i++)
		verbose(" %02x", get_feature_bits(dev)
			[dev->feature_len+i]);

	for (vq = dev->vq; vq; vq = vq->next) {
		if (vq->service)
			create_thread(vq);
	}
	dev->running = true;
}

static void cleanup_devices(void)
{
	struct device *dev;

	for (dev = devices.dev; dev; dev = dev->next)
		reset_device(dev);

	
	if (orig_term.c_lflag & (ISIG|ICANON|ECHO))
		tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
}

static void update_device_status(struct device *dev)
{
	
	if (dev->desc->status == 0)
		reset_device(dev);
	else if (dev->desc->status & VIRTIO_CONFIG_S_FAILED) {
		warnx("Device %s configuration FAILED", dev->name);
		if (dev->running)
			reset_device(dev);
	} else {
		if (dev->running)
			err(1, "Device %s features finalized twice", dev->name);
		start_device(dev);
	}
}

static void handle_output(unsigned long addr)
{
	struct device *i;

	
	for (i = devices.dev; i; i = i->next) {
		struct virtqueue *vq;

		if (from_guest_phys(addr) == i->desc) {
			update_device_status(i);
			return;
		}

		
		for (vq = i->vq; vq; vq = vq->next) {
			if (addr != vq->config.pfn*getpagesize())
				continue;
			errx(1, "Notification on %s before setup!", i->name);
		}
	}

	if (addr >= guest_limit)
		errx(1, "Bad NOTIFY %#lx", addr);

	write(STDOUT_FILENO, from_guest_phys(addr),
	      strnlen(from_guest_phys(addr), guest_limit - addr));
}


static u8 *device_config(const struct device *dev)
{
	return (void *)(dev->desc + 1)
		+ dev->num_vq * sizeof(struct lguest_vqconfig)
		+ dev->feature_len * 2;
}

static struct lguest_device_desc *new_dev_desc(u16 type)
{
	struct lguest_device_desc d = { .type = type };
	void *p;

	
	if (devices.lastdev)
		p = device_config(devices.lastdev)
			+ devices.lastdev->desc->config_len;
	else
		p = devices.descpage;

	
	if (p + sizeof(d) > (void *)devices.descpage + getpagesize())
		errx(1, "Too many devices");

	
	return memcpy(p, &d, sizeof(d));
}

static void add_virtqueue(struct device *dev, unsigned int num_descs,
			  void (*service)(struct virtqueue *))
{
	unsigned int pages;
	struct virtqueue **i, *vq = malloc(sizeof(*vq));
	void *p;

	
	pages = (vring_size(num_descs, LGUEST_VRING_ALIGN) + getpagesize() - 1)
		/ getpagesize();
	p = get_pages(pages);

	
	vq->next = NULL;
	vq->last_avail_idx = 0;
	vq->dev = dev;

	vq->service = service;
	vq->thread = (pid_t)-1;

	
	vq->config.num = num_descs;
	vq->config.irq = devices.next_irq++;
	vq->config.pfn = to_guest_phys(p) / getpagesize();

	
	vring_init(&vq->vring, num_descs, p, LGUEST_VRING_ALIGN);

	assert(dev->desc->config_len == 0 && dev->desc->feature_len == 0);
	memcpy(device_config(dev), &vq->config, sizeof(vq->config));
	dev->num_vq++;
	dev->desc->num_vq++;

	verbose("Virtqueue page %#lx\n", to_guest_phys(p));

	for (i = &dev->vq; *i; i = &(*i)->next);
	*i = vq;
}

static void add_feature(struct device *dev, unsigned bit)
{
	u8 *features = get_feature_bits(dev);

	
	if (dev->desc->feature_len <= bit / CHAR_BIT) {
		assert(dev->desc->config_len == 0);
		dev->feature_len = dev->desc->feature_len = (bit/CHAR_BIT) + 1;
	}

	features[bit / CHAR_BIT] |= (1 << (bit % CHAR_BIT));
}

static void set_config(struct device *dev, unsigned len, const void *conf)
{
	
	if (device_config(dev) + len > devices.descpage + getpagesize())
		errx(1, "Too many devices");

	
	memcpy(device_config(dev), conf, len);
	dev->desc->config_len = len;

	
	assert(dev->desc->config_len == len);
}

static struct device *new_device(const char *name, u16 type)
{
	struct device *dev = malloc(sizeof(*dev));

	
	dev->desc = new_dev_desc(type);
	dev->name = name;
	dev->vq = NULL;
	dev->feature_len = 0;
	dev->num_vq = 0;
	dev->running = false;

	if (devices.lastdev)
		devices.lastdev->next = dev;
	else
		devices.dev = dev;
	devices.lastdev = dev;

	return dev;
}

static void setup_console(void)
{
	struct device *dev;

	
	if (tcgetattr(STDIN_FILENO, &orig_term) == 0) {
		struct termios term = orig_term;
		term.c_lflag &= ~(ISIG|ICANON|ECHO);
		tcsetattr(STDIN_FILENO, TCSANOW, &term);
	}

	dev = new_device("console", VIRTIO_ID_CONSOLE);

	
	dev->priv = malloc(sizeof(struct console_abort));
	((struct console_abort *)dev->priv)->count = 0;

	add_virtqueue(dev, VIRTQUEUE_NUM, console_input);
	add_virtqueue(dev, VIRTQUEUE_NUM, console_output);

	verbose("device %u: console\n", ++devices.device_num);
}


static u32 str2ip(const char *ipaddr)
{
	unsigned int b[4];

	if (sscanf(ipaddr, "%u.%u.%u.%u", &b[0], &b[1], &b[2], &b[3]) != 4)
		errx(1, "Failed to parse IP address '%s'", ipaddr);
	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

static void str2mac(const char *macaddr, unsigned char mac[6])
{
	unsigned int m[6];
	if (sscanf(macaddr, "%02x:%02x:%02x:%02x:%02x:%02x",
		   &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6)
		errx(1, "Failed to parse mac address '%s'", macaddr);
	mac[0] = m[0];
	mac[1] = m[1];
	mac[2] = m[2];
	mac[3] = m[3];
	mac[4] = m[4];
	mac[5] = m[5];
}

static void add_to_bridge(int fd, const char *if_name, const char *br_name)
{
	int ifidx;
	struct ifreq ifr;

	if (!*br_name)
		errx(1, "must specify bridge name");

	ifidx = if_nametoindex(if_name);
	if (!ifidx)
		errx(1, "interface %s does not exist!", if_name);

	strncpy(ifr.ifr_name, br_name, IFNAMSIZ);
	ifr.ifr_name[IFNAMSIZ-1] = '\0';
	ifr.ifr_ifindex = ifidx;
	if (ioctl(fd, SIOCBRADDIF, &ifr) < 0)
		err(1, "can't add %s to bridge %s", if_name, br_name);
}

static void configure_device(int fd, const char *tapif, u32 ipaddr)
{
	struct ifreq ifr;
	struct sockaddr_in sin;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, tapif);

	
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(ipaddr);
	memcpy(&ifr.ifr_addr, &sin, sizeof(sin));
	if (ioctl(fd, SIOCSIFADDR, &ifr) != 0)
		err(1, "Setting %s interface address", tapif);
	ifr.ifr_flags = IFF_UP;
	if (ioctl(fd, SIOCSIFFLAGS, &ifr) != 0)
		err(1, "Bringing interface %s up", tapif);
}

static int get_tun_device(char tapif[IFNAMSIZ])
{
	struct ifreq ifr;
	int netfd;

	
	memset(&ifr, 0, sizeof(ifr));

	netfd = open_or_die("/dev/net/tun", O_RDWR);
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI | IFF_VNET_HDR;
	strcpy(ifr.ifr_name, "tap%d");
	if (ioctl(netfd, TUNSETIFF, &ifr) != 0)
		err(1, "configuring /dev/net/tun");

	if (ioctl(netfd, TUNSETOFFLOAD,
		  TUN_F_CSUM|TUN_F_TSO4|TUN_F_TSO6|TUN_F_TSO_ECN) != 0)
		err(1, "Could not set features for tun device");

	ioctl(netfd, TUNSETNOCSUM, 1);

	memcpy(tapif, ifr.ifr_name, IFNAMSIZ);
	return netfd;
}

static void setup_tun_net(char *arg)
{
	struct device *dev;
	struct net_info *net_info = malloc(sizeof(*net_info));
	int ipfd;
	u32 ip = INADDR_ANY;
	bool bridging = false;
	char tapif[IFNAMSIZ], *p;
	struct virtio_net_config conf;

	net_info->tunfd = get_tun_device(tapif);

	
	dev = new_device("net", VIRTIO_ID_NET);
	dev->priv = net_info;

	
	add_virtqueue(dev, VIRTQUEUE_NUM, net_input);
	add_virtqueue(dev, VIRTQUEUE_NUM, net_output);

	ipfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (ipfd < 0)
		err(1, "opening IP socket");

	
	if (!strncmp(BRIDGE_PFX, arg, strlen(BRIDGE_PFX))) {
		arg += strlen(BRIDGE_PFX);
		bridging = true;
	}

	
	p = strchr(arg, ':');
	if (p) {
		str2mac(p+1, conf.mac);
		add_feature(dev, VIRTIO_NET_F_MAC);
		*p = '\0';
	}

	
	if (bridging)
		add_to_bridge(ipfd, tapif, arg);
	else
		ip = str2ip(arg);

	
	configure_device(ipfd, tapif, ip);

	
	add_feature(dev, VIRTIO_NET_F_CSUM);
	add_feature(dev, VIRTIO_NET_F_GUEST_CSUM);
	add_feature(dev, VIRTIO_NET_F_GUEST_TSO4);
	add_feature(dev, VIRTIO_NET_F_GUEST_TSO6);
	add_feature(dev, VIRTIO_NET_F_GUEST_ECN);
	add_feature(dev, VIRTIO_NET_F_HOST_TSO4);
	add_feature(dev, VIRTIO_NET_F_HOST_TSO6);
	add_feature(dev, VIRTIO_NET_F_HOST_ECN);
	
	add_feature(dev, VIRTIO_RING_F_INDIRECT_DESC);
	set_config(dev, sizeof(conf), &conf);

	
	close(ipfd);

	devices.device_num++;

	if (bridging)
		verbose("device %u: tun %s attached to bridge: %s\n",
			devices.device_num, tapif, arg);
	else
		verbose("device %u: tun %s: %s\n",
			devices.device_num, tapif, arg);
}

struct vblk_info {
	
	off64_t len;

	
	int fd;

};

static void blk_request(struct virtqueue *vq)
{
	struct vblk_info *vblk = vq->dev->priv;
	unsigned int head, out_num, in_num, wlen;
	int ret;
	u8 *in;
	struct virtio_blk_outhdr *out;
	struct iovec iov[vq->vring.num];
	off64_t off;

	head = wait_for_vq_desc(vq, iov, &out_num, &in_num);

	if (out_num == 0 || in_num == 0)
		errx(1, "Bad virtblk cmd %u out=%u in=%u",
		     head, out_num, in_num);

	out = convert(&iov[0], struct virtio_blk_outhdr);
	in = convert(&iov[out_num+in_num-1], u8);
	off = out->sector * 512;

	if (out->type & VIRTIO_BLK_T_SCSI_CMD) {
		fprintf(stderr, "Scsi commands unsupported\n");
		*in = VIRTIO_BLK_S_UNSUPP;
		wlen = sizeof(*in);
	} else if (out->type & VIRTIO_BLK_T_OUT) {
		if (lseek64(vblk->fd, off, SEEK_SET) != off)
			err(1, "Bad seek to sector %llu", out->sector);

		ret = writev(vblk->fd, iov+1, out_num-1);
		verbose("WRITE to sector %llu: %i\n", out->sector, ret);

		if (ret > 0 && off + ret > vblk->len) {
			
			ftruncate64(vblk->fd, vblk->len);
			
			errx(1, "Write past end %llu+%u", off, ret);
		}

		wlen = sizeof(*in);
		*in = (ret >= 0 ? VIRTIO_BLK_S_OK : VIRTIO_BLK_S_IOERR);
	} else if (out->type & VIRTIO_BLK_T_FLUSH) {
		
		ret = fdatasync(vblk->fd);
		verbose("FLUSH fdatasync: %i\n", ret);
		wlen = sizeof(*in);
		*in = (ret >= 0 ? VIRTIO_BLK_S_OK : VIRTIO_BLK_S_IOERR);
	} else {
		if (lseek64(vblk->fd, off, SEEK_SET) != off)
			err(1, "Bad seek to sector %llu", out->sector);

		ret = readv(vblk->fd, iov+1, in_num-1);
		verbose("READ from sector %llu: %i\n", out->sector, ret);
		if (ret >= 0) {
			wlen = sizeof(*in) + ret;
			*in = VIRTIO_BLK_S_OK;
		} else {
			wlen = sizeof(*in);
			*in = VIRTIO_BLK_S_IOERR;
		}
	}

	
	add_used(vq, head, wlen);
}

static void setup_block_file(const char *filename)
{
	struct device *dev;
	struct vblk_info *vblk;
	struct virtio_blk_config conf;

	
	dev = new_device("block", VIRTIO_ID_BLOCK);

	
	add_virtqueue(dev, VIRTQUEUE_NUM, blk_request);

	
	vblk = dev->priv = malloc(sizeof(*vblk));

	
	vblk->fd = open_or_die(filename, O_RDWR|O_LARGEFILE);
	vblk->len = lseek64(vblk->fd, 0, SEEK_END);

	
	add_feature(dev, VIRTIO_BLK_F_FLUSH);

	
	conf.capacity = cpu_to_le64(vblk->len / 512);

	add_feature(dev, VIRTIO_BLK_F_SEG_MAX);
	conf.seg_max = cpu_to_le32(VIRTQUEUE_NUM - 2);

	
	set_config(dev, offsetof(struct virtio_blk_config, geometry), &conf);

	verbose("device %u: virtblock %llu sectors\n",
		++devices.device_num, le64_to_cpu(conf.capacity));
}

struct rng_info {
	int rfd;
};

static void rng_input(struct virtqueue *vq)
{
	int len;
	unsigned int head, in_num, out_num, totlen = 0;
	struct rng_info *rng_info = vq->dev->priv;
	struct iovec iov[vq->vring.num];

	
	head = wait_for_vq_desc(vq, iov, &out_num, &in_num);
	if (out_num)
		errx(1, "Output buffers in rng?");

	while (!iov_empty(iov, in_num)) {
		len = readv(rng_info->rfd, iov, in_num);
		if (len <= 0)
			err(1, "Read from /dev/random gave %i", len);
		iov_consume(iov, in_num, len);
		totlen += len;
	}

	
	add_used(vq, head, totlen);
}

static void setup_rng(void)
{
	struct device *dev;
	struct rng_info *rng_info = malloc(sizeof(*rng_info));

	
	rng_info->rfd = open_or_die("/dev/random", O_RDONLY);

	
	dev = new_device("rng", VIRTIO_ID_RNG);
	dev->priv = rng_info;

	
	add_virtqueue(dev, VIRTQUEUE_NUM, rng_input);

	verbose("device %u: rng\n", devices.device_num++);
}

static void __attribute__((noreturn)) restart_guest(void)
{
	unsigned int i;

	for (i = 3; i < FD_SETSIZE; i++)
		close(i);

	
	cleanup_devices();

	execv(main_args[0], main_args);
	err(1, "Could not exec %s", main_args[0]);
}

static void __attribute__((noreturn)) run_guest(void)
{
	for (;;) {
		unsigned long notify_addr;
		int readval;

		
		readval = pread(lguest_fd, &notify_addr,
				sizeof(notify_addr), cpu_id);

		
		if (readval == sizeof(notify_addr)) {
			verbose("Notify on address %#lx\n", notify_addr);
			handle_output(notify_addr);
		
		} else if (errno == ENOENT) {
			char reason[1024] = { 0 };
			pread(lguest_fd, reason, sizeof(reason)-1, cpu_id);
			errx(1, "%s", reason);
		
		} else if (errno == ERESTART) {
			restart_guest();
		
		} else
			err(1, "Running guest failed");
	}
}

static struct option opts[] = {
	{ "verbose", 0, NULL, 'v' },
	{ "tunnet", 1, NULL, 't' },
	{ "block", 1, NULL, 'b' },
	{ "rng", 0, NULL, 'r' },
	{ "initrd", 1, NULL, 'i' },
	{ "username", 1, NULL, 'u' },
	{ "chroot", 1, NULL, 'c' },
	{ NULL },
};
static void usage(void)
{
	errx(1, "Usage: lguest [--verbose] "
	     "[--tunnet=(<ipaddr>:<macaddr>|bridge:<bridgename>:<macaddr>)\n"
	     "|--block=<filename>|--initrd=<filename>]...\n"
	     "<mem-in-mb> vmlinux [args...]");
}

int main(int argc, char *argv[])
{
	
	unsigned long mem = 0, start, initrd_size = 0;
	
	int i, c;
	
	struct boot_params *boot;
	
	const char *initrd_name = NULL;

	
	struct passwd *user_details = NULL;

	
	char *chroot_path = NULL;

	
	main_args = argv;

	devices.lastdev = NULL;
	devices.next_irq = 1;

	
	cpu_id = 0;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			mem = atoi(argv[i]) * 1024 * 1024;
			guest_base = map_zeroed_pages(mem / getpagesize()
						      + DEVICE_PAGES);
			guest_limit = mem;
			guest_max = mem + DEVICE_PAGES*getpagesize();
			devices.descpage = get_pages(1);
			break;
		}
	}

	
	while ((c = getopt_long(argc, argv, "v", opts, NULL)) != EOF) {
		switch (c) {
		case 'v':
			verbose = true;
			break;
		case 't':
			setup_tun_net(optarg);
			break;
		case 'b':
			setup_block_file(optarg);
			break;
		case 'r':
			setup_rng();
			break;
		case 'i':
			initrd_name = optarg;
			break;
		case 'u':
			user_details = getpwnam(optarg);
			if (!user_details)
				err(1, "getpwnam failed, incorrect username?");
			break;
		case 'c':
			chroot_path = optarg;
			break;
		default:
			warnx("Unknown argument %s", argv[optind]);
			usage();
		}
	}
	if (optind + 2 > argc)
		usage();

	verbose("Guest base is at %p\n", guest_base);

	
	setup_console();

	
	start = load_kernel(open_or_die(argv[optind+1], O_RDONLY));

	
	boot = from_guest_phys(0);

	
	if (initrd_name) {
		initrd_size = load_initrd(initrd_name, mem);
		boot->hdr.ramdisk_image = mem - initrd_size;
		boot->hdr.ramdisk_size = initrd_size;
		
		boot->hdr.type_of_loader = 0xFF;
	}

	boot->e820_entries = 1;
	boot->e820_map[0] = ((struct e820entry) { 0, mem, E820_RAM });
	boot->hdr.cmd_line_ptr = to_guest_phys(boot + 1);
	
	concat((char *)(boot + 1), argv+optind+2);

	
	boot->hdr.kernel_alignment = 0x1000000;

	
	boot->hdr.version = 0x207;

	
	boot->hdr.hardware_subarch = 1;

	
	boot->hdr.loadflags |= KEEP_SEGMENTS;

	
	tell_kernel(start);

	
	signal(SIGCHLD, kill_launcher);

	
	atexit(cleanup_devices);

	
	if (chroot_path) {
		if (chroot(chroot_path) != 0)
			err(1, "chroot(\"%s\") failed", chroot_path);

		if (chdir("/") != 0)
			err(1, "chdir(\"/\") failed");

		verbose("chroot done\n");
	}

	
	if (user_details) {
		uid_t u;
		gid_t g;

		u = user_details->pw_uid;
		g = user_details->pw_gid;

		if (initgroups(user_details->pw_name, g) != 0)
			err(1, "initgroups failed");

		if (setresgid(g, g, g) != 0)
			err(1, "setresgid failed");

		if (setresuid(u, u, u) != 0)
			err(1, "setresuid failed");

		verbose("Dropping privileges completed\n");
	}

	
	run_guest();
}

