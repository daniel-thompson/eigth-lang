# SPDX-License-Identifier: GPL-3.0-or-later

all : eigth

SRCS = $(wildcard src/*.c)
HDRS = $(wildcard src/*.h)
OBJS = $(SRCS:.c=.o)

CC = clang
CFLAGS = -std=c99 -g -O2 -Wall

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
