/*
 * ipmi_si_sm.h
 *
 * State machine interface for low-level IPMI system management
 * interface state machines.  This code is the interface between
 * the ipmi_smi code (that handles the policy of a KCS, SMIC, or
 * BT interface) and the actual low-level state machine.
 *
 * Author: MontaVista Software, Inc.
 *         Corey Minyard <minyard@mvista.com>
 *         source@mvista.com
 *
 * Copyright 2002 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
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
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

struct si_sm_data;

struct si_sm_io {
	unsigned char (*inputb)(struct si_sm_io *io, unsigned int offset);
	void (*outputb)(struct si_sm_io *io,
			unsigned int  offset,
			unsigned char b);

	void __iomem *addr;
	int  regspacing;
	int  regsize;
	int  regshift;
	int addr_type;
	long addr_data;
};

enum si_sm_result {
	SI_SM_CALL_WITHOUT_DELAY, 
	SI_SM_CALL_WITH_DELAY,	
	SI_SM_CALL_WITH_TICK_DELAY,
	SI_SM_TRANSACTION_COMPLETE, 
	SI_SM_IDLE,		
	SI_SM_HOSED,		

	SI_SM_ATTN
};

struct si_sm_handlers {
	char *version;

	unsigned int (*init_data)(struct si_sm_data *smi,
				  struct si_sm_io   *io);

	int (*start_transaction)(struct si_sm_data *smi,
				 unsigned char *data, unsigned int size);

	int (*get_result)(struct si_sm_data *smi,
			  unsigned char *data, unsigned int length);

	enum si_sm_result (*event)(struct si_sm_data *smi, long time);

	int (*detect)(struct si_sm_data *smi);

	
	void (*cleanup)(struct si_sm_data *smi);

	
	int (*size)(void);
};

extern struct si_sm_handlers kcs_smi_handlers;
extern struct si_sm_handlers smic_smi_handlers;
extern struct si_sm_handlers bt_smi_handlers;

