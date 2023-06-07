/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "eigth.h"

static reg_t add(reg_t _, reg_t a, reg_t b)
{
	return a + b;
}

static reg_t x_alloc(reg_t _, reg_t sz)
{
	return (reg_t) (uintptr_t) alloc(sz);
}

static reg_t and(reg_t _, reg_t a, reg_t b)
{
	return a & b;
}

static reg_t x_assert(reg_t a, reg_t b)
{
	if (a != b)
		die("Assertion failed: 0x%x != 0x%x", a, b);

	return a;
}

static reg_t define(void)
{
	parse_define();
	return 0;
}

static reg_t x_disassemble(void)
{
	symtab_disassemble();
	return 0;
}

static reg_t dump(void)
{
	dbg_regs(stdout);
	return 0;
}

static reg_t x_exit(reg_t a)
{
	exit(a);
}

static reg_t hex(reg_t a)
{
	printf("%x\n", a);

	return a;
}

static reg_t x_if(reg_t cond)
{
	parse_if();
	return cond;
}

static reg_t mov(reg_t _, reg_t a)
{
	return a;
}

static reg_t or(reg_t _, reg_t a, reg_t b)
{
	return a | b;
}

static reg_t print(reg_t a)
{
	printf("%d\n", a);

	return a;
}

static reg_t x_putc(reg_t a)
{
	putchar(a);

	return a;
}

static reg_t shl(reg_t _, reg_t a, reg_t b)
{
	return a << b;
}

static reg_t shr(reg_t _, reg_t a, reg_t b)
{
	reg_t sb = (a >> 31) & 1;
	reg_t partial = (~(1u << 31) & a) >> b;


	return partial | (sb << (31 - b));
}

static reg_t shra(reg_t _, reg_t a, reg_t b)
{
	reg_t sb = ((a >> 31) & 1) * -1u;
	reg_t partial = (~(1u << 31) & a) >> b;

	return partial | (sb << (31 - b));
}

static reg_t sub(reg_t _, reg_t a, reg_t b)
{
	return a - b;
}

static reg_t x_while(reg_t cond)
{
	parse_while();
	return cond;
}

static reg_t words(void)
{
	symtab_list(stdout);
	return 0;
}

static reg_t xor(reg_t _, reg_t a, reg_t b)
{
	return a ^ b;
}

void register_ops(void)
{
#define FN(x)                                               \
	do {                                                \
		static struct symbol s = { .name = #x,      \
					   .type = FUNCPTR, \
					   { .sym = &x } }; \
		symtab_add(&s);                             \
	} while (0)
#define FX(x)                                                   \
	do {                                                    \
		static struct symbol s = { .name = #x,          \
					   .type = FUNCPTR,     \
					   { .sym = &x_##x } }; \
		symtab_add(&s);                                 \
	} while (0)
#define IMM (symtab_latest()->type = WORDPTR)

	FN(add);
	FX(alloc);
	FX(assert);
	FN(and);
	FN(define); IMM;
	FX(disassemble); IMM;
	FN(dump);
	FX(exit);
	FN(hex);
	FX(if); IMM;
	FN(mov);
	FN(or);
	FN(print);
	FX(putc);
	FN(shl);
	FN(shr);
	FN(shra);
	FN(sub);
	FX(while); IMM;
	FN(words);
	FN(xor);

#undef FN
#undef FX
#undef IMM
}
