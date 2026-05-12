CC     = gcc
CONF   = debug
CFLAGS = -Wall -Wextra -Werror -D_GNU_SOURCE

ifeq ($(CONF),debug)
CFLAGS += -g -O0 -ggdb
else ifeq ($(CONF),release)
CFLAGS += -O3
else
$(error CONF must be debug or release)
endif

TESTSRC := $(wildcard test/*.c)
SOURCES := $(wildcard *.c)
COMMON  := $(SOURCES:.c=.o)
TARGETS := $(TESTSRC:.c=)
DEPEND  := .depend

.PHONY: all clean

all: $(DEPEND) $(TARGETS)

$(DEPEND):
	$(CC) -MM *.c > $(DEPEND)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGETS): % : %.o test/test.h $(COMMON)
	$(CC) $(CFLAGS) -o $@ $< $(COMMON)

test/%.o: CFLAGS += -Wno-cast-function-type -Wno-deprecated-declarations

clean:
	rm -rf *.o test/*.o test/*.exe $(TARGETS) $(DEPEND)

ifeq ($(DEPEND),$(wildcard $(DEPEND)))
include $(DEPEND)
endif
