/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "eigth.h"

#define DBGI_ENUM(e)            \
	case e:                 \
		fprintf(f, #e); \
		break;

#define DBGI_ARRAY_PTR(name, fp, nelem, array)      \
	do {                                        \
		fprintf(fp, "[ ");                  \
		if (nelem)                          \
			dbgi_##name(fp, &array[0]); \
		for (int i = 1; i < nelem; i++) {   \
			fprintf(f, ", ");           \
			dbgi_##name(fp, &array[i]); \
		}                                   \
		fprintf(fp, " ]");                  \
	} while (0)

#define DBGI_ARRAY(name, fp, nelem, array)         \
	do {                                       \
		fprintf(fp, "[ ");                 \
		if (nelem)                         \
			dbgi_##name(fp, array[0]); \
		for (int i = 1; i < nelem; i++) {  \
			fprintf(f, ", ");          \
			dbgi_##name(fp, array[i]); \
		}                                  \
		fprintf(fp, " ]");                 \
	} while (0)

#define DBG(name, fp, ...)                    \
	do {                                  \
		dbgi_##name(fp, __VA_ARGS__); \
		fprintf(fp, "\n");            \
	} while (0)

static void dbgi_optype(FILE *f, enum optype t)
{
	switch (t) {
	DBGI_ENUM(INVALID);
	DBGI_ENUM(REGISTER);
	DBGI_ENUM(ARGUMENT);
	DBGI_ENUM(IMMEDIATE);
	default:
		fprintf(f, "bad-pattern:%d", t);
	};
}

void dbg_optype(FILE *f, enum optype t)
{
	DBG(optype, f, t);
}

static void dbgi_operand(FILE *f, struct operand *op)
{
	fprintf(f, "{ ");
	dbgi_optype(f, op->type);
	fprintf(f, ", %u }", op->value);
}

void dbg_operand(FILE *f, struct operand *op)
{
	DBG(operand, f, op);
}

static void dbgi_operand_array(FILE *f, int nelem, struct operand *ops)
{
	DBGI_ARRAY_PTR(operand, f, nelem, ops);
}

void dbg_operand_array(FILE *f, int nelem, struct operand *ops)
{
	DBG(operand_array, f, nelem, ops);
}

static void dbgi_symtype(FILE *f, enum symtype t)
{
	switch (t) {
	DBGI_ENUM(FUNCPTR);
	DBGI_ENUM(EXECPTR);
	DBGI_ENUM(VARIABLE);
	DBGI_ENUM(CONSTANT);
	default:
		fprintf(f, "bad-pattern:%d", t);
	};
}

void dbg_symtype(FILE *f, enum symtype t)
{
	DBG(symtype, f, t);
}

static void dbgi_symbol(FILE *f, struct symbol *s)
{
	fprintf(f, "{ \"%s\", ", s->name);
	dbgi_symtype(f, s->type);
	fprintf(f, ", 0x%x, %p }", s->val, s->next);

}

void dbg_symbol(FILE *f, struct symbol *s)
{
	DBG(symbol, f, s);
}

static void dbgi_command(FILE *f, struct command *c)
{
	fprintf(f, "{ %s, ", c->opcode);
	dbgi_symbol(f, c->sym);
	fprintf(f, ", ");
	dbgi_operand_array(f, 4, c->operand);
	fprintf(f, " }");
}

void dbg_command(FILE *f, struct command *c)
{
	DBG(command, f, c);
}


static void dbgi_reg(FILE *f, reg_t reg)
{
	fprintf(f, "%u", reg);
}

void dbg_reg(FILE *f, reg_t reg)
{
	DBG(reg, f, reg);
}

static void dbgi_reg_array(FILE *f, int nelem, reg_t *regs)
{
	DBGI_ARRAY(reg, f, nelem, regs);
}

void dbg_reg_array(FILE *f, int nelem, reg_t *regs)
{
	DBG(reg_array, f, nelem, regs);
}

static void dbgi_regset(FILE *f, struct regset *regs)
{
	fprintf(f, "{ ");
	dbgi_reg_array(f, 8, regs->r);
	fprintf(f, ", ");
	dbgi_reg_array(f, 4, regs->arg);
	fprintf(f, ", ");
	dbgi_reg(f, regs->sp);
	fprintf(f, " }");
}

void dbg_regset(FILE *f, struct regset *regs)
{
	DBG(regset, f, regs);
}

void dbg_regs(FILE *f)
{
	struct regset regs = get_regs();
	dbg_regset(f, &regs);
}
