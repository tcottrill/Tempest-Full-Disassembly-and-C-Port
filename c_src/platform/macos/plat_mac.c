/* plat_mac.c - Tempest C port: the macOS backend (SDL2).
 *
 * Implements platform/tempest_platform.h on SDL 2: a 3:4 portrait window with
 * SDL_Renderer geometry for the colour vector beams, mouse / trackpad /
 * keyboard / game-controller input (the spinner), a callback audio device fed
 * from a ring, the four option switches, and the EAROM image in
 * ~/Library/Application Support/Tempest/.  Started from the Battlezone Mac
 * backend; the geometry, flip handling, spinner arithmetic, command line and
 * demo script follow plat_win.c.
 *
 * Links exactly one backend per binary; this file owns main().
 *
 * Keys:
 *   mouse / trackpad X, Left / Right   spinner        [ ]  mouse speed -/+ 10%
 *   Space, Ctrl, left click            fire           Shift+[ ]  key speed
 *   Option (Alt), right click, Z       superzapper    F8   mouse capture
 *   1 / 2                              start 1 / 2    Cmd+F, Alt+Return  fullscreen
 *   5 / 6 / 7                          coin L / C / R Esc, Cmd+Q  quit
 *   F2 or T    TEST switch (latching)  F1 or D  diag step (hold)
 *   F3 or S    slam
 * Game controller: left stick X = spinner, A fire, B/X zap, Back coin,
 * Start = start 1.
 *
 * Settings: ~/Library/Application Support/Tempest/tempest.ini (key=value,
 * the names tempest_win.ini uses, any section) and --dip KEY=WORD; raw option
 * bytes may be forced with TEMPEST_DSW_N13 / _L12 / _POKEY1 / _POKEY2 (hex).
 */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

#include <SDL.h>

#include "../tempest_platform.h"
#include "../windows/win_probe.h"      /* read-only machine peeks; no Windows code in it */

#define APP_NAME   "Tempest"
#define WIN_W      720                  /* logical picture, 3:4 portrait */
#define WIN_H      960
#define AVG_XMAX   290.0f               /* the visible tube, AVG units (plat_win.c) */
#define AVG_YMAX   285.0f
#define BEAM_W     2.0f                 /* beam width, logical pixels (plat_win.c linewidth) */
#define AUDIO_RING    16384             /* frames; power of two */
#define AUDIO_PREFILL 360               /* ~2 IRQ pushes before playback starts */
#define AUDIO_MAXFILL 6000              /* beyond this, drop the backlog (clock drift) */

/* ---- state ------------------------------------------------------------------ */
static SDL_Window   *g_win;
static SDL_Renderer *g_ren;
static int   g_quit, g_fullscreen, g_mute, g_stats;
static int   g_capture = 1;             /* hide + confine the mouse while focused (F8) */
static int   g_focus   = 1;
static int   g_test_latch;
static int   g_shot_mode;               /* any --shot PASS FILE: turbo clock, hidden window, no audio */
static int   g_shot_w = WIN_W, g_shot_h = WIN_H;
static int   g_demo;
static int   g_demo_spin = 3;
static int   g_no_nvram, g_nv_override;
static double g_quit_after_ms;
static double g_turbo_ms;
static unsigned long g_cur_step;        /* the pass main() is running (0 = power-on) */

#define MAX_SHOTS 8
typedef struct { unsigned long pass; double secs; char path[1024]; } shot_req;
static shot_req g_shots[MAX_SHOTS];
static int      g_nshots, g_shots_done;

static char g_support_dir[1024], g_path_ini[1100], g_path_nv[1100];

/* ---- --hold / --spin scripting (plat_win.c) ----------------------------------- */
enum { H_TEST, H_DIAG, H_SLAM, H_FIRE, H_ZAP, H_START1, H_START2, H_COINL, H_COINC, H_COINR, H_SPIN };
typedef struct { int what; unsigned long from, to; int val; } hold_req;
#define MAX_HOLDS 32
static hold_req g_holds[MAX_HOLDS];
static int      g_nholds;

static int hold_name(const char *s)
{
    static const char *const names[] = { "test", "diag", "slam", "fire", "zap", "start1", "start2", "coinl", "coinc", "coinr" };
    for (int i = 0; i < (int)(sizeof names / sizeof names[0]); i++)
        if (!strcmp(s, names[i])) return i;
    return -1;
}

/* ---- settings: tempest.ini + --dip -------------------------------------------- */
#define MAX_KV 64
typedef struct { char key[48], val[96]; } kv_t;
static kv_t g_kv[MAX_KV];
static int  g_nkv;

static void kv_set(const char *k, const char *v)
{
    for (int i = 0; i < g_nkv; i++)
        if (!strcasecmp(g_kv[i].key, k)) { snprintf(g_kv[i].val, sizeof g_kv[i].val, "%s", v); return; }
    if (g_nkv < MAX_KV) {
        snprintf(g_kv[g_nkv].key, sizeof g_kv[g_nkv].key, "%s", k);
        snprintf(g_kv[g_nkv].val, sizeof g_kv[g_nkv].val, "%s", v);
        g_nkv++;
    }
}

static const char *kv_get(const char *k)
{
    for (int i = 0; i < g_nkv; i++)
        if (!strcasecmp(g_kv[i].key, k)) return g_kv[i].val;
    return NULL;
}

