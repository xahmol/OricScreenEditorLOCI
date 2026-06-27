# Makefile - OricScreenEditorLOCI (Oric Atmos, Oscar64)
#
# Two-step build:
#   1. Oscar64 (-n -tf=bin -rt=include/oric_crt.c) -> build/*.bin
#   2. tools/mktap.py -> build/*.tap
#
# make run requires Oricutron; see ORICUTRON_HOME below
# Overlay RAM / LOCI features require real LOCI hardware (not testable in Oricutron).

# -------------------------------------------------------------------------
# Cross-platform portability (Windows CMD vs POSIX)
# -------------------------------------------------------------------------

ifeq ($(OS),Windows_NT)
  NULLDEV = nul:
  DEL     = -del /f
  RMDIR   = rmdir /s /q
  MKDIR   = mkdir
else
  NULLDEV = /dev/null
  DEL     = $(RM)
  RMDIR   = $(RM) -r
  MKDIR   = mkdir -p
endif

# -------------------------------------------------------------------------
# Toolchain
# -------------------------------------------------------------------------

ifndef OSCAR64_HOME
$(warning OSCAR64_HOME not set. Defaulting to $(HOME)/oscar64)
export OSCAR64_HOME = $(HOME)/oscar64
endif

ifndef ORICUTRON_HOME
$(warning ORICUTRON_HOME not set. Defaulting to $(HOME)/oricutron)
export ORICUTRON_HOME = $(HOME)/oricutron
endif

CC    = $(OSCAR64_HOME)/bin/oscar64
PY    = python3
EMUL  = $(ORICUTRON_HOME)/oricutron

# -------------------------------------------------------------------------
# Build versioning
# -------------------------------------------------------------------------

VERSION_MAJOR = 0
VERSION_MINOR = 1
VERSION_PATCH = 0

# -------------------------------------------------------------------------
# Localisation
# -------------------------------------------------------------------------
# LANG=EN (default) or LANG=FR
# LANG=EN builds build/oseloci.tap
# LANG=FR builds build/oseloci_fr.tap
# make all-langs builds both

LANG ?= EN
ifeq ($(LANG),FR)
  LANGFLAG   = -dLANG_FR
  LANGSUFFIX = _fr
else
  LANGFLAG   =
  LANGSUFFIX =
endif

# -------------------------------------------------------------------------
# Project
# -------------------------------------------------------------------------

MAIN      = oseloci
PROGNAME  = OSELOCI
LOAD_ADDR = 0x0500

# -------------------------------------------------------------------------
# Compiler flags
# -------------------------------------------------------------------------

CFLAGS = \
  -n              \
  -tf=bin         \
  -rt=include/oric_crt.c \
  -i=include      \
  -i=src          \
  -O2             \
  -dNOFLOAT       \
  -dVERSION_MAJOR=$(VERSION_MAJOR) \
  -dVERSION_MINOR=$(VERSION_MINOR) \
  -dVERSION_PATCH=$(VERSION_PATCH) \
  $(LANGFLAG)

# -------------------------------------------------------------------------
# Source dependency lists
# Oscar64 follows #pragma compile chains internally; make does not --
# list everything here so rebuilds trigger correctly.
# -------------------------------------------------------------------------

MAIN_SRCS = \
  src/main.c            \
  src/appstate.h        \
  src/canvas.c          \
  src/canvas.h          \
  src/statusbar.c       \
  src/statusbar.h       \
  src/editor.c          \
  src/editor.h          \
  src/menu.c            \
  src/menu.h            \
  src/menudata.c        \
  src/menudata.h        \
  src/charsetedit.c     \
  src/charsetedit.h     \
  src/charsetswap.c     \
  src/charsetswap.h     \
  src/palette.c         \
  src/palette.h         \
  src/colourpicker.c    \
  src/colourpicker.h    \
  src/modes.c           \
  src/modes.h           \
  src/select.c          \
  src/select.h          \
  src/move.c            \
  src/move.h            \
  src/write.c           \
  src/write.h           \
  src/fileio.c          \
  src/fileio.h          \
  src/filepicker.c      \
  src/filepicker.h      \
  src/undo.c            \
  src/undo.h            \
  src/input.c           \
  src/input.h           \
  src/help.c            \
  src/help.h            \
  src/info.c            \
  src/info.h            \
  src/findreplace.c     \
  src/findreplace.h     \
  src/strings.h         \
  src/strings_en.h      \
  src/strings_fr.h      \
  include/oric_crt.c    \
  include/crt_math.c    \
  include/oric.h        \
  include/charwin.c      \
  include/charwin.h      \
  include/keyboard.c     \
  include/keyboard.h     \
  include/charset.c      \
  include/charset.h      \
  include/ijk.c          \
  include/ijk.h          \
  include/loci.c         \
  include/loci.h

