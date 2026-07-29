#
# pi-cannonball — Cannonball as a bootable bare-metal Raspberry Pi image.
#
#   make check-toolchain     report the cross compiler this build will use
#   make boost               check out the Boost headers Cannonball needs
#   make deps                boost, then the three circle-stdlib worlds and
#                            the shim archives built against them (long: the
#                            worlds build newlib and libc++ from source)
#   make rpi5 | rpi4 | rpi3  one board's kernel image
#   make kernels             all three, built in parallel
#   make verify              truth-gate: every image exists and is non-empty
#   make netboot             stage the Pi 5 image and its boot configuration
#                            into build/netboot-rpi5/
#   make card                stage the whole card, except the ROMs, into
#                            build/sd-card/
#   make clean-boards        drop every board's build tree
#
# The three boards never share mutable state: each has its own circle-stdlib
# world, its own shim archive and its own object directory, so building them
# at the same time is safe and building one never disturbs another.
#

include mk/toolchain.mk

# Stated explicitly because the first rule this file sees comes from an
# included makefile, and that would otherwise decide the default goal.
.DEFAULT_GOAL := kernels

BOARDS ?= rpi3 rpi4 rpi5

IMAGE_rpi3 = kernel8.img
IMAGE_rpi4 = kernel8-rpi4.img
IMAGE_rpi5 = kernel_2712.img

# Boost is used header-only — CRC checking of the ROM set, reading and
# writing config.xml, one string predicate and one static assertion — so
# nothing of it is ever compiled. These are the library repositories the
# Boost superproject pins that those four headers actually pull in, worked
# out by compiling against them until nothing was missing. Adding a Boost
# header to the port may add a name here; nothing else needs to change,
# because the include path is every checked-out library's include/.
BOOST_LIBS = \
	algorithm any array assert bind concept_check config container_hash \
	core crc integer iterator move mp11 mpl multi_index optional \
	preprocessor property_tree range smart_ptr static_assert \
	throw_exception tuple type_index type_traits utility

.PHONY: boost deps kernels verify netboot card clean-boards $(BOARDS)

boost:
	@for lib in $(BOOST_LIBS); do \
		[ -d boost/libs/$$lib/include ] || { \
			echo "  BOOST init libs/$$lib"; \
			git -C boost submodule update --init --depth 1 libs/$$lib || exit 1; }; \
	done
	@echo "  BOOST $$(ls -d boost/libs/*/include | wc -l | tr -d ' ') header libraries checked out"

deps: boost
	$(MAKE) -C circle-libsdl2 deps

$(BOARDS): check-toolchain
	$(MAKE) -C host RAPI_BOARD=$@

# All three at once. Each sub-make owns a different world and a different
# output directory, so there is nothing for them to collide on.
kernels: check-toolchain
	@for b in $(BOARDS); do $(MAKE) -C host RAPI_BOARD=$$b & done; wait

# Truth-gate: ask the filesystem, not the exit codes. An image that is
# missing or empty fails here even if the build claimed success.
verify:
	@fail=0; \
	for b in $(BOARDS); do \
		case $$b in \
			rpi3) img=host/build/rpi3/$(IMAGE_rpi3) ;; \
			rpi4) img=host/build/rpi4/$(IMAGE_rpi4) ;; \
			rpi5) img=host/build/rpi5/$(IMAGE_rpi5) ;; \
		esac; \
		if [ -s "$$img" ]; then \
			echo "  OK    $$img ($$(wc -c < $$img | tr -d ' ') bytes)"; \
		else \
			echo "  FAIL  $$img missing or empty"; fail=1; \
		fi; \
	done; \
	exit $$fail

# The Pi 5 netboot bundle: the image the Pi 5 firmware looks for, plus the
# boot configuration it must be served alongside. Copy the contents into the
# TFTP root the board boots from (the Raspberry Pi firmware files themselves
# come from that root's existing installation, not from here).
NETBOOT_DIR = build/netboot-rpi5
netboot: rpi5
	@mkdir -p $(NETBOOT_DIR)
	@cp host/build/rpi5/$(IMAGE_rpi5) $(NETBOOT_DIR)/
	@cp host/config.txt host/cmdline.txt $(NETBOOT_DIR)/
	@echo "  STAGED $(NETBOOT_DIR)/"
	@ls -l $(NETBOOT_DIR)/

# The card, staged into a directory to copy onto media formatted elsewhere:
# firmware, the three kernels, boot configuration, the game's resources and
# its config.xml. The ROM set is not here and is not ours to ship — mkcard
# leaves roms/ empty and says so.
CARD_DIR = build/sd-card
card: kernels
	@rm -rf $(CARD_DIR)
	@tools/mkcard --stage $(CARD_DIR)

clean-boards:
	@for b in $(BOARDS); do $(MAKE) -C host RAPI_BOARD=$$b clean-board; done
	rm -rf $(NETBOOT_DIR)
