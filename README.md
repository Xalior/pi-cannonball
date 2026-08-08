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

The game draws at its own resolution and the picture is scaled once onto
whatever your screen actually is.

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

```sh
make card
```

That builds the card into `build/sd-card/` for you to copy onto FAT32 media.
It fetches the Raspberry Pi firmware at the revision Circle is built against
and checks every file against a hash, stages the three kernel images under the
names each board's firmware looks for, and writes the boot configuration and
the game's `config.xml` and `res/` files. Given a mounted FAT32 volume instead,
`tools/mkcard /Volumes/YOUR-CARD` writes straight to it.

The same repository state always produces the same card, and the script ends
by reading back what actually landed rather than trusting that the copies
worked.

**The ROMs are not there and cannot be.** Cannonball needs the original arcade
ROM set, which is copyrighted and is not this project's to distribute, so
`roms/` is created empty. Supply them and the card boots: the game identifies
each ROM by checksum as it starts, so a renamed set still loads.

### The fan pin in `cmdline.txt`

One card boots any of the three boards, so all three read the same
`cmdline.txt`. It carries `gpiofanpin=45`, which is the pin a Raspberry Pi 5
Case Fan or Active Cooler is wired to. Naming a fan pin changes what happens
when the board gets hot: the fan is switched on and the processor is left at
full speed, instead of the processor being slowed down to cool itself. That
is what a game wants — a slowed processor drops frames.

`socmaxtemp=70` is the temperature in degrees Celsius at which that happens.

On a Pi 3 or a Pi 4 the pin number depends on how you wired your own fan, so
change `gpiofanpin=` to match it. With no fan fitted at all, remove the option
and the board cools itself by slowing down.

## What works

Fullscreen software rendering at 60fps, HDMI audio, USB keyboards, and USB
pads, wheels and game controllers, on all three boards. The picture is scaled
to the display by the SDL layer, so the game runs at its own native resolution
regardless of what the screen is.

Controllers are picked up as they are plugged in and released as they are
pulled out, while the game is running. The card carries an unmodified
`gamecontrollerdb.txt`, so a device that database recognises is mapped for
you; one it has never heard of still works as a plain joystick.

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
