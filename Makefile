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
  -dVERSION_PATCH=$(VERSION_PATCH)

# -------------------------------------------------------------------------
# Source dependency lists
# Oscar64 follows #pragma compile chains internally; make does not --
# list everything here so rebuilds trigger correctly.
# -------------------------------------------------------------------------

MAIN_SRCS = \
  src/main.c            \
  src/strings.h         \
  include/oric_crt.c    \
  include/crt_math.c    \
  include/oric.h        \
  include/charwin.c      \
  include/charwin.h      \
  include/keyboard.c     \
  include/keyboard.h     \
  include/ijk.c          \
  include/ijk.h          \
  include/loci.c         \
  include/loci.h

# -------------------------------------------------------------------------
# Emulator flags
# -------------------------------------------------------------------------

EMUFLAG = -ma --serial none --vsynchack off --turbotape on

# =========================================================================
# Targets
# all: must appear first so it is the default goal
# =========================================================================

.PHONY: all clean run docs

all: build/$(MAIN).tap

# Step 1: compile main app to raw binary
build/$(MAIN).bin: $(MAIN_SRCS)
	@$(MKDIR) build 2>$(NULLDEV) ; true
	$(CC) $(CFLAGS) -o=build/$(MAIN).bin src/main.c

# Step 2: wrap binary in Oric tape header
build/$(MAIN).tap: build/$(MAIN).bin
	$(PY) tools/mktap.py \
	    build/$(MAIN).bin \
	    build/$(MAIN).tap \
	    $(PROGNAME) \
	    $(LOAD_ADDR)

# Launch in Oricutron (must cd to oricutron dir -- it loads ROMs from cwd)
run: build/$(MAIN).tap
	cd $(ORICUTRON_HOME) && \
	    $(EMUL) $(EMUFLAG) "$(CURDIR)/build/$(MAIN).tap"

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