# -------------------------------------------------------------------------
# USB stick transfer -- variable declarations
# -------------------------------------------------------------------------
# Set USBPATH in .env (gitignored) -- path to the directory on the USB stick.
# Native Linux: /media/yourname/USBSTICK/oric
# WSL2: Windows drives auto-mount at /mnt/<letter>; USB stick on E: -> /mnt/e/oric
# See .env.example for a template.

-include .env
USBPATH  ?= NOT_SET

# Derived from USBPATH -- used for WSL2 auto-mount.
# Assumes USBPATH starts with /mnt/<letter>/... (standard WSL2 drvfs layout).
# Example: USBPATH=/mnt/e/oric -> USBMOUNT=/mnt/e, USBDRIVE=E:
USBMOUNT := $(shell echo "$(USBPATH)" | cut -d/ -f1-3)
USBDRIVE := $(shell echo "$(USBPATH)" | cut -d/ -f3 | tr a-z A-Z):

# Detect WSL2 at parse time so check-usb can branch without a shell function.
IS_WSL2  := $(shell grep -qi microsoft /proc/version 2>/dev/null && echo 1 || echo 0)

# -------------------------------------------------------------------------
# Emulator flags
# -------------------------------------------------------------------------

EMUFLAG = -ma --serial none --vsynchack off --turbotape on

# -------------------------------------------------------------------------
# Phosphoric automated testing
# -------------------------------------------------------------------------
# PHOSDIR is set in .env (see .env.example) -- checkout of
# https://github.com/benedictemarty/Phosphoric, providing oric1-emu and
# roms/basic11b.rom. Phosphoric lets oseloci.tap be fast-loaded (-t ... -f)
# under Atmos BASIC 1.1 and tested headless via RAM dumps.

PHOSDIR  ?= NOT_SET
PHOS      = $(PHOSDIR)/oric1-emu
ATMOSROM  = $(PHOSDIR)/roms/basic11b.rom

CYCLES   ?= 8000000

# =========================================================================
# Targets
# all: must appear first so it is the default goal
# =========================================================================

.PHONY: all all-langs clean run docs zip check-usb usb kbtest check-phosphoric sandbox-reset test-capture test-boot test-menus test-screenresize test-charsetram-spike test-charsetedit test-palette test-colourpicker test-cursor-autoscroll test-linebox test-select test-move test-writemode test-boot-no-loci test-select-cutcopy test-undo-overflow test-help-funct8 test-fileio-traffic test-findreplace test-write-hexattr test-trymode test-goto test-hollowbox test-ellipse test

all: zip

# Step 1: compile main app to raw binary
build/$(MAIN)$(LANGSUFFIX).bin: $(MAIN_SRCS)
	@$(MKDIR) build 2>$(NULLDEV) ; true
	$(CC) $(CFLAGS) -o=build/$(MAIN)$(LANGSUFFIX).bin src/main.c

# Step 2: wrap binary in Oric tape header
build/$(MAIN)$(LANGSUFFIX).tap: build/$(MAIN)$(LANGSUFFIX).bin
	$(PY) tools/mktap.py \
	    build/$(MAIN)$(LANGSUFFIX).bin \
	    build/$(MAIN)$(LANGSUFFIX).tap \
	    $(PROGNAME) \
	    $(LOAD_ADDR)

# Launch in Oricutron (must cd to oricutron dir -- it loads ROMs from cwd).
# NOTE: Oricutron cannot emulate LOCI at all, and LOCI is now required to
# even boot (screenmap[] lives in overlay RAM) -- `make run` will only
# ever show the "No LOCI device detected" gate, never the editor itself.
# That's still useful for testing the gate/Oricutron-absent path; for
# anything past it, use Phosphoric with --loci (see "Phosphoric test
# harness" in CLAUDE.md) or real LOCI hardware.
run: build/$(MAIN)$(LANGSUFFIX).tap
	cd $(ORICUTRON_HOME) && \
	    $(EMUL) $(EMUFLAG) "$(CURDIR)/build/$(MAIN)$(LANGSUFFIX).tap"

