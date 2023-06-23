/* SPDX-License-Identifier: GPL-3.0-or-later */

/*
 * This file contains Linux-specific mmap flags that are disabled
 * due to the -std=c99 we use for the rest of the project.
 */
#define _DEFAULT_SOURCE

#include "eigth.h"
#include <time.h>
#include <sys/mman.h>

enum delimiter {
	END,
	ELSE
};

static reg_t *memp;
static reg_t *ip;

//
// Out-of-band area must contain space for the canary and either:
//
//  * a worst-case four argument call
//    - VM: 44 = 2 * reg2reg, 3 * imm2reg, call, ret
//  * a 9-deep stack of immediate calls
//    - VM:  108 = call, ret
//
static reg_t *oob; // out-of-band exec area
static reg_t *ooip;
#define OOB_AREA 32
#define SET_OOB_CANARY() (oob[OOB_AREA - 1] = 0xc0ffee)
#define CHECK_OOB_CANARY() assert(oob[OOB_AREA - 1] == 0xc0ffee)

static struct symbol *globals = NULL;

const static unsigned int memsz = 4 * 1024 * 1024;

void *alloc(size_t sz)
{
	reg_t *p = (reg_t *) (uintptr_t) memp;
	reg_t *q = p + (sz + sizeof(reg_t) - 1) / sizeof(reg_t);
	memp = /*(reg_t) ( uintptr_t)*/ q;

	return p;
}

static reg_t* alloc_memp()
{
	void *p = mmap((void *)0x10000000, memsz,
		       PROT_EXEC | PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1,
		       0);
	if (!p)
		die("Cannot allocated core memory\n");

	return /*(reg_t) (uintptr_t)*/ p;
}

void die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\n");
	fflush(stderr);

	exit(1);
}

static bool is_whitespace(int c)
{
	switch (c) {
	case ' ':
	case '\t':
	case ',':
		return true;
	}

	return false;
}

static bool is_seperator(int c)
{
	switch (c) {
	case ' ':
	case '\t':
	case ',':
	case '\n':
	case EOF:
		return true;
	}

	return false;
}

static bool starts_with(const char *s, const char *prefix)
{
	if (!s)
		return false;

	size_t l = strlen(prefix);
	return strncmp(s, prefix, l) == 0;
}

static void skip_until_newline(void)
{
	int c;

	do {
		c = getchar();
		if (c == EOF)
			die("Unexpected end of file");
	} while (c != '\n');
}

static void skip_whitespace(void) {
	int c;

	do {
		c = getchar();
		if (c == EOF)
			die("Unexpected end of file");
	} while(is_whitespace(c));

	if (c == '#') {
		skip_until_newline();
		ungetc('\n', stdin);
	} else {
		ungetc(c, stdin);
	}
}

static char *token(char *p, size_t sz)
{

	int c;
	char *q = p;
	char *r = q + sz - 1;

	skip_whitespace();

	while (!is_seperator((c = getchar()))) {
		if (q == r)
			break;

		*q++ = c;
	}

	*q = '\0';
	if (c == EOF)
		die("Unexpected end of file");
	ungetc(c, stdin);

	return p[0] ? p : NULL;
}

static reg_t parse_number(char *p)
{
	char *q;
	long long n = strtoll(p, &q, 0);
	if (p == q) {
		return (reg_t) -1;
	}

	return (reg_t) n;
}

static struct operand parse_operand(char *p)
{
	struct operand op = { INVALID };

	if (!p)
		return op;

	if (starts_with(p, "r")) {
		reg_t n = parse_number(p+1);
		if (n < 8) {
			op.type = REGISTER;
			op.value = n;
		}
	} else if (starts_with(p, "arg")) {
		reg_t n = parse_number(p+3);
		if (n < 4) {
			op.type = ARGUMENT;
			op.value = n;
		}
	} else if (isdigit(p[0])) {
		op.type = IMMEDIATE;
		op.value = parse_number(p);
	}

	return op;
}

// Consuming symbols until we reach the end
void parse_error(void)
{
	assert(0);
}

static void parse_operands(struct operand *operand, int n)
{
	char *t;
	char buf[32];

	for (int i=0; i<n; i++) {
		t = token(buf, sizeof(buf));
		if (t)
			operand[i] = parse_operand(t);
	}
}

static struct command parse_command(void) {
	struct command cmd = { 0 };
	char *t;

	t = token(cmd.opcode, sizeof(cmd.opcode));
	if (!t) {
		switch (getchar()) {
		case '\n':
			return parse_command();
		case EOF:
			exit(0);
		default:
			assert(false);
		}
	}

	/* early exit for immediate words */
	cmd.sym = symtab_lookup(cmd.opcode);
	if (cmd.sym && cmd.sym->type == WORDPTR)
		return cmd;

	parse_operands(cmd.operand, lengthof(cmd.operand));
	skip_whitespace();

	/* final consistency check */
	switch(getchar()) {
		case '\n':
			break;
		case EOF:
			die("Unexpected end of file");
		default:
			fprintf(stderr, "Bad command\n");
			skip_until_newline();
			return parse_command();
	}

	return cmd;
}

static uint8_t get_clobbers(struct command *cmd)
{
	uint8_t clobbers = 0;

	for (int i = 0;
	     i < lengthof(cmd->operand) && cmd->operand[i].type == REGISTER;
	     i++) {
		clobbers |= 1 << cmd->operand[i].value;
	}

	return clobbers;
}

enum delimiter parse_block(void)
{
	while (true) {
		struct command c = parse_command();
		if (!c.sym) {
			if (0 == strcmp(c.opcode, "end"))
				return END;
			else if (0 == strcmp(c.opcode, "else"))
				return ELSE;

			parse_error();
			return END;
		}

		if (c.sym->type == WORDPTR) {
			// execute the word immediately
			reg_t *word = ooip;
			ooip = assemble_word(ooip, &c);
			ooip = assemble_ret(ooip);
			CHECK_OOB_CANARY();
			exec(word);
		} else {
			ip = assemble_word(ip, &c);
		}
	}
}

