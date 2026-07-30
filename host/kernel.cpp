//
// kernel.cpp — brings the board up, then hands control to Cannonball.
//
// Cannonball is an ordinary command-line program: it expects a working
// standard library, a filesystem holding its ROM set and resources, and an
// SDL2 implementation. This file supplies the first two and starts the
// third, then calls Cannonball's entry point with a fixed argument list.
//
// The argument list names the configuration file by absolute path. Every
// other path Cannonball uses — the ROM directory, the resource directory,
// where high scores are written — is read out of that file at runtime, so
// the card decides them and this kernel does not. They must be absolute
// paths in the file: a bare-metal program has no shell to give it a
// meaningful working directory.
//
// This kernel also decides the core layout (see kernel.h for the roles) and
// hands one core to the shim's presentation worker. The library never
// starts a core; electing one is the host's job, and this is where it
// happens. Cannonball itself knows none of it: it calls plain SDL, and its
// file access reaches the marshalled I/O service through the syscall layer
// in circle_syscalls.cpp.
//
#include "kernel.h"
#include "defaults.h"
#include "defaultsblock.h"
#include <circle/startup.h>
#include <circle/machineinfo.h>
#include <SDL2/SDL_circle.h>
#include <atomic>

// Cannonball's entry point. It is main() in the fork's own source; the build
// renames it for that one translation unit, because main() here belongs to
// the Circle kernel. Declared as ordinary C++ and not as extern "C": once
// renamed it is a normal C++ function, and its name is mangled like any
// other. The signature must match the fork's exactly.
int cannonball_main(int argc, char *argv[]);

void CGlueStdioInit(CConsole &rConsole);

// The shim's serial key injection: hand it this kernel's serial device and
// a console attached to the port can type into the running game. Declared
// here rather than included, because it is the shim's own internal surface
// and not part of the SDL one.
class CSerialDevice;
void SDL2Circle_SetInjectSerial(CSerialDevice *pSerial);

static const char From[] = "cannonball";

// Cannonball's own command line. It parses only -cfgfile and -file, and
// takes everything else from the configuration file this names.
//
// These are the baked arguments. Anything written into the image's defaults
// block is appended to them before the game runs, so a boot can add to this
// — or override it, since Cannonball takes the last -cfgfile it is given —
// without the card or the build changing.
static const char *CannonballArgv[] = {
    "cannonball",
    "-cfgfile", "/config.xml",
};

// The final list: the baked arguments, plus whatever the block carries.
// Sized for the block's worst case — every byte of its capacity a
// single-character argument — on top of the baked ones, plus the
// terminating null.
static const char *s_FinalArgv[sizeof(CannonballArgv) / sizeof(CannonballArgv[0])
                               + DEFAULTS_BUFFER_BYTES / 2 + 1];
static int s_FinalArgc = 0;

// ---------------------------------------------------------------------------
// The gate between core 0 and the application core.
//
// The cores are started at the end of Initialize, because that is where a
// Circle world is finished. But the application must not begin until the
// shim's split is armed — until then its platform calls would run on the
// wrong core with no mailbox to carry them. So the application core waits
// here, and core 0 opens the gate once SDL2Circle_SplitInit has returned.
//
// The return travels back the same way. Core 0 cannot join a core, so the
// application core publishes the result and core 0 watches for it while
// yielding to the scheduler — which is what keeps the servo, the watchdog
// and every device alive for as long as the game runs.
// ---------------------------------------------------------------------------

static std::atomic<int> s_AppGate{0};      // core 0 -> application core
static std::atomic<int> s_AppDone{0};      // application core -> core 0
static int s_AppResult = -1;

// The per-core C runtime is the library's SDL2Circle_ArmCoreRuntime: a core
// this kernel starts has no thread pointer until something sets it, and a
// C++ throw is the first thing to notice. Every core arms itself below,
// before it runs anything else.

static inline void PublishToOtherCores(void)
{
    asm volatile("dsb ish; sev" ::: "memory");
}

static void ParkCore(void)
{
    for (;;)
        asm volatile("wfe" ::: "memory");
}

