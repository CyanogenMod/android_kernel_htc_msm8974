/* This file distributed under the GNU GPL.                              */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <asm/io.h>
#include "hisax_if.h"
#include "hfc4s8s_l1.h"

static const char hfc4s8s_rev[] = "Revision: 1.10";

#define TRANS_FIFO_THRES 5

#define CLOCKMODE_0     0	
#define CLOCKMODE_1     1	
#define CHIP_ID_SHIFT   4
#define HFC_MAX_ST 8
#define MAX_D_FRAME_SIZE 270
#define MAX_B_FRAME_SIZE 1536
#define TRANS_TIMER_MODE (TRANS_FIFO_THRES & 0xf)
#define TRANS_FIFO_BYTES (2 << TRANS_FIFO_THRES)
#define MAX_F_CNT 0x0f

#define CLKDEL_NT 0x6c
#define CLKDEL_TE 0xf
#define CTRL0_NT  4
#define CTRL0_TE  0

#define L1_TIMER_T4 2		
#define L1_TIMER_T3 (7 * HZ)	
#define L1_TIMER_T1 ((120 * HZ) / 1000)	


static int card_cnt;

typedef struct {
	int chip_id;
	int clock_mode;
	int max_st_ports;
	char *device_name;
} hfc4s8s_param;

static struct pci_device_id hfc4s8s_ids[] = {
	{.vendor = PCI_VENDOR_ID_CCD,
	 .device = PCI_DEVICE_ID_4S,
	 .subvendor = 0x1397,
	 .subdevice = 0x08b4,
	 .driver_data =
	 (unsigned long) &((hfc4s8s_param) {CHIP_ID_4S, CLOCKMODE_0, 4,
				 "HFC-4S Evaluation Board"}),
	},
	{.vendor = PCI_VENDOR_ID_CCD,
	 .device = PCI_DEVICE_ID_8S,
	 .subvendor = 0x1397,
	 .subdevice = 0x16b8,
	 .driver_data =
	 (unsigned long) &((hfc4s8s_param) {CHIP_ID_8S, CLOCKMODE_0, 8,
				 "HFC-8S Evaluation Board"}),
	},
	{.vendor = PCI_VENDOR_ID_CCD,
	 .device = PCI_DEVICE_ID_4S,
	 .subvendor = 0x1397,
	 .subdevice = 0xb520,
	 .driver_data =
	 (unsigned long) &((hfc4s8s_param) {CHIP_ID_4S, CLOCKMODE_1, 4,
				 "IOB4ST"}),
	},
	{.vendor = PCI_VENDOR_ID_CCD,
	 .device = PCI_DEVICE_ID_8S,
	 .subvendor = 0x1397,
	 .subdevice = 0xb522,
	 .driver_data =
	 (unsigned long) &((hfc4s8s_param) {CHIP_ID_8S, CLOCKMODE_1, 8,
				 "IOB8ST"}),
	},
	{}
};

MODULE_DEVICE_TABLE(pci, hfc4s8s_ids);

MODULE_AUTHOR("Werner Cornelius, werner@cornelius-consult.de");
MODULE_DESCRIPTION("ISDN layer 1 for Cologne Chip HFC-4S/8S chips");
MODULE_LICENSE("GPL");

struct hfc4s8s_btype {
	spinlock_t lock;
	struct hisax_b_if b_if;
	struct hfc4s8s_l1 *l1p;
	struct sk_buff_head tx_queue;
	struct sk_buff *tx_skb;
	struct sk_buff *rx_skb;
	__u8 *rx_ptr;
	int tx_cnt;
	int bchan;
	int mode;
};

struct _hfc4s8s_hw;

struct hfc4s8s_l1 {
	spinlock_t lock;
	struct _hfc4s8s_hw *hw;	
	int l1_state;		
	struct timer_list l1_timer;	
	int nt_mode;		
	int st_num;		
	int enabled;		
	struct sk_buff_head d_tx_queue;	
	int tx_cnt;		
	struct hisax_d_if d_if;	
	struct hfc4s8s_btype b_ch[2];	
	struct hisax_b_if *b_table[2];
};

typedef struct _hfc4s8s_hw {
	spinlock_t lock;

	int cardnum;
	int ifnum;
	int iobase;
	int nt_mode;
	u_char *membase;
	u_char *hw_membase;
	void *pdev;
	int max_fifo;
	hfc4s8s_param driver_data;
	int irq;
	int fifo_sched_cnt;
	struct work_struct tqueue;
	struct hfc4s8s_l1 l1[HFC_MAX_ST];
	char card_name[60];
	struct {
		u_char r_irq_ctrl;
		u_char r_ctrl0;
		volatile u_char r_irq_statech;	
		u_char r_irqmsk_statchg;	
		u_char r_irq_fifo_blx[8];	
		u_char fifo_rx_trans_enables[8];	
		u_char fifo_slow_timer_service[8];	
		volatile u_char r_irq_oview;	
		volatile u_char timer_irq;
		int timer_usg_cnt;	
	} mr;
} hfc4s8s_hw;



#ifdef HISAX_HFC4S8S_PCIMEM	

#define Write_hfc8(a, b, c) {(*((volatile u_char *)(a->membase + b)) = c); inb(a->iobase + 4);}
#define fWrite_hfc8(a, b, c) (*((volatile u_char *)(a->membase + b)) = c)
#define Read_hfc8(a, b) (*((volatile u_char *)(a->membase + b)))
#define Write_hfc16(a, b, c) (*((volatile unsigned short *)(a->membase + b)) = c)
#define Read_hfc16(a, b) (*((volatile unsigned short *)(a->membase + b)))
#define Write_hfc32(a, b, c) (*((volatile unsigned long *)(a->membase + b)) = c)
#define Read_hfc32(a, b) (*((volatile unsigned long *)(a->membase + b)))
#define wait_busy(a) {while ((Read_hfc8(a, R_STATUS) & M_BUSY));}
#define PCI_ENA_MEMIO	0x03

#else

static inline void
SetRegAddr(hfc4s8s_hw *a, u_char b)
{
	outb(b, (a->iobase) + 4);
}

static inline u_char
GetRegAddr(hfc4s8s_hw *a)
{
	return (inb((volatile u_int) (a->iobase + 4)));
}


static inline void
Write_hfc8(hfc4s8s_hw *a, u_char b, u_char c)
{
	SetRegAddr(a, b);
	outb(c, a->iobase);
}

static inline void
fWrite_hfc8(hfc4s8s_hw *a, u_char c)
{
	outb(c, a->iobase);
}

static inline void
Write_hfc16(hfc4s8s_hw *a, u_char b, u_short c)
{
	SetRegAddr(a, b);
	outw(c, a->iobase);
}

static inline void
Write_hfc32(hfc4s8s_hw *a, u_char b, u_long c)
{
	SetRegAddr(a, b);
	outl(c, a->iobase);
}

