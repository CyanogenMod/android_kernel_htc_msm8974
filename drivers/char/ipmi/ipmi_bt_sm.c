/*
 *  ipmi_bt_sm.c
 *
 *  The state machine for an Open IPMI BT sub-driver under ipmi_si.c, part
 *  of the driver architecture at http://sourceforge.net/projects/openipmi 
 *
 *  Author:	Rocky Craig <first.last@hp.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <linux/kernel.h> 
#include <linux/string.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/ipmi_msgdefs.h>		
#include "ipmi_si_sm.h"

#define BT_DEBUG_OFF	0	
#define BT_DEBUG_ENABLE	1	
#define BT_DEBUG_MSG	2	
#define BT_DEBUG_STATES	4	

static int bt_debug; 

module_param(bt_debug, int, 0644);
MODULE_PARM_DESC(bt_debug, "debug bitmask, 1=enable, 2=messages, 4=states");


#define BT_NORMAL_TIMEOUT	5	
#define BT_NORMAL_RETRY_LIMIT	2
#define BT_RESET_DELAY		6	

/*
 * States are written in chronological order and usually cover
 * multiple rows of the state table discussion in the IPMI spec.
 */

enum bt_states {
	BT_STATE_IDLE = 0,	
	BT_STATE_XACTION_START,
	BT_STATE_WRITE_BYTES,
	BT_STATE_WRITE_CONSUME,
	BT_STATE_READ_WAIT,
	BT_STATE_CLEAR_B2H,
	BT_STATE_READ_BYTES,
	BT_STATE_RESET1,	
	BT_STATE_RESET2,
	BT_STATE_RESET3,
	BT_STATE_RESTART,
	BT_STATE_PRINTME,
	BT_STATE_CAPABILITIES_BEGIN,
	BT_STATE_CAPABILITIES_END,
	BT_STATE_LONG_BUSY	
};


#define BT_STATE_CHANGE(X, Y) { bt->state = X; return Y; }

#define BT_SI_SM_RETURN(Y)   { last_printed = BT_STATE_PRINTME; return Y; }

struct si_sm_data {
	enum bt_states	state;
	unsigned char	seq;		
	struct si_sm_io	*io;
	unsigned char	write_data[IPMI_MAX_MSG_LENGTH];
	int		write_count;
	unsigned char	read_data[IPMI_MAX_MSG_LENGTH];
	int		read_count;
	int		truncated;
	long		timeout;	
	int		error_retries;	
	int		nonzero_status;	
	enum bt_states	complete;	
	int		BT_CAP_outreqs;
	long		BT_CAP_req2rsp;
	int		BT_CAP_retries;	
};

#define BT_CLR_WR_PTR	0x01	
#define BT_CLR_RD_PTR	0x02
#define BT_H2B_ATN	0x04
#define BT_B2H_ATN	0x08
#define BT_SMS_ATN	0x10
#define BT_OEM0		0x20
#define BT_H_BUSY	0x40
#define BT_B_BUSY	0x80


#define BT_STATUS	bt->io->inputb(bt->io, 0)
#define BT_CONTROL(x)	bt->io->outputb(bt->io, 0, x)

#define BMC2HOST	bt->io->inputb(bt->io, 1)
#define HOST2BMC(x)	bt->io->outputb(bt->io, 1, x)

#define BT_INTMASK_R	bt->io->inputb(bt->io, 2)
#define BT_INTMASK_W(x)	bt->io->outputb(bt->io, 2, x)


static char *state2txt(unsigned char state)
{
	switch (state) {
	case BT_STATE_IDLE:		return("IDLE");
	case BT_STATE_XACTION_START:	return("XACTION");
	case BT_STATE_WRITE_BYTES:	return("WR_BYTES");
	case BT_STATE_WRITE_CONSUME:	return("WR_CONSUME");
	case BT_STATE_READ_WAIT:	return("RD_WAIT");
	case BT_STATE_CLEAR_B2H:	return("CLEAR_B2H");
	case BT_STATE_READ_BYTES:	return("RD_BYTES");
	case BT_STATE_RESET1:		return("RESET1");
	case BT_STATE_RESET2:		return("RESET2");
	case BT_STATE_RESET3:		return("RESET3");
	case BT_STATE_RESTART:		return("RESTART");
	case BT_STATE_LONG_BUSY:	return("LONG_BUSY");
	case BT_STATE_CAPABILITIES_BEGIN: return("CAP_BEGIN");
	case BT_STATE_CAPABILITIES_END:	return("CAP_END");
	}
	return("BAD STATE");
}
#define STATE2TXT state2txt(bt->state)