static double kv_num(const char *k, double def)
{
    const char *v = kv_get(k);
    return v ? atof(v) : def;
}

static void load_ini(void)
{
    char line[256];
    FILE *f = fopen(g_path_ini, "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char *p = line, *eq, *e;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ';' || *p == '#' || *p == '[' || *p == '\n' || *p == '\0') continue;
        eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        for (e = eq - 1; e >= p && (*e == ' ' || *e == '\t'); e--) *e = '\0';
        for (p = eq + 1; *p == ' ' || *p == '\t'; p++) ;
        for (e = p + strlen(p); e > p && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t'); e--) e[-1] = '\0';
        kv_set(line[0] ? line : "", p);
    }
    fclose(f);
}

/* ---- option switches (word lists and bit values: plat_win.c read_dips) --------- */
static uint8_t dsw_n13, dsw_l12, dsw_pokey1, dsw_pokey2;
uint8_t plat_dsw_n13(void)    { return dsw_n13; }
uint8_t plat_dsw_l12(void)    { return dsw_l12; }
uint8_t plat_dsw_pokey1(void) { return dsw_pokey1; }
uint8_t plat_dsw_pokey2(void) { return dsw_pokey2; }

static uint8_t dip_pick(const char *name, const char *const *names, const uint8_t *vals, int n)
{
    const char *s = kv_get(name);
    if (s)
        for (int i = 0; i < n; i++)
            if (!strcasecmp(s, names[i])) return vals[i];
    if (s) fprintf(stderr, "dip %s=%s: unknown word, using %s\n", name, s, names[0]);
    return vals[0];
}

static void read_dips(void)
{
    static const char *const coin_n[]  = { "1_coin_1_play", "2_coins_1_play", "1_coin_2_plays", "free_play" };
    static const uint8_t     coin_v[]  = { 0x00, 0x01, 0x03, 0x02 };
    static const char *const right_n[] = { "x1", "x4", "x5", "x6" };
    static const uint8_t     right_v[] = { 0x00, 0x04, 0x08, 0x0C };
    static const char *const left_n[]  = { "x1", "x2" };
    static const uint8_t     left_v[]  = { 0x00, 0x10 };
    static const char *const bcoin_n[] = { "none", "1_each_5", "1_each_4", "1_each_3", "2_each_4", "1_each_2", "freeze" };
    static const uint8_t     bcoin_v[] = { 0x00, 0x80, 0x40, 0xA0, 0x60, 0x20, 0xC0 };
    static const char *const minc_n[]  = { "1", "2" };
    static const uint8_t     minc_v[]  = { 0x00, 0x01 };
    static const char *const lang_n[]  = { "english", "french", "german", "spanish" };
    static const uint8_t     lang_v[]  = { 0x00, 0x02, 0x04, 0x06 };
    static const char *const bonus_n[] = { "20000", "10000", "30000", "40000", "50000", "60000", "70000", "none" };
    static const uint8_t     bonus_v[] = { 0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38 };
    static const char *const lives_n[] = { "3", "4", "5", "2" };
    static const uint8_t     lives_v[] = { 0x00, 0x40, 0x80, 0xC0 };
    static const char *const diff_n[]  = { "medium1", "easy", "medium2", "hard" };
    static const uint8_t     diff_v[]  = { 0x00, 0x01, 0x03, 0x02 };
    static const char *const rate_n[]  = { "1_3_5_7_9", "tied_to_high_score" };
    static const uint8_t     rate_v[]  = { 0x00, 0x04 };
    static const char *const cab_n[]   = { "upright", "cocktail" };
    static const uint8_t     cab_v[]   = { 0x00, 0x10 };
    static const char *const opt_n[]   = { "off", "on" };
    static const uint8_t     opt_v[]   = { 0x00, 0x20 };
    const char *e;

    dsw_n13  = (uint8_t)(dip_pick("coinage", coin_n, coin_v, 4) | dip_pick("right_coin", right_n, right_v, 4) |
                         dip_pick("left_coin", left_n, left_v, 2) | dip_pick("bonus_coins", bcoin_n, bcoin_v, 7));
    dsw_l12  = (uint8_t)(dip_pick("minimum_credits", minc_n, minc_v, 2) | dip_pick("language", lang_n, lang_v, 4) |
                         dip_pick("bonus_life", bonus_n, bonus_v, 8) | dip_pick("lives", lives_n, lives_v, 4));
    dsw_pokey2 = (uint8_t)(dip_pick("difficulty", diff_n, diff_v, 4) | dip_pick("rating", rate_n, rate_v, 2));
    dsw_pokey1 = (uint8_t)(dip_pick("cabinet", cab_n, cab_v, 2) | dip_pick("special_option", opt_n, opt_v, 2));
    if ((e = getenv("TEMPEST_DSW_N13")))    dsw_n13    = (uint8_t)strtoul(e, NULL, 16);
    if ((e = getenv("TEMPEST_DSW_L12")))    dsw_l12    = (uint8_t)strtoul(e, NULL, 16);
    if ((e = getenv("TEMPEST_DSW_POKEY1"))) dsw_pokey1 = (uint8_t)strtoul(e, NULL, 16);
    if ((e = getenv("TEMPEST_DSW_POKEY2"))) dsw_pokey2 = (uint8_t)strtoul(e, NULL, 16);
}

/* ---- video ------------------------------------------------------------------- */
/* AVG space -> the portrait picture (plat_win.c xform_point): X mirrors when
 * the OUT0 $08 bit is set, Y when $10 is clear; 580 x 570 AVG units stretch to
 * the 3:4 picture, origin at its centre, +y up. */
static int g_honour_flip = 1, g_rot_sign = 1;
static uint8_t g_cur_flip;

typedef struct { SDL_Vertex *v; int n, cap; } vbuf_t;
static vbuf_t g_vb[2];
static int    g_fill = 0, g_have_cur;      /* g_vb[g_fill] being built; g_vb[!g_fill] the newest complete picture */
static int    g_last_nsegs;
static unsigned long g_frames, g_seg_total, g_seg_dropped, g_seg_cut;

static int clip_seg(float *x0, float *y0, float *x1, float *y1)
{
    /* Liang-Barsky against the tube; 0 = invisible, 2 = cut at the edge */
    float t0 = 0.0f, t1 = 1.0f, dx = *x1 - *x0, dy = *y1 - *y0;
    float p[4] = { -dx, dx, -dy, dy };
    float q[4] = { *x0 + AVG_XMAX, AVG_XMAX - *x0, *y0 + AVG_YMAX, AVG_YMAX - *y0 };
    for (int i = 0; i < 4; i++) {
        if (p[i] == 0.0f) { if (q[i] < 0.0f) return 0; continue; }
        float r = q[i] / p[i];
        if (p[i] < 0.0f) { if (r > t1) return 0; if (r > t0) t0 = r; }
        else             { if (r < t0) return 0; if (r < t1) t1 = r; }
    }
    if (t1 < 1.0f) { *x1 = *x0 + t1 * dx; *y1 = *y0 + t1 * dy; }
    if (t0 > 0.0f) { *x0 = *x0 + t0 * dx; *y0 = *y0 + t0 * dy; }
    return (t0 > 0.0f || t1 < 1.0f) ? 2 : 1;
}

static void to_pixels(float x, float y, float *px, float *py)
{
    if (g_honour_flip) {
        if (g_cur_flip & 0x08)    x = -x;
        if (!(g_cur_flip & 0x10)) y = -y;
    }
    *px = (float)WIN_W * 0.5f + (float)g_rot_sign * x * ((float)WIN_W * 0.5f / AVG_XMAX);
    *py = (float)WIN_H * 0.5f - (float)g_rot_sign * y * ((float)WIN_H * 0.5f / AVG_YMAX);
}

static void vtx_push(vbuf_t *b, float x, float y, SDL_Color c)
{
    if (b->n == b->cap) {
        int nc = b->cap ? b->cap * 2 : 32768;
        SDL_Vertex *nv = (SDL_Vertex *)realloc(b->v, (size_t)nc * sizeof *nv);
        if (!nv) return;
        b->v = nv; b->cap = nc;
    }
    SDL_Vertex *v = &b->v[b->n++];
    v->position.x = x; v->position.y = y;
    v->color = c;
    v->tex_coord.x = v->tex_coord.y = 0.0f;
}

void plat_video_begin(uint8_t flip)
{
    g_cur_flip = flip;
    g_vb[g_fill].n = 0;
}

void plat_video_line(float x0, float y0, float x1, float y1, uint32_t rgb, int intensity)
{
    vbuf_t *b = &g_vb[g_fill];
    int vis;
    g_seg_total++;
    vis = clip_seg(&x0, &y0, &x1, &y1);
    if (vis == 0) { g_seg_dropped++; return; }
    if (vis == 2) g_seg_cut++;
    if (intensity < 0) intensity = 0;
    if (intensity > 15) intensity = 15;
    /* brightness ((intensity << 4) | $0F) / 255, as AAE's AVG scales the 4-bit intensity */
    float br = (float)((intensity << 4) | 0x0F) / 255.0f;
    SDL_Color c = { (Uint8)((rgb & 0xFF) * br + 0.5f), (Uint8)(((rgb >> 8) & 0xFF) * br + 0.5f),
                    (Uint8)(((rgb >> 16) & 0xFF) * br + 0.5f), 255 };
    float ax, ay, bx, by;
    to_pixels(x0, y0, &ax, &ay);
    to_pixels(x1, y1, &bx, &by);
    float dx = bx - ax, dy = by - ay, len = sqrtf(dx * dx + dy * dy);
    float h = BEAM_W * 0.5f, ux, uy;
    if (len < 1e-3f) { ux = 1.0f; uy = 0.0f; }          /* a dot: a square of the beam */
    else             { ux = dx / len; uy = dy / len; }
    float nx = -uy * h, ny = ux * h;
    ax -= ux * h; ay -= uy * h;                         /* square end caps */
    bx += ux * h; by += uy * h;
    vtx_push(b, ax + nx, ay + ny, c); vtx_push(b, ax - nx, ay - ny, c); vtx_push(b, bx + nx, by + ny, c);
    vtx_push(b, bx + nx, by + ny, c); vtx_push(b, ax - nx, ay - ny, c); vtx_push(b, bx - nx, by - ny, c);
}

static void render_cur(void)
{
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
    SDL_RenderClear(g_ren);
    if (g_have_cur && g_vb[!g_fill].n) {
        SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_ADD);
        SDL_RenderGeometry(g_ren, NULL, g_vb[!g_fill].v, g_vb[!g_fill].n, NULL, 0);
    }
}

