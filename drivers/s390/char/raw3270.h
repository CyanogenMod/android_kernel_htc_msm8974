/*
 * IBM/3270 Driver
 *
 * Author(s):
 *   Original 3270 Code for 2.4 written by Richard Hitt (UTS Global)
 *   Rewritten for 2.5 by Martin Schwidefsky <schwidefsky@de.ibm.com>
 *     Copyright IBM Corp. 2003, 2009
 */

#include <asm/idals.h>
#include <asm/ioctl.h>

#define TUBICMD		_IO('3', 3)	
#define TUBOCMD		_IO('3', 4)	
#define TUBGETI		_IO('3', 7)	
#define TUBGETO		_IO('3', 8)	
#define TUBSETMOD	_IO('3',12)	
#define TUBGETMOD	_IO('3',13)	

#define TC_WRITE	0x01		
#define TC_RDBUF	0x02		
#define TC_EWRITE	0x05		
#define TC_READMOD	0x06		
#define TC_EWRITEA	0x0d		
#define TC_WRITESF	0x11		

#define TO_SF		0x1d		
#define TO_SBA		0x11		
#define TO_IC		0x13		
#define TO_PT		0x05		
#define TO_RA		0x3c		
#define TO_SFE		0x29		
#define TO_EUA		0x12		
#define TO_MF		0x2c		
#define TO_SA		0x28		

#define TF_INPUT	0x40		
#define TF_INPUTN	0x4c		
#define TF_INMDT	0xc1		
#define TF_LOG		0x60

#define TAT_RESET	0x00
#define TAT_FIELD	0xc0
#define TAT_EXTHI	0x41
#define TAT_COLOR	0x42
#define TAT_CHARS	0x43
#define TAT_TRANS	0x46

#define TAX_RESET	0x00
#define TAX_BLINK	0xf1
#define TAX_REVER	0xf2
#define TAX_UNDER	0xf4

#define TAR_RESET	0x00

#define TAC_RESET	0x00
#define TAC_BLUE	0xf1
#define TAC_RED		0xf2
#define TAC_PINK	0xf3
#define TAC_GREEN	0xf4
#define TAC_TURQ	0xf5
#define TAC_YELLOW	0xf6
#define TAC_WHITE	0xf7
#define TAC_DEFAULT	0x00

#define TW_NONE		0x40		
#define TW_KR		0xc2		
#define TW_PLUSALARM	0x04		

#define RAW3270_FIRSTMINOR	1	
#define RAW3270_MAXDEVS		255	

struct raw3270_iocb {
	short model;
	short line_cnt;
	short col_cnt;
	short pf_cnt;
	short re_cnt;
	short map;
};

struct raw3270;
struct raw3270_view;

struct raw3270_request {
	struct list_head list;		
	struct raw3270_view *view;	
	struct ccw1 ccw;		
	void *buffer;			
	size_t size;			
	int rescnt;			
	int rc;				

	
	void (*callback)(struct raw3270_request *, void *);
	void *callback_data;
};

struct raw3270_request *raw3270_request_alloc(size_t size);
struct raw3270_request *raw3270_request_alloc_bootmem(size_t size);
void raw3270_request_free(struct raw3270_request *);
void raw3270_request_reset(struct raw3270_request *);
void raw3270_request_set_cmd(struct raw3270_request *, u8 cmd);
int  raw3270_request_add_data(struct raw3270_request *, void *, size_t);
void raw3270_request_set_data(struct raw3270_request *, void *, size_t);
void raw3270_request_set_idal(struct raw3270_request *, struct idal_buffer *);

static inline int
raw3270_request_final(struct raw3270_request *rq)
{
	return list_empty(&rq->list);
}

void raw3270_buffer_address(struct raw3270 *, char *, unsigned short);

#define RAW3270_IO_DONE		0	
#define RAW3270_IO_BUSY		1	
#define RAW3270_IO_RETRY	2	
#define RAW3270_IO_STOP		3	

