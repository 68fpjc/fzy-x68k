PROGRAM = fzy
VERSION0 = x68k-0.1.6
VERSION = "1.0 $(VERSION0)"

TARGET = $(PROGRAM).x
ARCHIVE = $(PROGRAM)-$(VERSION0).zip
DISTDIR = dist

LIBCONDRV_ARC = libcond100.zip
LIBCONDRV_URL = https://github.com/kg68k/libcondrv/releases/download/v1.0.0/$(LIBCONDRV_ARC)
LIBCONDRV_DIR = libcondrv
LIBCONDRV_INCLUDE_DIR = $(LIBCONDRV_DIR)/include
LIBCONDRV_INCLUDE = $(LIBCONDRV_INCLUDE_DIR)/condrv.h
LIBCONDRV_LIB_DIR = $(LIBCONDRV_DIR)
LIBCONDRV_LIB = $(LIBCONDRV_LIB_DIR)/libcondrv.a

LIBMB_VERSION = 20240408-01
LIBMB_ARC = libmb-$(LIBMB_VERSION).zip
LIBMB_URL = https://github.com/68fpjc/libmb/releases/download/$(LIBMB_VERSION)/$(LIBMB_ARC)
LIBMB_DIR = libmb
LIBMB_INCLUDE_DIR = $(LIBMB_DIR)
LIBMB_LIB_DIR = $(LIBMB_DIR)
LIBMB_LIB = $(LIBMB_LIB_DIR)/libmb.a

CROSS = m68k-xelf-
CC = $(CROSS)gcc
AS = $(CROSS)as
LD = $(CROSS)gcc

CFLAGS_COMMON = -Wall -Wextra -std=c99 -pedantic -DVERSION=\"${VERSION}\" -D_GNU_SOURCE -MMD \
  -m68000 -DHIGHLIGHT_OPTION -DNO_CALC_SCORE_OPTION -I$(LIBCONDRV_INCLUDE_DIR) -I$(LIBMB_INCLUDE_DIR)
OBJS=src/fzy.o src/match.o src/choices.o src/options.o src/tty_interface.o \
  src/arch_x68k.o
LDFLAGS =
LDLIBS = -Wl,-lcondrv -Wl,-L$(LIBCONDRV_LIB_DIR) -Wl,-lmb -Wl,-L$(LIBMB_LIB_DIR)
ifdef RELEASE_BUILD
  CFLAGS = $(CFLAGS_COMMON) -O3
else
  CFLAGS = $(CFLAGS_COMMON) -O0 -g
endif
DEPS = $(patsubst %.o,%.d,$(OBJS))

.PHONY: all configh extra-headers clean veryclean release bump-version

all: gen_config_h extra-headers $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $^ $(LDLIBS) -o $@

-include $(DEPS)

gen_config_h: config.h

config.h: src/config.def.h
	cp $< $@

extra-headers: \
  $(LIBCONDRV_INCLUDE_DIR)/condrv.h \
  $(LIBMB_INCLUDE_DIR)/mbctype.h $(LIBMB_INCLUDE_DIR)/mbstring.h

$(LIBCONDRV_INCLUDE_DIR)/condrv.h: | $(LIBCONDRV_LIB)
$(LIBMB_INCLUDE_DIR)/mbctype.h: | $(LIBMB_LIB)
$(LIBMB_INCLUDE_DIR)/mbstring.h: | $(LIBMB_LIB)

# libcondrv を展開 / 変換する
$(LIBCONDRV_LIB): | $(LIBCONDRV_DIR)/$(LIBCONDRV_ARC)
	7z x -y $(LIBCONDRV_DIR)/$(LIBCONDRV_ARC) -o$(LIBCONDRV_DIR)
	x68k2elf.py $(LIBCONDRV_DIR)/lib/libcondrv.a $@

# libcondrv をダウンロードする
$(LIBCONDRV_DIR)/$(LIBCONDRV_ARC):
	wget -q -P $(LIBCONDRV_DIR) $(LIBCONDRV_URL)

# libmb を展開する
$(LIBMB_LIB): | $(LIBMB_DIR)/$(LIBMB_ARC)
	7z x -y $(LIBMB_DIR)/$(LIBMB_ARC) -o$(LIBMB_LIB_DIR)

# libmb をダウンロードする
$(LIBMB_DIR)/$(LIBMB_ARC):
	wget -q -P $(LIBMB_DIR) $(LIBMB_URL)

clean:
	-rm -f *.x src/*.o *.elf* src/*.d
	-rm -rf $(LIBCONDRV_DIR)/* $(LIBMB_DIR)/*
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
