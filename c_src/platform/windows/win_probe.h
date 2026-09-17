/* win_probe.h - Tempest C port: read-only peeks at the machine state for the
 * Windows backend's pacing log and its --autoplay script (M8 part 2).
 *
 * plat_win.c includes <windows.h>, and the generated state_defs.h names
 * (QSTATE, ...) must not meet the Windows headers, so the peeks live in their
 * own translation unit (win_probe.c).  Nothing here writes machine state. */
#ifndef WIN_PROBE_H
#define WIN_PROBE_H

#include <stdint.h>

uint32_t win_probe_irqs(void);      /* g.irq_count (reset by tempest_app_init) */
uint32_t win_probe_passes(void);    /* g.pass_count */
uint8_t  win_probe_qstate(void);    /* QSTATE */
uint8_t  win_probe_qdstate(void);   /* QDSTATE */
int      win_probe_in_game(void);   /* QSTATUS & K_MATRACT: a game is running */
int      win_probe_getini(void);    /* QSTATE == K_CGETINI: high-score initials */
uint8_t  win_probe_wave(void);      /* CURWAV (0 = wave 1) */

#endif /* WIN_PROBE_H */
