/* avg.c - Tempest C port: the Analog Vector Generator display-list walker.
 *
 * Contract and semantics: avg.h.  Base: harvest/avg.c (Space Duel); the
 * Tempest differences are the colour/intensity STAT split (tempest_strobe2),
 * the 4-bit colour index into colour RAM (tempest_strobe3), the 4K + 4K map,
 * the hardware 4-slot stack and the JMPL-loop frame end.
 */
#include <stddef.h>
#include <stdint.h>
#include "avg.h"
#include "state.h"
#include "vecrom.h"

#define OP_VCTR 0
#define OP_HALT 1
#define OP_SVEC 2
#define OP_STAT 3
#define OP_CNTR 4
#define OP_JSRL 5
#define OP_RTSL 6
#define OP_JMPL 7

/* 13-bit two's complement delta (VGMC.MAC: .WORD DY&^H1FFF). */
static int s13(unsigned v)
{
    v &= 0x1FFFu;
    return (v & 0x1000u) ? (int)v - 0x2000 : (int)v;
}

/* 5-bit two's complement half delta of SVEC. */
static int s5(unsigned v)
{
    v &= 0x1Fu;
    return (v & 0x10u) ? (int)v - 0x20 : (int)v;
}

/* Word at CPU address a in the AVG map; 0 if outside $2000-$3FFF. */
static int fetch(const avg_mem *m, uint32_t a, unsigned *w)
{
    const uint8_t *p;

    if (a < AVG_VRAM_BASE || a + 1u >= AVG_SPACE_END)
        return 0;
    if (a < AVG_VROM_BASE) {
        if (a + 1u >= AVG_VROM_BASE)            /* word straddles $2FFF/$3000 */
            *w = (unsigned)m->vram[a - AVG_VRAM_BASE]
               | ((unsigned)m->vrom[0] << 8);
        else {
            p = m->vram + (a - AVG_VRAM_BASE);
            *w = (unsigned)p[0] | ((unsigned)p[1] << 8);
        }
        return 1;
    }
    p = m->vrom + (a - AVG_VROM_BASE);
    *w = (unsigned)p[0] | ((unsigned)p[1] << 8);
    return 1;
}

static uint16_t target(unsigned w)
{
    return (uint16_t)(AVG_VRAM_BASE + ((w & 0x1FFFu) << 1));
}

static int in_rom(uint32_t a)
{
    return a >= AVG_VROM_BASE && a < AVG_SPACE_END;
}

int32_t avg_scal_q15(int bin, int lin)
{
    return (int32_t)((255 - (lin & 0xFF)) << (7 - (bin & 7)));
}

uint32_t avg_colram_rgb(uint8_t v)
{
    uint32_t r = ((v & 2u) ? 0u : 0xF3u) + ((v & 1u) ? 0u : 0x0Cu);
    uint32_t gg = (v & 8u) ? 0u : 0xF3u;
    uint32_t b = (v & 4u) ? 0u : 0xF3u;
    return r | (gg << 8) | (b << 16) | 0xFF000000u;
}

void avg_state_init(avg_state *st)
{
    int i;

    st->x = 0;
    st->y = 0;
    st->scale_q15 = 0;
    st->color = 0;
    st->intensity = 0;
    for (i = 0; i < AVG_STACK_SLOTS; i++)
        st->stack[i] = 0;
    st->sp = 0;
    st->depth = 0;
}

static int32_t clamp32(int64_t v)
{
    if (v > INT32_MAX) return INT32_MAX;
    if (v < INT32_MIN) return INT32_MIN;
    return (int32_t)v;
}

static void bbox_add(avg_result *r, int32_t x, int32_t y)
{
    if (!r->have_bbox) {
        r->have_bbox = 1;
        r->minx = r->maxx = x;
        r->miny = r->maxy = y;
        return;
    }
    if (x < r->minx) r->minx = x;
    if (x > r->maxx) r->maxx = x;
    if (y < r->miny) r->miny = y;
    if (y > r->maxy) r->maxy = y;
}

/* JSRL push on the 4-slot / 4-bit-counter stack (avg_strobe0 + strobe1). */
static void push(avg_state *st, avg_result *r, uint16_t ret)
{
    st->stack[st->sp & 3u] = ret;
    st->sp = (uint8_t)((st->sp + 1u) & 0xFu);
    st->depth++;
    if (st->depth > AVG_STACK_SLOTS)
        r->flags |= AVG_FLAG_STACK_OVER;
    if (st->depth > r->max_depth)
        r->max_depth = st->depth;
}

/* RTSL pop; returns 0 at depth 0 (the walk ends there). */
static int pop(avg_state *st, uint16_t *pc)
{
    if (st->depth <= 0)
        return 0;
    st->depth--;
    st->sp = (uint8_t)((st->sp - 1u) & 0xFu);
    *pc = st->stack[st->sp & 3u];
    return 1;
}

