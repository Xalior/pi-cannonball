# Changelog

Each version is a proof of concept against an agreed spec. Commit references
name the repository they live in: `pi-cannonball` for the product, and
`circle-libsdl2` for the SDL2 implementation it is built on.

## vPoC3 — unreleased

Cannonball is given a display size of its own, and drawing the picture takes
about half the processor time it did.

### Cannonball is told its display size, and it is not the screen's

The kernel now states the display size Cannonball is given: 796 by 448
pixels, which is the game's widescreen picture at the doubled internal
resolution it renders at. Every answer SDL returns to the game reports that
size, whatever the screen attached to the board is actually showing.
Cannonball draws at its own size and never learns the screen's.

This matters most on a Pi 5. That board's firmware chooses its display mode
before any kernel starts and will not change it afterwards, so a program
that takes its size from the screen gets whatever monitor happens to be
plugged in. Cannonball no longer does. Its own copy of the frame is now an
exact one-to-one copy rather than a resize, and the single resize left in
the chain is done by circle-libsdl2 on the processor core reserved for
putting frames on screen.

Confirmed on all three boards. On a Pi 5 driving a 1920x1080 screen, and on
a Pi 3 and a Pi 4 across several different displays, each filling the screen
with no display mode named in the boot configuration.

`pi-cannonball` 2fdb8f9 · `circle-libsdl2` b2eca5c, 6741ffe, 1c83bcf

### Drawing a frame takes about half the processor time

The code in circle-libsdl2 that resizes the picture used to reuse a
completed output row by copying it, rather than building it again from the
source. An output row is much wider than the source row it comes from, so
copying it reads far more memory than recalculating it does, and that
reading pushes the source out of the processor's cache.

With that removed, the core that puts frames on screen went from using 76%
of its time to 41%, measured on a Pi 5 at 1920x1080 with the frame rate
steady at 59.9 in both cases. The frame rate was already at its limit, so
this does not appear as a faster game. It appears as free time on that core,
which is what any future work on the picture would have to come out of.

`circle-libsdl2` b16ac3e

### A new card asks for hi-res rendering and the repaired sound samples

Hi-res doubles the resolution Cannonball renders at internally, which is
what makes its picture match the display size the kernel declares, so no
resizing happens inside the game. The repaired sample ROM replaces the
faulty one the original arcade machine shipped with.

This needs a ROM set that contains the repaired sample ROM. The game locates
each ROM by checksum rather than by filename, so the file may be named
anything. Without it, the ROM set fails to load and the game does not start.

Confirmed running from a card built this way on a Pi 3 and a Pi 4.

`pi-cannonball` 545d8a5

### Building a card is one command

`make card` assembles a complete card into `build/sd-card/` to copy onto
FAT32 media, or writes directly to a mounted card. It downloads the
Raspberry Pi firmware at the revision the kernel is built against and checks
every file against a recorded hash, so the same repository state always
produces the same card. The README previously described copying the files by
hand and did not mention the command.

The ROM set is still yours to supply; `roms/` is created empty.

`pi-cannonball` 9a0e036, ef7da49

## vPoC2 — 2026-07-30

Joypad control and full-screen output on every board.

### USB pads, wheels and game controllers work in the game

The SDL joystick and game-controller calls are implemented, so a device is
seen as it is plugged in and again as it is pulled out while the game runs.
Rumble works where the device has it. An unmodified `gamecontrollerdb.txt`
maps a recognised device, and a device the database has never heard of still
works as a plain joystick.

Proven on a Pi 5 with a USB steering wheel: analogue steering, D-pad,
buttons and hot-plug, with the game playable on it.

`pi-cannonball` 7733144 · `circle-libsdl2` d84ac51

### Changing a video setting no longer stops the machine

Turning widescreen off from the in-game menu used to take the board down.
The present path is sized by the framebuffer grant, which is made once and
never returned, so rebuilding it for each new window leaked a DMA channel
and two full-screen buffers every time. The pool drained until the sound
device could not open a channel and Circle halted. The sound device gives
its own channel back on the core that owns it now, for the same reason.

`pi-cannonball` 88d8773 · `circle-libsdl2` b497ee5, 71cd6de

### The picture fills the screen on every board

The card no longer asks for a screen size. Requesting one gave a Pi 5 a
framebuffer narrower than the surface being displayed, so the picture sat
unscaled in the top-left corner with the rest black. Asking for nothing
gives a framebuffer that matches the display, on all three boards and on
every screen tried.