# Build both language variants (invoke .tap targets directly to avoid default-goal recursion)
all-langs:
	$(MAKE) LANG=EN build/$(MAIN).tap
	$(MAKE) LANG=FR build/$(MAIN)_fr.tap

# -------------------------------------------------------------------------
# USB stick transfer
# -------------------------------------------------------------------------
# usb also copies assets/PETSCII{PJ,SC,CS,CA}.BIN -- V1's original PETSCII
# demo project, unmodified (byte-identical to what V1 itself wrote; see
# CLAUDE.md "V1 file-format compatibility"), so a fresh build's File >
# Load project has something real to try immediately. Not embedded in
# the binary -- distributed alongside it.
#
# Also copies assets/OSEDEMO.BIN -- a custom-charset showcase screen (a
# 9x9-cell pixel-art sun built from hand-drawn 6x8 glyphs, a hand-drawn
# box-frame border, and ink/paper colour swatch bars), loadable via
# File > Load Combined (768-byte charset + 1120-byte 40x28 screen, no
# header -- see fileio.h). Generated by a one-off script, not checked
# in; see assets/ for the resulting binary only. Also copies
# assets/OSEDEMO{PJ,SC,CS,CA}.BIN -- the same artwork in File > Load
# Project's 4-file scheme (OSEDEMOCA.BIN added when the demo was
# updated to use alt-charset glyphs), added 2026-06-21 alongside
# the Combined-format file.
#
# Also copies assets/OSELOGO{PJ,SC,CS,CA}.BIN -- a from-scratch
# recreation of the official Oric company logo, built entirely from
# hand-drawn glyphs across both CHARSET_STD and CHARSET_ALT, loadable
# via File > Load Project (tools/gen_logo_screen.py, not part of the
# build).
#
# Also copies assets/LUDOTITL{PJ,SC,CS,CA}.BIN -- the Ludo title screen
# demo project, loadable via File > Load Project.
#
# Also copies assets/LUDOSCRM{PJ,SC,CS,CA}.BIN -- the Ludo game screen
# demo project, loadable via File > Load Project.

check-usb:
	@test "$(USBPATH)" != "NOT_SET" || \
	    (echo "ERROR: USBPATH not set -- copy .env.example to .env and set USBPATH" && false)
	@if ! test -d "$(USBPATH)"; then \
	    if [ "$(IS_WSL2)" = "1" ]; then \
	        echo "WSL2: mounting $(USBDRIVE) at $(USBMOUNT) via drvfs..."; \
	        sudo mount -t drvfs $(USBDRIVE) $(USBMOUNT); \
	    fi; \
	fi
	@test -d "$(USBPATH)" || \
	    (echo "ERROR: USB path '$(USBPATH)' not found -- plug in USB stick and retry" && false)

usb: check-usb all-langs kbtest
	cp build/$(MAIN).tap      "$(USBPATH)/"
	cp build/$(MAIN)_fr.tap   "$(USBPATH)/"
	cp assets/PETSCIIPJ.BIN assets/PETSCIISC.BIN assets/PETSCIICS.BIN assets/PETSCIICA.BIN "$(USBPATH)/"
	cp assets/OSETSC.BIN assets/OSEHS1.BIN assets/OSEHS2.BIN assets/OSEHS3.BIN assets/OSEHS4.BIN "$(USBPATH)/"
	cp assets/OSETSF.BIN assets/OSEHF1.BIN assets/OSEHF2.BIN assets/OSEHF3.BIN assets/OSEHF4.BIN "$(USBPATH)/"
	cp assets/OSEDEMO.BIN "$(USBPATH)/"
	cp assets/OSEDEMOPJ.BIN assets/OSEDEMOSC.BIN assets/OSEDEMOCS.BIN assets/OSEDEMOCA.BIN "$(USBPATH)/"
	cp assets/OSELOGOPJ.BIN assets/OSELOGOSC.BIN assets/OSELOGOCS.BIN assets/OSELOGOCA.BIN "$(USBPATH)/"
	cp assets/LUDOTITLPJ.BIN assets/LUDOTITLSC.BIN assets/LUDOTITLCS.BIN assets/LUDOTITLCA.BIN "$(USBPATH)/"
	cp assets/LUDOSCRMPJ.BIN assets/LUDOSCRMSC.BIN assets/LUDOSCRMCS.BIN assets/LUDOSCRMCA.BIN "$(USBPATH)/"
	cp kbtest/build/kbtest.tap "$(USBPATH)/"
	@if [ "$(IS_WSL2)" = "1" ]; then \
	    echo "WSL2: unmounting $(USBMOUNT)..."; \
	    sudo umount $(USBMOUNT); \
	    echo "Done -- USB stick can now be ejected in Windows."; \
	fi

