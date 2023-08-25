/* SPDX-License-Identifier: GPL-3.0-or-later */

/*!
 * \file a64.c
 * \brief AArch64 code generator
 *

 */

#include "eigth.h"

/*
 * \brief Get an A64 register number from any eigth register number.
 *
 * Eigth to A64 register mapping is:
 *
 * [0..7] r0..r7 to w19..w26
 * [8..11] arg0..arg3 to w0..w3
 * [12] wzero (31)
 */
static reg_t REG(reg_t x)
{
	if (x < 8)
		return x + 19;
	if (x < 12)
		return x - 8;

	return 31;
}

/*
 * \brief Get an A64 register number from an eigth argument number.
 *
 * This is a no-op for A64!
 */
#define ARG(x) (x)

/*
 * \brief Pack bits ready to "or" into an opcode
 */
static reg_t bits(reg_t val, reg_t width, reg_t shift)
{
	return (((1 << width) - 1) & val) << shift;
}


#define XFP 29
#define XLR 30
#define XSP 31
#define WZR 31
#define XZR 31

#define LSL 0
#define LSR 1
#define ASR 2
#define ROR 3

enum condition_codes {
	C_EQ,
	C_NE,
	C_CS,
	C_HS = C_CS,
	C_CC,
	C_LO = C_CC,
	C_MI,
	C_PL,
	C_VS,
	C_VC,
	C_HI,
	C_LS,
	C_GE,
	C_LT,
	C_GT,
	C_LE,
	C_AL,
};

