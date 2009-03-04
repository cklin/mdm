# Time-stamp: <2009-03-03 18:20:40 cklin>

CC := $(shell which mdm-run > /dev/null && echo mdm-run) $(CC)
CFLAGS := -Wall -D_GNU_SOURCE -Iinclude

LIB := $(patsubst %.c,%.o,$(wildcard library/*.c))
PROG := $(patsubst programs/%.c,%,$(wildcard programs/*.c))

all : $(PROG)

$(LIB) : include/middleman.h
mdm-top : LDFLAGS=-lcurses

mdm-% : programs/mdm-%.c $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $+

clean :
	$(RM) library/*.o

dist-clean : clean
	$(RM) mdm-*

.PHONY : all clean dist-clean