static void pump_events(void);

void plat_video_present(void)
{
    g_last_nsegs = g_vb[g_fill].n / 6;
    g_fill = !g_fill;                    /* the picture just built is now the newest complete one */
    g_have_cur = 1;
    g_frames++;
    if (g_shot_mode || !g_ren) return;   /* shots render on demand */
    render_cur();
    SDL_RenderPresent(g_ren);
}

/* ---- screenshots: the newest complete picture, through the same path, as BMP --- */
static int save_shot(const char *path)
{
    int w, h, rc = 1;
    SDL_Surface *sf;
    render_cur();
    SDL_GetRendererOutputSize(g_ren, &w, &h);
    sf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (sf && SDL_RenderReadPixels(g_ren, NULL, SDL_PIXELFORMAT_ARGB8888, sf->pixels, sf->pitch) == 0)
        rc = SDL_SaveBMP(sf, path);
    else
        fprintf(stderr, "shot: %s\n", SDL_GetError());
    if (sf) SDL_FreeSurface(sf);
    return rc;
}

/* ---- input ------------------------------------------------------------------- */
static plat_inputs g_sticky;             /* taps latched on key-down: a pass lasts ~37 ms */
static double g_mouse_dx;
static float  g_mouse_sens = 0.5f, g_key_spin = 180.0f, g_joy_spin = 180.0f;
static int    g_invert_spin;
static double g_spin_acc, g_input_last_ms;
static SDL_GameController *g_pad;
static unsigned long g_ap_pass, g_ap_mark;
static int    g_ap_playing;

