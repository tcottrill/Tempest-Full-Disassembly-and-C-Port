/* avgtime.c - how long the AVG takes to draw the oracle's display lists.
 *
 *   tests\avgtime.exe DIR [DIR ...]
 *
 * For every DIR\frame_NNNN.vram (tests\refrun.exe's dumps, one per MAINLN
 * pass captured): walks the list from $2000 with avg_walk() and prints the
 * cycle-true draw time (avg.h TIMING) - master cycles, milliseconds, the
 * refresh rate a looping list gives the monitor (1 / draw time) and the
 * draw time in IRQ periods (6144 CPU cycles = 49152 master cycles).  Ends
 * with min / mean / max per directory.
 */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "avg.h"
#include "vecrom.h"

#define IRQ_MASTER_CYC (6144.0 * 8.0)

static uint8_t vram[0x1000];

/* the port's picture rate (app_loop.c TP_VGW_CYCLES): the draw time, 4 IRQs at the least */
static double port_hz(double ms)
{
    double floor_ms = 4.0 * 6144.0 / 1512.0;
    return 1000.0 / (ms > floor_ms ? ms : floor_ms);
}

int main(int argc, char **argv)
{
    int a, quiet = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: avgtime [-q] DIR [DIR ...]\n");
        return 2;
    }
    for (a = 1; a < argc; a++) {
        int n, found = 0, misses = 0;
        double lo = 0.0, hi = 0.0, sum = 0.0;

        if (strcmp(argv[a], "-q") == 0) { quiet = 1; continue; }
        if (!quiet)
            printf("%-10s %-6s %6s %6s %9s %8s %7s %6s %8s\n", "frame", "stop", "ops", "segs", "cycles", "ms", "Hz", "IRQs", "port Hz");
        for (n = 0; n <= 99999 && misses < 2000; n++) {
            char path[512];
            FILE *f;
            avg_mem m;
            avg_result r;
            double ms;

            sprintf(path, "%s\\frame_%04d.vram", argv[a], n);
            f = fopen(path, "rb");
            if (f == NULL) { if (found) misses++; continue; }
            misses = 0;
            if (fread(vram, 1, sizeof vram, f) != sizeof vram) { fclose(f); continue; }
            fclose(f);
            m.vram = vram;
            m.vrom = vecrom;
            m.colram = NULL;
            r = avg_walk(&m, AVG_VRAM_BASE, NULL, NULL);
            ms = avg_cycles_ms(r.cycles);
            if (!quiet)
                printf("frame_%04d %-6s %6u %6u %9u %8.3f %7.2f %6.2f %8.2f\n", n, avg_stop_name(r.stop),
                       (unsigned)r.ops, (unsigned)r.nseg, (unsigned)r.cycles, ms,
                       ms > 0.0 ? 1000.0 / ms : 0.0, (double)r.cycles / IRQ_MASTER_CYC, port_hz(ms));
            if (r.stop != AVG_STOP_LOOP) continue;      /* HALT lists are not a refresh loop */
            if (!found || ms < lo) lo = ms;
            if (!found || ms > hi) hi = ms;
            sum += ms;
            found++;
        }
        if (found)
            printf("%s: %d looping lists, draw time min %.3f ms (%.2f Hz), mean %.3f ms (%.2f Hz), max %.3f ms (%.2f Hz)\n",
                   argv[a], found, lo, 1000.0 / lo, sum / found, 1000.0 * found / sum, hi, 1000.0 / hi);
        else
            printf("%s: no looping lists\n", argv[a]);
    }
    return 0;
}