`pi-cannonball` 6a0e661

### The board runs at full speed and cools itself

The library owns the CPU clock and the case fan. The clock is taken to its
maximum while the kernel is still building its world, so the card, the
filesystem and the console all come up at the speed the game will run at.
Where `cmdline.txt` names a fan pin with `gpiofanpin=`, a hot board spins
its fan and keeps its clock instead of slowing down and dropping frames.

`pi-cannonball` 6a0e661 · `circle-libsdl2` e017adb

### The framebuffer log line says what was granted

Only the pitch and the size come back from the firmware; the width, height,
virtual size and depth are what was asked for, echoed back by getters that
never learn what the firmware decided. Printed together and unlabelled they
read as one measured geometry, which is how a Pi 5 came to report a 640x480
framebuffer beside a pitch describing a 1920-wide surface. The two halves
are labelled now.

`pi-cannonball` c57e54d · `circle-libsdl2` 933a260

## vPoC1 — 2026-07-30

Cannonball playing on bare metal, on three boards, from one card.

### Boots straight into the game

The board powers on into a Circle kernel that brings up interrupts, timer,
serial console, SD card and scheduler, then calls Cannonball's entry point.
No operating system, no desktop, no launcher.

`pi-cannonball` 32745c2

### One card boots a Pi 3, a Pi 4 or a Pi 5

The card carries three kernel images and one `config.txt`. Raspberry Pi
firmware picks the image matching the board it finds itself on. Each board
is compiled against its own Circle world, so no board's objects can be
linked into another board's kernel.

`pi-cannonball` 32745c2, 2d88dca

### Sixty frames a second, with the work split across three cores

Core 0 owns the hardware, core 1 runs the game and nothing else, core 2
scales and presents. Getting there meant releasing the game core the moment
a frame is taken rather than after it is displayed, moving the screen
transfer to full-width bus bursts, handing the copy to the DMA engine on
boards that cannot page-flip, and composing frames in cached memory instead
of writing the uncached framebuffer three times over.

`circle-libsdl2` 2db0847, 6cf8768, 2686ac0, 1389432

### The game's picture is scaled to the display

Cannonball's native raster is a fraction of a modern screen. The shim
composes the game's frame, the requested canvas and the display's real
scanout into a single scaling pass at present time, rather than blitting
the raster unscaled into a corner.

`circle-libsdl2` e8f170b, 3c44d3c, 353139d, 3decf46, fadb54d, cb9ee81,
90b2445, ba7256b, d7edb16

### Sound over HDMI

The SDL audio callback API runs on Circle's HDMI sound device — 16-bit
stereo, the game's callback topping up a hardware queue. The card sets
Cannonball's sample rate to the 44100 the shim is proven at.

`circle-libsdl2` 71ee088

### A USB keyboard plays the game

USB HID keyboard input arrives as SDL key events.

`circle-libsdl2` 5b438dc

### The release is the card

CI builds all three boards, assembles the card from the results, and
attaches one zip — the staged card tree — to a draft release, and only for
a version tag on a commit that is on `main`. `tools/mkcard` fetches
Raspberry Pi firmware pinned to a revision, hash-verifies every file and
the Pi 4 boot stub, refuses to go on if a hash disagrees, and byte-compares
what landed on the card afterwards. The ROM set is never fetched and never
shipped; `roms/` is created empty and the build says so. Cannonball's own
source is a submodule pinned at upstream, unmodified.

`pi-cannonball` 2d88dca, 7bc418a, 20ad66e, b8fef0a, 31e623d

### A Pi 5 can network-boot instead

`make netboot` stages the Pi 5 image and its boot configuration for a TFTP
root, so the board runs with no card in it.

`pi-cannonball` 32745c2

### Boot options in cmdline.txt

`socmaxtemp=` sets the temperature the CPU clock is pulled back at.
`rapi-split=0` collapses the three-core split onto core 0 alone, so the
split and single-core paths can be measured against each other from one
image.

`pi-cannonball` 32745c2

### Known limitation: no game controllers

The SDL joystick, game-controller and haptic calls are stubbed to report no
devices. A pad, wheel or gamepad plugged into the board does nothing. The
game degrades cleanly rather than crashing, because Cannonball gates its
whole controller path behind the device count. Fixed in vPoC2.

`pi-cannonball` 32745c2 (`host/circle_stubs.cpp`)
