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
  include/loci.h         \
  assets/OSEforLOCI-Help1.bin \
  assets/OSEforLOCI-Help2.bin \
  assets/OSEforLOCI-Help3.bin \
  assets/OSEforLOCI-Help4.bin \
  assets/OSEforLOCI-Title.bin

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

-include .env

PHOSDIR  ?= NOT_SET
PHOS      = $(PHOSDIR)/oric1-emu
ATMOSROM  = $(PHOSDIR)/roms/basic11b.rom

CYCLES   ?= 8000000

# =========================================================================
# Targets
# all: must appear first so it is the default goal
# =========================================================================

.PHONY: all all-langs clean run docs check-phosphoric sandbox-reset test-capture test-boot test-menus test-screenresize test-charsetram-spike test-charsetedit test-palette test-colourpicker test-cursor-autoscroll test-linebox test-select test-move test-writemode test-boot-no-loci test-select-cutcopy test-undo-overflow test-help-funct8 test

all: build/$(MAIN)$(LANGSUFFIX).tap

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

# Build both language variants
all-langs:
	$(MAKE) LANG=EN
	$(MAKE) LANG=FR

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
	exit $$status

# -------------------------------------------------------------------------
# Documentation -- generate PDF from Markdown (requires pandoc)
# README.pdf is committed to git so it ships in release ZIPs even if the
# recipient does not have pandoc installed.
# -------------------------------------------------------------------------

docs: README.pdf

README.pdf: README.md
	@if which pandoc >/dev/null 2>&1; then \
	    pandoc README.md -o README.pdf; \
	else \
	    echo "WARNING: pandoc not found -- README.pdf not updated (install: sudo apt install pandoc texlive-xetex)"; \
	fi

# -------------------------------------------------------------------------
# Clean
# -------------------------------------------------------------------------

clean:
	$(DEL) build/$(MAIN).bin 2>$(NULLDEV) ; true
	$(DEL) build/$(MAIN).tap 2>$(NULLDEV) ; true
	$(DEL) build/$(MAIN)_fr.bin 2>$(NULLDEV) ; true
	$(DEL) build/$(MAIN)_fr.tap 2>$(NULLDEV) ; true