avg_result avg_walk(const avg_mem *m, uint16_t start, avg_state *st_in,
                    const avg_cfg *cfg)
{
    avg_state   local;
    avg_state  *st = st_in;
    avg_result  r = {0};
    uint32_t    budget = (cfg && cfg->max_ops) ? cfg->max_ops : AVG_FRAME_BUDGET;
    unsigned    opts = cfg ? cfg->options : 0u;
    int         preview = (opts & AVG_OPT_PREVIEW) != 0;
    uint16_t    pc = start;

    if (st == NULL) {
        avg_state_init(&local);
        st = &local;
    }
    r.stop = AVG_STOP_BUDGET;
    r.stop_pc = pc;

    for (;;) {
        unsigned w, w2 = 0;
        int      op, dx = 0, dy = 0, z = 0, is_svec = 0;
        uint16_t here = pc;

        if (r.ops >= budget) {
            r.stop = AVG_STOP_BUDGET;
            r.stop_pc = here;
            break;
        }
        if (preview && !(pc >= AVG_VROM_BASE && pc <= AVG_SPACE_END - 2u)) {
            r.stop = AVG_STOP_PREVIEW_EXIT;
            r.stop_pc = here;
            break;
        }
        if (!fetch(m, pc, &w)) {
            r.stop = AVG_STOP_BADADDR;
            r.stop_pc = here;
            break;
        }
        r.ops++;
        op = (int)(w >> 13);

        switch (op) {
        case OP_VCTR:
            if (!fetch(m, (uint32_t)pc + 2u, &w2)) {
                r.stop = AVG_STOP_BADADDR;
                r.stop_pc = here;
                return r;
            }
            dy = s13(w);
            dx = s13(w2);
            z = (int)((w2 >> 13) & 7u);
            pc = (uint16_t)(pc + 4u);
            goto draw;

        case OP_SVEC:
            dx = s5(w) * 2;
            dy = s5(w >> 8) * 2;
            z = (int)((w >> 5) & 7u);
            is_svec = 1;
            pc = (uint16_t)(pc + 2u);
        draw: {
                avg_seg s;
                int64_t nx = st->x + (int64_t)dx * st->scale_q15;
                int64_t ny = st->y + (int64_t)dy * st->scale_q15;
                int     lum = (z == 1) ? st->intensity : z * 2;

                s.x0 = clamp32(st->x);
                s.y0 = clamp32(st->y);
                s.x1 = clamp32(nx);
                s.y1 = clamp32(ny);
                s.intensity = (uint8_t)lum;
                s.color = (int8_t)st->color;
                if (st->color >= 0 && m->colram != NULL) {
                    s.colram = m->colram[st->color & 0xF];
                    s.rgb = avg_colram_rgb(s.colram);
                } else {
                    s.colram = 0xFF;
                    s.rgb = 0xFFFFFFFFu;
                }
                s.svec = (uint8_t)is_svec;
                s.pc = here;
                st->x = nx;
                st->y = ny;

                r.nseg++;
                if (z != 0) r.nz++;
                if (lum > 0) {
                    r.nlit++;
                    bbox_add(&r, s.x0, s.y0);
                    bbox_add(&r, s.x1, s.y1);
                }
                if (cfg != NULL) {
                    if (cfg->segs != NULL && r.nseg_stored < cfg->seg_cap)
                        cfg->segs[r.nseg_stored++] = s;
                    if (cfg->seg != NULL)
                        cfg->seg(cfg->ctx, &s);
                }
            }
            continue;

        case OP_STAT:
            if (w & 0x1000u)                       /* SCAL */
                st->scale_q15 = avg_scal_q15((int)((w >> 8) & 7u), (int)(w & 0xFFu));
            else if (w & 0x0800u)                  /* colour STAT */
                st->color = (int)(w & 0xFu);
            else                                   /* intensity STAT */
                st->intensity = (int)((w >> 4) & 0xFu);
            pc = (uint16_t)(pc + 2u);
            continue;

        case OP_CNTR:
            st->x = 0;
            st->y = 0;
            pc = (uint16_t)(pc + 2u);
            continue;

        case OP_JSRL: {
            uint16_t t = target(w);
            if (preview && !in_rom(t)) {
                r.vram_calls++;
                r.flags |= AVG_FLAG_VRAM_CALL;
                pc = (uint16_t)(pc + 2u);
                continue;
            }
            push(st, &r, (uint16_t)(pc + 2u));
            pc = t;
            continue;
        }

        case OP_RTSL:
            if (!pop(st, &pc)) {
                r.stop = AVG_STOP_RTSL;
                r.stop_pc = here;
                return r;
            }
            continue;

        case OP_JMPL: {
            uint16_t t = target(w);
            if (preview && !in_rom(t)) {
                r.vram_calls++;
                r.flags |= AVG_FLAG_VRAM_CALL;
                if (!pop(st, &pc)) {
                    r.stop = AVG_STOP_RTSL;
                    r.stop_pc = here;
                    return r;
                }
                continue;
            }
            if (!preview && t == start && st->depth == 0) {
                r.stop = AVG_STOP_LOOP;
                r.stop_pc = here;
                return r;
            }
            pc = t;
            continue;
        }

        case OP_HALT:
        default:
            r.stop = AVG_STOP_HALT;
            r.stop_pc = here;
            return r;
        }
    }
    return r;
}

avg_mem avg_mem_g(void)
{
    avg_mem m;
    m.vram = g.vram;
    m.vrom = vecrom;
    m.colram = g.colram;
    return m;
}

avg_result avg_run_frame(const avg_cfg *cfg)
{
    avg_mem m = avg_mem_g();
    return avg_walk(&m, AVG_VRAM_BASE, NULL, cfg);
}

const char *avg_stop_name(avg_stop s)
{
    switch (s) {
    case AVG_STOP_HALT:         return "HALT";
    case AVG_STOP_LOOP:         return "LOOP";
    case AVG_STOP_RTSL:         return "RTSL";
    case AVG_STOP_BUDGET:       return "BUDGET";
    case AVG_STOP_BADADDR:      return "BADADDR";
    case AVG_STOP_PREVIEW_EXIT: return "EXIT";
    }
    return "?";
}
