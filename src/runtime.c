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

static inline void sync_caches(void *begin, void *end)
{
	__builtin___clear_cache((char *) 0x04000000, (char *) 0x04000000 + memsz);
}

void *alloc(size_t sz)
{
	reg_t *p = (reg_t *) (uintptr_t) memp;
	reg_t *q = p + (sz + sizeof(reg_t) - 1) / sizeof(reg_t);
	memp = /*(reg_t) ( uintptr_t)*/ q;

	return p;
}

static reg_t* alloc_memp()
{
	void *p = mmap((void *)0x04000000, memsz,
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

	c = getchar();
	if (c == '"') {
		while ((c = getchar()) != '"') {
			if (q == r)
				q--;

			*q++ = c;

			// handle escapes
			if (c == '\\') {
				switch (c = getchar()) {
				case '"':
					q[-1] = '"';
					break;
				default:
					ungetc(c, stdin);
				}
			}
		}

		// pull a character so we have something to ungetc()
		c = getchar();
	} else if (!is_seperator(c)) {
		do {
			if (q == r)
				q--;

			*q++ = c;
		} while (!is_seperator(c = getchar()));
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
	} else if (starts_with(p, "'") && p[1] != '\0' && p[2] == '\'') {
		op.type = IMMEDIATE;
		op.value = p[1];
	} else if (starts_with(p, "'\\") && p[2] != '\0' && p[3] == '\'') {
		op.type = IMMEDIATE;
		switch (p[2]) {
		case 'n':
			op.value = '\n';
			break;
		case 'r':
			op.value = '\r';
			break;
		case 't':
			op.value = '\t';
			break;
		default:
			op.value = p[2];
		}
	} else if (isdigit(p[0])) {
		op.type = IMMEDIATE;
		op.value = parse_number(p);
	} else {
		struct symbol *sym = symtab_lookup(p);
		if (sym && sym->type == CONSTANT) {
			op.type = IMMEDIATE;
			op.value = sym->val;
		}
	}

	return op;
}

// Consuming symbols until we reach the end
void parse_error(void)
{
	fprintf(stderr, "Parse error - aborting\n");
	exit(1);
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
			ooip = assemble_preamble(ooip, NULL, 0);
			ooip = assemble_word(ooip, &c);
			ooip = assemble_postamble(ooip, NULL, 0);

			CHECK_OOB_CANARY();
			sync_caches(word, ooip);
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
	sync_caches(p, ip);

	// allocate the space for the freshly assembled function!
	memp = /*(reg_t) (uintptr_t)*/ ip;

	(void) symtab_new(cmd.opcode, EXECPTR, (reg_t) (uintptr_t) p);
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

void parse_const_if(reg_t condition)
{
	reg_t *oip = ip;

	enum delimiter delim = parse_block();
	if (condition && delim == ELSE) {
		oip = ip;
		(void) parse_block();
		ip = oip;
	} else if (delim == ELSE) {
		ip = oip;
		(void) parse_block();
	} else if (!condition) {
		// rewind everything (this is an `if 0` used to comment out
		// a block of code)
		ip = oip;
	}

}

void parse_if(void)
{
	struct compare cmp = parse_comparison();
	if (cmp.op1.type == IMMEDIATE) {
		return parse_const_if(cmp.op1.value);
	} else if (cmp.op1.type != REGISTER) {
		return parse_error();
	}

	reg_t *fixme;

	ip = assemble_if(ip, &cmp, &fixme);
	enum delimiter delim = parse_block();
	if (delim == ELSE) {
		ip = assemble_else(ip, &fixme);
		(void) parse_block();
	}
	fixup_if(ip, fixme);

}

void generate_addressof(const char *opcode, reg_t *val)
{
	struct command cmd;
	char addrop[sizeof(cmd.opcode)];

	/* prefix the symbol with & */
	addrop[0] = '&';
	strncpy(addrop+1, opcode, sizeof(addrop)-1);
	addrop[sizeof(addrop)-1] = '\0';

	(void) symtab_new(addrop, CONSTANT, (reg_t) (uintptr_t) val);
}

void parse_array(void)
{
	// a command is not expected right now, instead this is just a sneaky
	// bit of code reuse to collect a name and number from the input.
	struct command cmd = parse_command();
	// TODO: error checking...

	const size_t sz = cmd.operand[0].value * sizeof(reg_t);
	reg_t *r = alloc(sz);
	memset(r, 0, sz);

	generate_addressof(cmd.opcode, r);
}

void parse_bytes(void)
{
	// a command is not expected right now, instead this is just a sneaky
	// bit of code reuse to collect a name and number from the input.
	struct command cmd = parse_command();
	// TODO: error checking...

	const size_t sz = cmd.operand[0].value;
	reg_t *r = alloc(sz);
	memset(r, 0, sz);

	generate_addressof(cmd.opcode, r);
}

void parse_const(void)
{
	// a command is not expected right now, instead this is just a sneaky
	// bit of code reuse to collect a name and number from the input.
	struct command cmd = parse_command();
	// TODO: error checking...

	(void) symtab_new(cmd.opcode, CONSTANT, cmd.operand[0].value);
}

void parse_string(void)
{
	char sym[32];

	token(sym, sizeof(sym));
	char *t = (char *) token((char *) memp, 4096);
	reg_t *r = alloc(strlen(t) + 1);
	assert((void *) t == (void *) r);

	generate_addressof(sym, r);
}

void parse_var(void)
{
	// a command is not expected right now, instead this is just a sneaky
	// bit of code reuse to collect a name and number from the input.
	struct command cmd = parse_command();
	// TODO: error checking...

	reg_t *r = alloc(4);
	*r = cmd.operand[0].value;

	struct command mov = {
		.opcode = "mov",
		.sym = symtab_lookup("mov"),
		.operand = {
			{ REGISTER, /* arg0 */ 8 },
			{ IMMEDIATE, (reg_t) (uintptr_t) r },
		}
	};
	struct command ldw = {
		.opcode = "ldw",
		.sym = symtab_lookup("ldw"),
		.operand = {
			{ REGISTER, /* arg0 */ 8 },
			{ REGISTER, /* arg0 */ 8 },
			{ IMMEDIATE, 0 },
		}
	};

	reg_t *p;

	// TODO: symtab_new_start() and symtab_new_finalize() would be a better
	//       interface?
	p = ip = memp;
	ip = assemble_preamble(ip, NULL, 0);
	ip = assemble_word(ip, &mov);
	ip = assemble_word(ip, &ldw);
	ip = assemble_postamble(ip, NULL, 0);
	sync_caches(p, ip);
	memp = ip;
	(void) symtab_new(cmd.opcode, EXECPTR, (reg_t) (uintptr_t) p);

	generate_addressof(cmd.opcode, r);
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

struct symbol *symtab_new(const char *name, enum symtype type, reg_t val)
{
	size_t namelen = strlen(name) + 1;
	char *namemem = alloc(namelen);
	memcpy(namemem, name, namelen);

	struct symbol *s = alloc(sizeof(struct symbol));
	s->name = namemem;
	s->type = type;
	s->val = val;;
	symtab_add(s);

	return s;
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
			ooip = assemble_preamble(oob, NULL, 0);
			ooip = assemble_word(ooip, &cmd);
			ooip = assemble_postamble(ooip, NULL, 0);

			CHECK_OOB_CANARY();
			sync_caches(oob, ooip);
			exec(oob);
		} else {
			fprintf(stderr, "Bad symbol: %s\n", cmd.opcode);
		}
	}

	return 0;
}
