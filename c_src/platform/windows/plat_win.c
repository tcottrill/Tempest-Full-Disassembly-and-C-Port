/* plat_win.c - Tempest C port: the Windows backend (M8 part 2).
 *
 * Harvested from the Space Duel port's plat_win.c and adapted in place to
 * platform/tempest_platform.h.  Win32 + OpenGL 3.3 on the framework files in
 * this directory: raw input (key[] buffer, mouse mickeys), the XAudio2 mixer's
 * stream voice, the shader beam renderer, DirectInput joystick, DPI-aware
 * window with ALT+ENTER borderless fullscreen, ini config.
 *
 * Tempest differences from the Space Duel backend:
 *  - per-segment 24-bit RGB (the 16-colour palette is resolved from colour
 *    RAM by the core, avg_colram_rgb) and a 1..15 intensity;
 *  - the monitor is a 4:3 tube on its side (ORIENTATION_ROTATE_270): AVG
 *    space is clipped to the tube (x +-290, y +-285, AAE_DRIVER_SCREEN
 *    580 x 570), mirrored by the OUT0 invert bits and stretched onto a 3:4
 *    portrait picture (see xform_point for the orientation found on screen);
 *  - the core hands over a picture per AVG traversal (~61 a second, the
 *    list's own draw time; the game passes run at ~27 Hz underneath) and the
 *    backend swaps it at once (vsync=0) or presents the newest one on the
 *    display's own clock from the core's idle path (plat_sleep_ms);
 *  - spinner: mouse X (raw mickeys), Left/Right keys, joystick X axis;
 *  - no samples (Tempest has none): the POKEY stream is the only sound;
 *  - files next to the exe: tempest_win.ini, tempest_win.log, tempest.nv;
 *  - verification options (--shot, --autoplay, --quit-after-ms), see WinMain.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../tempest_platform.h"
#include "framework.h"     /* Windows.h, glew, log */
#include "sys_gl.h"
#include "rawinput.h"
#include "joystick.h"
#include "mixer.h"
#include "vector_draw.h"
#include "mat4.h"
#include "ini.h"
#include "colordefs.h"
#include "miniz.h"
#include "win_probe.h"
#include <mmsystem.h>
#include <dwmapi.h>
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#define APP_NAME "Tempest"
#define KEY_HELP "mouse/Left/Right spin, Ctrl/Space/LMB fire, Alt/RMB zap, " \
                 "5/6/7 coin, 1/2 start, F2 test switch, F1 diag step, F3 slam, " \
                 "[ ] mouse speed, Shift+[ ] key speed, F8 mouse capture, Esc quit"

/* ------------------------------------------------------------------ */
/* files next to the exe                                               */
/* ------------------------------------------------------------------ */

static char exe_dir[MAX_PATH];
static char path_ini[MAX_PATH], path_log[MAX_PATH], path_nv[MAX_PATH];

static void resolve_paths(void)
{
    char *slash;
    DWORD n = GetModuleFileNameA(NULL, exe_dir, (DWORD)sizeof exe_dir);
    if (n == 0 || n >= sizeof exe_dir) exe_dir[0] = '\0';
    slash = strrchr(exe_dir, '\\');
    if (slash) slash[1] = '\0'; else exe_dir[0] = '\0';
    snprintf(path_ini, sizeof path_ini, "%stempest_win.ini", exe_dir);
    snprintf(path_log, sizeof path_log, "%stempest_win.log", exe_dir);
    snprintf(path_nv,  sizeof path_nv,  "%stempest.nv",      exe_dir);
}

/* ------------------------------------------------------------------ */
/* command line (WinMain)                                              */
/* ------------------------------------------------------------------ */

#define MAX_SHOTS 8
typedef struct { unsigned long pass; char path[MAX_PATH]; } shot_req;

static shot_req shots[MAX_SHOTS];
static int      n_shots, shots_done;
static int      shot_mode;              /* any --shot: turbo clock, hidden window, no audio */
static int      shot_w = 720, shot_h = 960;
static int      autoplay;
static int      autoplay_spin = 3;
static double   quit_after_ms;          /* 0 = run until quit */
static int      no_nvram;               /* --no-nvram (implied by --shot / --autoplay) */
static FILE    *pass_log;               /* --pass-log FILE N: per pass 'qstate qdstate irqs' */
static unsigned long pass_log_n;
static int      hidden_mode;            /* --hidden: real clock, window never shown, audio muted */
static char     nv_override[MAX_PATH];  /* --nvram FILE */

/* M9 B5: the self-test switches.  test_latch = the board's TEST switch (a
 * toggle: F2, [main] test_switch / --test at launch); F1 = diag step and F3 =
 * slam are held keys.  --hold NAME FROM TO scripts any switch by pass number
 * (the step count WinMain runs, 1-based; TO exclusive, 0 = for ever), --spin
 * N FROM TO the spinner - for --shot / --hidden runs, which read no keys. */
static int      test_latch;
static int      test_cli;               /* --test given */
static int      last_test;              /* the TEST line as last polled (latch or --hold) */
static double   wd_cycles_ini;          /* [main] watchdog_cycles */
static unsigned long cur_step;          /* the pass WinMain is running (0 = power-on) */

enum { H_TEST, H_DIAG, H_SLAM, H_FIRE, H_ZAP, H_START1, H_START2, H_COINL, H_COINC, H_COINR, H_SPIN };
typedef struct { int what; unsigned long from, to; int val; } hold_req;
#define MAX_HOLDS 32
static hold_req holds[MAX_HOLDS];
static int      n_holds;

static int hold_name(const char *s)
{
    static const char *const names[] = { "test", "diag", "slam", "fire", "zap", "start1", "start2", "coinl", "coinc", "coinr" };
    for (int i = 0; i < (int)(sizeof names / sizeof names[0]); i++)
        if (!strcmp(s, names[i])) return i;
    return -1;
}

/* ------------------------------------------------------------------ */
/* option switches                                                     */
/* ------------------------------------------------------------------ */
/* [dips] in tempest_win.ini: one key per switch group, each a word from its
 * list.  Bit values: AAE's tempest.cpp INPUT_PORTS_START(tempest) (= MAME's),
 * cross-checked with the ROM where it reads them:
 *   N13 (INOP0 $0D00): INILIT S_CMODE = N13 ^ $02 (coinage b0-1); ALCOIN's
 *       coin-mech multipliers b2-4 and bonus coins b5-7; EXSTAT's freeze
 *       test (N13 & $83) == $82.
 *   L12 (INOP1 $0E00): INILIT - bonus life TBLIFI[(L12 & $38) >> 3] =
 *       20000,10000,30000..70000,none; lives GAMLVS[L12 >> 6] = 3,4,5,2;
 *       language LNGTAB[(L12 & $06) >> 1]; b0 minimum credits (AAE).
 *   POKEY 2 ALLPO2 b0-2 (GETOP3 K_MOPT13): difficulty b0-1, rating b2.
 *   POKEY 1 ALLPOT b4 (K_COCKTA "=1 IF COCKTAIL", the IRQ's COCTAL) and b5
 *       (K_MOPTI4, GETOP3's special option).
 * An unknown word falls back to the default; the word that took effect is
 * written back.  Defaults = all switches 0 = AAE/MAME defaults = the refrun
 * oracle's settings. */
static uint8_t dsw_n13, dsw_l12, dsw_pokey1, dsw_pokey2;

uint8_t plat_dsw_n13(void)    { return dsw_n13; }
uint8_t plat_dsw_l12(void)    { return dsw_l12; }
uint8_t plat_dsw_pokey1(void) { return dsw_pokey1; }
uint8_t plat_dsw_pokey2(void) { return dsw_pokey2; }

static uint8_t dip_pick(const char *name, const char *const *names,
                        const uint8_t *vals, int n, int def)
{
    char *s = get_config_string("dips", name, names[def]);
    int pick = def;
    if (s) {
        for (int i = 0; i < n; i++)
            if (_stricmp(s, names[i]) == 0) { pick = i; break; }
        free(s);
    }
    set_config_string("dips", name, names[pick]);
    return vals[pick];
}