static inline void
fWrite_hfc32(hfc4s8s_hw *a, u_long c)
{
	outl(c, a->iobase);
}

static inline u_char
Read_hfc8(hfc4s8s_hw *a, u_char b)
{
	SetRegAddr(a, b);
	return (inb((volatile u_int) a->iobase));
}

static inline u_char
fRead_hfc8(hfc4s8s_hw *a)
{
	return (inb((volatile u_int) a->iobase));
}


static inline u_short
Read_hfc16(hfc4s8s_hw *a, u_char b)
{
	SetRegAddr(a, b);
	return (inw((volatile u_int) a->iobase));
}

static inline u_long
Read_hfc32(hfc4s8s_hw *a, u_char b)
{
	SetRegAddr(a, b);
	return (inl((volatile u_int) a->iobase));
}

static inline u_long
fRead_hfc32(hfc4s8s_hw *a)
{
	return (inl((volatile u_int) a->iobase));
}

static inline void
wait_busy(hfc4s8s_hw *a)
{
	SetRegAddr(a, R_STATUS);
	while (inb((volatile u_int) a->iobase) & M_BUSY);
}

#define PCI_ENA_REGIO	0x01

#endif				

static u_char
Read_hfc8_stable(hfc4s8s_hw *hw, int reg)
{
	u_char ref8;
	u_char in8;
	ref8 = Read_hfc8(hw, reg);
	while (((in8 = Read_hfc8(hw, reg)) != ref8)) {
		ref8 = in8;
	}
	return in8;
}

static int
Read_hfc16_stable(hfc4s8s_hw *hw, int reg)
{
	int ref16;
	int in16;

	ref16 = Read_hfc16(hw, reg);
	while (((in16 = Read_hfc16(hw, reg)) != ref16)) {
		ref16 = in16;
	}
	return in16;
}

static void
dch_l2l1(struct hisax_d_if *iface, int pr, void *arg)
{
	struct hfc4s8s_l1 *l1 = iface->ifc.priv;
	struct sk_buff *skb = (struct sk_buff *) arg;
	u_long flags;

	switch (pr) {

	case (PH_DATA | REQUEST):
		if (!l1->enabled) {
			dev_kfree_skb(skb);
			break;
		}
		spin_lock_irqsave(&l1->lock, flags);
		skb_queue_tail(&l1->d_tx_queue, skb);
		if ((skb_queue_len(&l1->d_tx_queue) == 1) &&
		    (l1->tx_cnt <= 0)) {
			l1->hw->mr.r_irq_fifo_blx[l1->st_num] |=
				0x10;
			spin_unlock_irqrestore(&l1->lock, flags);
			schedule_work(&l1->hw->tqueue);
		} else
			spin_unlock_irqrestore(&l1->lock, flags);
		break;

	case (PH_ACTIVATE | REQUEST):
		if (!l1->enabled)
			break;
		if (!l1->nt_mode) {
			if (l1->l1_state < 6) {
				spin_lock_irqsave(&l1->lock,
						  flags);

				Write_hfc8(l1->hw, R_ST_SEL,
					   l1->st_num);
				Write_hfc8(l1->hw, A_ST_WR_STA,
					   0x60);
				mod_timer(&l1->l1_timer,
					  jiffies + L1_TIMER_T3);
				spin_unlock_irqrestore(&l1->lock,
						       flags);
			} else if (l1->l1_state == 7)
				l1->d_if.ifc.l1l2(&l1->d_if.ifc,
						  PH_ACTIVATE |
						  INDICATION,
						  NULL);
		} else {
			if (l1->l1_state != 3) {
				spin_lock_irqsave(&l1->lock,
						  flags);
				Write_hfc8(l1->hw, R_ST_SEL,
					   l1->st_num);
				Write_hfc8(l1->hw, A_ST_WR_STA,
					   0x60);
				spin_unlock_irqrestore(&l1->lock,
						       flags);
			} else if (l1->l1_state == 3)
				l1->d_if.ifc.l1l2(&l1->d_if.ifc,
						  PH_ACTIVATE |
						  INDICATION,
						  NULL);
		}
		break;

	default:
		printk(KERN_INFO
		       "HFC-4S/8S: Unknown D-chan cmd 0x%x received, ignored\n",
		       pr);
		break;
	}
	if (!l1->enabled)
		l1->d_if.ifc.l1l2(&l1->d_if.ifc,
				  PH_DEACTIVATE | INDICATION, NULL);
}				

