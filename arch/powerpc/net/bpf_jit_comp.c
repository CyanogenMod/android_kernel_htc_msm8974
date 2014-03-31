/* bpf_jit_comp.c: BPF JIT compiler for PPC64
 *
 * Copyright 2011 Matt Evans <matt@ozlabs.org>, IBM Corporation
 *
 * Based on the x86 BPF compiler, by Eric Dumazet (eric.dumazet@gmail.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#include <linux/moduleloader.h>
#include <asm/cacheflush.h>
#include <linux/netdevice.h>
#include <linux/filter.h>
#include "bpf_jit.h"

#ifndef __BIG_ENDIAN
#error "Little-endian PPC not supported in BPF compiler"
#endif

int bpf_jit_enable __read_mostly;


static inline void bpf_flush_icache(void *start, void *end)
{
	smp_wmb();
	flush_icache_range((unsigned long)start, (unsigned long)end);
}

static void bpf_jit_build_prologue(struct sk_filter *fp, u32 *image,
				   struct codegen_context *ctx)
{
	int i;
	const struct sock_filter *filter = fp->insns;

	if (ctx->seen & (SEEN_MEM | SEEN_DATAREF)) {
		
		if (ctx->seen & SEEN_DATAREF) {
			
			EMIT(PPC_INST_MFLR | __PPC_RT(0));
			PPC_STD(0, 1, 16);

			
			PPC_STD(r_D, 1, -(8*(32-r_D)));
			PPC_STD(r_HL, 1, -(8*(32-r_HL)));
		}
		if (ctx->seen & SEEN_MEM) {
			for (i = r_M; i < (r_M+16); i++) {
				if (ctx->seen & (1 << (i-r_M)))
					PPC_STD(i, 1, -(8*(32-i)));
			}
		}
		EMIT(PPC_INST_STDU | __PPC_RS(1) | __PPC_RA(1) |
		     (-BPF_PPC_STACKFRAME & 0xfffc));
	}

	if (ctx->seen & SEEN_DATAREF) {
		PPC_LWZ_OFFS(r_scratch1, r_skb, offsetof(struct sk_buff,
							 data_len));
		PPC_LWZ_OFFS(r_HL, r_skb, offsetof(struct sk_buff, len));
		PPC_SUB(r_HL, r_HL, r_scratch1);
		PPC_LD_OFFS(r_D, r_skb, offsetof(struct sk_buff, data));
	}

	if (ctx->seen & SEEN_XREG) {
		PPC_LI(r_X, 0);
	}

	switch (filter[0].code) {
	case BPF_S_RET_K:
	case BPF_S_LD_W_LEN:
	case BPF_S_ANC_PROTOCOL:
	case BPF_S_ANC_IFINDEX:
	case BPF_S_ANC_MARK:
	case BPF_S_ANC_RXHASH:
	case BPF_S_ANC_CPU:
	case BPF_S_ANC_QUEUE:
	case BPF_S_LD_W_ABS:
	case BPF_S_LD_H_ABS:
	case BPF_S_LD_B_ABS:
		
		break;
	default:
		
		PPC_LI(r_A, 0);
	}
}

static void bpf_jit_build_epilogue(u32 *image, struct codegen_context *ctx)
{
	int i;

	if (ctx->seen & (SEEN_MEM | SEEN_DATAREF)) {
		PPC_ADDI(1, 1, BPF_PPC_STACKFRAME);
		if (ctx->seen & SEEN_DATAREF) {
			PPC_LD(0, 1, 16);
			PPC_MTLR(0);
			PPC_LD(r_D, 1, -(8*(32-r_D)));
			PPC_LD(r_HL, 1, -(8*(32-r_HL)));
		}
		if (ctx->seen & SEEN_MEM) {
			
			for (i = r_M; i < (r_M+16); i++) {
				if (ctx->seen & (1 << (i-r_M)))
					PPC_LD(i, 1, -(8*(32-i)));
			}
		}
	}
	

	PPC_BLR();
}

#define CHOOSE_LOAD_FUNC(K, func) \
	((int)K < 0 ? ((int)K >= SKF_LL_OFF ? func##_negative_offset : func) : func##_positive_offset)

static int bpf_jit_build_body(struct sk_filter *fp, u32 *image,
			      struct codegen_context *ctx,
			      unsigned int *addrs)
{
	const struct sock_filter *filter = fp->insns;
	int flen = fp->len;
	u8 *func;
	unsigned int true_cond;
	int i;

	
	unsigned int exit_addr = addrs[flen];

	for (i = 0; i < flen; i++) {
		unsigned int K = filter[i].k;

		addrs[i] = ctx->idx * 4;

		switch (filter[i].code) {
			
		case BPF_S_ALU_ADD_X: 
			ctx->seen |= SEEN_XREG;
			PPC_ADD(r_A, r_A, r_X);
			break;
		case BPF_S_ALU_ADD_K: 
			if (!K)
				break;
			PPC_ADDI(r_A, r_A, IMM_L(K));
			if (K >= 32768)
				PPC_ADDIS(r_A, r_A, IMM_HA(K));
			break;
		case BPF_S_ALU_SUB_X: 
			ctx->seen |= SEEN_XREG;
			PPC_SUB(r_A, r_A, r_X);
			break;
		case BPF_S_ALU_SUB_K: 
			if (!K)
				break;
			PPC_ADDI(r_A, r_A, IMM_L(-K));
			if (K >= 32768)
				PPC_ADDIS(r_A, r_A, IMM_HA(-K));
			break;
		case BPF_S_ALU_MUL_X: 
			ctx->seen |= SEEN_XREG;
			PPC_MUL(r_A, r_A, r_X);
			break;
		case BPF_S_ALU_MUL_K: 
			if (K < 32768)
				PPC_MULI(r_A, r_A, K);
			else {
				PPC_LI32(r_scratch1, K);
				PPC_MUL(r_A, r_A, r_scratch1);
			}
			break;
		case BPF_S_ALU_DIV_X: 
			ctx->seen |= SEEN_XREG;
			PPC_CMPWI(r_X, 0);
			if (ctx->pc_ret0 != -1) {
				PPC_BCC(COND_EQ, addrs[ctx->pc_ret0]);
			} else {
				PPC_BCC_SHORT(COND_NE, (ctx->idx*4)+12);
				PPC_LI(r_ret, 0);
				PPC_JMP(exit_addr);
			}
			PPC_DIVWU(r_A, r_A, r_X);
			break;
		case BPF_S_ALU_DIV_K: 
			PPC_LI32(r_scratch1, K);
			
			PPC_MULHWU(r_A, r_A, r_scratch1);
			break;
		case BPF_S_ALU_AND_X:
			ctx->seen |= SEEN_XREG;
			PPC_AND(r_A, r_A, r_X);
			break;
		case BPF_S_ALU_AND_K:
			if (!IMM_H(K))
				PPC_ANDI(r_A, r_A, K);
			else {
				PPC_LI32(r_scratch1, K);
				PPC_AND(r_A, r_A, r_scratch1);
			}
			break;
		case BPF_S_ALU_OR_X:
			ctx->seen |= SEEN_XREG;
			PPC_OR(r_A, r_A, r_X);
			break;
		case BPF_S_ALU_OR_K:
			if (IMM_L(K))
				PPC_ORI(r_A, r_A, IMM_L(K));
			if (K >= 65536)
				PPC_ORIS(r_A, r_A, IMM_H(K));
			break;
		case BPF_S_ALU_LSH_X: 
			ctx->seen |= SEEN_XREG;
			PPC_SLW(r_A, r_A, r_X);
			break;
		case BPF_S_ALU_LSH_K:
			if (K == 0)
				break;
			else
				PPC_SLWI(r_A, r_A, K);
			break;
		case BPF_S_ALU_RSH_X: 
			ctx->seen |= SEEN_XREG;
			PPC_SRW(r_A, r_A, r_X);
			break;
		case BPF_S_ALU_RSH_K: 
			if (K == 0)
				break;
			else
				PPC_SRWI(r_A, r_A, K);
			break;
		case BPF_S_ALU_NEG:
			PPC_NEG(r_A, r_A);
			break;
		case BPF_S_RET_K:
			PPC_LI32(r_ret, K);
			if (!K) {
				if (ctx->pc_ret0 == -1)
					ctx->pc_ret0 = i;
			}
			if (i != flen - 1) {
				if (ctx->seen)
					PPC_JMP(exit_addr);
				else
					PPC_BLR();
			}
			break;
		case BPF_S_RET_A:
			PPC_MR(r_ret, r_A);
			if (i != flen - 1) {
				if (ctx->seen)
					PPC_JMP(exit_addr);
				else
					PPC_BLR();
			}
			break;
		case BPF_S_MISC_TAX: 
			PPC_MR(r_X, r_A);
			break;
		case BPF_S_MISC_TXA: 
			ctx->seen |= SEEN_XREG;
			PPC_MR(r_A, r_X);
			break;

			
		case BPF_S_LD_IMM: 
			PPC_LI32(r_A, K);
			break;
		case BPF_S_LDX_IMM: 
			PPC_LI32(r_X, K);
			break;
		case BPF_S_LD_MEM: 
			PPC_MR(r_A, r_M + (K & 0xf));
			ctx->seen |= SEEN_MEM | (1<<(K & 0xf));
			break;
		case BPF_S_LDX_MEM: 
			PPC_MR(r_X, r_M + (K & 0xf));
			ctx->seen |= SEEN_MEM | (1<<(K & 0xf));
			break;
		case BPF_S_ST: 
			PPC_MR(r_M + (K & 0xf), r_A);
			ctx->seen |= SEEN_MEM | (1<<(K & 0xf));
			break;
		case BPF_S_STX: 
			PPC_MR(r_M + (K & 0xf), r_X);
			ctx->seen |= SEEN_XREG | SEEN_MEM | (1<<(K & 0xf));
			break;
		case BPF_S_LD_W_LEN: 
			BUILD_BUG_ON(FIELD_SIZEOF(struct sk_buff, len) != 4);
			PPC_LWZ_OFFS(r_A, r_skb, offsetof(struct sk_buff, len));
			break;
		case BPF_S_LDX_W_LEN: 
			PPC_LWZ_OFFS(r_X, r_skb, offsetof(struct sk_buff, len));
			break;

			

		case BPF_S_ANC_PROTOCOL: 
			BUILD_BUG_ON(FIELD_SIZEOF(struct sk_buff,
						  protocol) != 2);
			PPC_LHZ_OFFS(r_A, r_skb, offsetof(struct sk_buff,
							  protocol));
			
			break;
		case BPF_S_ANC_IFINDEX:
			PPC_LD_OFFS(r_scratch1, r_skb, offsetof(struct sk_buff,
								dev));
			PPC_CMPDI(r_scratch1, 0);
			if (ctx->pc_ret0 != -1) {
				PPC_BCC(COND_EQ, addrs[ctx->pc_ret0]);
			} else {
				
				PPC_BCC_SHORT(COND_NE, (ctx->idx*4)+12);
				PPC_LI(r_ret, 0);
				PPC_JMP(exit_addr);
			}
			BUILD_BUG_ON(FIELD_SIZEOF(struct net_device,
						  ifindex) != 4);
			PPC_LWZ_OFFS(r_A, r_scratch1,
				     offsetof(struct net_device, ifindex));
			break;
		case BPF_S_ANC_MARK:
			BUILD_BUG_ON(FIELD_SIZEOF(struct sk_buff, mark) != 4);
			PPC_LWZ_OFFS(r_A, r_skb, offsetof(struct sk_buff,
							  mark));
			break;
		case BPF_S_ANC_RXHASH:
			BUILD_BUG_ON(FIELD_SIZEOF(struct sk_buff, rxhash) != 4);
			PPC_LWZ_OFFS(r_A, r_skb, offsetof(struct sk_buff,
							  rxhash));
			break;
		case BPF_S_ANC_QUEUE:
			BUILD_BUG_ON(FIELD_SIZEOF(struct sk_buff,
						  queue_mapping) != 2);
			PPC_LHZ_OFFS(r_A, r_skb, offsetof(struct sk_buff,
							  queue_mapping));
			break;
		case BPF_S_ANC_CPU:
#ifdef CONFIG_SMP
			BUILD_BUG_ON(FIELD_SIZEOF(struct paca_struct,
						  paca_index) != 2);
			PPC_LHZ_OFFS(r_A, 13,
				     offsetof(struct paca_struct, paca_index));
#else
			PPC_LI(r_A, 0);
#endif
			break;

			
		case BPF_S_LD_W_ABS:
			func = CHOOSE_LOAD_FUNC(K, sk_load_word);
			goto common_load;
		case BPF_S_LD_H_ABS:
			func = CHOOSE_LOAD_FUNC(K, sk_load_half);
			goto common_load;
		case BPF_S_LD_B_ABS:
			func = CHOOSE_LOAD_FUNC(K, sk_load_byte);
		common_load:
			
			ctx->seen |= SEEN_DATAREF;
			PPC_LI64(r_scratch1, func);
			PPC_MTLR(r_scratch1);
			PPC_LI32(r_addr, K);
			PPC_BLRL();
			PPC_BCC(COND_LT, exit_addr);
			break;

			
		case BPF_S_LD_W_IND:
			func = sk_load_word;
			goto common_load_ind;
		case BPF_S_LD_H_IND:
			func = sk_load_half;
			goto common_load_ind;
		case BPF_S_LD_B_IND:
			func = sk_load_byte;
		common_load_ind:
			ctx->seen |= SEEN_DATAREF | SEEN_XREG;
			PPC_LI64(r_scratch1, func);
			PPC_MTLR(r_scratch1);
			PPC_ADDI(r_addr, r_X, IMM_L(K));
			if (K >= 32768)
				PPC_ADDIS(r_addr, r_addr, IMM_HA(K));
			PPC_BLRL();
			
			PPC_BCC(COND_LT, exit_addr);
			break;

		case BPF_S_LDX_B_MSH:
			func = CHOOSE_LOAD_FUNC(K, sk_load_byte_msh);
			goto common_load;
			break;

			
		case BPF_S_JMP_JA:
			if (K != 0)
				PPC_JMP(addrs[i + 1 + K]);
			break;

		case BPF_S_JMP_JGT_K:
		case BPF_S_JMP_JGT_X:
			true_cond = COND_GT;
			goto cond_branch;
		case BPF_S_JMP_JGE_K:
		case BPF_S_JMP_JGE_X:
			true_cond = COND_GE;
			goto cond_branch;
		case BPF_S_JMP_JEQ_K:
		case BPF_S_JMP_JEQ_X:
			true_cond = COND_EQ;
			goto cond_branch;
		case BPF_S_JMP_JSET_K:
		case BPF_S_JMP_JSET_X:
			true_cond = COND_NE;
			
		cond_branch:
			
			if (filter[i].jt == filter[i].jf) {
				if (filter[i].jt > 0)
					PPC_JMP(addrs[i + 1 + filter[i].jt]);
				break;
			}

			switch (filter[i].code) {
			case BPF_S_JMP_JGT_X:
			case BPF_S_JMP_JGE_X:
			case BPF_S_JMP_JEQ_X:
				ctx->seen |= SEEN_XREG;
				PPC_CMPLW(r_A, r_X);
				break;
			case BPF_S_JMP_JSET_X:
				ctx->seen |= SEEN_XREG;
				PPC_AND_DOT(r_scratch1, r_A, r_X);
				break;
			case BPF_S_JMP_JEQ_K:
			case BPF_S_JMP_JGT_K:
			case BPF_S_JMP_JGE_K:
				if (K < 32768)
					PPC_CMPLWI(r_A, K);
				else {
					PPC_LI32(r_scratch1, K);
					PPC_CMPLW(r_A, r_scratch1);
				}
				break;
			case BPF_S_JMP_JSET_K:
				if (K < 32768)
					
					PPC_ANDI(r_scratch1, r_A, K);
				else {
					PPC_LI32(r_scratch1, K);
					PPC_AND_DOT(r_scratch1, r_A,
						    r_scratch1);
				}
				break;
			}
			if (filter[i].jt == 0)
				
				PPC_BCC(true_cond ^ COND_CMP_TRUE,
					addrs[i + 1 + filter[i].jf]);
			else {
				PPC_BCC(true_cond, addrs[i + 1 + filter[i].jt]);
				if (filter[i].jf != 0)
					PPC_JMP(addrs[i + 1 + filter[i].jf]);
			}
			break;
		default:
			if (printk_ratelimit())
				pr_err("BPF filter opcode %04x (@%d) unsupported\n",
				       filter[i].code, i);
			return -ENOTSUPP;
		}

	}
	
	addrs[i] = ctx->idx * 4;

	return 0;
}

void bpf_jit_compile(struct sk_filter *fp)
{
	unsigned int proglen;
	unsigned int alloclen;
	u32 *image = NULL;
	u32 *code_base;
	unsigned int *addrs;
	struct codegen_context cgctx;
	int pass;
	int flen = fp->len;

	if (!bpf_jit_enable)
		return;

	addrs = kzalloc((flen+1) * sizeof(*addrs), GFP_KERNEL);
	if (addrs == NULL)
		return;


	cgctx.idx = 0;
	cgctx.seen = 0;
	cgctx.pc_ret0 = -1;
	
	if (bpf_jit_build_body(fp, 0, &cgctx, addrs))
		
		goto out;

	bpf_jit_build_prologue(fp, 0, &cgctx);
	bpf_jit_build_epilogue(0, &cgctx);

	proglen = cgctx.idx * 4;
	alloclen = proglen + FUNCTION_DESCR_SIZE;
	image = module_alloc(max_t(unsigned int, alloclen,
				   sizeof(struct work_struct)));
	if (!image)
		goto out;

	code_base = image + (FUNCTION_DESCR_SIZE/4);

	
	for (pass = 1; pass < 3; pass++) {
		
		cgctx.idx = 0;
		bpf_jit_build_prologue(fp, code_base, &cgctx);
		bpf_jit_build_body(fp, code_base, &cgctx, addrs);
		bpf_jit_build_epilogue(code_base, &cgctx);

		if (bpf_jit_enable > 1)
			pr_info("Pass %d: shrink = %d, seen = 0x%x\n", pass,
				proglen - (cgctx.idx * 4), cgctx.seen);
	}

	if (bpf_jit_enable > 1)
		pr_info("flen=%d proglen=%u pass=%d image=%p\n",
		       flen, proglen, pass, image);

	if (image) {
		if (bpf_jit_enable > 1)
			print_hex_dump(KERN_ERR, "JIT code: ",
				       DUMP_PREFIX_ADDRESS,
				       16, 1, code_base,
				       proglen, false);

		bpf_flush_icache(code_base, code_base + (proglen/4));
		
		((u64 *)image)[0] = (u64)code_base;
		((u64 *)image)[1] = local_paca->kernel_toc;
		fp->bpf_func = (void *)image;
	}
out:
	kfree(addrs);
	return;
}

static void jit_free_defer(struct work_struct *arg)
{
	module_free(NULL, arg);
}

void bpf_jit_free(struct sk_filter *fp)
{
	if (fp->bpf_func != sk_run_filter) {
		struct work_struct *work = (struct work_struct *)fp->bpf_func;

		INIT_WORK(work, jit_free_defer);
		schedule_work(work);
	}
}