static void read_dips(void)
{
    /* N13 */
    static const char *const coin_n[]  = { "1_coin_1_play", "2_coins_1_play", "1_coin_2_plays", "free_play" };
    static const uint8_t     coin_v[]  = { 0x00, 0x01, 0x03, 0x02 };
    static const char *const right_n[] = { "x1", "x4", "x5", "x6" };
    static const uint8_t     right_v[] = { 0x00, 0x04, 0x08, 0x0C };
    static const char *const left_n[]  = { "x1", "x2" };
    static const uint8_t     left_v[]  = { 0x00, 0x10 };
    static const char *const bcoin_n[] = { "none", "1_each_5", "1_each_4", "1_each_3", "2_each_4", "1_each_2", "freeze" };
    static const uint8_t     bcoin_v[] = { 0x00, 0x80, 0x40, 0xA0, 0x60, 0x20, 0xC0 };
    /* L12 */
    static const char *const minc_n[]  = { "1", "2" };
    static const uint8_t     minc_v[]  = { 0x00, 0x01 };
    static const char *const lang_n[]  = { "english", "french", "german", "spanish" };
    static const uint8_t     lang_v[]  = { 0x00, 0x02, 0x04, 0x06 };
    static const char *const bonus_n[] = { "20000", "10000", "30000", "40000", "50000", "60000", "70000", "none" };
    static const uint8_t     bonus_v[] = { 0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38 };
    static const char *const lives_n[] = { "3", "4", "5", "2" };
    static const uint8_t     lives_v[] = { 0x00, 0x40, 0x80, 0xC0 };
    /* POKEY 2 */
    static const char *const diff_n[]  = { "medium1", "easy", "medium2", "hard" };
    static const uint8_t     diff_v[]  = { 0x00, 0x01, 0x03, 0x02 };
    static const char *const rate_n[]  = { "1_3_5_7_9", "tied_to_high_score" };
    static const uint8_t     rate_v[]  = { 0x00, 0x04 };
    /* POKEY 1 */
    static const char *const cab_n[]   = { "upright", "cocktail" };
    static const uint8_t     cab_v[]   = { 0x00, 0x10 };
    static const char *const opt_n[]   = { "off", "on" };
    static const uint8_t     opt_v[]   = { 0x00, 0x20 };

    dsw_n13  = (uint8_t)(dip_pick("coinage", coin_n, coin_v, 4, 0) |
                         dip_pick("right_coin", right_n, right_v, 4, 0) |
                         dip_pick("left_coin", left_n, left_v, 2, 0) |
                         dip_pick("bonus_coins", bcoin_n, bcoin_v, 7, 0));
    dsw_l12  = (uint8_t)(dip_pick("minimum_credits", minc_n, minc_v, 2, 0) |
                         dip_pick("language", lang_n, lang_v, 4, 0) |
                         dip_pick("bonus_life", bonus_n, bonus_v, 8, 0) |
                         dip_pick("lives", lives_n, lives_v, 4, 0));
    dsw_pokey2 = (uint8_t)(dip_pick("difficulty", diff_n, diff_v, 4, 0) |
                           dip_pick("rating", rate_n, rate_v, 2, 0));
    dsw_pokey1 = (uint8_t)(dip_pick("cabinet", cab_n, cab_v, 2, 0) |
                           dip_pick("special_option", opt_n, opt_v, 2, 0));
    LOG_INFO("option switches: N13=%02X L12=%02X POKEY1=%02X POKEY2=%02X",
             dsw_n13, dsw_l12, dsw_pokey1, dsw_pokey2);
}

/* ------------------------------------------------------------------ */
/* geometry: AVG space -> the portrait picture                         */
/* ------------------------------------------------------------------ */
/* The tube shows AVG x -290..290, y -285..285 (AAE_DRIVER_SCREEN 580 x 570,
 * origin = the beam centre) on a 4:3 monitor mounted on its side
 * (AAE/MAME ORIENTATION_ROTATE_270): a 3:4 portrait picture.
 *
 * WHICH WAY UP WAS FIXED ON SCREEN (--shot, NOTES_m8.md "Part 2").  A first
 * build rotated avg.c's space by 270 degrees and applied the OUT0 bits as
 * "set = invert": the text ran up the side of the picture, mirrored.  The
 * shots show that avg.c's space (+x right, +y up) already IS the cabinet's
 * upright view once the ROM's normal TOUT0 = $10 (RESET, COCFLI player 1) is
 * taken as the identity: text reads left to right along +x, glyph tops +y,
 * the score / high score at +y (top), CREDITS at -y (bottom).  The ROTATE_270
 * of the emulators is their landscape-frame bookkeeping; here it only means
 * the picture is 3:4 portrait, 580 units across filling 3 and 570 units down
 * filling 4 (the same stretch MAME/AAE give the visible area).
 *
 * The invert bits relative to that view: COCFLI writes $10 for the upright /
 * player 1 and $08 for cocktail player 2, which must be the picture turned
 * 180 degrees - both axes change state.  So X is mirrored when $08 is SET
 * and Y when $10 is CLEAR ($10 = the Y deflection's normal polarity).
 *
 * Picture space (what the beam renderer draws in): isotropic, PIC_HW*2 = 435
 * units across, PIC_HH*2 = 580 down, +y up; the projection maps it onto the
 * 3:4 viewport. */
#define AVG_XMAX   290.0f
#define AVG_YMAX   285.0f
#define PIC_HH     290.0f                      /* picture half-height, units */
#define PIC_HW     (PIC_HH * 3.0f / 4.0f)      /* 217.5: 3:4 */

static int rot_sign = 1;          /* [video] rotate_sign: -1 turns the picture 180 degrees */
static int honour_flip = 1;       /* [video] honour_flip */

static void xform_point(float x, float y, uint8_t flip, float *px, float *py)
{
    if (honour_flip) {
        if (flip & 0x08)    x = -x;
        if (!(flip & 0x10)) y = -y;
    }
    *px = (float)rot_sign * x * (PIC_HW / AVG_XMAX);
    *py = (float)rot_sign * y * (PIC_HH / AVG_YMAX);
}

/* Liang-Barsky against the tube window; 0 = nothing visible. */
static int clip_seg(float *x0, float *y0, float *x1, float *y1)
{
    float t0 = 0.0f, t1 = 1.0f;
    float dx = *x1 - *x0, dy = *y1 - *y0;
    float p[4], q[4];
    int i;
    p[0] = -dx; q[0] = *x0 + AVG_XMAX;
    p[1] =  dx; q[1] = AVG_XMAX - *x0;
    p[2] = -dy; q[2] = *y0 + AVG_YMAX;
    p[3] =  dy; q[3] = AVG_YMAX - *y0;
    for (i = 0; i < 4; i++) {
        if (p[i] == 0.0f) {
            if (q[i] < 0.0f) return 0;
        } else {
            float t = q[i] / p[i];
            if (p[i] < 0.0f) { if (t > t1) return 0; if (t > t0) t0 = t; }
            else             { if (t < t0) return 0; if (t < t1) t1 = t; }
        }
    }
    if (t1 < 1.0f) { *x1 = *x0 + t1 * dx; *y1 = *y0 + t1 * dy; }
    if (t0 > 0.0f) { *x0 = *x0 + t0 * dx; *y0 = *y0 + t0 * dy; }
    return (t0 > 0.0f || t1 < 1.0f) ? 2 : 1;       /* 2 = cut at the edge */
}

/* ---- phosphor / frame store ------------------------------------------
 * A pass's segments are captured already transformed into picture space.
 * The newest complete frame is what every present draws; with [vector]
 * phosphor_ms > 0 older frames are replayed under it at exp(-age/tau). */
#define PH_FRAMES  8
#define PH_SEGS    4096
#define PH_FLOOR   0.02

typedef struct { float x0, y0, x1, y1; rgb_t rgb; int intensity; } ph_seg;
typedef struct { ph_seg seg[PH_SEGS]; int n; double t; int used; } ph_frame;

static ph_frame ph_buf[PH_FRAMES];
static int      ph_cur;           /* the newest COMPLETE frame */
static int      ph_fill = -1;     /* the frame being captured, -1 = none */
static double   ph_tau;
static int      ph_dropped;
static uint8_t  cur_flip;
static unsigned long seg_clipped, seg_cut, seg_total;

/* ------------------------------------------------------------------ */
/* window, framework externs                                           */
/* ------------------------------------------------------------------ */

static HWND hWnd;
static int  g_quit;

int SCREEN_W = 720;       /* window client area, physical pixels */
int SCREEN_H = 960;
int DESIGN_W = 720;       /* the 3:4 design rect ViewOrthoScaled fits */
int DESIGN_H = 960;

HWND win_get_window(void) { return hWnd; }

static void msg_box(const char *title, const char *message)
{
    if (shot_mode || hidden_mode) { LOG_ERROR("%s: %s", title, message); return; }
    MessageBoxA(NULL, message, title, MB_ICONEXCLAMATION | MB_OK);
}

static void set_window_title(const char *title) { SetWindowTextA(hWnd, title); }

static float monitor_refresh_hz(HWND w)
{
    MONITORINFOEXW mi;
    DEVMODEW dm;
    HMONITOR m = MonitorFromWindow(w, MONITOR_DEFAULTTOPRIMARY);

    memset(&mi, 0, sizeof mi);
    mi.cbSize = sizeof mi;
    if (!GetMonitorInfoW(m, (MONITORINFO *)&mi)) return 0.0f;
    memset(&dm, 0, sizeof dm);
    dm.dmSize = sizeof dm;
    if (!EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm))
        return 0.0f;
    return dm.dmDisplayFrequency > 1 ? (float)dm.dmDisplayFrequency : 0.0f;
}

/* ------------------------------------------------------------------ */
/* DPI awareness                                                       */
/* ------------------------------------------------------------------ */

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif
#ifndef USER_DEFAULT_SCREEN_DPI
#define USER_DEFAULT_SCREEN_DPI 96
#endif
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

static void enable_dpi_awareness(void)
{
    typedef BOOL(WINAPI *SetProcessDpiAwarenessContext_t)(DPI_AWARENESS_CONTEXT);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        SetProcessDpiAwarenessContext_t set_ctx = (SetProcessDpiAwarenessContext_t)
            GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (set_ctx && set_ctx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
            return;
    }
    SetProcessDPIAware();
}

