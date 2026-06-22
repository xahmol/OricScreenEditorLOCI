# Makefile - kbtest, a standalone keyboard matrix test for Oric Atmos
#
# Builds a minimal Oscar64 binary (no LOCI, no overlay RAM, no IJK, no
# charwin/menu system) that does nothing but continuously display the raw
# 8x8 keyboard matrix, decoded character, and modifier flags -- see
# src/main.c's header comment and /EQUALS_KEY_BUG_ANALYSIS.md (project
# root, one level up) for why this exists.
#
# Reuses ../include/keyboard.c/h and ../include/oric.h/oric_crt.c
# verbatim (via -i=../include) -- never copied, so there is no risk of
# drift between the scanner code tested here and the scanner code in the
# real OricScreenEditorLOCI program.
#
# Usage:
#   make              # build build/kbtest.tap
#   make run          # build + launch in Oricutron
#   make clean

ifndef OSCAR64_HOME
$(warning OSCAR64_HOME not set. Defaulting to $(HOME)/oscar64)
export OSCAR64_HOME = $(HOME)/oscar64
endif

ifndef ORICUTRON_HOME
$(warning ORICUTRON_HOME not set. Defaulting to $(HOME)/oricutron)
export ORICUTRON_HOME = $(HOME)/oricutron
endif

CC = $(OSCAR64_HOME)/bin/oscar64
PY = python3

PROGNAME  = KBTEST
LOAD_ADDR = 0x0500

CFLAGS = \
  -n              \
  -tf=bin         \
  -rt=../include/oric_crt.c \
  -i=../include   \
  -i=src          \
  -O2             \
  -dNOFLOAT

.PHONY: all run clean

all: build/kbtest.tap

build/kbtest.bin: src/main.c
	@mkdir -p build 2>/dev/null ; true
	$(CC) $(CFLAGS) -o=build/kbtest.bin src/main.c

build/kbtest.tap: build/kbtest.bin
	$(PY) ../tools/mktap.py \
	    build/kbtest.bin \
	    build/kbtest.tap \
	    $(PROGNAME) \
	    $(LOAD_ADDR)

run: build/kbtest.tap
	cd $(ORICUTRON_HOME) && ./oricutron -ma --serial none --vsynchack off --turbotape on \
	    -tap $(CURDIR)/build/kbtest.tap

clean:
	rm -rf build