static void apply_capture(void)
{
    int on = g_capture && g_focus && !g_shot_mode && !g_demo;
    SDL_SetRelativeMouseMode(on ? SDL_TRUE : SDL_FALSE);
}

static void set_fullscreen(int on)
{
    g_fullscreen = on;
    SDL_SetWindowFullscreen(g_win, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

static void open_pad(void)
{
    if (g_pad) return;
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i) && (g_pad = SDL_GameControllerOpen(i)) != NULL) break;
}

static void pump_events(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            g_quit = 1; break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)   { g_focus = 0; apply_capture(); }
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) { g_focus = 1; apply_capture(); }
            break;
        case SDL_MOUSEMOTION:
            if (SDL_GetRelativeMouseMode()) g_mouse_dx += e.motion.xrel;
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (!SDL_GetRelativeMouseMode() && g_capture && !g_demo) { g_focus = 1; apply_capture(); break; }
            if (e.button.button == SDL_BUTTON_LEFT)  g_sticky.fire = 1;
            if (e.button.button == SDL_BUTTON_RIGHT) g_sticky.zap = 1;
            break;
        case SDL_CONTROLLERDEVICEADDED:
            open_pad(); break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (g_pad && !SDL_GameControllerGetAttached(g_pad)) { SDL_GameControllerClose(g_pad); g_pad = NULL; }
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            switch (e.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_A:     g_sticky.fire = 1; break;
            case SDL_CONTROLLER_BUTTON_B:
            case SDL_CONTROLLER_BUTTON_X:     g_sticky.zap = 1; break;
            case SDL_CONTROLLER_BUTTON_BACK:  g_sticky.coin_l = 1; break;
            case SDL_CONTROLLER_BUTTON_START: g_sticky.start1 = 1; break;
            default: break;
            }
            break;
        case SDL_KEYDOWN: {
            SDL_Keycode k = e.key.keysym.sym;
            Uint16 m = e.key.keysym.mod;
            if (k == SDLK_ESCAPE) { g_quit = 1; break; }
            if (e.key.repeat) break;
            if ((k == SDLK_f && (m & KMOD_GUI)) || (k == SDLK_RETURN && (m & KMOD_ALT))) { set_fullscreen(!g_fullscreen); break; }
            if (k == SDLK_LEFTBRACKET || k == SDLK_RIGHTBRACKET) {
                float f = k == SDLK_RIGHTBRACKET ? 1.1f : 0.9f;
                if (m & KMOD_SHIFT) g_key_spin *= f; else g_mouse_sens *= f;
                break;
            }
            switch (e.key.keysym.scancode) {
            case SDL_SCANCODE_SPACE:
            case SDL_SCANCODE_LCTRL:  g_sticky.fire = 1; break;
            case SDL_SCANCODE_LALT:
            case SDL_SCANCODE_Z:      g_sticky.zap = 1; break;
            case SDL_SCANCODE_1:      g_sticky.start1 = 1; break;
            case SDL_SCANCODE_2:      g_sticky.start2 = 1; break;
            case SDL_SCANCODE_5:      g_sticky.coin_l = 1; break;
            case SDL_SCANCODE_6:      g_sticky.coin_c = 1; break;
            case SDL_SCANCODE_7:      g_sticky.coin_r = 1; break;
            case SDL_SCANCODE_F1:
            case SDL_SCANCODE_D:      g_sticky.diag = 1; break;
            case SDL_SCANCODE_F2:
            case SDL_SCANCODE_T:      g_test_latch = !g_test_latch; break;
            case SDL_SCANCODE_F3:
            case SDL_SCANCODE_S:      g_sticky.slam = 1; break;
            case SDL_SCANCODE_F8:     g_capture = !g_capture; apply_capture(); break;
            default: break;
            }
            break;
        }
        default: break;
        }
    }
}

