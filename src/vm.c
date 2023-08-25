/* SPDX-License-Identifier: GPL-3.0-or-later */

/*!
 * \file vm.c
 * \brief Portable (and minimal) virtual machine
 *
 * The portable VM provides a very small set of opcodes that are sufficient
 * to make function calls and manage control flow. All other actions, up to
 * and including ALU operations, are handled by calling suitable
 * operations from the symbol table.
 *
 * The portable VM allows for testing on platforms where the code generator
 * is not supported. It can also be used as an alternative backend on
 * supported platforms to help test the code generator.
 */

#include "eigth.h"

#define F1SHIFT 28
#define F1MASK 0xf
#define F2SHIFT 24
#define F2MASK 0xf
#define F3SHIFT 8
/*
 * WARNING: the VM relies on casting through (int16_t) to sign extend F3... to
 *          change F3MASK then these need to be found and fixed!
 */
#define F3MASK 0xffff
#define F23SHIFT 8
#define F23MASK 0xfffff
#define OPMASK  0xff

#define F1DECODE(x) (x >> F1SHIFT)
#define F2DECODE(x) ((x >> F2SHIFT) & F2MASK)
#define F3DECODE(x) ((x >> F3SHIFT) & F3MASK)
#define F23DECODE(x) ((x >> F23SHIFT) & F23MASK)

#define ASM3(opcode, f1, f2, f3)                                   \
	((((f1)&F1MASK) << F1SHIFT) | (((f2)&F2MASK) << F2SHIFT) | \
	 (((f3)&F3MASK) << F3SHIFT) | (opcode))
#define ASM23(opcode, f1, f23) \
	((opcode) | (((f1)&F1MASK) << F1SHIFT) | (((f23)&F23MASK) << F23SHIFT))
#define ASM2(opcode, f1, f2) ASM3(opcode, f1, f2, 0)
#define ASM1(opcode, f1) ASM3(opcode, f1, 0, 0)

#define ARG(x) ((x) + 8)
#define RZERO 12

enum opcode {
	BEQ,
#define ASM_BEQ(a, b, offset) ASM3(BEQ, a, b, offset)
#define ASM_BEZ(a, offset) ASM3(BEQ, a, RZERO, offset)
#define ASM_B(offset) ASM3(BEQ, RZERO, RZERO, offset)
	BNE,
#define ASM_BNE(a, b, offset) ASM3(BNE, a, b, offset)
#define ASM_BNZ(a, offset) ASM3(BNE, a, RZERO, offset)
	BLT,
#define ASM_BLT(a, b, offset) ASM3(BLT, a, b, offset)
#define ASM_BGT(a, b, offset) ASM3(BLT, b, a, offset)
	BLTU,
#define ASM_BLTU(a, b, offset) ASM3(BLTU, a, b, offset)
#define ASM_BGTU(a, b, offset) ASM3(BLTU, b, a, offset)
	BGE,
#define ASM_BGE(a, b, offset) ASM3(BGE, a, b, offset)
#define ASM_BLE(a, b, offset) ASM3(BGE, b, a, offset)
	BGEU,
#define ASM_BGEU(a, b, offset) ASM3(BGEU, a, b, offset)
#define ASM_BLEU(a, b, offset) ASM3(BGEU, b, a, offset)
	CALL0,
#define ASM_CALL0() CALL0
	CALL1,
#define ASM_CALL1() CALL1
	CALL2,
#define ASM_CALL2() CALL2
	CALL3,
#define ASM_CALL3() CALL3
	CALL4,
#define ASM_CALL4() CALL4
	EXEC0,
#define ASM_EXEC0() EXEC0
	EXEC1,
#define ASM_EXEC1() EXEC1
	EXEC2,
#define ASM_EXEC2() EXEC2
	EXEC3,
#define ASM_EXEC3() EXEC3
	EXEC4,
#define ASM_EXEC4() EXEC4
	MOV,
#define ASM_MOV(dst, src) ASM2(MOV, dst, src)
	MOV16,
#define ASM_MOV16(dst, val) ASM23(MOV16, dst, val)
	MOVHI,
#define ASM_MOVHI(dst, val) ASM23(MOVHI, dst, val)
	POP,
#define ASM_POP(dst) ASM1(POP, dst)
	PUSH,
#define ASM_PUSH(src) ASM1(PUSH, src)
	RET,
#define ASM_RET() RET
};

