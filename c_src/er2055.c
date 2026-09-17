/* er2055.c - see er2055.h for what this models and where from.
 *
 * Translated arithmetic-for-arithmetic from MAME 0.286's
 * er2055_device::set_control/update_state/set_clk
 * (src/devices/machine/er2055.cpp); only `this->m_member` becomes
 * `e->member` and the two bool-ish MAME args (int cs1/cs2/c1/c2, int
 * state) become C `bool`.
 *
 * Used under er2055.cpp's BSD-3-Clause terms, copyright the MAME team
 * and the copyright holder it names (Aaron Giles): redistribution and
 * use in source and binary forms, with or without modification, are
 * permitted provided that redistributions of source code retain this
 * notice, that redistributions in binary form reproduce it in the
 * documentation, and that the names of the copyright holders are not
 * used to endorse derived products without permission; the software is
 * provided "as is" without warranty.  The rest of this file is under
 * the project's licence (see LICENSE).
 */
#include "er2055.h"

/* MAME's own bit names for the internal composite control_state byte -
 * kept as macros, not the EACTL bit names (EACK/EAC1/EAC2/EACE from
 * earom.c), because these are the chip's active-high pins *after*
 * asteroid.cpp's driver has decoded/inverted the EACTL byte for it; see
 * ad_er2055_control() below for that decode. */
#define CK  0x01
#define C1  0x02
#define C2  0x04
#define CS1 0x08
#define CS2 0x10

void ad_er2055_init(ad_er2055 *e)
{
    for (int i = 0; i < 64; i++)
        e->rom[i] = 0;                          /* ROMREGION_ERASE00 */
    e->address = 0;
    e->data = 0;
    e->control = 0;
    e->dirty = false;
}

void ad_er2055_set_addr_data(ad_er2055 *e, uint8_t addr, uint8_t data)
{
    e->address = addr & 0x3F;                   /* set_address() */
    e->data = data;                              /* set_data() */
}

/* update_state() - runs following a transition on clock, control or chip
 * select while both chip selects are set; called from both set_control()
 * and set_clk() below, exactly as the MAME device does. */
static void update_state(ad_er2055 *e)
{
    switch (e->control & (C1 | C2)) {
    case 0: {                                    /* write: AND against
                                                    * previous data - a
                                                    * write without an
                                                    * erase can only
                                                    * clear bits */
        uint8_t before = e->rom[e->address];
        e->rom[e->address] = (uint8_t)(before & e->data);
        if (e->rom[e->address] != before)
            e->dirty = true;
        break;
    }
    case C2:                                     /* erase */
        if (e->rom[e->address] != 0xFF)
            e->dirty = true;
        e->rom[e->address] = 0xFF;
        break;
    default:                                     /* C1 alone (read - the
                                                    * latch happens in
                                                    * set_clk, not here),
                                                    * or C1|C2: no ROM
                                                    * change either way */
        break;
    }
}

/* set_control() - MAME's er2055_device::set_control(cs1, cs2, c1, c2).
 * Only a genuine change while BOTH chip selects are set reaches
 * update_state(); this driver ties cs2 high always, so ad_er2055_control()
 * below always passes cs2 = true, and only cs1 (EACE) actually gates
 * selection. */
static void set_control(ad_er2055 *e, bool cs1, bool cs2, bool c1, bool c2)
{
    uint8_t old = e->control;
    uint8_t next = (uint8_t)(old & CK);          /* CK is set_clk()'s alone */
    if (c1)  next = (uint8_t)(next | C1);
    if (c2)  next = (uint8_t)(next | C2);
    if (cs1) next = (uint8_t)(next | CS1);
    if (cs2) next = (uint8_t)(next | CS2);
    e->control = next;

    if ((e->control & (CS1 | CS2)) != (CS1 | CS2) || e->control == old)
        return;                                  /* not selected, or no change */
    update_state(e);
}

/* set_clk() - MAME's er2055_device::set_clk(state). The falling edge
 * while selected latches a read (C1 set, C2 "don't care") into e->data,
 * then runs update_state() again - a read reaches update_state() too,
 * it just has no case that touches rom[] (see the `default` above). */
static void set_clk(ad_er2055 *e, bool state)
{
    uint8_t old = e->control;
    if (state)
        e->control = (uint8_t)(e->control | CK);
    else
        e->control = (uint8_t)(e->control & ~CK);

    if ((e->control & (CS1 | CS2)) == (CS1 | CS2) && e->control != old && !state) {
        if ((e->control & C1) == C1)
            e->data = e->rom[e->address];
        update_state(e);
    }
}

/* EACTL byte -> the chip's active-high pins, exactly as asteroid.cpp's
 * earom_control_w() decodes it, then set_control() followed by
 * set_clk() in that same order:
 *
 *     m_earom->set_control(BIT(data, 3), 1, !BIT(data, 2), BIT(data, 1));
 *     m_earom->set_clk(BIT(data, 0));
 *
 * bit 3 = EACE (chip select, this driver's cs1); cs2 is hardwired to 1.
 * bit 2 = EAC1, inverted (earom.c: "EAC1 (inverted to the chip)").
 * bit 1 = EAC2, direct.
 * bit 0 = EACK, the clock. */
void ad_er2055_control(ad_er2055 *e, uint8_t v)
{
    bool cs1 = (v & 0x08) != 0;
    bool c1  = (v & 0x04) == 0;
    bool c2  = (v & 0x02) != 0;
    bool ck  = (v & 0x01) != 0;

    set_control(e, cs1, true, c1, c2);
    set_clk(e, ck);
}

uint8_t ad_er2055_data(const ad_er2055 *e)
{
    return e->data;
}