static void
bch_l2l1(struct hisax_if *ifc, int pr, void *arg)
{
	struct hfc4s8s_btype *bch = ifc->priv;
	struct hfc4s8s_l1 *l1 = bch->l1p;
	struct sk_buff *skb = (struct sk_buff *) arg;
	long mode = (long) arg;
	u_long flags;

	switch (pr) {

	case (PH_DATA | REQUEST):
		if (!l1->enabled || (bch->mode == L1_MODE_NULL)) {
			dev_kfree_skb(skb);
			break;
		}
		spin_lock_irqsave(&l1->lock, flags);
		skb_queue_tail(&bch->tx_queue, skb);
		if (!bch->tx_skb && (bch->tx_cnt <= 0)) {
			l1->hw->mr.r_irq_fifo_blx[l1->st_num] |=
				((bch->bchan == 1) ? 1 : 4);
			spin_unlock_irqrestore(&l1->lock, flags);
			schedule_work(&l1->hw->tqueue);
		} else
			spin_unlock_irqrestore(&l1->lock, flags);
		break;

	case (PH_ACTIVATE | REQUEST):
	case (PH_DEACTIVATE | REQUEST):
		if (!l1->enabled)
			break;
		if (pr == (PH_DEACTIVATE | REQUEST))
			mode = L1_MODE_NULL;

		switch (mode) {
		case L1_MODE_HDLC:
			spin_lock_irqsave(&l1->lock,
					  flags);
			l1->hw->mr.timer_usg_cnt++;
			l1->hw->mr.
				fifo_slow_timer_service[l1->
							st_num]
				|=
				((bch->bchan ==
				  1) ? 0x2 : 0x8);
			Write_hfc8(l1->hw, R_FIFO,
				   (l1->st_num * 8 +
				    ((bch->bchan ==
				      1) ? 0 : 2)));
			wait_busy(l1->hw);
			Write_hfc8(l1->hw, A_CON_HDLC, 0xc);	
			Write_hfc8(l1->hw, A_SUBCH_CFG, 0);	
			Write_hfc8(l1->hw, A_IRQ_MSK, 1);	
			Write_hfc8(l1->hw, A_INC_RES_FIFO, 2);	
			wait_busy(l1->hw);

			Write_hfc8(l1->hw, R_FIFO,
				   (l1->st_num * 8 +
				    ((bch->bchan ==
				      1) ? 1 : 3)));
			wait_busy(l1->hw);
			Write_hfc8(l1->hw, A_CON_HDLC, 0xc);	
			Write_hfc8(l1->hw, A_SUBCH_CFG, 0);	
			Write_hfc8(l1->hw, A_IRQ_MSK, 1);	
			Write_hfc8(l1->hw, A_INC_RES_FIFO, 2);	

			Write_hfc8(l1->hw, R_ST_SEL,
				   l1->st_num);
			l1->hw->mr.r_ctrl0 |=
				(bch->bchan & 3);
			Write_hfc8(l1->hw, A_ST_CTRL0,
				   l1->hw->mr.r_ctrl0);
			bch->mode = L1_MODE_HDLC;
			spin_unlock_irqrestore(&l1->lock,
					       flags);

			bch->b_if.ifc.l1l2(&bch->b_if.ifc,
					   PH_ACTIVATE |
					   INDICATION,
					   NULL);
			break;

		case L1_MODE_TRANS:
			spin_lock_irqsave(&l1->lock,
					  flags);
			l1->hw->mr.
				fifo_rx_trans_enables[l1->
						      st_num]
				|=
				((bch->bchan ==
				  1) ? 0x2 : 0x8);
			l1->hw->mr.timer_usg_cnt++;
			Write_hfc8(l1->hw, R_FIFO,
				   (l1->st_num * 8 +
				    ((bch->bchan ==
				      1) ? 0 : 2)));
			wait_busy(l1->hw);
			Write_hfc8(l1->hw, A_CON_HDLC, 0xf);	
			Write_hfc8(l1->hw, A_SUBCH_CFG, 0);	
			Write_hfc8(l1->hw, A_IRQ_MSK, 0);	
			Write_hfc8(l1->hw, A_INC_RES_FIFO, 2);	
			wait_busy(l1->hw);

			Write_hfc8(l1->hw, R_FIFO,
				   (l1->st_num * 8 +
				    ((bch->bchan ==
				      1) ? 1 : 3)));
			wait_busy(l1->hw);
			Write_hfc8(l1->hw, A_CON_HDLC, 0xf);	
			Write_hfc8(l1->hw, A_SUBCH_CFG, 0);	
			Write_hfc8(l1->hw, A_IRQ_MSK, 0);	
			Write_hfc8(l1->hw, A_INC_RES_FIFO, 2);	

			Write_hfc8(l1->hw, R_ST_SEL,
				   l1->st_num);
			l1->hw->mr.r_ctrl0 |=
				(bch->bchan & 3);
			Write_hfc8(l1->hw, A_ST_CTRL0,
				   l1->hw->mr.r_ctrl0);
			bch->mode = L1_MODE_TRANS;
			spin_unlock_irqrestore(&l1->lock,
					       flags);

			bch->b_if.ifc.l1l2(&bch->b_if.ifc,
					   PH_ACTIVATE |
					   INDICATION,
					   NULL);
			break;

		default:
			if (bch->mode == L1_MODE_NULL)
				break;
			spin_lock_irqsave(&l1->lock,
					  flags);
			l1->hw->mr.
				fifo_slow_timer_service[l1->
							st_num]
				&=
				~((bch->bchan ==
				   1) ? 0x3 : 0xc);
			l1->hw->mr.
				fifo_rx_trans_enables[l1->
						      st_num]
				&=
				~((bch->bchan ==
				   1) ? 0x3 : 0xc);
			l1->hw->mr.timer_usg_cnt--;
			Write_hfc8(l1->hw, R_FIFO,
				   (l1->st_num * 8 +
				    ((bch->bchan ==
				      1) ? 0 : 2)));
			wait_busy(l1->hw);
			Write_hfc8(l1->hw, A_IRQ_MSK, 0);	
			wait_busy(l1->hw);
			Write_hfc8(l1->hw, R_FIFO,
				   (l1->st_num * 8 +
				    ((bch->bchan ==
				      1) ? 1 : 3)));
			wait_busy(l1->hw);
			Write_hfc8(l1->hw, A_IRQ_MSK, 0);	
			Write_hfc8(l1->hw, R_ST_SEL,
				   l1->st_num);
			l1->hw->mr.r_ctrl0 &=
				~(bch->bchan & 3);
			Write_hfc8(l1->hw, A_ST_CTRL0,
				   l1->hw->mr.r_ctrl0);
			spin_unlock_irqrestore(&l1->lock,
					       flags);

			bch->mode = L1_MODE_NULL;
			bch->b_if.ifc.l1l2(&bch->b_if.ifc,
					   PH_DEACTIVATE |
					   INDICATION,
					   NULL);
			if (bch->tx_skb) {
				dev_kfree_skb(bch->tx_skb);
				bch->tx_skb = NULL;
			}
			if (bch->rx_skb) {
				dev_kfree_skb(bch->rx_skb);
				bch->rx_skb = NULL;
			}
			skb_queue_purge(&bch->tx_queue);
			bch->tx_cnt = 0;
			bch->rx_ptr = NULL;
			break;
		}

		
		
		if (l1->hw->mr.timer_usg_cnt) {
			Write_hfc8(l1->hw, R_IRQMSK_MISC,
				   M_TI_IRQMSK);
		} else {
			Write_hfc8(l1->hw, R_IRQMSK_MISC, 0);
		}

		break;

	default:
		printk(KERN_INFO
		       "HFC-4S/8S: Unknown B-chan cmd 0x%x received, ignored\n",
		       pr);
		break;
	}
	if (!l1->enabled)
		bch->b_if.ifc.l1l2(&bch->b_if.ifc,
				   PH_DEACTIVATE | INDICATION, NULL);
}				

static void
hfc_l1_timer(struct hfc4s8s_l1 *l1)
{
	u_long flags;

	if (!l1->enabled)
		return;

	spin_lock_irqsave(&l1->lock, flags);
	if (l1->nt_mode) {
		l1->l1_state = 1;
		Write_hfc8(l1->hw, R_ST_SEL, l1->st_num);
		Write_hfc8(l1->hw, A_ST_WR_STA, 0x11);
		spin_unlock_irqrestore(&l1->lock, flags);
		l1->d_if.ifc.l1l2(&l1->d_if.ifc,
				  PH_DEACTIVATE | INDICATION, NULL);
		spin_lock_irqsave(&l1->lock, flags);
		l1->l1_state = 1;
		Write_hfc8(l1->hw, A_ST_WR_STA, 0x1);
		spin_unlock_irqrestore(&l1->lock, flags);
	} else {
		
		Write_hfc8(l1->hw, R_ST_SEL, l1->st_num);
		Write_hfc8(l1->hw, A_ST_WR_STA, 0x13);
		spin_unlock_irqrestore(&l1->lock, flags);
		l1->d_if.ifc.l1l2(&l1->d_if.ifc,
				  PH_DEACTIVATE | INDICATION, NULL);
		spin_lock_irqsave(&l1->lock, flags);
		Write_hfc8(l1->hw, R_ST_SEL, l1->st_num);
		Write_hfc8(l1->hw, A_ST_WR_STA, 0x3);
		spin_unlock_irqrestore(&l1->lock, flags);
	}
}				

