/* avgshapes.c - Gate V: avg.c renders every vector-ROM shape exactly like
 * ../disasm/emit_shapes.py / shapes_preview.html.
 *
 *   tests\avgshapes.exe [REF] [HTML]
 *     REF   default tests\ref\avg_shapes_ref.txt  (python tools\avg_ref.py shapes)
 *     HTML  default tests\avg_out\avg_shapes.html (inline-SVG preview)
 *
 * For each SHAPE record the reference gives the address, the start state
 * emit_shapes.py uses (scale, colour -1 = none, intensity 12) and the
 * segments disasm/vrender.py produced.  The walk here is avg_walk() with
 * AVG_OPT_PREVIEW over vecrom[] (vector RAM zeroed, never executed in that
 * mode).  "oneop" shapes (JSRL words the 6502 copies: LSYMBL, LSYMB0,
 * JSRDOT) are rendered like vrender's one_op: a JSRL into the ROM renders
 * its target, anything else executes that single instruction.
 * PASS = every shape has the same segment count and identical
 * (x0, y0, x1, y1, colour, intensity), exact integers in Q15.
 * Exit 0 on PASS.
 */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "avg.h"
#include "vecrom.h"
#include "progrom.h"

#define MAX_SEGS 8192

typedef struct { int32_t v[6]; } refseg;

static uint8_t zero_vram[0x1000];
static avg_seg got[MAX_SEGS];
static refseg  want[MAX_SEGS];

/* Wave-1 palette as disasm/vrender.py palette(mem, 0): COLTAB $C1FD low
 * nibbles -> 0-7, high nibbles -> 8-15, PDIWHI/YEL/RED nominal. */
static void palette(char pal[16][8])
{
    int i;
    for (i = 0; i < 16; i++) {
        uint8_t b = progrom[0xC1FD - 0x9000 + (i & 7)];
        uint8_t n = (uint8_t)(i < 8 ? (b & 0xF) : (b >> 4));
        uint32_t rgb = avg_colram_rgb(n);
        sprintf(pal[i], "#%02X%02X%02X", (unsigned)(rgb & 0xFF),
                (unsigned)((rgb >> 8) & 0xFF), (unsigned)((rgb >> 16) & 0xFF));
    }
    strcpy(pal[9], "#FFFFFF");
    strcpy(pal[10], "#FFFF00");
    strcpy(pal[11], "#FF3030");
}

static void svg_shape(FILE *h, const char *key, const char *name, const avg_seg *s,
                      int n, char pal[16][8], int ok)
{
    double minx = 0, miny = 0, maxx = 0, maxy = 0, w, hh, pad, sw;
    int i, have = 0;

    for (i = 0; i < n; i++) {
        double xs[2], ys[2];
        int k;
        if (s[i].intensity == 0) continue;
        xs[0] = AVG_Q15_TO_F(s[i].x0); xs[1] = AVG_Q15_TO_F(s[i].x1);
        ys[0] = AVG_Q15_TO_F(s[i].y0); ys[1] = AVG_Q15_TO_F(s[i].y1);
        for (k = 0; k < 2; k++) {
            if (!have) { minx = maxx = xs[k]; miny = maxy = ys[k]; have = 1; }
            if (xs[k] < minx) minx = xs[k];
            if (xs[k] > maxx) maxx = xs[k];
            if (ys[k] < miny) miny = ys[k];
            if (ys[k] > maxy) maxy = ys[k];
        }
    }
    w = maxx - minx; hh = maxy - miny;
    if (w < 1) w = 1;
    if (hh < 1) hh = 1;
    pad = (w > hh ? w : hh) * 0.08 + 1;
    sw = (w > hh ? w : hh) / 90.0;
    fprintf(h, "<div class='s%s'><svg viewBox='%g %g %g %g' width='104' height='104'>",
            ok ? "" : " bad", minx - pad, -maxy - pad, w + 2 * pad, hh + 2 * pad);
    for (i = 0; i < n; i++) {
        const char *c = s[i].color < 0 ? "#FFFFFF" : pal[s[i].color & 0xF];
        double op = s[i].intensity >= 15 ? 1.0 : 0.4 + 0.6 * s[i].intensity / 15.0;
        double x0 = AVG_Q15_TO_F(s[i].x0), y0 = -AVG_Q15_TO_F(s[i].y0);
        double x1 = AVG_Q15_TO_F(s[i].x1), y1 = -AVG_Q15_TO_F(s[i].y1);
        if (s[i].intensity == 0) continue;
        if (s[i].x0 == s[i].x1 && s[i].y0 == s[i].y1)
            fprintf(h, "<circle cx='%g' cy='%g' r='%g' fill='%s' opacity='%.2f'/>", x0, y0, sw * 1.5, c, op);
        else
            fprintf(h, "<line x1='%g' y1='%g' x2='%g' y2='%g' stroke='%s' stroke-width='%g' opacity='%.2f'/>",
                    x0, y0, x1, y1, c, sw, op);
    }
    fprintf(h, "</svg><br>%s<br>%s</div>\n", name, key);
}

