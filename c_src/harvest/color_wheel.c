/* color_wheel.c - standalone AVG color/beam renderer diagnostic.
 *
 * KEPT DELIBERATELY. This started life as main_stub.c, the temporary
 * stand-in that proved the platform seam and the GL color path before any
 * game code existed. It is preserved here as a permanent diagnostic
 * because it is the only thing in the tree that exercises the renderer
 * ALONE - no ROM, no display list, no game state. When the game's picture
 * looks wrong, this answers the first question: is it the renderer or is
 * it the port?
 *
 * What it draws (all through the platform contract, nothing else):
 *   - a border, one edge per bright primary/secondary at full luminance;
 *     the shared corners also exercise the renderer's join discs
 *   - a rotating wheel of 8 sectors x 16 spokes: sector = AVG color 0-7,
 *     spoke position within a sector = luminance 0-15. Color 0 is black,
 *     so ITS SECTOR STAYING DARK IS PART OF THE TEST - a visible sector
 *     there means the color mapping is off by one.
 *   - a static swatch grid (8 colors x 4 luminances) for side-by-side
 *     eyeballing, since a rotating wheel is hard to judge
 *   - one zero-length segment per color: the renderer must draw these as
 *     dots (Space Duel's shots are zero-length vectors - the Omega Race
 *     port shipped a bug where int truncation collapsed them to nothing)
 *
 * The color mapping under test comes from the real AAE AVG engine:
 * bit2 = RED, bit1 = GREEN, bit0 = BLUE (0 black, 1 blue, 2 green,
 * 3 cyan, 4 red, 5 magenta, 6 yellow, 7 white), intensity (lum<<4)|0x0f.
 * See NOTES_platform.md for the provenance.
 *
 * Controls: Left/Right arrows change the spin rate, Esc quits.
 *
 * BUILD (it defines the same sd_app_* hooks as the game, so it links
 * against the platform backend INSTEAD of app_loop.c - never both):
 *
 *   cl /nologo /W4 /std:c11 /MD /D_CRT_SECURE_NO_WARNINGS /DUNICODE /D_UNICODE ^
 *      tests\color_wheel.c platform\windows\plat_win.c obj\*.obj ^
 *      /Fe:tests\color_wheel.exe /link user32.lib gdi32.lib winmm.lib
 *
 * (run build_win.bat first so obj\ holds the compiled vendored framework
 * objects; adjust if build_win.bat's layout has moved.)
 */
#include <math.h>
#include <stdio.h>
#include "platform/sd_platform.h"

#define PI_F 3.14159265358979f

/* The AVG screen window (see sd_platform.h): x 0..520, y 0..395, y up. */
#define SCR_W 520.0f
#define SCR_H 395.0f

static double next_frame_ms;      /* absolute schedule, ~60 fps         */
static double stat_ms;            /* last status-line update            */
static int    stat_frames;
static float  wheel_angle;        /* radians                            */
static float  wheel_rate = 0.5f;  /* radians/second                     */

void sd_app_init(void)
{
    next_frame_ms = 0.0;          /* first step draws immediately */
}

static void draw_frame(void)
{
    const float cx = SCR_W * 0.5f;
    const float cy = SCR_H * 0.5f;
    const float r0 = 40.0f;       /* spoke inner radius  */
    const float r1 = 150.0f;      /* spoke outer radius  */
    int c, l;

    plat_video_begin();

    /* Border: one edge per bright primary/secondary, full luminance.
     * Shared corners also exercise the renderer's join discs. */
    {
        const float m = 6.0f;
        plat_video_line(m, m, SCR_W - m, m, 4, 15);                  /* red    */
        plat_video_line(SCR_W - m, m, SCR_W - m, SCR_H - m, 2, 15);  /* green  */
        plat_video_line(SCR_W - m, SCR_H - m, m, SCR_H - m, 1, 15);  /* blue   */
        plat_video_line(m, SCR_H - m, m, m, 7, 15);                  /* white  */
    }

    /* Color wheel: 8 sectors of 16 spokes; sector = color 0..7, spoke
     * position within the sector = luminance 0..15. Color 0 (black)
     * must draw nothing - its sector staying dark is part of the test. */
    for (c = 0; c < 8; c++) {
        for (l = 0; l < 16; l++) {
            float a = wheel_angle + (float)(c * 16 + l) * (2.0f * PI_F / 128.0f);
            float ca = cosf(a), sa = sinf(a);
            plat_video_line(cx + r0 * ca, cy + r0 * sa,
                            cx + r1 * ca, cy + r1 * sa, c, l);
        }
    }

    /* Swatch grid, bottom-left: 8 columns (color) x 4 rows (lum 3, 7,
     * 11, 15) of short bars - the same data as the wheel but static and
     * side by side, easier to eyeball. */
    for (c = 0; c < 8; c++) {
        for (l = 0; l < 4; l++) {
            float x = 20.0f + (float)c * 22.0f;
            float y = 24.0f + (float)l * 10.0f;
            plat_video_line(x, y, x + 16.0f, y, c, l * 4 + 3);
        }
    }

    /* A dot (zero-length segment) per color, bottom-right. */
    for (c = 1; c < 8; c++) {
        float x = SCR_W - 20.0f - (float)(7 - c) * 14.0f;
        plat_video_line(x, 28.0f, x, 28.0f, c, 15);
    }

    plat_video_present();
}

double sd_app_step(double now_ms)
{
    plat_inputs in;

    if (now_ms < next_frame_ms)
        return next_frame_ms - now_ms;

    plat_input_poll(&in);
    if (in.rotate_left)  wheel_rate -= 0.02f;
    if (in.rotate_right) wheel_rate += 0.02f;
    wheel_angle += wheel_rate / 60.0f;
    if (wheel_angle > 2.0f * PI_F) wheel_angle -= 2.0f * PI_F;
    if (wheel_angle < 0.0f)        wheel_angle += 2.0f * PI_F;

    draw_frame();
    stat_frames++;

    if (now_ms - stat_ms >= 1000.0) {
        char buf[64];
        snprintf(buf, sizeof buf, "[color test  %d fps]", stat_frames);
        plat_status_text(buf);
        stat_ms = now_ms;
        stat_frames = 0;
    }

    /* ~60 fps absolute schedule; resync after a stall. */
    next_frame_ms += 1000.0 / 60.0;
    if (next_frame_ms < now_ms) next_frame_ms = now_ms + 1000.0 / 60.0;
    return 0.0;
}

void sd_app_exit(void)
{
    /* Nothing to flush: this diagnostic owns no NVRAM. */
}

void sd_app_set_fps_lock(double fps)
{
    (void)fps;  /* no machine here to underclock (plat_win.c calls this) */
}

void sd_app_set_dropped_frame(int irqs, double hold_ms)
{
    (void)irqs; (void)hold_ms;  /* no dropped frames here; the wheel
                                 * presents itself */
}

void sd_app_set_refresh(double hz)
{
    (void)hz;
}