struct raw3270_fn {
	int  (*activate)(struct raw3270_view *);
	void (*deactivate)(struct raw3270_view *);
	int  (*intv)(struct raw3270_view *,
		     struct raw3270_request *, struct irb *);
	void (*release)(struct raw3270_view *);
	void (*free)(struct raw3270_view *);
};

struct raw3270_view {
	struct list_head list;
	spinlock_t lock;
	atomic_t ref_count;
	struct raw3270 *dev;
	struct raw3270_fn *fn;
	unsigned int model;
	unsigned int rows, cols;	
	unsigned char *ascebc;		
};

int raw3270_add_view(struct raw3270_view *, struct raw3270_fn *, int);
int raw3270_activate_view(struct raw3270_view *);
void raw3270_del_view(struct raw3270_view *);
void raw3270_deactivate_view(struct raw3270_view *);
struct raw3270_view *raw3270_find_view(struct raw3270_fn *, int);
int raw3270_start(struct raw3270_view *, struct raw3270_request *);
int raw3270_start_locked(struct raw3270_view *, struct raw3270_request *);
int raw3270_start_irq(struct raw3270_view *, struct raw3270_request *);
int raw3270_reset(struct raw3270_view *);
struct raw3270_view *raw3270_view(struct raw3270_view *);

static inline void
raw3270_get_view(struct raw3270_view *view)
{
	atomic_inc(&view->ref_count);
}

extern wait_queue_head_t raw3270_wait_queue;

static inline void
raw3270_put_view(struct raw3270_view *view)
{
	if (atomic_dec_return(&view->ref_count) == 0)
		wake_up(&raw3270_wait_queue);
}

struct raw3270 *raw3270_setup_console(struct ccw_device *cdev);
void raw3270_wait_cons_dev(struct raw3270 *);

int raw3270_register_notifier(void (*notifier)(int, int));
void raw3270_unregister_notifier(void (*notifier)(int, int));
void raw3270_pm_unfreeze(struct raw3270_view *);

struct string
{
	struct list_head list;
	struct list_head update;
	unsigned long size;
	unsigned long len;
	char string[0];
} __attribute__ ((aligned(8)));

static inline struct string *
alloc_string(struct list_head *free_list, unsigned long len)
{
	struct string *cs, *tmp;
	unsigned long size;

	size = (len + 7L) & -8L;
	list_for_each_entry(cs, free_list, list) {
		if (cs->size < size)
			continue;
		if (cs->size > size + sizeof(struct string)) {
			char *endaddr = (char *) (cs + 1) + cs->size;
			tmp = (struct string *) (endaddr - size) - 1;
			tmp->size = size;
			cs->size -= size + sizeof(struct string);
			cs = tmp;
		} else
			list_del(&cs->list);
		cs->len = len;
		INIT_LIST_HEAD(&cs->list);
		INIT_LIST_HEAD(&cs->update);
		return cs;
	}
	return NULL;
}

static inline unsigned long
free_string(struct list_head *free_list, struct string *cs)
{
	struct string *tmp;
	struct list_head *p, *left;

	
	left = free_list;
	list_for_each(p, free_list) {
		if (list_entry(p, struct string, list) > cs)
			break;
		left = p;
	}
	
	if (left->next != free_list) {
		tmp = list_entry(left->next, struct string, list);
		if ((char *) (cs + 1) + cs->size == (char *) tmp) {
			list_del(&tmp->list);
			cs->size += tmp->size + sizeof(struct string);
		}
	}
	
	if (left != free_list) {
		tmp = list_entry(left, struct string, list);
		if ((char *) (tmp + 1) + tmp->size == (char *) cs) {
			tmp->size += cs->size + sizeof(struct string);
			return tmp->size;
		}
	}
	__list_add(&cs->list, left, left->next);
	return cs->size;
}

static inline void
add_string_memory(struct list_head *free_list, void *mem, unsigned long size)
{
	struct string *cs;

	cs = (struct string *) mem;
	cs->size = size - sizeof(struct string);
	free_string(free_list, cs);
}

