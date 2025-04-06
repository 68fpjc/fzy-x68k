PROGRAM = fzy
VERSION0 = x68k-0.1.3-dev
VERSION = "1.0 $(VERSION0)"

TARGET = $(PROGRAM).x
ARCHIVE = $(PROGRAM)-$(VERSION0).zip
DISTDIR = dist

LIBCONDRV_ARC = libcond100.zip
LIBCONDRV_URL = https://github.com/kg68k/libcondrv/releases/download/v1.0.0/$(LIBCONDRV_ARC)
LIBCONDRV_DIR = libcondrv
LIBCONDRV_INCLUDE_DIR = $(LIBCONDRV_DIR)/include
LIBCONDRV_INCLUDE = $(LIBCONDRV_INCLUDE_DIR)/condrv.h
LIBCONDRV_LIB = $(LIBCONDRV_DIR)/libcondrv.a

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
	CFLAGS += -m68000 -DHIGHLIGHT_OPTION -I$(LIBCONDRV_INCLUDE_DIR)
	LDLIBS +=  $(LIBCONDRV_LIB)
	OBJS += src/arch_x68k.o
endif
DEPS = $(patsubst %.o,%.d,$(OBJS))

.PHONY: all configh clean veryclean release bump-version

all: gen_config_h $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $^ $(LDLIBS) -o $@

-include $(DEPS)

gen_config_h: config.h

config.h: src/config.def.h
	cp $< $@

src/arch_x68k.o: $(LIBCONDRV_LIB)

# libcondrv を展開 / 変換する
$(LIBCONDRV_LIB): $(LIBCONDRV_DIR)/$(LIBCONDRV_ARC)
	7z x -y $< -o$(LIBCONDRV_DIR)
	x68k2elf.py $(LIBCONDRV_DIR)/lib/libcondrv.a $@

# libcondrv をダウンロードする
$(LIBCONDRV_DIR)/$(LIBCONDRV_ARC):
	wget -P $(LIBCONDRV_DIR) $(LIBCONDRV_URL)

clean:
	-rm -f *.x src/*.o *.elf* src/*.d
	-rm -rf $(LIBCONDRV_DIR)/*
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

bump-version:
	@echo "Current version: $(VERSION0)"
	@read -p "New version: " new_version && \
	sed -i "s/VERSION0 = $(VERSION0)/VERSION0 = $$new_version/" makefile
