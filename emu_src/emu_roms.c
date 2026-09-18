/* emu_roms.c - tempest_emu: the ROM images, loaded from a MAME Tempest zip.
 *
 * tempest_win.exe links the generated arrays progrom.c / vecrom.c / mbprom.c
 * (c_src\tools\gen_roms.py, from the rev-3 set).  The emulator links THIS file
 * instead: the same four symbols, as writable storage, filled at start-up
 * from roms\tempest.zip with the Windows backend's miniz
 * (c_src\platform\windows\miniz.c).  The shared files that read them - state.c
 * (cpu_rd), avg.c (the vector ROM), mathbox.c (the mapping PROM and the
 * microcode) - are compiled unchanged from ..\c_src and see `extern const`
 * declarations; the definitions below have no `const`, which is why this file
 * must not include progrom.h / vecrom.h / mbprom.h (C linkage: the symbol
 * names are identical, the linker binds them).
 *
 * Layouts (file name -> CPU address; MAME 0.286 src\mame\atari\tempest.cpp,
 * and for the rev-3 sets disasm\gen_from_roms.py, whose CRC32 table is the
 * source of the rev-3 CRCs below; the other CRCs are MAME's):
 *   tempest   rev 3, 4K:  133.d1 $9000  134.f1 $A000  235.j1 $B000
 *                          136.lm1 $C000  237.p1 $D000;  vector 138.np3 $3000
 *   tempest1r rev 1, 4K:  as tempest with 135.j1 $B000, 137.p1 $D000
 *   tempest3  rev 3, 2K:  113.d1 $9000  114.e1 $9800  115.f1 $A000  316.h1 $A800
 *                          217.j1 $B000  118.k1 $B800  119.lm1 $C000  120.mn1 $C800
 *                          121.p1 $D000  222.r1 $D800;  vector 123.np3 $3000, 124.r3 $3800
 *   tempest2  rev 2, 2K:  tempest3 with 116.h1 at $A800
 *   tempest1  rev 1, 2K:  tempest2 with 117.j1 $B000, 122.r1 $D800
 *   Mathbox, every set:   126.a1 (32 bytes) = mb_map; 127.e1 .. 132.l1 (256
 *                          nibbles each) = mb_ucode, assembled exactly as
 *                          gen_roms.py does: word i = sum of
 *                          (prom[k][i] & $F) << (4 * k), k = 0 (127.e1) .. 5
 *                          (132.l1) - bits 23..0 = A B | SRC STALL FUNC |
 *                          LDAB DEST SIGN JMP MULT CARIN (MBUCOD.V05's $OUT).
 *   136002-125.d7 (the AVG state PROM) is not needed: avg.c counts the state
 *   machine's cycles itself.
 * The CPU's $E000-$FFFF mirror of $C000-$DFFF (the vectors) is the memory
 * map's business (emu_main.cpp, state.c), not the image's.
 *
 * The set is recognised by the file names in the zip (any directory prefix
 * ignored, case-insensitive): the first layout above whose every file is
 * present.  The zip directory's CRC32 of each file is compared with the table;
 * a mismatch is a warning - a hacked or patched set still runs.  A missing
 * file is an error.
 *
 * tests\romcheck.c (tests\build_tests.bat, /DEMU_ROMS_NO_BACKEND and the four
 * symbols renamed emu_*) proves the images of roms\tempest.zip byte-identical
 * to c_src's generated arrays.
 */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "platform/windows/miniz.h"
#include "emu_roms.h"

#ifndef EMU_ROMS_NO_BACKEND
#include <windows.h>
#include "platform/windows/log.h"
#define ROMS_INFO(...)  LOG_INFO(__VA_ARGS__)
#define ROMS_WARN(...)  LOG_WARN(__VA_ARGS__)
#define ROMS_ERROR(...) LOG_ERROR(__VA_ARGS__)
#else
#define ROMS_INFO(...)  (printf("roms: " __VA_ARGS__), printf("\n"))
#define ROMS_WARN(...)  (printf("roms: WARNING: " __VA_ARGS__), printf("\n"))
#define ROMS_ERROR(...) (printf("roms: ERROR: " __VA_ARGS__), printf("\n"))
#endif

/* ------------------------------------------------------------------ */
/* the images (what progrom.h / vecrom.h / mbprom.h declare)           */
/* ------------------------------------------------------------------ */