static void
rx_d_frame(struct hfc4s8s_l1 *l1p, int ech)
{
	int z1, z2;
	u_char f1, f2, df;
	struct sk_buff *skb;
	u_char *cp;


	if (!l1p->enabled)
		return;
	do {
		
		Write_hfc8(l1p->hw, R_FIFO,
			   (l1p->st_num * 8 + ((ech) ? 7 : 5)));
		wait_busy(l1p->hw);

		f1 = Read_hfc8_stable(l1p->hw, A_F1);
		f2 = Read_hfc8(l1p->hw, A_F2);
		df = f1 - f2;
		if ((f1 - f2) < 0)
			df = f1 - f2 + MAX_F_CNT + 1;


		if (!df) {
			return;	
		}

		z1 = Read_hfc16_stable(l1p->hw, A_Z1);
		z2 = Read_hfc16(l1p->hw, A_Z2);

		z1 = z1 - z2 + 1;
		if (z1 < 0)
			z1 += 384;

		if (!(skb = dev_alloc_skb(MAX_D_FRAME_SIZE))) {
			printk(KERN_INFO
			       "HFC-4S/8S: Could not allocate D/E "
			       "channel receive buffer");
			Write_hfc8(l1p->hw, A_INC_RES_FIFO, 2);
			wait_busy(l1p->hw);
			return;
		}

		if (((z1 < 4) || (z1 > MAX_D_FRAME_SIZE))) {
			if (skb)
				dev_kfree_skb(skb);
			
			if (df == 1) {
				
				Write_hfc8(l1p->hw, A_INC_RES_FIFO, 2);
				wait_busy(l1p->hw);
				return;
			} else {
				

#ifndef HISAX_HFC4S8S_PCIMEM
				SetRegAddr(l1p->hw, A_FIFO_DATA0);
#endif

				while (z1 >= 4) {
#ifdef HISAX_HFC4S8S_PCIMEM
					Read_hfc32(l1p->hw, A_FIFO_DATA0);
#else
					fRead_hfc32(l1p->hw);
#endif
					z1 -= 4;
				}

				while (z1--)
#ifdef HISAX_HFC4S8S_PCIMEM
					Read_hfc8(l1p->hw, A_FIFO_DATA0);
#else
				fRead_hfc8(l1p->hw);
#endif

				Write_hfc8(l1p->hw, A_INC_RES_FIFO, 1);
				wait_busy(l1p->hw);
				return;
			}
		}

		cp = skb->data;

#ifndef HISAX_HFC4S8S_PCIMEM
		SetRegAddr(l1p->hw, A_FIFO_DATA0);
#endif

		while (z1 >= 4) {
#ifdef HISAX_HFC4S8S_PCIMEM
			*((unsigned long *) cp) =
				Read_hfc32(l1p->hw, A_FIFO_DATA0);
#else
			*((unsigned long *) cp) = fRead_hfc32(l1p->hw);
#endif
			cp += 4;
			z1 -= 4;
		}

		while (z1--)
#ifdef HISAX_HFC4S8S_PCIMEM
			*cp++ = Read_hfc8(l1p->hw, A_FIFO_DATA0);
#else
		*cp++ = fRead_hfc8(l1p->hw);
#endif

		Write_hfc8(l1p->hw, A_INC_RES_FIFO, 1);	
		wait_busy(l1p->hw);

		if (*(--cp)) {
			dev_kfree_skb(skb);
		} else {
			skb->len = (cp - skb->data) - 2;
			if (ech)
				l1p->d_if.ifc.l1l2(&l1p->d_if.ifc,
						   PH_DATA_E | INDICATION,
						   skb);
			else
				l1p->d_if.ifc.l1l2(&l1p->d_if.ifc,
						   PH_DATA | INDICATION,
						   skb);
		}
	} while (1);
}				

static void
rx_b_frame(struct hfc4s8s_btype *bch)
{
	int z1, z2, hdlc_complete;
	u_char f1, f2;
	struct hfc4s8s_l1 *l1 = bch->l1p;
	struct sk_buff *skb;

	if (!l1->enabled || (bch->mode == L1_MODE_NULL))
		return;

	do {
		
		Write_hfc8(l1->hw, R_FIFO,
			   (l1->st_num * 8 + ((bch->bchan == 1) ? 1 : 3)));
		wait_busy(l1->hw);

		if (bch->mode == L1_MODE_HDLC) {
			f1 = Read_hfc8_stable(l1->hw, A_F1);
			f2 = Read_hfc8(l1->hw, A_F2);
			hdlc_complete = ((f1 ^ f2) & MAX_F_CNT);
		} else
			hdlc_complete = 0;
		z1 = Read_hfc16_stable(l1->hw, A_Z1);
		z2 = Read_hfc16(l1->hw, A_Z2);
		z1 = (z1 - z2);
		if (hdlc_complete)
			z1++;
		if (z1 < 0)
			z1 += 384;

		if (!z1)
			break;

		if (!(skb = bch->rx_skb)) {
			if (!
			    (skb =
			     dev_alloc_skb((bch->mode ==
					    L1_MODE_TRANS) ? z1
					   : (MAX_B_FRAME_SIZE + 3)))) {
				printk(KERN_ERR
				       "HFC-4S/8S: Could not allocate B "
				       "channel receive buffer");
				return;
			}
			bch->rx_ptr = skb->data;
			bch->rx_skb = skb;
		}

		skb->len = (bch->rx_ptr - skb->data) + z1;

		
		if ((bch->mode == L1_MODE_HDLC) &&
		    ((hdlc_complete && (skb->len < 4)) ||
		     (skb->len > (MAX_B_FRAME_SIZE + 3)))) {

			skb->len = 0;
			bch->rx_ptr = skb->data;
			Write_hfc8(l1->hw, A_INC_RES_FIFO, 2);	
			wait_busy(l1->hw);
			return;
		}
#ifndef HISAX_HFC4S8S_PCIMEM
		SetRegAddr(l1->hw, A_FIFO_DATA0);
#endif

		while (z1 >= 4) {
#ifdef HISAX_HFC4S8S_PCIMEM
			*((unsigned long *) bch->rx_ptr) =
				Read_hfc32(l1->hw, A_FIFO_DATA0);
#else
			*((unsigned long *) bch->rx_ptr) =
				fRead_hfc32(l1->hw);
#endif
			bch->rx_ptr += 4;
			z1 -= 4;
		}

		while (z1--)
#ifdef HISAX_HFC4S8S_PCIMEM
			*(bch->rx_ptr++) = Read_hfc8(l1->hw, A_FIFO_DATA0);
#else
		*(bch->rx_ptr++) = fRead_hfc8(l1->hw);
#endif

		if (hdlc_complete) {
			
			Write_hfc8(l1->hw, A_INC_RES_FIFO, 1);
			wait_busy(l1->hw);

			
			bch->rx_ptr--;
			if (*bch->rx_ptr) {
				skb->len = 0;
				bch->rx_ptr = skb->data;
				continue;
			}
			skb->len -= 3;
		}
		if (hdlc_complete || (bch->mode == L1_MODE_TRANS)) {
			bch->rx_skb = NULL;
			bch->rx_ptr = NULL;
			bch->b_if.ifc.l1l2(&bch->b_if.ifc,
					   PH_DATA | INDICATION, skb);
		}

	} while (1);
}				

