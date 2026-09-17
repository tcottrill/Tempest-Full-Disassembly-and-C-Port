/* er2055.h - the GI ER2055 EAROM (64 words x 8 bits) behind EACTL/EADAL/
 * EAIN, translated from MAME 0.286's device model
 * (src/devices/machine/er2055.cpp/.h) as this game's driver wires it
 * (src/mame/atari/asteroid.cpp's earom_read/earom_write/earom_control_w,
 * asteroid_m.cpp's machine-reset callback).
 *
 * That driver ties CS2 high permanently and drives CS1 from EACTL bit 3
 * (EACE); C1 is EACTL bit 2, INVERTED (the ROM calls the raw bit EAC1
 * and drives it active-low - see earom.c); C2 is EACTL bit 1 (EAC2,
 * active-high); CLK is EACTL bit 0 (EACK). A control or clock change
 * only does anything while BOTH chip selects read selected, and only on
 * a genuine transition - a repeated write of the same latch value is a
 * no-op, exactly as the chip model requires (`set_control`/`set_clk`
 * below keep MAME's own names in comments, and its CS1/CS2/C1/C2/CK bit
 * names, for anyone comparing against the .cpp).
 *
 * Not a ROM routine - the chip is hardware EAROM.MAC talks to - so, like
 * c012294.c/.h, this carries an `ad_` prefix and no platform includes: a
 * host owns one `ad_er2055` instance and drives it through these four
 * calls, matching astdelux.h's `ad_hw_earom_*` seam one for one.
 */
#ifndef AD_ER2055_H
#define AD_ER2055_H

#include <stdint.h>
#include <stdbool.h>

typedef struct ad_er2055 {
    uint8_t rom[64];      /* the 64x8 array (MAME's m_rom_data) */
    uint8_t address, data; /* set_address()'s low 6 bits; set_data()/data() */
    uint8_t control;       /* internal control_state: CK|C1|C2|CS1|CS2 */
    bool    dirty;         /* an erase or write changed rom[]; the host
                             * clears this after saving the image to disk */
} ad_er2055;

/* Zero-fills the image (MAME's ROMREGION_ERASE00 for this game, "to
 * suppress invalid high score display" - a factory-fresh chip is really
 * all $FF, but that is not what this board ships with) and clears the
 * control latch. */
void    ad_er2055_init(ad_er2055 *e);

/* EADAL,x: set_address(addr & 0x3F) then set_data(data), together -
 * EADAL,x puts both the chip address and the data bus byte down in one
 * store (see earom.c's eadal()), and MAME's earom_write() likewise sets
 * both from the one call. */
void    ad_er2055_set_addr_data(ad_er2055 *e, uint8_t addr, uint8_t data);

/* EACTL: runs set_control() then set_clk(), the order asteroid.cpp's
 * earom_control_w() uses. */
void    ad_er2055_control(ad_er2055 *e, uint8_t v);

/* EAIN: data(). */
uint8_t ad_er2055_data(const ad_er2055 *e);

#endif /* AD_ER2055_H */