/* --hold / --spin: scripted switches by pass number */
static void apply_holds(plat_inputs *in)
{
    for (int i = 0; i < g_nholds; i++) {
        const hold_req *h = &g_holds[i];
        if (g_cur_step < h->from || (h->to != 0 && g_cur_step >= h->to)) continue;
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

/* --demo: the self-test's script (plat_win.c autoplay): coin (right mech),
 * START 1, FIRE on RATE YOURSELF, then autofire and a steady spin; initials
 * are entered with fire pulses; after game over it coins again. */
static void demo_inputs(plat_inputs *in)
{
    unsigned long t = g_ap_pass - g_ap_mark;
    if (!g_ap_playing) {
        if (t >= 60 && t < 64)   in->coin_r = 1;
        if (t >= 260 && t < 263) in->start1 = 1;
        if (t >= 290 && t < 293) in->fire = 1;
        if (t >= 330 && win_probe_in_game()) g_ap_playing = 1;
        if (t > 900) g_ap_mark = g_ap_pass;
        return;
    }
    if (win_probe_getini()) { in->fire = (t % 10) >= 7; return; }
    if (!win_probe_in_game()) { g_ap_playing = 0; g_ap_mark = g_ap_pass; return; }
    in->fire = 1;
    in->spin_delta = g_demo_spin;
}

void plat_input_poll(plat_inputs *in)
{
    double now = plat_now_ms(), dt, spin = 0.0;

    memset(in, 0, sizeof *in);
    dt = g_input_last_ms > 0.0 ? now - g_input_last_ms : 0.0;
    if (dt < 0.0) dt = 0.0;
    if (dt > 100.0) dt = 100.0;
    g_input_last_ms = now;

    in->test = (uint8_t)g_test_latch;
    if (g_demo || g_shot_mode) {                    /* scripted: reads no keys */
        if (g_demo) demo_inputs(in);
        apply_holds(in);
        g_ap_pass++;
        in->quit = (uint8_t)g_quit;
        return;
    }

    SDL_PumpEvents();
    pump_events();
    {
        const Uint8 *ks = SDL_GetKeyboardState(NULL);
        in->fire   = (uint8_t)(ks[SDL_SCANCODE_SPACE] || ks[SDL_SCANCODE_LCTRL] || g_sticky.fire);
        in->zap    = (uint8_t)(ks[SDL_SCANCODE_LALT] || ks[SDL_SCANCODE_Z] || g_sticky.zap);
        in->start1 = (uint8_t)(ks[SDL_SCANCODE_1] || g_sticky.start1);
        in->start2 = (uint8_t)(ks[SDL_SCANCODE_2] || g_sticky.start2);
        in->coin_l = (uint8_t)(ks[SDL_SCANCODE_5] || g_sticky.coin_l);
        in->coin_c = (uint8_t)(ks[SDL_SCANCODE_6] || g_sticky.coin_c);
        in->coin_r = (uint8_t)(ks[SDL_SCANCODE_7] || g_sticky.coin_r);
        in->slam   = (uint8_t)(ks[SDL_SCANCODE_F3] || ks[SDL_SCANCODE_S] || g_sticky.slam);
        in->diag   = (uint8_t)(ks[SDL_SCANCODE_F1] || ks[SDL_SCANCODE_D] || g_sticky.diag);
        if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT) && SDL_GetRelativeMouseMode()) in->fire = 1;
        if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT) && SDL_GetRelativeMouseMode()) in->zap = 1;
        if (g_focus) {
            spin += g_mouse_dx * g_mouse_sens;
            if (ks[SDL_SCANCODE_RIGHT]) spin += g_key_spin * dt / 1000.0;
            if (ks[SDL_SCANCODE_LEFT])  spin -= g_key_spin * dt / 1000.0;
        }
    }
    g_mouse_dx = 0.0;

    if (g_pad) {
        int ax = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTX);
        if (ax > -4000 && ax < 4000) ax = 0;                     /* deadzone */
        spin += g_joy_spin * ((double)ax / 32767.0) * dt / 1000.0;
        if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_A)) in->fire = 1;
        if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_B) ||
            SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_X)) in->zap = 1;
    }
    memset(&g_sticky, 0, sizeof g_sticky);

    in->quit = (uint8_t)g_quit;

    /* Right (mouse or key) = the spinner counter counts DOWN (MAME's reversed
     * dial), as plat_win.c; invert_spin=1 swaps it. */
    if (!g_invert_spin) spin = -spin;
    g_spin_acc += spin;
    in->spin_delta = (int)g_spin_acc;
    g_spin_acc -= (double)in->spin_delta;
    apply_holds(in);
    g_ap_pass++;
}

/* ---- audio: the core's mixed mono POKEY stream --------------------------------- */
static SDL_AudioDeviceID g_adev;
static SDL_AudioSpec     g_aspec;
static int16_t           g_ring[AUDIO_RING];
static unsigned          g_rd, g_wr;           /* free-running frame counters */
static int               g_primed;
static unsigned long     g_pushes, g_underruns;
static int               g_peak;

