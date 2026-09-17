"""Hand-written routine descriptions for tempest_program_rom.asm.

Used by emit.py only for routines that have neither an Atari source description
(.SBTTL / label comment / comment block above the label / comment on the calls)
nor a Commented Source block header.  Written for this disassembly from reading
the code (and the neighbouring source comments); they are tagged [note] in the
listing.  Keep them short and factual.  A second line may follow after "\n".
"""

DOCS = {
    # ALWELG - wave start / skill contour parameter records (see CONTOUR's
    # 'PARAMETER TABLES DATA STRUCTURE' comment)
    "INIRAT": "Set up the player about to start (SWAPEN for the 2nd player), reset cursor and level window;\n"
              "outside attract mode enter the skill-level request state (CREQRAT / CDREQRA) with QTMPAUS=$10.",
    "DOTZAN": "Parameter type TZANDF: wave index = ((TEMP2-1) AND $0F)+1, then continue as ITMIZE\n"
              "(one byte per wave, repeating every 16 waves).",
    "ITMIZE": "Parameter type TZ: return in A the byte for wave TEMP2 from the record's per-wave list.",
    "ONEBYT": "Record skip (NPARAD): Y += 2 past a one-byte parameter field; TWOBYT enters one INY earlier.",
    "NITMIZ": "Record skip (NPARAD) for TZ/TZANDF records: advance Y past the (end-start+1) itemized bytes.",
    "DOTA":   "Parameter type TA: A = byte 3 + n * byte 4, n = RANGER (the wave's offset in the range).",
    "RANGER": "A = TEMP2 - start wave of the record (source: # of levels between start and end); Y preserved.",
    "NEWFLI": "NYMCHA type vector for a flipper: TEMP3 = TNEWI2+ZABFLI, A = WFLICAM CAM start, Y = ZABFLI;\n"
              "continues at NEWGN3.",
    "NEWGEN": "NYMCHA common exit for enemy type Y: TEMP3 = TNEWI2,Y, CAM start TNEWCAM,Y;\n"
              "then TEMP2 = type, TEMP4 = CAM, A = TEMP0 (success flag).",
    "JNOOP":  "CAM opcode VNOOP: does nothing (RTS).",
    "JSETPC": "CAM opcode VSETPC: CAMPC = operand byte (jump inside the CAM script area).",
    "JELTST": "CAM opcode VELTST: CAMSTA = 0 if INVAY(X) is above the enemy-line height LINEY of the\n"
              "enemy's lane (source: 'enemy on an enemy line?'), else CAMSTA = 1.",
    "JFUSKI": "CAM opcode VFUSKI: if the fuseball is at the cursor's height (CURSY) and line (CURSL1),\n"
              "kill the player (INFPSQ).  Source: CHECK FOR FUSE KILL CURSOR.",
    "FIXTOP": "Return a random value in A: 0..7 from RANDO2, negated when bit 0 of A on entry was 1.",
    "EXIKIL": "Kill enemy Y: clear its carrier bit (INVCAR in INVAC2) and start its explosion (JMP INCISQ).",
    # ALSCO2 - display states and info
    "DGOVER": "Display state: 'GAME OVER' (MGAMOV, special Y position $30) then 'PLAYER n' (GENPLA),\n"
              "ending in JMP HACKER (the rev-3 change from JMP INFO).",
    "DPRSTA": "Display state: all info (INFO) plus 'PRESS START' in the middle of the screen.",
    "DBOLOU": "Show the bonus-life interval (BOLOUT), then fall into the copyright and credits display.",
    "PL1RNK": "Display the rank of player X (RANKS,X; nothing if 0): red rank digits, a dot, 'PLAYER' and\n"
              "the player number at height HITRNK,X.",
    # ALDIS2
    "EXCESS": "End of the nymph display loop: VGADD Y-1 bytes if Y is non-zero, then the ZQATLI check\n"
              "(QT1 non-zero and wave >= 10 -> FRTIMR = $7A) and VGSCA1 with scale 1.",
    "WHICHB": "Return the info sub-buffer start address in A (high) / X (low): BFASTA when\n"
              "BUFACT+BCINFO is non-zero, else BFBSTA.",
    # ALEXEC
    "DLADR":  "No high score to enter: back to attract (clear MATRACT/MGTMOD, NUMPLA = 0), pause $A0 at\n"
              "double time showing the high-score table (CDHITB), then the logo state (QNXTSTA = CLOGO).",
    # ALSOUN
    "ESLSON": "Start the enemy-shot sound (sound SIDES) via SNDON.",
    # ALTES2
    "NOOPR_DB21": "Lone RTS: completes the push-address jump through SFTJSR set up just above; also used as\n"
                  "a do-nothing entry.",
    "SINTEN": "Self-test picture: JSRL the intensity test pattern INTEST, then turn off all sounds.",
    "SHATCH": "Self-test picture: JSRL the cross-hatch pattern HATCH, then turn off all sounds.",
}
