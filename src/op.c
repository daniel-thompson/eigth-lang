/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "eigth.h"

static reg_t op_add(reg_t _, reg_t a, reg_t b)
{
	return a + b;
}

static reg_t op_alloc(reg_t _, reg_t sz)
{
	return (reg_t) (uintptr_t) alloc(sz);
}

static reg_t op_and(reg_t _, reg_t a, reg_t b)
{
	return a & b;
}

static reg_t op_assert(reg_t a, reg_t b)
{
	if (a != b)
		die("Assertion failed: 0x%x != 0x%x", a, b);

	return a;
}


static reg_t op_define(void)
{
	parse_define();
	return 0;
}

static reg_t op_disassemble(void)
{
	symtab_disassemble();
	return 0;
}

static reg_t op_div(reg_t _, reg_t a, reg_t b)
{
	return (sreg_t) a / (sreg_t) b;
}

static reg_t op_dump(void)
{
	dbg_regs(stdout);
	return 0;
}

static reg_t op_exit(reg_t a)
{
	exit(a);
}

static reg_t op_hex(reg_t a)
{
	printf("%x\n", a);

	return a;
}

static reg_t op_if(reg_t cond)
{
	parse_if();
	return cond;
}

static reg_t op_mov(reg_t _, reg_t a)
{
	return a;
}

static reg_t op_mul(reg_t _, reg_t a, reg_t b)
{
	return (sreg_t) a * (sreg_t) b;
}

static reg_t op_or(reg_t _, reg_t a, reg_t b)
{
	return a | b;
}

static reg_t op_print(reg_t a)
{
	printf("%d\n", a);

	return a;
}

static reg_t op_putc(reg_t a)
{
	putchar(a);

	return a;
}

static reg_t op_puts(reg_t a)
{
	puts((char *) (uintptr_t) a);

	return a;
}

static reg_t op_shl(reg_t _, reg_t a, reg_t b)
{
	return a << b;
}

static reg_t op_shr(reg_t _, reg_t a, reg_t b)
{
	reg_t sb = (a >> 31) & 1;
	reg_t partial = (~(1u << 31) & a) >> b;


	return partial | (sb << (31 - b));
}

static reg_t op_shra(reg_t _, reg_t a, reg_t b)
{
	reg_t sb = ((a >> 31) & 1) * -1u;
	reg_t partial = (~(1u << 31) & a) >> b;

	return partial | (sb << (31 - b));
}

static reg_t op_sub(reg_t _, reg_t a, reg_t b)
{
	return a - b;
}

static reg_t op_var(void)
{
	parse_var();
	return 0;
}

static reg_t op_while(reg_t cond)
{
	parse_while();
	return cond;
}

static reg_t op_words(void)
{
	symtab_list(stdout);
	return 0;
}

static reg_t op_xor(reg_t _, reg_t a, reg_t b)
{
	return a ^ b;
}

void register_ops(void)
{
#define OP(x)                                                   \
	do {                                                    \
		static struct symbol s = { .name = #x,          \
					   .type = FUNCPTR,     \
					   { .sym = &op_##x } }; \
		symtab_add(&s);                                 \
	} while (0)
#define IMM (symtab_latest()->type = WORDPTR)

	OP(add);
	OP(alloc);
	OP(assert);
	OP(and);
	OP(define); IMM;
	OP(disassemble); IMM;
	OP(div);
	OP(dump);
	OP(exit);
	OP(hex);
	OP(if); IMM;
	OP(mov);
	OP(mul);
	OP(or);
	OP(print);
	OP(putc);
	OP(puts);
	OP(shl);
	OP(shr);
	OP(shra);
	OP(sub);
	OP(us);
	OP(var); IMM;
	OP(while); IMM;
	OP(words);
	OP(xor);

#undef OP
#undef IMM
}