static void audio_cb(void *ud, Uint8 *stream, int len)
{
    int16_t *out = (int16_t *)stream;
    int n = len / (int)sizeof(int16_t);
    (void)ud;
    for (int i = 0; i < n; i++) {
        int s = 0;
        if (!g_primed && (int)(g_wr - g_rd) >= AUDIO_PREFILL) g_primed = 1;
        if (g_primed) {
            if (g_wr == g_rd) { g_primed = 0; g_underruns++; }     /* underrun: rebuffer */
            else s = g_ring[g_rd++ & (AUDIO_RING - 1)];
        }
        out[i] = (int16_t)s;
    }
}

int plat_audio_open(int sample_rate)
{
    SDL_AudioSpec want;
    if (g_mute || g_shot_mode) return 1;
    if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return 1;
    SDL_zero(want);
    want.freq = sample_rate; want.format = AUDIO_S16SYS; want.channels = 1;
    want.samples = 512; want.callback = audio_cb;
    g_adev = SDL_OpenAudioDevice(NULL, 0, &want, &g_aspec, 0);
    if (!g_adev) { fprintf(stderr, "SDL audio: %s\n", SDL_GetError()); return 1; }
    if (g_aspec.format != AUDIO_S16SYS || g_aspec.channels != 1) {
        SDL_CloseAudioDevice(g_adev); g_adev = 0; return 1;
    }
    g_rd = g_wr = 0; g_primed = 0;
    SDL_PauseAudioDevice(g_adev, 0);
    return 0;
}

void plat_audio_push(const int16_t *pcm, int frames)
{
    if (!g_adev) return;
    g_pushes++;
    SDL_LockAudioDevice(g_adev);
    if ((int)(g_wr - g_rd) > AUDIO_MAXFILL) g_rd = g_wr - AUDIO_PREFILL;   /* device clock lags ours */
    for (int i = 0; i < frames; i++) {
        int v = pcm[i] < 0 ? -(int)pcm[i] : (int)pcm[i];
        if (v > g_peak) g_peak = v;
        g_ring[g_wr++ & (AUDIO_RING - 1)] = pcm[i];
    }
    SDL_UnlockAudioDevice(g_adev);
}

void plat_audio_close(void)
{
    if (g_adev) { SDL_CloseAudioDevice(g_adev); g_adev = 0; }
}

/* ---- time --------------------------------------------------------------------- */
double plat_now_ms(void)
{
    struct timespec ts;
    if (g_shot_mode) { g_turbo_ms += 0.25; return g_turbo_ms; }     /* turbo clock */
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

/* The core's idle hint (> 2 ms to the next IRQ): keep the window serviced and
 * never sleep past the next IRQ (tempest_app_idle_ms). */
void plat_sleep_ms(int ms)
{
    double idle;
    if (g_shot_mode) { g_turbo_ms += (double)ms; return; }
    idle = tempest_app_idle_ms();
    if (idle > 0.0 && (double)ms > idle - 1.0) ms = (int)(idle - 1.0);
    if (ms < 0) ms = 0;
    pump_events();
    if (ms > 0 && !g_quit) SDL_Delay((Uint32)ms);
}

/* ---- storage: the EAROM image --------------------------------------------------- */
static void make_support_dir(void)
{
    const char *home = getenv("HOME");
    char d1[1000], d2[1024];
    if (!home || !*home) home = ".";
    snprintf(d1, sizeof d1, "%s/Library/Application Support", home);
    snprintf(d2, sizeof d2, "%s/Tempest", d1);
    mkdir(d1, 0755);
    mkdir(d2, 0755);
    snprintf(g_support_dir, sizeof g_support_dir, "%s", d2);
    snprintf(g_path_ini, sizeof g_path_ini, "%s/tempest.ini", d2);
    if (!g_path_nv[0]) snprintf(g_path_nv, sizeof g_path_nv, "%s/tempest.nv", d2);
}

int plat_nvram_read(void *buf, unsigned len)
{
    size_t got;
    FILE *f;
    if (g_no_nvram) return 1;
    f = fopen(g_path_nv, "rb");
    if (!f) { fprintf(stderr, "EAROM: no %s, blank part ($FF)\n", g_path_nv); return 1; }
    got = fread(buf, 1, len, f);
    fclose(f);
    return got == len ? 0 : 1;
}

int plat_nvram_write(const void *buf, unsigned len)
{
    char tmp[1200];
    FILE *f;
    if (g_no_nvram) return 0;
    snprintf(tmp, sizeof tmp, "%s.tmp", g_path_nv);          /* write whole, then rename */
    f = fopen(tmp, "wb");
    if (!f) { fprintf(stderr, "EAROM: cannot write %s: %s\n", tmp, strerror(errno)); return 1; }
    if (fwrite(buf, 1, len, f) != len) { fclose(f); remove(tmp); return 1; }
    if (fclose(f) != 0 || rename(tmp, g_path_nv) != 0) { remove(tmp); return 1; }
    return 0;
}

/* ---- misc ----------------------------------------------------------------------- */
void plat_status_text(const char *s)
{
    if (g_win && !g_shot_mode) {
        char t[400];
        snprintf(t, sizeof t, APP_NAME "  mouse %.2f  keys %.0f/s%s%s", g_mouse_sens, g_key_spin,
                 g_stats ? "  " : "", g_stats ? s : "");
        SDL_SetWindowTitle(g_win, t);
    }
}

/* ---- lifecycle -------------------------------------------------------------------- */
int plat_init(void)
{
    Uint32 wf = SDL_WINDOW_ALLOW_HIGHDPI;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_SetHint(SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK, "0");
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
    wf |= g_shot_mode ? SDL_WINDOW_HIDDEN : SDL_WINDOW_RESIZABLE;
    g_win = SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             g_shot_mode ? g_shot_w : WIN_W * 3 / 4,
                             g_shot_mode ? g_shot_h : WIN_H * 3 / 4, wf);
    if (!g_win) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }
    g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED);
    if (!g_ren) g_ren = SDL_CreateRenderer(g_win, -1, 0);
    if (!g_ren) { fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); return 1; }
    SDL_RenderSetLogicalSize(g_ren, WIN_W, WIN_H);
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
    SDL_RenderClear(g_ren);
    SDL_RenderPresent(g_ren);
    open_pad();
    if (g_fullscreen) set_fullscreen(1);
    apply_capture();
    return 0;
}