#define OP_ADD_IMM_W(Rt, Rn, imm12) \
	(0x11000000 | bits(imm12, 12, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_ADD_IMM_X(Rt, Rn, imm12) \
	(0x91000000 | bits(imm12, 12, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_B(offset) \
	(0x14000000 | bits(offset, 26, 0))
#define OP_B_COND(cond, offset) \
	(0x54000000 | bits(offset, 19, 5) | bits(cond, 4, 0))
#define OP_BL(offset) \
	(0x94000000 | bits(offset, 26, 0))
#define OP_CMP_REG_W(Rn, Rm) OP_SUBS_REG_W(WZR, Rn, Rm)
#define OP_CMP_REG_X(Rn, Rm) OP_SUBS_REG_X(XZR, Rn, Rm)
#define OP_LDP_POST_W(Rt, Rt2, Rn, imm7) \
	(0x28c00000 | bits(imm7, 7, 15) | bits(Rt2, 5, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_LDP_POST_X(Rt, Rt2, Rn, imm7) \
	(0xa8c00000 | bits(imm7, 7, 15) | bits(Rt2, 5, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_LDP_PRE_W(Rt, Rt2, Rn, imm7) \
	(0x29c00000 | bits(imm7, 7, 15) | bits(Rt2, 5, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_LDP_PRE_X(Rt, Rt2, Rn, imm7) \
	(0xa9c00000 | bits(imm7, 7, 15) | bits(Rt2, 5, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_LDP_OFFSET_W(Rt, Rt2, Rn, imm7) \
	(0x29400000 | bits(imm7, 7, 15) | bits(Rt2, 5, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_LDP_OFFSET_X(Rt, Rt2, Rn, imm7) \
	(0xa9400000 | bits(imm7, 7, 15) | bits(Rt2, 5, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_LDR_POST_W(Rt, Rn, imm9) \
	(0xb8400400 | bits(imm9, 9, 12) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_LDR_POST_X(Rt, Rn, imm9) \
	(0xf8400400 | bits(imm9, 9, 12) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_LDR_PRE_W(Rt, Rn, imm9) \
	(0xb8400c00 | bits(imm9, 9, 12) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_LDR_PRE_X(Rt, Rn, imm9) \
	(0xf8400c00 | bits(imm9, 9, 12) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_LDR_OFFSET_W(Rt, Rn, imm12) \
	(0xb9400000 | bits(imm12, 12, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_LDR_OFFSET_X(Rt, Rn, imm12) \
	(0xf9400000 | bits(imm12, 12, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_MOV_SP(Rd, Rn) OP_ADD_IMM_X(Rd, Rn, 0)
#define OP_MOV_IMM_W(Rd, imm16) OP_MOVZ_W(Rd, imm16, 0)
#define OP_MOV_IMM_X(Rd, imm16) OP_MOVZ_X(Rd, imm16, 0)
#define OP_MOV_REG_W(Rd, Rn) OP_ORR_REG_W(Rd, Rn, WZR, LSL, 0)
#define OP_MOV_REG_X(Rd, Rn) OP_ORR_REG_X(Rd, Rn, XZR, LSL, 0)
#define OP_MOVK_W(Rd, imm16, lsl) \
	(0x72800000 | bits(lsl >> 4, 2, 21) | bits(imm16, 16, 5) | bits(Rd, 5, 0))
#define OP_MOVK_X(Rd, imm16, lsl) \
	(0xf2800000 | bits(lsl >> 4, 2, 21) | bits(imm16, 16, 5) | bits(Rd, 5, 0))
#define OP_MOVZ_W(Rd, imm16, lsl) \
	(0x52800000 | bits(lsl >> 4, 2, 21) | bits(imm16, 16, 5) | bits(Rd, 5, 0))
#define OP_MOVZ_X(Rd, imm16, lsl) \
	(0xd2800000 | bits(lsl >> 4, 2, 21) | bits(imm16, 16, 5) | bits(Rd, 5, 0))
#define OP_ORR_REG_W(Rt, Rn, Rm, shift, imm6)                \
	(0x2a000000 | bits(shift, 2, 22) | bits(Rm, 5, 16) | \
	 bits(imm6, 6, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_ORR_REG_X(Rt, Rn, Rm, shift, imm6)                \
	(0xaa000000 | bits(shift, 2, 22) | bits(Rm, 5, 16) | \
	 bits(imm6, 6, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_RET(Rn) \
	(0xd65f0000 | bits(Rn, 5, 5))
#define OP_STP_POST_W(Rt, Rt2, Rn, imm7) \
	(0x28800000 | bits(imm7, 7, 15) | bits(Rt2, 5, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_STP_POST_X(Rt, Rt2, Rn, imm7) \
	(0xa8800000 | bits(imm7, 7, 15) | bits(Rt2, 5, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_STP_PRE_W(Rt, Rt2, Rn, imm7) \
	(0x29800000 | bits(imm7, 7, 15) | bits(Rt2, 5, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_STP_PRE_X(Rt, Rt2, Rn, imm7) \
	(0xa9800000 | bits(imm7, 7, 15) | bits(Rt2, 5, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_STP_OFFSET_W(Rt, Rt2, Rn, imm7) \
	(0x29000000 | bits(imm7, 7, 15) | bits(Rt2, 5, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_STP_OFFSET_X(Rt, Rt2, Rn, imm7) \
	(0xa9000000 | bits(imm7, 7, 15) | bits(Rt2, 5, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_STR_POST_W(Rt, Rn, imm9) \
	(0xb8000400 | bits(imm9, 9, 12) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_STR_POST_X(Rt, Rn, imm9) \
	(0xf8000400 | bits(imm9, 9, 12) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_STR_PRE_W(Rt, Rn, imm9) \
	(0xb8000c00 | bits(imm9, 9, 12) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_STR_PRE_X(Rt, Rn, imm9) \
	(0xf8000c00 | bits(imm9, 9, 12) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_STR_OFFSET_W(Rt, Rn, imm12) \
	(0xb9000000 | bits(imm12, 12, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_STR_OFFSET_X(Rt, Rn, imm12) \
	(0xf9000000 | bits(imm12, 12, 10) | bits(Rn, 5, 5) | bits(Rt, 5, 0))
#define OP_SUBS_REG_W(Rd, Rn, Rm) \
	(0x6b000000 | bits(Rm, 5, 16) | bits(Rn, 5, 5) | bits(Rd, 5, 0))
#define OP_SUBS_REG_X(Rd, Rn, Rm) \
	(0xeb000000 | bits(Rm, 5, 16) | bits(Rn, 5, 5) | bits(Rd, 5, 0))
// TODO: OP_SUBS_SHIFTREG and _SXREG (don't want complexity of sign extending in all uses of maths ops)

static reg_t *assemble_prologue(reg_t *ip, int narg, struct operand *op)
{
	switch (op->type) {
	case REGISTER:
		*ip++ = OP_MOV_REG_W(ARG(narg), REG(op->value));
		break;
	case IMMEDIATE:
		*ip++ = OP_MOV_IMM_W(ARG(narg), op->value & 0xffff);
		if (op->value >> 16)
			*ip++ = OP_MOVK_W(ARG(narg),
					   (op->value >> 16) & 0xffff, 16);
		break;
	case ARGUMENT:
	case INVALID:
		assert(false);
		break;
	}

	return ip;
}

static reg_t *assemble_epilogue(reg_t *ip, struct operand *op)
{
	if (op->type == REGISTER)
		*ip++ = OP_MOV_REG_W(REG(op->value), ARG(0));

	return ip;
}

reg_t *assemble_word(reg_t *ip, struct command *word)
{
	int narg;

	assert(word->sym->type == FUNCPTR || word->sym->type == WORDPTR ||
	       word->sym->type == EXECPTR);

	for (narg = 0; narg < 4; narg++) {
		if (word->operand[narg].type == INVALID)
			break;

		ip = assemble_prologue(ip, narg, &word->operand[narg]);
	}

	reg_t absolute = word->sym->type == EXECPTR ?
				 word->sym->val :
				 (reg_t)(uintptr_t)word->sym->sym;
	reg_t offset = (absolute - (reg_t) (uintptr_t) ip) / 4;
	*ip++ = OP_BL(offset);

	ip = assemble_epilogue(ip, &word->operand[0]);

	return ip;
}

reg_t *assemble_ret(reg_t *ip)
{
	*ip++ = OP_RET(XLR);
	return ip;
}

reg_t *assemble_preamble(reg_t *ip, struct command *cmd, uint8_t clobbers)
{
	// add the arguments to the clobber list
	for (int i = 0; cmd && i < lengthof(cmd->operand) &&
			cmd->operand[i].type == REGISTER;
	     i++)
		clobbers |= 1 << cmd->operand[i].value;

	// save the state we are about to clobber
	for (int i = 0; i < 8; i++)
		if (clobbers & (1 << i))
			*ip++ = OP_STR_PRE_W(REG(i), XSP, -16);

	// push a frame record for the called function
	*ip++ = OP_STP_PRE_X(XFP, XLR, XSP, -2);
	*ip++ = OP_MOV_SP(XFP, XSP);

	// move the arguments into the right registers
	for (int i = 0; cmd && i < lengthof(cmd->operand) &&
			cmd->operand[i].type == REGISTER;
	     i++)
		*ip++ = OP_MOV_REG_W(REG(cmd->operand[i].value), ARG(i));

	return ip;
}

reg_t *assemble_postamble(reg_t *ip, struct command *cmd, uint8_t clobbers)
{
	// add the arguments to the clobber list
	for (int i = 0; cmd && i < lengthof(cmd->operand) &&
			cmd->operand[i].type == REGISTER;
	     i++)
		clobbers |= 1 << cmd->operand[i].value;

	// set the return value
	if (cmd && cmd->operand[0].type == REGISTER)
		*ip++ = OP_MOV_REG_W(ARG(0), REG(cmd->operand[0].value));

	// pop the frame record
	*ip++ = OP_LDP_POST_X(XFP, XLR, XSP, 2);

	// restore the saved registers
	for (int i = 7; i >= 0; i--)
		if (clobbers & (1 << i))
			*ip++ = OP_LDR_POST_W(REG(i), XSP, 16);

	return assemble_ret(ip);
}

reg_t translate_condition_code(reg_t code)
{
	switch (code) {
	case EQ:
		return C_EQ;
	case NE:
		return C_NE;
	case LT:
		return C_LT;
	case GT:
		return C_GT;
	case LTEQ:
		return C_LE;
	case GTEQ:
		return C_GE;
	case LTU:
		return C_LO;
	case GTU:
		return C_HI;
	case LTEU:
		return C_LS;
	case GTEU:
		return C_HS;
	}

	return C_AL;
}

reg_t *assemble_if(reg_t *ip, struct compare *cmp, reg_t **fixup)
{
	if (cmp->rel == CMPNZ) {
		*ip++ = OP_CMP_REG_W(REG(cmp->op1.value), WZR);
		*fixup = ip;
		*ip++ = OP_B_COND(C_EQ, 0);
	} else {
		*ip++ = OP_CMP_REG_W(REG(cmp->op1.value), REG(cmp->op2.value));
		*fixup = ip;
		*ip++ = OP_B_COND(translate_condition_code(cmp->rel)^1, 0);
	}

	return ip;
}

reg_t *assemble_else(reg_t *ip, reg_t **fixup)
{
	reg_t *oldip = ip;
	*ip++ = OP_B_COND(C_AL, 0);
	fixup_if(ip, *fixup);
	*fixup = oldip;

	return ip;

}

void fixup_if(reg_t *ip, reg_t *fixup)
{
	int offset = ip - fixup;
	*fixup |= bits(offset, 19, 5);
}

reg_t *assemble_while(reg_t *ip, struct compare *cmp, reg_t **fixup)
{
	return assemble_if(ip, cmp, fixup);
}

reg_t *assemble_endwhile(reg_t *ip, reg_t *fixup)
{
	*ip = OP_B(fixup - ip - 1);
	fixup_if(++ip, fixup);
	return ip;
}

void disassemble(FILE *f, reg_t *ip)
{
	fprintf(stderr, "TODO: Cannot disassemble yet\n");
}

static struct regset regs;

void exec(reg_t *ip)
{
	__asm__ __volatile__("mov	x27, %0\n\t"
			     "ldp	w19, w20, [x27, 0]\n\t"
			     "ldp	w21, w22, [x27, 8]\n\t"
			     "ldp	w23, w24, [x27, 16]\n\t"
			     "ldp	w25, w26, [x27, 24]\n\t"
			     "blr	%1\n\t"
			     "stp	w19, w20, [x27, 0]\n\t"
			     "stp	w21, w22, [x27, 8]\n\t"
			     "stp	w23, w24, [x27, 16]\n\t"
			     "stp	w25, w26, [x27, 24]\n\t"
			     :
			     : "r"(&regs), "r"(ip)
			     : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
			       "r8", "r9", "r10", "r11", "r12", "r13", "r14",
			       "r15", "r16", "r17", "r19", "r20", "r21",
			       "r22", "r23", "r24", "r25", "r26", "r27", "r30",
			       "cc", "memory");
}


/*!
 * \brief Get 'current' registers
 *
 * This is used by the dump opcode.
 *
 * \todo This implementation uses a potentially stale copy of the registers
 *       made when we called exec(). In practice this means we can only use
 *       the `dump` opcode from the top level (it won't work inside function)
 */
struct regset get_regs()
{
	assert(regs.zero == 0);
	return regs;
}

/*!
 * \brief Allow the caller to overrider the default stack pointer
 *
 * \todo Not implemented. We will crash if eigth code tries to take
 *       a pointer to stack allocated data (since stack is above 32-bit
 *       boundary). Happily at present this isn't supported...
 */
void set_sp(reg_t sp)
{
}
