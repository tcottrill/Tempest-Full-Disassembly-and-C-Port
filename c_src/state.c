/* state.c - the one machine-state instance and the computed-address
 * accessors (see state.h). */
#include "state.h"
#include "progrom.h"
#include "vecrom.h"

machine_state g;

uint8_t cpu_rd(uint16_t a)
{
    if (a < 0x0800) return g.ram[a];
    if (a >= 0x2000 && a < 0x3000) return g.vram[a - 0x2000];
    if (a >= 0x3000 && a < 0x4000) return vecrom[a - 0x3000];
    if (a >= 0x9000 && a < 0xE000) return progrom[a - 0x9000];
    if (a >= 0xE000) return progrom[a - 0x2000 - 0x9000];
    return 0x00;
}

void cpu_wr(uint16_t a, uint8_t v)
{
    if (a < 0x0800) { g.ram[a] = v; return; }
    if (a < 0x0810) { g.colram[a - 0x0800] = v; return; }
    if (a >= 0x2000 && a < 0x3000) { g.vram[a - 0x2000] = v; return; }
    g.stray_writes++;
}