void plat_shutdown(void)
{
    SDL_SetRelativeMouseMode(SDL_FALSE);
    if (g_pad) { SDL_GameControllerClose(g_pad); g_pad = NULL; }
    plat_audio_close();
    free(g_vb[0].v); free(g_vb[1].v);
    g_vb[0].v = g_vb[1].v = NULL;
    if (g_ren) SDL_DestroyRenderer(g_ren);
    if (g_win) SDL_DestroyWindow(g_win);
    g_ren = NULL; g_win = NULL;
    SDL_Quit();
}

/* ---- main --------------------------------------------------------------------------- */
static int is_number(const char *s)
{
    char *end;
    if (!*s) return 0;
    strtod(s, &end);
    return *end == '\0';
}

static int usage(const char *prog)
{
    fprintf(stderr,
        "usage: %s [--fullscreen] [--mute] [--stats] [--test] [--dip KEY=WORD]...\n"
        "          [--demo] [--demo-spin N] [--quit-after-ms MS] [--no-nvram] [--nvram FILE]\n"
        "          [--hold test|diag|slam|fire|zap|start1|start2|coinl|coinc|coinr FROM TO]...\n"
        "          [--spin N FROM TO]...\n"
        "          [--shot PASS FILE.bmp]... [--shot-size WxH]   headless, turbo clock, by pass number\n"
        "          [--shot FILE.bmp SECONDS]                      real time, saved after SECONDS\n", prog);
    return 2;
}