# kbtest/ is a separate, standalone diagnostic program (see CLAUDE.md
# "Standalone keyboard matrix test (kbtest/)") -- not part of the main
# build or ZIP release; `make kbtest` builds it explicitly, and
# `make usb` includes it on the USB stick for real-hardware testing.
kbtest:
	$(MAKE) -C kbtest

# -------------------------------------------------------------------------
# Phosphoric automated testing
# -------------------------------------------------------------------------
# make test          -- full automated suite (currently: test-boot)
# make test-capture CYCLES=N TYPEKEYS='...'
#                    -- calibration helper: fast-loads oseloci.tap under
#                       Atmos BASIC 1.1, runs for CYCLES, dumps
#                       tests/out/capture.bin (RAM) and tests/out/capture.png
#                       (screenshot). No assertions -- used to find the
#                       right cycle counts / --type-keys sequences.

check-phosphoric:
	@test "$(PHOSDIR)" != "NOT_SET" || \
	    (echo "ERROR: PHOSDIR not set -- copy .env.example to .env and set PHOSDIR" && false)
	@test -x "$(PHOS)" || \
	    (echo "ERROR: oric1-emu not found/executable at $(PHOS) -- check PHOSDIR in .env" && false)
	@test -f "$(ATMOSROM)" || \
	    (echo "ERROR: Atmos ROM not found at $(ATMOSROM)" && false)

# Reset the test sandbox from checked-in fixtures + the freshly built tap,
# so every test run starts from a known state.
sandbox-reset: build/$(MAIN)$(LANGSUFFIX).tap
	$(RMDIR) tests/sandbox 2>$(NULLDEV) ; true
	$(MKDIR) tests/sandbox 2>$(NULLDEV) ; true
	cp -r tests/fixtures/. tests/sandbox/
	find tests/sandbox -name '.gitkeep' -delete
	cp build/$(MAIN)$(LANGSUFFIX).tap tests/sandbox/

test-capture: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	$(PHOS) -r $(ATMOSROM) \
	    -t tests/sandbox/$(MAIN)$(LANGSUFFIX).tap -f --loci \
	    --headless -c $(CYCLES) \
	    $(if $(TYPEKEYS),--type-keys '$(TYPEKEYS)') \
	    --dump-ram-at $(CYCLES):tests/out/capture.bin \
	    --screenshot-at $(CYCLES):tests/out/capture.ppm
	@which pnmtopng >$(NULLDEV) 2>&1 && pnmtopng tests/out/capture.ppm > tests/out/capture.png || true
	python3 tests/scripts/oric_screen.py tests/out/capture.bin

test-boot: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_boot.sh

test-menus: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_menus.sh

test-screenresize: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_screenresize.sh

test-charsetram-spike: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_charsetram_spike.sh

test-charsetedit: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_charsetedit.sh

test-palette: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_palette.sh

test-colourpicker: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_colourpicker.sh

test-cursor-autoscroll: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_cursor_autoscroll.sh

test-linebox: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_linebox.sh

test-select: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_select.sh

test-move: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_move.sh

test-writemode: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_writemode.sh

test-boot-no-loci: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_boot_no_loci.sh

test-select-cutcopy: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_select_cutcopy.sh

test-undo-overflow: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_undo_overflow.sh

test-help-funct8: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_help_funct8.sh

test-fileio-traffic: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_fileio_traffic.sh

test-findreplace: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_findreplace.sh

test-write-hexattr: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_write_hexattr.sh

test-trymode: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_trymode.sh

test-goto: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_goto.sh

test-hollowbox: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_hollowbox.sh

test-ellipse: check-phosphoric sandbox-reset
	$(MKDIR) tests/out 2>$(NULLDEV) ; true
	PHOS=$(PHOS) ATMOSROM=$(ATMOSROM) SANDBOX=tests/sandbox OUT=tests/out \
	    TAPFILE=$(MAIN)$(LANGSUFFIX).tap \
	    bash tests/scripts/test_ellipse.sh

