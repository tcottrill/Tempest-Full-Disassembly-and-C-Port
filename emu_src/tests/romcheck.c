/* romcheck.c - tempest_emu: the ROM loader's self-check (DESIGN.md, "ROM
 * loading": loading roms\tempest.zip must give byte-identical progrom,
 * vecrom, mb_map, mb_ucode to c_src's generated arrays).
 *
 *   obj\romcheck.exe [ZIP]        default ..\roms\tempest.zip (run from emu_src)
 *
 * Built by tests\build_tests.bat: emu_roms.c is compiled with its four image
 * symbols renamed (/Dprogrom=emu_progrom ...) so that they can sit in one
 * program with c_src's generated progrom.c / vecrom.c / mbprom.c, the arrays
 * tempest_win.exe and tests\refrun.exe run on.  Exit 0 = all four identical.
 * A set other than rev 3 (tempest1.zip ...) is expected to differ in progrom;
 * the first differing address is printed.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "progrom.h"            /* c_src: the generated arrays */
#include "vecrom.h"
#include "mbprom.h"
#include "../emu_roms.h"

extern uint8_t  emu_progrom[0x5000];
extern uint8_t  emu_vecrom[0x1000];
extern uint8_t  emu_mb_map[32];
extern uint32_t emu_mb_ucode[256];

static int compare(const char *name, const void *a, const void *b, size_t len, size_t elem, unsigned base)
{
    const uint8_t *pa = (const uint8_t *)a, *pb = (const uint8_t *)b;
    size_t i;
    if (memcmp(a, b, len) == 0) {
        printf("  %-8s %5u bytes  IDENTICAL\n", name, (unsigned)len);
        return 0;
    }
    for (i = 0; i < len && pa[i] == pb[i]; i++) {}
    printf("  %-8s %5u bytes  DIFFERENT, first at element %u ($%04X)\n", name, (unsigned)len,
           (unsigned)(i / elem), base + (unsigned)(i / elem));
    return 1;
}

int main(int argc, char **argv)
{
    const char *zip = argc > 1 ? argv[1] : "..\\roms\\tempest.zip";
    char err[1400];
    int bad = 0;

    if (emu_roms_load(zip, err, sizeof err) != 0) {
        printf("ROMCHECK: %s\n", err);
        return 2;
    }
    printf("ROMCHECK: %s = set '%s', %u CRC mismatch(es); against c_src's generated arrays:\n",
           zip, emu_roms_set_name(), emu_roms_crc_mismatches());
    bad += compare("progrom",  emu_progrom,  progrom,  sizeof emu_progrom,  1, 0x9000);
    bad += compare("vecrom",   emu_vecrom,   vecrom,   sizeof emu_vecrom,   1, 0x3000);
    bad += compare("mb_map",   emu_mb_map,   mb_map,   sizeof emu_mb_map,   1, 0);
    bad += compare("mb_ucode", emu_mb_ucode, mb_ucode, sizeof emu_mb_ucode, sizeof emu_mb_ucode[0], 0);
    printf("ROMCHECK: %s\n", bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}
