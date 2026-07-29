//
// stub_cannonball.cpp — boot-bisection payload.
//
// Links in place of Cannonball's objects (make CB_OBJS=.../stub_cannonball.o)
// so the image is the host scaffolding alone: same kernel wrapper, same
// world, same link recipe, none of Cannonball's code or static constructors.
// If this image logs, the scaffolding is proven and the fault is in the
// payload; if it stays silent, the fault is in the scaffolding or the build.
//
#include <circle/logger.h>

// The cannonball:: globals live in the real main.cpp, which this stub
// displaces; the other translation units reference them, so the stub must
// carry them for the link. Audio's constructor runs as a static ctor here,
// exactly as it would in the real image. STUB_BARE omits them, for images
// that link no Cannonball objects at all.
#ifndef STUB_BARE
#include "main.hpp"
namespace cannonball
{
    Audio audio;
    int frame;
    bool tick_frame;
    double frame_ms;
    int fps_counter;
    int state;
}
#endif

// STUB_ALLOC_PROBE: does a static constructor that allocates survive this
// world? Cannonball's global TrackLoader does exactly this (new Level[]).
// The value is read back after boot so the allocation is proven, not
// merely survived.
#ifdef STUB_ALLOC_PROBE
static int *s_pProbe;
namespace
{
struct SAllocProbe
{
    SAllocProbe(void) { s_pProbe = new int[16]; s_pProbe[0] = 42; }
};
static SAllocProbe s_Probe;
}
#endif

// Incremented by MARK_SILENT ctor markers; logged so the count of
// constructors that actually ran is bench-visible.
int g_nCtorMarks;

int cannonball_main(int argc, char *argv[])
{
    CLogger::Get()->Write("stub", LogNotice,
                          "stub payload reached: host scaffolding boots");
    CLogger::Get()->Write("stub", LogNotice, "ctor markers ran: %d", g_nCtorMarks);
#ifdef STUB_ALLOC_PROBE
    CLogger::Get()->Write("stub", LogNotice,
                          "static-ctor allocation survived: %d", s_pProbe[0]);
#endif
    return 0;
}