void CSplitCores::Run(unsigned nCore)
{
    // Before this core runs anything of ours: every core here may reach code
    // that throws, and a throw reads this register first.
    SDL2Circle_ArmCoreRuntime();

    switch (nCore)
    {
    case 1:
        // The application core. Wait for the gate, run the game, publish
        // what it returned, then go quiet — this core has no other purpose
        // and must not fall through into anything.
        while (!s_AppGate.load(std::memory_order_acquire))
            asm volatile("wfe" ::: "memory");

        s_AppResult = cannonball_main(s_FinalArgc, const_cast<char **>(s_FinalArgv));

        s_AppDone.store(1, std::memory_order_release);
        PublishToOtherCores();
        ParkCore();
        break;

    case 2:
        // The elected presentation core. Never returns.
        SDL2Circle_SplitPresentCore();
        break;

    default:
        ParkCore();
        break;
    }
}

CKernel::CKernel(void)
    // Serial device 0 is the GPIO14/15 header UART on every board. Named
    // explicitly because Circle's RASPPI >= 5 default (SERIAL_DEVICE_DEFAULT
    // = 10) is the Pi 5's dedicated debug connector, not the header.
    : m_Serial(0, FALSE, 0),
      m_Timer(&m_Interrupt),
      m_Logger(m_Options.GetLogLevel(), &m_Timer),
      m_EMMC(&m_Interrupt, &m_Timer, &m_ActLED),
      m_Console(&m_Serial, &m_Serial)     // stdio over the UART
{
    m_ActLED.Blink(3);
}