test:
	@status=0; \
	$(MAKE) test-boot || status=1; \
	$(MAKE) test-menus || status=1; \
	$(MAKE) test-screenresize || status=1; \
	$(MAKE) test-charsetram-spike || status=1; \
	$(MAKE) test-charsetedit || status=1; \
	$(MAKE) test-palette || status=1; \
	$(MAKE) test-colourpicker || status=1; \
	$(MAKE) test-cursor-autoscroll || status=1; \
	$(MAKE) test-linebox || status=1; \
	$(MAKE) test-select || status=1; \
	$(MAKE) test-move || status=1; \
	$(MAKE) test-writemode || status=1; \
	$(MAKE) test-boot-no-loci || status=1; \
	$(MAKE) test-select-cutcopy || status=1; \
	$(MAKE) test-undo-overflow || status=1; \
	$(MAKE) test-help-funct8 || status=1; \
	$(MAKE) test-fileio-traffic || status=1; \
	$(MAKE) test-findreplace || status=1; \
	$(MAKE) test-write-hexattr || status=1; \
	$(MAKE) test-trymode || status=1; \
	$(MAKE) test-goto || status=1; \
	$(MAKE) test-hollowbox || status=1; \
	$(MAKE) test-ellipse || status=1; \
	exit $$status

# -------------------------------------------------------------------------
# Documentation -- generate PDF from Markdown (requires pandoc)
# README.pdf is committed to git so it ships in release ZIPs even if the
# recipient does not have pandoc installed.
# -------------------------------------------------------------------------

docs: README.pdf README_fr.pdf

README.pdf: README.md
	@if which pandoc >/dev/null 2>&1; then \
	    pandoc README.md -o README.pdf; \
	else \
	    echo "WARNING: pandoc not found -- README.pdf not updated (install: sudo apt install pandoc texlive-xetex)"; \
	fi

README_fr.pdf: README_fr.md
	@if which pandoc >/dev/null 2>&1; then \
	    pandoc README_fr.md -o README_fr.pdf; \
	else \
	    echo "WARNING: pandoc not found -- README_fr.pdf not updated (install: sudo apt install pandoc texlive-xetex)"; \
	fi

# -------------------------------------------------------------------------
# Release ZIP -- same payload as 'make usb', plus both PDF READMEs.
# ZIP name: OricScreenEditorLOCI_vMAJOR.MINOR.PATCH_YYYYMMDD.zip
# -------------------------------------------------------------------------

ZIPNAME = OricScreenEditorLOCI_v$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)_$(shell date +%Y%m%d)

zip: all-langs docs
	$(MKDIR) build 2>$(NULLDEV) ; true
	zip -j build/$(ZIPNAME).zip \
	    build/$(MAIN).tap \
	    build/$(MAIN)_fr.tap \
	    assets/PETSCIIPJ.BIN assets/PETSCIISC.BIN assets/PETSCIICS.BIN assets/PETSCIICA.BIN \
	    assets/OSETSC.BIN assets/OSEHS1.BIN assets/OSEHS2.BIN assets/OSEHS3.BIN assets/OSEHS4.BIN \
	    assets/OSETSF.BIN assets/OSEHF1.BIN assets/OSEHF2.BIN assets/OSEHF3.BIN assets/OSEHF4.BIN \
	    assets/OSEDEMO.BIN \
	    assets/OSEDEMOPJ.BIN assets/OSEDEMOSC.BIN assets/OSEDEMOCS.BIN assets/OSEDEMOCA.BIN \
	    assets/OSELOGOPJ.BIN assets/OSELOGOSC.BIN assets/OSELOGOCS.BIN assets/OSELOGOCA.BIN \
	    assets/LUDOTITLPJ.BIN assets/LUDOTITLSC.BIN assets/LUDOTITLCS.BIN assets/LUDOTITLCA.BIN \
	    assets/LUDOSCRMPJ.BIN assets/LUDOSCRMSC.BIN assets/LUDOSCRMCS.BIN assets/LUDOSCRMCA.BIN \
	    README.pdf README_fr.pdf
	@echo "Created build/$(ZIPNAME).zip"

# -------------------------------------------------------------------------
# Clean
# -------------------------------------------------------------------------

clean:
	$(DEL) build/$(MAIN).bin 2>$(NULLDEV) ; true
	$(DEL) build/$(MAIN).tap 2>$(NULLDEV) ; true
	$(DEL) build/$(MAIN)_fr.bin 2>$(NULLDEV) ; true
	$(DEL) build/$(MAIN)_fr.tap 2>$(NULLDEV) ; true
