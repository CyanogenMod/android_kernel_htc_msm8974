/*
 * spu_restore.c
 *
 * (C) Copyright IBM Corp. 2005
 *
 * SPU-side context restore sequence outlined in
 * Synergistic Processor Element Book IV
 *
 * Author: Mark Nutter <mnutter@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#ifndef LS_SIZE
#define LS_SIZE                 0x40000	
#endif

typedef unsigned int u32;
typedef unsigned long long u64;

#include <spu_intrinsics.h>
#include <asm/spu_csa.h>
#include "spu_utils.h"

#define BR_INSTR		0x327fff80	
#define NOP_INSTR		0x40200000	
#define HEQ_INSTR		0x7b000000	
#define STOP_INSTR		0x00000000	
#define ILLEGAL_INSTR		0x00800000	
#define RESTORE_COMPLETE	0x00003ffc	

static inline void fetch_regs_from_mem(addr64 lscsa_ea)
{
	unsigned int ls = (unsigned int)&regs_spill[0];
	unsigned int size = sizeof(regs_spill);
	unsigned int tag_id = 0;
	unsigned int cmd = 0x40;	

	spu_writech(MFC_LSA, ls);
	spu_writech(MFC_EAH, lscsa_ea.ui[0]);
	spu_writech(MFC_EAL, lscsa_ea.ui[1]);
	spu_writech(MFC_Size, size);
	spu_writech(MFC_TagID, tag_id);
	spu_writech(MFC_Cmd, cmd);
}

static inline void restore_upper_240kb(addr64 lscsa_ea)
{
	unsigned int ls = 16384;
	unsigned int list = (unsigned int)&dma_list[0];
	unsigned int size = sizeof(dma_list);
	unsigned int tag_id = 0;
	unsigned int cmd = 0x44;	

	spu_writech(MFC_LSA, ls);
	spu_writech(MFC_EAH, lscsa_ea.ui[0]);
	spu_writech(MFC_EAL, list);
	spu_writech(MFC_Size, size);
	spu_writech(MFC_TagID, tag_id);
	spu_writech(MFC_Cmd, cmd);
}

static inline void restore_decr(void)
{
	unsigned int offset;
	unsigned int decr_running;
	unsigned int decr;

	offset = LSCSA_QW_OFFSET(decr_status);
	decr_running = regs_spill[offset].slot[0] & SPU_DECR_STATUS_RUNNING;
	if (decr_running) {
		offset = LSCSA_QW_OFFSET(decr);
		decr = regs_spill[offset].slot[0];
		spu_writech(SPU_WrDec, decr);
	}
}

static inline void write_ppu_mb(void)
{
	unsigned int offset;
	unsigned int data;

	offset = LSCSA_QW_OFFSET(ppu_mb);
	data = regs_spill[offset].slot[0];
	spu_writech(SPU_WrOutMbox, data);
}

static inline void write_ppuint_mb(void)
{
	unsigned int offset;
	unsigned int data;

	offset = LSCSA_QW_OFFSET(ppuint_mb);
	data = regs_spill[offset].slot[0];
	spu_writech(SPU_WrOutIntrMbox, data);
}

static inline void restore_fpcr(void)
{
	unsigned int offset;
	vector unsigned int fpcr;

	offset = LSCSA_QW_OFFSET(fpcr);
	fpcr = regs_spill[offset].v;
	spu_mtfpscr(fpcr);
}

static inline void restore_srr0(void)
{
	unsigned int offset;
	unsigned int srr0;

	offset = LSCSA_QW_OFFSET(srr0);
	srr0 = regs_spill[offset].slot[0];
	spu_writech(SPU_WrSRR0, srr0);
}

static inline void restore_event_mask(void)
{
	unsigned int offset;
	unsigned int event_mask;

	offset = LSCSA_QW_OFFSET(event_mask);
	event_mask = regs_spill[offset].slot[0];
	spu_writech(SPU_WrEventMask, event_mask);
}

static inline void restore_tag_mask(void)
{
	unsigned int offset;
	unsigned int tag_mask;

	offset = LSCSA_QW_OFFSET(tag_mask);
	tag_mask = regs_spill[offset].slot[0];
	spu_writech(MFC_WrTagMask, tag_mask);
}

static inline void restore_complete(void)
{
	extern void exit_fini(void);
	unsigned int *exit_instrs = (unsigned int *)exit_fini;
	unsigned int offset;
	unsigned int stopped_status;
	unsigned int stopped_code;


	offset = LSCSA_QW_OFFSET(stopped_status);
	stopped_status = regs_spill[offset].slot[0];
	stopped_code = regs_spill[offset].slot[1];

	switch (stopped_status) {
	case SPU_STOPPED_STATUS_P_I:
		exit_instrs[0] = RESTORE_COMPLETE;
		exit_instrs[1] = ILLEGAL_INSTR;
		exit_instrs[2] = STOP_INSTR | stopped_code;
		break;
	case SPU_STOPPED_STATUS_P_H:
		exit_instrs[0] = RESTORE_COMPLETE;
		exit_instrs[1] = HEQ_INSTR;
		exit_instrs[2] = STOP_INSTR | stopped_code;
		break;
	case SPU_STOPPED_STATUS_S_P:
		exit_instrs[0] = RESTORE_COMPLETE;
		exit_instrs[1] = STOP_INSTR | stopped_code;
		exit_instrs[2] = NOP_INSTR;
		exit_instrs[3] = BR_INSTR;
		break;
	case SPU_STOPPED_STATUS_S_I:
		exit_instrs[0] = RESTORE_COMPLETE;
		exit_instrs[1] = ILLEGAL_INSTR;
		exit_instrs[2] = NOP_INSTR;
		exit_instrs[3] = BR_INSTR;
		break;
	case SPU_STOPPED_STATUS_I:
		exit_instrs[0] = RESTORE_COMPLETE;
		exit_instrs[1] = ILLEGAL_INSTR;
		exit_instrs[2] = NOP_INSTR;
		exit_instrs[3] = BR_INSTR;
		break;
	case SPU_STOPPED_STATUS_S:
		
		exit_instrs[0] = RESTORE_COMPLETE;
		exit_instrs[1] = NOP_INSTR;
		exit_instrs[2] = NOP_INSTR;
		exit_instrs[3] = BR_INSTR;
		break;
	case SPU_STOPPED_STATUS_H:
		exit_instrs[0] = RESTORE_COMPLETE;
		exit_instrs[1] = HEQ_INSTR;
		exit_instrs[2] = NOP_INSTR;
		exit_instrs[3] = BR_INSTR;
		break;
	case SPU_STOPPED_STATUS_P:
		exit_instrs[0] = RESTORE_COMPLETE;
		exit_instrs[1] = STOP_INSTR | stopped_code;
		break;
	case SPU_STOPPED_STATUS_R:
		
		exit_instrs[0] = RESTORE_COMPLETE;
		exit_instrs[1] = NOP_INSTR;
		exit_instrs[2] = NOP_INSTR;
		exit_instrs[3] = BR_INSTR;
		break;
	default:
		
		break;
	}
	spu_sync();
}

int main()
{
	addr64 lscsa_ea;

	lscsa_ea.ui[0] = spu_readch(SPU_RdSigNotify1);
	lscsa_ea.ui[1] = spu_readch(SPU_RdSigNotify2);
	fetch_regs_from_mem(lscsa_ea);

	set_event_mask();		
	set_tag_mask();			
	build_dma_list(lscsa_ea);	
	restore_upper_240kb(lscsa_ea);	
					
	enqueue_putllc(lscsa_ea);	
	set_tag_update();		
	read_tag_status();		
	restore_decr();			
	read_llar_status();		
	write_ppu_mb();			
	write_ppuint_mb();		
	restore_fpcr();			
	restore_srr0();			
	restore_event_mask();		
	restore_tag_mask();		
					
	restore_complete();		

	return 0;
}