static UINT window_dpi(HWND hwnd)
{
    typedef UINT(WINAPI *GetDpiForWindow_t)(HWND);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    UINT dpi = 0;
    if (user32) {
        GetDpiForWindow_t get_dpi = (GetDpiForWindow_t)
            GetProcAddress(user32, "GetDpiForWindow");
        if (get_dpi) dpi = get_dpi(hwnd);
    }
    if (dpi == 0) {
        HDC hdc = GetDC(NULL);
        if (hdc) {
            dpi = (UINT)GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(NULL, hdc);
        }
    }
    return dpi ? dpi : USER_DEFAULT_SCREEN_DPI;
}

/* The 3:4 design size at the window's DPI, shrunk (keeping 3:4) to fit 90%
 * of the monitor's work area - a 960-px-tall client does not fit a 1080p
 * panel at 150%. */
static void size_window_for_dpi(HWND hwnd, UINT dpi, DWORD style)
{
    typedef BOOL(WINAPI *AdjustWindowRectExForDpi_t)(LPRECT, DWORD, BOOL, DWORD, UINT);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    AdjustWindowRectExForDpi_t adjust_for_dpi = NULL;
    MONITORINFO mi;
    RECT wr;
    int cw = MulDiv(DESIGN_W, (int)dpi, USER_DEFAULT_SCREEN_DPI);
    int ch = MulDiv(DESIGN_H, (int)dpi, USER_DEFAULT_SCREEN_DPI);

    memset(&mi, 0, sizeof mi);
    mi.cbSize = sizeof mi;
    if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
        int avail_h = (mi.rcWork.bottom - mi.rcWork.top) * 9 / 10 - 40;
        if (avail_h > 200 && ch > avail_h) {
            ch = avail_h;
            cw = ch * 3 / 4;
        }
    }

    wr.left = 0;
    wr.top = 0;
    wr.right = cw;
    wr.bottom = ch;

    if (user32)
        adjust_for_dpi = (AdjustWindowRectExForDpi_t)
            GetProcAddress(user32, "AdjustWindowRectExForDpi");
    if (adjust_for_dpi)
        adjust_for_dpi(&wr, style, FALSE, 0, dpi);
    else
        AdjustWindowRect(&wr, style, FALSE);

    SetWindowPos(hwnd, NULL, 0, 0, wr.right - wr.left, wr.bottom - wr.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/* ------------------------------------------------------------------ */
/* borderless fullscreen, ALT+ENTER                                    */
/* ------------------------------------------------------------------ */

static int      is_fullscreen;
static RECT     windowed_rect;
static LONG_PTR windowed_style;
static LONG_PTR windowed_exstyle;

int IsFullscreen(void) { return is_fullscreen; }

void ToggleFullscreen(void)
{
    if (is_fullscreen) {
        SetWindowLongPtr(hWnd, GWL_STYLE, windowed_style);
        SetWindowLongPtr(hWnd, GWL_EXSTYLE, windowed_exstyle);
        SetWindowPos(hWnd, HWND_NOTOPMOST,
                     windowed_rect.left, windowed_rect.top,
                     windowed_rect.right - windowed_rect.left,
                     windowed_rect.bottom - windowed_rect.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        is_fullscreen = 0;
    } else {
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &mi);
        GetWindowRect(hWnd, &windowed_rect);
        windowed_style = GetWindowLongPtr(hWnd, GWL_STYLE);
        windowed_exstyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
        SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_APPWINDOW | WS_EX_TOPMOST);
        SetWindowPos(hWnd, HWND_TOPMOST,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        is_fullscreen = 1;
    }
}

/* ------------------------------------------------------------------ */
/* beam width                                                          */
/* ------------------------------------------------------------------ */
/* Same rule as the Space Duel backend, with Tempest's projection: [vector]
 * linewidth = the beam in PIXELS AT THE DEFAULT 720-WIDE WINDOW, scaling with
 * the picture (its full-intensity core scales, the feather is added back);
 * [vector] line_smoothing = the AA feather in physical pixels on any screen.
 * The beam renderer takes both in projection units: PIC_HW * 2 across. */
static float beam_px = 2.0f;
static float beam_feather_px = 1.0f;
static float beam_proj[16];

static void update_beam_width_for(double vw)
{
    double span = (double)PIC_HW * 2.0, feather_du, core_px;
    if (vw < 1.0) vw = 1.0;
    feather_du = beam_feather_px * span / vw;
    beam_set_smoothing((float)feather_du);
    core_px = beam_px - beam_feather_px;
    if (core_px < 0.0) core_px = 0.0;
    beam_set_linewidth((float)(core_px * span / (double)DESIGN_W + feather_du));
}

static void update_beam_width(void)
{
    double vw = (double)SCREEN_W;
    if ((double)SCREEN_H * DESIGN_W < vw * DESIGN_H)
        vw = (double)SCREEN_H * DESIGN_W / (double)DESIGN_H;
    update_beam_width_for(vw);
}

/* ------------------------------------------------------------------ */
/* mouse capture                                                       */
/* ------------------------------------------------------------------ */

static int capture_enabled = 1;   /* [input] mouse_capture; F8 toggles */
static int window_active;

static void apply_capture(void)
{
    if (!hWnd) return;
    if (capture_enabled && window_active && !shot_mode) {
        RECT rc;
        POINT tl = { 0, 0 }, br;
        GetClientRect(hWnd, &rc);
        br.x = rc.right; br.y = rc.bottom;
        ClientToScreen(hWnd, &tl);
        ClientToScreen(hWnd, &br);
        rc.left = tl.x; rc.top = tl.y; rc.right = br.x; rc.bottom = br.y;
        ClipCursor(&rc);
    } else {
        ClipCursor(NULL);
    }
}

/* ------------------------------------------------------------------ */
/* presentation                                                        */
/* ------------------------------------------------------------------ */

static float mouse_sens = 0.5f;           /* [input] mouse_sensitivity, counts/mickey */
static int   tuning_changed;              /* a hotkey changed mouse_sens / key_spin_rate: saved on exit */
static float key_spin_rate = 180.0f;      /* [input] key_spin_rate, counts/s */
static int    swap_mode;
static float  fps_lock;                 /* [main] fps_lock, IRQ Hz, 0 = native */
static int    vg_window_ini;          /* [main] vg_window: TP_VGW_* */
static double refresh_ms = 1000.0 / 60.0;
static double last_swap_ms;
static int    present_pending;
static unsigned long n_swaps, n_frames_in, n_swap_deferred;
static double last_frame_ms, frame_dt_min = 1e9, frame_dt_max;   /* frame-to-frame interval, ms (status line) */
static int    last_nsegs;

static void ph_replay(void)
{
    double now = plat_now_ms();
    int i;

    beam_clear();
    for (i = 0; i < PH_FRAMES; i++) {
        /* oldest first: start one past the newest and wrap */
        int idx = (ph_cur + 1 + i) % PH_FRAMES;
        ph_frame *f = &ph_buf[idx];
        double fac;
        int k;

        if (!f->used || idx == ph_fill) continue;
        if (idx == ph_cur) fac = 1.0;
        else if (ph_tau <= 0.0) continue;
        else {
            fac = exp(-(now - f->t) / ph_tau);
            if (fac < PH_FLOOR) continue;
        }
        for (k = 0; k < f->n; k++) {
            const ph_seg *s = &f->seg[k];
            /* brightness ((intensity << 4) | $0F) / 255, as AAE's AVG scales
             * the 4-bit intensity; multiplied, not AND-ed, into the colour */
            double b = (double)((s->intensity << 4) | 0x0F) / 255.0 * fac;
            int r = (int)(RGB_RED(s->rgb) * b + 0.5);
            int gg = (int)(RGB_GREEN(s->rgb) * b + 0.5);
            int bl = (int)(RGB_BLUE(s->rgb) * b + 0.5);
            if (r <= 0 && gg <= 0 && bl <= 0) continue;
            beam_add_line(s->x0, s->y0, s->x1, s->y1, 0xFF, MAKE_RGB(r, gg, bl));
        }
    }
    beam_draw_all(beam_proj);
}

/* Time to the display's next vblank, ms: DWM's composition clock (qpcVBlank,
 * qpcRefreshPeriod - the same QueryPerformanceCounter plat_now_ms reads);
 * without DWM timing, the phase of the last swap; -1 = unknown (swap anyway,
 * once, to learn the phase). */
static double ms_to_vblank(double now)
{
    DWM_TIMING_INFO ti;
    LARGE_INTEGER f;
    memset(&ti, 0, sizeof ti);
    ti.cbSize = sizeof ti;
    QueryPerformanceFrequency(&f);
    if (SUCCEEDED(DwmGetCompositionTimingInfo(NULL, &ti)) && ti.qpcRefreshPeriod > 0 && f.QuadPart > 0) {
        double per = (double)ti.qpcRefreshPeriod * 1000.0 / (double)f.QuadPart;
        double vb  = (double)ti.qpcVBlank * 1000.0 / (double)f.QuadPart;
        double ph  = fmod(now - vb, per);
        if (ph < 0.0) ph += per;
        return per - ph;
    }
    if (last_swap_ms <= 0.0 || now - last_swap_ms > 4.0 * refresh_ms) return -1.0;
    return refresh_ms - fmod(now - last_swap_ms, refresh_ms);
}

static void present_now(void)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ph_replay();
    glSwap();
    last_swap_ms = plat_now_ms();
    present_pending = 0;
    n_swaps++;
}

