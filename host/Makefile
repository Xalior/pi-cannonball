#
# Cannonball as a bootable Raspberry Pi kernel image.
#
# One board per invocation (RAPI_BOARD, default rpi5). The board selects
# three things at once: the circle-stdlib world the kernel is compiled and
# linked against, the shim archive built against that world, and — through
# the world's Config.mk — the CPU tuning and RASPPI value. Each board's
# newlib and libc++ are built for its own architecture, so an image is
# never portable between boards.
#
#   make                     the default board's image
#   make RAPI_BOARD=rpi3     that board's image
#   make clean-board         drop just this board's build/<board>/ tree
#
# Everything this invocation generates lands under build/<board>/: the
# fixed-name kernel objects, every Cannonball object, and the final
# .elf/.map/.lst/.img. The object names are shared across boards but their
# contents are not (different -mcpu, different -DRASPPI, different world),
# so a flat build directory would silently relink one board's objects into
# another board's kernel — a corrupt image with no build error. A directory
# per board keeps all three isolated: no `make clean` between boards, and
# three boards can build at the same time without touching each other.
#
# Cannonball's own source tree (../cannonball) is never written into: it is
# read-only input, and every object it produces is redirected here.
#
# Prerequisite: the board's world and shim archive exist —
# `make -C ../circle-libsdl2 deps` — and Boost's headers are checked out
# (`make boost` at the repo root).
#

include ../mk/toolchain.mk

RAPI_BOARD ?= rpi5

include ../circle-libsdl2/circle-stdlib-$(RAPI_BOARD)/Config.mk

ROOT       = ..
SHIM       = $(ROOT)/circle-libsdl2
CANNONBALL = $(ROOT)/cannonball/src/main
BOOST      = $(ROOT)/boost

OBJDIR = build/$(RAPI_BOARD)

# Circle's own image names, which the Raspberry Pi firmware selects between
# from the model sections of config.txt: the Pi 3 world boots kernel8.img,
# the Pi 4 world kernel8-rpi4.img, the Pi 5 world kernel_2712.img.
IMAGE_rpi3 = kernel8
IMAGE_rpi4 = kernel8-rpi4
IMAGE_rpi5 = kernel_2712
IMAGE = $(IMAGE_$(RAPI_BOARD))
ifeq ($(IMAGE),)
$(error unknown RAPI_BOARD '$(RAPI_BOARD)' — expected rpi3, rpi4 or rpi5)
endif

TARGET = $(OBJDIR)/$(IMAGE)

# Stated explicitly because the first rule this file sees comes from an
# included makefile, and that would otherwise decide the default goal.
.DEFAULT_GOAL := image

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------

# The kernel wrapper, the syscall layer and the stub file: this project's
# own code.
HOST_SRCS = main.cpp kernel.cpp defaults.cpp circle_syscalls.cpp circle_stubs.cpp
HOST_OBJS = $(addprefix $(OBJDIR)/,$(HOST_SRCS:.cpp=.o))

# Cannonball's source list, as its own CMakeLists.txt enumerates it, minus
# the two OpenGL renderers. Neither can be built here — there is no bare-metal
# GPU driver — so the software renderer (rendersurface.cpp) is the one that
# compiles, which is also the one this shim implements the SDL2 surface for.
CB_SRCS = \
	main.cpp romloader.cpp trackloader.cpp roms.cpp video.cpp utils.cpp \
	frontend/cabdiag.cpp frontend/config.cpp frontend/menu.cpp \
	frontend/ttrial.cpp \
	hwvideo/hwroad.cpp hwvideo/hwsprites.cpp hwvideo/hwtiles.cpp \
	hwaudio/segapcm.cpp hwaudio/soundchip.cpp hwaudio/ym2151.cpp \
	sdl2/audio.cpp sdl2/timer.cpp sdl2/input.cpp sdl2/renderbase.cpp \
	sdl2/rendersurface.cpp \
	directx/ffeedback.cpp \
	engine/oanimseq.cpp engine/oattractai.cpp engine/obonus.cpp \
	engine/ocrash.cpp engine/oferrari.cpp engine/ohiscore.cpp \
	engine/ohud.cpp engine/oinitengine.cpp engine/oinputs.cpp \
	engine/olevelobjs.cpp engine/ologo.cpp engine/omap.cpp \
	engine/omusic.cpp engine/ooutputs.cpp engine/opalette.cpp \
	engine/oroad.cpp engine/osmoke.cpp engine/osprite.cpp \
	engine/osprites.cpp engine/ostats.cpp engine/otiles.cpp \
	engine/otraffic.cpp engine/outils.cpp engine/outrun.cpp \
	engine/audio/osound.cpp engine/audio/osoundint.cpp