// Build-timestamp epoch (seconds since 1970-01-01 UTC) from __DATE__/__TIME__.
// Monotonic across releases, always a plausible "now".
static unsigned BuildEpoch(void)
{
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *d = __DATE__;   // "Mmm dd yyyy"
    const char *t = __TIME__;   // "hh:mm:ss"

    int mon = 1;
    for (int i = 0; i < 12; i++)
        if (d[0] == months[i*3] && d[1] == months[i*3+1] && d[2] == months[i*3+2])
            { mon = i + 1; break; }
    int day  = (d[4] == ' ' ? 0 : d[4] - '0') * 10 + (d[5] - '0');
    int year = (d[7]-'0')*1000 + (d[8]-'0')*100 + (d[9]-'0')*10 + (d[10]-'0');
    int hh = (t[0]-'0')*10 + (t[1]-'0');
    int mm = (t[3]-'0')*10 + (t[4]-'0');
    int ss = (t[6]-'0')*10 + (t[7]-'0');

    // days since 1970-01-01 (civil-to-days, treated as UTC)
    int y = year - (mon <= 2);
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153u * (mon + (mon > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    unsigned doe = yoe*365 + yoe/4 - yoe/100 + doy;
    long days = (long)era*146097 + (long)doe - 719468;
    return (unsigned)(days * 86400L + hh*3600 + mm*60 + ss);
}

boolean CKernel::Initialize(void)
{
    boolean bOK = TRUE;
    if (bOK) bOK = m_Serial.Initialize(115200);
    if (bOK) bOK = m_Logger.Initialize(&m_Serial);
    if (bOK) bOK = m_Interrupt.Initialize();
    if (bOK) bOK = m_Timer.Initialize();
    // No battery RTC on a Pi: seed the wall clock with the build time —
    // like a device whose clock was set once at the factory — so time()
    // is plausible and the shim's pre-main factory clock hands over
    // seamlessly (see circle-libsdl2 src/init.cpp).
    if (bOK) m_Timer.SetTime(BuildEpoch(), FALSE /* universal */);
    if (bOK) bOK = m_EMMC.Initialize();
    if (bOK) bOK = (f_mount(&m_FileSystem, "SD:", 1) == FR_OK);
    if (bOK) bOK = m_Console.Initialize();
    if (bOK) CGlueStdioInit(m_Console);

    // Core 0 runs application and library code like any other core, so it
    // arms itself too — before the secondary cores start, and before the
    // first thing that can throw.
    if (bOK) SDL2Circle_ArmCoreRuntime();

    // Start the secondary cores last: the world they are about to work in
    // has to be complete first — the card mounted, stdio wired — because
    // core 0 will be busy serving them from the moment they run. They park
    // in CSplitCores::Run until Run() below arms the split and opens the
    // gate.
    m_bSplit = m_Options.GetAppOptionDecimal("rapi-split", 1) != 0;
    if (bOK && m_bSplit) bOK = m_Cores.Initialize();
    return bOK;
}

TShutdownMode CKernel::Run(void)
{
    m_Logger.Write(From, LogNotice, "starting Cannonball");

    // Geometry evidence belongs on serial: what boot config handed us, read
    // next to the shim's framebuffer-grant line when the window is created.
    // On Pi 3/4 the firmware honors this request and outputs it as the video
    // signal; on Pi 5 the firmware ignores mode requests and every kernel
    // inherits the one surface the firmware made at its own boot.
    m_Logger.Write(From, LogNotice, "boot config geometry: %ux%u",
                   m_Options.GetWidth(), m_Options.GetHeight());

    // Render throughput lives and dies by the ARM and core clocks. The shim
    // owns the class that manages them, so the readings come from the shim;
    // this kernel never makes a CCPUThrottle of its own, because Circle
    // allows exactly one and a second stops the board. Above the socmaxtemp
    // limit in the card's cmdline.txt the clock is pulled back to idle — or,
    // where that file also names a fan pin with gpiofanpin=, the fan is
    // switched on instead and the clock is left alone.
    m_Logger.Write(From, LogNotice,
                   "SoC: %uC, arm %u MHz, core %u MHz, socmaxtemp %uC",
                   SDL2Circle_SoCTemperature(),
                   SDL2Circle_CPUClockRate() / 1000000,
                   CMachineInfo::Get()->GetClockRate(CLOCK_ID_CORE) / 1000000,
                   CKernelOptions::Get()->GetSoCMaxTemp());

    // Build the game's argument list before anything else looks at it: the
    // baked arguments, plus whatever a pre-boot writer stamped into the
    // image. Kernel switches are taken out here and never reach the game.
    s_FinalArgc = DefaultsBuildArgv(CannonballArgv,
                                    sizeof(CannonballArgv) / sizeof(CannonballArgv[0]),
                                    s_FinalArgv,
                                    sizeof(s_FinalArgv) / sizeof(s_FinalArgv[0]));

    // Serial key injection, if the block asked for it. Armed HERE, before
    // the split is armed and before the application core is let go: the
    // shim's injection pump runs in the hardware core's servo, which is
    // where reading the serial port is legal, and that servo does not exist
    // until SDL2Circle_SplitInit below. Arming after that point would race
    // the first pump; arming before it cannot.
    if (rapi_debug_uart)
    {
        SDL2Circle_SetInjectSerial(&m_Serial);
        m_Logger.Write(From, LogNotice,
                       "serial key injection armed (--rapi-debug-uart)");
    }

    // Performance receipts are the library's own feature now: cmdline.txt
    // `rapi-perf=N` arms one serial line every N seconds (frame rate, then
    // the cycle split), read by the shim inside SDL_Init. Nothing to wire
    // here; bench and product cards share one kernel either way.

    int res;
    if (m_bSplit)
    {
        m_Logger.Write(From, LogNotice,
                       "core split: hardware core 0, application core 1, presentation core 2");

        // Arm the split before the application's first instruction: the
        // servo and watchdog on core 0, and the mailboxes every marshalled
        // call rides. Then open the gate.
        SDL2Circle_SplitInit();
        s_AppGate.store(1, std::memory_order_release);
        PublishToOtherCores();

        // Core 0's idle loop for the whole run. Yielding is not politeness
        // here: the servo task is what answers the application core, feeds
        // the sound device and pumps USB, and it only runs when this loop
        // gives it the core.
        while (!s_AppDone.load(std::memory_order_acquire))
            m_Scheduler.Yield();
        res = s_AppResult;
    }
    else
    {
        // rapi-split=0: everything on core 0, the library's degenerate
        // path. The secondary cores were never started.
        m_Logger.Write(From, LogNotice,
                       "core split disabled (rapi-split=0): everything on core 0");
        res = cannonball_main(s_FinalArgc, const_cast<char **>(s_FinalArgv));
    }

    // Park instead of rebooting. A reboot stops the clocks with the UART
    // FIFO still draining, so the exit line reaches the bench truncated —
    // and it destroys the machine state worth inspecting. The board sits
    // here until power-cycled, which boots the card's chainloader again.
    m_Logger.Write(From, LogNotice, "Cannonball exited with %d — parked", res);
    for (;;)
    {
        m_Timer.MsDelay(1000);
    }
}