static char *status2txt(unsigned char status)
{
	static char buf[40];

	strcpy(buf, "[ ");
	if (status & BT_B_BUSY)
		strcat(buf, "B_BUSY ");
	if (status & BT_H_BUSY)
		strcat(buf, "H_BUSY ");
	if (status & BT_OEM0)
		strcat(buf, "OEM0 ");
	if (status & BT_SMS_ATN)
		strcat(buf, "SMS ");
	if (status & BT_B2H_ATN)
		strcat(buf, "B2H ");
	if (status & BT_H2B_ATN)
		strcat(buf, "H2B ");
	strcat(buf, "]");
	return buf;
}
#define STATUS2TXT status2txt(status)


static unsigned int bt_init_data(struct si_sm_data *bt, struct si_sm_io *io)
{
	memset(bt, 0, sizeof(struct si_sm_data));
	if (bt->io != io) {
		
		bt->io = io;
		bt->seq = 0;
	}
	bt->state = BT_STATE_IDLE;	
	bt->complete = BT_STATE_IDLE;	
	bt->BT_CAP_req2rsp = BT_NORMAL_TIMEOUT * 1000000;
	bt->BT_CAP_retries = BT_NORMAL_RETRY_LIMIT;
	
	return 3; 
}


static void force_result(struct si_sm_data *bt, unsigned char completion_code)
{
	bt->read_data[0] = 4;				
	bt->read_data[1] = bt->write_data[1] | 4;	
	bt->read_data[2] = bt->write_data[2];		
	bt->read_data[3] = bt->write_data[3];		
	bt->read_data[4] = completion_code;
	bt->read_count = 5;
}


static int bt_start_transaction(struct si_sm_data *bt,
				unsigned char *data,
				unsigned int size)
{
	unsigned int i;

	if (size < 2)
		return IPMI_REQ_LEN_INVALID_ERR;
	if (size > IPMI_MAX_MSG_LENGTH)
		return IPMI_REQ_LEN_EXCEEDED_ERR;

	if (bt->state == BT_STATE_LONG_BUSY)
		return IPMI_NODE_BUSY_ERR;

	if (bt->state != BT_STATE_IDLE)
		return IPMI_NOT_IN_MY_STATE_ERR;

	if (bt_debug & BT_DEBUG_MSG) {
		printk(KERN_WARNING "BT: +++++++++++++++++ New command\n");
		printk(KERN_WARNING "BT: NetFn/LUN CMD [%d data]:", size - 2);
		for (i = 0; i < size; i ++)
			printk(" %02x", data[i]);
		printk("\n");
	}
	bt->write_data[0] = size + 1;	
	bt->write_data[1] = *data;	
	bt->write_data[2] = bt->seq++;
	memcpy(bt->write_data + 3, data + 1, size - 1);
	bt->write_count = size + 2;
	bt->error_retries = 0;
	bt->nonzero_status = 0;
	bt->truncated = 0;
	bt->state = BT_STATE_XACTION_START;
	bt->timeout = bt->BT_CAP_req2rsp;
	force_result(bt, IPMI_ERR_UNSPECIFIED);
	return 0;
}


static int bt_get_result(struct si_sm_data *bt,
			 unsigned char *data,
			 unsigned int length)
{
	int i, msg_len;

	msg_len = bt->read_count - 2;		
	if (msg_len < 3 || msg_len > IPMI_MAX_MSG_LENGTH) {
		force_result(bt, IPMI_ERR_UNSPECIFIED);
		msg_len = 3;
	}
	data[0] = bt->read_data[1];
	data[1] = bt->read_data[3];
	if (length < msg_len || bt->truncated) {
		data[2] = IPMI_ERR_MSG_TRUNCATED;
		msg_len = 3;
	} else
		memcpy(data + 2, bt->read_data + 4, msg_len - 2);

	if (bt_debug & BT_DEBUG_MSG) {
		printk(KERN_WARNING "BT: result %d bytes:", msg_len);
		for (i = 0; i < msg_len; i++)
			printk(" %02x", data[i]);
		printk("\n");
	}
	return msg_len;
}

#define BT_BMC_HWRST	0x80

static void reset_flags(struct si_sm_data *bt)
{
	if (bt_debug)
		printk(KERN_WARNING "IPMI BT: flag reset %s\n",
					status2txt(BT_STATUS));
	if (BT_STATUS & BT_H_BUSY)
		BT_CONTROL(BT_H_BUSY);	
	BT_CONTROL(BT_CLR_WR_PTR);	
	BT_CONTROL(BT_SMS_ATN);		
	BT_INTMASK_W(BT_BMC_HWRST);
}


