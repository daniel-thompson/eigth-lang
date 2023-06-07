# SPDX-License-Identifier: GPL-3.0-or-later

all : eigth

SRCS = $(wildcard src/*.c)
HDRS = $(wildcard src/*.h)
OBJS = $(SRCS:.c=.o)

CC = clang
CFLAGS = -std=c99 -g -O0 -Wall

eigth : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

check : eigth
	./eigth < test/test.8th

clean :
	$(RM) eigth $(OBJS)

$(OBJS) : Makefile $(HDRS)