/* PRESENTATION POLICY (the Gravitar port's, M8 part 2).
 *
 * vsync=0 (default): plat_video_present draws and swaps at once, inside the
 * core's wait for the next IRQ - a free-running swap never blocks, so nothing can distort the
 * IRQ grid.  present_idle only repaints (WM_PAINT / resize) and redraws the
 * phosphor afterglow.
 *
 * vsync=1: a swap waits for the display's refresh, so it must never run
 * where an IRQ would be kept waiting.  It is only attempted from the core's
 * idle point (plat_sleep_ms), and only when the estimated time to the next
 * vblank (phase from the last swap) plus 1 ms fits inside the time to the
 * next IRQ (tempest_app_idle_ms); otherwise the frame waits for a later
 * idle.  from_idle = 0 (the main loop between passes) never swaps then. */
static void present_idle(int from_idle)
{
    double now;
    if (shot_mode || !hWnd) return;
    now = plat_now_ms();
    if (!present_pending && !(ph_tau > 0.0 && now - last_swap_ms >= refresh_ms)) return;
    if (swap_mode != 0) {
        double to_vb;
        if (!from_idle) return;
        to_vb = ms_to_vblank(now);
        if (to_vb >= 0.0 && (to_vb < 0.5 || to_vb + 1.0 >= tempest_app_idle_ms())) { n_swap_deferred++; return; }
    }
    present_now();
}

/* ------------------------------------------------------------------ */
/* window procedure                                                    */
/* ------------------------------------------------------------------ */

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            SCREEN_W = LOWORD(lParam);
            SCREEN_H = HIWORD(lParam);
            ViewOrthoScaled(SCREEN_W, SCREEN_H, DESIGN_W, DESIGN_H);
            update_beam_width();
            present_pending = 1;
            apply_capture();
        }
        return 0;

    case WM_MOVE:
        apply_capture();
        return 0;

    case WM_DPICHANGED: {
        const RECT *suggested = (const RECT *)lParam;
        SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_ACTIVATE:
        window_active = LOWORD(wParam) != WA_INACTIVE;
        if (!window_active) memset(key, 0, sizeof key);   /* no stuck keys */
        apply_capture();
        return 0;

    case WM_SYSKEYDOWN:
        /* ALT+ENTER; lParam bit 29 is the recorded ALT context flag. */
        if (wParam == VK_RETURN && (lParam & (1 << 29))) {
            ToggleFullscreen();
            key[KEY_ENTER] = 0;
            return 0;
        }
        return DefWindowProc(hwnd, message, wParam, lParam);

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        ValidateRect(hwnd, NULL);
        present_pending = 1;
        return 0;

    case WM_SYSCOMMAND:
        /* Superzapper is Left Alt; a lone ALT release would open the window
         * menu's modal loop (a multi-second freeze).  No menu: eat it. */
        if ((wParam & 0xFFF0) == SC_KEYMENU) return 0;
        return DefWindowProc(hwnd, message, wParam, lParam);

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT && (is_fullscreen || (capture_enabled && window_active))) {
            SetCursor(NULL);
            return 1;
        }
        return DefWindowProc(hwnd, message, wParam, lParam);

    case WM_CLOSE:
        LOG_INFO("quit: window closed (WM_CLOSE)");
        g_quit = 1;               /* the main loop tears down between passes */
        return 0;

    case WM_INPUT:
        RawInput_ProcessInput(hwnd, wParam, lParam);
        return DefWindowProc(hwnd, message, wParam, lParam);

    case WM_DEVICECHANGE:
        joystick_device_change();
        return DefWindowProc(hwnd, message, wParam, lParam);

    case WM_DESTROY:
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) { LOG_INFO("quit: Esc"); g_quit = 1; return 0; }
        if (wParam == VK_OEM_4 || wParam == VK_OEM_6) {
            /* live spinner tuning: [ / ] mouse sensitivity, Shift+[ / Shift+]
             * Left/Right key rate; 10% steps, saved to the ini on exit */
            int up = wParam == VK_OEM_6;
            if (GetKeyState(VK_SHIFT) < 0) {
                key_spin_rate *= up ? 1.1f : 1.0f / 1.1f;
                if (key_spin_rate < 5.0f) key_spin_rate = 5.0f;
                if (key_spin_rate > 2000.0f) key_spin_rate = 2000.0f;
            } else {
                mouse_sens *= up ? 1.1f : 1.0f / 1.1f;
                if (mouse_sens < 0.01f) mouse_sens = 0.01f;
                if (mouse_sens > 20.0f) mouse_sens = 20.0f;
            }
            tuning_changed = 1;
            LOG_INFO("tuning: mouse_sensitivity %.3f counts/mickey, key_spin_rate %.1f counts/s", mouse_sens, key_spin_rate);
            {
                char t[200];
                snprintf(t, sizeof t, APP_NAME "  mouse sensitivity %.3f  key spin rate %.0f/s  ([ ] mouse, Shift+[ ] keys)",
                         mouse_sens, key_spin_rate);
                set_window_title(t);
            }
            return 0;
        }
        if (wParam == VK_F2 && !(lParam & (1 << 30))) {
            /* the TEST switch (a latching toggle on the board) */
            char t[200];
            test_latch = !test_latch;
            LOG_INFO("TEST switch %s (F2)", test_latch ? "CLOSED (self test / options)" : "open");
            snprintf(t, sizeof t, APP_NAME "  TEST switch %s", test_latch
                     ? "ON: attract -> options (DSPSYS); diag loop runs the self test (F1 steps screens)"
                     : "OFF: the game (from the self test via the watchdog reboot)");
            set_window_title(t);
            return 0;
        }
        if (wParam == VK_F8 && !(lParam & (1 << 30))) {
            capture_enabled = !capture_enabled;
            LOG_INFO("mouse capture %s (F8)", capture_enabled ? "on" : "off");
            apply_capture();
        }
        return 0;

    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

static void pump_messages(void)
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) { g_quit = 1; break; }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

/* ------------------------------------------------------------------ */
/* lifecycle                                                           */
/* ------------------------------------------------------------------ */

static int   pokey_volume = 100;          /* [sound] pokey_volume, percent */
static int   audio_prime_blocks = 6;      /* [sound] latency_blocks */
static float joy_spin_rate = 180.0f;      /* [input] joy_spin_rate, counts/s at full stick */
static int   invert_spin;                 /* [input] invert_spin */