int main(int argc, char **argv)
{
    const char *refp = argc > 1 ? argv[1] : "tests\\ref\\avg_shapes_ref.txt";
    const char *htmlp = argc > 2 ? argv[2] : "tests\\avg_out\\avg_shapes.html";
    FILE *f = fopen(refp, "r");
    FILE *h;
    char line[512], pal[16][8];
    avg_mem m;
    int nshapes = 0, nmatch = 0, nsegs = 0, maxdepth = 0, overflow = 0;

    if (!f) { fprintf(stderr, "cannot open %s\n", refp); return 2; }
    h = fopen(htmlp, "w");
    if (!h) { fprintf(stderr, "cannot write %s (create tests\\avg_out)\n", htmlp); return 2; }
    palette(pal);
    fprintf(h, "<!doctype html><meta charset='utf-8'><title>avg.c shapes (Gate V)</title>\n"
               "<style>body{background:#000;color:#0f0;font:12px monospace;margin:8px 16px}"
               ".s{display:inline-block;margin:4px;text-align:center;width:110px;vertical-align:top}"
               "svg{border:1px solid #333;background:#000}.bad svg{border-color:#f00}</style>\n"
               "<h3>Tempest vector ROM shapes rendered by c_src/avg.c (tests/avgshapes.exe)</h3>\n");

    m.vram = zero_vram;
    m.vrom = vecrom;
    m.colram = NULL;

    while (fgets(line, sizeof line, f)) {
        char key[32], mode[16], name[256];
        unsigned addr;
        int scale, color, inten, nseg, i, n = 0, ok = 1, pos = 0;
        avg_state st;
        avg_cfg cfg;
        avg_result r;

        if (strncmp(line, "SHAPE ", 6) != 0) continue;
        if (sscanf(line, "SHAPE %31s %x %15s %d %d %d %d %n", key, &addr, mode,
                   &scale, &color, &inten, &nseg, &pos) != 7 || nseg > MAX_SEGS) {
            fprintf(stderr, "bad record: %s", line);
            return 2;
        }
        strncpy(name, line + pos, sizeof name - 1);
        name[sizeof name - 1] = 0;
        name[strcspn(name, "\r\n")] = 0;
        for (i = 0; i < nseg; i++) {
            if (!fgets(line, sizeof line, f) ||
                sscanf(line, "%d %d %d %d %d %d", &want[i].v[0], &want[i].v[1], &want[i].v[2],
                       &want[i].v[3], &want[i].v[4], &want[i].v[5]) != 6) {
                fprintf(stderr, "bad segment in %s\n", key);
                return 2;
            }
        }

        avg_state_init(&st);
        st.scale_q15 = scale;
        st.color = color;
        st.intensity = inten;
        memset(&cfg, 0, sizeof cfg);
        cfg.segs = got;
        cfg.seg_cap = MAX_SEGS;
        cfg.max_ops = 4000;                    /* vrender.render max_ops */
        cfg.options = AVG_OPT_PREVIEW;
        if (strcmp(mode, "oneop") == 0) {
            unsigned w = vecrom[addr - 0x3000] | ((unsigned)vecrom[addr - 0x3000 + 1] << 8);
            unsigned t = 0x2000 + ((w & 0x1FFF) << 1);
            if ((w >> 13) == 5 && t >= 0x3000 && t <= 0x3FFF)
                r = avg_walk(&m, (uint16_t)t, &st, &cfg);
            else {
                cfg.max_ops = 1;
                r = avg_walk(&m, (uint16_t)addr, &st, &cfg);
            }
        } else {
            r = avg_walk(&m, (uint16_t)addr, &st, &cfg);
        }
        n = (int)r.nseg_stored;
        if (r.max_depth > maxdepth) maxdepth = r.max_depth;
        if (r.flags & AVG_FLAG_STACK_OVER) overflow++;

        if (n != nseg || r.nseg != (uint32_t)nseg) {
            ok = 0;
            printf("MISMATCH %s (%s): %d segments, reference %d\n", key, name, (int)r.nseg, nseg);
        } else {
            for (i = 0; i < n; i++) {
                int32_t gv[6];
                int k;
                gv[0] = got[i].x0; gv[1] = got[i].y0; gv[2] = got[i].x1; gv[3] = got[i].y1;
                gv[4] = got[i].color; gv[5] = got[i].intensity;
                for (k = 0; k < 6; k++) if (gv[k] != want[i].v[k]) break;
                if (k < 6) {
                    ok = 0;
                    printf("MISMATCH %s (%s) segment %d: got %d %d %d %d %d %d want %d %d %d %d %d %d\n",
                           key, name, i, gv[0], gv[1], gv[2], gv[3], gv[4], gv[5],
                           want[i].v[0], want[i].v[1], want[i].v[2], want[i].v[3], want[i].v[4], want[i].v[5]);
                    break;
                }
            }
        }
        nshapes++;
        nsegs += nseg;
        nmatch += ok;
        svg_shape(h, key, name, got, n, pal, ok);
    }
    fclose(f);
    fprintf(h, "<p>%d shapes, %d match the disasm/vrender.py reference</p>\n", nshapes, nmatch);
    fclose(h);

    printf("avgshapes: %d shapes, %d segments compared, %d match, %d differ; max JSRL depth %d, stack overflow in %d\n",
           nshapes, nsegs, nmatch, nshapes - nmatch, maxdepth, overflow);
    printf("preview: %s\n", htmlp);
    printf("GATE V: %s\n", (nshapes > 0 && nmatch == nshapes) ? "PASS" : "FAIL");
    return (nshapes > 0 && nmatch == nshapes) ? 0 : 1;
}
