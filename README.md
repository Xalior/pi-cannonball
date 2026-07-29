# pi-cannonball

**Cannonball — the OutRun engine — running directly on a Raspberry Pi with no
operating system.** The board powers on and the game is what boots: no Linux,
no desktop, no launcher, and nothing else running beside it.

It builds for the Raspberry Pi 3, Pi 4 and Pi 5, all three from one source
tree, and holds 60 frames per second on every one of them.

## What this is

[Cannonball](https://github.com/djyt/cannonball) is an ordinary SDL2
application. This repository is the thin layer that lets it run with nothing
underneath: a [Circle](https://github.com/rsta2/circle) kernel that brings the
board up, and [circle-libsdl2](https://github.com/Xalior/circle-libsdl2), an
SDL2 implementation built on Circle's bare-metal drivers.

The game's own source is not copied or modified here. It is a submodule,
pinned at an upstream commit, and the build reads it without ever writing to
it.

Three processor cores are given separate work:

- **Core 0** owns the hardware. Circle's world lives here — interrupts, USB,
  the SD card, sound — and no other core touches a device.
- **Core 1** runs the game and nothing else.
- **Core 2** puts finished frames on the screen: the game's own 398x224 picture
  arrives as pixels and is scaled once, at the end, to whatever the display is
  showing.

## Building

You need a Linux or macOS machine, GNU make, and the Arm GNU toolchain for
`aarch64-none-elf` (release 15.2.Rel1). Put its `bin` directory on your `PATH`,
or unpack it into `toolchains/` in this repository.

```sh
git clone --recursive https://github.com/Xalior/pi-cannonball.git
cd pi-cannonball
make deps       # long: builds newlib and libc++ from source, once per board
make kernels    # the three board images
make verify     # confirms each image exists and is not empty
```

`make deps` is the slow step, and it is slow once. It checks out the Boost
headers Cannonball needs, then builds a complete C and C++ world for each
board, because each board's world is compiled for its own processor.

The images land in `host/build/<board>/`:

| Board | Image |
|---|---|
| Pi 3 | `host/build/rpi3/kernel8.img` |
| Pi 4 | `host/build/rpi4/kernel8-rpi4.img` |
| Pi 5 | `host/build/rpi5/kernel_2712.img` |

## Putting it on a card

Format an SD card as FAT32 and copy onto it:

- The Raspberry Pi firmware files (`bootcode.bin`, `start*.elf`, `fixup*.dat`
  and the device trees) from the
  [firmware repository](https://github.com/raspberrypi/firmware).
- The kernel images above, keeping their names, so one card boots any board.
- `host/config.txt` and `host/cmdline.txt`.
- Cannonball's `config.xml`, its `roms/` directory and its `res/` files.

**The ROMs are not here and cannot be.** Cannonball needs the original arcade
ROM set, which is copyrighted; the build checks what you supply against the
CRCs the game itself expects.

## What works

Fullscreen software rendering at 60fps, HDMI audio, and USB keyboards, on all
three boards. The picture is scaled to the display by the SDL layer, so the
game runs at its own native resolution regardless of what the screen is.

There is no GPU driver on bare metal, so everything is drawn by the processor.
That is the design rather than a limitation: it is what makes one build run
across three generations of board.

## License

The code in this repository — the kernel layer in `host/` and the build — is
released under the GNU Lesser General Public License, version 3. See
[LICENSE](LICENSE).

The submodules are other people's work and carry their own terms. Two of them
matter before you distribute anything you build here:

- **Cannonball** is released under a custom license that permits
  redistribution and modification but **forbids selling it or using it in a
  commercial product**, and requires complete source with any modified
  redistribution. It is not an OSI-approved open-source license, and that
  restriction travels into any image built from it.
- **Circle** is released under the GNU General Public License, version 3.

Building a kernel image here combines all three. Doing that for yourself is
straightforward; redistributing the result means satisfying every one of those
terms at once, and Cannonball's non-commercial clause is the binding one.

OutRun is a trademark of SEGA. This project is not affiliated with SEGA.