uint8_t  progrom[0x5000];      /* program ROM, CPU $9000-$DFFF: progrom[addr - $9000] */
uint8_t  vecrom[0x1000];       /* vector ROM,  CPU $3000-$3FFF: vecrom[addr - $3000]  */
uint8_t  mb_map[32];           /* 136002-126.a1: Mathbox write offset -> uPC          */
uint32_t mb_ucode[256];        /* 136002-127..132: 24-bit microcode words             */

/* ------------------------------------------------------------------ */
/* the sets                                                            */
/* ------------------------------------------------------------------ */

enum { R_PROG, R_VEC, R_MAP, R_NIB };

typedef struct {
    const char *name;          /* file name in the zip                         */
    uint8_t     region;        /* R_*                                          */
    uint16_t    addr;          /* CPU address (R_PROG / R_VEC); nibble no. k (R_NIB) */
    uint16_t    size;
    uint32_t    crc;           /* CRC32 of the good dump                       */
} rom_file;

#define MATHBOX_PROMS \
    { "136002-126.a1", R_MAP, 0, 0x020, 0x8b04f921 }, \
    { "136002-127.e1", R_NIB, 0, 0x100, 0x276eadd5 }, \
    { "136002-128.f1", R_NIB, 1, 0x100, 0x823b61ae }, \
    { "136002-129.h1", R_NIB, 2, 0x100, 0x09f5a4d5 }, \
    { "136002-130.j1", R_NIB, 3, 0x100, 0x8119b847 }, \
    { "136002-131.k1", R_NIB, 4, 0x100, 0xb31f6e24 }, \
    { "136002-132.l1", R_NIB, 5, 0x100, 0x2af82e87 }, \
    { NULL, 0, 0, 0, 0 }

static const rom_file set_tempest[] = {        /* rev 3, 4K parts */
    { "136002-133.d1",  R_PROG, 0x9000, 0x1000, 0x1d0cc503 },
    { "136002-134.f1",  R_PROG, 0xA000, 0x1000, 0xc88e3524 },
    { "136002-235.j1",  R_PROG, 0xB000, 0x1000, 0xa4b2ce3f },
    { "136002-136.lm1", R_PROG, 0xC000, 0x1000, 0x65a9a9f9 },
    { "136002-237.p1",  R_PROG, 0xD000, 0x1000, 0xde4e9e34 },
    { "136002-138.np3", R_VEC,  0x3000, 0x1000, 0x9995256d },
    MATHBOX_PROMS
};

static const rom_file set_tempest1r[] = {      /* rev 1, 4K parts */
    { "136002-133.d1",  R_PROG, 0x9000, 0x1000, 0x1d0cc503 },
    { "136002-134.f1",  R_PROG, 0xA000, 0x1000, 0xc88e3524 },
    { "136002-135.j1",  R_PROG, 0xB000, 0x1000, 0x1ca27781 },
    { "136002-136.lm1", R_PROG, 0xC000, 0x1000, 0x65a9a9f9 },
    { "136002-137.p1",  R_PROG, 0xD000, 0x1000, 0xd75fd2ef },
    { "136002-138.np3", R_VEC,  0x3000, 0x1000, 0x9995256d },
    MATHBOX_PROMS
};

#define SET_2K(h1, h1crc, j1, j1crc, r1, r1crc) \
    { "136002-113.d1",  R_PROG, 0x9000, 0x0800, 0x65d61fe7 }, \
    { "136002-114.e1",  R_PROG, 0x9800, 0x0800, 0x11077375 }, \
    { "136002-115.f1",  R_PROG, 0xA000, 0x0800, 0xf3e2827a }, \
    { h1,               R_PROG, 0xA800, 0x0800, h1crc      }, \
    { j1,               R_PROG, 0xB000, 0x0800, j1crc      }, \
    { "136002-118.k1",  R_PROG, 0xB800, 0x0800, 0xbeb352ab }, \
    { "136002-119.lm1", R_PROG, 0xC000, 0x0800, 0xa4de050f }, \
    { "136002-120.mn1", R_PROG, 0xC800, 0x0800, 0x35619648 }, \
    { "136002-121.p1",  R_PROG, 0xD000, 0x0800, 0x73d38e47 }, \
    { r1,               R_PROG, 0xD800, 0x0800, r1crc      }, \
    { "136002-123.np3", R_VEC,  0x3000, 0x0800, 0x29f7e937 }, \
    { "136002-124.r3",  R_VEC,  0x3800, 0x0800, 0xc16ec351 }, \
    MATHBOX_PROMS