CB_OBJS = $(addprefix $(OBJDIR)/cb/,$(CB_SRCS:.cpp=.o))

OBJS = $(HOST_OBJS) $(CB_OBJS)

STANDARD = -std=gnu++17

# Stated, not inherited: this is Circle's own default, written down so the
# build's optimization level is a fact in the file rather than a forensic
# reconstruction. Bench-measured on the Pi 5: -O3 is indistinguishable
# (the present path is memory bandwidth), and a true -O0 does not survive
# this world at all.
OPTIMIZE = -O2

include $(CIRCLEHOME)/Rules.mk

# The world must be the multicore one circle-libsdl2 configures: the shim's
# core-split machinery cannot exist without it, and an object compiled
# against a single-core world links without complaint and then misbehaves.
# Every object here inherits the flag through Rules.mk's CPPFLAGS, so this
# check covers the whole build.
ifeq ($(findstring ARM_ALLOW_MULTI_CORE,$(DEFINE)),)
$(error the payload requires the multicore circle-stdlib world — build it with 'make -C ../circle-libsdl2 deps')
endif

# Boost is header-only here: nothing of it is ever compiled or linked. Each
# Boost library repository publishes its headers under its own include/
# directory, so the include path is simply every one that is checked out.
BOOST_INCLUDES = $(addprefix -I ,$(wildcard $(BOOST)/libs/*/include))

# Cannonball writes `#include <SDL.h>`, the spelling a desktop sdl2-config
# include path provides, so the shim's SDL2/ directory goes on the path in
# its own right alongside the SDL2/-prefixed one this project's own code uses.
INCLUDE := -I $(SHIM)/include -I $(SHIM)/include/SDL2 -I $(CANNONBALL) \
	$(BOOST_INCLUDES) $(CIRCLE_STDLIB_INCLUDES) $(INCLUDE)

# The shim comes in whole, every member, rather than only the members the
# link happens to need. An object file linked straight into the kernel beats
# an archive member of the same name and does it silently, so a stub in
# circle_stubs.cpp left behind after the library implemented the real thing
# would quietly win — the game would keep calling the stub and nothing would
# say so. Pulling the archive in whole turns that into a duplicate-symbol
# error at link time, which is the only moment it can still be cheap to fix.
LIBS = --whole-archive $(SHIM)/libSDL2-$(RAPI_BOARD).a --no-whole-archive \
	$(CIRCLE_STDLIB_LIBS)

# ---------------------------------------------------------------------------
# Filesystem syscalls, redirected at link time
# ---------------------------------------------------------------------------
#
# The game runs on the application core, and newlib's Circle glue drives
# FatFs and the SD card straight from wherever it is called — which off core
# 0 is illegal. circle_syscalls.cpp puts the shim's any-core I/O service
# underneath the C library instead; these flags are what put it there.
#
# --wrap rather than redefinition, because redefinition cannot work here:
# the glue defines _open, _read, _write, _close, _lseek, _fstat, opendir,
# readdir, closedir, dup and more in a SINGLE object file. Redefining part
# of that set either collides at link time or, worse, leaves the originals
# linked in beside the replacements with no diagnostic. --wrap renames the
# references instead and leaves the vendored glue untouched — and it is what
# keeps __real_* reachable, which is how the service performs the real work
# once it has reached core 0.
#
# Every name here must also appear in circle_syscalls.cpp: a wrap with no
# __wrap_ definition is an undefined reference, so the link is the check.
WRAPPED_SYSCALLS = \
	_open _close _read _write _lseek _fstat _stat lstat _unlink _rename \
	_fcntl mkdir chdir getcwd dup dup2 \
	opendir readdir closedir rewinddir

# The link is driven by the cross ld directly (Circle's Rules.mk sets
# LD = <prefix>ld for GCC builds), so these are plain linker options, not
# compiler-driver -Wl, ones.
LDFLAGS += $(addprefix --wrap=,$(WRAPPED_SYSCALLS))

# ---------------------------------------------------------------------------
# Compilation
# ---------------------------------------------------------------------------

# This project's own objects, board-scoped. These more-specific rules win
# over Rules.mk's in-place %.o/%.d, so nothing is written beside the source.
$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

$(OBJDIR)/%.d: %.cpp | $(OBJDIR)
	@$(CPP) $(CPPFLAGS) -M -MG -MT $(@:.d=.o) -MT $@ -MF $@ $<

# Cannonball's objects, mirroring its source tree under $(OBJDIR)/cb/. The
# source tree itself is a submodule holding the fork, and is never written to.
$(OBJDIR)/cb/%.o: $(CANNONBALL)/%.cpp
	@mkdir -p $(dir $@)
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

$(OBJDIR)/cb/%.d: $(CANNONBALL)/%.cpp
	@mkdir -p $(dir $@)
	@$(CPP) $(CPPFLAGS) -M -MG -MT $(@:.d=.o) -MT $@ -MF $@ $<

# Cannonball's main() is the program's entry point on a desktop, but here
# main() belongs to the Circle kernel (main.cpp in this directory), which
# calls Cannonball once the board is up. Renaming the entry point through
# the preprocessor for this one translation unit keeps the fork's own
# source untouched: the declaration in main.hpp and the definition in
# main.cpp are renamed together, and the kernel calls the new name.
$(OBJDIR)/cb/main.o: $(CANNONBALL)/main.cpp
	@mkdir -p $(dir $@)
	@echo "  CPP   $@ (entry point renamed)"
	@$(CPP) $(CPPFLAGS) -Dmain=cannonball_main -c -o $@ $<

$(OBJDIR)/cb/main.d: $(CANNONBALL)/main.cpp
	@mkdir -p $(dir $@)
	@$(CPP) $(CPPFLAGS) -Dmain=cannonball_main -M -MG -MT $(@:.d=.o) -MT $@ -MF $@ $<

$(OBJDIR):
	@mkdir -p $(OBJDIR)

-include $(DEPS)

# The image link rule. sdl-app.mk lets the caller choose the linker script,
# so this build supplies its own: the shim's script with room made at image
# offset 0x800 for the patchable-defaults block. Everything else about the
# script — including the TLS-section adjacency binutils 2.44 and later
# require — is inherited from the shim's.
SDL_APP_LDSCRIPT = $(CURDIR)/cannonball-defaults.ld
include $(SHIM)/sdl-app.mk

# The other half of the guarantee the linker script's ASSERTs begin: check
# the BYTES of the finished image, not just the link. A pre-boot writer
# refuses to stamp an image whose magic is not at 0x800, so an image that
# lost it would boot and then quietly ignore everything ever sent to it.
# Better to have no image than that one.
.PHONY: image
image: $(TARGET).img
	@if [ "`dd if=$(TARGET).img bs=4 skip=512 count=1 2>/dev/null`" = "PM8D" ]; \
	then \
		echo "  GATE  $(TARGET).img defaults block at 0x800: present"; \
	else \
		echo "  GATE  $(TARGET).img: defaults block ABSENT at 0x800 — image deleted"; \
		rm -f $(TARGET).img; \
		exit 1; \
	fi

# Board-scoped clean: drop just this board's tree, leaving the other two
# boards' images and objects untouched.
.PHONY: clean-board
clean-board:
	rm -rf $(OBJDIR)
