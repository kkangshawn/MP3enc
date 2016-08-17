CC=gcc

ifndef UNAME
UNAME = $(shell uname)
ifneq (,$(findstring MINGW, $(UNAME)))
UNAME = MINGW
endif
endif

ifndef ARCH
ARCH = $(shell uname -m)
endif

ifndef CFLAGS
CFLAGS = -MMD -O0 -Wall -g3 -MP
ifeq ($(UNAME), MINGW)
CFLAGS += -DPTW32_STATIC_LIB
endif
endif

LDFLAGS = -static

OBJS = main.o
OBJS += audio.o

ifeq ($(UNAME), Linux)
ifeq ($(ARCH), x86_64)
LIBS = -lmp3lame
else ifeq ($(ARCH), armv7l)
LIBS = -lmp3lame_armv7l
endif
else ifeq ($(UNAME), MINGW)
LIBS = -lmp3lame_mingw
endif

LIBS += -lpthread -lm
LIBDIR += -Llib
LIBS += $(LIBDIR)


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


BINALL=MP3enc
ALL = $(BINALL)

all: $(ALL)

%.o: %.c
	$(Q)$(CC) -c -o $@ $(CFLAGS) $<
	@$(E) "  CC " $<

MP3enc: $(OBJS)
	$(Q)$(LDO) $(LDFLAGS) -o MP3enc $(OBJS) $(LIBS)
	@$(E) "  LD " $@

clean:
ifneq ($(UNAME), MINGW)
	rm -f MP3enc
	rm -f *.o
	rm -f *.d
else
	rm MP3enc.exe *.o *.d
endif