static const rom_file set_tempest3[] = { SET_2K("136002-316.h1", 0xaeb0f7e9, "136002-217.j1", 0xef2eb645, "136002-222.r1", 0x707bd5c3) };
static const rom_file set_tempest2[] = { SET_2K("136002-116.h1", 0x7356896c, "136002-217.j1", 0xef2eb645, "136002-222.r1", 0x707bd5c3) };
static const rom_file set_tempest1[] = { SET_2K("136002-116.h1", 0x7356896c, "136002-117.j1", 0x55952119, "136002-122.r1", 0x796a9918) };

static const struct { const char *name; const char *what; const rom_file *files; } sets[] = {
    { "tempest",   "rev 3, 4K ROMs", set_tempest   },
    { "tempest1r", "rev 1, 4K ROMs", set_tempest1r },
    { "tempest3",  "rev 3, 2K ROMs", set_tempest3  },
    { "tempest2",  "rev 2, 2K ROMs", set_tempest2  },
    { "tempest1",  "rev 1, 2K ROMs", set_tempest1  },
};
#define N_SETS ((int)(sizeof sets / sizeof sets[0]))

static const char *loaded_set = "";
static char        loaded_zip[1024];
static unsigned    crc_mismatches;

const char *emu_roms_set_name(void)       { return loaded_set; }
const char *emu_roms_zip_path(void)       { return loaded_zip; }
unsigned    emu_roms_crc_mismatches(void) { return crc_mismatches; }

/* ------------------------------------------------------------------ */
/* loading                                                             */
/* ------------------------------------------------------------------ */

static int locate(mz_zip_archive *zip, const char *name)
{
    return mz_zip_reader_locate_file(zip, name, NULL, MZ_ZIP_FLAG_IGNORE_PATH);   /* case-insensitive by default */
}

/* How many of a set's files the zip holds; *first_missing = the first absent one. */
static int count_present(mz_zip_archive *zip, const rom_file *f, int *total, const char **first_missing)
{
    int have = 0;
    *total = 0;
    *first_missing = NULL;
    for (; f->name; f++) {
        (*total)++;
        if (locate(zip, f->name) >= 0) have++;
        else if (!*first_missing) *first_missing = f->name;
    }
    return have;
}

