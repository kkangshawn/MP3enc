ifndef CC
CC=gcc
endif

ifndef CFLAGS
CFLAGS = -MMD -O0 -Wall -g3 -MP
endif


BINALL=MP3enc

ALL = $(BINALL)

all: $(ALL)

LDFLAGS = -static

OBJS = main.o
OBJS += audio.o

LIBS = -lmp3lame -lpthread -lm

ifndef LDO
LDO=$(CC)
endif

Q=@
E=echo
ifeq ($(V), 1)
Q=
E=true
endif
ifeq ($(QUIET), 1)
Q=@
E=true
endif

%.o: %.c
	$(Q)$(CC) -c -o $@ $(CFLAGS) $<
	@$(E) "  CC " $<

MP3enc: $(OBJS)
	$(Q)$(LDO) $(LDFLAGS) -o MP3enc $(OBJS) $(LIBS)
	@$(E) "  LD " $@

clean:
	rm -f MP3enc
	rm -f *.o
	rm -f *.d

