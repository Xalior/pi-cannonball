//
// ctor_marker.cpp — static-constructor bisection marker.
//
// Compiled once per marker with -DMARK_ID=<n>, and interleaved between
// Cannonball's objects on the link line. Static constructors run in link
// order, so the last marker that reaches the UART names the object whose
// constructor the boot died in. The serial device is polled — no interrupt
// system exists this early — and deliberately re-initialized later by the
// kernel proper.
//
// MARK_SILENT: no serial, no hardware — the constructor only increments a
// counter the stub logs after boot. Distinguishes "a marker's serial write
// kills the boot" from "many constructors kill the boot".
#ifndef MARK_SILENT
#include <circle/serial.h>
#endif

#ifndef MARK_ID
#define MARK_ID 0
#endif

#define XSTR(x) STR(x)
#define STR(x) #x

#ifdef MARK_SILENT

extern int g_nCtorMarks;   // defined in stub_cannonball.cpp, logged after boot

namespace
{
struct SCtorMark
{
    SCtorMark(void) { g_nCtorMarks++; }
};
static SCtorMark s_Mark;
}

#else

static void MarkWrite(const char *pMsg, size_t nLen)
{
    static CSerialDevice s_Serial(0, FALSE, 0);
    static bool s_bInit = false;
    if (!s_bInit)
    {
        s_Serial.Initialize(115200);
        s_bInit = true;
    }
    s_Serial.Write(pMsg, nLen);
}

namespace
{
struct SCtorMark
{
    SCtorMark(void)
    {
        static const char Msg[] = "<M" XSTR(MARK_ID) ">";
        MarkWrite(Msg, sizeof(Msg) - 1);
    }
};
static SCtorMark s_Mark;
}

#endif