int plat_init(void)
{
    WNDCLASS wc;
    DWORD dwStyle;
    RECT wr = { 0, 0, 720, 960 };
    HINSTANCE hInstance = GetModuleHandle(NULL);

    resolve_paths();
    if (nv_override[0]) snprintf(path_nv, sizeof path_nv, "%s", nv_override);
    log_open(path_log);
    log_set_level(LOG_LEVEL_INFO);
    LOG_INFO("tempest_win backend starting%s%s%s", shot_mode ? " (shot mode: turbo clock, no audio)" : "",
             hidden_mode ? " (hidden: real clock, no window, audio muted)" : "",
             autoplay ? " (autoplay)" : "");

    enable_dpi_awareness();

    memset(&wc, 0, sizeof wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"TempestWin";
    if (!RegisterClass(&wc)) {
        msg_box(APP_NAME, "Failed to register the window class.");
        return 1;
    }

    dwStyle = WS_CAPTION | WS_POPUPWINDOW | WS_CLIPSIBLINGS
            | WS_CLIPCHILDREN | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;
    AdjustWindowRect(&wr, dwStyle, FALSE);
    hWnd = CreateWindow(L"TempestWin", L"Tempest", dwStyle,
                        CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
                        NULL, NULL, hInstance, NULL);
    if (!hWnd) {
        msg_box(APP_NAME, "Failed to create the main window.");
        return 1;
    }

    size_window_for_dpi(hWnd, window_dpi(hWnd), dwStyle);

    set_config_file(path_ini);
    read_dips();

    /* [main] vsync: 0 off / 1 on (default) / 2 adaptive.  The game makes
     * ~27 frames a second; each is shown on the panel's own refresh. */
    swap_mode = get_config_int("main", "vsync", 0);
    set_config_int("main", "vsync", swap_mode);

    /* [main] fps_lock: the board's IRQ rate in Hz (Gravitar's fps_lock knob;
     * for Tempest the natural unit is the IRQ, 9 of which make a frame gate).
     * 0 = the board's own 246.09 Hz (12.096 MHz / 4096 / 12); 240 = AAE's
     * Tempest (60 fps x 4 IRQs per frame), 2.5% slower - game, timers, sound
     * tempo and pitch all scale together. */
    fps_lock = get_config_float("main", "fps_lock", 0.0f);
    if (fps_lock < 0.0f || (fps_lock > 0.0f && (fps_lock < 100.0f || fps_lock > 1000.0f))) fps_lock = 0.0f;
    set_config_float("main", "fps_lock", fps_lock);
    {
        /* [main] vg_window: how long a picture lasts (tempest_platform.h TP_VGW_*) */
        char *s = get_config_string("main", "vg_window", "cycles");
        vg_window_ini = TP_VGW_CYCLES;
        if (s) {
            if (_stricmp(s, "free") == 0) vg_window_ini = TP_VGW_FREE;
            free(s);
        }
        set_config_string("main", "vg_window", vg_window_ini == TP_VGW_FREE ? "free" : "cycles");
        LOG_INFO("[main] vg_window = %s", vg_window_ini == TP_VGW_FREE ? "free" : "cycles");
    }
    LOG_INFO("[main] fps_lock = %.2f: IRQ at %.3f Hz (%s)", fps_lock, fps_lock > 0.0f ? fps_lock : 1512000.0 / 6144.0,
             fps_lock > 0.0f ? "board underclocked/overclocked to this rate" : "the board's own rate");

    /* [main] test_switch: the TEST switch at power-on (1 = closed: the ROM's
     * power-on self test); F2 toggles it at run time (not saved).  --test
     * forces 1.  [main] watchdog_cycles: the hardware watchdog timeout. */
    test_latch = get_config_int("main", "test_switch", 0) != 0;
    set_config_int("main", "test_switch", test_latch);
    if (test_cli) test_latch = 1;
    wd_cycles_ini = get_config_float("main", "watchdog_cycles", 1134000.0f);
    if (wd_cycles_ini < 12288.0) wd_cycles_ini = 1134000.0;
    set_config_float("main", "watchdog_cycles", (float)wd_cycles_ini);
    LOG_INFO("[main] test_switch: TEST %s at power-on; watchdog_cycles %.0f", test_latch ? "CLOSED" : "open", wd_cycles_ini);

    ph_tau = (double)get_config_float("vector", "phosphor_ms", 0.0f);
    set_config_float("vector", "phosphor_ms", (float)ph_tau);

    /* [video] rotate_sign: 1 = the cabinet's orientation (checked on
     * screen), -1 = turned 180 degrees.  honour_flip: 1 = apply the ROM's
     * OUT0 invert bits (cocktail player 2 flips), 0 = ignore them. */
    rot_sign = get_config_int("video", "rotate_sign", 1) < 0 ? -1 : 1;
    set_config_int("video", "rotate_sign", rot_sign);
    honour_flip = get_config_int("video", "honour_flip", 1) != 0;
    set_config_int("video", "honour_flip", honour_flip);

    mouse_sens = get_config_float("input", "mouse_sensitivity", 0.5f);
    set_config_float("input", "mouse_sensitivity", mouse_sens);
    key_spin_rate = get_config_float("input", "key_spin_rate", 180.0f);
    set_config_float("input", "key_spin_rate", key_spin_rate);
    joy_spin_rate = get_config_float("input", "joy_spin_rate", 180.0f);
    set_config_float("input", "joy_spin_rate", joy_spin_rate);
    invert_spin = get_config_int("input", "invert_spin", 0) != 0;
    set_config_int("input", "invert_spin", invert_spin);
    capture_enabled = get_config_int("input", "mouse_capture", 1) != 0;
    set_config_int("input", "mouse_capture", capture_enabled);

    if (FAILED(RawInput_Initialize(hWnd))) {
        msg_box(APP_NAME, "Failed to register for raw input; keyboard and mouse will not work.");
        LOG_ERROR("RawInput_Initialize failed");
    }

    install_joystick();

    pokey_volume = get_config_int("sound", "pokey_volume", 100);
    if (pokey_volume < 0)   pokey_volume = 0;
    if (pokey_volume > 100) pokey_volume = 100;
    set_config_int("sound", "pokey_volume", pokey_volume);
    audio_prime_blocks = get_config_int("sound", "latency_blocks", 6);
    if (audio_prime_blocks < 1) audio_prime_blocks = 1;
    if (audio_prime_blocks > 8) audio_prime_blocks = 8;
    set_config_int("sound", "latency_blocks", audio_prime_blocks);
    if (!shot_mode && pokey_volume > 0) {
        if (mixer_init() != 0)
            LOG_ERROR("mixer_init failed; continuing without audio");
        stream_set_prime_blocks(audio_prime_blocks);
    }

    if (!CreateGLContext()) {
        msg_box(APP_NAME, "Failed to create an OpenGL context.");
        return 1;
    }
    swap_mode = SetSwapMode(shot_mode ? 0 : swap_mode);
    ViewOrthoScaled(SCREEN_W, SCREEN_H, DESIGN_W, DESIGN_H);

    /* Pixel-based beam width (update_beam_width): written before beam_init so
     * its own read of the same keys (as design units, with its own defaults)
     * finds these values; update_beam_width then re-applies them as pixels. */
    beam_px = get_config_float("vector", "linewidth", 2.0f);
    beam_feather_px = get_config_float("vector", "line_smoothing", 1.0f);
    if (beam_px < 0.1f) beam_px = 0.1f;
    if (beam_feather_px < 0.0f) beam_feather_px = 0.0f;
    set_config_float("vector", "linewidth", beam_px);
    set_config_float("vector", "line_smoothing", beam_feather_px);

    if (!beam_init()) {
        msg_box(APP_NAME, "Beam renderer failed to build (needs OpenGL 3.3) - see tempest_win.log.");
        return 1;
    }
    beam_set_color_mode(1);
    update_beam_width();
    LOG_INFO("beam: %.2f px wide at a %d-wide window (proportional), %.2f px feather",
             beam_px, DESIGN_W, beam_feather_px);

    {
        float hz = get_config_float("main", "refresh_hz", 0.0f);
        set_config_float("main", "refresh_hz", hz);
        if (hz <= 0.0f) hz = monitor_refresh_hz(hWnd);
        if (hz < 20.0f || hz > 500.0f) hz = 60.0f;
        refresh_ms = 1000.0 / hz;
        LOG_INFO("display refresh %.2f Hz, swap mode %d", hz, swap_mode);
    }

    mat4_ortho(beam_proj, -PIC_HW, PIC_HW, -PIC_HH, PIC_HH, -1.0f, 1.0f);
    LOG_INFO("picture: AVG x +-%.0f / y +-%.0f clipped, 3:4 portrait (rotate_sign %d), OUT0 flips %s",
             AVG_XMAX, AVG_YMAX, rot_sign, honour_flip ? "honoured" : "ignored");

    timeBeginPeriod(1);

    if (!shot_mode && !hidden_mode) {
        set_window_title(APP_NAME " - " KEY_HELP);
        ShowWindow(hWnd, SW_SHOW);
        SetForegroundWindow(hWnd);
        window_active = GetForegroundWindow() == hWnd;
        apply_capture();
    }
    return 0;
}

void plat_shutdown(void)
{
    if (tuning_changed && !shot_mode) {
        set_config_float("input", "mouse_sensitivity", mouse_sens);
        set_config_float("input", "key_spin_rate", key_spin_rate);
        LOG_INFO("tuning saved: mouse_sensitivity %.3f, key_spin_rate %.1f", mouse_sens, key_spin_rate);
    }
    ClipCursor(NULL);
    timeEndPeriod(1);
    beam_shutdown();
    remove_joystick();
    plat_audio_close();
    mixer_end();
    DeleteGLContext();
    LOG_INFO("tempest_win backend closing");
    log_close();
    if (IsWindow(hWnd)) DestroyWindow(hWnd);
}

/* ------------------------------------------------------------------ */
/* video                                                               */
/* ------------------------------------------------------------------ */

void plat_video_begin(uint8_t flip)
{
    cur_flip = flip;
    ph_fill = (ph_cur + 1) % PH_FRAMES;
    ph_buf[ph_fill].n = 0;
    ph_buf[ph_fill].used = 1;
}

void plat_video_line(float x0, float y0, float x1, float y1, uint32_t rgb, int intensity)
{
    ph_frame *f;
    ph_seg *s;
    int vis;
    if (ph_fill < 0) return;
    seg_total++;
    vis = clip_seg(&x0, &y0, &x1, &y1);
    if (vis == 0) { seg_clipped++; return; }
    if (vis == 2) seg_cut++;
    f = &ph_buf[ph_fill];
    if (f->n >= PH_SEGS) {
        if (!ph_dropped) {
            ph_dropped = 1;
            LOG_WARN("display list exceeded %d segments; frames are truncated", PH_SEGS);
        }
        return;
    }
    s = &f->seg[f->n++];
    xform_point(x0, y0, cur_flip, &s->x0, &s->y0);
    xform_point(x1, y1, cur_flip, &s->x1, &s->y1);
    s->rgb = rgb;
    s->intensity = intensity < 0 ? 0 : intensity > 15 ? 15 : intensity;
}

void plat_video_present(void)
{
    if (ph_fill < 0) return;
    ph_buf[ph_fill].t = plat_now_ms();
    last_nsegs = ph_buf[ph_fill].n;
    ph_cur = ph_fill;
    ph_fill = -1;
    n_frames_in++;
    {
        double now = ph_buf[ph_cur].t;
        if (last_frame_ms > 0.0) {
            double dt = now - last_frame_ms;
            if (dt < frame_dt_min) frame_dt_min = dt;
            if (dt > frame_dt_max) frame_dt_max = dt;
        }
        last_frame_ms = now;
    }
    present_pending = 1;
    if (swap_mode == 0 && !shot_mode && hWnd) present_now();   /* vsync off: swap right here */
}

/* ---- --shot: render the newest frame through the same beam path into an
 * offscreen framebuffer and save it (.png via miniz, else .bmp) ---------- */
static int write_bmp(const char *path, const uint8_t *rgb_bottom_up, int w, int h)
{
    int pad = (4 - (w * 3) % 4) % 4;
    uint32_t data = (uint32_t)((w * 3 + pad) * h);
    uint8_t hdr[54];
    FILE *f;
    int y, x;
    memset(hdr, 0, sizeof hdr);
    hdr[0] = 'B'; hdr[1] = 'M';
    #define PUT32(o, v) do { hdr[o] = (uint8_t)(v); hdr[o+1] = (uint8_t)((v) >> 8); hdr[o+2] = (uint8_t)((v) >> 16); hdr[o+3] = (uint8_t)((v) >> 24); } while (0)
    PUT32(2, 54u + data);
    PUT32(10, 54u);
    PUT32(14, 40u);
    PUT32(18, (uint32_t)w);
    PUT32(22, (uint32_t)h);
    hdr[26] = 1; hdr[28] = 24;
    PUT32(34, data);
    #undef PUT32
    f = fopen(path, "wb");
    if (!f) return 1;
    fwrite(hdr, 1, sizeof hdr, f);
    for (y = 0; y < h; y++) {
        const uint8_t *row = rgb_bottom_up + (size_t)y * (size_t)w * 3u;
        for (x = 0; x < w; x++) {
            uint8_t px[3];
            px[0] = row[x * 3 + 2]; px[1] = row[x * 3 + 1]; px[2] = row[x * 3];
            fwrite(px, 1, 3, f);
        }
        if (pad) { static const uint8_t z[3] = { 0, 0, 0 }; fwrite(z, 1, (size_t)pad, f); }
    }
    fclose(f);
    return 0;
}

static int save_shot(const char *path)
{
    GLuint fbo = 0, rb = 0;
    uint8_t *buf;
    int rc = 1;
    size_t len = strlen(path);

    buf = (uint8_t *)malloc((size_t)shot_w * (size_t)shot_h * 3u);
    if (!buf) return 1;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, shot_w, shot_h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR("shot: framebuffer incomplete");
    } else {
        glViewport(0, 0, shot_w, shot_h);
        update_beam_width_for((double)shot_w);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ph_replay();
        glFinish();
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, shot_w, shot_h, GL_RGB, GL_UNSIGNED_BYTE, buf);
        if (len > 4 && _stricmp(path + len - 4, ".png") == 0) {
            size_t png_len = 0;
            void *png = tdefl_write_image_to_png_file_in_memory_ex(buf, shot_w, shot_h, 3, &png_len, 6, MZ_TRUE);
            if (png) {
                FILE *f = fopen(path, "wb");
                if (f) { rc = fwrite(png, 1, png_len, f) == png_len ? 0 : 1; fclose(f); }
                free(png);
            }
        } else {
            rc = write_bmp(path, buf, shot_w, shot_h);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteRenderbuffers(1, &rb);
    glDeleteFramebuffers(1, &fbo);
    ViewOrthoScaled(SCREEN_W, SCREEN_H, DESIGN_W, DESIGN_H);
    update_beam_width();
    free(buf);
    return rc;
}

/* ------------------------------------------------------------------ */
/* input                                                               */
/* ------------------------------------------------------------------ */

static double spin_acc;             /* fractional spinner counts carried */
static double input_last_ms;
static unsigned long ap_pass;       /* passes since tempest_app_init (autoplay clock) */
static unsigned long ap_mark;       /* the pass the current coin/start cycle began */
static int      ap_playing;

/* --hold / --spin: scripted switches by pass (cur_step; the power-on poll is step 0) */
static void apply_holds(plat_inputs *in)
{
    for (int i = 0; i < n_holds; i++) {
        const hold_req *h = &holds[i];
        if (cur_step < h->from || (h->to != 0 && cur_step >= h->to)) continue;
        switch (h->what) {
        case H_TEST:   in->test = 1; break;
        case H_DIAG:   in->diag = 1; break;
        case H_SLAM:   in->slam = 1; break;
        case H_FIRE:   in->fire = 1; break;
        case H_ZAP:    in->zap = 1; break;
        case H_START1: in->start1 = 1; break;
        case H_START2: in->start2 = 1; break;
        case H_COINL:  in->coin_l = 1; break;
        case H_COINC:  in->coin_c = 1; break;
        case H_COINR:  in->coin_r = 1; break;
        case H_SPIN:   in->spin_delta += h->val; break;
        default: break;
        }
    }
}

/* --autoplay: the self-test's script (tests\tempest_selftest.exe) - coin
 * (right mech), START 1, FIRE on RATE YOURSELF, then autofire + a steady spin;
 * initials are entered with fire pulses; after game over it coins again. */
static void autoplay_inputs(plat_inputs *in)
{
    unsigned long t = ap_pass - ap_mark;
    if (!ap_playing) {
        if (t >= 60 && t < 64)   in->coin_r = 1;
        if (t >= 260 && t < 263) in->start1 = 1;
        if (t >= 290 && t < 293) in->fire = 1;
        if (t >= 330 && win_probe_in_game()) ap_playing = 1;
        if (t > 900) ap_mark = ap_pass;              /* did not start: try again */
        return;
    }
    if (win_probe_getini()) {
        in->fire = (t % 10) >= 7;
        return;
    }
    if (!win_probe_in_game()) { ap_playing = 0; ap_mark = ap_pass; return; }
    in->fire = 1;
    in->spin_delta = autoplay_spin;
}

void plat_input_poll(plat_inputs *in)
{
    double now = plat_now_ms(), dt;
    double spin = 0.0;
    int mx = 0, my = 0;

    memset(in, 0, sizeof *in);
    dt = input_last_ms > 0.0 ? now - input_last_ms : 0.0;
    if (dt < 0.0) dt = 0.0;
    if (dt > 100.0) dt = 100.0;
    input_last_ms = now;

    in->test = (uint8_t)test_latch;
    if (autoplay || shot_mode || hidden_mode) {
        if (autoplay) autoplay_inputs(in);
        apply_holds(in);
        last_test = in->test;
        ap_pass++;
        in->quit = (uint8_t)g_quit;
        return;
    }

    /* Mouse X is the spinner; the raw mickeys accumulate between polls. */
    get_mouse_mickeys(&mx, &my);
    if (window_active) spin += (double)mx * mouse_sens;

    if (window_active) {
        in->fire   = (key[KEY_LCONTROL] || key[KEY_SPACE] || (mouse_b & 1)) ? 1 : 0;
        in->zap    = (key[KEY_ALT] || (mouse_b & 2)) ? 1 : 0;
        in->start1 = key[KEY_1] ? 1 : 0;
        in->start2 = key[KEY_2] ? 1 : 0;
        in->coin_l = key[KEY_5] ? 1 : 0;
        in->coin_c = key[KEY_6] ? 1 : 0;
        in->coin_r = key[KEY_7] ? 1 : 0;
        in->slam   = key[KEY_F3] ? 1 : 0;   /* M9 B5: slam (steps the self-test screens too) */
        in->diag   = key[KEY_F1] ? 1 : 0;   /* diagnostic step (held ~3 diag passes = one screen) */
        if (key[KEY_RIGHT]) spin += key_spin_rate * dt / 1000.0;
        if (key[KEY_LEFT])  spin -= key_spin_rate * dt / 1000.0;
    }
    in->quit = (key[KEY_ESC] || g_quit) ? 1 : 0;
    if (in->quit) g_quit = 1;

    /* Joystick: X axis spins (proportional), button 1 fire, 2 zap,
     * 7 coin, 8 start 1 (an Xbox pad's Back / Start). */
    poll_joystick();
    if (num_joysticks > 0 && joystick_is_connected(0)) {
        int ax = joy_x;
        if (ax != 0) spin += joy_spin_rate * ((double)ax / 127.0) * dt / 1000.0;
        if (joy[0].num_buttons > 0 && joy[0].button[0].b) in->fire   = 1;
        if (joy[0].num_buttons > 1 && joy[0].button[1].b) in->zap    = 1;
        if (joy[0].num_buttons > 6 && joy[0].button[6].b) in->coin_l = 1;
        if (joy[0].num_buttons > 7 && joy[0].button[7].b) in->start1 = 1;
    }

    /* Right (mouse or key) = the spinner counter counts DOWN (MAME's
     * IPF_REVERSE dial); [input] invert_spin swaps it. */
    if (!invert_spin) spin = -spin;
    spin_acc += spin;
    in->spin_delta = (int)spin_acc;
    spin_acc -= (double)in->spin_delta;
    apply_holds(in);
    last_test = in->test;
    ap_pass++;
}

/* ------------------------------------------------------------------ */
/* audio: the core's mixed mono POKEY stream                           */
/* ------------------------------------------------------------------ */

static unsigned long audio_pushes;
static int audio_peak;               /* largest |sample| pushed (log) */
static int audio_open_ok;

int plat_audio_open(int sample_rate)
{
    if (shot_mode) return -1;
    if (pokey_volume <= 0) {
        LOG_INFO("POKEY stream off ([sound] pokey_volume=0)");
        return -1;
    }
    if (stream_open(sample_rate, 1) != 0) {
        LOG_ERROR("audio: stream_open(%d Hz mono) failed; no sound", sample_rate);
        return -1;
    }
    stream_set_volume(hidden_mode ? 0 : mixer_percent_to_byte(pokey_volume));
    LOG_INFO("audio device open: XAudio2 stream, %d Hz mono int16, %d prime blocks, volume %d%%%s",
             sample_rate, audio_prime_blocks, pokey_volume, hidden_mode ? " (muted: --hidden)" : "");
    audio_open_ok = 1;
    return 0;
}

void plat_audio_push(const int16_t *pcm, int frames)
{
    audio_pushes++;
    for (int i = 0; i < frames; i++) {
        int v = pcm[i] < 0 ? -(int)pcm[i] : (int)pcm[i];
        if (v > audio_peak) audio_peak = v;
    }
    stream_push(pcm, frames);
}

void plat_audio_close(void)
{
    stream_close();
    audio_open_ok = 0;
}

/* ------------------------------------------------------------------ */
/* time                                                                */
/* ------------------------------------------------------------------ */

static double turbo_ms;

double plat_now_ms(void)
{
    LARGE_INTEGER cnt;
    static LARGE_INTEGER freq;
    if (shot_mode) { turbo_ms += 0.25; return turbo_ms; }
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    return (double)cnt.QuadPart * 1000.0 / (double)freq.QuadPart;
}

/* The core's idle hint (machine_idle, > 2 ms to the next IRQ): pump the
 * window, present the newest frame (a vsynced swap is the sleep), else sleep. */
void plat_sleep_ms(int ms)
{
    unsigned long swaps0 = n_swaps;
    if (shot_mode) { turbo_ms += (double)ms; return; }
    pump_messages();
    present_idle(1);
    if (n_swaps == swaps0 || swap_mode == 0) Sleep((DWORD)ms);
}

/* ------------------------------------------------------------------ */
/* NVRAM (EAROM): tempest.nv next to the exe                           */
/* ------------------------------------------------------------------ */

int plat_nvram_read(void *buf, unsigned len)
{
    size_t got;
    FILE *f;
    if (no_nvram) return 1;
    f = fopen(path_nv, "rb");
    if (!f) { LOG_INFO("EAROM: no %s, blank part ($FF)", path_nv); return 1; }
    got = fread(buf, 1, len, f);
    fclose(f);
    LOG_INFO("EAROM: read %s (%u bytes)", path_nv, (unsigned)got);
    return got == len ? 0 : 1;
}

int plat_nvram_write(const void *buf, unsigned len)
{
    FILE *f;
    if (no_nvram) return 0;
    f = fopen(path_nv, "wb");
    if (!f) { LOG_ERROR("EAROM: cannot write %s", path_nv); return 1; }
    fwrite(buf, 1, len, f);
    fclose(f);
    LOG_INFO("EAROM: wrote %s", path_nv);
    return 0;
}

/* ------------------------------------------------------------------ */
/* status + main loop                                                  */
/* ------------------------------------------------------------------ */

void plat_status_text(const char *s)
{
    char buf[320];
    char st[160];
    snprintf(buf, sizeof buf, APP_NAME "  %s  - " KEY_HELP, s);
    if (!shot_mode) set_window_title(buf);
    LOG_INFO("%s", s);
    if (audio_open_ok) {
        stream_stats(st, sizeof st);
        LOG_INFO("%s", st);
    }
}

static int parse_args(void)
{
    int i;
    for (i = 1; i < __argc; i++) {
        const char *a = __argv[i];
        if (!strcmp(a, "--shot") && i + 2 < __argc && n_shots < MAX_SHOTS) {
            shots[n_shots].pass = strtoul(__argv[i + 1], NULL, 0);
            snprintf(shots[n_shots].path, sizeof shots[n_shots].path, "%s", __argv[i + 2]);
            n_shots++;
            i += 2;
        } else if (!strcmp(a, "--shot-size") && i + 1 < __argc) {
            if (sscanf(__argv[++i], "%dx%d", &shot_w, &shot_h) != 2 || shot_w < 16 || shot_h < 16) {
                shot_w = 720; shot_h = 960;
            }
        } else if (!strcmp(a, "--autoplay")) {
            autoplay = 1;
        } else if (!strcmp(a, "--autoplay-spin") && i + 1 < __argc) {
            autoplay_spin = atoi(__argv[++i]);
        } else if (!strcmp(a, "--quit-after-ms") && i + 1 < __argc) {
            quit_after_ms = atof(__argv[++i]);
        } else if (!strcmp(a, "--pass-log") && i + 2 < __argc) {
            pass_log = fopen(__argv[i + 1], "w");
            pass_log_n = strtoul(__argv[i + 2], NULL, 0);
            if (!pass_log) return 1;
            i += 2;
        } else if (!strcmp(a, "--no-nvram")) {
            no_nvram = 1;
        } else if (!strcmp(a, "--hidden")) {
            hidden_mode = 1;
        } else if (!strcmp(a, "--nvram") && i + 1 < __argc) {
            snprintf(nv_override, sizeof nv_override, "%s", __argv[++i]);
        } else if (!strcmp(a, "--test")) {
            test_cli = 1;
        } else if (!strcmp(a, "--hold") && i + 3 < __argc && n_holds < MAX_HOLDS) {
            int w = hold_name(__argv[i + 1]);
            if (w < 0) return 1;
            holds[n_holds].what = w;
            holds[n_holds].from = strtoul(__argv[i + 2], NULL, 0);
            holds[n_holds].to = strtoul(__argv[i + 3], NULL, 0);
            n_holds++;
            i += 3;
        } else if (!strcmp(a, "--spin") && i + 3 < __argc && n_holds < MAX_HOLDS) {
            holds[n_holds].what = H_SPIN;
            holds[n_holds].val = atoi(__argv[i + 1]);
            holds[n_holds].from = strtoul(__argv[i + 2], NULL, 0);
            holds[n_holds].to = strtoul(__argv[i + 3], NULL, 0);
            n_holds++;
            i += 3;
        } else {
            return 1;
        }
    }
    if (hidden_mode && !nv_override[0]) no_nvram = 1;
    for (i = 1; i < n_shots; i++) {          /* in pass order */
        int j = i;
        while (j > 0 && shots[j - 1].pass > shots[j].pass) {
            shot_req t = shots[j]; shots[j] = shots[j - 1]; shots[j - 1] = t;
            j--;
        }
    }
    /* --pass-log alone runs on the turbo clock; with --quit-after-ms it runs in real time */
    if (n_shots > 0 || (pass_log && quit_after_ms <= 0.0)) shot_mode = 1;
    if (n_shots > 0 || pass_log) no_nvram = 1;
    if (autoplay) no_nvram = 1;
    if (nv_override[0]) no_nvram = 0;        /* an explicit image file is always used */
    if (shot_mode) hidden_mode = 0;
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int iCmdShow)
{
    double t_start, stat_t0;
    uint32_t stat_irq0, stat_pass0, irq_base = 0, pass_base = 0, last_irq = 0, last_pass = 0;
    unsigned long stat_swap0, stat_frames0, total_passes = 0;
    /* title bar fps: a rolling average over the last FPS_AVG_N one-second samples, so a
     * short stretch of an almost empty (fast) or crowded (slow) list does not make it jump */
#define FPS_AVG_N 5
    double fps_frames[FPS_AVG_N] = {0}, fps_secs[FPS_AVG_N] = {0}, fps_avg = 0.0;
    int fps_slot = 0;
    uint32_t irq_start;
    ULONGLONG tick_start;
    unsigned long stat_diag0 = 0, diag_start = 0;
    uint64_t stat_cyc0 = 0, cyc_start = 0;
    tempest_app_stats st0;
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)iCmdShow;

    if (parse_args()) {
        MessageBoxA(NULL,
            "usage: tempest_win.exe [--shot PASS FILE.png|.bmp]... [--shot-size WxH]\n"
            "                       [--autoplay] [--autoplay-spin N] [--quit-after-ms MS] [--no-nvram]\n"
            "                       [--test] [--hold test|diag|slam|fire|zap|start1|start2|coinl|coinc|coinr FROM TO]...\n"
            "                       [--spin N FROM TO]... [--hidden] [--nvram FILE] [--pass-log FILE N]",
            APP_NAME, MB_OK);
        return 2;
    }
    if (plat_init()) { log_close(); return 1; }
    tempest_app_set_fps_lock((double)fps_lock);
    tempest_app_set_vg_window(vg_window_ini);
    tempest_app_set_watchdog_cycles(wd_cycles_ini);
    tempest_app_init();
    LOG_INFO("power-on done: first loop head reached, %u IRQs on the boot clock", (unsigned)win_probe_irqs());

    t_start = stat_t0 = plat_now_ms();
    tick_start = GetTickCount64();
    stat_irq0 = win_probe_irqs(); stat_pass0 = win_probe_passes();
    irq_start = stat_irq0;
    last_irq = stat_irq0; last_pass = stat_pass0;
    stat_swap0 = n_swaps; stat_frames0 = n_frames_in;
    tempest_app_get_stats(&st0);
    stat_diag0 = diag_start = st0.diag_passes;
    stat_cyc0 = cyc_start = st0.machine_cycles;
    LOG_INFO("power-on: %s", st0.diag ? "TEST closed -> the ROM's self test (diag loop)" : "TEST open -> the game");

    while (!g_quit) {
        uint32_t irq, pass;
        if (!shot_mode) pump_messages();
        if (g_quit) break;

        {
            uint8_t qs = win_probe_qstate(), qd = win_probe_qdstate();
            uint32_t i0 = win_probe_irqs();
            cur_step = total_passes + 1;
            tempest_app_step(plat_now_ms());
            total_passes++;
            if (pass_log) {
                uint32_t i1 = win_probe_irqs();
                unsigned long r_, w_; uint32_t o_; double wk_;
                tempest_app_pass_info(&r_, &w_, &o_, &wk_);
                fprintf(pass_log, "%u %u %u %lu %lu %u %.0f\n", qs, qd, i1 >= i0 ? (unsigned)(i1 - i0) : 0u, r_, w_, (unsigned)o_, wk_);
                if (total_passes >= pass_log_n) g_quit = 1;
            }
        }
        present_idle(0);

        /* counters survive a watchdog re-boot (tempest_app_init / reset clear g) */
        irq = win_probe_irqs(); pass = win_probe_passes();
        if (irq < last_irq)   irq_base += last_irq;
        if (pass < last_pass) pass_base += last_pass;
        last_irq = irq; last_pass = pass;

        while (shots_done < n_shots && total_passes >= shots[shots_done].pass) {
            int rc = save_shot(shots[shots_done].path);
            LOG_INFO("shot: pass %lu (QSTATE $%02X, wave %u, %d segs, flip %02X) -> %s %s",
                     total_passes, win_probe_qstate(), win_probe_wave() + 1u, last_nsegs, cur_flip,
                     shots[shots_done].path, rc ? "FAILED" : "ok");
            shots_done++;
        }
        if (n_shots > 0 && shots_done >= n_shots) g_quit = 1;

        if (!shot_mode) {
            double now = plat_now_ms();
            if (now - stat_t0 >= 1000.0) {
                char s[320];
                double dt = (now - stat_t0) / 1000.0;
                uint32_t irq_all = irq_base + irq, pass_all = pass_base + pass;
                tempest_app_stats st;
                tempest_app_get_stats(&st);
                {
                    double fsum = 0.0, ssum = 0.0;
                    int k;
                    fps_frames[fps_slot] = (double)(n_frames_in - stat_frames0);
                    fps_secs[fps_slot] = dt;
                    fps_slot = (fps_slot + 1) % FPS_AVG_N;
                    for (k = 0; k < FPS_AVG_N; k++) { fsum += fps_frames[k]; ssum += fps_secs[k]; }
                    fps_avg = ssum > 0.0 ? fsum / ssum : 0.0;
                }
                if (st.diag) {
                    /* the self test's diag loop: no IRQ; passes and machine time instead */
                    snprintf(s, sizeof s,
                             "SELF TEST (TEST %s)  %.1f diag passes/s  machine/wall %.4f  %.1f fps avg %ds (%.1f now, %.1f swaps/s)  %d segs  "
                             "screen QSTATE %u  bites %lu",
                             last_test ? "on" : "OFF", (double)(st.diag_passes - stat_diag0) / dt,
                             (double)(st.machine_cycles - stat_cyc0) / 1512000.0 * (fps_lock > 0.0f ? 1512000.0 / 6144.0 / fps_lock : 1.0) / dt,
                             fps_avg, FPS_AVG_N, (double)(n_frames_in - stat_frames0) / dt, (double)(n_swaps - stat_swap0) / dt,
                             last_nsegs, win_probe_qstate(), st.watchdog_bites);
                } else {
                    snprintf(s, sizeof s,
                             "%.1f fps avg %ds (%.1f now, %.1f swaps/s)  %.1f IRQ/s  %.2f IRQs/pass  frame %.1f..%.1f ms  %d segs  "
                             "mouse %.2f keys %.0f/s  QSTATE $%02X wave %u  TEST %s",
                             fps_avg, FPS_AVG_N, (double)(n_frames_in - stat_frames0) / dt, (double)(n_swaps - stat_swap0) / dt,
                             (irq_all - stat_irq0) / dt,
                             pass_all > stat_pass0 ? (double)(irq_all - stat_irq0) / (double)(pass_all - stat_pass0) : 0.0,
                             frame_dt_max > 0.0 ? frame_dt_min : 0.0, frame_dt_max,
                             last_nsegs, mouse_sens, key_spin_rate, win_probe_qstate(), win_probe_wave() + 1u,
                             last_test ? "ON" : "off");
                }
                frame_dt_min = 1e9; frame_dt_max = 0.0;
                plat_status_text(s);
                stat_t0 = now;
                stat_irq0 = irq_all; stat_pass0 = pass_all;
                stat_swap0 = n_swaps; stat_frames0 = n_frames_in;
                stat_diag0 = st.diag_passes; stat_cyc0 = st.machine_cycles;
            }
            if (quit_after_ms > 0.0 && now - t_start >= quit_after_ms) {
                LOG_INFO("--quit-after-ms %.0f reached", quit_after_ms);
                g_quit = 1;
            }
        }
    }

    {
        double secs = (plat_now_ms() - t_start) / 1000.0;
        uint32_t irqs = irq_base + win_probe_irqs() - irq_start;
        LOG_INFO("exit: %lu passes, %u IRQs, %lu frames in, %lu swaps, %lu audio pushes (peak %d), "
                 "%.2f s -> %.2f IRQ/s, %.2f passes/s; lit segments %lu: %lu outside the tube (dropped), %lu cut at its edge",
                 total_passes, (unsigned)irqs, n_frames_in, n_swaps, audio_pushes, audio_peak,
                 secs, secs > 0.0 ? irqs / secs : 0.0, secs > 0.0 ? total_passes / secs : 0.0,
                 seg_total, seg_clipped, seg_cut);
    }
    {
        /* machine time (IRQs on the 6144-cycle grid) against the wall clock, and the
         * wall clock against GetTickCount64 (a second, independent timer) */
        double wall = (plat_now_ms() - t_start) / 1000.0;
        double tick = (double)(GetTickCount64() - tick_start) / 1000.0;
        double irq_hz = fps_lock > 0.0f ? (double)fps_lock : 1512000.0 / 6144.0;
        double mach = (double)(irq_base + win_probe_irqs() - irq_start) / irq_hz;
        LOG_INFO("pacing: machine %.3f s (IRQs / %.3f Hz) over wall %.3f s (QPC) / %.3f s (GetTickCount64): ratio %.4f",
                 mach, irq_hz, wall, tick, wall > 0.0 ? mach / wall : 0.0);
        {
            /* M9 B5: machine time from CPU cycles (counts the diag loop, which has no IRQ;
             * boots on the synthetic clock are inside it, see NOTES_m9.md B5) */
            tempest_app_stats st;
            double mcyc;
            tempest_app_get_stats(&st);
            mcyc = (double)(st.machine_cycles - cyc_start) / 1512000.0 * (1512000.0 / 6144.0 / irq_hz);
            LOG_INFO("pacing (cycles): machine %.3f s over wall %.3f s: ratio %.4f; diag passes %lu (%.2f/s), boots %lu "
                     "(self test %lu), watchdog bites %lu, JMP RESET %lu, soft-watchdog trips %lu",
                     mcyc, wall, wall > 0.0 ? mcyc / wall : 0.0, st.diag_passes - diag_start,
                     wall > 0.0 ? (double)(st.diag_passes - diag_start) / wall : 0.0, st.boots, st.selftest_boots,
                     st.watchdog_bites, st.jmp_resets, st.soft_watchdog_trips);
            /* the picture: the AVG's traversals of the looping list (app_loop.c vg_picture) */
            LOG_INFO("picture: %lu pictures, mean period %.2f ms = %.2f Hz (vg_window); %lu presented = %.2f per wall s, "
                     "%.2f per pass",
                     st.pictures, st.pictures ? (double)st.picture_cycles / (double)st.pictures / 1512.0 : 0.0,
                     st.picture_cycles ? 1512000.0 * (double)st.pictures / (double)st.picture_cycles : 0.0,
                     n_frames_in, wall > 0.0 ? (double)n_frames_in / wall : 0.0,
                     total_passes ? (double)n_frames_in / (double)total_passes : 0.0);
            for (int gm = 0; gm < 2; gm++) {
                unsigned long tot = 0;
                for (int k = 0; k < 10; k++) tot += st.picture_irqs[gm][k];
                LOG_INFO("picture (%s): %lu pictures, mean AVG draw time %.2f ms; by length in IRQs <=4:%lu 5:%lu 6:%lu 7:%lu 8:%lu 9+:%lu",
                         gm ? "in a game" : "attract", tot, tot ? st.picture_draw_ms[gm] / (double)tot : 0.0,
                         st.picture_irqs[gm][4], st.picture_irqs[gm][5], st.picture_irqs[gm][6], st.picture_irqs[gm][7],
                         st.picture_irqs[gm][8], st.picture_irqs[gm][9]);
            }
            {
                /* in a game, by game state and wave: what each kind of screen costs the AVG */
                const tempest_pic_row *rows;
                int nr = tempest_app_picture_rows(&rows);
                for (int k = 0; k < nr; k++)
                    if (rows[k].pictures >= 20) {
                        double mean = rows[k].draw_ms / (double)rows[k].pictures;
                        LOG_INFO("picture  QSTATE $%02X wave %2u: %5lu pictures, AVG draw time %.2f ms (min %.2f, max %.2f) = %.1f Hz",
                                 rows[k].qstate, rows[k].wave + 1u, rows[k].pictures, mean, rows[k].draw_min,
                                 rows[k].draw_max, 1000.0 / (mean > 6144.0 * 4.0 / 1512.0 ? mean : 6144.0 * 4.0 / 1512.0));
                    }
            }
        }
    }
    if (pass_log) fclose(pass_log);
    tempest_app_exit();
    plat_shutdown();
    return 0;
}