static void
tx_d_frame(struct hfc4s8s_l1 *l1p)
{
	struct sk_buff *skb;
	u_char f1, f2;
	u_char *cp;
	long cnt;

	if (l1p->l1_state != 7)
		return;

	
	Write_hfc8(l1p->hw, R_FIFO, (l1p->st_num * 8 + 4));
	wait_busy(l1p->hw);

	f1 = Read_hfc8(l1p->hw, A_F1);
	f2 = Read_hfc8_stable(l1p->hw, A_F2);

	if ((f1 ^ f2) & MAX_F_CNT)
		return;		

	if (l1p->tx_cnt > 0) {
		cnt = l1p->tx_cnt;
		l1p->tx_cnt = 0;
		l1p->d_if.ifc.l1l2(&l1p->d_if.ifc, PH_DATA | CONFIRM,
				   (void *) cnt);
	}

	if ((skb = skb_dequeue(&l1p->d_tx_queue))) {
		cp = skb->data;
		cnt = skb->len;
#ifndef HISAX_HFC4S8S_PCIMEM
		SetRegAddr(l1p->hw, A_FIFO_DATA0);
#endif

		while (cnt >= 4) {
#ifdef HISAX_HFC4S8S_PCIMEM
			fWrite_hfc32(l1p->hw, A_FIFO_DATA0,
				     *(unsigned long *) cp);
#else
			SetRegAddr(l1p->hw, A_FIFO_DATA0);
			fWrite_hfc32(l1p->hw, *(unsigned long *) cp);
#endif
			cp += 4;
			cnt -= 4;
		}

#ifdef HISAX_HFC4S8S_PCIMEM
		while (cnt--)
			fWrite_hfc8(l1p->hw, A_FIFO_DATA0, *cp++);
#else
		while (cnt--)
			fWrite_hfc8(l1p->hw, *cp++);
#endif

		l1p->tx_cnt = skb->truesize;
		Write_hfc8(l1p->hw, A_INC_RES_FIFO, 1);	
		wait_busy(l1p->hw);

		dev_kfree_skb(skb);
	}
}				

static void
tx_b_frame(struct hfc4s8s_btype *bch)
{
	struct sk_buff *skb;
	struct hfc4s8s_l1 *l1 = bch->l1p;
	u_char *cp;
	int cnt, max, hdlc_num;
	long ack_len = 0;

	if (!l1->enabled || (bch->mode == L1_MODE_NULL))
		return;

	
	Write_hfc8(l1->hw, R_FIFO,
		   (l1->st_num * 8 + ((bch->bchan == 1) ? 0 : 2)));
	wait_busy(l1->hw);
	do {

		if (bch->mode == L1_MODE_HDLC) {
			hdlc_num = Read_hfc8(l1->hw, A_F1) & MAX_F_CNT;
			hdlc_num -=
				(Read_hfc8_stable(l1->hw, A_F2) & MAX_F_CNT);
			if (hdlc_num < 0)
				hdlc_num += 16;
			if (hdlc_num >= 15)
				break;	
		} else
			hdlc_num = 0;

		if (!(skb = bch->tx_skb)) {
			if (!(skb = skb_dequeue(&bch->tx_queue))) {
				l1->hw->mr.fifo_slow_timer_service[l1->
								   st_num]
					&= ~((bch->bchan == 1) ? 1 : 4);
				break;	
			}
			bch->tx_skb = skb;
			bch->tx_cnt = 0;
		}

		if (!hdlc_num)
			l1->hw->mr.fifo_slow_timer_service[l1->st_num] |=
				((bch->bchan == 1) ? 1 : 4);
		else
			l1->hw->mr.fifo_slow_timer_service[l1->st_num] &=
				~((bch->bchan == 1) ? 1 : 4);

		max = Read_hfc16_stable(l1->hw, A_Z2);
		max -= Read_hfc16(l1->hw, A_Z1);
		if (max <= 0)
			max += 384;
		max--;

		if (max < 16)
			break;	

		cnt = skb->len - bch->tx_cnt;
		if (cnt > max)
			cnt = max;
		cp = skb->data + bch->tx_cnt;
		bch->tx_cnt += cnt;

#ifndef HISAX_HFC4S8S_PCIMEM
		SetRegAddr(l1->hw, A_FIFO_DATA0);
#endif
		while (cnt >= 4) {
#ifdef HISAX_HFC4S8S_PCIMEM
			fWrite_hfc32(l1->hw, A_FIFO_DATA0,
				     *(unsigned long *) cp);
#else
			fWrite_hfc32(l1->hw, *(unsigned long *) cp);
#endif
			cp += 4;
			cnt -= 4;
		}

		while (cnt--)
#ifdef HISAX_HFC4S8S_PCIMEM
			fWrite_hfc8(l1->hw, A_FIFO_DATA0, *cp++);
#else
		fWrite_hfc8(l1->hw, *cp++);
#endif

		if (bch->tx_cnt >= skb->len) {
			if (bch->mode == L1_MODE_HDLC) {
				
				Write_hfc8(l1->hw, A_INC_RES_FIFO, 1);
			}
			ack_len += skb->truesize;
			bch->tx_skb = NULL;
			bch->tx_cnt = 0;
			dev_kfree_skb(skb);
		} else
			
			Write_hfc8(l1->hw, R_FIFO,
				   (l1->st_num * 8 +
				    ((bch->bchan == 1) ? 0 : 2)));
		wait_busy(l1->hw);
	} while (1);

	if (ack_len)
		bch->b_if.ifc.l1l2((struct hisax_if *) &bch->b_if,
				   PH_DATA | CONFIRM, (void *) ack_len);
}				

