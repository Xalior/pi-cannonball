# Changelog

Each version is a proof of concept against an agreed spec. Commit references
name the repository they live in: `pi-cannonball` for the product, and
`circle-libsdl2` for the SDL2 implementation it is built on.

## vPoC2 — unreleased

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