static void drain_BMC2HOST(struct si_sm_data *bt)
{
	int i, size;

	if (!(BT_STATUS & BT_B2H_ATN)) 	
		return;

	BT_CONTROL(BT_H_BUSY);		
	BT_CONTROL(BT_B2H_ATN);		
	BT_STATUS;			
	BT_CONTROL(BT_B2H_ATN);		
	BT_CONTROL(BT_CLR_RD_PTR);	
	if (bt_debug)
		printk(KERN_WARNING "IPMI BT: stale response %s; ",
			status2txt(BT_STATUS));
	size = BMC2HOST;
	for (i = 0; i < size ; i++)
		BMC2HOST;
	BT_CONTROL(BT_H_BUSY);		
	if (bt_debug)
		printk("drained %d bytes\n", size + 1);
}

static inline void write_all_bytes(struct si_sm_data *bt)
{
	int i;

	if (bt_debug & BT_DEBUG_MSG) {
		printk(KERN_WARNING "BT: write %d bytes seq=0x%02X",
			bt->write_count, bt->seq);
		for (i = 0; i < bt->write_count; i++)
			printk(" %02x", bt->write_data[i]);
		printk("\n");
	}
	for (i = 0; i < bt->write_count; i++)
		HOST2BMC(bt->write_data[i]);
}

static inline int read_all_bytes(struct si_sm_data *bt)
{
	unsigned char i;


	bt->read_data[0] = BMC2HOST;
	bt->read_count = bt->read_data[0];

	if (bt->read_count < 4 || bt->read_count >= IPMI_MAX_MSG_LENGTH) {
		if (bt_debug & BT_DEBUG_MSG)
			printk(KERN_WARNING "BT: bad raw rsp len=%d\n",
				bt->read_count);
		bt->truncated = 1;
		return 1;	
	}
	for (i = 1; i <= bt->read_count; i++)
		bt->read_data[i] = BMC2HOST;
	bt->read_count++;	

	if (bt_debug & BT_DEBUG_MSG) {
		int max = bt->read_count;

		printk(KERN_WARNING "BT: got %d bytes seq=0x%02X",
			max, bt->read_data[2]);
		if (max > 16)
			max = 16;
		for (i = 0; i < max; i++)
			printk(KERN_CONT " %02x", bt->read_data[i]);
		printk(KERN_CONT "%s\n", bt->read_count == max ? "" : " ...");
	}

	
	if ((bt->read_data[3] == bt->write_data[3]) &&
	    (bt->read_data[2] == bt->write_data[2]) &&
	    ((bt->read_data[1] & 0xF8) == (bt->write_data[1] & 0xF8)))
			return 1;

	if (bt_debug & BT_DEBUG_MSG)
		printk(KERN_WARNING "IPMI BT: bad packet: "
		"want 0x(%02X, %02X, %02X) got (%02X, %02X, %02X)\n",
		bt->write_data[1] | 0x04, bt->write_data[2], bt->write_data[3],
		bt->read_data[1],  bt->read_data[2],  bt->read_data[3]);
	return 0;
}


static enum si_sm_result error_recovery(struct si_sm_data *bt,
					unsigned char status,
					unsigned char cCode)
{
	char *reason;

	bt->timeout = bt->BT_CAP_req2rsp;

	switch (cCode) {
	case IPMI_TIMEOUT_ERR:
		reason = "timeout";
		break;
	default:
		reason = "internal error";
		break;
	}

	printk(KERN_WARNING "IPMI BT: %s in %s %s ", 	
		reason, STATE2TXT, STATUS2TXT);

	(bt->error_retries)++;
	if (bt->error_retries < bt->BT_CAP_retries) {
		printk("%d retries left\n",
			bt->BT_CAP_retries - bt->error_retries);
		bt->state = BT_STATE_RESTART;
		return SI_SM_CALL_WITHOUT_DELAY;
	}

	printk(KERN_WARNING "failed %d retries, sending error response\n",
	       bt->BT_CAP_retries);
	if (!bt->nonzero_status)
		printk(KERN_ERR "IPMI BT: stuck, try power cycle\n");

	
	else if (bt->seq <= (unsigned char)(bt->BT_CAP_retries & 0xFF)) {
		printk(KERN_WARNING "IPMI: BT reset (takes 5 secs)\n");
		bt->state = BT_STATE_RESET1;
		return SI_SM_CALL_WITHOUT_DELAY;
	}


	bt->state = BT_STATE_IDLE;
	switch (cCode) {
	case IPMI_TIMEOUT_ERR:
		if (status & BT_B_BUSY) {
			cCode = IPMI_NODE_BUSY_ERR;
			bt->state = BT_STATE_LONG_BUSY;
		}
		break;
	default:
		break;
	}
	force_result(bt, cCode);
	return SI_SM_TRANSACTION_COMPLETE;
}