static void
hfc4s8s_bh(struct work_struct *work)
{
	hfc4s8s_hw *hw = container_of(work, hfc4s8s_hw, tqueue);
	u_char b;
	struct hfc4s8s_l1 *l1p;
	volatile u_char *fifo_stat;
	int idx;

	
	b = 1;
	l1p = hw->l1;
	while (b) {
		if ((b & hw->mr.r_irq_statech)) {
			
			hw->mr.r_irq_statech &= ~b;
			if (l1p->enabled) {
				if (l1p->nt_mode) {
					u_char oldstate = l1p->l1_state;

					Write_hfc8(l1p->hw, R_ST_SEL,
						   l1p->st_num);
					l1p->l1_state =
						Read_hfc8(l1p->hw,
							  A_ST_RD_STA) & 0xf;

					if ((oldstate == 3)
					    && (l1p->l1_state != 3))
						l1p->d_if.ifc.l1l2(&l1p->
								   d_if.
								   ifc,
								   PH_DEACTIVATE
								   |
								   INDICATION,
								   NULL);

					if (l1p->l1_state != 2) {
						del_timer(&l1p->l1_timer);
						if (l1p->l1_state == 3) {
							l1p->d_if.ifc.
								l1l2(&l1p->
								     d_if.ifc,
								     PH_ACTIVATE
								     |
								     INDICATION,
								     NULL);
						}
					} else {
						
						Write_hfc8(hw, A_ST_WR_STA,
							   M_SET_G2_G3);
						mod_timer(&l1p->l1_timer,
							  jiffies +
							  L1_TIMER_T1);
					}
					printk(KERN_INFO
					       "HFC-4S/8S: NT ch %d l1 state %d -> %d\n",
					       l1p->st_num, oldstate,
					       l1p->l1_state);
				} else {
					u_char oldstate = l1p->l1_state;

					Write_hfc8(l1p->hw, R_ST_SEL,
						   l1p->st_num);
					l1p->l1_state =
						Read_hfc8(l1p->hw,
							  A_ST_RD_STA) & 0xf;

					if (((l1p->l1_state == 3) &&
					     ((oldstate == 7) ||
					      (oldstate == 8))) ||
					    ((timer_pending
					      (&l1p->l1_timer))
					     && (l1p->l1_state == 8))) {
						mod_timer(&l1p->l1_timer,
							  L1_TIMER_T4 +
							  jiffies);
					} else {
						if (l1p->l1_state == 7) {
							del_timer(&l1p->
								  l1_timer);
							l1p->d_if.ifc.
								l1l2(&l1p->
								     d_if.ifc,
								     PH_ACTIVATE
								     |
								     INDICATION,
								     NULL);
							tx_d_frame(l1p);
						}
						if (l1p->l1_state == 3) {
							if (oldstate != 3)
								l1p->d_if.
									ifc.
									l1l2
									(&l1p->
									 d_if.
									 ifc,
									 PH_DEACTIVATE
									 |
									 INDICATION,
									 NULL);
						}
					}
					printk(KERN_INFO
					       "HFC-4S/8S: TE %d ch %d l1 state %d -> %d\n",
					       l1p->hw->cardnum,
					       l1p->st_num, oldstate,
					       l1p->l1_state);
				}
			}
		}
		b <<= 1;
		l1p++;
	}

	
	idx = 0;
	fifo_stat = hw->mr.r_irq_fifo_blx;
	l1p = hw->l1;
	while (idx < hw->driver_data.max_st_ports) {

		if (hw->mr.timer_irq) {
			*fifo_stat |= hw->mr.fifo_rx_trans_enables[idx];
			if (hw->fifo_sched_cnt <= 0) {
				*fifo_stat |=
					hw->mr.fifo_slow_timer_service[l1p->
								       st_num];
			}
		}
		
		*fifo_stat &= 0xff - 0x40;

		while (*fifo_stat) {

			if (!l1p->nt_mode) {
				
				if ((*fifo_stat & 0x20)) {
					*fifo_stat &= ~0x20;
					rx_d_frame(l1p, 0);
				}
				
				if ((*fifo_stat & 0x80)) {
					*fifo_stat &= ~0x80;
					rx_d_frame(l1p, 1);
				}
				
				if ((*fifo_stat & 0x10)) {
					*fifo_stat &= ~0x10;
					tx_d_frame(l1p);
				}
			}
			
			if ((*fifo_stat & 0x2)) {
				*fifo_stat &= ~0x2;
				rx_b_frame(l1p->b_ch);
			}
			
			if ((*fifo_stat & 0x1)) {
				*fifo_stat &= ~0x1;
				tx_b_frame(l1p->b_ch);
			}
			
			if ((*fifo_stat & 0x8)) {
				*fifo_stat &= ~0x8;
				rx_b_frame(l1p->b_ch + 1);
			}
			
			if ((*fifo_stat & 0x4)) {
				*fifo_stat &= ~0x4;
				tx_b_frame(l1p->b_ch + 1);
			}
		}
		fifo_stat++;
		l1p++;
		idx++;
	}

	if (hw->fifo_sched_cnt <= 0)
		hw->fifo_sched_cnt += (1 << (7 - TRANS_TIMER_MODE));
	hw->mr.timer_irq = 0;	
}				

static irqreturn_t
hfc4s8s_interrupt(int intno, void *dev_id)
{
	hfc4s8s_hw *hw = dev_id;
	u_char b, ovr;
	volatile u_char *ovp;
	int idx;
	u_char old_ioreg;

	if (!hw || !(hw->mr.r_irq_ctrl & M_GLOB_IRQ_EN))
		return IRQ_NONE;

#ifndef	HISAX_HFC4S8S_PCIMEM
	
	old_ioreg = GetRegAddr(hw);
#endif

	
	hw->mr.r_irq_statech |=
		(Read_hfc8(hw, R_SCI) & hw->mr.r_irqmsk_statchg);
	if (!
	    (b = (Read_hfc8(hw, R_STATUS) & (M_MISC_IRQSTA | M_FR_IRQSTA)))
	    && !hw->mr.r_irq_statech) {
#ifndef	HISAX_HFC4S8S_PCIMEM
		SetRegAddr(hw, old_ioreg);
#endif
		return IRQ_NONE;
	}

	
	if (Read_hfc8(hw, R_IRQ_MISC) & M_TI_IRQ) {
		hw->mr.timer_irq = 1;
		hw->fifo_sched_cnt--;
	}

	
	if ((ovr = Read_hfc8(hw, R_IRQ_OVIEW))) {
		hw->mr.r_irq_oview |= ovr;
		idx = R_IRQ_FIFO_BL0;
		ovp = hw->mr.r_irq_fifo_blx;
		while (ovr) {
			if ((ovr & 1)) {
				*ovp |= Read_hfc8(hw, idx);
			}
			ovp++;
			idx++;
			ovr >>= 1;
		}
	}

	
	schedule_work(&hw->tqueue);

#ifndef	HISAX_HFC4S8S_PCIMEM
	SetRegAddr(hw, old_ioreg);
#endif
	return IRQ_HANDLED;
}				

