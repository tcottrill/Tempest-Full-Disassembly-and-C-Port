/* avgframe.c - frame check for avg.c over the oracle's vector RAM dumps.
 *
 *   tests\avgframe.exe OUTDIR NNNN [NNNN ...]
 *
 * For each tests\ref\frame_NNNN.vram: walks the display list from $2000 with
 * avg_walk() over a plain memory struct AND with avg_run_frame() over the
 * machine state g (the dump copied into g.vram) - the two must agree - then
 * writes OUTDIR\frame_NNNN.c.txt (segments + summary, the format of
 * tools\avg_ref.py frame, so the two diff directly) and OUTDIR\frame_NNNN.svg.
 * The SVG colours come from the dumped colour RAM (frame_NNNN.col, via
 * avg_colram_rgb); for older dumps without it, the wave-1 palette (COLTAB $C1FD).
 */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "avg.h"
#include "state.h"
#include "vecrom.h"
#include "progrom.h"

#define MAX_SEGS 20000

static avg_seg segs[MAX_SEGS];
static avg_seg segs_g[MAX_SEGS];
static uint8_t vram[0x1000];

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

int main(int argc, char **argv)
{
    char pal[16][8], path[512];
    int a, bad = 0;

    if (argc < 3) {
        fprintf(stderr, "usage: avgframe OUTDIR NNNN [NNNN ...]\n");
        return 2;
    }
    for (a = 2; a < argc; a++) {
        int n = atoi(argv[a]);
        FILE *f;
        avg_mem m;
        avg_cfg cfg;
        avg_result r, rg;
        uint32_t i;
        int same;

        sprintf(path, "tests\\ref\\frame_%04d.vram", n);
        f = fopen(path, "rb");
        if (!f || fread(vram, 1, sizeof vram, f) != sizeof vram) {
            fprintf(stderr, "cannot read %s\n", path);
            return 2;
        }
        fclose(f);

        /* real colour RAM when refrun dumped it (frame_NNNN.col), else wave-1 palette */
        palette(pal);
        sprintf(path, "tests\\ref\\frame_%04d.col", n);
        f = fopen(path, "rb");
        if (f) {
            uint8_t col[16];
            if (fread(col, 1, sizeof col, f) == sizeof col) {
                int k;
                for (k = 0; k < 16; k++) {
                    uint32_t rgb = avg_colram_rgb(col[k]);
                    sprintf(pal[k], "#%02X%02X%02X", (unsigned)(rgb & 0xFF),
                            (unsigned)((rgb >> 8) & 0xFF), (unsigned)((rgb >> 16) & 0xFF));
                }
            }
            fclose(f);
        }

        m.vram = vram;
        m.vrom = vecrom;
        m.colram = NULL;
        memset(&cfg, 0, sizeof cfg);
        cfg.segs = segs;
        cfg.seg_cap = MAX_SEGS;
        r = avg_walk(&m, AVG_VRAM_BASE, NULL, &cfg);

        memcpy(g.vram, vram, sizeof vram);
        cfg.segs = segs_g;
        rg = avg_run_frame(&cfg);
        same = rg.stop == r.stop && rg.ops == r.ops && rg.nseg == r.nseg && rg.nlit == r.nlit;
        for (i = 0; same && i < r.nseg_stored; i++)
            same = segs[i].x0 == segs_g[i].x0 && segs[i].y0 == segs_g[i].y0 &&
                   segs[i].x1 == segs_g[i].x1 && segs[i].y1 == segs_g[i].y1 &&
                   segs[i].color == segs_g[i].color && segs[i].intensity == segs_g[i].intensity;

        sprintf(path, "%s\\frame_%04d.c.txt", argv[1], n);
        f = fopen(path, "w");
        if (!f) { fprintf(stderr, "cannot write %s\n", path); return 2; }
        for (i = 0; i < r.nseg_stored; i++)
            fprintf(f, "%d %d %d %d %d %d\n", segs[i].x0, segs[i].y0, segs[i].x1, segs[i].y1,
                    segs[i].color, segs[i].intensity);
        fprintf(f, "STOP %s ops=%u seg=%u lit=%u z=%u depth=%d over=%d bbox=",
                avg_stop_name(r.stop), r.ops, r.nseg, r.nlit, r.nz, r.max_depth,
                (r.flags & AVG_FLAG_STACK_OVER) ? 1 : 0);
        if (r.have_bbox) fprintf(f, "%d %d %d %d\n", r.minx, r.miny, r.maxx, r.maxy);
        else fprintf(f, "none\n");
        fclose(f);

        sprintf(path, "%s\\frame_%04d.svg", argv[1], n);
        f = fopen(path, "w");
        if (!f) { fprintf(stderr, "cannot write %s\n", path); return 2; }
        {
            double minx = AVG_Q15_TO_F(r.minx), maxx = AVG_Q15_TO_F(r.maxx);
            double miny = AVG_Q15_TO_F(r.miny), maxy = AVG_Q15_TO_F(r.maxy);
            double pad = 8, w = maxx - minx + 2 * pad, h = maxy - miny + 2 * pad;
            fprintf(f, "<svg xmlns='http://www.w3.org/2000/svg' viewBox='%g %g %g %g' width='%d' height='%d'"
                       " style='background:#000'>\n<title>frame_%04d: %s, %u segments (%u lit)</title>\n"
                       "<rect x='%g' y='%g' width='%g' height='%g' fill='#000'/>\n",
                    minx - pad, -maxy - pad, w, h, 720, (int)(720.0 * h / (w > 1 ? w : 1)),
                    n, avg_stop_name(r.stop), r.nseg, r.nlit, minx - pad, -maxy - pad, w, h);
            for (i = 0; i < r.nseg_stored; i++) {
                const avg_seg *s = &segs[i];
                const char *c = s->color < 0 ? "#FFFFFF" : pal[s->color & 0xF];
                double op = s->intensity >= 15 ? 1.0 : 0.35 + 0.65 * s->intensity / 15.0;
                if (s->intensity == 0) continue;
                if (s->x0 == s->x1 && s->y0 == s->y1)
                    fprintf(f, "<circle cx='%.3f' cy='%.3f' r='1.2' fill='%s' opacity='%.2f'/>\n",
                            AVG_Q15_TO_F(s->x0), -AVG_Q15_TO_F(s->y0), c, op);
                else
                    fprintf(f, "<line x1='%.3f' y1='%.3f' x2='%.3f' y2='%.3f' stroke='%s' stroke-width='1' opacity='%.2f'/>\n",
                            AVG_Q15_TO_F(s->x0), -AVG_Q15_TO_F(s->y0),
                            AVG_Q15_TO_F(s->x1), -AVG_Q15_TO_F(s->y1), c, op);
            }
            fprintf(f, "</svg>\n");
        }
        fclose(f);

        printf("frame %04d: STOP %s ops=%u seg=%u lit=%u z=%u depth=%d bbox=%d %d %d %d | g walk %s\n",
               n, avg_stop_name(r.stop), r.ops, r.nseg, r.nlit, r.nz, r.max_depth,
               r.minx, r.miny, r.maxx, r.maxy, same ? "same" : "DIFFERENT");
        bad += !same || r.nseg_stored != r.nseg;
    }
    return bad ? 1 : 0;
}