int main(int argc, char **argv)
{
    unsigned long total_passes = 0;
    double t_start, stat_t0;
    unsigned long stat_frames0 = 0;
    uint32_t stat_irq0;
    int i;

    make_support_dir();
    load_ini();
    /* command line: a first pass for --nvram (path) and --dip before the settings are read */
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--fullscreen") || !strcmp(a, "-f")) g_fullscreen = 1;
        else if (!strcmp(a, "--mute")) g_mute = 1;
        else if (!strcmp(a, "--stats")) g_stats = 1;
        else if (!strcmp(a, "--test")) g_test_latch = 1;
        else if (!strcmp(a, "--demo") || !strcmp(a, "--autoplay")) g_demo = 1;
        else if (!strcmp(a, "--demo-spin") && i + 1 < argc) g_demo_spin = atoi(argv[++i]);
        else if (!strcmp(a, "--quit-after-ms") && i + 1 < argc) g_quit_after_ms = atof(argv[++i]);
        else if (!strcmp(a, "--no-nvram")) g_no_nvram = 1;
        else if (!strcmp(a, "--nvram") && i + 1 < argc) { snprintf(g_path_nv, sizeof g_path_nv, "%s", argv[++i]); g_nv_override = 1; }
        else if (!strcmp(a, "--dip") && i + 1 < argc) {
            char *eq = strchr(argv[++i], '=');
            if (!eq) return usage(argv[0]);
            *eq = '\0';
            kv_set(argv[i], eq + 1);
        }
        else if (!strcmp(a, "--shot-size") && i + 1 < argc) {
            if (sscanf(argv[++i], "%dx%d", &g_shot_w, &g_shot_h) != 2 || g_shot_w < 16 || g_shot_h < 16) return usage(argv[0]);
        }
        else if (!strcmp(a, "--shot") && i + 2 < argc && g_nshots < MAX_SHOTS) {
            shot_req *s = &g_shots[g_nshots++];
            if (is_number(argv[i + 1]) && !is_number(argv[i + 2])) {           /* PASS FILE */
                s->pass = strtoul(argv[i + 1], NULL, 0);
                snprintf(s->path, sizeof s->path, "%s", argv[i + 2]);
                g_shot_mode = 1;
            } else if (is_number(argv[i + 2])) {                               /* FILE SECONDS */
                snprintf(s->path, sizeof s->path, "%s", argv[i + 1]);
                s->secs = atof(argv[i + 2]);
            } else return usage(argv[0]);
            i += 2;
        }
        else if (!strcmp(a, "--hold") && i + 3 < argc && g_nholds < MAX_HOLDS) {
            int w = hold_name(argv[i + 1]);
            if (w < 0) return usage(argv[0]);
            g_holds[g_nholds].what = w;
            g_holds[g_nholds].from = strtoul(argv[i + 2], NULL, 0);
            g_holds[g_nholds].to = strtoul(argv[i + 3], NULL, 0);
            g_nholds++; i += 3;
        }
        else if (!strcmp(a, "--spin") && i + 3 < argc && g_nholds < MAX_HOLDS) {
            g_holds[g_nholds].what = H_SPIN;
            g_holds[g_nholds].val = atoi(argv[i + 1]);
            g_holds[g_nholds].from = strtoul(argv[i + 2], NULL, 0);
            g_holds[g_nholds].to = strtoul(argv[i + 3], NULL, 0);
            g_nholds++; i += 3;
        }
        else if (!strncmp(a, "-psn_", 5)) continue;                           /* Finder launch */
        else return usage(argv[0]);
    }
    /* shots in pass order */
    for (i = 1; i < g_nshots; i++)
        for (int j = i; j > 0 && g_shots[j - 1].pass > g_shots[j].pass; j--) {
            shot_req t = g_shots[j]; g_shots[j] = g_shots[j - 1]; g_shots[j - 1] = t;
        }
    /* scripted runs never touch the saved EAROM, unless --nvram names a file */
    if ((g_shot_mode || g_demo) && !g_nv_override) g_no_nvram = 1;
    if (g_nv_override) g_no_nvram = 0;

    g_mouse_sens  = (float)kv_num("mouse_sensitivity", g_mouse_sens);
    g_key_spin    = (float)kv_num("key_spin_rate", g_key_spin);
    g_joy_spin    = (float)kv_num("joy_spin_rate", g_joy_spin);
    g_invert_spin = kv_num("invert_spin", 0) != 0;
    g_honour_flip = kv_num("honour_flip", 1) != 0;
    g_rot_sign    = kv_num("rotate_sign", 1) < 0 ? -1 : 1;
    if (kv_num("test_switch", 0) != 0) g_test_latch = 1;
    read_dips();

    {
        double fps_lock = kv_num("fps_lock", 0.0);
        const char *vgw = kv_get("vg_window");
        if (fps_lock < 0.0 || (fps_lock > 0.0 && (fps_lock < 100.0 || fps_lock > 1000.0))) fps_lock = 0.0;
        if (plat_init()) return 1;
        tempest_app_set_fps_lock(fps_lock);
        tempest_app_set_vg_window(vgw && !strcasecmp(vgw, "free") ? TP_VGW_FREE : TP_VGW_CYCLES);
        tempest_app_set_watchdog_cycles(kv_num("watchdog_cycles", 1134000.0));
    }
    fprintf(stderr, "option switches: N13=%02X L12=%02X POKEY1=%02X POKEY2=%02X; EAROM %s\n",
            dsw_n13, dsw_l12, dsw_pokey1, dsw_pokey2, g_no_nvram ? "(not used)" : g_path_nv);

    tempest_app_init();
    t_start = stat_t0 = plat_now_ms();
    stat_irq0 = win_probe_irqs();

    while (!g_quit) {
        g_cur_step = total_passes + 1;
        tempest_app_step(plat_now_ms());
        total_passes++;
        if (!g_shot_mode) pump_events();

        /* --shot PASS FILE (turbo clock) and --shot FILE SECONDS (real time) */
        while (g_shots_done < g_nshots) {
            const shot_req *s = &g_shots[g_shots_done];
            int due = g_shot_mode ? total_passes >= s->pass : plat_now_ms() - t_start >= s->secs * 1000.0;
            int rc;
            if (!due) break;
            rc = save_shot(s->path);
            fprintf(stderr, "shot: pass %lu (QSTATE $%02X, %s, CURWAV %u, %d segs, flip %02X) -> %s %s\n",
                    total_passes, win_probe_qstate(), win_probe_in_game() ? "in game" : "attract", win_probe_wave(), g_last_nsegs, g_cur_flip,
                    s->path, rc ? "FAILED" : "ok");
            g_shots_done++;
        }
        if (g_nshots > 0 && g_shots_done >= g_nshots) g_quit = 1;

        if (!g_shot_mode) {
            double now = plat_now_ms();
            if (now - stat_t0 >= 1000.0) {
                char s[200];
                double dt = (now - stat_t0) / 1000.0;
                uint32_t irq = win_probe_irqs();
                snprintf(s, sizeof s, "%.1f pictures/s  %.1f IRQ/s  %d segs  QSTATE $%02X wave %u  TEST %s",
                         (double)(g_frames - stat_frames0) / dt, irq >= stat_irq0 ? (irq - stat_irq0) / dt : 0.0,
                         g_last_nsegs, win_probe_qstate(), win_probe_wave() + 1u, g_test_latch ? "ON" : "off");
                plat_status_text(s);
                stat_t0 = now; stat_irq0 = irq; stat_frames0 = g_frames;
            }
            if (g_quit_after_ms > 0.0 && now - t_start >= g_quit_after_ms) g_quit = 1;
        }
    }

    {
        double secs = (plat_now_ms() - t_start) / 1000.0;
        fprintf(stderr, "exit: %lu passes, %lu pictures, %lu audio pushes (peak %d, underruns %lu), %.2f s (%s clock); "
                "lit segments %lu: %lu outside the tube (dropped), %lu cut at its edge\n",
                total_passes, g_frames, g_pushes, g_peak, g_underruns, secs, g_shot_mode ? "turbo" : "real",
                g_seg_total, g_seg_dropped, g_seg_cut);
    }
    tempest_app_exit();
    plat_shutdown();
    return 0;
}
