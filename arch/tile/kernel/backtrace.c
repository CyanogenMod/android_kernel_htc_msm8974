/*
 * Copyright 2011 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <asm/backtrace.h>
#include <asm/tile-desc.h>
#include <arch/abi.h>

#ifdef __tilegx__
#define TILE_MAX_INSTRUCTIONS_PER_BUNDLE TILEGX_MAX_INSTRUCTIONS_PER_BUNDLE
#define tile_decoded_instruction tilegx_decoded_instruction
#define tile_mnemonic tilegx_mnemonic
#define parse_insn_tile parse_insn_tilegx
#define TILE_OPC_IRET TILEGX_OPC_IRET
#define TILE_OPC_ADDI TILEGX_OPC_ADDI
#define TILE_OPC_ADDLI TILEGX_OPC_ADDLI
#define TILE_OPC_INFO TILEGX_OPC_INFO
#define TILE_OPC_INFOL TILEGX_OPC_INFOL
#define TILE_OPC_JRP TILEGX_OPC_JRP
#define TILE_OPC_MOVE TILEGX_OPC_MOVE
#define OPCODE_STORE TILEGX_OPC_ST
typedef long long bt_int_reg_t;
#else
#define TILE_MAX_INSTRUCTIONS_PER_BUNDLE TILEPRO_MAX_INSTRUCTIONS_PER_BUNDLE
#define tile_decoded_instruction tilepro_decoded_instruction
#define tile_mnemonic tilepro_mnemonic
#define parse_insn_tile parse_insn_tilepro
#define TILE_OPC_IRET TILEPRO_OPC_IRET
#define TILE_OPC_ADDI TILEPRO_OPC_ADDI
#define TILE_OPC_ADDLI TILEPRO_OPC_ADDLI
#define TILE_OPC_INFO TILEPRO_OPC_INFO
#define TILE_OPC_INFOL TILEPRO_OPC_INFOL
#define TILE_OPC_JRP TILEPRO_OPC_JRP
#define TILE_OPC_MOVE TILEPRO_OPC_MOVE
#define OPCODE_STORE TILEPRO_OPC_SW
typedef int bt_int_reg_t;
#endif

struct BacktraceBundle {
	tile_bundle_bits bits;
	int num_insns;
	struct tile_decoded_instruction
	insns[TILE_MAX_INSTRUCTIONS_PER_BUNDLE];
};


static const struct tile_decoded_instruction *find_matching_insn(
	const struct BacktraceBundle *bundle,
	tile_mnemonic mnemonic,
	const int *operand_values,
	int num_operands_to_match)
{
	int i, j;
	bool match;

	for (i = 0; i < bundle->num_insns; i++) {
		const struct tile_decoded_instruction *insn =
			&bundle->insns[i];

		if (insn->opcode->mnemonic != mnemonic)
			continue;

		match = true;
		for (j = 0; j < num_operands_to_match; j++) {
			if (operand_values[j] != insn->operand_values[j]) {
				match = false;
				break;
			}
		}

		if (match)
			return insn;
	}

	return NULL;
}

static inline bool bt_has_iret(const struct BacktraceBundle *bundle)
{
	return find_matching_insn(bundle, TILE_OPC_IRET, NULL, 0) != NULL;
}

static bool bt_has_addi_sp(const struct BacktraceBundle *bundle, int *adjust)
{
	static const int vals[2] = { TREG_SP, TREG_SP };

	const struct tile_decoded_instruction *insn =
		find_matching_insn(bundle, TILE_OPC_ADDI, vals, 2);
	if (insn == NULL)
		insn = find_matching_insn(bundle, TILE_OPC_ADDLI, vals, 2);
#ifdef __tilegx__
	if (insn == NULL)
		insn = find_matching_insn(bundle, TILEGX_OPC_ADDXLI, vals, 2);
	if (insn == NULL)
		insn = find_matching_insn(bundle, TILEGX_OPC_ADDXI, vals, 2);
#endif
	if (insn == NULL)
		return false;

	*adjust = insn->operand_values[2];
	return true;
}

static int bt_get_info_ops(const struct BacktraceBundle *bundle,
		int operands[MAX_INFO_OPS_PER_BUNDLE])
{
	int num_ops = 0;
	int i;

	for (i = 0; i < bundle->num_insns; i++) {
		const struct tile_decoded_instruction *insn =
			&bundle->insns[i];

		if (insn->opcode->mnemonic == TILE_OPC_INFO ||
		    insn->opcode->mnemonic == TILE_OPC_INFOL) {
			operands[num_ops++] = insn->operand_values[0];
		}
	}

	return num_ops;
}

static bool bt_has_jrp(const struct BacktraceBundle *bundle, int *target_reg)
{
	const struct tile_decoded_instruction *insn =
		find_matching_insn(bundle, TILE_OPC_JRP, NULL, 0);
	if (insn == NULL)
		return false;

	*target_reg = insn->operand_values[0];
	return true;
}

static bool bt_modifies_reg(const struct BacktraceBundle *bundle, int reg)
{
	int i, j;
	for (i = 0; i < bundle->num_insns; i++) {
		const struct tile_decoded_instruction *insn =
			&bundle->insns[i];

		if (insn->opcode->implicitly_written_register == reg)
			return true;

		for (j = 0; j < insn->opcode->num_operands; j++)
			if (insn->operands[j]->is_dest_reg &&
			    insn->operand_values[j] == reg)
				return true;
	}

	return false;
}

static inline bool bt_modifies_sp(const struct BacktraceBundle *bundle)
{
	return bt_modifies_reg(bundle, TREG_SP);
}

static inline bool bt_modifies_lr(const struct BacktraceBundle *bundle)
{
	return bt_modifies_reg(bundle, TREG_LR);
}

static inline bool bt_has_move_r52_sp(const struct BacktraceBundle *bundle)
{
	static const int vals[2] = { 52, TREG_SP };
	return find_matching_insn(bundle, TILE_OPC_MOVE, vals, 2) != NULL;
}

static inline bool bt_has_sw_sp_lr(const struct BacktraceBundle *bundle)
{
	static const int vals[2] = { TREG_SP, TREG_LR };
	return find_matching_insn(bundle, OPCODE_STORE, vals, 2) != NULL;
}

#ifdef __tilegx__
static inline void bt_update_moveli(const struct BacktraceBundle *bundle,
				    int moveli_args[])
{
	int i;
	for (i = 0; i < bundle->num_insns; i++) {
		const struct tile_decoded_instruction *insn =
			&bundle->insns[i];

		if (insn->opcode->mnemonic == TILEGX_OPC_MOVELI) {
			int reg = insn->operand_values[0];
			moveli_args[reg] = insn->operand_values[1];
		}
	}
}

static bool bt_has_add_sp(const struct BacktraceBundle *bundle, int *adjust,
			  int moveli_args[])
{
	static const int vals[2] = { TREG_SP, TREG_SP };

	const struct tile_decoded_instruction *insn =
		find_matching_insn(bundle, TILEGX_OPC_ADDX, vals, 2);
	if (insn) {
		int reg = insn->operand_values[2];
		if (moveli_args[reg]) {
			*adjust = moveli_args[reg];
			return true;
		}
	}
	return false;
}
#endif

static void find_caller_pc_and_caller_sp(CallerLocation *location,
					 const unsigned long start_pc,
					 BacktraceMemoryReader read_memory_func,
					 void *read_memory_func_extra)
{
	bool sp_determined = false;

	
	bool lr_modified = false;

	
	bool sp_moved_to_r52 = false;

	
	bool seen_terminating_bundle = false;

	tile_bundle_bits prefetched_bundles[32];
	int num_bundles_prefetched = 0;
	int next_bundle = 0;
	unsigned long pc;

#ifdef __tilegx__
	
	int moveli_args[TILEGX_NUM_REGISTERS] = { 0 };
#endif

	location->sp_location = SP_LOC_OFFSET;
	location->sp_offset = 0;

	
	location->pc_location = PC_LOC_UNKNOWN;

	
	if (start_pc % TILE_BUNDLE_ALIGNMENT_IN_BYTES != 0)
		return;

	for (pc = start_pc;; pc += sizeof(tile_bundle_bits)) {

		struct BacktraceBundle bundle;
		int num_info_ops, info_operands[MAX_INFO_OPS_PER_BUNDLE];
		int one_ago, jrp_reg;
		bool has_jrp;

		if (next_bundle >= num_bundles_prefetched) {
			unsigned int bytes_to_prefetch = 4096 - (pc & 4095);
			if (bytes_to_prefetch > sizeof prefetched_bundles)
				bytes_to_prefetch = sizeof prefetched_bundles;

			if (!read_memory_func(prefetched_bundles, pc,
					      bytes_to_prefetch,
					      read_memory_func_extra)) {
				if (pc == start_pc) {
					location->pc_location = PC_LOC_IN_LR;
					return;
				}

				
				break;
			}

			next_bundle = 0;
			num_bundles_prefetched =
				bytes_to_prefetch / sizeof(tile_bundle_bits);
		}

		
		bundle.bits = prefetched_bundles[next_bundle++];
		bundle.num_insns =
			parse_insn_tile(bundle.bits, pc, bundle.insns);
		num_info_ops = bt_get_info_ops(&bundle, info_operands);

		for (one_ago = (pc != start_pc) ? 1 : 0;
		     one_ago >= 0; one_ago--) {
			int i;
			for (i = 0; i < num_info_ops; i++) {
				int info_operand = info_operands[i];
				if (info_operand < CALLER_UNKNOWN_BASE)	{
					
					continue;
				}

				if (((info_operand & ONE_BUNDLE_AGO_FLAG) != 0)
				    != (one_ago != 0))
					continue;

				info_operand &= ~ONE_BUNDLE_AGO_FLAG;

				
				if (info_operand & PC_IN_LR_FLAG)
					location->pc_location =
						PC_LOC_IN_LR;
				else
					location->pc_location =
						PC_LOC_ON_STACK;

				switch (info_operand) {
				case CALLER_UNKNOWN_BASE:
					location->pc_location = PC_LOC_UNKNOWN;
					location->sp_location = SP_LOC_UNKNOWN;
					return;

				case CALLER_SP_IN_R52_BASE:
				case CALLER_SP_IN_R52_BASE | PC_IN_LR_FLAG:
					location->sp_location = SP_LOC_IN_R52;
					return;

				default:
				{
					const unsigned int val = info_operand
						- CALLER_SP_OFFSET_BASE;
					const unsigned int sp_offset =
						(val >> NUM_INFO_OP_FLAGS) * 8;
					if (sp_offset < 32768) {
						location->sp_location =
							SP_LOC_OFFSET;
						location->sp_offset =
							sp_offset;
						return;
					} else {
					}
				}
				break;
				}
			}
		}

		if (seen_terminating_bundle) {
			break;
		}

		if (bundle.bits == 0) {
			break;
		}


		if (!sp_determined) {
			int adjust;
			if (bt_has_addi_sp(&bundle, &adjust)
#ifdef __tilegx__
			    || bt_has_add_sp(&bundle, &adjust, moveli_args)
#endif
				) {
				location->sp_location = SP_LOC_OFFSET;

				if (adjust <= 0) {
					location->sp_offset = 0;
				} else {
					
					location->sp_offset = adjust;
				}

				sp_determined = true;
			} else {
				if (bt_has_move_r52_sp(&bundle)) {
					sp_moved_to_r52 = true;
				}

				if (bt_modifies_sp(&bundle)) {
					if (sp_moved_to_r52) {
						location->sp_location =
							SP_LOC_OFFSET;
						location->sp_offset = 0;
					} else {
						location->sp_location =
							SP_LOC_IN_R52;
					}
					sp_determined = true;
				}
			}

#ifdef __tilegx__
			
			bt_update_moveli(&bundle, moveli_args);
#endif
		}

		if (bt_has_iret(&bundle)) {
			
			seen_terminating_bundle = true;
			continue;
		}


		jrp_reg = -1;
		has_jrp = bt_has_jrp(&bundle, &jrp_reg);
		if (has_jrp)
			seen_terminating_bundle = true;

		if (location->pc_location == PC_LOC_UNKNOWN) {
			if (has_jrp) {
				if (jrp_reg == TREG_LR && !lr_modified) {
					location->pc_location =
						PC_LOC_IN_LR;
				} else {
					location->pc_location =
						PC_LOC_ON_STACK;
				}
			} else if (bt_has_sw_sp_lr(&bundle)) {
				
				location->pc_location = PC_LOC_IN_LR;
			} else if (bt_modifies_lr(&bundle)) {
				lr_modified = true;
			}
		}
	}
}

void backtrace_init(BacktraceIterator *state,
		    BacktraceMemoryReader read_memory_func,
		    void *read_memory_func_extra,
		    unsigned long pc, unsigned long lr,
		    unsigned long sp, unsigned long r52)
{
	CallerLocation location;
	unsigned long fp, initial_frame_caller_pc;

	
	find_caller_pc_and_caller_sp(&location, pc,
				     read_memory_func, read_memory_func_extra);

	switch (location.sp_location) {
	case SP_LOC_UNKNOWN:
		
		fp = -1;
		break;

	case SP_LOC_IN_R52:
		fp = r52;
		break;

	case SP_LOC_OFFSET:
		fp = sp + location.sp_offset;
		break;

	default:
		
		fp = -1;
		break;
	}

	if (fp % sizeof(bt_int_reg_t) != 0)
		fp = -1;

	
	initial_frame_caller_pc = -1;

	switch (location.pc_location) {
	case PC_LOC_UNKNOWN:
		
		fp = -1;
		break;

	case PC_LOC_IN_LR:
		if (lr == 0 || lr % TILE_BUNDLE_ALIGNMENT_IN_BYTES != 0) {
			
			fp = -1;
		} else {
			initial_frame_caller_pc = lr;
		}
		break;

	case PC_LOC_ON_STACK:
		break;

	default:
		
		fp = -1;
		break;
	}

	state->pc = pc;
	state->sp = sp;
	state->fp = fp;
	state->initial_frame_caller_pc = initial_frame_caller_pc;
	state->read_memory_func = read_memory_func;
	state->read_memory_func_extra = read_memory_func_extra;
}

static bool valid_addr_reg(bt_int_reg_t reg)
{
	return ((unsigned long)reg == reg);
}

bool backtrace_next(BacktraceIterator *state)
{
	unsigned long next_fp, next_pc;
	bt_int_reg_t next_frame[2];

	if (state->fp == -1) {
		
		return false;
	}

	
	if (!state->read_memory_func(&next_frame, state->fp, sizeof next_frame,
				     state->read_memory_func_extra)) {
		return false;
	}

	next_fp = next_frame[1];
	if (!valid_addr_reg(next_frame[1]) ||
	    next_fp % sizeof(bt_int_reg_t) != 0) {
		
		return false;
	}

	if (state->initial_frame_caller_pc != -1) {
		next_pc = state->initial_frame_caller_pc;

		state->initial_frame_caller_pc = -1;
	} else {
		
		next_pc = next_frame[0];
		if (!valid_addr_reg(next_frame[0]) || next_pc == 0 ||
		    next_pc % TILE_BUNDLE_ALIGNMENT_IN_BYTES != 0) {
			
			return false;
		}
	}

	
	state->pc = next_pc;
	state->sp = state->fp;
	state->fp = next_fp;

	return true;
}