int emu_roms_load(const char *zip_path, char *err, size_t err_len)
{
    mz_zip_archive zip;
    const rom_file *f;
    uint8_t nib[6][256];
    uint8_t buf[0x1000];
    int set = -1, best = -1, best_have = -1, i;
    const char *best_missing = NULL;

    if (err_len) err[0] = '\0';
    memset(&zip, 0, sizeof zip);
    if (!mz_zip_reader_init_file(&zip, zip_path, 0)) {
        snprintf(err, err_len, "cannot open ROM zip %s", zip_path);
        return 1;
    }

    for (i = 0; i < N_SETS; i++) {
        int total;
        const char *missing;
        int have = count_present(&zip, sets[i].files, &total, &missing);
        if (have == total) { set = i; break; }
        if (have > best_have) { best_have = have; best = i; best_missing = missing; }
    }
    if (set < 0) {
        snprintf(err, err_len, "%s is not a Tempest set: closest layout '%s' lacks %s (and maybe more)",
                 zip_path, sets[best].name, best_missing ? best_missing : "?");
        mz_zip_reader_end(&zip);
        return 1;
    }

    crc_mismatches = 0;
    memset(nib, 0, sizeof nib);
    for (f = sets[set].files; f->name; f++) {
        mz_zip_archive_file_stat st;
        int idx = locate(&zip, f->name);
        if (idx < 0 || !mz_zip_reader_file_stat(&zip, (mz_uint)idx, &st)) {
            snprintf(err, err_len, "%s: cannot stat %s", zip_path, f->name);
            mz_zip_reader_end(&zip);
            return 1;
        }
        if (st.m_uncomp_size != f->size) {
            snprintf(err, err_len, "%s: %s is %u bytes, expected %u", zip_path, f->name,
                     (unsigned)st.m_uncomp_size, (unsigned)f->size);
            mz_zip_reader_end(&zip);
            return 1;
        }
        if (!mz_zip_reader_extract_to_mem(&zip, (mz_uint)idx, buf, f->size, 0)) {
            snprintf(err, err_len, "%s: cannot extract %s (corrupt zip?)", zip_path, f->name);
            mz_zip_reader_end(&zip);
            return 1;
        }
        if ((uint32_t)st.m_crc32 != f->crc) {
            crc_mismatches++;
            ROMS_WARN("%s: CRC32 %08x, expected %08x (set '%s') - not the known good dump, loading it anyway",
                      f->name, (unsigned)st.m_crc32, (unsigned)f->crc, sets[set].name);
        }
        switch (f->region) {
        case R_PROG: memcpy(progrom + (f->addr - 0x9000), buf, f->size); break;
        case R_VEC:  memcpy(vecrom  + (f->addr - 0x3000), buf, f->size); break;
        case R_MAP:  memcpy(mb_map, buf, f->size); break;
        default:     memcpy(nib[f->addr], buf, f->size); break;
        }
    }
    mz_zip_reader_end(&zip);

    /* the microcode words, low nibble first (gen_roms.py) */
    for (i = 0; i < 256; i++) {
        uint32_t w = 0;
        int k;
        for (k = 0; k < 6; k++) w |= (uint32_t)(nib[k][i] & 0x0Fu) << (4 * k);
        mb_ucode[i] = w;
    }
    /* gen_roms.py's two order checks against MBUCOD.V05: RSTO's "LDL 0 LDAB 16
     * JMP" (write AL, uPC $20) = $1073B4; RDISP's "OR Z,n NOP STALL" (uPC
     * $10+n) = $0n3B10 (RN's slot, n = 6, is the CLRNH jump). */
    {
        int bad = mb_ucode[0x20] != 0x1073B4u;
        for (i = 0; i < 16; i++)
            if (i != 6 && mb_ucode[0x10 + i] != (0x003B10u | ((uint32_t)i << 16))) bad = 1;
        if (bad) ROMS_WARN("Mathbox microcode does not pass gen_roms.py's nibble-order checks (uPC $20 = %06X)",
                           (unsigned)mb_ucode[0x20]);
    }

    loaded_set = sets[set].name;
    snprintf(loaded_zip, sizeof loaded_zip, "%s", zip_path);
    ROMS_INFO("ROM set '%s' (%s) from %s; vectors NMI $%02X%02X RESET $%02X%02X IRQ $%02X%02X; %u CRC mismatch(es)",
              sets[set].name, sets[set].what, zip_path,
              progrom[0x4FFB], progrom[0x4FFA], progrom[0x4FFD], progrom[0x4FFC], progrom[0x4FFF], progrom[0x4FFE],
              crc_mismatches);
    return 0;
}

#ifndef EMU_ROMS_NO_BACKEND
static int file_exists(const char *path)
{
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

int emu_roms_load_default(const char *override_path, int quiet)
{
    char exe_dir[MAX_PATH], path[MAX_PATH + 32], err[1400];
    char *slash;
    DWORD n = GetModuleFileNameA(NULL, exe_dir, (DWORD)sizeof exe_dir);
    if (n == 0 || n >= sizeof exe_dir) exe_dir[0] = '\0';
    slash = strrchr(exe_dir, '\\');
    if (slash) slash[1] = '\0'; else exe_dir[0] = '\0';

    if (override_path && override_path[0]) {
        snprintf(path, sizeof path, "%s", override_path);
    } else {
        snprintf(path, sizeof path, "%sroms\\tempest.zip", exe_dir);
        if (!file_exists(path)) snprintf(path, sizeof path, "%s..\\roms\\tempest.zip", exe_dir);
    }
    if (!file_exists(path))
        snprintf(err, sizeof err, "ROM zip not found: %s%s", path,
                 override_path && override_path[0] ? "" : " (looked in roms\\ beside the exe, then ..\\roms\\)");
    else if (emu_roms_load(path, err, sizeof err) == 0)
        return 0;

    ROMS_ERROR("%s", err);
    if (!quiet) MessageBoxA(NULL, err, "Tempest emulator - ROMs", MB_OK | MB_ICONERROR);
    return 1;
}
#endif /* EMU_ROMS_NO_BACKEND */