static enum si_sm_result bt_event(struct si_sm_data *bt, long time)
{
	unsigned char status, BT_CAP[8];
	static enum bt_states last_printed = BT_STATE_PRINTME;
	int i;

	status = BT_STATUS;
	bt->nonzero_status |= status;
	if ((bt_debug & BT_DEBUG_STATES) && (bt->state != last_printed)) {
		printk(KERN_WARNING "BT: %s %s TO=%ld - %ld \n",
			STATE2TXT,
			STATUS2TXT,
			bt->timeout,
			time);
		last_printed = bt->state;
	}


	if ((bt->state < BT_STATE_WRITE_BYTES) && (status & BT_B2H_ATN)) {
		drain_BMC2HOST(bt);
		BT_SI_SM_RETURN(SI_SM_CALL_WITH_DELAY);
	}

	if ((bt->state != BT_STATE_IDLE) &&
	    (bt->state <  BT_STATE_PRINTME)) {
		
		bt->timeout -= time;
		if ((bt->timeout < 0) && (bt->state < BT_STATE_RESET1))
			return error_recovery(bt,
					      status,
					      IPMI_TIMEOUT_ERR);
	}

	switch (bt->state) {


	case BT_STATE_IDLE:
		if (status & BT_SMS_ATN) {
			BT_CONTROL(BT_SMS_ATN);	
			return SI_SM_ATTN;
		}

		if (status & BT_H_BUSY)		
			BT_CONTROL(BT_H_BUSY);

		
		if (!bt->BT_CAP_outreqs)
			BT_STATE_CHANGE(BT_STATE_CAPABILITIES_BEGIN,
					SI_SM_CALL_WITHOUT_DELAY);
		bt->timeout = bt->BT_CAP_req2rsp;
		BT_SI_SM_RETURN(SI_SM_IDLE);

	case BT_STATE_XACTION_START:
		if (status & (BT_B_BUSY | BT_H2B_ATN))
			BT_SI_SM_RETURN(SI_SM_CALL_WITH_DELAY);
		if (BT_STATUS & BT_H_BUSY)
			BT_CONTROL(BT_H_BUSY);	
		BT_STATE_CHANGE(BT_STATE_WRITE_BYTES,
				SI_SM_CALL_WITHOUT_DELAY);

	case BT_STATE_WRITE_BYTES:
		if (status & BT_H_BUSY)
			BT_CONTROL(BT_H_BUSY);	
		BT_CONTROL(BT_CLR_WR_PTR);
		write_all_bytes(bt);
		BT_CONTROL(BT_H2B_ATN);	
		BT_STATE_CHANGE(BT_STATE_WRITE_CONSUME,
				SI_SM_CALL_WITHOUT_DELAY);

	case BT_STATE_WRITE_CONSUME:
		if (status & (BT_B_BUSY | BT_H2B_ATN))
			BT_SI_SM_RETURN(SI_SM_CALL_WITH_DELAY);
		BT_STATE_CHANGE(BT_STATE_READ_WAIT,
				SI_SM_CALL_WITHOUT_DELAY);

	

	case BT_STATE_READ_WAIT:
		if (!(status & BT_B2H_ATN))
			BT_SI_SM_RETURN(SI_SM_CALL_WITH_DELAY);
		BT_CONTROL(BT_H_BUSY);		


		BT_CONTROL(BT_B2H_ATN);		
		BT_STATE_CHANGE(BT_STATE_CLEAR_B2H,
				SI_SM_CALL_WITHOUT_DELAY);

	case BT_STATE_CLEAR_B2H:
		if (status & BT_B2H_ATN) {
			
			BT_CONTROL(BT_B2H_ATN);
			BT_SI_SM_RETURN(SI_SM_CALL_WITH_DELAY);
		}
		BT_STATE_CHANGE(BT_STATE_READ_BYTES,
				SI_SM_CALL_WITHOUT_DELAY);

	case BT_STATE_READ_BYTES:
		if (!(status & BT_H_BUSY))
			
			BT_CONTROL(BT_H_BUSY);
		BT_CONTROL(BT_CLR_RD_PTR);	
		i = read_all_bytes(bt);		
		BT_CONTROL(BT_H_BUSY);		
		if (!i) 			
			BT_STATE_CHANGE(BT_STATE_READ_WAIT,
					SI_SM_CALL_WITHOUT_DELAY);
		bt->state = bt->complete;
		return bt->state == BT_STATE_IDLE ?	
			SI_SM_TRANSACTION_COMPLETE :	
			SI_SM_CALL_WITHOUT_DELAY;	

	case BT_STATE_LONG_BUSY:	
		if (!(status & BT_B_BUSY)) {
			reset_flags(bt);	
			bt_init_data(bt, bt->io);
		}
		return SI_SM_CALL_WITH_DELAY;	

	case BT_STATE_RESET1:
		reset_flags(bt);
		drain_BMC2HOST(bt);
		BT_STATE_CHANGE(BT_STATE_RESET2,
				SI_SM_CALL_WITH_DELAY);

	case BT_STATE_RESET2:		
		BT_CONTROL(BT_CLR_WR_PTR);
		HOST2BMC(3);		
		HOST2BMC(0x18);		
		HOST2BMC(42);		
		HOST2BMC(3);		
		BT_CONTROL(BT_H2B_ATN);
		bt->timeout = BT_RESET_DELAY * 1000000;
		BT_STATE_CHANGE(BT_STATE_RESET3,
				SI_SM_CALL_WITH_DELAY);

	case BT_STATE_RESET3:		
		if (bt->timeout > 0)
			return SI_SM_CALL_WITH_DELAY;
		drain_BMC2HOST(bt);
		BT_STATE_CHANGE(BT_STATE_RESTART,
				SI_SM_CALL_WITH_DELAY);

	case BT_STATE_RESTART:		
		bt->read_count = 0;
		bt->nonzero_status = 0;
		bt->timeout = bt->BT_CAP_req2rsp;
		BT_STATE_CHANGE(BT_STATE_XACTION_START,
				SI_SM_CALL_WITH_DELAY);

	case BT_STATE_CAPABILITIES_BEGIN:
		bt->BT_CAP_outreqs = 1;
		{
			unsigned char GetBT_CAP[] = { 0x18, 0x36 };
			bt->state = BT_STATE_IDLE;
			bt_start_transaction(bt, GetBT_CAP, sizeof(GetBT_CAP));
		}
		bt->complete = BT_STATE_CAPABILITIES_END;
		BT_STATE_CHANGE(BT_STATE_XACTION_START,
				SI_SM_CALL_WITH_DELAY);

	case BT_STATE_CAPABILITIES_END:
		i = bt_get_result(bt, BT_CAP, sizeof(BT_CAP));
		bt_init_data(bt, bt->io);
		if ((i == 8) && !BT_CAP[2]) {
			bt->BT_CAP_outreqs = BT_CAP[3];
			bt->BT_CAP_req2rsp = BT_CAP[6] * 1000000;
			bt->BT_CAP_retries = BT_CAP[7];
		} else
			printk(KERN_WARNING "IPMI BT: using default values\n");
		if (!bt->BT_CAP_outreqs)
			bt->BT_CAP_outreqs = 1;
		printk(KERN_WARNING "IPMI BT: req2rsp=%ld secs retries=%d\n",
			bt->BT_CAP_req2rsp / 1000000L, bt->BT_CAP_retries);
		bt->timeout = bt->BT_CAP_req2rsp;
		return SI_SM_CALL_WITHOUT_DELAY;

	default:	
		return error_recovery(bt,
				      status,
				      IPMI_ERR_UNSPECIFIED);
	}
	return SI_SM_CALL_WITH_DELAY;
}

static int bt_detect(struct si_sm_data *bt)
{

	if ((BT_STATUS == 0xFF) && (BT_INTMASK_R == 0xFF))
		return 1;
	reset_flags(bt);
	return 0;
}

static void bt_cleanup(struct si_sm_data *bt)
{
}

static int bt_size(void)
{
	return sizeof(struct si_sm_data);
}

struct si_sm_handlers bt_smi_handlers = {
	.init_data		= bt_init_data,
	.start_transaction	= bt_start_transaction,
	.get_result		= bt_get_result,
	.event			= bt_event,
	.detect			= bt_detect,
	.cleanup		= bt_cleanup,
	.size			= bt_size,
};
