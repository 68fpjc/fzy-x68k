PROGRAM = fzy
VERSION0 = 20250323-01
VERSION = "1.0 x68k-$(VERSION0)"

TARGET = $(PROGRAM).x
ARCHIVE = $(PROGRAM)-$(VERSION0).zip
DISTDIR = dist

CROSS = m68k-xelf-
CC = $(CROSS)gcc
AS = $(CROSS)as
LD = $(CROSS)gcc

CFLAGS_COMMON = -Wall -Wextra -std=c99 -pedantic -DVERSION=\"${VERSION}\" -D_GNU_SOURCE -MMD
OBJS=src/fzy.o src/match.o src/choices.o src/options.o src/tty_interface.o src/cp932.o
LDFLAGS =
LDLIBS =
ifdef RELEASE_BUILD
  CFLAGS = $(CFLAGS_COMMON) -O3
else
  CFLAGS = $(CFLAGS_COMMON) -O0 -g
endif
ifeq ($(CC),m68k-xelf-gcc)
	CFLAGS += -m68000 -DHIGHLIGHT_OPTION
	OBJS += src/arch_x68k.o
endif
DEPS = $(patsubst %.o,%.d,$(OBJS))

.PHONY: all configh clean veryclean release

all: gen_config_h $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $^ $(LDLIBS) -o $@

-include $(DEPS)

gen_config_h: config.h

config.h: src/config.def.h
	cp $< $@

clean:
	-rm -f *.x src/*.o *.elf* src/*.d
	-rm -f config.h

veryclean: clean
	-rm -rf $(DISTDIR)

release:
	$(MAKE) clean
	$(MAKE) RELEASE_BUILD=yes
	mkdir -p $(DISTDIR)
	mv $(TARGET) $(DISTDIR)
	pandoc -f markdown -t plain README-x68k.md | iconv -t cp932 >$(DISTDIR)/README-x68k.txt
	cd $(DISTDIR) && 7z a $(ARCHIVE) $(TARGET) README-x68k.txt
	$(MAKE) clean
