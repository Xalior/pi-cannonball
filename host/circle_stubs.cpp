//
// circle_stubs.cpp — the SDL2 entry points Cannonball references that
// circle-libsdl2 does not implement yet.
//
// Every function here fails honestly rather than pretending to work: device
// enumerations report nothing, opens return null, queries return an error
// code. That is what keeps them unreachable at runtime. Cannonball guards
// its whole joystick, game-controller and force-feedback path behind
// SDL_NumJoysticks() > pad_id (src/main/sdl2/input.cpp), so a joystick
// count of zero disables the family cleanly, and its WAV music loader
// treats a failed load as "no custom music" and plays the emulated
// soundtrack instead.
//
// These are seams, not permanent furniture. circle-libsdl2 is growing real
// game-controller and mouse support, and this port is the program that will
// exercise it: when the shim implements one of these for real, the way to
// adopt it is to DELETE the stub here. An object file linked directly into
// the kernel beats an archive member, so a stub left behind would silently
// win over the working implementation.
//
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include <SDL2/SDL.h>

extern "C" {

// ---- joystick ---------------------------------------------------------------

int SDL_NumJoysticks(void) { return 0; }
SDL_Joystick *SDL_JoystickOpen(int) { return nullptr; }
void SDL_JoystickClose(SDL_Joystick *) {}

// ---- game controller --------------------------------------------------------

SDL_bool SDL_IsGameController(int) { return SDL_FALSE; }
SDL_GameController *SDL_GameControllerOpen(int) { return nullptr; }
void SDL_GameControllerClose(SDL_GameController *) {}

// Cannonball loads res/gamecontrollerdb.txt at startup and only reports the
// failure. It calls SDL_GameControllerAddMappingsFromFile, which is a macro
// over this function and SDL_RWFromFile, so this is the name that has to
// exist. -1 is the "no mappings added" answer.
int SDL_GameControllerAddMappingsFromRW(SDL_RWops *, int) { return -1; }

// ---- force feedback ---------------------------------------------------------

SDL_Haptic *SDL_HapticOpen(int) { return nullptr; }
void SDL_HapticClose(SDL_Haptic *) {}
int SDL_HapticRumbleSupported(SDL_Haptic *) { return SDL_FALSE; }
int SDL_HapticRumbleInit(SDL_Haptic *) { return -1; }
int SDL_HapticRumblePlay(SDL_Haptic *, float, Uint32) { return -1; }
int SDL_HapticRumbleStop(SDL_Haptic *) { return -1; }

// ---- WAV loading and audio conversion ---------------------------------------
//
// Used only for user-supplied WAV music files. The emulated soundtrack
// (YM2151 plus PCM samples) does not pass through any of this.

SDL_RWops *SDL_RWFromFile(const char *, const char *) { return nullptr; }

SDL_AudioSpec *SDL_LoadWAV_RW(SDL_RWops *, int, SDL_AudioSpec *, Uint8 **,
                              Uint32 *)
{
    SDL_SetError("WAV loading is not available");
    return nullptr;
}

void SDL_FreeWAV(Uint8 *audio_buf) { free(audio_buf); }

void SDL_MixAudioFormat(Uint8 *, const Uint8 *, SDL_AudioFormat, Uint32, int) {}

int SDL_BuildAudioCVT(SDL_AudioCVT *, SDL_AudioFormat, Uint8, int,
                      SDL_AudioFormat, Uint8, int)
{
    SDL_SetError("audio conversion is not available");
    return -1;
}

int SDL_ConvertAudio(SDL_AudioCVT *)
{
    SDL_SetError("audio conversion is not available");
    return -1;
}

// ---- miscellaneous ----------------------------------------------------------

// Only reached on Cannonball's Linux branch, which SDL_GetPlatform()
// answering "Circle" already keeps this build out of.
int SDL_AudioInit(const char *) { return -1; }

// There is no mouse cursor to show or hide.
int SDL_ShowCursor(int) { return SDL_DISABLE; }

// stdout is the serial console.
void SDL_Log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    putchar('\n');
}

} // extern "C"
