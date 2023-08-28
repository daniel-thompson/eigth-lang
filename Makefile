# SPDX-License-Identifier: GPL-3.0-or-later

all : eigth

CC ?= gcc
CFLAGS = -std=c99 -g -O2 -Wall -Isrc/ -fno-PIE

# eigth makes the assumptions that pointers to symbols provided by the
# application sit below the 32-bit boundary (so we can store function
# pointers in a 32-bit eigth word). Linking as a position independent
# executable will break that assumption!
LDFLAGS = -no-pie $(EXTRA_LDFLAGS)

# Automatically enable native codegen for applicable hosts
ifeq ($(shell uname -m)-$(CC),aarch64-gcc)
A64=1
endif

SRCS = $(wildcard src/*.c)
ifeq ($(A64),1)
SRCS := $(subst src/vm.c,src/arm/a64.c,$(SRCS))
endif

HDRS = $(wildcard src/*.h)
OBJS = $(SRCS:.c=.o)



eigth : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

bench : eigth
	./eigth < examples/benchmark.8th

test : eigth
	./eigth < test/test.8th

check : test bench

clean :
	$(RM) eigth $(OBJS)

$(OBJS) : Makefile $(HDRS)

.PHONY : all bench check clean test