void parse_define(void)
{
	// Get the name of the function
	struct command cmd = parse_command();

	// Read the clobbers
	uint8_t clobbers = 0;
	struct command use;
	do {
		use = parse_command();
		if (0 == strcmp(use.opcode, "use"))
			clobbers |= get_clobbers(&use);
	} while (0 != strcmp(use.opcode, "begin"));

	reg_t *p = /*(reg_t *)(uintptr_t)*/memp;
	ip = p;

	ip = assemble_preamble(ip, &cmd, clobbers);
	(void) parse_block();
	ip = assemble_postamble(ip, &cmd, clobbers);

	// allocate the space for the freshly assembled function!
	memp = /*(reg_t) (uintptr_t)*/ ip;

	size_t namelen = strlen(cmd.opcode) + 1;
	char *name = alloc(namelen);
	memcpy(name, cmd.opcode, namelen);

	struct symbol *s = alloc(sizeof(struct symbol));
	s->name = name;
	s->type = EXECPTR;
	s->val = (reg_t) (uintptr_t) p;
	symtab_add(s);
}

enum relop parse_relop(const char *t)
{
	if (!t)
		return CMPNZ;

	if (0 == strcmp(t, "=="))
		return EQ;
	else if (0 == strcmp(t, "!="))
		return NE;
	else if (0 == strcmp(t, "<"))
		return LT;
	else if (0 == strcmp(t, ">"))
		return GT;
	else if (0 == strcmp(t, "<="))
		return LTEQ;
	else if (0 == strcmp(t, ">="))
		return GTEQ;
	else if (0 == strcmp(t, "u<"))
		return LTU;
	else if (0 == strcmp(t, "u>"))
		return GTU;
	else if (0 == strcmp(t, "u<="))
		return LTEU;
	else if (0 == strcmp(t, "u>="))
		return GTEU;

	return CMPNZ;
}

struct compare parse_comparison(void)
{
	char buf[32];
	struct compare cmp = {
		.op1 = parse_operand(token(buf, sizeof(buf))),
		.rel = parse_relop(token(buf, sizeof(buf))),
		.op2 = parse_operand(token(buf, sizeof(buf)))
	};

	if (cmp.op2.type != REGISTER && cmp.rel != CMPNZ)
		cmp.op1.type = INVALID;
	return cmp;
}

void parse_if(void)
{
	struct compare cmp = parse_comparison();
	if (cmp.op1.type != REGISTER)
		return parse_error();

	reg_t *fixme;

	ip = assemble_if(ip, &cmp, &fixme);
	enum delimiter delim = parse_block();
	if (delim == ELSE) {
		ip = assemble_else(ip, &fixme);
		(void) parse_block();
	}
	fixup_if(ip, fixme);

}

void parse_var(void)
{
	// this is a sneaky trick to capture a name and a number
	struct command cmd = parse_command();
	dbg_command(stderr, &cmd);
}

void parse_while(void)
{

	struct compare cmp = parse_comparison();
	if (cmp.op1.type != REGISTER)
		return parse_error();

	reg_t *fixme;

	ip = assemble_while(ip, &cmp, &fixme);
	(void) parse_block();
	ip = assemble_endwhile(ip, fixme);
}

void symtab_add(struct symbol *s)
{
	s->next = globals;
	globals = s;
}

static struct symbol *symtab_before(struct symbol *s)
{
	struct symbol *prev, *next = NULL;

	for (prev = next, next = globals;
	     next;
	     prev = next, next = next->next)
		if (next == s)
			return prev;

	return s ? NULL : prev;
}

void symtab_disassemble(void)
{
	struct command c = parse_command();
	struct symbol *s = c.sym;
	if (s && s->type == EXECPTR)
		disassemble(stdout, (reg_t *) (uintptr_t) s->val);
	else
		printf("No symbol found\n");
}

struct symbol *symtab_latest(void)
{
	return globals;
}

void symtab_list(FILE *f)
{
	// traverse the symbol list backwards...
	for (struct symbol *s = symtab_before(NULL); s; s = symtab_before(s))
		fprintf(f, "    %s\n", s->name);
}

struct symbol *symtab_lookup(const char *name)
{
	for (struct symbol *s = globals; s; s = s->next)
		if (0 == strcmp(name, s->name))
			return s;

	return NULL;
}

const char *symtab_name(reg_t addr)
{
	void *p = (void *) (uintptr_t) addr;

	for (struct symbol *s = globals; s; s = s->next)
		if (s->sym == p || s->val == addr)
			return s->name;

	return NULL;
}

reg_t op_us(reg_t _)
{
	struct timespec tv;

	int res = clock_gettime(CLOCK_MONOTONIC, &tv);
	assert(0 == res);

	return (reg_t) ((tv.tv_nsec / 1000) + 1000000 * tv.tv_sec);
}

int main(int argc, char *argv[])
{
	int c;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	memp = alloc_memp();
	set_sp(((reg_t) (uintptr_t) memp) + memsz);

	oob = memp;
	memp += OOB_AREA;
	SET_OOB_CANARY();

	register_ops();

	while ((c = getchar()) != EOF) {
		ungetc(c, stdin);

		struct command cmd = parse_command();
		if (cmd.sym) {
			ooip = assemble_word(oob, &cmd);
			ooip = assemble_ret(ooip);

			CHECK_OOB_CANARY();
			exec(oob);
		} else {
			fprintf(stderr, "Bad symbol\n");
		}
	}

	return 0;
}
