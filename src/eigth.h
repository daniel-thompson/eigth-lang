/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef EIGTH_LANG_EIGTH_H_
#define EIGTH_LANG_EIGTH_H_

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


#define lengthof(x) ((sizeof(x) / sizeof(*(x))))

typedef uint32_t reg_t;

struct regset {
	reg_t r[8];
	/*
	 * WARNING: the VM relies that r[9] == arg[0] (in other words we
	 *          knowingly read/write pass the end of the r array to
	 *          handle arguments).
	 */
	reg_t arg[4];
	reg_t zero;

	reg_t sp;
};

enum optype {
	INVALID,
	REGISTER,
	ARGUMENT,
	IMMEDIATE,
};

struct operand {
	enum optype type;
	reg_t value;
};

enum symtype {
	FUNCPTR,
	WORDPTR,
	EXECPTR,
	VARIABLE,
	CONSTANT
};

enum relop {
	CMPNZ, /* compare non-zero */
	EQ,
	NE,
	LT,
	GT,
	LTEQ,
	GTEQ,
	LTU,
	GTU,
	LTEU,
	GTEU
};

struct compare {
	struct operand op1;
	enum relop rel;
	struct operand op2;
};

struct symbol {
	const char *name;
	enum symtype type;
	union {
		reg_t (*sym)();
		reg_t val;
	};
	struct symbol *next;
};

struct command {
	char opcode[32];
	struct symbol *sym;
	struct operand operand[4];
};

static inline void clear_cache(reg_t begin, reg_t end) {
	__builtin___clear_cache((char *)(uintptr_t)begin,
				(char *)(uintptr_t)end);
}

reg_t *assemble_word(reg_t *ip, struct command *cmd);
reg_t *assemble_ret(reg_t *ip);
reg_t *assemble_preamble(reg_t *ip, struct command *cmd, uint8_t clobbers);
reg_t *assemble_postamble(reg_t *ip, struct command *cmd, uint8_t clobbers);
reg_t *assemble_if(reg_t *ip, struct compare *cmp, reg_t **fixup);
reg_t *assemble_else(reg_t *ip, reg_t **fixup);
reg_t *assemble_while(reg_t *ip, struct compare *cmp, reg_t **fixup);
reg_t *assemble_endwhile(reg_t *ip, reg_t *fixup);
void fixup_if(reg_t *ip, reg_t *fixup);
void disassemble(FILE *f, reg_t *ip);
void exec(reg_t *ip);
struct regset get_regs(void);
void set_sp(reg_t sp);

void register_ops(void);

void *alloc(size_t sz);
void die(const char *fmt, ...);
void parse_define(void);
void parse_if(void);
void parse_while(void);
void symtab_add(struct symbol *s);
void symtab_define(void);
void symtab_disassemble(void);
struct symbol *symtab_latest(void);
void symtab_list(FILE *f);
struct symbol *symtab_lookup(const char *name);
const char *symtab_name(reg_t addr);

void dbg_optype(FILE *f, enum optype t);
void dbg_operand(FILE *f, struct operand *op);
void dbg_command(FILE *f, struct command *c);
void dbg_reg(FILE *f, reg_t reg);
void dbg_reg_array(FILE *f, int nelem, reg_t *regs);
void dbg_regset(FILE *f, struct regset *regs);
void dbg_regs(FILE *f);

#endif /* EIGTH_LANG_EIGTH_H_ */
