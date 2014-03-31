
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in.h>
#ifdef __sun__
#include <inttypes.h>
#else
#include <stdint.h>
#endif

typedef struct boot_block {
	uint32_t bb_magic;		
	uint32_t bb_dest;		
	uint32_t bb_num_512blocks;	
	uint32_t bb_debug_flag;	
	uint32_t bb_entry_point;	
	uint32_t bb_checksum;	
	uint32_t reserved[2];
} boot_block_t;

#define IMGBLK	512
unsigned int	tmpbuf[IMGBLK / sizeof(unsigned int)];

int main(int argc, char *argv[])
{
	int	in_fd, out_fd;
	int	nblks, i;
	unsigned int	cksum, *cp;
	struct	stat	st;
	boot_block_t	bt;

	if (argc < 5) {
		fprintf(stderr, "usage: %s <zImage-file> <boot-image> <load address> <entry point>\n",argv[0]);
		exit(1);
	}

	if (stat(argv[1], &st) < 0) {
		perror("stat");
		exit(2);
	}

	nblks = (st.st_size + IMGBLK) / IMGBLK;

	bt.bb_magic = htonl(0x0052504F);

	
	bt.bb_dest = htonl(strtoul(argv[3], NULL, 0));
	bt.bb_entry_point = htonl(strtoul(argv[4], NULL, 0));

	bt.bb_num_512blocks = htonl(nblks);
	bt.bb_debug_flag = 0;

	bt.bb_checksum = 0;

	bt.reserved[0] = 0;
	bt.reserved[1] = 0;

	if ((in_fd = open(argv[1], O_RDONLY)) < 0) {
		perror("zImage open");
		exit(3);
	}

	if ((out_fd = open(argv[2], (O_RDWR | O_CREAT | O_TRUNC), 0666)) < 0) {
		perror("bootfile open");
		exit(3);
	}

	cksum = 0;
	cp = (void *)&bt;
	for (i = 0; i < sizeof(bt) / sizeof(unsigned int); i++)
		cksum += *cp++;

	if (read(in_fd, tmpbuf, sizeof(tmpbuf)) != sizeof(tmpbuf)) {
		fprintf(stderr, "%s is too small to be an ELF image\n",
				argv[1]);
		exit(4);
	}

	if (tmpbuf[0] != htonl(0x7f454c46)) {
		fprintf(stderr, "%s is not an ELF image\n", argv[1]);
		exit(4);
	}

	if (lseek(in_fd, (64 * 1024), SEEK_SET) < 0) {
		fprintf(stderr, "%s failed to seek in ELF image\n", argv[1]);
		exit(4);
	}

	nblks -= (64 * 1024) / IMGBLK;

	if (write(out_fd, &bt, sizeof(bt)) != sizeof(bt)) {
		perror("boot-image write");
		exit(5);
	}

	while (nblks-- > 0) {
		if (read(in_fd, tmpbuf, sizeof(tmpbuf)) < 0) {
			perror("zImage read");
			exit(5);
		}
		cp = tmpbuf;
		for (i = 0; i < sizeof(tmpbuf) / sizeof(unsigned int); i++)
			cksum += *cp++;
		if (write(out_fd, tmpbuf, sizeof(tmpbuf)) != sizeof(tmpbuf)) {
			perror("boot-image write");
			exit(5);
		}
	}

	bt.bb_checksum = htonl(cksum);
	if (lseek(out_fd, 0, SEEK_SET) < 0) {
		perror("rewrite seek");
		exit(1);
	}
	if (write(out_fd, &bt, sizeof(bt)) != sizeof(bt)) {
		perror("boot-image rewrite");
		exit(1);
	}

	exit(0);
}
