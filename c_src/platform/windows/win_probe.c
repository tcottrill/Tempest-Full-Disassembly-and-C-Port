/* win_probe.c - Tempest C port: machine-state peeks for the Windows backend
 * (see win_probe.h).  Read-only. */
#include "../../state.h"
#include "win_probe.h"

uint32_t win_probe_irqs(void)    { return g.irq_count; }
uint32_t win_probe_passes(void)  { return g.pass_count; }
uint8_t  win_probe_qstate(void)  { return QSTATE; }
uint8_t  win_probe_qdstate(void) { return QDSTATE; }
int      win_probe_in_game(void) { return (QSTATUS & K_MATRACT) != 0; }
int      win_probe_getini(void)  { return QSTATE == K_CGETINI; }
uint8_t  win_probe_wave(void)    { return CURWAV; }