static struct regset regs;

static reg_t *assemble_prologue(reg_t *ip, int narg, struct operand *op)
{
	switch (op->type) {
		case REGISTER:
			*ip++ = ASM_MOV(ARG(narg), op->value);
			break;
		case IMMEDIATE:
			*ip++ = ASM_MOV16(ARG(narg), op->value & 0xffff);
			if (op->value >> 16)
				*ip++ = ASM_MOVHI(ARG(narg),
						  (op->value >> 16) & 0xffff);
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
		*ip++ = ASM_MOV(op->value, ARG(0));

	return ip;
}

reg_t *assemble_word(reg_t *ip, struct command *word)
{
	int narg;

	assert(word->sym->type == FUNCPTR || word->sym->type == WORDPTR ||
	       word->sym->type == EXECPTR);

	for (narg=0; narg<4; narg++) {
		if (word->operand[narg].type == INVALID)
			break;

		ip = assemble_prologue(ip, narg, &word->operand[narg]);
	}

	switch (narg) {
	case 0:
		*ip++ = word->sym->type == EXECPTR ? ASM_EXEC0() : ASM_CALL0();
		break;
	case 1:
		*ip++ = word->sym->type == EXECPTR ? ASM_EXEC1() : ASM_CALL1();
		break;
	case 2:
		*ip++ = word->sym->type == EXECPTR ? ASM_EXEC2() : ASM_CALL2();
		break;
	case 3:
		*ip++ = word->sym->type == EXECPTR ? ASM_EXEC3() : ASM_CALL3();
		break;
	case 4:
		*ip++ = word->sym->type == EXECPTR ? ASM_EXEC4() : ASM_CALL4();
		break;
	default:
		assert(false);
	}
	*ip++ = word->sym->type == EXECPTR ? word->sym->val :
					     (reg_t)(uintptr_t)word->sym->sym;

	ip = assemble_epilogue(ip, &word->operand[0]);

	return ip;
}

reg_t *assemble_ret(reg_t *ip)
{
	*ip++ = ASM_RET();
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
			*ip++ = ASM_PUSH(i);

	// move the arguments into the right registers
	for (int i = 0; cmd && i < lengthof(cmd->operand) &&
			cmd->operand[i].type == REGISTER;
	     i++)
		*ip++ = ASM_MOV(cmd->operand[i].value, ARG(i));

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
		*ip++ = ASM_MOV(ARG(0), cmd->operand[0].value);

	// restore the saved registers
	for (int i = 7; i >= 0; i--)
		if (clobbers & (1 << i))
			*ip++ = ASM_POP(i);

	return assemble_ret(ip);
}

reg_t *assemble_if(reg_t *ip, struct compare *cmp, reg_t **fixup)
{
	*fixup = ip;
	switch (cmp->rel) {
	case EQ:
		*ip++ = ASM_BNE(cmp->op1.value, cmp->op2.value, 0);
		break;
	case NE:
		*ip++ = ASM_BEQ(cmp->op1.value, cmp->op2.value, 0);
		break;
	case LT:
		*ip++ = ASM_BGE(cmp->op1.value, cmp->op2.value, 0);
		break;
	case GT:
		*ip++ = ASM_BLT(cmp->op1.value, cmp->op2.value, 0);
		break;
	case LTEQ:
		*ip++ = ASM_BGT(cmp->op1.value, cmp->op2.value, 0);
		break;
	case GTEQ:
		*ip++ = ASM_BLT(cmp->op1.value, cmp->op2.value, 0);
		break;
	case LTU:
		*ip++ = ASM_BGEU(cmp->op1.value, cmp->op2.value, 0);
		break;
	case GTU:
		*ip++ = ASM_BLEU(cmp->op1.value, cmp->op2.value, 0);
		break;
	case LTEU:
		*ip++ = ASM_BGTU(cmp->op1.value, cmp->op2.value, 0);
		break;
	case GTEU:
		*ip++ = ASM_BLTU(cmp->op1.value, cmp->op2.value, 0);
		break;
	case CMPNZ:
		*ip++ = ASM_BEZ(cmp->op1.value, 0);
		break;
	}

	return ip;
}

reg_t *assemble_else(reg_t *ip, reg_t **fixup)
{
	reg_t *oldip = ip;
	*ip++ = ASM_B(0);
	fixup_if(ip, *fixup);
	*fixup = oldip;

	return ip;

}

void fixup_if(reg_t *ip, reg_t *fixup)
{
	int offset = ip - fixup - 1;
	*fixup |= offset << F3SHIFT;
}

reg_t *assemble_while(reg_t *ip, struct compare *cmp, reg_t **fixup)
{
	return assemble_if(ip, cmp, fixup);
}

reg_t *assemble_endwhile(reg_t *ip, reg_t *fixup)
{
	*ip = ASM_B(fixup - ip - 1);
	fixup_if(++ip, fixup);
	return ip;
}

static const char *regname(int r)
{
	switch (r) {
#define R(x) case x: return "r" #x
	R(0);
	R(1);
	R(2);
	R(3);
	R(4);
	R(5);
	R(6);
	R(7);
#undef R
#define A(x) case x+8: return "arg" #x
	A(0);
	A(1);
	A(2);
	A(3);
#undef A
	case 12:
		return "rZ";
	}

	return "INVALID";
}

static void trace_symbol(FILE *f, const char *op, reg_t arg)
{
	const char *name = symtab_name(arg);
	if (name)
		fprintf(f, "\t%s\t%s\n", op, name);
	else
		fprintf(f, "\t%s\t%p\n", op, (void *) (uintptr_t) arg);
}

static reg_t *trace(FILE *f, reg_t *ip)
{
	reg_t a, b;
	int16_t off;

	reg_t op = *ip++;
	switch (op & OPMASK) {
	case BEQ:
		a = F1DECODE(op);
		b = F2DECODE(op);
		off = F3DECODE(op);
		if (b == RZERO) {
			if (a == RZERO)
				fprintf(f, "\tb\t%d\n", off);
			else
				fprintf(f, "\tbez\t%s, %d\n", regname(a), off);
		} else {
			fprintf(f, "\tbeq\t%s, %s, %d\n", regname(a),
				regname(b), off);
		}
		break;
	case BNE:
		a = F1DECODE(op);
		b = F2DECODE(op);
		off = F3DECODE(op);
		if (a == RZERO) {
			fprintf(f, "\tbnz\t%d, %d\n", a, off);
		} else {
			fprintf(f, "\tbne\t%d, %d, %d\n", a, b, off);
		}
		break;
	case BLT:
	case BLTU:
	case BGE:
	case BGEU:
		a = F1DECODE(op);
		b = F2DECODE(op);
		off = F3DECODE(op);
		fprintf(f, "\t%s\t%d, %d, %d\n",
			(op & OPMASK) == BLT  ? "blt" :
			(op & OPMASK) == BLTU ? "bltu" :
			(op & OPMASK) == BGE  ? "bge" :
						"bgeu",
			a, b, off);
		break;
	case CALL0:
		trace_symbol(f, "call0", *ip++);
		break;
	case CALL1:
		trace_symbol(f, "call1", *ip++);
		break;
	case CALL2:
		trace_symbol(f, "call2", *ip++);
		break;
	case CALL3:
		trace_symbol(f, "call3", *ip++);
		break;
	case CALL4:
		trace_symbol(f, "call4", *ip++);
		break;
	case EXEC0:
		trace_symbol(f, "exec0", *ip++);
		break;
	case EXEC1:
		trace_symbol(f, "exec1", *ip++);
		break;
	case EXEC2:
		trace_symbol(f, "exec2", *ip++);
		break;
	case EXEC3:
		trace_symbol(f, "exec3", *ip++);
		break;
	case EXEC4:
		trace_symbol(f, "exec4", *ip++);
		break;
	case MOV:
		fprintf(f, "\tmov\t%s, %s\n", regname(F1DECODE(op)),
			regname(F2DECODE(op)));
		break;
	case MOV16:
		fprintf(f, "\tmov16\t%s, %d\n", regname(F1DECODE(op)),
			F23DECODE(op));
		break;
	case MOVHI:
		fprintf(f, "\tmovhi\t%s, %d\n", regname(F1DECODE(op)),
			F23DECODE(op));
		break;
	case POP:
		fprintf(f, "\tpop\t%s\n", regname(F1DECODE(op)));
		break;
	case PUSH:
		fprintf(f, "\tpush\t%s\n", regname(F1DECODE(op)));
		break;
	case RET:
		fprintf(f, "\tret\n");
		return NULL;
	}

	return ip;
}

void disassemble(FILE *f, reg_t *ip)
{
	while (ip)
		ip = trace(f, ip);
}

void exec(reg_t *ip)
{
	reg_t fn;
	reg_t *sp;

	while (true) {
		//fprintf(stderr, "%p: ", ip);
		//(void) trace(stderr, ip);
		reg_t op = *ip++;
		switch (op & OPMASK) {
		case BEQ:
			if (regs.r[F1DECODE(op)] == regs.r[F2DECODE(op)])
				ip += (int16_t) F3DECODE(op);
			break;
		case BNE:
			if (regs.r[F1DECODE(op)] != regs.r[F2DECODE(op)])
				ip += (int16_t) F3DECODE(op);
			break;
		case BLT:
			if ((int)regs.r[F1DECODE(op)] <
			    (int)regs.r[F2DECODE(op)])
				ip += (int16_t) F3DECODE(op);
			break;
		case BLTU:
			if (regs.r[F1DECODE(op)] < regs.r[F2DECODE(op)])
				ip += (int16_t) F3DECODE(op);
			break;
		case BGE:
			if ((int)regs.r[F1DECODE(op)] >=
			    (int)regs.r[F2DECODE(op)])
				ip += (int16_t) F3DECODE(op);
			break;
		case BGEU:
			if (regs.r[F1DECODE(op)] >= regs.r[F2DECODE(op)])
				ip += (int16_t) F3DECODE(op);
			break;
		case CALL0:
			fn = *ip++;
			regs.arg[0] = ((reg_t(*)(void))(uintptr_t)fn)();
			break;
		case CALL1:
			fn = *ip++;
			regs.arg[0] =
				((reg_t(*)(reg_t))(uintptr_t)fn)(regs.arg[0]);
			break;
		case CALL2:
			fn = *ip++;
			regs.arg[0] = ((reg_t(*)(reg_t, reg_t))(uintptr_t)fn)(
				regs.arg[0], regs.arg[1]);
			break;
		case CALL3:
			fn = *ip++;
			regs.arg[0] =
				((reg_t(*)(reg_t, reg_t, reg_t))(uintptr_t)fn)(
					regs.arg[0], regs.arg[1], regs.arg[2]);
			break;
		case CALL4:
			fn = *ip++;
			regs.arg[0] = ((reg_t(*)(reg_t, reg_t, reg_t, reg_t))(
				uintptr_t)fn)(regs.arg[0], regs.arg[1],
					      regs.arg[2], regs.arg[3]);
			break;
		case EXEC0:
		case EXEC1:
		case EXEC2:
		case EXEC3:
		case EXEC4:
			exec((reg_t *) (uintptr_t) (*ip++));
			break;
		case MOV:
			regs.r[F1DECODE(op)] = regs.r[F2DECODE(op)];
			break;
		case MOV16:
			regs.r[F1DECODE(op)] = F23DECODE(op);
			break;
		case MOVHI:
			regs.r[F1DECODE(op)] |= (F23DECODE(op) << 16);
			break;
		case POP:
			sp = (reg_t *) (uintptr_t) regs.sp;
			regs.r[F1DECODE(op)] = *sp++;
			regs.sp = (reg_t) (uintptr_t) sp;
			break;
		case PUSH:
			sp = (reg_t *) (uintptr_t) regs.sp;
			*--sp = regs.r[F1DECODE(op)];
			regs.sp = (reg_t) (uintptr_t) sp;
			break;
		case RET:
			return;
		}
	}
}

struct regset get_regs()
{
	assert(regs.zero == 0);
	return regs;
}

void set_sp(reg_t sp)
{
	regs.sp = sp;
}