static void
chipreset(hfc4s8s_hw *hw)
{
	u_long flags;

	spin_lock_irqsave(&hw->lock, flags);
	Write_hfc8(hw, R_CTRL, 0);	
	Write_hfc8(hw, R_RAM_MISC, 0);	
	Write_hfc8(hw, R_FIFO_MD, 0);	
	Write_hfc8(hw, R_CIRM, M_SRES);	
	hw->mr.r_irq_ctrl = 0;	
	spin_unlock_irqrestore(&hw->lock, flags);

	udelay(3);
	Write_hfc8(hw, R_CIRM, 0);	
	wait_busy(hw);

	Write_hfc8(hw, R_PCM_MD0, M_PCM_MD);	
	Write_hfc8(hw, R_RAM_MISC, M_FZ_MD);	
	if (hw->driver_data.clock_mode == 1)
		Write_hfc8(hw, R_BRG_PCM_CFG, M_PCM_CLK);	
	Write_hfc8(hw, R_TI_WD, TRANS_TIMER_MODE);	

	memset(&hw->mr, 0, sizeof(hw->mr));
}				

static void
hfc_hardware_enable(hfc4s8s_hw *hw, int enable, int nt_mode)
{
	u_long flags;
	char if_name[40];
	int i;

	if (enable) {
		
		hw->nt_mode = nt_mode;

		
		hw->mr.r_irq_ctrl = M_FIFO_IRQ;
		Write_hfc8(hw, R_IRQ_CTRL, hw->mr.r_irq_ctrl);
		hw->mr.r_irqmsk_statchg = 0;
		Write_hfc8(hw, R_SCI_MSK, hw->mr.r_irqmsk_statchg);
		Write_hfc8(hw, R_PWM_MD, 0x80);
		Write_hfc8(hw, R_PWM1, 26);
		if (!nt_mode)
			Write_hfc8(hw, R_ST_SYNC, M_AUTO_SYNC);

		
		for (i = 0; i < hw->driver_data.max_st_ports; i++) {
			hw->mr.r_irqmsk_statchg |= (1 << i);
			Write_hfc8(hw, R_SCI_MSK, hw->mr.r_irqmsk_statchg);
			Write_hfc8(hw, R_ST_SEL, i);
			Write_hfc8(hw, A_ST_CLK_DLY,
				   ((nt_mode) ? CLKDEL_NT : CLKDEL_TE));
			hw->mr.r_ctrl0 = ((nt_mode) ? CTRL0_NT : CTRL0_TE);
			Write_hfc8(hw, A_ST_CTRL0, hw->mr.r_ctrl0);
			Write_hfc8(hw, A_ST_CTRL2, 3);
			Write_hfc8(hw, A_ST_WR_STA, 0);	

			hw->l1[i].enabled = 1;
			hw->l1[i].nt_mode = nt_mode;

			if (!nt_mode) {
				
				Write_hfc8(hw, R_FIFO, i * 8 + 7);	
				wait_busy(hw);
				Write_hfc8(hw, A_CON_HDLC, 0x11);	
				Write_hfc8(hw, A_SUBCH_CFG, 2);	
				Write_hfc8(hw, A_IRQ_MSK, 1);	
				Write_hfc8(hw, A_INC_RES_FIFO, 2);	
				wait_busy(hw);

				
				Write_hfc8(hw, R_FIFO, i * 8 + 5);	
				wait_busy(hw);
				Write_hfc8(hw, A_CON_HDLC, 0x11);	
				Write_hfc8(hw, A_SUBCH_CFG, 2);	
				Write_hfc8(hw, A_IRQ_MSK, 1);	
				Write_hfc8(hw, A_INC_RES_FIFO, 2);	
				wait_busy(hw);

				
				Write_hfc8(hw, R_FIFO, i * 8 + 4);	
				wait_busy(hw);
				Write_hfc8(hw, A_CON_HDLC, 0x11);	
				Write_hfc8(hw, A_SUBCH_CFG, 2);	
				Write_hfc8(hw, A_IRQ_MSK, 1);	
				Write_hfc8(hw, A_INC_RES_FIFO, 2);	
				wait_busy(hw);
			}

			sprintf(if_name, "hfc4s8s_%d%d_", hw->cardnum, i);

			if (hisax_register
			    (&hw->l1[i].d_if, hw->l1[i].b_table, if_name,
			     ((nt_mode) ? 3 : 2))) {

				hw->l1[i].enabled = 0;
				hw->mr.r_irqmsk_statchg &= ~(1 << i);
				Write_hfc8(hw, R_SCI_MSK,
					   hw->mr.r_irqmsk_statchg);
				printk(KERN_INFO
				       "HFC-4S/8S: Unable to register S/T device %s, break\n",
				       if_name);
				break;
			}
		}
		spin_lock_irqsave(&hw->lock, flags);
		hw->mr.r_irq_ctrl |= M_GLOB_IRQ_EN;
		Write_hfc8(hw, R_IRQ_CTRL, hw->mr.r_irq_ctrl);
		spin_unlock_irqrestore(&hw->lock, flags);
	} else {
		
		spin_lock_irqsave(&hw->lock, flags);
		hw->mr.r_irq_ctrl &= ~M_GLOB_IRQ_EN;
		Write_hfc8(hw, R_IRQ_CTRL, hw->mr.r_irq_ctrl);
		spin_unlock_irqrestore(&hw->lock, flags);

		for (i = hw->driver_data.max_st_ports - 1; i >= 0; i--) {
			hw->l1[i].enabled = 0;
			hisax_unregister(&hw->l1[i].d_if);
			del_timer(&hw->l1[i].l1_timer);
			skb_queue_purge(&hw->l1[i].d_tx_queue);
			skb_queue_purge(&hw->l1[i].b_ch[0].tx_queue);
			skb_queue_purge(&hw->l1[i].b_ch[1].tx_queue);
		}
		chipreset(hw);
	}
}				

static void
release_pci_ports(hfc4s8s_hw *hw)
{
	pci_write_config_word(hw->pdev, PCI_COMMAND, 0);
#ifdef HISAX_HFC4S8S_PCIMEM
	if (hw->membase)
		iounmap((void *) hw->membase);
#else
	if (hw->iobase)
		release_region(hw->iobase, 8);
#endif
}

static void
enable_pci_ports(hfc4s8s_hw *hw)
{
#ifdef HISAX_HFC4S8S_PCIMEM
	pci_write_config_word(hw->pdev, PCI_COMMAND, PCI_ENA_MEMIO);
#else
	pci_write_config_word(hw->pdev, PCI_COMMAND, PCI_ENA_REGIO);
#endif
}

