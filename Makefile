# SPDX-License-Identifier: GPL-3.0-or-later

all : eigth

SRCS = $(wildcard src/*.c)
HDRS = $(wildcard src/*.h)
OBJS = $(SRCS:.c=.o)

CC = gcc
CFLAGS = -std=c99 -g -O2 -Wall

# eigth makes the assumptions that pointers to symbols provided by the
# application sit below the 32-bit boundary (so we can store function
# pointers in a 32-bit eigth word). Linking as a position independent
# executable will break that assumption!
LDFLAGS = -no-pie

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
