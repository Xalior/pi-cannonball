# Changelog

Each version is a proof of concept against an agreed spec. Commit references
name the repository they live in: `pi-cannonball` for the product, and
`circle-libsdl2` for the SDL2 implementation it is built on.

## vPoC3 — unreleased

The game is given a display of its own, and the picture costs half of what
it did.

### The game is told what display it has, and it is not the panel

The kernel states the display the game runs on — 796x448, OutRun's
widescreen raster at the doubled internal resolution — and every answer SDL
gives the game reports that, whatever the screen is really showing. The game
draws at its own size, and the SDL layer carries the finished frame to the
panel in one pass on the presentation core.

This is what a Pi 5 needs. That board's firmware settles its display mode
before any kernel runs and cannot be moved off it, so a game that sizes
itself from the screen is at the mercy of whatever monitor is plugged in.
Now it is not: the same build fills a 1080p television and a 4K one
identically, and the game's own copy of the frame is unscaled on every
board.

`pi-cannonball` 2fdb8f9 · `circle-libsdl2` b2eca5c, 6741ffe, 1c83bcf

### The picture costs half the work it used to

The SDL layer's scaler had an optimisation that read finished rows back out
of the screen buffer instead of building them again from the source. Reading
the wide side back costs more than recomputing it from the narrow one, and
it evicted the source from cache while it did so.

Removing it halved what the presentation core spends putting a frame on a
1080p screen. The frame rate was already at its ceiling, so this shows as
headroom rather than speed — but it is headroom on the core that would pay
for anything added to the picture later.

`circle-libsdl2` b16ac3e

### Hi-res rendering and the repaired sound samples are on

A fresh card asks for the doubled internal resolution, which is what makes
the game's frame land on the declared display without being resized, and for
the repaired PCM sample ROM in place of the corrupt one the original arcade
board shipped with. Supply a ROM set containing it and the samples play as
they were meant to; the game finds it by checksum, so its filename does not
matter.

`pi-cannonball` 545d8a5

### Building the card is one command

`make card` stages the whole card into `build/sd-card/`, or writes straight
to a mounted volume. It fetches the firmware at the revision the kernel is
built against and checks every file against a hash, so the same repository
state always produces the same card. The README described copying files by
hand and named neither.

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