static int __devinit
setup_instance(hfc4s8s_hw *hw)
{
	int err = -EIO;
	int i;

	for (i = 0; i < HFC_MAX_ST; i++) {
		struct hfc4s8s_l1 *l1p;

		l1p = hw->l1 + i;
		spin_lock_init(&l1p->lock);
		l1p->hw = hw;
		l1p->l1_timer.function = (void *) hfc_l1_timer;
		l1p->l1_timer.data = (long) (l1p);
		init_timer(&l1p->l1_timer);
		l1p->st_num = i;
		skb_queue_head_init(&l1p->d_tx_queue);
		l1p->d_if.ifc.priv = hw->l1 + i;
		l1p->d_if.ifc.l2l1 = (void *) dch_l2l1;

		spin_lock_init(&l1p->b_ch[0].lock);
		l1p->b_ch[0].b_if.ifc.l2l1 = (void *) bch_l2l1;
		l1p->b_ch[0].b_if.ifc.priv = (void *) &l1p->b_ch[0];
		l1p->b_ch[0].l1p = hw->l1 + i;
		l1p->b_ch[0].bchan = 1;
		l1p->b_table[0] = &l1p->b_ch[0].b_if;
		skb_queue_head_init(&l1p->b_ch[0].tx_queue);

		spin_lock_init(&l1p->b_ch[1].lock);
		l1p->b_ch[1].b_if.ifc.l2l1 = (void *) bch_l2l1;
		l1p->b_ch[1].b_if.ifc.priv = (void *) &l1p->b_ch[1];
		l1p->b_ch[1].l1p = hw->l1 + i;
		l1p->b_ch[1].bchan = 2;
		l1p->b_table[1] = &l1p->b_ch[1].b_if;
		skb_queue_head_init(&l1p->b_ch[1].tx_queue);
	}

	enable_pci_ports(hw);
	chipreset(hw);

	i = Read_hfc8(hw, R_CHIP_ID) >> CHIP_ID_SHIFT;
	if (i != hw->driver_data.chip_id) {
		printk(KERN_INFO
		       "HFC-4S/8S: invalid chip id 0x%x instead of 0x%x, card ignored\n",
		       i, hw->driver_data.chip_id);
		goto out;
	}

	i = Read_hfc8(hw, R_CHIP_RV) & 0xf;
	if (!i) {
		printk(KERN_INFO
		       "HFC-4S/8S: chip revision 0 not supported, card ignored\n");
		goto out;
	}

	INIT_WORK(&hw->tqueue, hfc4s8s_bh);

	if (request_irq
	    (hw->irq, hfc4s8s_interrupt, IRQF_SHARED, hw->card_name, hw)) {
		printk(KERN_INFO
		       "HFC-4S/8S: unable to alloc irq %d, card ignored\n",
		       hw->irq);
		goto out;
	}
#ifdef HISAX_HFC4S8S_PCIMEM
	printk(KERN_INFO
	       "HFC-4S/8S: found PCI card at membase 0x%p, irq %d\n",
	       hw->hw_membase, hw->irq);
#else
	printk(KERN_INFO
	       "HFC-4S/8S: found PCI card at iobase 0x%x, irq %d\n",
	       hw->iobase, hw->irq);
#endif

	hfc_hardware_enable(hw, 1, 0);

	return (0);

out:
	hw->irq = 0;
	release_pci_ports(hw);
	kfree(hw);
	return (err);
}

static int __devinit
hfc4s8s_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int err = -ENOMEM;
	hfc4s8s_param *driver_data = (hfc4s8s_param *) ent->driver_data;
	hfc4s8s_hw *hw;

	if (!(hw = kzalloc(sizeof(hfc4s8s_hw), GFP_ATOMIC))) {
		printk(KERN_ERR "No kmem for HFC-4S/8S card\n");
		return (err);
	}

	hw->pdev = pdev;
	err = pci_enable_device(pdev);

	if (err)
		goto out;

	hw->cardnum = card_cnt;
	sprintf(hw->card_name, "hfc4s8s_%d", hw->cardnum);
	printk(KERN_INFO "HFC-4S/8S: found adapter %s (%s) at %s\n",
	       driver_data->device_name, hw->card_name, pci_name(pdev));

	spin_lock_init(&hw->lock);

	hw->driver_data = *driver_data;
	hw->irq = pdev->irq;
	hw->iobase = pci_resource_start(pdev, 0);

#ifdef HISAX_HFC4S8S_PCIMEM
	hw->hw_membase = (u_char *) pci_resource_start(pdev, 1);
	hw->membase = ioremap((ulong) hw->hw_membase, 256);
#else
	if (!request_region(hw->iobase, 8, hw->card_name)) {
		printk(KERN_INFO
		       "HFC-4S/8S: failed to rquest address space at 0x%04x\n",
		       hw->iobase);
		goto out;
	}
#endif

	pci_set_drvdata(pdev, hw);
	err = setup_instance(hw);
	if (!err)
		card_cnt++;
	return (err);

out:
	kfree(hw);
	return (err);
}

static void __devexit
hfc4s8s_remove(struct pci_dev *pdev)
{
	hfc4s8s_hw *hw = pci_get_drvdata(pdev);

	printk(KERN_INFO "HFC-4S/8S: removing card %d\n", hw->cardnum);
	hfc_hardware_enable(hw, 0, 0);

	if (hw->irq)
		free_irq(hw->irq, hw);
	hw->irq = 0;
	release_pci_ports(hw);

	card_cnt--;
	pci_disable_device(pdev);
	kfree(hw);
	return;
}

static struct pci_driver hfc4s8s_driver = {
	.name	= "hfc4s8s_l1",
	.probe	= hfc4s8s_probe,
	.remove	= __devexit_p(hfc4s8s_remove),
	.id_table	= hfc4s8s_ids,
};

static int __init
hfc4s8s_module_init(void)
{
	int err;

	printk(KERN_INFO
	       "HFC-4S/8S: Layer 1 driver module for HFC-4S/8S isdn chips, %s\n",
	       hfc4s8s_rev);
	printk(KERN_INFO
	       "HFC-4S/8S: (C) 2003 Cornelius Consult, www.cornelius-consult.de\n");

	card_cnt = 0;

	err = pci_register_driver(&hfc4s8s_driver);
	if (err < 0) {
		goto out;
	}
	printk(KERN_INFO "HFC-4S/8S: found %d cards\n", card_cnt);

#if !defined(CONFIG_HOTPLUG)
	if (err == 0) {
		err = -ENODEV;
		pci_unregister_driver(&hfc4s8s_driver);
		goto out;
	}
#endif

	return 0;
out:
	return (err);
}				

static void __exit
hfc4s8s_module_exit(void)
{
	pci_unregister_driver(&hfc4s8s_driver);
	printk(KERN_INFO "HFC-4S/8S: module removed\n");
}				

module_init(hfc4s8s_module_init);
module_exit(hfc4s8s_module_exit);
