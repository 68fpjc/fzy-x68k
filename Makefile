VERSION = "1.0 x68k-20250323-01"

TARGET = fzy.x

CC = m68k-xelf-gcc
LD = $(CC)
# CFLAGS =-Wall -Wextra -g -std=c99 -O0 -pedantic -DVERSION=\"${VERSION}\" -D_GNU_SOURCE -MMD
CFLAGS =-Wall -Wextra -std=c99 -O3 -pedantic -DVERSION=\"${VERSION}\" -D_GNU_SOURCE -MMD
LDLIBS =
OBJS=src/fzy.o src/match.o src/choices.o src/options.o src/tty_interface.o src/cp932.o

ifeq ($(CC),m68k-xelf-gcc)
	CFLAGS += -DHIGHLIGHT_OPTION
	OBJS += src/arch_x68k.o
endif

DEPS = $(patsubst %.o,%.d,$(OBJS))

.PHONY: all configh clean

all: gen_config_h $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $^ $(LDLIBS) -o $@

-include $(DEPS)

gen_config_h: config.h

config.h: src/config.def.h
	cp $< $@

clean:
	-rm -f $(TARGET) src/*.o *.elf* src/*.d config.h
