;Tempest (Atari, 1981) - annotated disassembly of the program ROM $9000-$DFFF.
;Revision 3 (MAME 'tempest' / 'tempest3': 136002-133..237 / -113..-222 + -316).
;Reconstructed by re-assembling Atari's own source (Dave Theurer et al., the
;ALEXEC.LDA module set ALWELG ALSCO2 ALDIS2 ALEXEC ALSOUN ALVROM ALCOIN ALLANG
;ALHAR2 ALTES2 ALEARO ALVGUT) byte-exact with disasm/macasm.py, so names and
;upper-case comments are Atari's.  Remarks marked [CS] are from 'Tempest Commented
;Source' (Josh McCormick et al., 1999-2004).  Routine header descriptions come from
;the source (.SBTTL / label comment / comment block), else [CS], else were written
;for this disassembly (tagged [note]).
;Every line below was re-encoded and byte-compared with the ROM (verify.py);
;gen_from_roms.py --check round-trips it with ca65/ld65.
;Syntax: Ophis style (.alias, .byte, .word); [ ] groups expressions, which Atari's
;MAC65 evaluated strictly left to right; ~ is one's complement, | is OR.
;$E000-$FFFF is a hardware mirror of the top of this ROM (see the end of the file).

.org $9000

.include "tempest_defines.asm"


;==============================================================================
; MODULE ALWELG   ALWELG.MAC
;   Game mainline: new wave/life init, skill-level select, wave (skill
;   contour) parameter tables, player cursor, nymphs and invaders (flipper,
;   tanker, spiker, fuseball, pulsar), CAM enemy-motion scripts, charges,
;   collisions.
;==============================================================================

PROG:
L9000:  .byte $02, $BB, $5A, $30    ;MORSE CODE ATARI  [CS] Intuitive guess: CRC data? The self-test will give the
L9004:  .byte $50, $EE, $3D, $A8    ;[CS] data that should be here if you modify just one byte. Not

CHKSM2:
L9008:  .byte QCHKS2                ;[CS] true for other ROM locations.

;------------------------------------------------------------------------------
; INEWAV - INITIALIZE - MAINLINE
;   NEW WAVE
;   [CS] Unknown subroutine.
;------------------------------------------------------------------------------
INEWAV:
L9009:  JSR  CONTOUR
L900C:  JSR  INIENE                 ;INITIALIZE NYMPHS, ENEMY LINES  [CS] Set number of enemies to appear and size of the spikes to begin with.
L900F:  JSR  INIOBJ                 ;INITIALIZE OBJECTS
L9012:  JSR  INISUZ                 ;NEW SUPERZAPPER  [CS] Clear out the superzapper status.
L9015:  LDA  #$FA
L9017:  STA  EYH
L9019:  LDA  #$00                   ;CURSOR STARTS AT TOP, NOT DESCENDING
L901B:  STA  CURMOD
L901E:  STA  EYL
L9020:  LDA  #CDPLAY                ;[CS] Set status to gameplay mode
L9022:  STA  QDSTATE                ;[CS] (or attract mode).
L9024:  RTS

;------------------------------------------------------------------------------
; INEWLI - NEW LIFE
;   [CS] SUBROUTINE: Game initialization.
;------------------------------------------------------------------------------
INEWLI:
L9025:  JSR  INICUR                 ;INITIALIZE CURSOR  [CS] Set the player's position.
L9028:  JSR  CONTOUR                ;SET SKILL LEVEL ACC TO WAVE

;------------------------------------------------------------------------------
; INIOBJ - INITIALIZE OBJECTS  (comment at the call)
;------------------------------------------------------------------------------
INIOBJ:
L902B:  JSR  INICHA                 ;DEACTIVATE CHARGES  [CS] Remove all bullets from play.
L902E:  JSR  INIINV                 ;DEACTIVATE INVADERS  [CS] Remove all enemies from tube.
L9031:  JSR  ININYM                 ;INITIALIZE NYMPHS
L9034:  JSR  INIEXP                 ;DEACTIVATE EXPLOSIONS  [CS] Clear out any enemy deaths.
L9037:  JSR  CLRPOT                 ;CLEAR POT  [CS] Zero out the spinner.
L903A:  JSR  INIDSP                 ;INITIALIZE DISPLAY
L903D:  LDA  #$FF
L903F:  STA  BOFLASH                ;BONUS FLASHER CLEARED
L9042:  STA  PULSON
L9045:  LDA  #$00                   ;CLEAR ENEMY SPIKE COUNTER
L9047:  STA  ELICNT
L904A:  RTS

;------------------------------------------------------------------------------
; NEWAV2 - INITIALIZE-NEW WAVE PART 2
;------------------------------------------------------------------------------
NEWAV2:
L904B:  LDA  #ILINLIY               ;[CS] Put the player at the top
L904D:  STA  CURSY                  ;[CS] of the tunnel.
L9050:  LDA  #$00
L9052:  STA  TEMP0
L9054:  STA  TEMP2
L9056:  LDA  ZADEST
L9059:  STA  TEMP1
L905B:  BPL  L905F                  ;IFMI
L905D:  DEC  TEMP2
L905F:  LDX  #$01
L9061:  LDA  TEMP1
L9063:  ASL
L9064:  ROR  TEMP1
L9066:  ROR  TEMP0
L9068:  DEX
L9069:  BPL  L9061                  ;MIEND
L906B:  LDA  TEMP0                  ;UPDATE Z CENTER
L906D:  CLC
L906E:  ADC  ZADEST+1
L9071:  STA  ZADEST+1
L9074:  LDA  TEMP1
L9076:  ADC  ZADJL
L9078:  STA  ZADJL
L907A:  LDA  TEMP2
L907C:  ADC  ZADJL+1
L907E:  STA  ZADJL+1
L9080:  LDA  EYL                    ;MOVE EYE CLOSER TO WELL
L9082:  CLC
L9083:  ADC  #$18
L9085:  STA  EYL
L9087:  LDA  EYH
L9089:  ADC  #$00
L908B:  STA  EYH
L908D:  CMP  #$FC
L908F:  BCC  L9096                  ;IFCS
L9091:  LDA  #$01                   ;TURN OFF STAR FIELD
L9093:  STA  PLAGRO
L9096:  LDA  EYL                    ;CALCULATE EYE-DESTINATION DELTA
L9098:  SEC
L9099:  SBC  EYLDES
L909B:  LDA  EYH
L909D:  BEQ  L90A1                  ;IFNE
L909F:  SBC  #$FF
L90A1:  BNE  L90BC                  ;IFEQ  PAST DESTINATION?
L90A3:  LDA  EYLDES                 ;YES STOP AT DEST
L90A5:  STA  EYL
L90A7:  LDA  #$FF
L90A9:  STA  EYH
L90AB:  LDA  #CPLAY                 ;GO PLAY GAME
L90AD:  BIT  QSTATUS
L90AF:  BMI  L90B3                  ;IFPL  ATTRACT?
L90B1:  LDA  #CENDGA                ;YES. END IT  [CS] Set game mode to test screen:
L90B3:  STA  QSTATE                 ;[CS] color bars test. HUH?!!?
L90B5:  LDX  PLAYUP
L90B7:  LDA  #$00                   ;[CS] Clear out the level selected
L90B9:  STA  BONUS,X                ;CLEAR BONUS  [CS] by the user to start on.
L90BC:  LDA  #$FF                   ;REQUEST WELL PIC UPDATE
L90BE:  STA  ROTDIS
L90C1:  JMP  MOVCUR                 ;UPDATE CURSOR POSITION

;------------------------------------------------------------------------------
; INIRA0 - INITIALIZE-PREPARE FOR SKILL LEVEL REQUEST STATE
;   [CS] LEVEL SELECTION CODE BEGINS HERE
;------------------------------------------------------------------------------
INIRA0:
L90C4:  LDA  HIWAVE                 ;YES. SET START LEVEL=ODD HIGHEST LEVEL  [CS] Find out what level the player is
;ACC=HIGHEST LEVEL (-1) COMPLETED IN LAST GAME
L90C7:  LDX  #<[LEVELE-LEVEL]       ;[CS] permitted to restart on. Look it
L90C9:  DEX                         ;[CS] up in a table to see how many
L90CA:  CMP  LEVEL,X                ;[CS] levels they have to choose from.
L90CD:  BCC  L90C9                  ;CSEND  EXIT WHEN WAVE IN TABLE <=HIGHEST LEVEL LAST GAME  [CS] X = # of levels to present
L90CF:  LDY  #$04
L90D1:  LDA  OPTIN3
L90D4:  AND  #$04
L90D6:  BEQ  L90EA                  ;IFNE  MAX MIN TIED TO HI SCORE OPTION?
L90D8:  LDA  HSCORH+21              ;YES. GET MSB OF HIGH SCORE
L90DB:  CMP  #$30                   ;[CS] Highest score > 300,000?
L90DD:  BCC  L90E0                  ;IFCS  >300000?
L90DF:  INY                         ;YES.
L90E0:  CMP  #$50                   ;[CS] Highest score > 500,000?
L90E2:  BCC  L90E5                  ;IFCS  >500000?
L90E4:  INY                         ;YES.
L90E5:  CMP  #$70                   ;[CS] Highest score > 700,000?
L90E7:  BCC  L90EA                  ;IFCS  >700000?
L90E9:  INY                         ;YES.
L90EA:  LDA  OPTIN1
L90EC:  AND  #$43
L90EE:  CMP  #$40
L90F0:  BNE  L90F4                  ;IFEQ  SALES MODE?
L90F2:  LDY  #$1B                   ;YES. ANYTHING GOES  [CS] Limit the number of
L90F4:  STY  TEMP0                  ;NEW MAX MIN  [CS] menu items to #1B?
L90F6:  CPX  TEMP0
L90F8:  BCS  L90FC                  ;IFCC  PLAYER HI LEVEL < MAX MIN?
L90FA:  LDX  TEMP0                  ;YES. USE MAX MIN FOR RIGHT LIMIT
L90FC:  STX  HIRATE                 ;MAX INDEX INTO LEVEL TABLE
L90FF:  LDA  QSTATUS
L9101:  BPL  INIRAT                 ;IFMI  ATTRACT?
L9103:  LDA  #$00                   ;NO
L9105:  STA  HIWAVE

;------------------------------------------------------------------------------
; INIRAT - [note] Set up the player about to start (SWAPEN for the 2nd player), reset cursor and level window;
;   outside attract mode enter the skill-level request state (CREQRAT / CDREQRA) with QTMPAUS=$10.
;------------------------------------------------------------------------------
INIRAT:
L9108:  LDX  NEWPLA
L910A:  STX  PLAYUP                 ;YES
L910C:  BEQ  L9111                  ;IFNE  SPECIAL CASE FOR 2ND PLAYER
L910E:  JSR  SWAPEN                 ;SWAP 1ST PLAYER'S ENEMIES OUT  [CS] Swap some game data between the active and inactive players.
L9111:  LDA  #$04                   ;SET UP DEFAULT LEVELS (LEFT & RIGHT SIDES)
L9113:  STA  RITSID
L9115:  LDA  #$FF                   ;STOP RUMBLE
L9117:  STA  EYH
L9119:  LDA  #$00                   ;INITIALIZE CURSOR
L911B:  STA  CURSL1                 ;[CS] Start the player on segment 00.
L911E:  STA  CURSPO                 ;[CS] Start the player at position 00.
L9120:  STA  LEFSID
L9122:  STA  TIMHIS                 ;NO ATTRACT DELAY  [CS] Zero out the 1/20th sec counter.
L9125:  LDX  QSTATUS
L9127:  BPL  L9144                  ;IFMI  ATTRACT?
L9129:  LDA  #SECOND                ;NO  [CS] A regularly decremented counter,
L912B:  STA  TIMHIS                 ;[CS] when it reachs 0, 1 second elapsed.
L912E:  LDA  #$FF
L9130:  STA  WELTYP                 ;PREVENT WRAP
L9133:  LDA  #CREQRAT               ;GO TO REQUEST  [CS] Set game mode to pre-game
L9135:  STA  QSTATE                 ;RATE STATE  [CS] level selection screen.
L9137:  LDA  #CDREQRA               ;[CS] Set mode to pre-game level
L9139:  STA  QDSTATE                ;REQUEST RATE DISPLAY STATE  [CS] selection.
L913B:  LDA  #$00                   ;[CS] Set current level to the
L913D:  STA  CURWAV                 ;TO GET 1ST COLORS  [CS] first screen.
L913F:  JSR  INICOL
L9142:  LDA  #$10                   ;START TIMER  [CS] Give the player ten seconds to
L9144:  STA  QTMPAUS                ;[CS] select their level.
L9146:  JSR  CLRPOT                 ;CLEAR POT  [CS] Zero out the spinner.

;------------------------------------------------------------------------------
; PRORAT - INITIALIZE-SET SKILL LEVEL
;   UPDATE TIMER
;   FALL INTO PRORAT STATE
;------------------------------------------------------------------------------
PRORAT:
L9149:  DEC  TIMHIS                 ;[CS] Has the 1 second timer reached
L914C:  BPL  L9169                  ;IFMI  ANOTHER SECOND DONE?  [CS] zero? If not, branch $9169
L914E:  SED                         ;YES  [CS] Go into decimal mode to adjust
L914F:  LDA  QTMPAUS                ;DECREMENT # SECONDS  [CS] the countdown from ten timer.
L9151:  SEC                         ;[CS] Reduce it by one.
L9152:  SBC  #$01
L9154:  STA  QTMPAUS
L9156:  CLD                         ;[CS] Go to normal math mode.
L9157:  BPL  L915D                  ;IFMI  SECONDS LEFT AT 0?
L9159:  LDA  #MFIRE                 ;YES. AUTO CHOOSE  [CS] Set the fire button as needing to
L915B:  STA  SWFINA                 ;[CS] be processed. Or Test button?
L915D:  CMP  #$03
L915F:  BNE  L9164                  ;IFEQ
L9161:  JSR  S3SWAR                 ;3 SECONDS WARNING
L9164:  LDA  #SECOND                ;RESTART FRACTIONAL SECONDS TIMER  [CS] Reset the 1/20th second countdown
L9166:  STA  TIMHIS                 ;[CS] timer to one second.
L9169:  JSR  GETCUR                 ;UPDATE CURSOR POSITION
L916C:  LDA  #MSUZA|MFIRE
L916E:  LDY  QTMPAUS
L9170:  CPY  #$08
L9172:  BCS  L9176                  ;IFCC
L9174:  LDA  #MSUZA|MFIRE|MSTRT1|MSTRT2 ;[CS] Have any buttons been pressed?
L9176:  AND  SWFINA
L9178:  BEQ  L91AE                  ;IFNE  PLAYER SELECTING THIS LEVEL
L917A:  LDA  #$00                   ;[CS] Clear the list of buttons that
L917C:  STA  SWFINA                 ;[CS] need to be processed.
L917E:  LDA  CURSL1                 ;YES. USE LEVEL FOR THIS PLAYER  [CS] Start the player on segment 00.
L9181:  TAY
L9182:  LDX  PLAYUP                 ;[CS] Figure out which player is up.
L9184:  STA  BONUS,X                ;[CS] Store the menu selection chosen by the user.
L9187:  LDA  LEVEL,Y
L918A:  BIT  QSTATUS
L918C:  BMI  L9197                  ;IFPL  ATTRACT?  [CS] If this is real gameplay, skip down a bit.
L918E:  LDY  #$01                   ;[CS] ATTRACT MODE GAMEPLAY SETUP
L9190:  STY  LIVES1                 ;[CS] Set player one lives to 1.
L9192:  LDA  RANDOM                 ;YES. CHOOSE FROM 1ST 8 LEVELS  [CS] Get a random number
L9195:  AND  #$07                   ;[CS] 0-7.
L9197:  STA  WAVEN1,X               ;[CS] Store it as the current level for player one.
L9199:  STA  CURWAV                 ;[CS] Store it in the other current level register (which is level -1)
L919B:  JSR  INICOL
L919E:  JSR  CONTOUR
L91A1:  JSR  INIENE                 ;INITIALIZE ENEMY  [CS] Set the number of enemies to appear and the size of the spikes that start off in the tunnel.
L91A4:  JSR  INISUZ                 ;NEW SUPERZAPPER  [CS] Clear out the superzapper status.
L91A7:  LDA  #CNEWLI                ;GO ON TO GAME PLAY
L91A9:  STA  QSTATE
L91AB:  JSR  CLRPOT                 ;CLEAR POT  [CS] Zero out the spinner.
L91AE:  LDA  SWFINA
L91B0:  AND  #<[~[MFAKE|MFIRE|MSUZA|MSTRT1|MSTRT2]]
L91B2:  STA  SWFINA                 ;CLEAR "SWITCHES NOT PROCESSED" FLAG
L91B4:  RTS

;------------------------------------------------------------------------------
; BONSCO - BONUS SCORE DETERMINATION
;   INPUT: ACC=BONUS LEVEL INDEX
;   OUTPUT:TEMP0,1,&2:BONUS POINTS
;   ACC,X DESTROYED
;------------------------------------------------------------------------------
BONSCO:
L91B5:  ASL
L91B6:  TAX
L91B7:  LDA  #$00                   ;LSB ALWAYS 0
L91B9:  STA  TEMP0
L91BB:  LDA  BONPTM,X
L91BE:  STA  TEMP1
L91C0:  LDA  BONPTH,X
L91C3:  STA  TEMP2
L91C5:  RTS

BONPTM:
;[CS] Data segment used by above subroutine per Ken Lui.
L91C6:  .word $0000, $0060, $0160, $0320
.alias BONPTH           $91C7    ;inside the line at L91C6 (L91C6+1)
L91CE:  .word $0540, $0740, $0940, $1140
L91D6:  .word $1340
L91D8:  .word $1520, $1700, $1880, $2080
L91E0:  .word $2260, $2480, $2660, $3000
L91E8:  .word $3400
L91EA:  .word $3820, $4150, $4390, $4720
L91F2:  .word $5310, $5810
L91F6:  .word $6240, $6560, $7660, $8980

LEVEL:           ;TABLE OF LEVEL #S(-1) FOR RATING DISPLAY
L91FE:  .byte $00, $02, $04, $06, $08, $0A, $0C, $0E ;[CS] Data used by $90C4 and $9187. This table contains a list of the valid levels that a user is able to start on. The highest starting level is decimal 81 (#50 + 1).
L9206:  .byte $10, $13, $15, $17, $19, $1B, $1E, $20
L920E:  .byte $23, $27, $2B, $2E, $30, $33, $37, $3B
L9216:  .byte $3E, $40
L9218:  .byte $48, $50

LEVELE:
L921A:  .byte $FF                   ;END OF TABLE FLAG

;------------------------------------------------------------------------------
; INICUR - INITIALIZE - CURSOR
;   [CS] Subroutine: Set the player's position
;------------------------------------------------------------------------------
INICUR:
L921B:  LDA  #$0E                   ;INITIALIZE CURSOR POSITION  [CS] Put the player on segment 0F
L921D:  STA  CURSL1                 ;[CS] (E+1) of the tunnel.
L9220:  LDA  #$F0
L9222:  STA  CURSPO                 ;[CS] Put the player at position F0.
L9224:  LDA  #$00
L9226:  STA  CURMOD
L9229:  LDA  #$0F                   ;[CS] Put the player on segment F
L922B:  STA  CURSL2                 ;[CS] of the tunnel.
L922E:  LDA  #ILINLIY               ;[CS] Put the player at the top of
L9230:  STA  CURSY                  ;[CS] the tunnel.
L9233:  RTS

;------------------------------------------------------------------------------
; INIENE - INITIALIZE - NYMPHS
;   INITIALIZE NYMPHS
;   [CS] Subroutine: Upon new tunnel, set # of enemies and size of spikes.
;   [CS] Only used for *certain* levels.
;------------------------------------------------------------------------------
INIENE:
L9234:  LDA  NWNYMC                 ;INITIALIZE FOR NEW WAVE (NYMPH COUNT + ENEMY LINE HEIGHT)  [CS] Find out how many enemies to
L9237:  STA  NYMCOU                 ;[CS] create for this level and set it.
L923A:  LDA  NWTELI                 ;INITIALIZE ENEMY LINES HIGHT  [CS] Find out how tall the spikes

;.SBTTL INIT ENEMY LINES
;ACC=INITIAL HEIGHT
L923D:  LDX  #NLINES-1              ;[CS] should be to start with and
L923F:  STA  LINEY,X                ;[CS] create spikes in all the tunnel
L9242:  DEX                         ;[CS] segments to be that large.
L9243:  BPL  L923F                  ;MIEND
L9245:  RTS

;------------------------------------------------------------------------------
; ININYM - INITIALIZE NYMPHS  (comment at the call)
;------------------------------------------------------------------------------
ININYM:
L9246:  LDA  #$00                   ;[CS] Clear out the $40 timers used
L9248:  LDX  #NNYMPH-1              ;[CS] to say when an enemy can be put
L924A:  STA  NYMPY,X                ;[CS] onto the tube.
L924D:  DEX                         ;[CS] "Appearance Timer"
L924E:  BPL  L924A                  ;MIEND
L9250:  LDX  NYMCOU                 ;[CS] Get the number of enemies to make an appearance in this tube.
L9253:  DEX
L9254:  LDA  RANDOM                 ;[CS] Get a random number.
L9257:  AND  #$0F
L9259:  STA  NYMPL,X                ;[CS] Basic idea:
L925C:  TXA                         ;[CS] Randomize the movement styles
L925D:  ASL                         ;[CS] for the enemies that are going
L925E:  ASL                         ;[CS] to be placed in the tube.
L925F:  ASL
L9260:  ASL
L9261:  ORA  NYMPL,X
L9264:  BNE  L9268                  ;IFEQ
L9266:  LDA  #$0F
L9268:  STA  NYMPY,X
L926B:  DEX
L926C:  BPL  L9254                  ;MIEND
L926E:  RTS

;------------------------------------------------------------------------------
; INIINV - INITIALIZE - INVADERS
;   INITIALIZE INVADERS
;   [CS] Subroutine to remove all enemies from the tube.
;------------------------------------------------------------------------------
INIINV:
L926F:  LDX  #NINVAD-1              ;[CS] Clear out the table that holds
L9271:  LDA  #$00                   ;[CS] the height of the enemies inside
L9273:  STA  INVAY,X                ;DEACTIVATE  [CS] the tunnel
L9276:  DEX
L9277:  BPL  L9273                  ;MIEND
L9279:  STA  INMCOU                 ;[CS] No enemies inside the tube or
L927C:  STA  INCCOU                 ;[CS] at the top of the tube.
L927F:  STA  SPINCO                 ;[CS] Clear out the addresses which
L9282:  STA  FLIPCO                 ;[CS] contain how many of each type
L9285:  STA  TANKCO                 ;[CS] of enemy are currently in
L9288:  STA  PULSCO                 ;[CS] the tubes.
L928B:  STA  FUSECO
L928E:  RTS

;------------------------------------------------------------------------------
; INICHA - INITIALIZE - CHARGES
;   [CS] Subroutine to remove all bullets from play.
;------------------------------------------------------------------------------
INICHA:
L928F:  LDA  #$00
L9291:  LDX  #NCHARG-1
L9293:  STA  CHARY,X                ;DEACTIVATE CHARGE  [CS] Get rid of all the bullets,
L9296:  DEX                         ;[CS] both player and enemy.
L9297:  BPL  L9293                  ;MIEND
L9299:  STA  CHACOU                 ;[CS] Player bullets in play = 0
L929C:  STA  ESHCOU                 ;[CS] Enemy bullets in play = 0
L929E:  RTS

;------------------------------------------------------------------------------
; INIEXP - INITIALIZE EXPLOSIONS
;------------------------------------------------------------------------------
INIEXP:
L929F:  LDX  #NEXPLO-1              ;[CS] Clear out the special images
L92A1:  LDA  #$00                   ;[CS] to display for an enemy death
L92A3:  STA  EXPLOY,X               ;[CS] and set the death-in-progress
L92A6:  DEX                         ;[CS] counter to ZERO.
L92A7:  BPL  L92A3                  ;MIEND
L92A9:  STA  EXPCOU                 ;[CS] Clear out any enemy death sequences in progress.
L92AC:  RTS

;------------------------------------------------------------------------------
; CLRPOT - CLEARS POTS
;   [CS] SUBROUTINE: Zero out the spinner since last read.
;------------------------------------------------------------------------------
CLRPOT:
L92AD:  LDA  #$00                   ;[CS] Zero out the "spinner position
L92AF:  STA  TBHD                   ;[CS] since last read" location.
L92B1:  RTS

;------------------------------------------------------------------------------
; SWAPEN - SWAP 1ST PLAYER'S ENEMIES OUT  (comment at the call)
;   SWAP ENEMIES  (another call)
;   [CS] Subroutine: Swap some data between the inactive and active player.
;------------------------------------------------------------------------------
SWAPEN:
L92B2:  LDX  #SAVEND-SAVEP-1        ;[CS] This will swap the following
L92B4:  LDA  ACTIP,X                ;SWAP ACTIVE TO SAVE AREAS  [CS] variables between the active
L92B7:  LDY  SAVEP,X                ;[CS] player and the inactive player:
L92BA:  STA  SAVEP,X
L92BD:  TYA                         ;[CS] # of times superzapper used
L92BE:  STA  ACTIP,X                ;[CS] # of enemies left
L92C1:  DEX                         ;[CS] Height of the spikes in the
L92C2:  BPL  L92B4                  ;MIEND  [CS] segments.
L92C4:  RTS

;------------------------------------------------------------------------------
; CONTOUR - INITIALIZE-SET SKILL LEVEL FOR WAVE
;------------------------------------------------------------------------------
CONTOUR:
L92C5:  LDA  CURWAV                 ;[CS] Are we at the last level?
L92C7:  CMP  #$62
L92C9:  BCC  L92D2                  ;IFCS
L92CB:  LDA  RANDO2                 ;[CS] Grab a random number,
L92CE:  AND  #$1F                   ;[CS] 40-5F.
L92D0:  ORA  #$40
L92D2:  STA  TEMP2                  ;[CS] Store into a scratchpad address
L92D4:  INC  TEMP2                  ;[CS] and add one.
L92D6:  LDX  #<[WTABEND-WTABLE-1]
L92D8:  STX  INDEX1
L92DA:  LDX  INDEX1
L92DC:  LDA  WTABLE,X
L92DF:  STA  INDYHI
L92E1:  LDA  WTABLE-1,X
L92E4:  STA  INDYLO                 ;SET UP POINTER TO BYTE TO BE SET UP
L92E6:  LDA  WTABLE-2,X
L92E9:  STA  TEMP4
L92EB:  LDA  WTABLE-3,X
L92EE:  STA  TEMP3                  ;SET UP POINTER TO ARRAY OP PARAMETERS
L92F0:  LDA  #$01
L92F2:  STA  INDEX2                 ;SET UP START RANGE COUNTER
L92F4:  LDY  #$00                   ;SET UP TABLE POINTER
L92F6:  LDA  (TEMP3),Y
L92F8:  STA  TYPCOD                 ;GET TYPE OF RECORD
L92FB:  BEQ  TEXIT                  ;EXIT ON EOT TYPE CODE WITH 0
L92FD:  LDA  TEMP2
L92FF:  INY
L9300:  CMP  (TEMP3),Y
L9302:  INY
L9303:  BCC  L9313                  ;IFCS  IS CURRENT WAVE>=START WAVE OF RANGE?
L9305:  CMP  (TEMP3),Y              ;YES.
L9307:  BNE  L930A                  ;IFEQ  <=END WAVE OF RANGE?
L9309:  CLC
L930A:  BCS  L9313                  ;IFCC
L930C:  INY
L930D:  JSR  DOTYPE                 ;YES. GET PARAMETER FROM RECORD
L9310:  JMP  TEXIT                  ;EXIT LOOP
L9313:  JSR  DONEXT                 ;DO. UP POINTER TO NEXT RECORD
L9316:  CLC
L9317:  BCC  L92F6                  ;CSEND  ALWAYS LOOP

TEXIT:
L9319:  LDY  #$00                   ;GOT PARAMETER
L931B:  STA  (INDYLO),Y             ;SAVE IT
L931D:  LDA  INDEX1
L931F:  SEC
L9320:  SBC  #$04
L9322:  STA  INDEX1                 ;UPDATE MASTER TABLE POINTER
L9324:  CMP  #$FF
L9326:  BNE  L92DA                  ;EQEND

;.SBTTL EASY - MED - HARD OPTIONS
L9328:  LDA  OPTIN3
L932B:  AND  #$03
L932D:  CMP  #ZEASY
L932F:  BNE  L934D                  ;IFEQ  EASY?
L9331:  DEC  WCHAMX                 ;YES. LESS ENEMY SHOTS
L9334:  LDA  WINVIL
L9337:  EOR  #$FF
L9339:  LSR
L933A:  LSR
L933B:  LSR
L933C:  ADC  WINVIL
L933F:  STA  WINVIL                 ;DECREASE SPEEDS BY 1/8
L9342:  LDA  CURWAV                 ;[CS] Are up to at least the RED
L9344:  CMP  #$11                   ;[CS] level? (The level after Green.)
L9346:  BCS  L934A                  ;IFCC
L9348:  DEC  WTTFRA                 ;DECREASE FLIP RATE AT TOP
L934A:  CLV                         ;ELSE
L934B:  BVC  L9382
L934D:  CMP  #ZHARD
L934F:  BNE  L9382                  ;IFEQ  HARD?
L9351:  INC  WCHAMX                 ;YES. MORE ENEMY SHOTS UP TO 4
L9354:  LDA  WCHAMX
L9357:  CMP  #$03
L9359:  BCC  L9360                  ;IFCS
L935B:  LDA  #$03
L935D:  STA  WCHAMX
L9360:  LDA  WINVIL                 ;INCREASE SPEED BY 1/8
L9363:  LSR
L9364:  LSR
L9365:  LSR
L9366:  ORA  #$E0
L9368:  ADC  WINVIL
L936B:  STA  WINVIL
L936E:  LDA  NWNYMC                 ;INCREASE ATTACK BY 1/8
L9371:  LSR
L9372:  LSR
L9373:  LSR
L9374:  ADC  NWNYMC
L9377:  STA  NWNYMC
L937A:  LDA  WPULFI
L937D:  ORA  #ZFIRYE
L937F:  STA  WPULFI                 ;PULSARS FIRE
L9382:  LDA  WINVIL+ZABTRA          ;SPINNER
L9385:  JSR  TIMES8
L9388:  STA  WINVIL+ZABTRA          ;SPEED (FRAC)
L938B:  STY  WINVIN+ZABTRA          ;SPEED (INT)
L938E:  STX  ENSIZE+ZABTRA          ;COLLISION RANGE
L9391:  LDA  WCHARL
L9394:  JSR  TIMES8                 ;ENEMY SHOT
L9397:  STA  WCHARL
L939A:  STY  WCHARIN
L939D:  STX  CHACHA                 ;CHARGE CHARGE COLLISION RANGE
L939F:  LDA  WINVIL
L93A2:  JSR  TIMES8
L93A5:  STA  WINVIL
L93A8:  STA  WINVIL+ZABTAN
L93AB:  STY  WINVIN+ZABTAN
L93AE:  STY  WINVIN
L93B1:  STX  ENSIZE+ZABFLI          ;CHARGE INVADER COLLISION RANGE
L93B4:  STX  ENSIZE+ZABTAN
L93B7:  STX  ENSIZE+ZABPUL
L93BA:  LDA  WINVIL
L93BD:  ASL
L93BE:  STA  WFUSIL
L93C1:  LDA  WINVIN
L93C4:  ROL
L93C5:  STA  WFUSIH                 ;FUSE INC=2X INVADER SPEED
L93C8:  LDA  #[PCVELO+3]/2
L93CA:  STA  ENSIZE+ZABFUS
L93CD:  LDA  #$A0
L93CF:  STA  WINVIL+ZABPUL
L93D2:  LDA  #$FE
L93D4:  STA  WINVIN+ZABPUL
L93D7:  LDA  #ZCARFL
L93D9:  STA  WTACAR+1
L93DC:  STA  WTACAR+0
L93DF:  RTS

;------------------------------------------------------------------------------
; TIMES8 - INPUT: ACC=SPEED (SIGNED)
;   OUTPUT: ACC=LOW BYTE OF SPEED
;   Y=HI BYTE OF SPEED (SIGN EXT)
;   X=COLLISION RANGE WITH PC
;   TEMP0 TRASHED
;------------------------------------------------------------------------------
TIMES8:
L93E0:  LDY  #$FF                   ;ALL SPEEDS ARE MINUS SO START SIGN
L93E2:  STY  TEMP0                  ;EXTEND AT ALL-
L93E4:  ASL
L93E5:  ROL  TEMP0
L93E7:  ASL
L93E8:  ROL  TEMP0
L93EA:  ASL
L93EB:  ROL  TEMP0                  ;X 8
L93ED:  LDY  TEMP0
L93EF:  PHA                         ;SAVE RESULT
L93F0:  TYA                         ;COLLISION RANGE=AVERAGE OF
L93F1:  EOR  #$FF                   ;ABS VAL OF SPEEDS.
L93F3:  CLC
L93F4:  ADC  #PCVELO+1+1+2
L93F6:  LSR
L93F7:  TAX
L93F8:  PLA
L93F9:  RTS

;.SBTTL SKILL CONTOUR TABLES

TCHARFR:
;FRAMES UNTIL INVADER CAN FIRE (28 PER SECOND)
L93FA:  .byte TA, $01, $14, $50, $FD
L93FF:  .byte T1, $15, $40, $14
L9403:  .byte T1, $41, $63, $0A

TCHAMX:
L9407:  .byte TZ, $01, $09, $01, $01, $01, $02, $03 ;ADD 1
L940F:  .byte $02, $02, $03, $03
L9413:  .byte T1, $0A, $40, $02
L9417:  .byte T1, $41, $63, $03

TINVIN:
;ENEMY SHOT INCREMENT
L941B:  .byte TA, $01, $08, $D4, $FB
L9420:  .byte TZ, $09, $10, $AF, $AC, $AC, $AC, $A8
L9428:  .byte $A4, $A0, $A0
L942B:  .byte TA, $11, $19, $AF, $FD
L9430:  .byte TA, $1A, $20, $9D, $FD
L9435:  .byte TA, $21, $27, $94, $FD
L943A:  .byte TA, $28, $30, $92, $FF
L943F:  .byte TA, $31, $40, $88, $FF
L9444:  .byte TR, $41, $63, $60, $41

TCHARIN:
L9449:  .byte TB, $01, $63, $C0

TSPIIN:
L944D:  .byte TB, $01, $14, $00
L9451:  .byte TB, $15, $20, $D0
L9455:  .byte TB, $21, $30, $D8
L9459:  .byte TB, $31, $63, $D0

WPULPOT:         ;PULSAR POTENCY HEIGHT
L945D:  .byte T1, $01, $20, $A0
L9461:  .byte T1, $21, $40, $A0
L9465:  .byte T1, $41, $63, $C0

WPULTIM:         ;PULSAR TIMER INCREMENT
L9469:  .byte T1, $01, $30, $04
L946D:  .byte T1, $31, $40, $06
L9471:  .byte T1, $41, $63, $08

WWTAC2:
L9475:  .byte T1, $01, $20, ZCARFL
L9479:  .byte T1, $21, $28, ZCARFU
L947D:  .byte T1, $29, $63, ZCARPU

WWTAC3:
L9481:  .byte T1, $01, $30, ZCARFL
L9485:  .byte T1, $31, $63, ZCARFU

WSPIMI:
L9489:  .byte TZ, $01, $04, $00, $00, $00, $01
L9490:  .byte T1, $05, $10, $02
L9494:  .byte T1, $11, $13, $00
L9498:  .byte T1, $14, $20, $01
L949C:  .byte T1, $23, $27, $01
L94A0:  .byte T1, $2C, $63, $01
L94A4:  .byte TE

WSPIMX:
L94A5:  .byte TZ, $01, $06, $00, $00, $00, $02, $03
L94AD:  .byte $04
L94AE:  .byte T1, $07, $0A, $04
L94B2:  .byte T1, $0B, $10, $03
L94B6:  .byte T1, $14, $19, $02
L94BA:  .byte TZ, $1A, $20, $01, $02, $02, $02, $01
L94C2:  .byte $01, $02
L94C4:  .byte T1, $35, $27, $01
L94C8:  .byte T1, $2B, $63, $01
L94CC:  .byte TE

WFLIMI:
L94CD:  .byte T1, $01, $04, $01
L94D1:  .byte T1, $05, $63, $00
L94D5:  .byte TE

WFLIMX:
L94D6:  .byte T1, $01, $04, $04
L94DA:  .byte T1, $05, $10, $05
L94DE:  .byte T1, $11, $13, $03
L94E2:  .byte T1, $14, $19, $04
L94E6:  .byte T1, $1A, $63, $05
L94EA:  .byte TE

WTANMI:
L94EB:  .byte TZ, $01, $04, $00, $00, $01, $00
L94F2:  .byte T1, $05, $10, $01
L94F6:  .byte T1, $11, $20, $01
L94FA:  .byte T1, $21, $27, $01
L94FE:  .byte T1, $28, $63, $01
L9502:  .byte TE

WTANMX:
L9503:  .byte TZ, $01, $05, $00, $00, $01, $00, $01
L950B:  .byte T1, $06, $10, $02
L950F:  .byte T1, $11, $1A, $01
L9513:  .byte T1, $1B, $20, $01
L9517:  .byte T1, $21, $2C, $02
L951B:  .byte T1, $2D, $63, $03
L951F:  .byte TE

WPULMI:
L9520:  .byte T1, $11, $20, $02
L9524:  .byte T1, $21, $63, $01
L9528:  .byte TE

WPULMX:
L9529:  .byte TZ, $11, $20, $05, $03, $02, $02, $02
L9531:  .byte $02, $02, $02, $02, $02, $02, $02, $02
L9539:  .byte $03, $04, $02
L953C:  .byte T1, $21, $63, $03
L9540:  .byte TE

WFUSMI:
L9541:  .byte T1, $0B, $10, $01
L9545:  .byte T1, $16, $19, $01
L9549:  .byte T1, $1B, $63, $01
L954D:  .byte TE

WFUSMX:
L954E:  .byte T1, $0B, $10, $01
L9552:  .byte T1, $16, $19, $01
L9556:  .byte T1, $1B, $20, $01
L955A:  .byte T1, $21, $27, $04
L955E:  .byte T1, $28, $63, $03
L9562:  .byte TE

TPUCHDE:
L9563:  .byte TZ, $11, $12, PN, PC
L9568:  .byte TR, $13, $20, PC, PN
L956D:  .byte TA, $21, $27, $14, $FF
L9572:  .byte TR, $28, $63, $14, $0A
L9577:  .byte TE

TWFUSC:
L9578:  .byte TR, $11, $20, $00, $40
L957D:  .byte TR, $21, $30, $40, $C0
L9582:  .byte T1, $31, $63, $C0
L9586:  .byte TE

TFUFRQ:
L9587:  .byte T1, $01, $10, $DC
L958B:  .byte T1, $11, $27, $C0
L958F:  .byte TA, $28, $40, $C0, $01
L9594:  .byte T1, $41, $63, $E6

TINVMX:
L9598:  .byte T1, $01, $63, $06

TELIHI:
L959C:  .byte TZANDF, $01, $63, $00, $00, $00, $E0, $D8
L95A4:  .byte $D4, $D0, $C8, $C0, $B8, $B0, $A8, $A0
L95AC:  .byte $A0, $A0, $A8, $A0, $9C, $9A, $98

TNYMMX:
L95B3:  .byte TZ, $01, $10, $0A, $0C, $0F, $11, $14
L95BB:  .byte $16, $14, $18, $1B, $1D, $1B, $18, $1A
L95C3:  .byte $1C, $1E, $1B
L95C6:  .byte TA, $11, $1A, $14, $01
L95CB:  .byte T1, $1B, $27, $1B
L95CF:  .byte TA, $28, $30, $1D, $01
L95D4:  .byte TA, $31, $40, $1F, $01
L95D9:  .byte TA, $41, $50, $23, $01
L95DE:  .byte TA, $51, $63, $2B, $01

TWTTFRA:
L95E3:  .byte T1, $01, $14, $02
L95E7:  .byte T1, $15, $20, $02
L95EB:  .byte T1, $21, $63, $03

TWPULF:
L95EF:  .byte T1, $3C, $63, ZFIRYE
L95F3:  .byte TE

CAMWAV:
;  CAMWAV: TZANDF,1,99 then 16 bytes: flipper CAM per (wave-1)&15
;    entries: NOJUMP, MOVJMP, SPIRAL, SPIRCH, COWJM2, MOVJMP, SPIRCH, SPIRAL, COWJM2, AVOIDR, SPIRCH, SPIRAL, COWJM2, NOJUMP, AVOIDR, SPIRCH
;SEQUENCE:CIRCLE,SQUARE,CROSS,PEANUT,KEY,TRIANGLE,CLOVER,V,STAIRS,U,FLAT,
;HEART,STAR,WAVES,TOPO,8
L95F4:  .byte TZANDF, $01, $63
L95F7:  .byte NOJUMP-CAM
L95F8:  .byte MOVJMP-CAM
L95F9:  .byte SPIRAL-CAM
L95FA:  .byte SPIRCH-CAM
L95FB:  .byte COWJM2-CAM
L95FC:  .byte MOVJMP-CAM
L95FD:  .byte SPIRCH-CAM
L95FE:  .byte SPIRAL-CAM
L95FF:  .byte COWJM2-CAM
L9600:  .byte AVOIDR-CAM
L9601:  .byte SPIRCH-CAM
L9602:  .byte SPIRAL-CAM
L9603:  .byte COWJM2-CAM
L9604:  .byte NOJUMP-CAM
;[CS] Data segment per Ken Lui.
L9605:  .byte AVOIDR-CAM
L9606:  .byte SPIRCH-CAM

WTABLE:
L9607:  .word TWPULF, WPULFI
L960B:  .word TWTTFRA, WTTFRA
L960F:  .word TCHARFR, WCHARFR      ;INVADER'S FIRE TIMER (FRAMES)
L9613:  .word TCHAMX, WCHAMX        ;MAX # ENEMY SHOTS -1
L9617:  .word WFLIMI, WFLMIN        ;MIN # FLIPPERS
L961B:  .word WFLIMX, WFLMAX        ;MAX
L961F:  .word WPULMI, WPUMIN
L9623:  .word WPULMX, WPUMAX
L9627:  .word WTANMI, WTAMIN
L962B:  .word WTANMX, WTAMAX
L962F:  .word WSPIMI, WSPMIN
L9633:  .word WSPIMX, WSPMAX
L9637:  .word WFUSMI, WFUMIN
L963B:  .word WFUSMX, WFUMAX
L963F:  .word WPULPOT, PULPOT
L9643:  .word WPULTIM, PULTIM
L9647:  .word WWTAC2, WTACAR+2
L964B:  .word WWTAC3, WTACAR+3
L964F:  .word TINVMX, WINVMX
L9653:  .word TNYMMX, NWNYMC
L9657:  .word TELIHI, NWTELI
L965B:  .word TPUCHDE, PUCHDE       ;PULSAR CHASE DELAY
L965F:  .word CAMWAV, WFLICAM       ;FLIPPER CAM
L9663:  .word TSPIIN, WINVIL+ZABTRA
L9667:  .word TCHARIN, WCHARL
L966B:  .word TINVIN, WINVIL
L966F:  .word TWFUSC, WFUSCH
L9673:  .word TFUFRQ, WFUFRQ

;------------------------------------------------------------------------------
; WTABEND - PARAMETER TYPE CODE EXTRACTION VECTORS
;   INPUT:Y=POINTER TO 1ST PARAMETER IN RECORD
;   TYPCOD=RECORD TYPE
;   (also: DOTYPE)
;------------------------------------------------------------------------------
WTABEND:
DOTYPE:
L9677:  LDX  TYPCOD                 ;[CS] HEY.... Re-disassemble. We've got a
L967A:  LDA  SPARAD+1,X             ;[CS] JSR to $9677 from $930D.
L967D:  PHA
L967E:  LDA  SPARAD,X
L9681:  PHA
L9682:  RTS

;------------------------------------------------------------------------------
; DONEXT - INPUT:Y=PTS. TO END RANGE FIELD
;------------------------------------------------------------------------------
DONEXT:
L9683:  LDX  TYPCOD
L9686:  LDA  NPARAD+1,X
L9689:  PHA
L968A:  LDA  NPARAD,X
L968D:  PHA
L968E:  RTS

SPARAD:
;[CS] Data segment, per Ken Lui.
L968F:  .word $0000                 ;EOT
L9691:  .word SAMALL-1              ;ONE BYTE FOR ALL
L9693:  .word ITMIZE-1              ;ITEMIZED BYTE/LEVEL
L9695:  .word DOTZAN-1
L9697:  .word DOTA-1
L9699:  .word DOTB-1
L969B:  .word DOTR-1

NPARAD:
;[CS] Another data segment, per Ken Lui.
L969D:  .word $0000
L969F:  .word ONEBYT-1
L96A1:  .word NITMIZ-1
L96A3:  .word NITMIZ-1
L96A5:  .word TWOBYT-1
L96A7:  .word ONEBYT-1
L96A9:  .word TWOBYT-1

;------------------------------------------------------------------------------
; DOTZAN - [note] Parameter type TZANDF: wave index = ((TEMP2-1) AND $0F)+1, then continue as ITMIZE
;   (one byte per wave, repeating every 16 waves).
;------------------------------------------------------------------------------
DOTZAN:
L96AB:  LDA  TEMP2                  ;[CS] Chances are we'll have to re disassemble these two lines.
L96AD:  SEC
L96AE:  SBC  #$01
L96B0:  AND  #$0F
L96B2:  CLC
L96B3:  ADC  #$01
L96B5:  BPL  ITMIZ2

;------------------------------------------------------------------------------
; ITMIZE - [note] Parameter type TZ: return in A the byte for wave TEMP2 from the record's per-wave list.
;------------------------------------------------------------------------------
ITMIZE:
L96B7:  LDA  TEMP2

ITMIZ2:
L96B9:  STY  TEMP0                  ;ITEMIZED BYTE FOR EACH WAVE
L96BB:  DEY
L96BC:  DEY
L96BD:  SEC
L96BE:  SBC  (TEMP3),Y
L96C0:  CLC
L96C1:  ADC  TEMP0
L96C3:  TAY

;------------------------------------------------------------------------------
; SAMALL - SAME BYTE FOR EACH WAVE IN RANGE
;------------------------------------------------------------------------------
SAMALL:
L96C4:  LDA  (TEMP3),Y
L96C6:  RTS

;------------------------------------------------------------------------------
; TWOBYT - [CS] Apparently unreferenced?
;------------------------------------------------------------------------------
TWOBYT:
L96C7:  INY                         ;[CS] Y=Y+3

;------------------------------------------------------------------------------
; ONEBYT - [note] Record skip (NPARAD): Y += 2 past a one-byte parameter field; TWOBYT enters one INY earlier.
;------------------------------------------------------------------------------
ONEBYT:
L96C8:  INY
L96C9:  INY
L96CA:  RTS

;------------------------------------------------------------------------------
; NITMIZ - [note] Record skip (NPARAD) for TZ/TZANDF records: advance Y past the (end-start+1) itemized bytes.
;------------------------------------------------------------------------------
NITMIZ:
L96CB:  LDA  (TEMP3),Y
L96CD:  DEY
L96CE:  SEC
L96CF:  SBC  (TEMP3),Y
L96D1:  STA  TEMP0
L96D3:  TYA
L96D4:  SEC
L96D5:  ADC  TEMP0
L96D7:  TAY
L96D8:  INY
L96D9:  INY
L96DA:  RTS

;------------------------------------------------------------------------------
; DOTB - [CS] Apparently unreferenced?
;------------------------------------------------------------------------------
DOTB:
L96DB:  LDA  (TEMP3),Y
L96DD:  CLC
L96DE:  ADC  WINVIL
L96E1:  RTS

;------------------------------------------------------------------------------
; DOTA - [note] Parameter type TA: A = byte 3 + n * byte 4, n = RANGER (the wave's offset in the range).
;------------------------------------------------------------------------------
DOTA:
L96E2:  JSR  RANGER
L96E5:  TAX
L96E6:  LDA  (TEMP3),Y
L96E8:  INY
L96E9:  CPX  #$00
L96EB:  BEQ  L96F3                  ;IFNE
L96ED:  CLC
L96EE:  ADC  (TEMP3),Y
L96F0:  DEX
L96F1:  BNE  L96ED                  ;EQEND
L96F3:  RTS

;------------------------------------------------------------------------------
; RANGER - [note] A = TEMP2 - start wave of the record (source: # of levels between start and end); Y preserved.
;------------------------------------------------------------------------------
RANGER:
L96F4:  LDA  TEMP2                  ;CALCULATE # OF LEVELS BETWEEN
L96F6:  STY  TEMP0                  ;START AND END INCLUSIVE (ACC).
L96F8:  DEY                         ;PRESERVE Y
L96F9:  DEY
L96FA:  SEC
L96FB:  SBC  (TEMP3),Y
L96FD:  INY
L96FE:  INY
L96FF:  RTS

;------------------------------------------------------------------------------
; DOTR - ALTERNATE BETWEEN 2 VALUES
;------------------------------------------------------------------------------
DOTR:
L9700:  JSR  RANGER
L9703:  AND  #$01
L9705:  BEQ  L9708                  ;IFNE
L9707:  INY
L9708:  LDA  (TEMP3),Y
L970A:  RTS

;------------------------------------------------------------------------------
; PLAY - PLAY - MAINLINE (TOP OF WELL)
;------------------------------------------------------------------------------
PLAY:
L970B:  JSR  MOVCUR                 ;MOVE CURSOR AROUND
L970E:  JSR  FIREPC                 ;FIRE PLAYER CHARGE
L9711:  JSR  PROSUZ                 ;PROCESS SUPER ZAP
L9714:  JSR  MOVNYM                 ;MOVE NYMPHS
L9717:  JSR  MOVINV                 ;MOVE INVADERS
L971A:  JSR  MOVCHA                 ;MOVE CHARGES
L971D:  JSR  FIREIC                 ;FIRE INVADER CHARGE
L9720:  JSR  COLLIS                 ;COLLISION DETECT
L9723:  JSR  PROEXP                 ;EXPLOSIONS
L9726:  JMP  ANALYZ                 ;ANALYZE PLAYER STATUS

;------------------------------------------------------------------------------
; PLDROP - PLAY - MAINLINE (DROP MODE)
;   PLAYER IS SHOOTING THRU TUBE TO GET TO NEXT
;------------------------------------------------------------------------------
PLDROP:
L9729:  LDA  ELICNT                 ;CLEAR WARNING REQUEST
L972C:  AND  #$7F
L972E:  STA  ELICNT
L9731:  JSR  MOVCUR                 ;MOVE CURSOR AROUND
L9734:  JSR  MOVCUD                 ;MOVE CURSOR DOWN
L9737:  JSR  PROEXP                 ;EXPLOSIONS
L973A:  JSR  FIREPC                 ;FIRE PLAYER CHARGES
L973D:  JSR  MOVCHA                 ;MOVE CHARGES
L9740:  LDA  CURSL2
L9743:  BPL  L9748                  ;IFMI  CURSOR DEAD?
L9745:  JSR  ANALYZ                 ;YES. ANALYZE CURSOR STATUS
L9748:  RTS

;------------------------------------------------------------------------------
; MOVCUR - PLAY - MOVE CURSOR (PRELIMINARY CHECK)
;------------------------------------------------------------------------------
MOVCUR:
L9749:  LDA  CURSL2                 ;[CS] What segment is the player on?
L974C:  BPL  L974F                  ;IFMI  --;CURSOR DEAD?  [CS] If the player is dead, then
L974E:  RTS                         ;YES. DON'T MOVE IT  [CS] just return. (segment >= 80)

;.SBTTL PLAY - MOVE CURSOR (MAINLINE)
L974F:  LDX  #$00
L9751:  LDA  QSTATUS
L9753:  BMI  L975B                  ;IFPL  ATTRACT?
L9755:  JSR  AUTOCU                 ;YES, AUTO MOVEMENT
L9758:  CLV                         ;ELSE
L9759:  BVC  L9770
L975B:  LDA  TBHD                   ;NO. MANUAL
L975D:  BPL  L9768                  ;IFMI  MAXIMIZE KNOB READING
L975F:  CMP  #$E1
L9761:  BCS  L9765                  ;IFCC
L9763:  LDA  #$E1
L9765:  CLV                         ;ELSE
L9766:  BVC  L976E
L9768:  CMP  #$1F
L976A:  BCC  L976E                  ;IFCS
L976C:  LDA  #$1F
L976E:  STX  TBHD
L9770:  STA  TEMP2
L9772:  EOR  #$FF                   ;INVERT READING
L9774:  SEC
L9775:  ADC  CURSPO                 ;UPDATE CURSOR MASTER POSITION
L9777:  STA  TEMP3                  ;NEW CURSPO
L9779:  LDX  WELTYP
L977C:  BEQ  L979D                  ;IFNE  PLANAR SURFACE?
L977E:  CMP  #$F0                   ;YES.
L9780:  BCC  L9786                  ;IFCS  SPLIT CURSOR (WRAP)?
L9782:  LDA  #$EF                   ;YES. MOVE AWAY FROM EDGE
L9784:  STA  TEMP3
L9786:  EOR  TEMP2
L9788:  BPL  L979D                  ;IFMI
L978A:  LDA  TEMP3
L978C:  EOR  CURSPO
L978E:  BPL  L979D                  ;IFMI  WRAPPED AROUND?
L9790:  LDA  CURSPO                 ;YES.
L9792:  BMI  L9799                  ;IFPL  OLD POSITION LOW OR HI?
L9794:  LDA  #$00                   ;LOW END
L9796:  CLV                         ;ELSE
L9797:  BVC  L979B
L9799:  LDA  #$EF                   ;HIGH END
L979B:  STA  TEMP3                  ;NEW CURSPO
L979D:  LDA  TEMP3                  ;NEW CURSPO
L979F:  LSR
L97A0:  LSR
L97A1:  LSR
L97A2:  LSR
L97A3:  STA  TEMP1                  ;NEW CURSL1
L97A5:  CLC
L97A6:  ADC  #$01                   ;CCW ADJACENT LINE # FOR CURSOR IS 1 AWAY
L97A8:  AND  #$0F
L97AA:  STA  TEMP2                  ;NEW CURSL2
L97AC:  LDA  TEMP1
L97AE:  CMP  CURSL1                 ;[CS] Same tunnel segment as the player?
L97B1:  BEQ  L97B6                  ;IFNE  NEW POSITION?
L97B3:  JSR  SBOING                 ;YES. MAKE SOUND
L97B6:  LDA  TEMP1                  ;UPDATE CURSOR POSITION
L97B8:  STA  CURSL1
L97BB:  LDA  TEMP2
L97BD:  STA  CURSL2
L97C0:  LDA  TEMP3
L97C2:  STA  CURSPO
L97C4:  RTS

;------------------------------------------------------------------------------
; AUTOCU - PLAY-AUTO MOVE OF CURSOR
;------------------------------------------------------------------------------
AUTOCU:
L97C5:  LDA  #$FF
L97C7:  STA  TEMP0
L97C9:  STA  TEMP1
L97CB:  LDX  WINVMX                 ;[CS] Start with slot 6 of 0-6
L97CE:  LDA  INVAY,X                ;[CS] Is there an enemy here?
L97D1:  BEQ  L97DB                  ;IFNE  ALIVE?  [CS] If not, check for another.
L97D3:  CMP  TEMP0                  ;YES.
L97D5:  BCS  L97DB                  ;IFCC  HIGHEST?
L97D7:  STA  TEMP0                  ;YES.
L97D9:  STX  TEMP1
L97DB:  DEX
L97DC:  BPL  L97CE                  ;MIEND
L97DE:  LDX  TEMP1
L97E0:  BMI  L97F7                  ;IFPL
L97E2:  LDA  INVAL1,X
L97E5:  LDY  CURSL1
L97E8:  JSR  POLDEL                 ;HOW FAR & BEST DIRECTION?
L97EB:  TAY
L97EC:  BEQ  L97F7                  ;IFNE  ALREADY THERE?
L97EE:  BMI  L97F5                  ;IFPL  YES. WHICH WAY?
L97F0:  LDA  #$F7
L97F2:  CLV                         ;ELSE
L97F3:  BVC  L97F7
L97F5:  LDA  #$09
L97F7:  RTS

;------------------------------------------------------------------------------
; MOVCUD - PLAY-MOVE CURSOR DOWN
;------------------------------------------------------------------------------
MOVCUD:
L97F8:  LDA  CURSL2                 ;[CS] Is the player alive?
L97FB:  BPL  L97FE                  ;IFMI  [CS] If so, return.
L97FD:  RTS
;[CS] Start of ROM 136002.114 at $9800.
L97FE:  LDA  CURMOD
L9801:  BMI  L9804                  ;IFPL  CURSOR DROPPING?
L9803:  RTS                         ;NO
;YES.
L9804:  LDA  CURSY                  ;[CS] Get the player's Y position.
L9807:  CMP  #ILINLIY               ;[CS] Are they are the top of the tunnel?
L9809:  BNE  L980E                  ;IFEQ  STILL AT TOP?
L980B:  JSR  SOUTS2                 ;YES. START RUMBLE
L980E:  LDA  CURSYL                 ;UPDATE CURSOR DEPTH
L9811:  CLC
L9812:  ADC  CURSVL
L9815:  STA  CURSYL
L9818:  LDA  CURSY                  ;[CS] Get player's Y pos in the tunnel.
L981B:  ADC  CURSVH                 ;[CS] Move them downwards.
L981E:  STA  CURSY                  ;[CS] Store player's Y position.
L9821:  BCS  L9825                  ;IFCC
L9823:  CMP  #ILINDDY
L9825:  BCC  L9833                  ;IFCS  IS CURSOR PAST BOTTOM?
L9827:  LDA  #CENDWAV               ;YES. INITIALIZE SPACE MODE
L9829:  STA  QSTATE
L982B:  JSR  SOUTS3                 ;START SPACE SOUND
L982E:  LDA  #$FF                   ;[CS] Put player at the BOTTOM of tunnel.
L9830:  STA  CURSY
L9833:  LDA  CURSY                  ;[CS] Is the player above or below the
L9836:  CMP  #$50                   ;[CS] mid-point of the tunnel?
L9838:  BCC  L9842                  ;IFCS
L983A:  LDA  PLAGRO
L983D:  BNE  L9842                  ;IFEQ
L983F:  JSR  INSTAR
L9842:  LDA  EYLL                   ;UPDATE EYE POSITION
L9844:  CLC
L9845:  ADC  CURSVL
L9848:  STA  EYLL
L984A:  LDA  EYL
L984C:  ADC  CURSVH
L984F:  BCC  L9853                  ;IFCS
L9851:  INC  EYH
L9853:  CMP  EYL
L9855:  BEQ  L985A                  ;IFNE  EYE POSITION CHANGE?
L9857:  INC  ROTDIS                 ;YES. REQUEST NEW WELL DISPLAY
L985A:  STA  EYL
;CONSTANT ACCELERATION FOR VELOCITY
L985C:  LDA  CURWAV                 ;WAVE ACCELERATION +
L985E:  ASL
L985F:  ASL
L9860:  CMP  #$30
L9862:  BCC  L9866                  ;IFCS  MAX OUT
L9864:  LDA  #$30
L9866:  CLC
L9867:  ADC  #$20                   ;BASE ACCELERATION
L9869:  CLC
L986A:  ADC  CURSVL
L986D:  STA  CURSVL
L9870:  LDA  CURSVH
L9873:  ADC  #$00
L9875:  STA  CURSVH
;CHECK FOR COLLISION WITH ENEMY LINES
;[CS] Spike/Player collision detection.
L9878:  LDA  CURSY                  ;[CS] Is the player at the very
L987B:  CMP  #ILINDDY               ;[CS] bottom of the tunnel?
L987D:  BCS  L98A1                  ;IFCC  [CS] If so, ignore this crap. Return.
L987F:  LDX  #NLINES-1              ;CURSOR STILL ON LINES  [CS] For all tunnel segments...
L9881:  LDA  LINEY,X                ;[CS] Load the height of the spike.
L9884:  BEQ  L989E                  ;IFNE  ACTIVE LINE?  [CS] If no spike, check the next one.
L9886:  CPX  CURSL1                 ;YES.  [CS] Player on the same segment?
L9889:  BNE  L989E                  ;IFEQ  SAME LINE AS CURSOR?  [CS] If not, check next spike.
L988B:  CMP  CURSY                  ;YES.  [CS] Player ran into the spike?
L988E:  BCS  L989E                  ;IFCC  CURSOR AT ENEMY LINE POSITION?  [CS] If not, check next spike.
L9890:  JSR  PULSTO                 ;TURN OFF THRUST SOUND  [CS] PLAYER/SPIKE Collision Detected.
L9893:  JSR  INPPSQ                 ;YES. START BANG. KILL CURSOR
L9896:  LDA  #$00                   ;TURN OFF STARFIELD, EXIT LOOP
L9898:  STA  PLAGRO
L989B:  JSR  INICHA                 ;CLEAR OUT ALL CHARGES  [CS] Remove all bullets from play.
L989E:  DEX                         ;[CS] Check the next spike
L989F:  BPL  L9881                  ;MIEND
L98A1:  RTS                         ;[CS] All done.

;------------------------------------------------------------------------------
; MOVNYM - PLAY - MOVE NYMPHS
;------------------------------------------------------------------------------
MOVNYM:
L98A2:  LDY  #$00
L98A4:  STY  NEOFLI                 ;CLEAR NEW OFF LIMITS FLAGS
L98A7:  LDA  INMCOU                 ;[CS] Get # of enemies in the tube.
L98AA:  CLC                         ;[CS] Add with the # of enemies at
L98AB:  ADC  INCCOU                 ;[CS] the top of the tube.
L98AE:  CMP  WINVMX
L98B1:  BCC  L98B7                  ;IFCS  INVADER SLOTS BOOKED ALREADY?
L98B3:  BEQ  L98B7                  ;IFNE
L98B5:  LDY  #$FF                   ;YES.
L98B7:  LDA  SUZTIM                 ;AVOID KAMIKAZE
L98BA:  BEQ  L98BE                  ;IFNE
L98BC:  LDY  #$FF
L98BE:  STY  TEMPY                  ;ALLOW/DISALLOW UP NYMPH MOVEMENT
L98C0:  LDX  #NNYMPH-1              ;[CS] Go through all of the countdown
L98C2:  LDA  NYMPY,X                ;[CS] positions for an enemy to appear.
L98C5:  BEQ  L9919                  ;IFNE  ACTIVE?  [CS] If no enemy pending in this slot, skip to the next slot.
L98C7:  BIT  TEMPY                  ;YES.
L98C9:  BMI  L98EE                  ;IFPL  UP MOVEMENT OK?
L98CB:  SEC                         ;YES.
L98CC:  SBC  #$01
L98CE:  STA  NYMPY,X
L98D1:  BNE  L98D9                  ;IFEQ  UPDATE NYMPH POSITION. CONVERT?
L98D3:  JSR  CONYMP                 ;YES. MAKE IT AN INVADER
L98D6:  CLV                         ;ELSE
L98D7:  BVC  L98EE
L98D9:  CMP  #$3F                   ;NO.
L98DB:  BNE  L98EE                  ;IFEQ  JUST ENTERING ALONE ZONE?
L98DD:  LDY  NYMPL,X                ;YES.
L98E0:  LDA  NEOFLI
L98E3:  ORA  NEOFLI
L98E6:  AND  D70MSK,Y
L98E9:  BEQ  L98EE                  ;IFNE  ALREADY OCCUPIED?
L98EB:  INC  NYMPY,X                ;YES. BACK OFF
L98EE:  LDA  NYMPY,X
L98F1:  CMP  #$40
L98F3:  BCC  L9909                  ;IFCS  DON'T ROTATE PAST A CERTAIN PT.
L98F5:  LDA  QFRAME                 ;OK TO ROTATE
L98F7:  AND  #$01
L98F9:  BNE  L9906                  ;IFEQ  TIME TO ROTATE?
L98FB:  LDA  NYMPL,X                ;YES. ROTATE NYMPH
L98FE:  CLC
L98FF:  ADC  #$01
L9901:  AND  #$0F
L9903:  STA  NYMPL,X
L9906:  CLV                         ;ELSE
L9907:  BVC  L9919
L9909:  CMP  #$20                   ;NO ROTATE.
L990B:  BCC  L9919                  ;IFCS  IN ALONE ZONE?
L990D:  LDY  NYMPL,X                ;YES.
L9910:  LDA  D70MSK,Y               ;MARK LINE OFF LIMITS
L9913:  ORA  NEOFLI
L9916:  STA  NEOFLI
L9919:  DEX
L991A:  BPL  L98C2                  ;MIEND
L991C:  LDA  NEOFLI
L991F:  STA  OLOFLI                 ;NEW TO OLD OFF LIMITS
L9922:  RTS

;------------------------------------------------------------------------------
; CONYMP - PLAY - CONVERT NYMPH TO INVADER
;------------------------------------------------------------------------------
CONYMP:
L9923:  LDA  #ILINDDY               ;START AT BOTTOM
L9925:  STA  TEMP0
L9927:  LDA  NYMPL,X                ;START LINE
L992A:  STA  TEMP1
L992C:  STX  SAVEX
L992E:  JSR  NYMCHA                 ;NYMPH CHARACTERISTICS
L9931:  LDX  SAVEX
L9933:  LDA  TEMP0
L9935:  BEQ  L9945                  ;IFNE
L9937:  JSR  ACTINV                 ;ACTIVATE AN INVADER
L993A:  BEQ  L9945                  ;IFNE  SLOT FOUND?
L993C:  DEC  NYMCOU                 ;YES. DECREMENT NYMPH COUNT  [CS] One less enemy at the very bottom.
L993F:  LDA  #$00
L9941:  STA  NYMPY,X                ;DEACTIVATE INVADER
L9944:  RTS
L9945:  LDA  #$FF                   ;NO. STOP UP MOVEMENT FLAG
L9947:  STA  TEMPY
L9949:  INC  NYMPY,X                ;MOVE NYMPH BACK TO OLD POSITION
L994C:  RTS

;------------------------------------------------------------------------------
; ACTINV - PLAY - ACTIVATE INVADER
;   INPUT:TEMP0:Y POSITION AT WHICH TO START INVADER
;   TEMP2,3:CHARACTERISTICS OF NEW INVADER
;   TEMP1:CW LINE #
;   TEMP4:CAM VALUE
;   OUTPUT:IF A SLOT IS FOUND:INMCOU INCREMENTED
;   INVAC1,2(N) UPDATED WITH CHARACTERISTICS
;   INVAL1 =CW LINE #
;   INVAL2 =CCW LINE #
;   INVAY =Y POSITION
;   INVCAM =CAM PC
;   INVACT =0
;   STATUS FLAGS=0
;   IF NO SLOT IS FOUND:STATUS FLAGS=0
;   X,Y PRESERVED
;   SAVEY DESTROYED
;------------------------------------------------------------------------------
ACTINV:
L994D:  STY  SAVEY
L994F:  LDY  WINVMX                 ;[CS] Start with slot 6 (of 0-6)
L9952:  LDA  INVAY,Y                ;[CS] Is there an enemy in this lot?
L9955:  BNE  L999D                  ;IFEQ  SLOT?  [CS] If not, loop through next slot.
L9957:  LDA  TEMP0                  ;YES.
L9959:  STA  INVAY,Y                ;Y
L995C:  LDA  TEMP1
L995E:  CMP  #$0F
L9960:  BNE  L996C                  ;IFEQ  POTENTIAL PLANAR SPLIT?
L9962:  BIT  WELTYP                 ;YES
L9965:  BPL  L996C                  ;IFMI  PLANAR?
L9967:  LDA  RANDOM                 ;YES. NO SPLITS  [CS] Get a random number,
L996A:  AND  #$0E                   ;[CS] 0-E that is even.
L996C:  STA  INVAL1,Y               ;CW LINE
L996F:  CLC
L9970:  ADC  #$01
L9972:  AND  #$0F
L9974:  STA  INVAL2,Y               ;CCW LINE
L9977:  LDA  #$00
L9979:  STA  INVACT,Y               ;TIMER
L997C:  LDA  TEMP3
L997E:  STA  INVAC2,Y
L9981:  LDA  TEMP4
L9983:  STA  INVCAM,Y
L9986:  INC  INMCOU                 ;INVADER COUNT  [CS] One more enemy inside the tube.
L9989:  LDA  TEMP2                  ;[CS] Set their movement style based
L998B:  STA  INVAC1,Y               ;CHARACTERISTICS  [CS] on the passed parameter.
L998E:  LDY  SAVEY
L9990:  AND  #INVABI
L9992:  STX  SAVEY
L9994:  TAX
L9995:  INC  FLIPCO,X               ;UPDATE INVADER TYPE COUNTER  [CS] There is one more of some type of enemy in the tunnel.
L9998:  LDX  SAVEY                  ;RESTORE X
L999A:  LDA  #$10                   ;SOT FOUND FLAG
L999C:  RTS
L999D:  DEY
L999E:  BPL  L9952                  ;MIEND
L99A0:  LDY  SAVEY
L99A2:  LDA  #$00                   ;SLOT NOT FOUND FLAG
L99A4:  RTS

;------------------------------------------------------------------------------
; NYMCHA - PLAY - DETERMINE NYMPH TYPE
;------------------------------------------------------------------------------
NYMCHA:
L99A5:  LDA  #$00
L99A7:  LDX  #$04
L99A9:  STA  OPFLIP,X               ;0 ALL OPENING COUNTERS
L99AC:  DEX
L99AD:  BPL  L99A9                  ;MIEND
L99AF:  LDX  #$04                   ;[CS] Go through all the enemies.
L99B1:  LDA  WFLMAX,X               ;[CS] Find out the max # of this type of enemy allowed in the tunnel.
L99B4:  SEC                         ;[CS] There is one less of some type
L99B5:  SBC  FLIPCO,X               ;[CS] of enemy in the tunnel.
L99B8:  BCC  L99BD                  ;IFCS  MAX OF TYPE ALREADY?
L99BA:  STA  OPFLIP,X               ;NO SAVE # OPENINGS
L99BD:  DEX
L99BE:  BPL  L99B1                  ;MIEND
;TAKE AWAY Z OPENINGS OF TYPE FOR EACH TANKER
L99C0:  LDY  WINVMX
L99C3:  LDA  INVAY,Y
L99C6:  BEQ  L99DC                  ;IFNE  ALIVE?
L99C8:  LDA  INVAC2,Y               ;YES.
L99CB:  AND  #INVCAR                ;CARRIER?
L99CD:  BEQ  L99DC                  ;IFNE
L99CF:  TAX                         ;YES.
L99D0:  CPX  #ZCARFU
L99D2:  BNE  L99D6                  ;IFEQ
L99D4:  LDX  #ZABFUS+1
L99D6:  DEC  OPFLIP-1,X             ;2 LESS OPENINGS OF THAT TYPE
L99D9:  DEC  OPFLIP-1,X
L99DC:  DEY
L99DD:  BPL  L99C3                  ;MIEND
L99DF:  LDX  #$04
L99E1:  LDA  WINVMX
L99E4:  CLC
L99E5:  ADC  #$01
L99E7:  SEC                         ;[CS] There is one less of some type
L99E8:  SBC  FLIPCO,X               ;[CS] of enemy in the tunnel.
L99EB:  DEX
L99EC:  BPL  L99E7                  ;MIEND
L99EE:  LDX  #$04
L99F0:  CMP  OPFLIP,X
L99F3:  BCS  L99F8                  ;IFCC  IF TOTAL # OPENINGS <TYPE OPENINGS
L99F5:  STA  OPFLIP,X               ;THEN DECREASE TYPE OPENINGS
L99F8:  DEX
L99F9:  BPL  L99F0                  ;MIEND
L99FB:  LDX  #$04
L99FD:  LDY  #$00
L99FF:  LDA  OPFLIP,X
L9A02:  BEQ  L9A05                  ;IFNE
L9A04:  INY                         ;COUNT # TYPES WITH OPENINGS
L9A05:  DEX
L9A06:  BPL  L99FF                  ;MIEND
L9A08:  TYA
L9A09:  BEQ  L9A82                  ;IFNE  OPENING?
L9A0B:  DEY                         ;YES.
L9A0C:  BNE  L9A26                  ;IFEQ  ONLY 1 TYPE?
L9A0E:  LDX  #$04                   ;YES.
L9A10:  LDA  OPFLIP,X
L9A13:  BEQ  L9A20                  ;IFNE
L9A15:  LDA  WFLMIN,X               ;YES
L9A18:  BEQ  L9A20                  ;IFNE  LAUNCH OK?
L9A1A:  JSR  NEWTYP                 ;NO. TRY FOR TYPE
L9A1D:  BEQ  L9A20                  ;IFNE  GOT IT?
L9A1F:  RTS                         ;YES. EXIT
L9A20:  DEX
L9A21:  BPL  L9A10                  ;MIEND
L9A23:  CLV                         ;ELSE
L9A24:  BVC  L9A82
L9A26:  STY  SXL                    ;NO.
L9A28:  LDX  #$04
L9A2A:  LDA  OPFLIP,X
L9A2D:  BEQ  L9A3D                  ;IFNE  TYPE OPENINGS?
L9A2F:  LDA  FLIPCO,X               ;YES.  [CS] How many of this type of enemy is in the tunnel?
L9A32:  CMP  WFLMIN,X               ;[CS] Compare it to the ratio of this type of enemy that is allowed.
L9A35:  BCS  L9A3D                  ;IFCC  TYPE MIN SATISFIED?
L9A37:  JSR  NEWTYP                 ;NO. TRY FOR TYPE
L9A3A:  BEQ  L9A3D                  ;IFNE  GOT IT?
L9A3C:  RTS                         ;YES. EXIT
L9A3D:  DEX
L9A3E:  BPL  L9A2A                  ;MIEND
;MINS ARE OK.
L9A40:  LDA  OPSPIN                 ;TRY FOR SMART LAUNCH
L9A43:  BEQ  L9A61                  ;IFNE
L9A45:  LDA  OPTANK
L9A48:  BEQ  L9A61                  ;IFNE  SLOTS FOR TANKERS & SPINNER OPEN?
L9A4A:  LDY  TEMP1                  ;YES.
L9A4C:  LDA  LINEY,Y
L9A4F:  BNE  L9A53                  ;IFEQ  LINE DEAD?
L9A51:  LDA  #$FF                   ;YES. REAL SHORT THEN
L9A53:  LDX  #OPSPIN-OPFLIP         ;SHORT LINE:LAUNCH SPINNER
L9A55:  CMP  #$CC
L9A57:  BCS  L9A5B                  ;IFCC  LONG ENEMY LINE?
L9A59:  LDX  #OPTANK-OPFLIP         ;YES LAUNCH TANKER
L9A5B:  JSR  NEWTYP                 ;NO. TRY FOR TYPE
L9A5E:  BEQ  L9A61                  ;IFNE  GOT IT?
L9A60:  RTS                         ;YES. EXIT
L9A61:  LDA  RANDO2                 ;RANDOM TYPE (ELIM TYPE 0 THO)  [CS] Grab a random number,
L9A64:  AND  #$03                   ;[CS] 0-3.
L9A66:  TAX
L9A67:  INX
L9A68:  LDY  #$04                   ;START AT RANDOM SPOT AND
L9A6A:  LDA  WFLMIN,X
L9A6D:  BEQ  L9A7A                  ;IFNE  OK FROM BOTTOM (NOT 0)?
L9A6F:  LDA  OPFLIP,X               ;NO.
L9A72:  BEQ  L9A7A                  ;IFNE  NEEDY TYPE?
L9A74:  JSR  NEWTYP                 ;YES. TRY LAUNCH
L9A77:  BEQ  L9A7A                  ;IFNE  GOT IT?
L9A79:  RTS                         ;YES. EXIT
L9A7A:  DEX
L9A7B:  BPL  L9A7F                  ;IFMI
L9A7D:  LDX  #$04                   ;WRAP
L9A7F:  DEY
L9A80:  BPL  L9A6A                  ;MIEND
L9A82:  LDA  #$00                   ;SIGNAL FAILURE
L9A84:  STA  TEMP0
L9A86:  RTS

;------------------------------------------------------------------------------
; NEWTYP - NO. TRY FOR TYPE  (comment at the calls)
;   YES. TRY LAUNCH  (another call)
;------------------------------------------------------------------------------
NEWTYP:
L9A87:  TXA

;------------------------------------------------------------------------------
; NEWTY2 - NO. ASSIGN NORMAL PARAMETERS  (comment at the call)
;------------------------------------------------------------------------------
NEWTY2:
L9A88:  ASL
L9A89:  TAY
L9A8A:  LDA  NYMTAD+1,Y
L9A8D:  PHA
L9A8E:  LDA  NYMTAD,Y
L9A91:  PHA
L9A92:  RTS

NYMTAD:
;[CS] Start of a data segment, per Ken Lui. Length unknown.
;[CS] Will not label as data until more is known.
L9A93:  .word NEWFLI-1
L9A95:  .word NEWPUL-1
L9A97:  .word NEWTAN-1
L9A99:  .word NEWSPI-1
L9A9B:  .word NEWFUS-1

;------------------------------------------------------------------------------
; NEWFLI - [note] NYMCHA type vector for a flipper: TEMP3 = TNEWI2+ZABFLI, A = WFLICAM CAM start, Y = ZABFLI;
;   continues at NEWGN3.
;------------------------------------------------------------------------------
NEWFLI:
L9A9D:  LDA  TNEWI2+ZABFLI
L9AA0:  STA  TEMP3                  ;INVAC2
L9AA2:  LDA  WFLICAM
L9AA5:  LDY  #ZABFLI                ;FLIPPER (INVAC1)
L9AA7:  BEQ  NEWGN3                 ;ALWAYS

;------------------------------------------------------------------------------
; NEWPUL - PULSAR
;------------------------------------------------------------------------------
NEWPUL:
L9AA9:  LDA  TNEWI2+ZABPUL
L9AAC:  ORA  WPULFI                 ;PULSAR FIRE?
L9AAF:  LDY  #ZABPUL                ;PULSAR (INVAC1)
L9AB1:  BNE  NEWGN2                 ;ALWAYS

;------------------------------------------------------------------------------
; NEWFUS - FUSE
;------------------------------------------------------------------------------
NEWFUS:
L9AB3:  LDY  #ZABFUS
L9AB5:  BNE  NEWGEN

;------------------------------------------------------------------------------
; NEWSPI - SPINNER
;------------------------------------------------------------------------------
NEWSPI:
L9AB7:  LDY  #ZABTRA
L9AB9:  BNE  NEWGEN

;------------------------------------------------------------------------------
; NEWTAN - TANKER
;------------------------------------------------------------------------------
NEWTAN:
L9ABB:  LDA  RANDOM                 ;[CS] Get a random number,
L9ABE:  AND  #$03                   ;[CS] 0-3.
L9AC0:  TAY
L9AC1:  LDA  #$04
L9AC3:  STA  TEMP2
L9AC5:  STX  INDEX3                 ;SAVE X
L9AC7:  DEC  TEMP2
L9AC9:  BPL  L9AD0                  ;IFMI  FAILURE FOR ALL?
L9ACB:  LDX  INDEX3                 ;YES. RESTORE X
L9ACD:  LDA  #$00                   ;SIGNAL FAILURE
L9ACF:  RTS
L9AD0:  DEY
L9AD1:  BPL  L9AD5                  ;IFMI  CYCLE BETWEEN 0+3
L9AD3:  LDY  #$03
L9AD5:  LDX  WTACAR,Y               ;GET TYPE OF TANKER
L9AD8:  CPX  #ZCARFU
L9ADA:  BNE  L9ADE                  ;IFEQ
L9ADC:  LDX  #ZABFUS+1
L9ADE:  LDA  OPFLIP-1,X
L9AE1:  BEQ  L9AC7                  ;NEEND  EXIT IF OPENINGS FOR TYPE
L9AE3:  LDX  INDEX3                 ;RESTORE X
L9AE5:  LDA  WTACAR,Y               ;GET TANKER CONTENTS
L9AE8:  ORA  #ZFIRYE
L9AEA:  LDY  #ZABTAN
L9AEC:  BNE  NEWGN2

;------------------------------------------------------------------------------
; NEWGEN - [note] NYMCHA common exit for enemy type Y: TEMP3 = TNEWI2,Y, CAM start TNEWCAM,Y;
;   then TEMP2 = type, TEMP4 = CAM, A = TEMP0 (success flag).
;------------------------------------------------------------------------------
NEWGEN:
L9AEE:  LDA  TNEWI2,Y

NEWGN2:
L9AF1:  STA  TEMP3
L9AF3:  LDA  TNEWCAM,Y

NEWGN3:
L9AF6:  STY  TEMP2                  ;GENERAL
L9AF8:  STA  TEMP4
L9AFA:  LDA  TEMP0                  ;SUCCESS SIGNAL
L9AFC:  RTS

TNEWCAM:
;  TNEWCAM: initial CAM per enemy type (flipper, pulsar, tanker, spiker, fuseball)
;    entries: NOJUMP, PULSCH, NOJUMP, TRALUP, FUSEUP
L9AFD:  .byte NOJUMP-CAM, PULSCH-CAM, NOJUMP-CAM
L9B00:  .byte TRALUP-CAM, FUSEUP-CAM

TNEWI2:
L9B02:  .byte ZCARNO|ZFIRYE|ZDIRUP
L9B03:  .byte ZCARNO|ZFIRNO|ZDIRUP
L9B04:  .byte ZCARFL|ZFIRYE|ZDIRUP
L9B05:  .byte ZCARNO|ZFIRYE|ZDIRUP
L9B06:  .byte ZCARNO|ZFIRNO|ZDIRUP

;------------------------------------------------------------------------------
; SPLCHA - PLAY - DETERMINE SPLIT INVADER CHARACTERISTICS
;   INPUT: Y=INVADER INDEX
;   TEMP2=INVABI TYPE CODE ;TEMP0=SPLIT DEPTH
;------------------------------------------------------------------------------
SPLCHA:
L9B07:  STY  SAVEY
L9B09:  LDA  TEMP0
L9B0B:  CMP  #$20
L9B0D:  LDA  TEMP2
L9B0F:  BCS  L9B18                  ;IFCC  SPLITTING TOO CLOSE TO PLAYER?
L9B11:  TAY                         ;YES. NO FLIPPING
L9B12:  JSR  NEWGEN
L9B15:  CLV                         ;ELSE
L9B16:  BVC  L9B1B
L9B18:  JSR  NEWTY2                 ;NO. ASSIGN NORMAL PARAMETERS
L9B1B:  LDY  SAVEY
L9B1D:  RTS

;------------------------------------------------------------------------------
; MOVINV - PLAY - MOVE INVADERS (MAINLINE)
;------------------------------------------------------------------------------
MOVINV:
L9B1E:  LDA  CURSL2
L9B21:  BMI  L9B56                  ;IFPL  PLAYER DEAD OR DROPPING?
;YES. EXIT
L9B23:  LDX  WINVMX
L9B26:  STX  INDEX1
L9B28:  LDX  INDEX1
L9B2A:  LDA  INVAY,X
L9B2D:  BEQ  L9B52                  ;IFNE  ACTIVE?
L9B2F:  LDA  #$01                   ;SET NO EXIT FLAG
L9B31:  STA  EXICAM
L9B34:  LDA  INVCAM,X               ;SET UP INVADER'S CAM PC
L9B37:  STA  CAMPC
L9B3A:  LDA  CAMPC
L9B3D:  TAY                         ;GET INTO INTO CAM TABLE
L9B3E:  LDA  CAM,Y                  ;GET CAM CODE
L9B41:  JSR  JSRCAM                 ;EXECUTE CAM REQUESTED
L9B44:  INC  CAMPC                  ;AUTO INCREMENT CAM PC
L9B47:  LDA  EXICAM                 ;EXIT REQUESTED?
L9B4A:  BNE  L9B3A                  ;EQEND
L9B4C:  LDA  CAMPC                  ;UPDATE INVADER'S CAM PC
L9B4F:  STA  INVCAM,X
L9B52:  DEC  INDEX1
L9B54:  BPL  L9B28                  ;MIEND
;UPDATE PULSE STATUS
L9B56:  LDA  PULSON
L9B59:  CLC
L9B5A:  ADC  PULTIM
L9B5D:  TAY
L9B5E:  EOR  PULSON
L9B61:  STY  PULSON
L9B64:  BPL  L9B7C                  ;IFMI  PULSAR STATUS CHANGE?
L9B66:  TYA                         ;YES.
L9B67:  BPL  L9B6F                  ;IFMI  GO OFF?
L9B69:  JSR  PULSTO                 ;YES. TURN OFF
L9B6C:  CLV                         ;ELSE
L9B6D:  BVC  L9B7C
L9B6F:  LDA  FLIPCO+ZABPUL          ;NO. TURN ON IF ACTIVE PULSARS
L9B72:  BEQ  L9B7C                  ;IFNE
L9B74:  LDA  CURSL2
L9B77:  BMI  L9B7C                  ;IFPL
L9B79:  JSR  PULSTR                 ;ACTIVE SO TURN ON
L9B7C:  LDA  PULSON
L9B7F:  BMI  L9B88                  ;IFPL  BONUCE BETWEEN-27. AND +15.
L9B81:  CMP  #$0F
L9B83:  BCS  NEGPUL
L9B85:  CLV                         ;ELSE
L9B86:  BVC  L9B97
L9B88:  CMP  #$C1
L9B8A:  BCS  L9B97                  ;IFCC

NEGPUL:
L9B8C:  LDA  PULTIM                 ;NEGATE INCREMENT
L9B8F:  EOR  #$FF
L9B91:  CLC
L9B92:  ADC  #$01
L9B94:  STA  PULTIM
L9B97:  RTS

;------------------------------------------------------------------------------
; JSRCAM - PLAY - INVADERS - CAM DISPATCHER
;------------------------------------------------------------------------------
JSRCAM:
L9B98:  TAY                         ;JSR INDIRECT TO CAM ROUTINE
L9B99:  LDA  TABJSR+1,Y
L9B9C:  PHA
L9B9D:  LDA  TABJSR,Y
L9BA0:  PHA
L9BA1:  RTS

;.SBTTL CAM TABLE SUBROUTINE POINTERS

TABJSR:
;  TABJSR: CAM opcode handlers, .WORD handler-1 (dispatched with RTS), index = opcode/2:
;    $00 VEXIT   End this enemy's processing for the frame (JEXIT clears EXICAM)
;    $02 VSLOOP  Set loop counter INVLOO(X) = operand
;    $04 VSKIP0  If CAMSTA = 0, skip the next 2-byte instruction
;    $06 VSETPC  Go to label
;    $08 VELOOP  Decrement INVLOO(X); if not zero go to label, else continue
;    $0A VNOOP   No operation
;    $0C VSMOVE  Move one step along the lane (up, or down when INVAC2 says so); at the top convert to chaser (TOPPER)
;    $0E VSTRAI  Trailer (spiker) processing: lay/extend the spike on this line; CAMSTA=0 converts to carrier
;    $10 VSLOPB  Set loop counter INVLOO(X) = contents of zero-page operand
;    $12 VJUMPS  Start a jump (flip) to the adjacent lane in the current rotation direction
;    $14 VJUMPM  Continue the jump one angle step; CAMSTA = 0 when the flip is finished
;    $16 VCHROT  Reverse the jump (rotation) direction (INVAC1 ^= INVROT)
;    $18 VKITST  Top chaser: kill the player if on the same lane legs (INIPSQ)
;    $1A VBR0PC  If CAMSTA = 0 go to label
;    $1C VELTST  CAMSTA = 0 if the enemy is on an enemy (spike) line, else 1
;    $1E VSFUSE  Fuseball up/down motion along the lane (JFUSEUP)
;    $20 VFUSKI  Fuseball kills the player if at the same height and lane
;    $22 VSPUMO  Pulsar move (faster outside the power zone; reverses at PULPOT)
;    $24 VCHPLA  Set jump direction the shortest way toward the player
;    $26 VCHKPU  CAMSTA = $80 if the pulsars are pulsing now or within 4 frames, else 0
;[CS] Data Segment, per Ken Lui.
L9BA2:  .word JEXIT-1               ;CAMAC JEXIT,VEXIT,0
L9BA4:  .word JSLOOP-1              ;CAMA2I JSLOOP,VSLOOP,2
L9BA6:  .word JSKIP0-1              ;CAMAC JSKIP0,VSKIP0,4
L9BA8:  .word JSETPC-1              ;CAMA2F JSETPC,VSETPC,6
L9BAA:  .word JELOOP-1              ;CAMA2F JELOOP,VELOOP,8
L9BAC:  .word JNOOP-1               ;CAMAC JNOOP,VNOOP,0A
L9BAE:  .word JSMOVE-1              ;CAMAC JSMOVE,VSMOVE,0C
L9BB0:  .word JSTRAI-1              ;CAMAC JSTRAI,VSTRAI,0E
L9BB2:  .word JSLOPB-1              ;CAMA2I JSLOPB,VSLOPB,10
L9BB4:  .word JJUMPS-1              ;CAMAC JJUMPS,VJUMPS,12
L9BB6:  .word JJUMPM-1              ;CAMAC JJUMPM,VJUMPM,14
L9BB8:  .word JCHROT-1              ;CAMAC JCHROT,VCHROT,16
L9BBA:  .word JKITST-1              ;CAMAC JKITST,VKITST,18
L9BBC:  .word JBR0PC-1              ;CAMA2F JBR0PC,VBR0PC,1A
L9BBE:  .word JELTST-1              ;CAMAC JELTST,VELTST,1C
L9BC0:  .word JFUSEUP-1             ;CAMAC JFUSEUP,VSFUSE,1E
L9BC2:  .word JFUSKI-1              ;CAMAC JFUSKI,VFUSKI,20
L9BC4:  .word JPULMO-1              ;CAMAC JPULMO,VSPUMO,22
L9BC6:  .word JCHPLA-1              ;CAMAC JCHPLA,VCHPLA,24
L9BC8:  .word JCHKPU-1              ;CAMAC JCHKPU,VCHKPU,26

;------------------------------------------------------------------------------
; TABJSE - PLAY - INVADERS - CAM SUBROUTINES
;   EXIT CAM
;   (also: JEXIT)
;------------------------------------------------------------------------------
TABJSE:
JEXIT:
L9BCA:  LDA  #$00                   ;EXIT CAM FLAG
L9BCC:  STA  EXICAM

;------------------------------------------------------------------------------
; JNOOP - [note] CAM opcode VNOOP: does nothing (RTS).
;------------------------------------------------------------------------------
JNOOP:
L9BCF:  RTS

;------------------------------------------------------------------------------
; JSLOOP - SET CAM LOOP COUNTER
;------------------------------------------------------------------------------
JSLOOP:
L9BD0:  INC  CAMPC
L9BD3:  LDY  CAMPC
L9BD6:  LDA  CAM,Y
L9BD9:  STA  INVLOO,X               ;NEW LOOP VALUE
L9BDC:  RTS

;------------------------------------------------------------------------------
; JSLOPB - SAME AS JSLOOP EXCEPT OPERAND=BP ADDRESS OF PARAMETER
;------------------------------------------------------------------------------
JSLOPB:
L9BDD:  INC  CAMPC
L9BE0:  LDY  CAMPC
L9BE3:  LDA  CAM,Y
L9BE6:  TAY                         ;BP LOC OF VALUE
L9BE7:  .byte $B9, $00, $00         ;LDA $0000,Y (forced absolute)  NEW LOOP VALUE
L9BEA:  STA  INVLOO,X
L9BED:  RTS

;------------------------------------------------------------------------------
; JSKIP0 - SKIP NEXT CAM LINE IF CAMSTA=0
;   (DOUBLE BYTE)
;------------------------------------------------------------------------------
JSKIP0:
L9BEE:  LDA  CAMSTA
L9BF1:  BNE  L9BF9                  ;IFEQ
L9BF3:  INC  CAMPC
L9BF6:  INC  CAMPC
L9BF9:  RTS

;------------------------------------------------------------------------------
; JBR0PC - BRANCH IF CAMSTA=0
;------------------------------------------------------------------------------
JBR0PC:
L9BFA:  INC  CAMPC
L9BFD:  LDA  CAMSTA
L9C00:  BNE  L9C0B                  ;IFEQ  BRANCH?
L9C02:  LDY  CAMPC                  ;YES. SET NEW PC
L9C05:  LDA  CAM,Y
L9C08:  STA  CAMPC
L9C0B:  RTS

;------------------------------------------------------------------------------
; JELOOP - DEC LOOP VALUE
;   IF 0 THEN EXIT
;   ELSE RELOOP
;------------------------------------------------------------------------------
JELOOP:
L9C0C:  DEC  INVLOO,X
L9C0F:  BNE  JSETPC                 ;IFEQ
L9C11:  INC  CAMPC                  ;EXIT LOOP
L9C14:  CLV                         ;ELSE
L9C15:  BVC  L9C20

;------------------------------------------------------------------------------
; JSETPC - [note] CAM opcode VSETPC: CAMPC = operand byte (jump inside the CAM script area).
;------------------------------------------------------------------------------
JSETPC:
L9C17:  LDY  CAMPC                  ;NEW CAM PC
L9C1A:  LDA  CAM+1,Y                ;RELOOP
L9C1D:  STA  CAMPC
L9C20:  RTS

;------------------------------------------------------------------------------
; JELTST - [note] CAM opcode VELTST: CAMSTA = 0 if INVAY(X) is above the enemy-line height LINEY of the
;   enemy's lane (source: 'enemy on an enemy line?'), else CAMSTA = 1.
;------------------------------------------------------------------------------
JELTST:
L9C21:  LDY  INVAL1,X
L9C24:  LDA  LINEY,Y
L9C27:  BNE  L9C2B                  ;IFEQ
L9C29:  LDA  #$FF                   ;WORST CASE LINE (DEAD)
L9C2B:  CMP  INVAY,X
L9C2E:  BCS  L9C35                  ;IFCC  ENEMY ON AN ENEMY LINE?
L9C30:  LDA  #$00                   ;YES.
L9C32:  CLV                         ;ELSE
L9C33:  BVC  L9C37
L9C35:  LDA  #$01                   ;NO.
L9C37:  STA  CAMSTA
L9C3A:  RTS

;------------------------------------------------------------------------------
; JCHKPU - PLAY - INVADERS - CAM ROUTINES
;   CHECK FOR PULSING NOW OR IN NEXT 4 FRAMES
;------------------------------------------------------------------------------
JCHKPU:
L9C3B:  LDA  PULTIM
L9C3E:  ASL
L9C3F:  ASL
L9C40:  CLC
L9C41:  ADC  PULSON
L9C44:  AND  PULSON
L9C47:  AND  #$80
L9C49:  EOR  #$80
L9C4B:  STA  CAMSTA                 ;EXIT: 0=NO PULSE ;80=PULSE
L9C4E:  RTS

;------------------------------------------------------------------------------
; JCHROT - CHANGE DIRECTION OF JUMP
;------------------------------------------------------------------------------
JCHROT:
L9C4F:  LDA  INVAC1,X               ;[CS] Get enemy movement style and
L9C52:  EOR  #INVROT                ;[CS] set it to ??????
L9C54:  STA  INVAC1,X
L9C57:  RTS

;------------------------------------------------------------------------------
; JSMOVE - PLAY - MOVE INVADERS (MOVE 1 UP)
;   INPUT: X=INVADER INDEX
;------------------------------------------------------------------------------
JSMOVE:
L9C58:  LDA  INVAC1,X               ;[CS] Get enemy movement style and
L9C5B:  AND  #INVABI                ;[CS] set it to upwards movement only.
L9C5D:  TAY                         ;INVADER TYPE
L9C5E:  LDA  INVAC2,X
L9C61:  BMI  JSMOVD                 ;IFPL  GOING UP?

;------------------------------------------------------------------------------
; JSMOVU - MOVE UP  (comment at the call)
;------------------------------------------------------------------------------
JSMOVU:
L9C63:  LDA  INVAYL,X               ;YES.
L9C66:  CLC
L9C67:  ADC  WINVIL,Y
L9C6A:  STA  INVAYL,X               ;MOVE UP
L9C6D:  LDA  INVAY,X                ;[CS] Get the position of a tunnel enemy,
L9C70:  ADC  WINVIN,Y               ;[CS] and move them *UP* by their
L9C73:  STA  INVAY,X                ;[CS] pretermined movement rate.
L9C76:  CMP  CURSY                  ;[CS] Enemy at the top of the tunnel?
L9C79:  BEQ  ATOP
L9C7B:  BCS  L9C83                  ;IFCC  AT TOP?

ATOP:
L9C7D:  JSR  CHASER                 ;YES. CONVERT TO CHASER
L9C80:  CLV                         ;ELSE
L9C81:  BVC  L9C96
L9C83:  CMP  #$20                   ;NO
L9C85:  BCS  L9C96                  ;IFCC  TOO CLOSE TO TOP FOR CARRIER?
L9C87:  LDA  INVAC2,X               ;YES.
L9C8A:  AND  #INVCAR                ;CARRIER?
L9C8C:  BEQ  L9C96                  ;IFNE
L9C8E:  TXA                         ;YES.
L9C8F:  PHA                         ;SAVE X
L9C90:  TAY
L9C91:  JSR  KILINV                 ;SPLIT CARRIER
L9C94:  PLA
L9C95:  TAX                         ;RESTORE X
L9C96:  CLV                         ;ELSE
L9C97:  BVC  L9CB5

;------------------------------------------------------------------------------
; JSMOVD - MOVE DOWN (RETURN WITH ACC=Y POS)  (comment at the call)
;   MOVE DOWN  (another call)
;------------------------------------------------------------------------------
JSMOVD:
L9C99:  LDA  INVAYL,X               ;DOWN
L9C9C:  SEC
L9C9D:  SBC  WINVIL,Y
L9CA0:  STA  INVAYL,X
L9CA3:  LDA  INVAY,X                ;[CS] Get the Y pos of an enemy,
L9CA6:  SBC  WINVIN,Y               ;[CS] and move them *UP* by their
L9CA9:  STA  INVAY,X                ;[CS] predetermined movement rate.
L9CAC:  CMP  #ILINDDY
L9CAE:  BCC  L9CB5                  ;IFCS  AT BOTTOM?
L9CB0:  LDA  #$F2
L9CB2:  STA  INVAY,X                ;YES.
L9CB5:  RTS

;------------------------------------------------------------------------------
; JPULMO - PLAY - INVADERS (PULSE MOVE)
;------------------------------------------------------------------------------
JPULMO:
L9CB6:  LDY  #ZABPUL
L9CB8:  LDA  INVAC2,X
L9CBB:  BMI  L9CCD                  ;IFPL  GOING UP?
L9CBD:  LDA  INVAY,X                ;YES.
L9CC0:  CMP  PULPOT
L9CC3:  BCC  L9CC7                  ;IFCS  IN POWER ZONE?
L9CC5:  LDY  #ZABFLI                ;NO. GO FASTER
L9CC7:  JSR  JSMOVU                 ;MOVE UP
L9CCA:  CLV                         ;ELSE
L9CCB:  BVC  L9CE4
L9CCD:  JSR  JSMOVD                 ;MOVE DOWN (RETURN WITH ACC=Y POS)
L9CD0:  LDY  NYMCOU                 ;[CS] Are the enemies waiting at the
L9CD3:  BNE  L9CD7                  ;IFEQ  NYMPHS GONE?
L9CD5:  LDA  #$FF                   ;SEND PULSAR UP
L9CD7:  CMP  PULPOT
L9CDA:  BCC  L9CE4                  ;IFCS  TIME TO REVERSE?
L9CDC:  LDA  INVAC2,X               ;YES
L9CDF:  EOR  #INVDIR
L9CE1:  STA  INVAC2,X
L9CE4:  LDA  PULSON                 ;YES. SEE IF CURSOR GOT ZAPPED
L9CE7:  BMI  L9D04                  ;IFPL  PULSAR ON?
L9CE9:  LDA  INVAY,X                ;YES.
L9CEC:  CMP  PULPOT
L9CEF:  BCS  L9D04                  ;IFCC  PULSAR IN RANGE?
L9CF1:  LDA  CURSL1                 ;YES
L9CF4:  CMP  INVAL1,X
L9CF7:  BNE  L9D04                  ;IFEQ
L9CF9:  LDA  CURSL2
L9CFC:  CMP  INVAL2,X
L9CFF:  BNE  L9D04                  ;IFEQ  ON CURSOR LINES?
L9D01:  JSR  INPPSQ                 ;YES. KILL CURSOR
L9D04:  RTS

CHKSM3:
L9D05:  .byte QCHKS3

;------------------------------------------------------------------------------
; CHASER - PLAY - INVADERS (CONVERT TO CHASER)
;   INPUT: X=INVADER=INDEX
;------------------------------------------------------------------------------
CHASER:
L9D06:  LDA  CURSY                  ;PLACE EXACTLY AT TOP
L9D09:  STA  INVAY,X
L9D0C:  LDA  INVAC1,X               ;[CS] Look at this enemy. Is it a
L9D0F:  AND  #INVABI                ;[CS] Pulsar?
L9D11:  CMP  #ZABPUL
L9D13:  BNE  L9D23                  ;IFEQ  PULSAR?
L9D15:  LDA  NYMCOU                 ;YES.  [CS] If no more enemies waiting at the
L9D18:  BEQ  L9D23                  ;IFNE  ANY MORE NYMPHS?
L9D1A:  LDA  INVAC2,X               ;YES. SEND IT DOWN
L9D1D:  EOR  #INVDIR
L9D1F:  STA  INVAC2,X
L9D22:  RTS                         ;EXIT
L9D23:  LDA  INVAC1,X
L9D26:  BPL  L9D2C                  ;IFMI  STILL FLIPPING 2
L9D28:  INC  INVAY,X                ;YES. FINISH FLIP  [CS] Move this enemy upwards.
L9D2B:  RTS                         ;BEFORE AT TOP STATUS
L9D2C:  DEC  INMCOU                 ;-1 TO # WALL INVADERS  [CS] One less enemy INSIDE the tube.
L9D2F:  LDA  INCCOU                 ;[CS] Are the any enemies at the
L9D32:  CMP  #$01                   ;[CS] top of the tube?
L9D34:  BEQ  L9D3C                  ;IFNE  OTHER THAN 1 CHASER?
L9D36:  JSR  JCHPLA                 ;YES. SEND CHASER SHORTEST WAY
L9D39:  CLV                         ;ELSE
L9D3A:  BVC  L9D5E
;NO. 1 OTHER CHASER, SO SEND
L9D3C:  LDY  #NINVAD-1              ;THIS GUY IN OPPOSITE DIRECTION
L9D3E:  LDA  INVAY,Y
L9D41:  BEQ  L9D51                  ;IFNE
L9D43:  STY  INDEX2
L9D45:  CPX  INDEX2
L9D47:  BEQ  L9D51                  ;IFNE  MAKE SURE IT'S NOT NEW CHASER
L9D49:  LDA  INVAY,Y                ;[CS] Get the Y position of an enemy.
L9D4C:  CMP  CURSY                  ;[CS] Are they at the top of the tunnel?
L9D4F:  BEQ  GOTCHA                 ;EXIT LOOP IF FOUND
L9D51:  DEY
L9D52:  BPL  L9D3E                  ;MIEND

GOTCHA:
L9D54:  LDA  INVAC1,Y               ;[CS] If they are at the top,
L9D57:  AND  #INVROT                ;GET OTHER CHASER'S DIRECTION  [CS] change their movement style
L9D59:  EOR  #INVROT                ;USE ITS OPPOSITS  [CS] to ____.
;SET CHASE DIRECTION
L9D5B:  STA  INVAC1,X
L9D5E:  LDA  #<[TOPPER-CAM-1]
L9D60:  STA  CAMPC                  ;SET CHASER CAM
L9D63:  INC  INCCOU                 ;+1 TO CHASER COUNT  [CS] One more enemy at the tube top.
L9D66:  RTS

;------------------------------------------------------------------------------
; JCHPLA - YES. SEND CHASER SHORTEST WAY  (comment at the call)
;   CHASE PLAYER  (another call)
;------------------------------------------------------------------------------
JCHPLA:
L9D67:  LDA  INVAL1,X               ;SEND CHASER SHORTEST WAY
L9D6A:  TAY
L9D6B:  LDA  CURSL1
L9D6E:  JSR  POLDEL                 ;DETERMINE POLAR DELTA TO CURSOR
L9D71:  ASL
L9D72:  LDA  INVAC1,X               ;[CS] Load enemy type and movement.
L9D75:  BCS  L9D7C                  ;IFCC  SET CHASE DIRECTION= SHORTEST WAY
L9D77:  ORA  #INVROT                ;CCW
L9D79:  CLV                         ;ELSE
L9D7A:  BVC  L9D7E
L9D7C:  AND  #<[~INVROT]            ;CW
L9D7E:  STA  INVAC1,X               ;[CS] Save enemy type and movement.
L9D81:  RTS

;------------------------------------------------------------------------------
; JJUMPM - PLAY - MOVE INVADERS (PROCESS JUMP)
;   UPDATE JUMP ANGLE
;------------------------------------------------------------------------------
JJUMPM:
L9D82:  LDY  INVAL2,X
L9D85:  LDA  INVAC1,X               ;[CS] Load enemy type and movement.
L9D88:  AND  #INVROT
L9D8A:  BNE  L9D90                  ;IFEQ  MOVING
L9D8C:  INY                         ;CW (JUMP ROTATION CCW)
L9D8D:  CLV                         ;ELSE
L9D8E:  BVC  L9D91
L9D90:  DEY                         ;CCW (JUMP ROTATION CW)
L9D91:  TYA                         ;NEW JUMP ANGLE
L9D92:  AND  #$0F                   ;MOD 16
L9D94:  ORA  #$80                   ;JUMP CODE
L9D96:  STA  INVAL2,X               ;UPDATED JUMP ANGLE
L9D99:  LDA  INVAC1,X               ;[CS] Load enemy type and movement.
L9D9C:  AND  #INVABI
L9D9E:  CMP  #ZABFUS                ;FUSE AT A JUNCTION (IFEQ)?
L9DA0:  BNE  L9DEE                  ;IFEQ
L9DA2:  LDA  INVAL2,X               ;MAYBE.
L9DA5:  AND  #$07
L9DA7:  BNE  L9DEB                  ;IFEQ  AT A JUNCTION?
L9DA9:  LDA  INVAL2,X               ;YES
L9DAC:  AND  #$08
L9DAE:  BEQ  L9DBB                  ;IFNE  MOVING CCW?
L9DB0:  LDA  INVAL1,X               ;YES. ADJUST BASE
L9DB3:  CLC
L9DB4:  ADC  #$01
L9DB6:  AND  #$0F
L9DB8:  STA  INVAL1,X
L9DBB:  LDA  INVAC1,X               ;YES  [CS] Load enemy type and movement.
L9DBE:  AND  #<[~INVMOT]            ;[CS] Strip away any clockwise movement.
L9DC0:  STA  INVAC1,X               ;SET STATUS BACK TO LINE  [CS] Store enemy type and movement.
L9DC3:  LDA  #$20
L9DC5:  STA  INVAL2,X               ;MAKE IT INVINCIBLE
L9DC8:  LDA  INVAC2,X
L9DCB:  EOR  #INVDIR
L9DCD:  STA  INVAC2,X               ;REVERSE UP DOWN DIRECTION
L9DD0:  LDA  NYMCOU                 ;[CS] If no more enemies waiting at the
L9DD3:  BNE  L9DEB                  ;IFEQ  NYMPHS GONE?  [CS] bottom, skip ahead.
L9DD5:  LDA  INVAY,X                ;YES  [CS] Is the curent enemy at the
L9DD8:  CMP  CURSY                  ;[CS] very top?
L9DDB:  BNE  L9DE3                  ;IFEQ  AT TOP?
L9DDD:  JSR  FUCHPL                 ;YES. STAY THERE & CHASE PLAYER
L9DE0:  CLV                         ;ELSE
L9DE1:  BVC  L9DEB
L9DE3:  LDA  INVAC2,X               ;NO. SEND UP.  [CS] Stop vertical movement for
L9DE6:  AND  #INVDIR                ;[CS] this particular enemy? (Because
L9DE8:  STA  INVAC2,X               ;[CS] they are at the top.)
L9DEB:  CLV                         ;ELSE
L9DEC:  BVC  L9E26
;CALCULATE FINAL JUMP ANGLE
L9DEE:  LDY  INVAL1,X
L9DF1:  LDA  INVAC1,X               ;[CS] Load enemy type and movement.
L9DF4:  EOR  #INVROT                ;BACKWARDS
L9DF6:  JSR  CALSAN
L9DF9:  CMP  INVAL2,X
L9DFC:  BNE  L9E26                  ;IFEQ  FINAL JUMP ANGLE=UPDATED ANGLE?
L9DFE:  LDA  INVAC1,X               ;YES  [CS] Load enemy type and movement.
L9E01:  AND  #<[~INVMOT]            ;[CS] Strip away clockwise movement.
L9E03:  STA  INVAC1,X               ;SET STATUS BACK TO MOVER  [CS] Store enemy type and movement.
L9E06:  AND  #INVROT
L9E08:  BNE  L9E1B                  ;IFEQ  NEW LINE IN WHICH DIRECTION?
L9E0A:  LDA  INVAL1,X               ;CW
L9E0D:  STA  INVAL2,X
L9E10:  SEC
L9E11:  SBC  #$01
L9E13:  AND  #$0F
L9E15:  STA  INVAL1,X
L9E18:  CLV                         ;ELSE
L9E19:  BVC  L9E26
L9E1B:  LDA  INVAL1,X               ;CCW
L9E1E:  CLC
L9E1F:  ADC  #$01
L9E21:  AND  #$0F
L9E23:  STA  INVAL2,X
L9E26:  LDA  INVAC1,X               ;[CS] Load enemy type and movement.
L9E29:  AND  #INVMOT                ;RETURN WITH STATUS (0=JUMP DONE)
L9E2B:  STA  CAMSTA                 ;SET CAM STATUS
L9E2E:  RTS

;------------------------------------------------------------------------------
; JKITST - PLAY - MOVE INVADERS (CHASE PLAYER)
;------------------------------------------------------------------------------
JKITST:
L9E2F:  LDA  INVAC1,X               ;[CS] Load enemy type and movement.
L9E32:  BMI  L9E47                  ;IFPL  MOVING (NOT JUMPING)
L9E34:  LDA  INVAL1,X               ;YES
L9E37:  CMP  CURSL1
L9E3A:  BNE  L9E47                  ;IFEQ  IS ANY INVADER LEG ON SAME LINE
L9E3C:  LDA  INVAL2,X               ;AS ANY CURSOR LEG?
L9E3F:  CMP  CURSL2
L9E42:  BNE  L9E47                  ;IFEQ
L9E44:  JSR  INIPSQ                 ;YES. DESTROY CURSOR
L9E47:  RTS

;------------------------------------------------------------------------------
; JFUSKI - [note] CAM opcode VFUSKI: if the fuseball is at the cursor's height (CURSY) and line (CURSL1),
;   kill the player (INFPSQ).  Source: CHECK FOR FUSE KILL CURSOR.
;------------------------------------------------------------------------------
JFUSKI:
L9E48:  LDA  INVAY,X                ;CHECK FOR FUSE KILL CURSOR
L9E4B:  CMP  CURSY
L9E4E:  BNE  L9E5B                  ;IFEQ  SAME HEIGHT?
L9E50:  LDA  INVAL1,X               ;YES.
L9E53:  CMP  CURSL1
L9E56:  BNE  L9E5B                  ;IFEQ  SAME LINE?
L9E58:  JSR  INFPSQ                 ;YES. DEAD CURSOR NOW
L9E5B:  RTS

;------------------------------------------------------------------------------
; JJUMPS - PLAY - MOVE INVADERS (START A JUMP)
;   INPUT: INVACO(X) BIT IJDIRE
;   ACC BIT IJDIRE=JUMP DIRECTION (JUMPSD ENTRY ONLY)
;   OUTPUT:INVACO(X) BIT IMOVER
;   INVAL1(X) SET TO BASE LEG
;   INVAL2(X) SET TO JUMP SEQ START
;------------------------------------------------------------------------------
JJUMPS:
L9E5C:  JSR  OKTOJM                 ;VERIFY JUMP DIRECTION

JUMPSD:
L9E5F:  LDA  INVAC1,X               ;[CS] Load enemy type and movement.
L9E62:  ORA  #ZMOTJM                ;[CS] Set clockwise movement.
L9E64:  STA  INVAC1,X               ;SET JUMPS STATUS  [CS] Store enemy type and movement.
L9E67:  AND  #INVABI                ;[CS] Is this enemy a
L9E69:  CMP  #ZABFUS                ;[CS] fuseball?
L9E6B:  BNE  L9E8C                  ;IFEQ  FUSE?
L9E6D:  LDA  INVAC1,X               ;YES.  [CS] Load enemy type and movement.
L9E70:  AND  #INVROT
L9E72:  BNE  L9E79                  ;IFEQ  WHICH WAY?
L9E74:  LDA  #$81                   ;CCW
L9E76:  CLV                         ;ELSE
L9E77:  BVC  L9E86
L9E79:  LDA  INVAL1,X               ;CW
L9E7C:  SEC
L9E7D:  SBC  #$01
L9E7F:  AND  #$0F
L9E81:  STA  INVAL1,X
L9E84:  LDA  #$87
L9E86:  STA  INVAL2,X
L9E89:  CLV                         ;ELSE
L9E8A:  BVC  L9EAA
L9E8C:  LDA  INVAC1,X               ;NO  [CS] Load enemy type and movement.
L9E8F:  AND  #INVROT
L9E91:  BEQ  L9E9E                  ;IFNE  MOVING CCW?
L9E93:  LDA  INVAL1,X               ;YES. ADJUST BASE LEG
L9E96:  CLC
L9E97:  ADC  #$01
L9E99:  AND  #$0F
L9E9B:  STA  INVAL1,X
L9E9E:  LDA  INVAC1,X               ;NO.  [CS] Load enemy type and movement.
L9EA1:  LDY  INVAL1,X
L9EA4:  JSR  CALSAN                 ;CALC. STARTING ANGLE
L9EA7:  STA  INVAL2,X
L9EAA:  RTS

;------------------------------------------------------------------------------
; OKTOJM - VERIFY JUMP DIRECTION  (comment at the call)
;------------------------------------------------------------------------------
OKTOJM:
L9EAB:  LDA  WELTYP
L9EAE:  BEQ  L9ED6                  ;IFNE  PLANAR SURFACE?
L9EB0:  LDA  INVAC1,X               ;YES  [CS] Load enemy type and movement.
L9EB3:  AND  #INVROT
L9EB5:  BEQ  L9EC9                  ;IFNE  MOVING CCW?
L9EB7:  LDA  INVAL1,X               ;CCW
L9EBA:  CMP  #$0E
L9EBC:  BCC  L9EC6                  ;IFCS  AT RIGHT EDGE?
L9EBE:  LDA  INVAC1,X               ;YES CHANGE TO CW JUMP  [CS] Load enemy type and movement.
L9EC1:  AND  #<[~INVROT]
L9EC3:  STA  INVAC1,X               ;[CS] Store enemy type and movement.
L9EC6:  CLV                         ;ELSE
L9EC7:  BVC  L9ED6
L9EC9:  LDA  INVAL1,X               ;CW
L9ECC:  BNE  L9ED6                  ;IFEQ  AT LEFT EDGE?
L9ECE:  LDA  INVAC1,X               ;YES CHANGE TO CCW JUMPS
L9ED1:  ORA  #INVROT
L9ED3:  STA  INVAC1,X
L9ED6:  RTS

;------------------------------------------------------------------------------
; CALSAN - CALCULATE STARTING JUMP ANGLE
;   BASE LEG IN Y
;------------------------------------------------------------------------------
CALSAN:
L9ED7:  AND  #INVROT
L9ED9:  BEQ  L9EEB                  ;IFNE  MOVING CCW?
L9EDB:  DEY                         ;YES.
L9EDC:  TYA
L9EDD:  AND  #$0F
L9EDF:  TAY
L9EE0:  LDA  LINANG,Y
L9EE3:  CLC                         ;YES. ADJUST ANGLE FOR BASE LEG ON
L9EE4:  ADC  #$08                   ;RIGHT SIDE
L9EE6:  AND  #$0F                   ;MOD 16
L9EE8:  CLV                         ;ELSE
L9EE9:  BVC  L9EEE
L9EEB:  LDA  LINANG,Y               ;CW
L9EEE:  ORA  #$80                   ;JUMP CODE
L9EF0:  RTS

;------------------------------------------------------------------------------
; JFUSEUP - PLAY-INVADER FUSE UP/DOWN MOTION
;------------------------------------------------------------------------------
JFUSEUP:
L9EF1:  LDY  #ZABFUS
L9EF3:  LDA  INVAC2,X
L9EF6:  BMI  L9F43                  ;IFPL  UP OR DOWN?
L9EF8:  LDA  INVAYL,X               ;UP.
L9EFB:  CLC
L9EFC:  ADC  WFUSIL
L9EFF:  STA  INVAYL,X
L9F02:  LDA  INVAY,X
L9F05:  ADC  WFUSIH
L9F08:  STA  INVAY,X
L9F0B:  CMP  CURSY
L9F0E:  BCS  L9F19                  ;IFCC  AT TOP?
L9F10:  LDA  CURSY                  ;YES
L9F13:  STA  INVAY,X
L9F16:  CLV                         ;ELSE
L9F17:  BVC  L9F2A
L9F19:  LDY  NYMCOU                 ;NO  [CS] If no more enemies waiting at
L9F1C:  BEQ  L9F29                  ;IFNE  NYMPHS LEFT?  [CS] the bottom, skip ahead.
L9F1E:  LDY  CURWAV                 ;YES.  [CS] Are we at or beyond the RED
L9F20:  CPY  #$11                   ;[CS] LEVEL, just after the green?
L9F22:  BCS  L9F26                  ;IFCC  EARLY WAVE?
L9F24:  CMP  #$20                   ;YES. TURN BACK BEFORE TOP
L9F26:  CLV                         ;ELSE
L9F27:  BVC  L9F2A
L9F29:  RTS                         ;NONE LEFT. HEAD FOR TOP
L9F2A:  BCS  L9F3D                  ;IFCC  TOO HIGH?
L9F2C:  LDA  WFUSCH                 ;YES.
L9F2F:  BPL  L9F37                  ;IFMI  CHASE PLAYER AT TOP?
L9F31:  JSR  FUCHPL                 ;YES. CHASE
L9F34:  CLV                         ;ELSE
L9F35:  BVC  L9F3A
L9F37:  JSR  LEFRIT                 ;NO. RANDOM
L9F3A:  CLV                         ;ELSE
L9F3B:  BVC  L9F40
L9F3D:  JSR  MAYBLR                 ;NO. MAYBE GO LEFT OR RIGHT ANYWAY
L9F40:  CLV                         ;ELSE
L9F41:  BVC  L9F5E
L9F43:  JSR  JSMOVD                 ;MOVE DOWN
L9F46:  CMP  #$80
L9F48:  BCC  L9F5B                  ;IFCS  AT BOTTOM OF RANGE?
L9F4A:  BIT  WFUSCH                 ;YES.
L9F4D:  BVC  L9F55                  ;IFVS  CHASE PLAYER ON TUBE?
L9F4F:  JSR  FUCHPL                 ;YES. CHASE
L9F52:  CLV                         ;ELSE
L9F53:  BVC  L9F58
L9F55:  JSR  LEFRIT                 ;NO. RANDOM
L9F58:  CLV                         ;ELSE
L9F59:  BVC  L9F5E
L9F5B:  JSR  MAYBLR                 ;NO. MAYBE GO LEFT OR RIGHT
L9F5E:  RTS

;------------------------------------------------------------------------------
; MAYBLR - INVADER FUSE JUMP DECISION
;------------------------------------------------------------------------------
MAYBLR:
L9F5F:  LDA  INVAY,X
L9F62:  AND  #$20
L9F64:  BEQ  L9F80                  ;IFNE
L9F66:  LDA  RANDO2                 ;[CS] Grab a random number
L9F69:  CMP  WFUFRQ
L9F6C:  BCC  L9F80                  ;IFCS
L9F6E:  BIT  WFUSCH
L9F71:  BVC  L9F7D                  ;IFVS  CHASE PLAYERS ON TUBE?
L9F73:  TXA                         ;YES. ONLY IF INDEX IS EVEN
L9F74:  LSR
L9F75:  BCC  LEFRIT
L9F77:  JSR  FUCHPL                 ;YES. CHASE
L9F7A:  CLV                         ;ELSE
L9F7B:  BVC  L9F80
L9F7D:  JSR  LEFRIT                 ;NO. RANDOM
L9F80:  RTS

;------------------------------------------------------------------------------
; FUCHPL - INVADER FUSE LEFT/RIGHT VECTOR
;------------------------------------------------------------------------------
FUCHPL:
L9F81:  JSR  JCHPLA                 ;CHASE PLAYER
L9F84:  JSR  JCHROT                 ;REVERSE DIRECTION (FUSE IS BACKWARDS)
L9F87:  JMP  GOTJUM

;------------------------------------------------------------------------------
; LEFRIT - NO. RANDOM  (comment at the calls)
;------------------------------------------------------------------------------
LEFRIT:
L9F8A:  LDA  INVAC1,X               ;RANDOMLY CHOOSE LEFT OR RIGHT
L9F8D:  AND  #<[~INVROT]
L9F8F:  BIT  RANDOM
L9F92:  BVC  L9F96                  ;IFVS
L9F94:  ORA  #INVROT
L9F96:  STA  INVAC1,X

GOTJUM:
L9F99:  LDA  WELTYP
L9F9C:  BEQ  L9FBC                  ;IFNE  PLANAR SURFACE?
L9F9E:  LDA  INVAC1,X               ;YES.
L9FA1:  AND  #INVROT
L9FA3:  BNE  L9FAF                  ;IFEQ  GOING CCW?
L9FA5:  LDA  INVAL1,X               ;YES.
L9FA8:  CMP  #$0F
L9FAA:  BCS  REVFLP                 ;AT RIGHT EDGE?
L9FAC:  CLV                         ;ELSE  NO.
L9FAD:  BVC  L9FBC
L9FAF:  LDA  INVAL1,X               ;NO. GOING CW
L9FB2:  BNE  L9FBC                  ;IFEQ  AT LEFT EDGE?

REVFLP:
L9FB4:  LDA  INVAC1,X               ;YES. GO BACK
L9FB7:  EOR  #INVROT
L9FB9:  STA  INVAC1,X
L9FBC:  LDA  #<[FUSELR-CAM]
L9FBE:  STA  CAMPC                  ;PT TO LEFT RIGHT FUSE CAM
L9FC1:  JMP  JUMPSD                 ;GO START JUMP

;------------------------------------------------------------------------------
; JSTRAI - PLAY - INVADERS -TRAILER
;   SPECIAL TRAILER PROCESSING
;------------------------------------------------------------------------------
JSTRAI:
L9FC4:  LDA  #$01
L9FC6:  STA  CAMSTA
L9FC9:  LDY  INVAL1,X
L9FCC:  LDA  LINEY,Y
L9FCF:  BNE  L9FD6                  ;IFEQ  LINE VACANT?
L9FD1:  LDA  #ILINDDY+1             ;YES. START LOW. 2
L9FD3:  STA  LINEY,Y
L9FD6:  LDA  INVAY,X
L9FD9:  CMP  LINEY,Y
L9FDC:  BCS  L9FE6                  ;IFCC  NEW ENEMY LINE?
L9FDE:  STA  LINEY,Y                ;YES.
L9FE1:  LDA  #$80
L9FE3:  STA  LINSTA,Y               ;REQUEST RECALC.
L9FE6:  LDA  INVAY,X
L9FE9:  CMP  #$20
L9FEB:  BCS  L9FFD                  ;IFCC  MAX HEIGHT?
L9FED:  LDA  INVAC2,X               ;YES.
L9FF0:  ORA  #ZDIRDO                ;SEND IT DOWN
L9FF2:  STA  INVAC2,X
L9FF5:  LDA  #$20                   ;MAX HEIGHT
L9FF7:  STA  INVAY,X
L9FFA:  CLV                         ;ELSE
L9FFB:  BVC  LA027
L9FFD:  CMP  #$F2                   ;NO.
;[CS] Start of ROM 136002.115 at $A000.
L9FFF:  BCC  LA027                  ;IFCS  MIN HEIGHT?
LA001:  JSR  ASTRAL                 ;YES. REASSIGN, REVERSE
LA004:  LDA  #$F0                   ;DON'T LET IT GET TO LOW
LA006:  STA  INVAY,X
LA009:  LDA  NYMCOU                 ;ANY NYMPHS, OR NON SPIKER TYPE CLIMBERS?  [CS] If enemies waiting at the bottom,
LA00C:  BNE  LA027                  ;IFEQ  [CS] skip ahead.
LA00E:  LDA  INVAC2,X
LA011:  AND  #<[~INVCAR]            ;CONVERT IT TO TANKER
LA013:  ORA  #ZCARFL                ;CARRYING FLIPPERS
LA015:  STA  INVAC2,X
LA018:  LDA  INVAC1,X               ;LOOKS LIKE TANKER TOO
LA01B:  AND  #<[~INVABI]
LA01D:  ORA  #ZABTAN
LA01F:  STA  INVAC1,X
LA022:  LDA  #$00                   ;SET ZERO STATUS (CONVERTED TOO CARRIER)
LA024:  STA  CAMSTA
LA027:  RTS

;------------------------------------------------------------------------------
; ASTRAL - YES. REASSIGN, REVERSE  (comment at the call)
;------------------------------------------------------------------------------
ASTRAL:
LA028:  LDA  #$00
LA02A:  STA  TEMP4
LA02C:  LDA  #NLINES-1              ;LOOP LINE COUNTER
LA02E:  STA  OPSPIN
LA031:  LDA  RANDO2                 ;START AT A RANDOM LINE  [CS] Grab a random number,
LA034:  AND  #$0F                   ;[CS] 0-F.
LA036:  TAY
LA037:  CPY  #$0F
LA039:  BNE  LA040                  ;IFEQ
LA03B:  LDA  WELTYP
LA03E:  BNE  SKIPIT                 ;SKIP LINE IF PLANAR FAR RIGHT EDGE
LA040:  LDA  LINEY,Y
LA043:  BNE  LA047                  ;IFEQ  DEAD LINE?
LA045:  LDA  #$FF                   ;YES. WORST CASE
LA047:  CMP  TEMP4
LA049:  BCC  SKIPIT                 ;IFCS  NEEDIEST LINE SO FAR?
LA04B:  STA  TEMP4                  ;YES. CONDITION
LA04D:  STY  TEMP0                  ;LINE #

SKIPIT:
LA04F:  DEY
LA050:  BPL  LA054                  ;IFMI
LA052:  LDY  #NLINES-1
LA054:  DEC  OPSPIN
LA057:  BPL  LA037                  ;MIEND
LA059:  LDA  TEMP0                  ;REASSIGN TO NEW LINE
LA05B:  STA  INVAL1,X
LA05E:  CLC
LA05F:  ADC  #$01
LA061:  AND  #$0F
LA063:  STA  INVAL2,X
LA066:  LDA  INVAC2,X               ;SEND BACK UP
LA069:  AND  #<[~INVDIR]
LA06B:  STA  INVAC2,X
LA06E:  RTS

;------------------------------------------------------------------------------
; KILINV - PLAY - KILL INVADER
;   INPUT:Y=INVADER TO BE SPLIT
;   OUTPUT:ORIGINAL KILLED OFF
;   UP TO 2 NEW ONES CREATED
;   X IS PRESERVED
;------------------------------------------------------------------------------
KILINV:
LA06F:  LDA  INVAY,Y                ;SAVE Y
LA072:  STA  TEMP0
LA074:  CMP  CURSY
LA077:  BNE  MOVER                  ;IFEQ  DECREMENT COUNTER
LA079:  LDA  INVAC1,Y
LA07C:  AND  #INVABI
LA07E:  CMP  #ZABFUS
LA080:  BEQ  MOVER                  ;FUSE (BRANCH IF FUSE) OR CHASE
LA082:  DEC  INCCOU                 ;CHASER  [CS] One less enemy at the tube top.
LA085:  CLV                         ;ELSE
LA086:  BVC  LA08B

MOVER:
LA088:  DEC  INMCOU                 ;MOVER  [CS] One less enemy INSIDE the tube.
LA08B:  LDA  #$00                   ;DEACTIVATE ENEMY
LA08D:  STA  INVAY,Y
LA090:  LDA  INVAC1,Y
LA093:  AND  #INVABI
LA095:  STX  SAVEX
LA097:  TAX
LA098:  DEC  FLIPCO,X               ;UPDATE TYPE COUNTER  [CS] One less of this type of enemy is in the tunnel.
LA09B:  LDX  SAVEX
LA09D:  LDA  INVAC2,Y
LA0A0:  AND  #INVCAR
LA0A2:  BEQ  LA0F6                  ;IFNE  SPLIT TYPE INVADER?
LA0A4:  SEC                         ;YES
LA0A5:  SBC  #$01
LA0A7:  CMP  #ZABTAN
LA0A9:  BNE  LA0AD                  ;IFEQ  TANKER?
LA0AB:  LDA  #ZABFUS                ;YES. REALLY FUSE
LA0AD:  STA  TEMP2                  ;RESULTANT MUTATION
LA0AF:  LDA  INVAL1,Y               ;YES.
LA0B2:  SEC
LA0B3:  SBC  #$01
LA0B5:  AND  #$0F
LA0B7:  CMP  #$0F                   ;DON'T ALLOW WRAPAROUND ON PLANE
LA0B9:  BCC  LA0C2                  ;IFCS
LA0BB:  BIT  WELTYP
LA0BE:  BPL  LA0C2                  ;IFMI
LA0C0:  LDA  #$00
LA0C2:  STA  TEMP1                  ;LINE # CW
;Y
LA0C4:  JSR  SPLCHA                 ;CHARACTERISTICS
LA0C7:  LDA  TEMP4                  ;JUST IN CASE THE DEAD
LA0C9:  STA  CAMPC                  ;SLOT GETS USED
LA0CC:  DEC  CAMPC
LA0CF:  LDA  #$00                   ;SET EXIT FLAG
LA0D1:  STA  EXICAM
LA0D4:  JSR  ACTINV                 ;ACTIVATE AN INVADER
LA0D7:  BEQ  LA0F6                  ;IFNE  ANY SLOTS?
LA0D9:  LDA  TEMP1                  ;YES
LA0DB:  CLC
LA0DC:  ADC  #$02
LA0DE:  AND  #$0F
LA0E0:  CMP  #$0F
LA0E2:  BNE  LA0EB                  ;IFEQ  DON'T ALLOW WRAP AROUND ON PLANE
LA0E4:  BIT  WELTYP
LA0E7:  BPL  LA0EB                  ;IFMI
LA0E9:  LDA  #$0E
LA0EB:  STA  TEMP1                  ;LINE #CCW
LA0ED:  LDA  TEMP2
LA0EF:  ORA  #ZROCCW
LA0F1:  STA  TEMP2
LA0F3:  JSR  ACTINV                 ;ACTIVATE ANOTHER INVADER
LA0F6:  RTS

;.SBTTL PLAY - INVADER CAM TABLES

CAM:
TRALUP:
;  CAM enemy-motion scripts $A0F7-$A18F: one opcode byte (+ operand), interpreted by
;  JSRCAM through TABJSR; branch operands are target-CAM-1. MOVINV: per active enemy, CAMPC=INVCAM(X); loop {op=CAM[CAMPC]; JSRCAM; CAMPC++} until EXICAM=0; INVCAM(X)=CAMPC
;  script TRALUP (7 bytes): Trailer (spiker) moving up, laying a spike; converts to a carrier
;TRAILER MOVING UP
LA0F7:  .byte $0C                   ;VSMOVE  MOVE UP
LA0F8:  .byte $0E                   ;VSTRAI  PROCESS TRALER
LA0F9:  .byte $1A, NOJUMP-CAM-1     ;VBR0PC NOJUMP  CONVERT TO CARRIER
LA0FB:  .byte $00                   ;VEXIT  EXIT
LA0FC:  .byte $06, <[TRALUP-CAM-1]  ;VSETPC TRALUP  RELOOP

NOJUMP:
;  script NOJUMP (4 bytes): Move up, never flip
;MOVING UP (NO JUMPS)
LA0FE:  .byte $0C                   ;VSMOVE  MOVE UP
LA0FF:  .byte $00                   ;VEXIT
LA100:  .byte $06, NOJUMP-CAM-1     ;VSETPC NOJUMP  RELOOP

MOVJMP:
;  script MOVJMP (14 bytes): Move up 8 frames, then flip once, repeat
;MOVE 3 TIMES, THEN JUMP
LA102:  .byte $02, $08              ;VSLOOP 8

MJLOP1:
LA104:  .byte $0C                   ;VSMOVE  MOVE UP N FRAMES
LA105:  .byte $00                   ;VEXIT
LA106:  .byte $08, MJLOP1-CAM-1     ;VELOOP MJLOP1
LA108:  .byte $12                   ;VJUMPS  START JUMP

MJLOP5:
LA109:  .byte $00                   ;VEXIT
LA10A:  .byte $14                   ;VJUMPM  PROCESS JUMP
LA10B:  .byte $04                   ;VSKIP0  SKIP IF JUMP IS DONE
LA10C:  .byte $06, MJLOP5-CAM-1     ;VSETPC MJLOP5
LA10E:  .byte $06, MOVJMP-CAM-1     ;VSETPC MOVJMP  JUMP IS DONE. RESTART SEQUENCE

SPIRAL:
;  script SPIRAL (11 bytes): Smooth upward spiral: flip continuously while moving up
;SMOOTH UPWARD SPIRAL
LA110:  .byte $0C                   ;VSMOVE
LA111:  .byte $00                   ;VEXIT
LA112:  .byte $12                   ;VJUMPS  START JUMP

SPILOP:
LA113:  .byte $00                   ;VEXIT
LA114:  .byte $14                   ;VJUMPM  PROCESS JUMP
LA115:  .byte $0C                   ;VSMOVE  MOVE UP
LA116:  .byte $04                   ;VSKIP0
LA117:  .byte $06, SPILOP-CAM-1     ;VSETPC SPILOP
LA119:  .byte $06, SPIRAL-CAM-1     ;VSETPC SPIRAL  RESTART JUMP WHEN FINISHED

SPIRCH:
;  script SPIRCH (30 bytes): Spiral, changing flip direction after 2 then 3 flips
;CHANGE JUMP DIRECTION EVERY N JUMPS
LA11B:  .byte $0C                   ;VSMOVE
LA11C:  .byte $00                   ;VEXIT
LA11D:  .byte $02, $02              ;VSLOOP 2  LOOP FOR N JUMPS

SPRLP1:
LA11F:  .byte $12                   ;VJUMPS  START JUMP

SPRLP2:
LA120:  .byte $00                   ;VEXIT
LA121:  .byte $14                   ;VJUMPM  CONTINUE JUMP
LA122:  .byte $0C                   ;VSMOVE  MOVE UP
LA123:  .byte $04                   ;VSKIP0  JUMP DONE?
LA124:  .byte $06, SPRLP2-CAM-1     ;VSETPC SPRLP2  NO. CONTINUE JUMP
LA126:  .byte $00                   ;VEXIT
LA127:  .byte $08, SPRLP1-CAM-1     ;VELOOP SPRLP1  YES. NEW JUMP OR EXIT
LA129:  .byte $16                   ;VCHROT  CHANGE JUMP DIRECTION
LA12A:  .byte $02, $03              ;VSLOOP 3  LOOP FOR N JUMPS

SPRLP3:
LA12C:  .byte $12                   ;VJUMPS  START JUMP

SPRLP4:
LA12D:  .byte $00                   ;VEXIT
LA12E:  .byte $14                   ;VJUMPM  CONTINUE JUMP
LA12F:  .byte $0C                   ;VSMOVE  MOVE UP
LA130:  .byte $04                   ;VSKIP0  JUMP DONE?
LA131:  .byte $06, SPRLP4-CAM-1     ;VSETPC SPRLP4  NO. CONT JUMP
LA133:  .byte $00                   ;VEXIT
LA134:  .byte $08, SPRLP3-CAM-1     ;VELOOP SPRLP3  YES. NEW JUMP OR EXIT
LA136:  .byte $16                   ;VCHROT
LA137:  .byte $06, SPIRCH-CAM-1     ;VSETPC SPIRCH  START OVER

TOPPER:
;  script TOPPER (17 bytes): Chaser on the rim: wait 4 frames testing for a kill, then flip toward the player (WTTFRA steps/frame)
;CHASE PLAYER AROUND TOP
LA139:  .byte $02, $04              ;VSLOOP 4  WAIT IN CROUCH FOR N FRAMES

KICHEK:
LA13B:  .byte $18                   ;VKITST  TEST FOR CURSOR KILL
LA13C:  .byte $00                   ;VEXIT
LA13D:  .byte $08, KICHEK-CAM-1     ;VELOOP KICHEK
LA13F:  .byte $12                   ;VJUMPS  START A JUMP

KJULP1:
LA140:  .byte $00                   ;VEXIT
LA141:  .byte $10, WTTFRA           ;VSLOPB WTTFRA

KJULP2:
LA143:  .byte $14                   ;VJUMPM  DOUBLE SPEED JUMP
LA144:  .byte $1A, TOPPER-CAM-1     ;VBR0PC TOPPER  SKIP IF JUMP IS DONE
LA146:  .byte $08, KJULP2-CAM-1     ;VELOOP KJULP2
LA148:  .byte $06, KJULP1-CAM-1     ;VSETPC KJULP1

COWJM2:
;  script COWJM2 (14 bytes): Flip and move on open lanes, just move up while on an enemy (spike) line
;ENEMY FLIPS & MOVES ON OPEN LINES, MOVES ON ENEMY LINES
LA14A:  .byte $00                   ;VEXIT

COWJMP:
LA14B:  .byte $0C                   ;VSMOVE  MOVE ENEMY
LA14C:  .byte $1C                   ;VELTST  ON AN ENEMY LINE?
LA14D:  .byte $1A, COWJM2-CAM-1     ;VBR0PC COWJM2  YES. CONTINUE UP ON LINE
LA14F:  .byte $12                   ;VJUMPS  NO. START A JUMP
LA150:  .byte $00                   ;VEXIT
LA151:  .byte $0C                   ;VSMOVE  MOVE UP

COWJM3:
LA152:  .byte $14                   ;VJUMPM  PROCESS JUMP
LA153:  .byte $1A, COWJM2-CAM-1     ;VBR0PC COWJM2  JUMP DONE
LA155:  .byte $00                   ;VEXIT
LA156:  .byte $06, COWJM3-CAM-1     ;VSETPC COWJM3  CONTINUE JUMP

FUSEUP:
;  script FUSEUP (5 bytes): Fuseball moving up/down a lane, killing the player on contact
;PULSAR
;FUSE UP/DOWN
LA158:  .byte $1E                   ;VSFUSE  PROCESS FUSE
LA159:  .byte $20                   ;VFUSKI  FUSE KILL CURSOR
LA15A:  .byte $00                   ;VEXIT  EXIT
LA15B:  .byte $06, FUSEUP-CAM-1     ;VSETPC FUSEUP  RELOOP

FUSELR:
;  script FUSELR (12 bytes): Fuseball moving left/right between lanes (3-frame delay per step)
LA15D:  .byte $00                   ;VEXIT  FUSE LEFT/RIGHT
LA15E:  .byte $02, $03              ;VSLOOP 3  SLOWL

FUSLOP:
LA160:  .byte $20                   ;VFUSKI  CURSOR KILLED?
LA161:  .byte $00                   ;VEXIT
LA162:  .byte $08, FUSLOP-CAM-1     ;VELOOP FUSLOP
LA164:  .byte $14                   ;VJUMPM  LEFT/RIGHT
LA165:  .byte $1A, FUSEUP-CAM-1     ;VBR0PC FUSEUP  JUMP DONE?
LA167:  .byte $06, FUSELR-CAM-1     ;VSETPC FUSELR  NO. CONTINUE JUMP

PULSCH:
PULSCP:          ;PULSAR CHASER PLAYER
;  script PULSCH (21 bytes): Pulsar chasing the player: move PUCHDE frames, wait out a pulse, flip toward player
LA169:  .byte $10, PUCHDE           ;VSLOPB PUCHDE

PULSC1:
LA16B:  .byte $22                   ;VSPUMO  MOVE 1/8 OF TUBE BEFORE NEXT FLIP
LA16C:  .byte $00                   ;VEXIT
LA16D:  .byte $08, PULSC1-CAM-1     ;VELOOP PULSC1

PULSC2:
LA16F:  .byte $26                   ;VCHKPU  PULSING?
LA170:  .byte $1A, PULSC3-CAM-1     ;VBR0PC PULSC3  BRANCH IF NOT
LA172:  .byte $22                   ;VSPUMO  PULSING, SO KEEP MOVING
LA173:  .byte $00                   ;VEXIT
LA174:  .byte $06, PULSC2-CAM-1     ;VSETPC PULSC2  RECHECK FOR PULSE

PULSC3:
LA176:  .byte $24                   ;VCHPLA  SET FLIP DIRECTION TOWARD PLAYER
LA177:  .byte $12                   ;VJUMPS  START FLIP

PULSCJ:
LA178:  .byte $00                   ;VEXIT
LA179:  .byte $14                   ;VJUMPM  CONTINUE FLIP
LA17A:  .byte $1A, PULSCP-CAM-1     ;VBR0PC PULSCP  DONE?
LA17C:  .byte $06, PULSCJ-CAM-1     ;VSETPC PULSCJ  NO

AVOIDR:
;  script AVOIDR (17 bytes): Avoider flipper: flip away from the player while moving, then move up 4 frames
;AVOIDANCE FLIPPER
LA17E:  .byte $24                   ;VCHPLA  SET DIRECTION TOWARD PLAYER
LA17F:  .byte $16                   ;VCHROT  REVERSE IT
LA180:  .byte $12                   ;VJUMPS

AVOID1:
LA181:  .byte $00                   ;VEXIT  FLIP PROCESSING LOOP
LA182:  .byte $0C                   ;VSMOVE
LA183:  .byte $14                   ;VJUMPM
LA184:  .byte $04                   ;VSKIP0
LA185:  .byte $06, AVOID1-CAM-1     ;VSETPC AVOID1
LA187:  .byte $02, $04              ;VSLOOP 4.

AVOID2:
LA189:  .byte $00                   ;VEXIT  FLIP DONE. MOVE UP LOOP
LA18A:  .byte $0C                   ;VSMOVE
LA18B:  .byte $08, AVOID2-CAM-1     ;VELOOP AVOID2
LA18D:  .byte $06, AVOIDR-CAM-1     ;VSETPC AVOIDR

;------------------------------------------------------------------------------
; MOVCHA - PLAY - MOVE CHARGES
;------------------------------------------------------------------------------
MOVCHA:
LA18F:  LDX  #NPCHARG+NICHARG-1
LA191:  STX  INDEX1
;[CS] Beginning of loop
;[CS] RE-EVALUATE THIS. IT APPEARS TO HANDLE BOTH PLAYER AND ENEMY BULLET LOGIC
LA193:  LDX  INDEX1
LA195:  LDA  CHARY,X                ;[CS] Grab the height of a player's
LA198:  BEQ  LA1DF                  ;IFNE  CHARGE ACTIVE?  [CS] bullet. No bullet? Check the next.
LA19A:  CPX  #NPCHARG
LA19C:  BCS  LA1C0                  ;IFCC  DETERMINE DIRECTION
;TOWARD INVADER
LA19E:  ADC  #PCVELO
LA1A0:  LDY  CHARCO,X
LA1A3:  BEQ  LA1A8                  ;IFNE  CHARGE IN COLLISION W. LINE?
LA1A5:  SEC                         ;YES. SLOW IT DOWN
LA1A6:  SBC  #$04
LA1A8:  STA  CHARY,X
LA1AB:  JSR  LIFECT
LA1AE:  LDA  CHARY,X
LA1B1:  CMP  #ILINDDY
LA1B3:  BCC  LA1BD                  ;IFCS  AT END?
LA1B5:  DEC  CHACOU                 ;[CS] One less shot by the player is on the screen.
LA1B8:  LDA  #$00                   ;YES, DEACTIVATE  [CS] Get rid of the player's
LA1BA:  STA  CHARY,X                ;[CS] bullet.
LA1BD:  CLV                         ;ELSE
LA1BE:  BVC  LA1DF
LA1C0:  LDA  CHARYL,X
LA1C3:  CLC                         ;TOWARD PLAYER
LA1C4:  ADC  WCHARL
LA1C7:  STA  CHARYL,X
LA1CA:  LDA  CHARY,X                ;[CS] Grab the enemy bullet Y position.
LA1CD:  ADC  WCHARIN                ;[CS] Move it up by its movement rate.
LA1D0:  CMP  CURSY                  ;[CS] Has it hit the player?
LA1D3:  BCS  LA1DC                  ;IFCC  AT TOP?
LA1D5:  DEC  ESHCOU                 ;[CS] One less enemy shot outstanding.
LA1D7:  JSR  CHATOP                 ;YES. CHECK FOR COLLISION WITH CURSOR
LA1DA:  LDA  #$00                   ;DEACTIVATE  [CS] Get rid of this player's bullet.
LA1DC:  STA  CHARY,X                ;[CS] Store the bullet position.
LA1DF:  DEC  INDEX1                 ;[CS] More bullets to check?
LA1E1:  BPL  LA193                  ;MIEND  [CS] Re-run the loop else RTS.
LA1E3:  RTS

;------------------------------------------------------------------------------
; CHATOP - CHECK FOR CURSOR CHARGE COLLISION
;------------------------------------------------------------------------------
CHATOP:
LA1E4:  LDA  CURSL1                 ;[CS] Is this shot going down the
LA1E7:  CMP  CHARL1,X               ;[CS] same segment the player is on?
LA1EA:  BNE  LA1F9                  ;IFEQ  [CS] If so, return.
LA1EC:  LDA  CURSL2                 ;SAME LINE AS CURSOR.  [CS] If they player is already dead,
LA1EF:  BMI  LA1F9                  ;IFPL  CURSOR ALREADY DEAD?  [CS] return.
LA1F1:  JSR  INCPSQ                 ;NO. KILL CURSOR
LA1F4:  LDA  #$81                   ;SPECIAL BLASTED CODE  [CS] Player is dead. Indicate this
LA1F6:  STA  CURSL2                 ;[CS] with tunnel position #81.
LA1F9:  RTS

;------------------------------------------------------------------------------
; LIFECT - PLAY - CHARGE LINE COLLISION
;   PROCESS PLAYER CHARGE'S EFFECT
;   ON ENEMY LINES
;------------------------------------------------------------------------------
LIFECT:
LA1FA:  LDY  CHARL1,X               ;DO CHARGE LINE 1 FIRST
LA1FD:  LDA  LINEY,Y
LA200:  BEQ  LA23E                  ;IFNE  LINE DEAD?
LA202:  LDA  CHARY,X                ;NO.
LA205:  CMP  LINEY,Y
LA208:  BCC  LA22F                  ;IFCS  CHARGE ON ENEMY LINES?
LA20A:  CMP  #ILINDDY               ;YES
LA20C:  BCC  LA210                  ;IFCS  LINE DEAD?
LA20E:  LDA  #$00                   ;YES
LA210:  STA  LINEY,Y                ;YES. UPDATE LINE ENEMY TO
LA213:  INC  CHARCO,X               ;UPDATE CHARGE - ENEMY LINE COLLISION COUNTER
LA216:  LDA  #$C0
LA218:  STA  LINSTA,Y               ;SET RECALC FLAG
;REQUEST LINE DESTRUCTION PIC.
LA21B:  JSR  SELICO                 ;MAKE SOUND
;GIVE PTS
LA21E:  LDX  #$FF                   ;SIGNAL SCORE ROUTINE TO USE TEMPS
LA220:  LDA  #$00                   ;ADD 1 TO SCORE FOR EACH HIT
LA222:  STA  TEMP1
LA224:  STA  TEMP2
LA226:  LDA  #$01
LA228:  STA  TEMP0
LA22A:  JSR  UPSCOR
LA22D:  LDX  INDEX1                 ;RESTORE CHARGE INDEX
LA22F:  LDA  CHARCO,X
LA232:  CMP  #$02
LA234:  BCC  LA23E                  ;IFCS  CHARGE EXHAUSTED?
LA236:  LDA  #$00                   ;YES. DEACTIVATE IT  [CS] Player's shot has disappeared.
LA238:  STA  CHARY,X                ;[CS] Clear it's Y slot and record this
LA23B:  DEC  CHACOU                 ;[CS] in the # of bullets "outstanding".
LA23E:  RTS

;------------------------------------------------------------------------------
; FIREPC - PLAY - FIRE PLAYER CHARGE
;------------------------------------------------------------------------------
FIREPC:
LA23F:  LDA  CURSL2
LA242:  BMI  LA2A5                  ;IFPL  PLAYER ALIVE
LA244:  LDA  QSTATUS
LA246:  BMI  LA270                  ;IFPL  ATTRACT?
LA248:  LDA  CURMOD                 ;YES. AUTO FIRE
LA24B:  STA  TEMP0
LA24D:  LDX  #NICHARG+NINVAD-1
LA24F:  LDA  CHARY+NPCHARG,X
LA252:  BEQ  LA268                  ;IFNE  ACTIVE?
LA254:  LDA  CHARL1+NPCHARG,X       ;YES CALUCLATE ABSOLUTE VALUE OF LINE DELTA
LA257:  SEC
LA258:  SBC  CURSL1
LA25B:  BPL  LA262                  ;IFMI
LA25D:  EOR  #$FF
LA25F:  CLC
LA260:  ADC  #$01
LA262:  CMP  #$02
LA264:  BCS  LA268                  ;IFCC  TOO CLOSE?
LA266:  INC  TEMP0                  ;YES. FIRE
LA268:  DEX
LA269:  BPL  LA24F                  ;MIEND
LA26B:  LDA  TEMP0
LA26D:  CLV                         ;ELSE
LA26E:  BVC  LA274
LA270:  LDA  SWSTAT
LA272:  AND  #MFIRE
LA274:  BEQ  LA2A5                  ;IFNE  FIRE CHARGE?
LA276:  LDX  #NPCHARG-1             ;YES  [CS] Start by checking the last bullet slot.
LA278:  LDA  CHARY,X                ;[CS] Is this currently being used?
LA27B:  BNE  LA2A2                  ;IFEQ  VACANCY?  [CS] If so, branch to a statement that will check another slot and loop.
;YES FIRE CHARGE
LA27D:  INC  CHACOU
LA280:  LDA  CURSY                  ;START AT CURSOR  [CS] Get the player's current Y position
LA283:  STA  CHARY,X                ;[CS] and store it as the bullet's Y pos.
LA286:  LDA  CURSL1                 ;[CS] Get the player's X-1 and store it as
LA289:  STA  CHARL1,X               ;STARTS AT SAME LINE AS CURSOR  [CS] the bullet's X-1 position.
LA28C:  LDA  CURSL2                 ;[CS] Get the player's X and store it as
LA28F:  STA  CHARL2,X               ;[CS] the bullet's X position.
LA292:  LDA  #$00                   ;0 COLLISION COUNTER
LA294:  STA  CHARCO,X
LA297:  JSR  SLAUNC                 ;LAUNCH SOUND
LA29A:  LDA  CURSY
LA29D:  JSR  COLCHK                 ;CHECK FOR COLLISION
LA2A0:  LDX  #$00                   ;EXIT LOOP
LA2A2:  DEX                         ;[CS] Branch from A27B. If slot is taken, then look at the previous slot to see if we can store a bullet in it.
LA2A3:  BPL  LA278                  ;MIEND
LA2A5:  RTS

;------------------------------------------------------------------------------
; FIREIC - PLAY - FIRE INVADER CHARGE
;------------------------------------------------------------------------------
FIREIC:
LA2A6:  LDA  CURSL2
LA2A9:  BMI  LA303                  ;IFPL  PLAYER ALIVE?
LA2AB:  LDX  #NINVAD-1              ;YES.
LA2AD:  LDA  INVAY,X
LA2B0:  BEQ  LA300                  ;IFNE  ACTIVE?
LA2B2:  CMP  #ILINLIY+$20           ;YES
LA2B4:  BCC  LA300                  ;IFCS  INVADER LOW ENOUGH?
LA2B6:  LDA  INVAC2,X               ;YES
LA2B9:  AND  #INVFIR
LA2BB:  BEQ  LA300                  ;IFNE  INVADER MOVING(BOTH LEGS ON LINES)?
LA2BD:  DEC  INVACT,X               ;YES. UPDATE INVADER'S FIRE TIMR
LA2C0:  BPL  LA300                  ;IFMI
LA2C2:  INC  INVACT,X
LA2C5:  LDA  INVAC1,X
LA2C8:  AND  #INVMOT
LA2CA:  BNE  LA300                  ;IFEQ
LA2CC:  LDA  RANDOM
LA2CF:  LDY  ESHCOU
LA2D1:  CMP  CHANCE,Y
LA2D4:  BCC  LA300                  ;IFCS  TIMER IN FIRE WINDOW?
LA2D6:  LDY  WCHAMX
LA2D9:  LDA  CHARY+NPCHARG,Y        ;UNTIL VACANCY
LA2DC:  BNE  LA2FD                  ;IFEQ  VACANCY?
LA2DE:  LDA  INVAY,X                ;YES
LA2E1:  STA  CHARY+NPCHARG,Y        ;START AT INVADER LOC
LA2E4:  LDA  INVAL1,X
LA2E7:  STA  CHARL1+NPCHARG,Y       ;SAME LINE AS INVADER
LA2EA:  LDA  INVAL2,X
LA2ED:  STA  CHARL2+NPCHARG,Y
LA2F0:  LDA  WCHARFR
LA2F3:  STA  INVACT,X               ;RESTART TIMER
LA2F6:  JSR  ESLSON
LA2F9:  INC  ESHCOU                 ;[CS] One more enemy bullet is in play.
LA2FB:  LDY  #$00                   ;EXIT LOOP
LA2FD:  DEY
LA2FE:  BPL  LA2D9                  ;MIEND
LA300:  DEX
LA301:  BPL  LA2AD                  ;MIEND
LA303:  RTS

CHANCE:
LA304:  .byte $00, $E0, $F0, $FA, $FF ;HIGHER CHANCE FOR ENEMY SHOT IF LESS ON SCREEN

;------------------------------------------------------------------------------
; INCFS2 - PLAY-EXPLOSION OF FUSE INIT
;   OUTPUT:X AND Y PRESERVED
;   SAVEX,SAVEY,TEMP0,1,2,3,4 ARE GARBAGE
;------------------------------------------------------------------------------
INCFS2:
LA309:  STX  INDEX1
LA30B:  LDA  #$FF                   ;MARK SHOT USED
LA30D:  STA  CHARCO,X
LA310:  TYA                         ;CONVERT SHOT INDEX TO INVADER INDEX
LA311:  SEC
LA312:  SBC  #NICHARG
LA314:  TAY
LA315:  LDA  INVAL1,Y
LA318:  STA  TEMP4
LA31A:  LDA  RANDO2                 ;[CS] Grab a random number,
LA31D:  AND  #$07                   ;[CS] 0-7.
LA31F:  CMP  #$03
LA321:  BCC  LA325                  ;IFCS  RANDOMLY CHOOSE 0(250_,1(500), OR 2(750)
LA323:  LDA  #$00
LA325:  PHA
LA326:  CLC
LA327:  ADC  #CFTYPE
LA329:  JSR  GEXIFU                 ;INITIALIZE EXPLOSION
LA32C:  JSR  KILINV                 ;KILL FUSE
LA32F:  PLA
LA330:  CLC
LA331:  ADC  #$05
LA333:  TAX
LA334:  JSR  UPSCOR                 ;UPDATE SCORE
LA337:  LDX  INDEX1
LA339:  RTS

;------------------------------------------------------------------------------
; INIPSQ - YES. DESTROY CURSOR  (comment at the call)
;------------------------------------------------------------------------------
INIPSQ:
LA33A:  LDA  #IPTYPE
LA33C:  JSR  DEADCU                 ;KILL CURSOR
LA33F:  DEC  CURSL2                 ;DISPLAY CURSOR  [CS] Move player one segment over.
LA342:  RTS

;------------------------------------------------------------------------------
; INFPSQ - YES. DEAD CURSOR NOW  (comment at the call)
;------------------------------------------------------------------------------
INFPSQ:
LA343:  LDA  #FPSPXI                ;SPECIAL BANG PIC CODE
LA345:  BNE  INCP2

;------------------------------------------------------------------------------
; INPPSQ - YES. START BANG. KILL CURSOR  (comment at the call)
;   YES. KILL CURSOR  (another call)
;------------------------------------------------------------------------------
INPPSQ:
LA347:  LDA  #PPSPXI                ;SPECIAL EXPLOSION PIC CODE
LA349:  BNE  INCP2

;------------------------------------------------------------------------------
; INCPSQ - NO. KILL CURSOR  (comment at the call)
;------------------------------------------------------------------------------
INCPSQ:
LA34B:  LDA  #<CPSPXI               ;SPECIAL EXPLOSION PIC CODE

INCP2:
LA34D:  STA  SPXIND
LA350:  LDA  #CPTYPE

;------------------------------------------------------------------------------
; DEADCU - KILL CURSOR
;------------------------------------------------------------------------------
DEADCU:
LA352:  STA  TEMP3                  ;EXPOLSION CODE
LA354:  LDA  CURSY                  ;POSITION
LA357:  STA  TEMP0
LA359:  LDA  CURSL1
LA35C:  STA  TEMP4
LA35E:  JSR  CPEXPL                 ;START NOISE
LA361:  JSR  GENEX2                 ;INIT EXPLOSION
LA364:  LDA  #$81                   ;KILL CURSOR/NO DISP  [CS] Player is dead. Indicate this
LA366:  STA  CURSL2                 ;[CS] with tunnel position #81.
LA369:  LDA  #$01                   ;INIT TIMER FOR EXP.
LA36B:  STA  SPFTIM
LA36E:  RTS

;------------------------------------------------------------------------------
; INCCSQ - YES. INITIALIZE EXPLOSION  (comment at the call)
;------------------------------------------------------------------------------
INCCSQ:
LA36F:  JSR  CCEXPL                 ;CHARGE-CHARGE
LA372:  LDA  CHARY+NPCHARG,Y
LA375:  STA  TEMP0
LA377:  LDA  CHARL1+NPCHARG,Y
LA37A:  STA  TEMP4
LA37C:  LDA  #CCTYPE
LA37E:  JSR  GENEXP
LA381:  LDA  #$00                   ;DEACTIVATE SHOT  [CS] Get rid of this enemy's
LA383:  STA  CHARY+NPCHARG,Y        ;[CS] bullet and decrease the
LA386:  DEC  ESHCOU                 ;ONE LESS SHOT  [CS] bullet-in-play count.
LA388:  LDA  #$FF                   ;SHOT USED FLAG
LA38A:  STA  CHARCO,X
LA38D:  RTS

;------------------------------------------------------------------------------
; INCIS2 - START BANG  (comment at the call)
;------------------------------------------------------------------------------
INCIS2:
LA38E:  LDA  #$FF                   ;SHOT USED MARKER
LA390:  STA  CHARCO,X
LA393:  TYA                         ;CONVERT SHOT INDEX TO INVADER INDEX
LA394:  SEC
LA395:  SBC  #NICHARG
LA397:  TAY

INCISQ:
LA398:  LDA  INVAC1,Y
LA39B:  AND  #ZROCCW|ZMOTJM
LA39D:  CMP  #ZROCCW|ZMOTJM
LA39F:  BEQ  LA3A7                  ;IFNE  FLIPPING CCW?
LA3A1:  LDA  INVAL1,Y               ;NO. USE BASE LEG
LA3A4:  CLV                         ;ELSE
LA3A5:  BVC  LA3AF
LA3A7:  LDA  INVAL1,Y               ;YES. ADJUST BASE LIVE
LA3AA:  SEC
LA3AB:  SBC  #$01
LA3AD:  AND  #$0F
LA3AF:  STA  TEMP4
LA3B1:  LDA  #CITYPE
LA3B3:  JSR  GEXIFU                 ;INITIALIZE BANG PIC
LA3B6:  JSR  KILINV                 ;KILL INVADER
LA3B9:  LDA  INVAC1,Y
LA3BC:  AND  #INVABI
LA3BE:  TAY
LA3BF:  LDX  INVPIN,Y               ;INDEX FOR PTS TO ADD
LA3C2:  JMP  UPSCOR                 ;UPDATE SCORE

INVPIN:
LA3C5:  .byte $01, $02, $03, $04, $01

;------------------------------------------------------------------------------
; GEXIFU - INITIALIZE EXPLOSION  (comment at the call)
;   INITIALIZE BANG PIC  (another call)
;------------------------------------------------------------------------------
GEXIFU:
LA3CA:  PHA
LA3CB:  JSR  CIEXPL                 ;BANG SOUND
LA3CE:  LDA  INVAY,Y
LA3D1:  STA  TEMP0
LA3D3:  PLA

;------------------------------------------------------------------------------
; GENEXP - TEMP0=EXPLOSION Y ;TEMP4=EXPLOSION LINE
;   GENERAL EXPLOSION STARTER
;   INPUT:ACC=EXPLOSION TYPE
;------------------------------------------------------------------------------
GENEXP:
LA3D4:  STA  TEMP3                  ;SAVE TYPE & DEPTH

;------------------------------------------------------------------------------
; GENEX2 - INIT EXPLOSION  (comment at the call)
;------------------------------------------------------------------------------
GENEX2:
LA3D6:  STX  SAVEX
LA3D8:  STY  SAVEY
LA3DA:  LDA  #$00
LA3DC:  STA  TEMP1
LA3DE:  STA  TEMP2
LA3E0:  LDX  #NEXPLO-1
LA3E2:  LDA  EXPLOY,X
LA3E5:  BEQ  GOTEXP                 ;EXIT IF VACANCY
LA3E7:  LDA  EXPLOS,X
LA3EA:  CMP  TEMP1
LA3EC:  BCC  LA3F2                  ;IFCS  FURTHEST ALONG SO FAR?
LA3EE:  STA  TEMP1                  ;YES. SAVE IT
LA3F0:  STX  TEMP2
LA3F2:  DEX
LA3F3:  BPL  LA3E2                  ;MIEND
LA3F5:  DEC  EXPCOU                 ;WILL BE INCD LATER
LA3F8:  LDX  TEMP2                  ;NO VACANCIES. USE FURTHEST AONG

GOTEXP:
LA3FA:  LDA  #$00
LA3FC:  STA  EXPLOS,X               ;START SEQUENCES
LA3FF:  LDA  TEMP3
LA401:  STA  EXPLOT,X               ;EXPLOSION TYPE
LA404:  LDA  TEMP0
LA406:  STA  EXPLOY,X               ;EXPLOSION DEPTH
LA409:  LDA  TEMP4
LA40B:  STA  EXPLOL,X               ;EXPLOSION LINE
LA40E:  INC  EXPCOU                 ;INC COUNTER
LA411:  LDX  SAVEX
LA413:  LDY  SAVEY
LA415:  RTS

;------------------------------------------------------------------------------
; PROEXP - PLAY-PROCESS EXPLOSIONS
;------------------------------------------------------------------------------
PROEXP:
LA416:  LDA  EXPCOU                 ;[CS] Are there any enemy explosions (deaths) to handle?
LA419:  BEQ  LA447                  ;IFNE  ANY BANGS?  [CS] If not, return.
LA41B:  LDA  #$00                   ;YES CLEAR COUNT  [CS] Clear out all enemy explosions.
LA41D:  STA  EXPCOU
LA420:  LDX  #NEXPLO-1
LA422:  LDA  EXPLOY,X
LA425:  BEQ  LA444                  ;IFNE  ACTIVE BANG?
LA427:  LDA  EXPLOS,X               ;YES. UPDATE SEQUENCES
LA42A:  LDY  EXPLOT,X
LA42D:  CLC
LA42E:  ADC  TEXINC,Y
LA431:  STA  EXPLOS,X
LA434:  CMP  TEXPDN,Y
LA437:  BCC  LA441                  ;IFCS  EXPLOSION DONE?
LA439:  LDA  #$00                   ;YES. DEACTIVATE IT
LA43B:  STA  EXPLOY,X
LA43E:  CLV                         ;ELSE
LA43F:  BVC  LA444
LA441:  INC  EXPCOU                 ;NO. INC COUNTER
LA444:  DEX
LA445:  BPL  LA422                  ;MIEND
LA447:  RTS

TEXPDN:
;[CS] DATA used by previous subroutine
LA448:  .byte $10, $15, $20, $20, $20, $10 ;LAST SEQUENCE # TABLE(*4)

TEXINC:
LA44E:  .byte $03, $01, $03, $03, $03, $03

;------------------------------------------------------------------------------
; COLLIS - PLAY - COLLISION (MAINLINE)
;------------------------------------------------------------------------------
COLLIS:
LA454:  LDX  #NPCHARG-1
LA456:  LDA  CHARY,X
LA459:  BEQ  LA45E                  ;IFNE  PLAYER CHARGE ACTIVE?
LA45B:  JSR  COLCHK
LA45E:  DEX
LA45F:  BPL  LA456                  ;MIEND  ENDLOOP FOR PLAYER CHARGES
LA461:  RTS

CHKSM4:
LA462:  .byte QCHKS4

;------------------------------------------------------------------------------
; COLCHK - PLAY - COLLISION - SINGLE CHECK
;   INPUT:ACC=PLAYER CHARGE Y
;------------------------------------------------------------------------------
COLCHK:
LA463:  STA  TEMPX
LA465:  LDY  #NICHARG-1+NINVAD      ;YES.
LA467:  LDA  CHARY+NPCHARG,Y
LA46A:  BEQ  LA4EB                  ;IFNE  I C OR INVADER ACTIVE?
LA46C:  CMP  TEMPX                  ;YES. DETERMINE OBSOLUTE DELTA
LA46E:  BCC  LA475                  ;IFCS
LA470:  SBC  TEMPX
LA472:  CLV                         ;ELSE
LA473:  BVC  LA47B
LA475:  LDA  TEMPX
LA477:  SEC
LA478:  SBC  CHARY+NPCHARG,Y
LA47B:  CPY  #NICHARG
LA47D:  BCS  LA491                  ;IFCC  ENEMY SHOT OR INVADER?
LA47F:  CMP  CHACHA                 ;SHOT
LA481:  BCS  LA48E                  ;IFCC  IN RANGE?
LA483:  LDA  CHARL1+NPCHARG,Y       ;YES.
LA486:  EOR  CHARL1,X
LA489:  BNE  LA48E                  ;IFEQ  ON SAME LINE?
LA48B:  JSR  INCCSQ                 ;YES. INITIALIZE EXPLOSION
LA48E:  CLV                         ;ELSE
LA48F:  BVC  LA4EB
LA491:  PHA                         ;INVADER. SAVE DELTA
LA492:  STY  INDEX2
LA494:  LDA  INVAC1-NICHARG,Y
LA497:  AND  #INVABI
LA499:  TAY
LA49A:  PLA
LA49B:  CMP  ENSIZE,Y
LA49E:  BCS  NOCOL                  ;IFCC  IN RANGE BY TYPE?
LA4A0:  CPY  #ZABFUS                ;YES.
LA4A2:  BNE  LA4C1                  ;IFEQ  FUSE?
LA4A4:  LDY  INDEX2                 ;YES.
LA4A6:  LDA  INVAY-NICHARG,Y
LA4A9:  CMP  CURSY
LA4AC:  BEQ  LA4BE                  ;IFNE  FUSE AT TOP?
LA4AE:  LDA  CHARL1,X               ;NO.  [CS] Is the player's bullet on the
LA4B1:  CMP  INVAL1-NICHARG,Y       ;[CS] same segment as an enemy?
LA4B4:  BNE  LA4BE                  ;IFEQ  SAME BASE LINE?
LA4B6:  LDA  INVAL2-NICHARG,Y       ;YES.
LA4B9:  BPL  LA4BE                  ;IFMI  VULNERABLE FUSE?
LA4BB:  JSR  INCFS2                 ;YES. START BANG, KILL FUSE, GIVE PTS.
LA4BE:  CLV                         ;ELSE
LA4BF:  BVC  NOCOL
LA4C1:  LDY  INDEX2                 ;NO. FLIPPER,TANKER,SPINNER,PULSAR
LA4C3:  LDA  INVAL2-NICHARG,Y
LA4C6:  BPL  LA4D2                  ;IFMI  FLIPPER?
LA4C8:  LDA  INVAL1-NICHARG,Y       ;YES.
LA4CB:  CMP  CHARL2,X               ;BASE & SECONDARY MATCH?
LA4CE:  BEQ  YESCOL
LA4D0:  BNE  OKATOP                 ;NO. CHECK FOR BASE MATCH
LA4D2:  LDA  INVAY-NICHARG,Y
LA4D5:  CMP  CURSY
LA4D8:  BEQ  NOCOL                  ;IFNE  AT TOP?

OKATOP:
LA4DA:  LDA  INVAL1-NICHARG,Y       ;NO.
LA4DD:  CMP  CHARL1,X
LA4E0:  BNE  NOCOL                  ;IFEQ  BASE LEG MATCH?

YESCOL:
LA4E2:  STX  INDEX1                 ;YES.
LA4E4:  JSR  INCIS2                 ;START BANG
LA4E7:  LDX  INDEX1

NOCOL:
LA4E9:  LDY  INDEX2
LA4EB:  DEY
LA4EC:  BMI  LA4F1                  ;MIEND  ENDLOOP FOR ICS
LA4EE:  JMP  LA467
LA4F1:  LDA  CHARCO,X
LA4F4:  CMP  #$FF
LA4F6:  BNE  LA503                  ;IFEQ  PLAYER CHARGE SPENT?
LA4F8:  LDA  #$00                   ;YES. DEACTIVATE IT
LA4FA:  STA  CHARY,X
LA4FD:  DEC  CHACOU
LA500:  STA  CHARCO,X
LA503:  RTS

;------------------------------------------------------------------------------
; ANALYZ - PLAY - ANALYZE GAME
;------------------------------------------------------------------------------
ANALYZ:
LA504:  LDA  CURSL2
LA507:  BPL  ZQVAVG                 ;IFMI  CURSOR DEAD?
LA509:  LDA  CHACOU                 ;YES  [CS] If no player shots on screen
LA50C:  ORA  ESHCOU                 ;[CS] ??????
LA50E:  ORA  EXPCOU                 ;[CS] and if no enemy shots on screen
LA511:  BNE  LA57E                  ;IFEQ  ANY ACTIVE CHARGES OR BANGS?  [CS] then skip a lot of this, else...
LA513:  LDX  WINVMX                 ;NO. DROP EVERYBODY INTO WELL  [CS] Start with slot 6 (of 0-6)
LA516:  LDA  INVAY,X                ;[CS] Is there an enemy in this slot?
LA519:  BEQ  LA529                  ;IFNE  ACTIVE INVADER?  [CS] If not, check the other slots.
LA51B:  CLC                         ;YES MOVE IT DOWN
LA51C:  ADC  #$0F
LA51E:  BCS  LA522                  ;IFCC
LA520:  CMP  #ILINDDY
LA522:  BCC  LA526                  ;IFCS  INVADER AT BOTTOM?
LA524:  LDA  #$00                   ;YES. DEACTIVATE IT  [CS] Kill this enemy.
LA526:  STA  INVAY,X                ;[CS] Null their height.
LA529:  DEX                         ;[CS] Decrement loop and check
LA52A:  BPL  LA516                  ;MIEND  [CS] the next enemy.
LA52C:  LDX  PLAYUP                 ;[CS] Does the current player
LA52E:  LDA  LIVES1,X               ;[CS] have exactly one life left?
LA530:  CMP  #$01
LA532:  BNE  LA554                  ;IFEQ  GAME OVER?  [CS] If so, skip a bit.
LA534:  LDA  #$00                   ;YES. REQUEST RECALC OF WELL TOP
LA536:  STA  LEVELY
LA539:  LDA  #$01                   ;REQUEST REDISPLAY OF WELL
LA53B:  STA  ROTDIS
LA53E:  LDA  EYL
LA540:  SEC
LA541:  SBC  #$20
LA543:  STA  EYL                    ;SHRINK HOLE
LA545:  LDA  EYH
LA547:  SBC  #$00
LA549:  STA  EYH
LA54B:  CMP  #$FA
LA54D:  CLC
LA54E:  BNE  LA551                  ;IFEQ  FAR ENOUGH?
LA550:  SEC                         ;YES. END GAME
LA551:  CLV                         ;ELSE
LA552:  BVC  LA561
LA554:  LDA  CURSY                  ;MOVE CURSOR DOWN
LA557:  CLC
LA558:  ADC  #$0F
LA55A:  STA  CURSY
LA55D:  BCS  LA561                  ;IFCC
LA55F:  CMP  #ILINDDY
LA561:  BCC  LA57E                  ;IFCS  CURSOR AT BOTTOM?
;YES. END OF LIFE PHASE.
LA563:  LDA  #CENDLI                ;YES. GO TO END OF LIFE STATE
LA565:  STA  QSTATE
LA567:  JSR  INICHA                 ;CLEAR CHARGES  [CS] Remove all bullets from play.
LA56A:  LDA  INMCOU                 ;ADD # OF INVADERS  [CS] How many enemies are inside the
LA56D:  CLC                         ;[CS] tube? Add with the # at the
LA56E:  ADC  INCCOU                 ;[CS] top of the tube. Add with the #
LA571:  CLC                         ;[CS] of enemies YET to appear.
LA572:  ADC  NYMCOU                 ;TO # NYMPHS  [CS] If the total is less than
LA575:  CMP  #NNYMPH-1              ;[CS] 3F, then make the number of
LA577:  BCC  LA57B                  ;IFCS  MAX OUT  [CS] enemies yet to appear equal
LA579:  LDA  #NNYMPH-1              ;[CS] to 3F (63 decimal).
LA57B:  STA  NYMCOU                 ;FOR NEXT LIFE
LA57E:  CLV                         ;ELSE
LA57F:  BVC  LA5CA                  ;[CS] Return from Subroutine

ZQVAVG:
LA581:  LDA  QT3                    ;[CS] More copy protection?
LA584:  ORA  QT6
LA587:  BEQ  LA593                  ;IFNE
LA589:  LDA  #$17                   ;[CS] Compare player one's score to
LA58B:  CMP  LSCORH                 ;[CS] 170,000. "famous" score
LA58D:  BCS  LA593                  ;IFCC  [CS] bug (copy protection) for
LA58F:  LDX  LSCORL                 ;[CS] Tempest. Mangle $0000-$0099.
LA591:  INC  $00,X                  ;[CS] Kinda. Uses BCD numbers.
LA593:  LDA  CURMOD
LA596:  BNE  LA5CA                  ;IFEQ  TOP MODE?  [CS] Return from Subroutine.
LA598:  LDA  NYMCOU                 ;YES CURSOR ALIVE & BANGS DONE?
LA59B:  ORA  EXPCOU
LA59E:  BNE  LINER                  ;IFEQ  ALL NYMPHS CONVERTED?
LA5A0:  LDY  WINVMX                 ;YES. ALL INVADERS OOF LINES?
LA5A3:  LDA  INVAY,Y
LA5A6:  BEQ  LA5AC                  ;IFNE
LA5A8:  CMP  #$11
LA5AA:  BCS  LINER                  ;EXIT IF LINER (NOT AT TOP)
LA5AC:  DEY
LA5AD:  BPL  LA5A3                  ;MIEND  EXIT AFTER ALL CHECKED. (NO LINERS)
LA5AF:  JSR  INDROP                 ;YES.
LA5B2:  JSR  INICHA                 ;CLEAR CHARGES  [CS] Remove all bullets from play.

LINER:
LA5B5:  LDA  SWSTRT
LA5B7:  AND  #MSTRT2|MSTRT1
LA5B9:  BEQ  LA5CA                  ;IFNE  EITHER START PRESSED?  [CS] Return from Subroutine.
LA5BB:  BIT  QSTATUS                ;YES
LA5BD:  BPL  LA5CA                  ;IFMI  ATTRACT?  [CS] Return from Subroutine.
LA5BF:  LDA  OPTIN1                 ;NO.
LA5C1:  AND  #$43
LA5C3:  CMP  #$40
LA5C5:  BNE  LA5CA                  ;IFEQ  FREE PLAY & ABORT ENABLED?  [CS] Return from Subroutine.
LA5C7:  JSR  INDROP                 ;YES. INITIATE DROP MODE
LA5CA:  RTS

;------------------------------------------------------------------------------
; INDROP - INITIALIZE CURSOR DROP MODE
;------------------------------------------------------------------------------
INDROP:
LA5CB:  LDA  #CDROP                 ;DROP STATE NEXT  [CS] Level complete. Set general game
LA5CD:  STA  QSTATE                 ;[CS] status = zoom-out of current level.
LA5CF:  LDA  CURMOD                 ;SET CURSOR DROP MODE
LA5D2:  ORA  #$80
LA5D4:  STA  CURMOD
LA5D7:  LDA  #$00                   ;INITIALIZE DOWNWARD ACCELERATION
LA5D9:  STA  CURSVL
LA5DC:  STA  CURSYL                 ;ZERO FRAC. POSITION
LA5DF:  STA  EYLL                   ;TO PREVENT JERKING
LA5E1:  STA  ELICNT
LA5E4:  LDA  #$02
LA5E6:  STA  CURSVH
LA5E9:  LDX  #NLINES-1
LA5EB:  LDA  LINEY,X
LA5EE:  BEQ  LA5F3                  ;IFNE
LA5F0:  INC  ELICNT                 ;COUNT LIVE SPIKES
LA5F3:  DEX
LA5F4:  BPL  LA5EB                  ;MIEND
LA5F6:  LDA  ELICNT
LA5F9:  BEQ  LA612                  ;IFNE  ENEMY LINES?
LA5FB:  LDA  CURWAV                 ;YES.
LA5FD:  CMP  #$07
LA5FF:  BCS  LA612                  ;IFCC  WARN PLAYER?
;YES
LA601:  LDA  #6*QUASEC              ;WARNING DELAY
LA603:  STA  QTMPAUS
LA605:  LDA  #CPAUSE                ;PAUSE FIRST  [CS] Set game status to non-player
LA607:  STA  QSTATE                 ;[CS] input mode.
LA609:  LDA  #CDROP                 ;THEN DROP MODE
LA60B:  STA  QNXTSTA
LA60D:  LDA  #$80                   ;SET WARNING FLAG
LA60F:  STA  ELICNT
LA612:  LDA  #$FF
LA614:  STA  SUZTIM                 ;DEACTIVATE SUPERZAPPER
LA617:  RTS

;------------------------------------------------------------------------------
; PRBOOM - PLAY-PROCESS BIG BOOM
;------------------------------------------------------------------------------
PRBOOM:
LA618:  LDA  BOOMTI                 ;SET BOOM OFF FLAG
LA61B:  STA  BOOMFL
LA61E:  LDX  #NPARTI-1
LA620:  STX  INDEX1
LA622:  LDX  INDEX1
LA624:  LDA  PARTIY,X
LA627:  BNE  LA634                  ;IFEQ  ACTIVE PARTICLE?
LA629:  LDA  BOOMTI                 ;NO.
LA62C:  BEQ  LA631                  ;IFNE  BOOM TIMER EXPIRED?
LA62E:  JSR  TIMLAU                 ;NO. LAUNCH MORE PARTICLES OF TIME
LA631:  CLV                         ;ELSE
LA632:  BVC  LA63F
LA634:  JSR  UPARPO                 ;YES. UPDATE PARTICLE POSITION
LA637:  JSR  DECPAR                 ;DECELERATE PARTICLE
LA63A:  LDA  #$FF                   ;BOOM ACTIVE
LA63C:  STA  BOOMFL
LA63F:  DEC  INDEX1
LA641:  BPL  LA622                  ;MIEND  END LOOP
LA643:  LDA  QFRAME
LA645:  AND  #$01
LA647:  BNE  LA651                  ;IFEQ
LA649:  LDA  BOOMTI
LA64C:  BEQ  LA651                  ;IFNE
LA64E:  DEC  BOOMTI                 ;UPDATE BOOM TIMER (STOP AT 0)
LA651:  LDA  BOOMFL
LA654:  BNE  LA65A                  ;IFEQ  BOOM ACTIVE?
LA656:  LDA  #CGETINI               ;NO. GET INITIALS  [CS] Game status = entry of high score
LA658:  STA  QSTATE                 ;[CS] onto high score list, after game.
LA65A:  RTS

;------------------------------------------------------------------------------
; TIMLAU - NO. LAUNCH MORE PARTICLES OF TIME  (comment at the call)
;------------------------------------------------------------------------------
TIMLAU:
LA65B:  LDA  QFRAME
LA65D:  AND  #$00
LA65F:  BNE  LA69A                  ;IFEQ  DELAY SINCE LAST LAUNCH OK?
;YES. LAUNCH ANOTHER
LA661:  LDA  #$80                   ;SET UP INITIAL LOCATION IN CENTER
LA663:  STA  PARTIX,X
LA666:  STA  PARTIY,X
LA669:  STA  PARTIZ,X
;SET UP VELOCITY (RANDOM WITHIN
LA66C:  LDA  RANDO2                 ;GIVE RANGE)
LA66F:  STA  PARLXV,X               ;FRACTIONAL X VELOCITY
LA672:  JSR  FIXTOP
LA675:  STA  PARTXV,X               ;INTEGER X
LA678:  LDA  RANDOM
LA67B:  STA  PARLYV,X               ;Y
LA67E:  JSR  FIXTOP
LA681:  BMI  LA688                  ;IFPL  UPDATE PARTICLE POSITION
LA683:  EOR  #$FF
LA685:  CLC
LA686:  ADC  #$01
LA688:  STA  PARTYV,X
LA68B:  LDA  RANDOM                 ;Z
LA68E:  STA  PARLZV,X
LA691:  JSR  FIXTOP
LA694:  STA  PARTZV,X
LA697:  JSR  CIEXPL                 ;MAKE NOISE
LA69A:  RTS

;------------------------------------------------------------------------------
; FIXTOP - [note] Return a random value in A: 0..7 from RANDO2, negated when bit 0 of A on entry was 1.
;------------------------------------------------------------------------------
FIXTOP:
LA69B:  LSR
LA69C:  LDA  RANDO2                 ;[CS] Grab a random number,
LA69F:  AND  #$07                   ;[CS] 0-7.
LA6A1:  BCC  LA6A8                  ;IFCS
LA6A3:  EOR  #$FF
LA6A5:  CLC
LA6A6:  ADC  #$01
LA6A8:  RTS

;------------------------------------------------------------------------------
; UPARPO - UPDATE PARTICLE POSITION
;------------------------------------------------------------------------------
UPARPO:
LA6A9:  LDA  PARLYV,X               ;Y
LA6AC:  CLC
LA6AD:  ADC  PARLIY,X
LA6B0:  STA  PARLIY,X               ;FRACTIONAL
LA6B3:  LDA  PARTYV,X
LA6B6:  BMI  LA6C4                  ;IFPL
LA6B8:  ADC  PARTIY,X               ;+ VELOCITY
LA6BB:  CMP  #$F0
LA6BD:  BCC  LA6C1                  ;IFCS
LA6BF:  LDA  #$00                   ;OFF SCREEN
LA6C1:  CLV                         ;ELSE
LA6C2:  BVC  LA6CD
LA6C4:  ADC  PARTIY,X               ;- VELOCITY
LA6C7:  CMP  #$10
LA6C9:  BCS  LA6CD                  ;IFCC
LA6CB:  LDA  #$00                   ;OFF SCREEN
LA6CD:  TAY
LA6CE:  LDA  PARLXV,X               ;X
LA6D1:  CLC
LA6D2:  ADC  PARLIX,X
LA6D5:  STA  PARLIX,X               ;FRACTIONAL
LA6D8:  LDA  PARTXV,X
LA6DB:  BMI  LA6E9                  ;IFPL
LA6DD:  ADC  PARTIX,X               ;+VELOCITY
LA6E0:  CMP  #$F0
LA6E2:  BCC  LA6E6                  ;IFCS
LA6E4:  LDY  #$00                   ;OFF SCREEN
LA6E6:  CLV                         ;ELSE
LA6E7:  BVC  LA6F2
LA6E9:  ADC  PARTIX,X               ;-VELOCITY
LA6EC:  CMP  #$10
LA6EE:  BCS  LA6F2                  ;IFCC
LA6F0:  LDY  #$00                   ;OFF SCREEN
LA6F2:  STA  PARTIX,X
LA6F5:  LDA  PARLZV,X               ;Z
LA6F8:  CLC
LA6F9:  ADC  PARLIZ,X
LA6FC:  STA  PARLIZ,X               ;FRACTIONAL
LA6FF:  LDA  PARTZV,X
LA702:  BMI  LA710                  ;IFPL
LA704:  ADC  PARTIZ,X               ;+ VELOCITY
LA707:  CMP  #$F0
LA709:  BCC  LA70D                  ;IFCS
LA70B:  LDY  #$00                   ;OFF SCREEN
LA70D:  CLV                         ;ELSE
LA70E:  BVC  LA719
LA710:  ADC  PARTIZ,X               ;VELOCITY
LA713:  CMP  #$10
LA715:  BCS  LA719                  ;IFCC
LA717:  LDY  #$00                   ;OFF SCREEN
LA719:  STA  PARTIZ,X
LA71C:  TYA
LA71D:  STA  PARTIY,X
LA720:  RTS

;------------------------------------------------------------------------------
; DECPAR - DECELERATE PARTICLE  (comment at the call)
;------------------------------------------------------------------------------
DECPAR:
LA721:  LDA  #$FD                   ;VELOCITY=0 COUNTER
LA723:  STA  TEMP0
LA725:  LDA  PARLXV,X
LA728:  LDY  PARTXV,X
LA72B:  JSR  DECELE                 ;DECELERATE X VELO
LA72E:  STA  PARLXV,X
LA731:  TYA
LA732:  STA  PARTXV,X
LA735:  LDA  PARLYV,X
LA738:  LDY  PARTYV,X
LA73B:  JSR  DECELE                 ;DECELERATE Y VELO
LA73E:  STA  PARLYV,X
LA741:  TYA
LA742:  STA  PARTYV,X
LA745:  LDA  PARLZV,X
LA748:  LDY  PARTZV,X
LA74B:  JSR  DECELE                 ;DECELERATE Z VELO
LA74E:  STA  PARLZV,X
LA751:  TYA
LA752:  STA  PARTZV,X
LA755:  LDA  TEMP0
LA757:  BNE  LA75C                  ;IFEQ  ALL 3 DIRECTIONS VELOCITY=0?
LA759:  STA  PARTIY,X               ;YES. DEACTIVATE PARTICLE
LA75C:  RTS

;------------------------------------------------------------------------------
; DECELE - DECELERATE X VELO  (comment at the call)
;   DECELERATE Y VELO  (another call)
;   DECELERATE Z VELO  (another call)
;------------------------------------------------------------------------------
DECELE:
LA75D:  STY  TEMP2
LA75F:  BIT  TEMP2
LA761:  BMI  LA772                  ;IFPL  VELOCITY+ OR -?
LA763:  SEC                         ;+ SO ECELERATE BY SUBTRACTING
LA764:  SBC  DECELO
LA767:  STA  TEMP1
LA769:  LDA  TEMP2
LA76B:  SBC  #$00
LA76D:  BCC  HIT0                   ;VELOCITY HIT 0? (BR IF YES)
LA76F:  CLV                         ;ELSE
LA770:  BVC  LA784
LA772:  CLC                         ;-, SO DECELERATE BY ADDING
LA773:  ADC  DECELO
LA776:  STA  TEMP1
LA778:  LDA  TEMP2
LA77A:  ADC  #$00
LA77C:  BCC  LA784                  ;IFCS  VELOCITY HIT 0?

HIT0:
LA77E:  INC  TEMP0                  ;YES INCREMENT VELOCITY=0 COUNTER
LA780:  LDA  #$00
LA782:  STA  TEMP1
LA784:  TAY                         ;RETURN WITH NEW VELOCITY
LA785:  LDA  TEMP1
LA787:  RTS

DECELO:
LA788:  .byte $20

;------------------------------------------------------------------------------
; INBOOM - INITIALIZE PARTICLES
;------------------------------------------------------------------------------
INBOOM:
LA789:  LDX  #NPARTI-1
LA78B:  LDA  #$00
LA78D:  STA  PARTIY,X               ;DEACTIVATE PARTICLE
LA790:  DEX
LA791:  BPL  LA78B                  ;MIEND
LA793:  LDA  #$20                   ;1/5 SECOND UNTIS
LA795:  STA  BOOMTI
LA798:  STA  BOOMFL                 ;ACTIVATE BOOM
LA79B:  LDA  #CDBOOM                ;BOOM DISPLAY STATE  [CS] Set mode to end-game victory
LA79D:  STA  QDSTATE                ;[CS] explosion for high score.
LA79F:  LDA  #$00
LA7A1:  STA  ZADJL
LA7A3:  STA  ZADJL+1
LA7A5:  RTS

;------------------------------------------------------------------------------
; POLDEL - UTILITY - LINE LINE POLOR DELTA
;   INPUT: Y,ACC=LINE # FOR DETERMINATIN
;   OUTPUT:ACC=# OF LINES ACC LINE IS FROM Y LINE IN
;   SHORTEST DIRECTION (-8 TO +7) (-MEANS CLOCKWISE)
;------------------------------------------------------------------------------
POLDEL:
LA7A6:  STY  TEMP1
LA7A8:  SEC
LA7A9:  SBC  TEMP1
LA7AB:  STA  TEMP1
LA7AD:  BIT  WELTYP
LA7B0:  BMI  LA7BB                  ;IFPL  PLANAR?  [CS] Return from subroutine
LA7B2:  AND  #$0F
LA7B4:  BIT  EIGHT                  ;NO.
LA7B7:  BEQ  LA7BB                  ;IFNE  TAKE SHORTEST ROUTE  [CS] Return from subroutine
LA7B9:  ORA  #$F8
LA7BB:  RTS

EIGHT:
LA7BC:  .byte $08

;------------------------------------------------------------------------------
; INSTAR - INITIALIZE-PLANES OF STARS
;------------------------------------------------------------------------------
INSTAR:
LA7BD:  LDX  #NPLANE-1
LA7BF:  LDA  #$00
LA7C1:  STA  PLANEY,X
LA7C4:  DEX
LA7C5:  BPL  LA7C1                  ;MIEND
LA7C7:  LDA  #$F0
LA7C9:  STA  PLANEY+NPLANE-1        ;ACTIVATE LAST PLANE FAR AWAY
LA7CC:  LDA  #$FF
LA7CE:  STA  PLAGRO                 ;SET STAR FIELD GROWING FLAG
LA7D1:  RTS

;------------------------------------------------------------------------------
; PRSTAR - PLAY-PROCESS PLANES OF STARS
;   INPUT:IF PLAGRO IS-,THEN STAR FIELD IS STILL GROWING
;   IF PLAGRO IS 0,THEN STAR FIELD IS DEACTIVATED
;   OUTPUT:IF PLAGRO IS 0,THEN STAR FIELD IS COMPLETELY DEAD
;------------------------------------------------------------------------------
PRSTAR:
LA7D2:  LDA  PLAGRO                 ;STAR FIELD ACTIVE?
LA7D5:  BEQ  LA830                  ;IFNE
LA7D7:  LDA  #$00                   ;YES. PROCESS PLANES
LA7D9:  STA  TEMP0                  ;CLEAR COUNT OF ACTIVE PLANES
LA7DB:  LDX  #NPLANE-1
LA7DD:  STX  INDEX1
LA7DF:  LDX  INDEX1
LA7E1:  LDA  PLANEY,X
LA7E4:  BEQ  LA7FE                  ;IFNE  PLANE ACTIVE?
LA7E6:  SEC                         ;YES.
LA7E7:  SBC  #$07                   ;UPDATE PLANE POSITION
LA7E9:  BCC  LA7ED                  ;IFCS
LA7EB:  CMP  #$10
LA7ED:  BCS  LA7FB                  ;IFCC  TOO CLOSE?
LA7EF:  LDY  PLAGRO                 ;YES
LA7F2:  BPL  LA7F9                  ;IFMI  STILL GROWING?
LA7F4:  LDA  #$F0                   ;YES. START AT FARTHEST POINT
LA7F6:  CLV                         ;ELSE
LA7F7:  BVC  LA7FB
LA7F9:  LDA  #$00                   ;NO. DEACTIVATE
LA7FB:  CLV                         ;ELSE
LA7FC:  BVC  LA81E
;[CS] Start of ROM 136002.316 at $A800.	   NOTE: VERSION 3 ROMS, FOLKS.
LA7FE:  LDY  PLAGRO                 ;NO. STILL GROWING?
LA801:  BPL  LA81E                  ;IFMI
LA803:  TXA                         ;YES.
LA804:  CLC
LA805:  ADC  #$01                   ;GET INDEX OF PREVIOUS PLANE
LA807:  CMP  #NPLANE
LA809:  BCC  LA80D                  ;IFCS
LA80B:  LDA  #$00
LA80D:  TAY
LA80E:  LDA  PLANEY,Y               ;PREVIOUS PLANE ACTIVE?
LA811:  BEQ  LA81E                  ;IFNE
LA813:  CMP  #$D5                   ;YES.
LA815:  BCS  LA81C                  ;IFCC  IS PREVIOUS PLANE CLOSE ENOUGH?
LA817:  LDA  #$F0                   ;YES. START NEW PLANE
LA819:  CLV                         ;ELSE
LA81A:  BVC  LA81E
LA81C:  LDA  #$00                   ;NO. STILL INACTIVE
LA81E:  STA  PLANEY,X
LA821:  ORA  TEMP0
LA823:  STA  TEMP0
LA825:  DEC  INDEX1
LA827:  BPL  LA7DF                  ;MIEND
LA829:  LDA  TEMP0
LA82B:  BNE  LA830                  ;IFEQ
LA82D:  STA  PLAGRO
LA830:  RTS

;------------------------------------------------------------------------------
; INISUZ - INITIALIZE SUPER ZAP
;   [CS] Superzapper routines begin here
;------------------------------------------------------------------------------
INISUZ:
LA831:  LDA  #$00                   ;SET SUPZAP USE COUNTER AND TIMER TO 0.  [CS] Clear out the number of times
LA833:  STA  SUZCNT                 ;[CS] the zapper has been used and
LA836:  STA  SUZTIM                 ;[CS] stop any zapper session that
LA839:  RTS                         ;[CS] is currently running.

;------------------------------------------------------------------------------
; PROSUZ - PROCESS SUPER ZAPPER
;   [CS] Main superzapper logic
;------------------------------------------------------------------------------
PROSUZ:
LA83A:  LDA  QSTATUS
LA83C:  BPL  LA87C                  ;IFMI  ATTRACT?
LA83E:  LDA  SUZTIM                 ;NO  [CS] If the superzapper is running,
LA841:  BNE  LA866                  ;IFEQ  ZAP ACTIVE?  [CS] skip to the code which handles the duration count incrementing and checking.
LA843:  LDA  CURSL2                 ;NO.  [CS] If the player is dead (highest
LA846:  BMI  LA863                  ;IFPL  CURSOR ALIVE?  [CS] bit set) this skip this.
LA848:  LDA  SWFINA                 ;YES  [CS] If they have not pressed the
LA84A:  AND  #MSUZA                 ;[CS] zapper button, then there is
LA84C:  BEQ  LA863                  ;IFNE  ZAP PRESSED?  [CS] nothing to do.
LA84E:  LDA  SUZCNT                 ;YES.  [CS] If the zapper buttons has been
LA851:  CMP  #CSUMAX                ;[CS] used twice already, clear and
LA853:  BCS  LA85D                  ;IFCC  ZAPS LEFT?  [CS] then ignore the request.
LA855:  INC  SUZCNT                 ;YES. UPDATE ZAP COUNTER  [CS] Increase the counter for the #
LA858:  LDA  #$01                   ;[CS] of times the zapper has been
LA85A:  STA  SUZTIM                 ;START ZAP TIMER  [CS] used. Start the counter which is used to control the duration of a superzapper event.
LA85D:  LDA  SWFINA                 ;[CS] Clear the zapper from the list
LA85F:  AND  #<[~[MSUZA|MFAKE]]     ;[CS] of button events which need
LA861:  STA  SWFINA                 ;[CS] to he handled.
LA863:  CLV                         ;ELSE
LA864:  BVC  LA87C                  ;[CS] Onto other things.
LA866:  INC  SUZTIM                 ;YES. ZAP ACTIVE  [CS] Increase the counter for the
LA869:  LDX  SUZCNT                 ;[CS] duration the superzapper has been
LA86C:  LDA  SUZTIM                 ;[CS] running. Look up the zapper
LA86F:  CMP  TIMAX,X                ;[CS] run-length table to see if it
LA872:  BCC  LA879                  ;IFCS  ZAP TIMER EXPIRED?  [CS] is time to end the current zapper session. (The table at A884 and A885 control the length of time the zapper runs for the first use and the second use. Branch if it isn't time to end yet.
LA874:  LDA  #$00                   ;[CS] End the current zapper session by
LA876:  STA  SUZTIM                 ;YES. DEACTIVATE ZAP  [CS] setting the run timer to zero.
LA879:  JSR  KILENE                 ;WIPE OUT INVADERS & CHARGES
LA87C:  LDA  SWFINA
LA87E:  AND  #<[~MFAKE]
LA880:  STA  SWFINA                 ;CLEAR "SWITCH NOT PROCESSED" FLAG
LA882:  RTS

TIMAX:
LA883:  .byte $00, CSUSTA+[8*[CSUINT+1]], CSUSTA+[1*[CSUINT+1]], $00, $00

;------------------------------------------------------------------------------
; KILENE - SUPER ZAP-WIPE OUT ENEMY
;------------------------------------------------------------------------------
KILENE:
LA888:  LDA  SUZTIM
LA88B:  CMP  #CSUSTA
LA88D:  BCC  LA8A3                  ;IFCS
LA88F:  AND  #CSUINT
LA891:  BNE  LA8A3                  ;IFEQ  TIME FOR ANOTHER WIPE OUT?
LA893:  LDY  WINVMX                 ;YES.
LA896:  LDA  INVAY,Y
LA899:  BNE  EXIKIL                 ;SPECIAL EXIT FOR 1ST LIVE ONE
LA89B:  DEY                         ;EXIT LOOP IF ALL ARE DEACTIVE
LA89C:  BPL  LA896                  ;MIEND
LA89E:  LDA  #$00                   ;ALL ARE DEAD. DEACTIVATE ZAP
LA8A0:  STA  SUZTIM
LA8A3:  RTS

;------------------------------------------------------------------------------
; EXIKIL - [note] Kill enemy Y: clear its carrier bit (INVCAR in INVAC2) and start its explosion (JMP INCISQ).
;------------------------------------------------------------------------------
EXIKIL:
LA8A4:  LDA  INVAC2,Y               ;MAKE SURE IT'S NOT A CARRIER
LA8A7:  AND  #<[~INVCAR]
LA8A9:  STA  INVAC2,Y
LA8AC:  JMP  INCISQ                 ;START EXPLOSION

CHKSM5:
LA8AF:  .byte QCHKS5

;==============================================================================
; MODULE ALSCO2   ALSCO2.MAC
;   Score module: score update, bonus lives, high-score table and initials
;   entry, credits/info display, player-switch and game-over logic.
;==============================================================================

;.SBTTL INFO DISPLAY-MESSAGES

TCOMOD:
;INPUT: MSGREQ: BITS SET TO REQUEST A MSG
;OUTPUT: ACC,X,Y,SAVEX DESTROYED
LA8B0:  .byte MCMODE, MCMOD1, MCMOD2, MCMOD3

;------------------------------------------------------------------------------
; INFO - DISPLAY SCORE & LIVES INFO  (comment at the calls)
;   DISPLAY ALL INFO  (another call)
;------------------------------------------------------------------------------
INFO:
LA8B4:  LDA  #$01
LA8B6:  STA  VGSIZE
LA8B8:  JSR  VGSCA1
LA8BB:  LDY  #LETCOL                ;STANDARD LETTER COLOR
LA8BD:  JSR  NWCOLO
LA8C0:  LDA  QSTATUS
LA8C2:  BMI  LA8EA                  ;IFPL  ATTRACT?
LA8C4:  LDX  #MGAMOV                ;YES. "GAME OVER"
LA8C6:  LDA  QFRAME
LA8C8:  AND  #$20
LA8CA:  BNE  LA8D8                  ;IFEQ
LA8CC:  LDX  #MINSER                ;FLASH INSERT COINS
LA8CE:  LDA  S_S_CRDT               ;[CS] Are there any game credits?
LA8D0:  BEQ  LA8D8                  ;IFNE
LA8D2:  BIT  TCMFLG
LA8D4:  BMI  LA8D8                  ;IFPL  2 GAME MINIMUM?
LA8D6:  LDX  #MPRESS                ;NO. PRESS START
LA8D8:  JSR  MSGS
LA8DB:  JSR  VGCNTR_AB0D
LA8DE:  LDA  VGMSGA                 ;BLANK OUT LEVEL  [CS] Read something from Vector
LA8E1:  STA  SCLEVEL                ;[CS] ROM and put it in Vector RAM
LA8E4:  STA  SCLEVEL+2

HACKER:
LA8E7:  JSR  DSPCRD                 ;DISPLAY CREDITS & ATARI

;.SBTTL INFO DISPLAY-LIVES, SCORES
LA8EA:  LDA  #$01                   ;(SCALE)
LA8EC:  LDY  #$00                   ;(PLAYER ID)
LA8EE:  JSR  UPSCLI                 ;PLAYER 1 DATA
LA8F1:  BIT  QSTATUS
LA8F3:  BMI  LA8FE                  ;IFPL  ATTRACT?
LA8F5:  LDA  RSCORL                 ;YES. DISPLAY P2 SCORE IF NOT 0
LA8F7:  ORA  RSCORM
LA8F9:  ORA  RSCORH
LA8FB:  CLV                         ;ELSE
LA8FC:  BVC  LA900
LA8FE:  LDA  NUMPLA                 ;NO. GAME MODE. 2 PLAYERS?
LA900:  BEQ  LA908                  ;IFNE
LA902:  LDA  #$01                   ;YES. DISPLAY PLAYER 2 DATA
LA904:  TAY                         ;(SCALE 1, PLAYER 1)
LA905:  JSR  UPSCLI
LA908:  LDA  QSTATE                 ;[CS] Are we in Self-test screen with
LA90A:  CMP  #CPLAY                 ;PLAY STATE?  [CS] diagional lines and character set?
LA90C:  BEQ  LA943                  ;IFNE
LA90E:  LDA  #[[HSCORL+21]&$FF]+2   ;NO. SET UP FOR HI SCORE UPDATE
LA910:  STA  INDYLO
LA912:  LDA  #[[HSCORL+21]+2]/$100
LA914:  STA  INDYLO+1
LA916:  LDX  HISLOC                 ;INDEX INTO TEMPLATE
LA919:  JSR  NWDIGS

ZATC4V:
LA91C:  LDY  #ZATC4C                ;VERIFY CALL TO ATARI LITERAL
LA91E:  LDA  #$A7
LA920:  EOR  ZATC4S,Y
LA923:  DEY
LA924:  BPL  LA920                  ;MIEND
LA926:  STA  QT2
LA929:  LDX  HIILOC
LA92C:  LDA  #$02                   ;INITIALS COUNTER
LA92E:  STA  INDEX2
LA930:  LDY  INDEX2
LA932:  LDA  INITAL+[[3*NHISCO]-3],Y ;GET INITIAL
LA935:  ASL
LA936:  TAY
LA937:  LDA  VGMSGA+22,Y            ;GET LSB OF JSRL
LA93A:  STA  SCOBUF,X               ;UPDATE TEMPLATE
LA93D:  INX
LA93E:  INX
LA93F:  DEC  INDEX2
LA941:  BPL  LA930                  ;MIEND
LA943:  LDA  #>[SCOBUF+1]           ;LDAH SCOBUF+1  INSERT JSRL TO INFO BUFFER
LA945:  LDX  #<SCOBUF               ;LXL SCOBUF
LA947:  JSR  VGJSRL
LA94A:  LDA  ELICNT
LA94D:  BPL  LA954                  ;IFMI  WARNING?
LA94F:  LDX  #MSPIKE                ;YES. AVOID SPIKES
LA951:  JSR  MSGS

;.SBTTL STARFIELD MESSAGES
LA954:  LDA  QSTATE
LA956:  CMP  #CNEWV2
LA958:  BNE  LA97C                  ;IFEQ
LA95A:  LDA  QSTATUS
LA95C:  BPL  LA97C                  ;IFMI  ATTRACT?
LA95E:  LDX  PLAYUP                 ;NO.  [CS] Did the user choose to start
LA960:  LDA  BONUS,X                ;[CS] out on the first level?
LA963:  BEQ  LA972                  ;IFNE  DISPLAY BONUS?  [CS] If so, skip a bit.
LA965:  LDX  #MBONPT                ;YES.
LA967:  JSR  MSGS
LA96A:  LDY  PLAYUP                 ;[CS] Load into Y reg the choice #
LA96C:  LDX  BONUS,Y                ;[CS] that the player started on. NOTE: This is the choice #, not the actual level #.
LA96F:  JSR  BODSPL
LA972:  LDX  #MSUPZA                ;"SUPER ZAPPER RECHARGED"
LA974:  JSR  MSGS
LA977:  LDX  #MAPROA
LA979:  JSR  MSGS                   ;LEVEL LITERAL
LA97C:  RTS

;.SBTTL UPDATE PLAYER'S SCORE, LIVES, SCALE

SCOSOL:
LA97D:  .byte LSCORL+2, RSCORL+2    ;SCORE LOCATIONS (PLAYER 0,1)

;------------------------------------------------------------------------------
; UPSCLI - PLAYER 1 DATA  (comment at the call)
;------------------------------------------------------------------------------
UPSCLI:
LA97F:  LDX  QSTATE                 ;UPDATE SCORE SCALE, LIVES
LA981:  CPX  #CPLAY                 ;Y=PLAYER ID
LA983:  STY  TEMP2                  ;ACC=PROPOSED BINARY SCALE
LA985:  CPY  PLAYUP                 ;NO. UPDATE LIVES & SCALE (146 MICROSECONDS)
LA987:  BNE  LA98F                  ;IFEQ  PLAYER UP?
LA989:  BIT  QSTATUS                ;YES.
LA98B:  BPL  LA98F                  ;IFMI  ATTRACT?
LA98D:  LDA  #$00                   ;NO. BIG SCORE
;ACC=SCALE (0 OR 1)
LA98F:  ORA  #$70
LA991:  LDX  SCALOC,Y
LA994:  STA  SCOBUF,X               ;UPDATE SCALE
LA997:  LDX  LIVLOC,Y
LA99A:  .byte $B9, $48, $00         ;LDA LIVES1,Y (forced absolute)
LA99D:  STA  INDEX2
LA99F:  BEQ  LA9A7                  ;IFNE
LA9A1:  CPY  PLAYUP
LA9A3:  BNE  LA9A7                  ;IFEQ  PLAYER UP BEING DISPLAYED?
LA9A5:  DEC  INDEX2                 ;YES. TAKE 1 LIFE FOR CURSOR
LA9A7:  LDY  #$01
LA9A9:  LDA  LSYMBL                 ;DEFAULT LIFE PIC
LA9AC:  CPY  INDEX2
LA9AE:  BCC  LA9B5                  ;IFCS  NO LIFE?
LA9B0:  BEQ  LA9B5                  ;IFNE
LA9B2:  LDA  LSYMB0                 ;NO LIFE. BLANK PIC
LA9B5:  STA  SCOBUF,X
LA9B8:  INX
LA9B9:  INX
LA9BA:  INY
LA9BB:  CPY  #$07
LA9BD:  BCC  LA9A9                  ;CSEND
LA9BF:  LDY  TEMP2
LA9C1:  LDA  QSTATE                 ;[CS] Are we in self-test screen with
LA9C3:  CMP  #CPLAY                 ;IF NOT PLAYING THEN  [CS] diagional lines and character set?
LA9C5:  BNE  ANYWAY                 ;ALWAYS UPDATE BOTH SCORES
LA9C7:  CPY  PLAYUP
LA9C9:  BNE  LA9FB                  ;IFEQ  PLAYER UP?

ANYWAY:
LA9CB:  LDX  SCOLOC,Y               ;YES.
LA9CE:  LDA  SCOSOL,Y
LA9D1:  STA  INDYLO
LA9D3:  LDA  #$00
LA9D5:  STA  INDYHI

;------------------------------------------------------------------------------
; NWDIGS - DISPLAY 6 BCD DIGITS W. ZERO SUP.
;   INDYLOC(2)=LOC OF 3 BYTES
;   CONTAINING 6 BCD DIGITS
;   X=DESTINATION INDEX FROM SCOBUF
;------------------------------------------------------------------------------
NWDIGS:
LA9D7:  LDY  #$02                   ;BYTE COUNTER
LA9D9:  STY  TEMP1
LA9DB:  SEC
LA9DC:  PHP
LA9DD:  LDY  #$00
LA9DF:  LDA  (INDYLO),Y             ;DISPLAY HIGH NIBBLE
LA9E1:  LSR
LA9E2:  LSR
LA9E3:  LSR
LA9E4:  LSR
LA9E5:  PLP
LA9E6:  JSR  NWHEXZ
LA9E9:  LDA  TEMP1
LA9EB:  BNE  LA9EE                  ;IFEQ
LA9ED:  CLC                         ;ALWAYS DISPLAY LAST DIGIT
LA9EE:  LDY  #$00
LA9F0:  LDA  (INDYLO),Y             ;NOW DO LOW NIBBLE
LA9F2:  JSR  NWHEXZ
LA9F5:  DEC  INDYLO
LA9F7:  DEC  TEMP1
LA9F9:  BPL  LA9DC                  ;MIEND
LA9FB:  RTS

;------------------------------------------------------------------------------
; NWHEXZ - DISPLAY 1 BCD DIGIT WITH ZERO SUPR.
;------------------------------------------------------------------------------
NWHEXZ:
LA9FC:  AND  #$0F                   ;ISOLATE 1 BCD DIGIT
LA9FE:  TAY
LA9FF:  BEQ  LAA02                  ;IFNE  NON ZERO?
LAA01:  CLC                         ;YES. CLEAR 0 SUPPRESSOR
LAA02:  BCS  LAA05                  ;IFCC  ZERO SUPPRESS TO ZERO?
LAA04:  INY                         ;NO. DISPLAY ZERO
LAA05:  PHP
LAA06:  TYA
LAA07:  ASL                         ;*2 TO GET VGMSGA OFFSET
LAA08:  TAY
LAA09:  LDA  VGMSGA,Y               ;GET LSB OF CHARACTER'S JSRL
LAA0C:  STA  SCOBUF,X               ;UPDATE TEMPLATE
LAA0F:  INX
LAA10:  INX
LAA11:  PLP
LAA12:  RTS

;------------------------------------------------------------------------------
; INITEM - INITIALIZE SCORE/LIVES TEMPLATE
;   INITIALIZE RAM SCORE DISPLAY TEMPLATE
;------------------------------------------------------------------------------
INITEM:
LAA13:  LDX  NUMPLA
LAA15:  BIT  QSTATUS
LAA17:  BMI  LAA23                  ;IFPL  ATTRACT?
LAA19:  LDA  RSCORL                 ;YES.
LAA1B:  ORA  RSCORM
LAA1D:  ORA  RSCORH
LAA1F:  BEQ  LAA23                  ;IFNE
LAA21:  LDX  #$01                   ;DISPLAY PLAYER 2 SCORE IF NOT 0
LAA23:  LDA  #<SCOBUF               ;LDAL SCOBUF
LAA25:  STA  VGLIST
LAA27:  LDA  #>[SCOBUF+1]           ;LDAH SCOBUF+1
LAA29:  STA  VGLIST+1
LAA2B:  LDA  SCECOU,X
LAA2E:  TAY
LAA2F:  SEC
LAA30:  ADC  VGLIST
LAA32:  PHA
LAA33:  LDA  SCORES,Y
LAA36:  STA  (VGLIST),Y             ;COPY ROM TEMPLATE INTO RAM
LAA38:  DEY
LAA39:  BNE  LAA33                  ;EQEND
LAA3B:  LDA  SCORES,Y
LAA3E:  STA  (VGLIST),Y
LAA40:  LDA  QSTATUS
LAA42:  BPL  LAA54                  ;IFMI  ATTRACT?
LAA44:  LDA  #>[SCLEVEL+1]          ;LDAH SCLEVEL+1
LAA46:  STA  VGLIST+1
LAA48:  LDA  #<SCLEVEL              ;LDAL SCLEVEL
LAA4A:  STA  VGLIST                 ;POINT AT LEVEL JSRL
LAA4C:  LDA  CURWAV
LAA4E:  CLC
LAA4F:  ADC  #$01
LAA51:  JSR  DSP1HX
LAA54:  PLA
LAA55:  STA  VGLIST
LAA57:  JMP  VGRTSL                 ;PUT RTSL INTO SCORE BUFFER

;------------------------------------------------------------------------------
; DPLPLA - INFO ONLY - GAME OVER & PLAY PLAYER
;------------------------------------------------------------------------------
DPLPLA:
LAA5A:  LDX  #MPLAY                 ;"PLAY"
LAA5C:  JSR  MSGS
LAA5F:  JMP  GENPLA

;------------------------------------------------------------------------------
; DGOVER - [note] Display state: 'GAME OVER' (MGAMOV, special Y position $30) then 'PLAYER n' (GENPLA),
;   ending in JMP HACKER (the rev-3 change from JMP INFO).
;------------------------------------------------------------------------------
DGOVER:
LAA62:  LDA  #$30                   ;SPECIAL COORDINATES
LAA64:  LDX  #MGAMOV                ;"GAME OVER"
LAA66:  JSR  MSGEN3

GENPLA:
LAA69:  JSR  DPLRNO                 ;PLAYER X
LAA6C:  JMP  HACKER

;------------------------------------------------------------------------------
; DPRSTA - [note] Display state: all info (INFO) plus 'PRESS START' in the middle of the screen.
;------------------------------------------------------------------------------
DPRSTA:
LAA6F:  JSR  INFO                   ;DISPLAY ALL INFO
LAA72:  LDA  #$00
LAA74:  LDX  #MPRESS                ;SPECIAL PRESS START IN MIDDLE OF SCREEN
LAA76:  JMP  MSGEN3

;------------------------------------------------------------------------------
; D2GAME - SPECIAL 2 GAME MIN/INSERT COINS IN CENTER
;------------------------------------------------------------------------------
D2GAME:
LAA79:  LDA  #$00
LAA7B:  LDX  #M2GAME                ;2 GAME MINIMUM
LAA7D:  JSR  MSGEN3
LAA80:  LDA  QFRAME
LAA82:  AND  #$1F
LAA84:  CMP  #$10
LAA86:  BCS  LAA8F                  ;IFCC
LAA88:  LDA  #$E0                   ;FLASH INSERT COINS
LAA8A:  LDX  #MINSER
LAA8C:  JSR  MSGEN3
LAA8F:  JMP  INFO

;------------------------------------------------------------------------------
; DPLRNO - PLAYER X  (comment at the calls)
;------------------------------------------------------------------------------
DPLRNO:
LAA92:  LDX  #MPLAYR                ;GENERAL "PLAYER X"
LAA94:  JSR  MSGS

;------------------------------------------------------------------------------
; DPLRX - PLAYER #  (comment at the call)
;------------------------------------------------------------------------------
DPLRX:
LAA97:  LDA  #$00
LAA99:  JSR  NWSCA1                 ;BIG PLAYER #
LAA9C:  LDX  PLAYUP                 ;JUST PLAYER #

;------------------------------------------------------------------------------
; DPLRXX - PLAYER #  (comment at the call)
;------------------------------------------------------------------------------
DPLRXX:
LAA9E:  INX
LAA9F:  STX  SXL
LAAA1:  LDA  #SXL
LAAA3:  LDY  #$01
LAAA5:  JMP  DIGTYS

;------------------------------------------------------------------------------
; DSPCRD - INFOR DISPLAY-CREDITS,COPYRIGHT
;------------------------------------------------------------------------------
DSPCRD:
LAAA8:  LDA  S_CMODE                ;YES
LAAAA:  AND  #$03
LAAAC:  TAX
LAAAD:  LDA  TCOMOD,X
LAAB0:  TAX
LAAB1:  JSR  MSGS                   ;DISPLAY COIN MODE
LAAB4:  DEC  SECUVY
LAAB7:  LDA  OPTIN2
LAAB9:  AND  #OM2GAM
LAABB:  BEQ  DBOLOU                 ;IFNE
LAABD:  LDA  QFRAME
LAABF:  AND  #$20
LAAC1:  BNE  DBOLOU                 ;FLASH EITHER
LAAC3:  LDX  #M2GAME                ;2 GAME MINIMUM
LAAC5:  JSR  MSGS
LAAC8:  CLV                         ;ELSE
LAAC9:  BVC  ZATC4S

;------------------------------------------------------------------------------
; DBOLOU - [note] Show the bonus-life interval (BOLOUT), then fall into the copyright and credits display.
;------------------------------------------------------------------------------
DBOLOU:
LAACB:  JSR  BOLOUT                 ;YES. DISSLAY BONUS LIFE INTERVAL

ZATC4S:
LAACE:  LDX  #MATARI
LAAD0:  JSR  MSGS
LAAD3:  LDX  #MCREDI
LAAD5:  JSR  MSGS                   ;DISPLAY CREDITS

ZATC4E:
LAAD8:  LDA  S_S_CRDT               ;[CS] If the number of game credits
LAADA:  CMP  #$28                   ;[CS] exceed $28 (40), then reduce it
LAADC:  BCC  LAAE2                  ;IFCS  MAXIMIZE # CREDITS TO 40.  [CS] back down to 40.
LAADE:  LDA  #$28
LAAE0:  STA  S_S_CRDT
LAAE2:  JSR  DSP1HX                 ;OUTPUT 2 DIGITS W. ZERO SUPPRESION
LAAE5:  LDA  S_CNCT
LAAE7:  BEQ  LAAF2                  ;IFNE  PARTIAL CREDITS?
LAAE9:  LDA  IHALF+1                ;OUTPUT HALF
LAAEC:  LDX  IHALF
LAAEF:  JSR  VGJSRL
LAAF2:  RTS

IHALF:
LAAF3:  .word HALF

;------------------------------------------------------------------------------
; HEXBCD - HEX TO BCD CONVERSION
;   CONVERT HEX TO BCD (SINGLE PRECISION)
;   INPUT: ACC=HEX
;   OUTPUT: ACC,TEMP0=BCD
;------------------------------------------------------------------------------
HEXBCD:
LAAF5:  SED
LAAF6:  STA  TEMP0
LAAF8:  LDA  #$00
LAAFA:  STA  TEMP3
LAAFC:  LDY  #$07                   ;CONVERT HEX TO BCD
LAAFE:  ASL  TEMP0
LAB00:  LDA  TEMP3
LAB02:  ADC  TEMP3
LAB04:  STA  TEMP3
LAB06:  DEY
LAB07:  BPL  LAAFE                  ;MIEND
LAB09:  CLD
LAB0A:  STA  TEMP0
LAB0C:  RTS

;------------------------------------------------------------------------------
; VGCNTR_AB0D - FAST CENTER
;------------------------------------------------------------------------------
VGCNTR_AB0D:
LAB0D:  LDA  #$20
LAB0F:  LDX  #$80
LAB11:  JMP  VGADD2

;------------------------------------------------------------------------------
; MSGS - MESSAGE ROUTINE
;   BLOCK NAME: MSGS
;   DESCRIPTION: WILL OUTPUT SPECIFIED MESSAGE TO SPECIFIED LOCATION ON SCREEN
;   INPUT PARAMS: (X)=MESSAGE # * 2
;   OUTPUT PARAMS: NONE
;   REGISTERS: A,X,Y
;------------------------------------------------------------------------------
MSGS:
LAB14:  LDA  MSGLBS+1,X

;------------------------------------------------------------------------------
; MSGEN3 - DISPLAY COPYRIGHT LOGO  (comment at the call)
;------------------------------------------------------------------------------
MSGEN3:
LAB17:  STX  SAVEX
LAB19:  STA  TEMP2
LAB1B:  LDY  SAVEX
LAB1D:  LDA  (LITRAL),Y             ;GET LITERAL PTR
LAB1F:  STA  INDYLO
LAB21:  INY
LAB22:  LDA  (LITRAL),Y
LAB24:  STA  INDYHI

ZSECL0:
LAB26:  CPX  #MATARI
LAB28:  BNE  LAB32                  ;IFEQ  COPYRIGHT MSG?
LAB2A:  LDA  VGLIST                 ;YES. SAVE START LOC
LAB2C:  STA  SECUVG
LAB2E:  LDA  VGLIST+1
LAB30:  STA  SECUVG+1
LAB32:  LDY  #$00
LAB34:  LDA  (INDYLO),Y             ;GET HORIZ POSITION FROM LITERAL
LAB36:  STA  TEMP1

MSGENT:
LAB38:  JSR  VGCNTR_AB0D

MSGEN2:
LAB3B:  LDA  #$00
LAB3D:  STA  VGBRIT
LAB3F:  LDA  #$01
LAB41:  STA  VGSIZE
LAB43:  JSR  VGSCA1
LAB46:  LDA  TEMP1
LAB48:  LDX  TEMP2
LAB4A:  JSR  VGVTR1                 ;C POSITION BEAM (USE VGBRIT)

MSGNOP:
LAB4D:  LDY  SAVEX                  ;SET UP PTR TO LITERAL
LAB4F:  LDA  (LITRAL),Y
LAB51:  STA  INDYLO
LAB53:  INY
LAB54:  LDA  (LITRAL),Y
LAB56:  STA  INDYHI
LAB58:  LDX  SAVEX
LAB5A:  LDA  MSGLBS,X
LAB5D:  PHA
LAB5E:  LSR
LAB5F:  LSR
LAB60:  LSR
LAB61:  LSR
LAB62:  TAY
LAB63:  JSR  NWCOLO                 ;SET NEW COLOR
LAB66:  PLA
LAB67:  AND  #$0F
LAB69:  JSR  NWSCA1                 ;SET NEW SCALE
LAB6C:  LDY  #$01
LAB6E:  LDA  #$00                   ;C INIT VGLIST OFFSET
LAB70:  STA  TEMP1
LAB72:  LDA  (INDYLO),Y             ;C GET CHARACTER REPRESENTATION
LAB74:  STA  TEMP2
LAB76:  AND  #$7F
LAB78:  INY
LAB79:  STY  TEMP3                  ;SAVE Y
LAB7B:  TAX
LAB7C:  LDA  VGMSGA,X               ;C GET CORRECT JSRL
LAB7F:  LDY  TEMP1
LAB81:  STA  (VGLIST),Y
LAB83:  INY
LAB84:  LDA  VGMSGA+1,X
LAB87:  STA  (VGLIST),Y
LAB89:  INY
LAB8A:  STY  TEMP1                  ;SAVE Y
LAB8C:  LDY  TEMP3                  ;C GET CHARACTER PTR
LAB8E:  BIT  TEMP2                  ;D IF NOT END OF STRING
LAB90:  BPL  LAB72                  ;MIEND
LAB92:  LDY  TEMP1                  ;C UPDATE VGLIST
LAB94:  DEY
LAB95:  JMP  VGADD

;------------------------------------------------------------------------------
; MSGFOL - NO VERTICAL POSITIONING (Y)
;------------------------------------------------------------------------------
MSGFOL:
LAB98:  STX  SAVEX                  ;INPUT: X=MSG#*2
LAB9A:  STA  TEMP1                  ;ACC=X OFFSET
LAB9C:  LDA  #$00
LAB9E:  STA  TEMP2
LABA0:  BEQ  MSGEN2                 ;ALWAYS

;------------------------------------------------------------------------------
; INICHK - NEW GAME OPTION SETUP & CHECK
;------------------------------------------------------------------------------
INICHK:
LABA2:  JSR  GAMSTA                 ;CHECK FOR GAME PLAY OPTION CHANGE
LABA5:  LDA  EABAD
LABA8:  AND  #$03
LABAA:  BEQ  LAC07                  ;IFNE  CHANGES?

;------------------------------------------------------------------------------
; INIINI - YES. REINITIALIZE SCORES & INITIALS
;------------------------------------------------------------------------------
INIINI:
LABAC:  JSR  GAMSTA                 ;CHECK FOR GAME PLAY OPTION CHANGES

INIIN2:
LABAF:  LDA  #NHISCO                ;START AT 8 RANK GAMES
LABB1:  STA  NGAMES
LABB4:  LDA  HSCORL+[3*NHISCO]-3    ;[CS] Is the highest score 000000?
LABB7:  ORA  HSCORM+[3*NHISCO]-3
LABBA:  ORA  HSCORH+[3*NHISCO]-3
LABBD:  BNE  LABC2                  ;IFEQ  HI SCORES CLEARED?
LABBF:  JSR  INDUCE                 ;YES. INDUCE COMPLETE INITIALIZATION  [CS] If so, JSR to $AC36.
;INITIALIZE INITIALS
LABC2:  LDX  #[3*NHISCO]-1          ;DEFAULT TO BAD INITIALS
LABC4:  LDA  EABAD
LABC7:  AND  #$01
LABC9:  BNE  LABCD                  ;IFEQ  INITIALS VALID?
LABCB:  LDX  #[3*NHISCO]-1-9        ;YES. DON'T WRITE OVER TOP 3  [CS] Load from ROM the initials of
LABCD:  LDA  SCOINI,X               ;[CS] the bottom 5 high score into the
LABD0:  STA  INITAL,X               ;[CS] high score list.
LABD3:  DEX
LABD4:  BPL  LABCD                  ;MIEND
;INITIALIZE HI SCORES
LABD6:  LDX  #[3*NHISCO]-1          ;DEFAULT TO BAD HI SCORE
LABD8:  LDA  EABAD
LABDB:  AND  #$02
LABDD:  BNE  LABE1                  ;IFEQ  HI SCORES VALID?
LABDF:  LDX  #[3*NHISCO]-1-9        ;YES. DON'T WRITE OVER TOP 3  [CS] Set the last five high scores
LABE1:  LDA  #$01                   ;[CS] to "10101".
LABE3:  STA  HSCORL,X
LABE6:  DEX
LABE7:  BPL  LABE1                  ;MIEND
LABE9:  LDA  EABAD
LABEC:  AND  #$03
LABEE:  BEQ  LABFF                  ;IFNE  REINITIALIZING?
LABF0:  LDA  OPTIN2                 ;YES. SET UP GAME PLAY OPTIONS
LABF2:  AND  #$F8
LABF4:  STA  GAMOP1
LABF7:  LDA  OPTIN3
LABFA:  AND  #$03
LABFC:  STA  GAMOP3
LABFF:  LDA  EABAD                  ;CLEAR BAD READ FLAG
LAC02:  AND  #$FC
LAC04:  STA  EABAD
LAC07:  RTS

SCOINI:
LAC08:  .byte $07, $04, $01, $0F, $09, $0C, $0B, $03 ;.ASCVG <HEBPJMLDSTFDHPMRRRSEDDJE>  [CS] BEH	Initials to place in the the MJP	lowest five slots of the high SDL	high score table. (Always done
LAC10:  .byte $12, $13, $05, $03, $07, $0F, $0C, $11 ;[CS] DFT	upon power-up.) MPH	0=A 1=B 2=C ... 24=Y 25=Z 26=SPACE RRR	Top three high scores to be used
LAC18:  .byte $11, $11, $12, $04, $03, $03, $09, $04 ;[CS] DES	when the EAROM's high score list has EJD	been zero'd out. Fake command or not used?

;------------------------------------------------------------------------------
; GAMSTA - CHECK FOR GAME PLAY OPTION CHANGE
;------------------------------------------------------------------------------
GAMSTA:
LAC20:  JSR  INILIT                 ;READ OPTIONS
LAC23:  LDA  OPTIN2
LAC25:  AND  #$F8
LAC27:  CMP  GAMOP1
LAC2A:  BNE  LAC34                  ;IFEQ
LAC2C:  LDA  OPTIN3
LAC2F:  AND  #$03
LAC31:  CMP  GAMOP3
LAC34:  BEQ  LAC3E                  ;IFNE  NEW GAME PLAY OPTIONS?

;------------------------------------------------------------------------------
; INDUCE - YES. INDUCE COMPLETE INITIALIZATION  (comment at the call)
;------------------------------------------------------------------------------
INDUCE:
LAC36:  LDA  EABAD                  ;YES. INDUCE REINITIALIZATION
LAC39:  ORA  #$03                   ;OF SCORES & INITIALS
LAC3B:  STA  EABAD
LAC3E:  RTS

;------------------------------------------------------------------------------
; HISCHK - HISCHK:HIGH SCORE DETECTION
;   EXTERNAL ENTRY POINT
;------------------------------------------------------------------------------
HISCHK:
LAC3F:  LDA  QSTATUS
LAC41:  AND  #<[~MGTMOD]
LAC43:  STA  QSTATUS                ;OUT OF GAME TIME MODE
LAC45:  LDA  OPTIN1
LAC47:  AND  #$43
LAC49:  CMP  #$40
LAC4B:  BNE  LAC50                  ;IFEQ  SCALES MODE?
LAC4D:  JSR  CLRSCO                 ;YES. CLEAR SCORES  [CS] Clear out the player 1&2 scores.
LAC50:  JSR  WRBOOK                 ;WRITE OUT BOOKKEEPING INFO
LAC53:  LDA  #$00
LAC55:  STA  RANKS+1                ;NO RANK FOR PLAYER 2 IN CASE 1 PLAY OR GAME
LAC58:  LDX  NUMPLA
LAC5A:  BEQ  LAC5E                  ;IFNE  2 PLAYER GAME?
LAC5C:  LDX  #$03                   ;YES. START WITH PLAYER 2
LAC5E:  LDA  LSCORH,X
LAC60:  STA  TEMP3
LAC62:  LDA  LSCORM,X
LAC64:  STA  TEMP4
LAC66:  LDA  LSCORL,X
LAC68:  STA  TEMPX                  ;SET UP PLAYER'S SCORE
LAC6A:  TXA
LAC6B:  AND  #$01
LAC6D:  STA  SAVEY
LAC6F:  LDA  #$00
LAC71:  STA  TEMP2
LAC73:  LDA  #CBLANK
LAC75:  STA  TEMP1
LAC77:  STA  TEMP0                  ;SET UP INITIALS (A BLANK BLANK)
LAC79:  LDA  #$00
LAC7B:  STA  TIMHIS                 ;INITIALIZE RANK
LAC7E:  LDY  #$FD
;UNTIL PLAYERS SCORE > SCORE IN TABLE
LAC80:  LDA  HRANKH,Y
LAC83:  CMP  TEMP3
LAC85:  BNE  LAC9B                  ;IFEQ  COMPARE SCORE TO TABLE ENTRY
LAC87:  LDA  HRANKM,Y
LAC8A:  CMP  TEMP4
LAC8C:  BNE  LAC9B                  ;IFEQ
LAC8E:  CPY  #$52
LAC90:  BCC  LAC9A                  ;IFCS  PRECISION?
LAC92:  LDA  HRANKL,Y               ;TRIPLE
LAC95:  CMP  TEMPX
LAC97:  CLV                         ;ELSE
LAC98:  BVC  LAC9B
LAC9A:  SEC                         ;DOUBLE
LAC9B:  BCS  LACEC                  ;IFCC  PLAYER'S SCORE > TABLE ENTRY?
LAC9D:  CPY  #$E8
LAC9F:  BCC  LACBF                  ;IFCS  HI SCORE TABLE?
LACA1:  LDA  TEMP0                  ;YES. MOVE INITIALS DOWN
LACA3:  LDX  INITAL-232,Y           ;[CS] High score sort routine
LACA6:  STA  INITAL-232,Y           ;[CS] for scores and initials.
LACA9:  STX  TEMP0                  ;[CS] Starts here and extends down.
LACAB:  LDA  TEMP1
LACAD:  LDX  INITAL+1-232,Y
LACB0:  STA  INITAL+1-232,Y
LACB3:  STX  TEMP1
LACB5:  LDA  TEMP2
LACB7:  LDX  INITAL+2-232,Y
LACBA:  STA  INITAL+2-232,Y
LACBD:  STX  TEMP2
LACBF:  LDA  TEMP4                  ;MOVE SCORES DOWN
LACC1:  LDX  HRANKM,Y
LACC4:  STA  HRANKM,Y
LACC7:  STX  TEMP4
LACC9:  LDA  TEMP3
LACCB:  LDX  HRANKH,Y
LACCE:  STA  HRANKH,Y
LACD1:  STX  TEMP3
LACD3:  CPY  #$52
LACD5:  BCC  LACE1                  ;IFCS  TRIPLE PRECISION?
LACD7:  LDA  TEMPX
LACD9:  LDX  HRANKL,Y
LACDC:  STA  HRANKL,Y
LACDF:  STX  TEMPX
LACE1:  CPY  #$55                   ;TRIPLE PRECISION?
LACE3:  BCC  LACE6                  ;IFCS
LACE5:  DEY                         ;YES.
LACE6:  DEY
LACE7:  DEY
LACE8:  BNE  LAC9D                  ;EQEND
LACEA:  LDY  #$02                   ;ABORT OUTER LOOP
LACEC:  INC  TIMHIS                 ;UPDATE RANK
LACEF:  CPY  #$55
LACF1:  BCC  LACF4                  ;IFCS  TRIPLE PRECISION?
LACF3:  DEY                         ;YES.
LACF4:  DEY
LACF5:  DEY
LACF6:  BNE  LAC80                  ;EQEND
LACF8:  LDX  SAVEY
LACFA:  LDA  TIMHIS
LACFD:  STA  RANKS,X                ;UPDATE PLAYER'S RANK
LAD00:  DEX
LAD01:  BMI  LAD06                  ;MIEND
LAD03:  JMP  LAC5E
LAD06:  LDA  RANKS+1
LAD09:  CMP  RANKS
LAD0C:  BCC  LAD15                  ;IFCS  BOTH RANKS SAME?
LAD0E:  CMP  #NRANKS                ;YES. MAKE PLAYER 2 LOW
LAD10:  BCS  LAD15                  ;IFCC
LAD12:  INC  RANKS+1
LAD15:  LDA  PLAYUP                 ;SET UP PLAYER ORDER OF FINISH
LAD17:  EOR  #$01                   ;LAST PLAYER UP IN D0,D1
LAD19:  ASL                         ;1ST PLAYER TO DIE IN D2,D3
LAD1A:  ASL
LAD1B:  ORA  PLAYUP
LAD1D:  ADC  #$05
LAD1F:  STA  FLGNHI

;------------------------------------------------------------------------------
; INTLDR - HISCORE INITIALS PREP
;   JSR INTLDR ;GO SEE IF ANYBODY MADE IT
;------------------------------------------------------------------------------
INTLDR:
LAD22:  LDY  #CNOTFOU               ;DEFAULT IS FAILURE
LAD24:  LDA  FLGNHI
LAD27:  BEQ  LAD6B                  ;IFNE  MORE HI SCORES?
LAD29:  AND  #$03                   ;YES.
LAD2B:  STA  PLAYUP                 ;SET UP PLAYER ID
LAD2D:  DEC  PLAYUP
LAD2F:  LSR  FLGNHI
LAD32:  LSR  FLGNHI
LAD35:  LDX  PLAYUP
LAD37:  LDA  RANKS,X
LAD3A:  BEQ  LAD68                  ;IFNE
LAD3C:  CMP  #$09
LAD3E:  BCS  LAD68                  ;IFCC  PLAYER GET HI SCORE?
LAD40:  ASL                         ;YES.
LAD41:  CLC
LAD42:  ADC  RANKS,X
LAD45:  EOR  #$FF
LAD47:  SEC
LAD48:  SBC  #$E5                   ;(src: SBC I,232.-3)
LAD4A:  STA  TBLIND                 ;SET INDEX TO PT TO MS LETTER
LAD4D:  JSR  COCFLI                 ;COCKTAIL FLIP
LAD50:  LDA  #ITIMHI                ;[CS] Set high score entry timer
LAD52:  STA  TIMHIS                 ;SET TIMER  [CS] to $60 seconds. Yeah. In HEX.
LAD55:  LDA  #$00                   ;CLEAR SWITCHES  [CS] Reset the list of buttons to
LAD57:  STA  SWFINA                 ;[CS] process and the relative change
LAD59:  STA  TBHD                   ;[CS] in spinner position.
LAD5B:  LDA  #$02
LAD5D:  STA  ININDX                 ;SET UP INITIAL COUNTER
LAD60:  JSR  INBOOM                 ;INITIALIZE BOOM STATE (SUCCESS)
LAD63:  LDY  #CBOOM                 ;[CS] Set game mode to the post-game
LAD65:  STY  QSTATE                 ;[CS] "victory explosion" for high score
LAD67:  RTS
LAD68:  JMP  INTLDR                 ;TRY AGAIN
LAD6B:  STY  QSTATE
LAD6D:  RTS

;------------------------------------------------------------------------------
; GETINI - GETINI:GET INITIALS
;   ENTERNAL ENTRY POINT
;------------------------------------------------------------------------------
GETINI:
LAD6E:  LDA  #CDGETI                ;GET INIT DISPLAY STATE  [CS] Set game mode to the high score
LAD70:  STA  QDSTATE                ;[CS] input screen.
LAD72:  LDA  QFRAME
LAD74:  AND  #$1F
LAD76:  BNE  LAD82                  ;IFEQ  TIME TO UPDATE TIMER?
LAD78:  DEC  TIMHIS                 ;YES  [CS] Decrease the timer.
LAD7B:  BNE  LAD82                  ;IFEQ  TIME UP?
LAD7D:  LDY  #CNOTFOU               ;YES. ABORT
LAD7F:  STY  QSTATE
LAD81:  RTS
LAD82:  LDX  TBLIND
LAD85:  LDA  INITAL,X               ;GET LETTER BEING CHANGED
LAD88:  JSR  GINICO                 ;CHANGE IT
LAD8B:  TAY
LAD8C:  BPL  LAD93                  ;IFMI
LAD8E:  LDA  #$1A
LAD90:  CLV                         ;ELSE
LAD91:  BVC  LAD99
LAD93:  CMP  #$1B
LAD95:  BCC  LAD99                  ;IFCS
LAD97:  LDA  #$00
LAD99:  LDX  TBLIND
LAD9C:  STA  INITAL,X               ;SAVE IT
LAD9F:  LDA  SWFINA
LADA1:  AND  #MFIRE|MSUZA
LADA3:  TAY
LADA4:  LDA  SWFINA
LADA6:  AND  #<[~[MFIRE|MSUZA|MFAKE]]
LADA8:  STA  SWFINA
LADAA:  TYA
LADAB:  BEQ  LADCD                  ;IFNE  USE THIS LETTER?
;YES
LADAD:  DEC  TBLIND                 ;UPDATE INITIAL INDEX
LADB0:  DEC  ININDX
LADB3:  BPL  LADC7                  ;IFMI  ALL DONE WITH PLAYER?
LADB5:  LDX  PLAYUP                 ;YES.
LADB7:  LDA  RANKS,X
LADBA:  CMP  #$04
LADBC:  BCS  LADC1                  ;IFCC  SAVE IN EAROM?
LADBE:  JSR  WRHIIN                 ;YES. UPDATE EAROM
LADC1:  JSR  INTLDR                 ;YES. SEE IF OTHER PLAYER GO ON.
LADC4:  CLV                         ;ELSE
LADC5:  BVC  LADCD
LADC7:  DEX
LADC8:  LDA  #$00                   ;START AT A
LADCA:  STA  INITAL,X
LADCD:  RTS

;------------------------------------------------------------------------------
; GINICO - USER SUBROUTINES FOR I/O
;------------------------------------------------------------------------------
GINICO:
LADCE:  PHA                         ;RETURN WITH ACC INCD OR DECD
LADCF:  LDA  TBHD
LADD1:  ASL                         ;SCALE POT READING
LADD2:  ASL
LADD3:  ASL
LADD4:  CLC
LADD5:  ADC  CURSPO
LADD7:  STA  CURSPO
LADD9:  PLA                         ;GET CURRENT INITIAL
LADDA:  LDY  TBHD
LADDC:  BMI  LADE3                  ;IFPL  +DIRECTION?
LADDE:  ADC  #$00                   ;YES. ADD IN DIRECTION+CARRY
LADE0:  CLV                         ;ELSE
LADE1:  BVC  LADE5
LADE3:  ADC  #$FF                   ;NO. ADD IN DIRECTION+CARRY
LADE5:  LDY  #$00
LADE7:  STY  TBHD
LADE9:  RTS

;------------------------------------------------------------------------------
; GETDSP - GET INITIALS DISPLAY
;------------------------------------------------------------------------------
GETDSP:
LADEA:  JSR  INFO                   ;DISPLAY SCORE & LIVES INFO
LADED:  LDA  #$C0
LADEF:  LDX  #MPLAYR                ;PLAYER
LADF1:  JSR  MSGEN3
LADF4:  DEC  SECUVY
LADF7:  JSR  DPLRX                  ;PLAYER #
LADFA:  LDX  #MENTER                ;ENTER YOUR INITIALS
LADFC:  JSR  MSGS
LADFF:  LDA  #$A6
LAE01:  LDX  #MPRMOV                ;PRESS MOVE
LAE03:  JSR  MSGEN3

ZATC3S:
LAE06:  LDA  #$9C
LAE08:  LDX  #MPRFIR                ;PRESS FIRE
LAE0A:  JSR  MSGEN3
LAE0D:  LDX  #MATARI
LAE0F:  JSR  MSGS
LAE12:  LDA  TBLIND
LAE15:  SEC
LAE16:  SBC  ININDX                 ;SET GLOW CODE

ZATC3E:
LAE19:  JMP  LDROUT                 ;DISPLAY LADDER

;------------------------------------------------------------------------------
; LDRDSP - DISPLAY HIGH SCORE LADDER
;------------------------------------------------------------------------------
LDRDSP:
LAE1C:  JSR  INFO                   ;DISPLAY SCORE & LIVES INFO

ZPONTS:
;TEST BOTH POKEY'S FOR RANDOM NUMBERS - SEQUENTIALS SHOULD
;HAVE MATCHING NIBBLES
;OUTPUT-ACC=0 IF ALL IS WELL
LAE1F:  SEI                         ;[CS] This is the start of a very clever routine for Tempest Pokey chip protection. The interrupts are disabled to preserve timing. From the POKEY chip emulator 4.5 -- 4.5: changed the 9/17 bit polynomial formulas such that the values required for the Tempest Pokey protection will be found. Tempest expects the upper 4 bits of the RNG to appear in the lower 4 bits after four cycles, so there has to be a shift of 1 per cycle (which was not the case before). Bits #6-#13 of the new RNG give this expected result now, bits #0-7 of the 9 bit poly.
LAE20:  LDA  RANDOM                 ;[CS] Randomize A register.
LAE23:  LDY  RANDOM                 ;[CS] Randomize Y register.
LAE26:  STY  TEMP0
LAE28:  LSR
LAE29:  LSR
LAE2A:  LSR
LAE2B:  LSR
LAE2C:  EOR  TEMP0
LAE2E:  STA  TEMP0
LAE30:  LDA  RANDO2                 ;[CS] Randomize A register.
LAE33:  LDY  RANDO2                 ;[CS] Randomize Y register.
LAE36:  CLI
LAE37:  EOR  TEMP0
LAE39:  AND  #$F0
LAE3B:  EOR  TEMP0
LAE3D:  STA  TEMP0
LAE3F:  TYA
LAE40:  ASL
LAE41:  ASL
LAE42:  ASL
LAE43:  ASL
LAE44:  EOR  TEMP0
LAE46:  STA  QT5                    ;NO. KILL STACK
LAE49:  JSR  RNKDSP                 ;DISPLAY RANKS
LAE4C:  LDA  #$FF                   ;NO GLOW

;.SBTTL DISPLAY HI SCORE LADDER

LDROUT:
LAE4E:  STA  SZL                    ;GLOW CODE (-1=NONE;ELSE=INITIALS TO GLOW)
LAE50:  LDX  #MHIGHS                ;HIGH SCORES
LAE52:  JSR  MSGS
LAE55:  LDA  #$01
LAE57:  STA  SXL                    ;INITIAL RANK
LAE59:  JSR  NWSCA1
LAE5C:  LDA  #$28
LAE5E:  STA  TEMP3                  ;INITIAL HEIGHT
LAE60:  LDX  #3*[NHISCO-1]
LAE62:  STX  INDEX1
LAE64:  JSR  VGCNTR_AB0D
LAE67:  LDA  #$00
LAE69:  STA  VGBRIT
LAE6B:  LDA  TEMP3                  ;Y COORD
LAE6D:  TAX
LAE6E:  SEC
LAE6F:  SBC  #$0A
LAE71:  STA  TEMP3
LAE73:  LDA  #$D0                   ;X COORD
LAE75:  JSR  VGVTR1                 ;POSITION BEAM
LAE78:  LDY  #BLULET                ;DEFAULT BLUE
LAE7A:  LDA  SZL
LAE7C:  CMP  INDEX1
LAE7E:  BNE  LAE82                  ;IFEQ  SPECIAL INITIALS?
LAE80:  LDY  #WHITE                 ;YES. MAKE THEM GLOW WHITE
LAE82:  JSR  NWCOLO
LAE85:  LDA  #SXL                   ;OUTPUT RANK
LAE87:  LDY  #$01
LAE89:  JSR  DIGTYS
LAE8C:  LDA  #$A0
LAE8E:  JSR  VGDOT
LAE91:  LDA  #$00
LAE93:  STA  VGBRIT
LAE95:  TAX
LAE96:  LDA  #$08
LAE98:  JSR  VGVTR1                 ;SPACE
LAE9B:  INC  SXL
LAE9D:  LDA  INDEX1
LAE9F:  JSR  OUTINI                 ;OUTPUT INITIALS
LAEA2:  LDX  #$00
LAEA4:  LDA  #$08
LAEA6:  JSR  VGVTR1                 ;LEAVE SPACE
LAEA9:  LDX  INDEX1
LAEAB:  LDA  HSCORL,X
LAEAE:  STA  PXL
LAEB0:  LDA  HSCORM,X
LAEB3:  STA  PYL
LAEB5:  LDA  HSCORH,X
LAEB8:  STA  PZL
LAEBA:  LDA  #PXL
LAEBC:  LDY  #$03
LAEBE:  JSR  DIGTYS                 ;OUTPUT SCORE
LAEC1:  DEC  INDEX1
LAEC3:  DEC  INDEX1
LAEC5:  DEC  INDEX1
LAEC7:  BPL  LAE64                  ;MIEND
LAEC9:  RTS

;------------------------------------------------------------------------------
; BOLOUT - DISPLAY BONUS INTERVAL
;------------------------------------------------------------------------------
BOLOUT:
LAECA:  LDA  BLIFIN
LAECD:  BEQ  ZATLIV                 ;IFNE  BONUS LIFE?
;YES
LAECF:  STA  PZL                    ;SET INTERVAL
LAED1:  LDX  #MBOLIF
LAED3:  JSR  MSGS                   ;BONUS LIFE EVERY
LAED6:  LDA  #$00
LAED8:  STA  PXL
LAEDA:  STA  PYL
LAEDC:  LDA  #PXL
LAEDE:  LDY  #$03
LAEE0:  JSR  DIGTYS                 ;OUTPUT 10K & 20K

ZATLIV:
LAEE3:  CLC
LAEE4:  LDY  #ZATLIC
LAEE6:  LDA  #$85
LAEE8:  ADC  ZATLIS,Y
LAEEB:  DEY
LAEEC:  BPL  LAEE8                  ;MIEND
LAEEE:  STA  QT1                    ;VERIFY ATARI LITERAL
LAEF0:  RTS

;------------------------------------------------------------------------------
; OUTCUR - UTILITY-DISPLAY 1 SET OF INITIALS
;   OUTINI INPUT:ACC=INDEX INTO INITAL OF INITIALS
;------------------------------------------------------------------------------
OUTCUR:
LAEF1:  LDA  TBLIND
LAEF4:  SEC
LAEF5:  SBC  ININDX

;------------------------------------------------------------------------------
; OUTINI - OUTPUT INITIALS  (comment at the call)
;------------------------------------------------------------------------------
OUTINI:
LAEF8:  CLC
LAEF9:  ADC  #$02
LAEFB:  STA  INDEX2
LAEFD:  LDY  #$00
LAEFF:  LDA  #$02
LAF01:  STA  INDEX3
LAF03:  LDX  INDEX2
LAF05:  LDA  INITAL,X
LAF08:  CMP  #$1E
LAF0A:  BCC  LAF0E                  ;IFCS  VALID?
LAF0C:  LDA  #$1A                   ;NO. SUBSTITUTE SPACE
LAF0E:  ASL
LAF0F:  TAX
LAF10:  LDA  VGMSGA+22,X            ;CONVERT INITIAL(0-26.)TO INDEX
LAF13:  STA  (VGLIST),Y
LAF15:  INY
LAF16:  LDA  VGMSGA+1+22,X
LAF19:  STA  (VGLIST),Y
LAF1B:  INY
LAF1C:  DEC  INDEX2                 ;UPDATE COUNT OF VG BYTES
LAF1E:  DEC  INDEX3
LAF20:  BPL  LAF03                  ;MIEND
LAF22:  DEY
LAF23:  JMP  VGADD                  ;UPDATE VGLIST POINTER

;------------------------------------------------------------------------------
; RNKDSP - RANK DISPLAY
;------------------------------------------------------------------------------
RNKDSP:
LAF26:  LDA  RANKS
LAF29:  ORA  RANKS+1
LAF2C:  BEQ  LAF6E                  ;IFNE  VALID RANKING?
LAF2E:  LDX  #MRANK                 ;YES
LAF30:  JSR  MSGS                   ;"RANKING..." MSG OUTPUT
LAF33:  LDA  #$63
LAF35:  JSR  ONERNK
LAF38:  LDX  #$00                   ;PLAYER 1 RANK
LAF3A:  JSR  PL1RNK
LAF3D:  LDX  #$01                   ;PLAYER 2 RANK

;------------------------------------------------------------------------------
; PL1RNK - [note] Display the rank of player X (RANKS,X; nothing if 0): red rank digits, a dot, 'PLAYER' and
;   the player number at height HITRNK,X.
;------------------------------------------------------------------------------
PL1RNK:
LAF3F:  LDA  RANKS,X
LAF42:  BEQ  LAF6E                  ;IFNE  VALID RANK?
LAF44:  PHA                         ;YES.
LAF45:  STX  TEMPX
LAF47:  LDY  #RED
LAF49:  JSR  NWCOLO                 ;GREEN DIGIT
LAF4C:  JSR  VGCNTR_AB0D            ;CENTER BEAM
LAF4F:  LDA  #$D0
LAF51:  LDY  TEMPX
LAF53:  LDX  HITRNK,Y
LAF56:  JSR  VGVTR1                 ;POSITION BEAM
LAF59:  PLA
LAF5A:  JSR  ONERNK                 ;OUTPUT RANK
LAF5D:  LDA  #$A0
LAF5F:  JSR  VGDOT                  ;DOT
LAF62:  LDA  #$10
LAF64:  LDX  #MPLYR2                ;PLAYER
LAF66:  JSR  MSGFOL
LAF69:  LDX  TEMPX
LAF6B:  JSR  DPLRXX                 ;PLAYER #
LAF6E:  RTS

HITRNK:
LAF6F:  .byte $C0, $B0              ;HEIGHT FOR RANK DISPLAYS

;------------------------------------------------------------------------------
; ONERNK - OUTPUT RANK  (comment at the call)
;------------------------------------------------------------------------------
ONERNK:
LAF71:  CMP  #NRANKS
LAF73:  BCC  DSP1HX                 ;IFCS  MAX AT NRANKS
LAF75:  LDA  #NRANKS

;------------------------------------------------------------------------------
; DSP1HX - OUTPUT 2 DIGITS W. ZERO SUPPRESION  (comment at the call)
;   -OUTPUT LEVEL #  (another call)
;------------------------------------------------------------------------------
DSP1HX:
LAF77:  JSR  HEXBCD                 ;CONVERT TO BCD
LAF7A:  LDA  #TEMP0
LAF7C:  LDY  #$01
LAF7E:  JMP  DIGTYS                 ;DISPLAY RANK

;------------------------------------------------------------------------------
; RQRDSP - DISPLAY PLAYER RATING REQUEST
;------------------------------------------------------------------------------
RQRDSP:
LAF81:  JSR  COCFLI                 ;COCKTAIL FLIP
LAF84:  DEC  SECUVY
LAF87:  LDY  #RED                   ;SET LETTER COLOR
LAF89:  JSR  NWCOLO
LAF8C:  LDA  #$01
LAF8E:  STA  VGSIZE
LAF90:  JSR  VGSCA1

ZATC2S:
LAF93:  LDX  #MATARI
LAF95:  LDA  #$60
LAF97:  JSR  MSGEN3
LAF9A:  JSR  DPLRNO                 ;PLAYER X

ZATC2E:
LAF9D:  LDX  #<[ENDMSG-MSGTAB-1]
LAF9F:  STX  INDEX1
LAFA1:  LDY  INDEX1
LAFA3:  LDX  MSGTAB,Y               ;MESSAGE ID
LAFA6:  JSR  MSGS                   ;DISPLAY MESSAGE
LAFA9:  DEC  INDEX1
LAFAB:  BPL  LAFA1                  ;MIEND
;IF CURSOR IS AT EDGE OF VISIBLE SCREEN, TRY TO SCROLL
LAFAD:  LDA  CURSL1                 ;ACTUAL CURSOR POSITION
LAFB0:  SEC
LAFB1:  SBC  LEFSID
LAFB3:  BPL  LAFBC                  ;IFMI
LAFB5:  DEC  LEFSID
LAFB7:  DEC  RITSID
LAFB9:  CLV                         ;ELSE
LAFBA:  BVC  LAFE1
LAFBC:  BNE  LAFCB                  ;IFEQ  AT LEFT SIDE OF VISIBLE SCREEN?
LAFBE:  DEC  RITSID                 ;YES. TRY TO SCROLL LEFT
LAFC0:  DEC  LEFSID
LAFC2:  BPL  LAFC8                  ;IFMI  VALID?
LAFC4:  INC  LEFSID                 ;NO. UNSCROLL
LAFC6:  INC  RITSID
LAFC8:  CLV                         ;ELSE
LAFC9:  BVC  LAFE1
LAFCB:  LDA  RITSID                 ;NO.
LAFCD:  CMP  HIRATE
LAFD0:  BEQ  YES                    ;DISPLAY NEXT ONE
LAFD2:  BCS  LAFE1                  ;IFCC  AT RIGHT SIDE OF VISIBLE SCREEN?

YES:
LAFD4:  SEC                         ;YES.
LAFD5:  SBC  CURSL1
LAFD8:  BNE  LAFDB                  ;IFEQ
LAFDA:  CLC
LAFDB:  BCS  LAFE1                  ;IFCC  SCROLL RIGHT VALID?
LAFDD:  INC  LEFSID                 ;YES. SCROLL
LAFDF:  INC  RITSID
LAFE1:  LDA  RITSID
LAFE3:  STA  INDEX4
LAFE5:  LDX  #$04
LAFE7:  STX  INDEX1
LAFE9:  LDY  #GREEN
LAFEB:  JSR  NWCOLO
LAFEE:  LDA  #$00
LAFF0:  STA  VGBRIT
LAFF2:  JSR  VGCNTR_AB0D            ;DISPLAY LEVEL #
LAFF5:  LDX  #$D8
LAFF7:  LDY  INDEX1
LAFF9:  LDA  XPOTAB,Y
LAFFC:  CLC
LAFFD:  ADC  #$F8
;[CS] Start of ROM 136002.217 at $B000.   	NOTE: VERSION 2 ROMS, FOLKS.
LAFFF:  JSR  VGVTR1                 ;-GET INTO POSITION
LB002:  LDX  INDEX4
LB004:  LDY  LEVEL,X
LB007:  CPY  #$63
LB009:  BCS  LB042                  ;IFCC  IN RANGE?
LB00B:  INY                         ;YES
LB00C:  TYA
LB00D:  JSR  DSP1HX                 ;-OUTPUT LEVEL #
LB010:  LDY  #RED
LB012:  JSR  NWCOLO
LB015:  JSR  VGCNTR_AB0D            ;DISPLAY BONUS POINTS
LB018:  LDX  #$BA
LB01A:  LDY  INDEX1
LB01C:  LDA  XPOTAB,Y
LB01F:  CLC
LB020:  ADC  #$EC
LB022:  JSR  VGVTR1                 ;-GET INTO POSITION
LB025:  LDX  INDEX4
LB027:  JSR  BODSPL                 ;DISPLAY BONUS
;-OUTPUT POINTS
LB02A:  JSR  VGCNTR_AB0D
LB02D:  LDX  #$CC
LB02F:  LDY  INDEX1
LB031:  LDA  XPOTAB,Y
LB034:  CLC
LB035:  ADC  #$00
LB037:  JSR  VGVTR1                 ;POSITION BEAM
LB03A:  LDX  INDEX4                 ;DISPLAY HOLE
LB03C:  LDA  LEVEL,X
LB03F:  JSR  DSPHOL
LB042:  DEC  INDEX4
LB044:  DEC  INDEX1
LB046:  BPL  LAFE9                  ;MIEND
;DISPLAY TIME LEFT
LB048:  LDA  #$00
LB04A:  STA  VGBRIT
LB04C:  JSR  VGCNTR_AB0D
LB04F:  LDX  #MTIME                 ;"TIME"
LB051:  JSR  MSGS
LB054:  LDA  #QTMPAUS
LB056:  LDY  #$01
LB058:  JSR  DIGTYS                 ;-OUTPUT SECONDS
;DISPLAY CURSOR
;DISPLAY BOX AROUND LEVEL
LB05B:  LDY  #WHITE
LB05D:  JSR  NWCOLO                 ;CURSOR COLOR
LB060:  JSR  VGCNTR_AB0D
LB063:  LDX  #$B8
LB065:  JSR  GETCUR                 ;GET CURSOR POS.
LB068:  SEC                         ;MAKE IT RELATIVE
LB069:  SBC  LEFSID
LB06B:  TAY
LB06C:  LDA  XPOTAB,Y
LB06F:  SEC
LB070:  SBC  #$16
LB072:  JSR  VGVTR1                 ;POSITION BEAM AT UPPER RIGHT CORNER OF LEVEL
LB075:  LDA  #$E0
LB077:  STA  VGBRIT                 ;BEAM ON
LB079:  LDX  #$00                   ;BALL INTO DRAW BOX

DOBOX:
;DRAW BOX: X=INDEX INTO BOXTAB OF 1ST SET OF OFFSETS IN BOX
LB07B:  STX  INDEX2
LB07D:  LDY  #$03
LB07F:  STY  INDEX1
LB081:  LDY  INDEX2
LB083:  LDA  BOXTAB,Y
LB086:  TAX
LB087:  INY
LB088:  LDA  BOXTAB,Y
LB08B:  INY
LB08C:  STY  INDEX2
LB08E:  JSR  VGVTR1
LB091:  DEC  INDEX1
LB093:  BPL  LB081                  ;MIEND
LB095:  RTS

XPOTAB:
LB096:  .byte $BE, $E3, $09, $30, $58 ;[CS] Start of a DATA segment ref'd by several locations.

MSGTAB:
LB09B:  .byte MRATE, MPRMOV, MPRFIR ;RATE MESSAGES
LB09E:  .byte MNOVIC, MEXPER, MLEVEL
LB0A1:  .byte MHOLE, MBONUS

ENDMSG:
BOXTAB:
LB0A3:  .byte $00, $26
LB0A5:  .byte $28, $00
LB0A7:  .byte $00, $DA
LB0A9:  .byte $D8, $00

;------------------------------------------------------------------------------
; GETCUR - UPDATE CURSOR POSITION  (comment at the call)
;   GET CURSOR POS.  (another call)
;------------------------------------------------------------------------------
GETCUR:
LB0AB:  LDA  CURSL1                 ;CURRENT POSITION
LB0AE:  JSR  GINICO                 ;UPDATE POSITION
LB0B1:  TAY
LB0B2:  BPL  LB0B9                  ;IFMI  APPLY LIMITS
LB0B4:  LDA  #$00                   ;MIN
LB0B6:  CLV                         ;ELSE
LB0B7:  BVC  LB0C1
LB0B9:  CMP  HIRATE
LB0BC:  BCC  LB0C1                  ;IFCS
LB0BE:  LDA  HIRATE                 ;MAX
LB0C1:  STA  CURSL1                 ;NEW POSITION
LB0C4:  TAY
LB0C5:  RTS

;------------------------------------------------------------------------------
; BODSPL - DISPLAY BONUS  (comment at the call)
;------------------------------------------------------------------------------
BODSPL:
LB0C6:  TXA
LB0C7:  JSR  BONSCO                 ;SET UP BONUS SCORE
LB0CA:  LDA  #TEMP0
LB0CC:  LDY  #$03
LB0CE:  JMP  DIGTYS

;------------------------------------------------------------------------------
; NWCOLO - INPUT: Y=NEW COLOR
;------------------------------------------------------------------------------
NWCOLO:
LB0D1:  CPY  COLOR                  ;CHANGE COLOR IF NECESSARY
LB0D3:  BEQ  LB0DC                  ;IFNE
LB0D5:  STY  COLOR
LB0D7:  LDA  #$08                   ;CHANGE COLOR FLAG
LB0D9:  JMP  VGSTAT
LB0DC:  RTS

;------------------------------------------------------------------------------
; NWSCA1 - INPUT: ACC=NEW BINARY SCALE
;------------------------------------------------------------------------------
NWSCA1:
LB0DD:  CMP  VGSIZE
LB0DF:  BEQ  LB0E6                  ;IFNE  CHANGE BINARY SCALE IF NECESSARY
LB0E1:  STA  VGSIZE
LB0E3:  JMP  VGSCA1
LB0E6:  RTS

;------------------------------------------------------------------------------
; LOGINI - LOGO INITIALIZATION
;------------------------------------------------------------------------------
LOGINI:
LB0E7:  LDA  #CPAUSE                ;GAME STATE:PAUSE FOR ENTIRE LOGO  [CS] Game status = non-player input
LB0E9:  STA  QSTATE                 ;[CS] mode.
LB0EB:  LDA  #CNEWGA                ;THEN GO TO GAME
LB0ED:  STA  QNXTSTA
LB0EF:  LDA  #$DF
LB0F1:  STA  QTMPAUS                ;[CS] START OF TITLE SCREEN
LB0F3:  LDA  #CDBOXP                ;FIRST DO SHRINKING BOX RAINBOW  [CS] Set game status to title start
LB0F5:  STA  QDSTATE                ;[CS] where a box shrinks to the center.
LB0F7:  LDA  #$19                   ;STARTING CLOSE
LB0F9:  STA  FARY
LB0FC:  LDA  #$18
LB0FE:  STA  NEARY
LB101:  RTS

;.SBTTL LOGO-SHRINKING BOX RAINBOW

BOXPRO:
LB102:  LDA  #>[VORBOX+1]           ;LDAH VORBOX+1
LB104:  LDX  #<VORBOX               ;LXL VORBOX
;SET PIC TO USE
LB106:  JSR  SCARNG                 ;DRAW RAINBOW OF BOX
LB109:  LDA  FARY
LB10C:  CMP  #$A0
LB10E:  BCS  LB115                  ;IFCC  FAR PT. PAST DESTINATION?
LB110:  ADC  #$14                   ;NO. MOVE IT FARTHER
LB112:  STA  FARY
LB115:  CMP  #$50
LB117:  BCC  LB130                  ;IFCS  FOR PT. PAST PT. WHERE NEAR PT. MOVES?
LB119:  LDA  NEARY                  ;YES. MOVE NEAR PT. FARTHER
LB11C:  CLC
LB11D:  ADC  #FARINC
LB11F:  STA  NEARY
LB122:  CMP  FARY
LB125:  BCC  LB130                  ;IFCS  NEAR PT. PAST FAR PT.?
LB127:  LDA  #$A0                   ;YES.
LB129:  STA  NEARY
LB12C:  LDA  #CDLOGP                ;NOW DO GROWING LOGO  [CS] Set game mode to the part of
LB12E:  STA  QDSTATE                ;[CS] the attract mode where the Tempest logo grows bigger.
LB130:  RTS

;.SBTTL LOGO - LOGO RAINBOW

LOGPRO:          ;APPROACHING LOGO PROCESS
LB131:  LDA  #>[VORLIT+1]           ;LDAH VORLIT+1  [CS] Code section responsible
LB133:  LDX  #<VORLIT               ;LXL VORLIT  [CS] for zoming IN the tempest
;SET PICTURE TO USE
LB135:  JSR  SCARNG                 ;DRAW RAINBOW OF LOGO  [CS] logo
LB138:  LDA  NEARY
LB13B:  CMP  #$30                   ;[CS] How big the logo becomes
LB13D:  BCC  LB144                  ;IFCS  NEAR PT. PAST DESTINATION?
LB13F:  SBC  #$01                   ;NO. BRING IT CLOSER
LB141:  STA  NEARY
LB144:  CMP  #$80                   ;[CS] How close the logo gets before the back-end starts to catch up.
LB146:  BCS  LB159                  ;IFCC  NEAR PT. PAST PT. WHERE FAR PT. MOVES?
LB148:  LDA  FARY                   ;YES. BRING FAR PT. CLOSER
LB14B:  SEC
LB14C:  SBC  #$01
LB14E:  CMP  NEARY
LB151:  BCS  LB156                  ;IFCC  FAR PT PAST NEAR?
LB153:  LDA  NEARY                  ;YES. SET AT NEAR PT
LB156:  STA  FARY
LB159:  RTS

;------------------------------------------------------------------------------
; SCARNG - LOGO RAINBOW BUILDER
;------------------------------------------------------------------------------
SCARNG:
LB15A:  STA  PYL                    ;POINTER TO PICTURE SUBR
LB15C:  STX  PXL
LB15E:  LDA  NEARY
LB161:  STA  INDEX1
LB163:  DEC  SECUVY
LB166:  LDA  INDEX1                 ;SCALE IS A FUNCTION OF DISTANCE
LB168:  ASL
LB169:  ASL
LB16A:  AND  #$7F                   ;LINEAR SCALE
LB16C:  TAY
LB16D:  LDA  INDEX1
LB16F:  LSR
LB170:  LSR
LB171:  LSR
LB172:  LSR
LB173:  LSR                         ;BINARY SCALE
LB174:  JSR  VGSCAL
LB177:  LDA  INDEX1                 ;COLOR DEPENDS ON POSITION IN RAINBOW
LB179:  CMP  NEARY
LB17C:  BNE  LB183                  ;IFEQ  LEADING PT.?
LB17E:  LDA  #WHITE                 ;YES. MAKE IT MOST VISIBLE
LB180:  CLV                         ;ELSE
LB181:  BVC  LB18F
LB183:  LSR
LB184:  LSR
LB185:  LSR
LB186:  NOP
LB187:  AND  #$07                   ;NO FOLLOWERS
LB189:  CMP  #$07
LB18B:  BNE  LB18F                  ;IFEQ
LB18D:  LDA  #RED                   ;USE RED FOR BLACK
LB18F:  TAY
LB190:  LDA  #$68
LB192:  JSR  VGSTAT                 ;SET COLOR
LB195:  LDA  PYL
LB197:  LDX  PXL
LB199:  JSR  VGJSRL                 ;DRAW PICTURE AT PT.

ZATC1S:
LB19C:  LDA  INDEX1
LB19E:  CLC
LB19F:  ADC  #$02
LB1A1:  STA  INDEX1
LB1A3:  CMP  FARY
LB1A6:  BCC  LB166                  ;CSEND  EXIT IF PAST NEAR PT.
LB1A8:  LDX  #MATARI
LB1AA:  LDA  #$D0
LB1AC:  JSR  MSGEN3                 ;DISPLAY COPYRIGHT LOGO
LB1AF:  LDA  #>[KILLER+1]           ;LDAH KILLER+1
LB1B1:  LDX  #<KILLER               ;LXL KILLER

ZATC1E:
LB1B3:  JMP  VGJSRL                 ;DEFEAT BEAM KILLER

;==============================================================================
; MODULE ALDIS2   ALDIS2.MAC
;   Display: well projection and perspective, drawing of well, enemies, shots,
;   explosions, cursor, stars; vector RAM sub-buffer management; object
;   pictures.
;==============================================================================

;------------------------------------------------------------------------------
; DISPLAY - DISPLAY-MAINLINE
;------------------------------------------------------------------------------
DISPLAY:
LB1B6:  JSR  INIMAT                 ;SET UP MATH BOX
LB1B9:  LDA  VECRAM                 ;[CS] Look at the first byte in
LB1BC:  CMP  JMPMAL+4               ;[CS] vector RAM.
LB1BF:  BNE  LB1C7                  ;IFEQ  TRYING TO HALT?
LB1C1:  LDA  SPARE3                 ;YES.
LB1C4:  BNE  LB1C7                  ;IFEQ  HALT YET
LB1C6:  RTS                         ;NO. GO AWAY
LB1C7:  LDA  QDSTATE                ;[CS] Are we in attract or gameplay
LB1C9:  CMP  #CDPLAY                ;[CS] mode?
LB1CB:  BEQ  LB209                  ;IFNE  ANYTHING BUT PLAY STATE?
LB1CD:  LDA  #BCINFO                ;YES. DEFAULT TO INFO BUFFER
LB1CF:  JSR  SBCLOG
LB1D2:  JSR  BIGTEX
LB1D5:  BCS  LB1F5                  ;IFCC  SET UP LARGE BUFFER
;BUFFER AVAILABLE FILL IT
LB1D7:  JSR  DSTATE                 ;EXECUTE DISPLAY STATE

ZATVG2:
LB1DA:  LDA  SECUVY
LB1DD:  BEQ  LB1F5                  ;IFNE  ATARI ON SCREEN?
LB1DF:  LDY  #$27                   ;YES. VERIFY
LB1E1:  LDA  #$0E
LB1E3:  SEC
LB1E4:  SBC  (SECUVG),Y
LB1E6:  DEY
LB1E7:  BPL  LB1E4                  ;MIEND
LB1E9:  TAY
LB1EA:  BEQ  LB1EE                  ;IFNE
LB1EC:  EOR  #$E5
LB1EE:  BEQ  LB1F2                  ;IFNE
LB1F0:  EOR  #$29
LB1F2:  STA  QT3
LB1F5:  LDA  #BCINFO
LB1F7:  JSR  SBCSWI
LB1FA:  LDA  JMPMAL+2
LB1FD:  STA  VECRAM
LB200:  LDA  JMPMAH+2
LB203:  STA  VECRAM+1
LB206:  CLV                         ;ELSE
LB207:  BVC  LB20C
LB209:  JMP  DENORM                 ;PLAY STATE
LB20C:  RTS

;------------------------------------------------------------------------------
; DSTATE - DISPLAY STATE EXECUTOR
;------------------------------------------------------------------------------
DSTATE:
LB20D:  LDX  QDSTATE
LB20F:  LDA  DROUTAD+1,X
LB212:  PHA
LB213:  LDA  DROUTAD,X
LB216:  PHA

NOOPR:
LB217:  RTS

DROUTAD:
;[CS] Data Segment, per Ken Lui
LB218:  .word DENORM-1              ;GAME PLAY - TOP OF WELL, DOWN THE TUBE
LB21A:  .word DSPSYS-1              ;SYSTEM CONFIGURATION
LB21C:  .word DSBOOM-1              ;GAME PLAY - BOOM
LB21E:  .word GETDSP-1              ;DATA ENTRY - HI SCORE INITIALS
LB220:  .word RQRDSP-1              ;DATA ENTRY - RANKING
LB222:  .word LDRDSP-1              ;INFO ONLY - HI SCORE TABLE
LB224:  .word DGOVER-1              ;GAME OVER PLAYER X
LB226:  .word DPLPLA-1              ;PLAY PLAYER X
LB228:  .word DPRSTA-1              ;"PRESS START"
LB22A:  .word BOXPRO-1              ;LOGO BOX
LB22C:  .word LOGPRO-1              ;LOGO
LB22E:  .word D2GAME-1              ;2 GAME MINIMUM

;------------------------------------------------------------------------------
; DROUTEN - DISPLAY-GAME PLAY MAINLINE
;   DISPLAY CURSOR
;   (also: DENORM)
;------------------------------------------------------------------------------
DROUTEN:
DENORM:
LB230:  LDA  #BCCURS
LB232:  JSR  SBCLOG
LB235:  JSR  DSPCUR
LB238:  LDA  #BCCURS
LB23A:  JSR  SBCSWI
;DISPLAY CHARGES
LB23D:  LDA  #BCSHOT
LB23F:  JSR  SBCLOG
LB242:  JSR  DSPCHG
LB245:  LDA  #BCSHOT
LB247:  JSR  SBCSWI
;DISPLAY INVADERS
LB24A:  LDA  #BCINVA
LB24C:  JSR  SBCLOG
LB24F:  JSR  DSPINV
LB252:  LDA  #BCINVA
LB254:  JSR  SBCSWI
;DISPLAY EXPLOSIONS
LB257:  LDA  #BCEXPL
LB259:  JSR  SBCLOG
LB25C:  JSR  DSPEXP
LB25F:  LDA  #BCEXPL
LB261:  JSR  SBCSWI
;DISPLAY NYMPHS
LB264:  LDA  #BCNYMP
LB266:  JSR  SBCLOG
LB269:  JSR  DSPNYM
LB26C:  LDA  #BCNYMP
LB26E:  JSR  SBCSWI
;DISPLAY INFORMATION (SCORES, MSGS, ETC.)
LB271:  LDA  #BCINFO
LB273:  JSR  SBCLOG
LB276:  JSR  INFO

ZATVG1:
LB279:  LDA  QSTATUS
LB27B:  BMI  LB28A                  ;IFPL  ATTRACT?
LB27D:  LDA  #$F2                   ;YES. ATARI BETTER BE ON SCREEN
LB27F:  CLC
LB280:  LDY  #$27
LB282:  ADC  (SECUVG),Y
LB284:  DEY
LB285:  BPL  LB282                  ;MIEND
LB287:  STA  QT6                    ;SAVE RESSLT (SHOULD BE 0)  [CS] Copy protection code?
LB28A:  LDA  #BCINFO
LB28C:  JSR  SBCSWI
;DISPLAY WELL
LB28F:  JSR  DSPWEL                 ;DISPLAY WELL
LB292:  LDA  #BCENEL                ;DISPLAY ENEMY LINES
LB294:  JSR  SBCLOG
LB297:  JSR  DSPENL
LB29A:  LDA  #BCENEL
LB29C:  JSR  SBCSWI
LB29F:  LDA  #BCSTAR                ;DISPLAY STAR FIELD
LB2A1:  JSR  SBCLOG
LB2A4:  JSR  DSTARF
LB2A7:  LDA  #BCSTAR
LB2A9:  JSR  SBCSWI
LB2AC:  LDA  #$00
LB2AE:  STA  ROTDIS
LB2B1:  LDA  JMPMAL                 ;SET MASTER POINTER TO JSRL
LB2B4:  STA  VECRAM                 ;LIST FOR SUBLISTS CREATED ABOVE
LB2B7:  LDA  JMPMAH
LB2BA:  STA  VECRAM+1
LB2BD:  RTS

;------------------------------------------------------------------------------
; SBCLOG - BUFFER CONTROL
;   INPUT:ACC=SUB BUFFER GROUP INDEX CODE
;   OUTPUT:VGLIST(2) & VGY SET UP TO VACANT BUFFER
;   ACC,X,Y DESTROYED
;------------------------------------------------------------------------------
SBCLOG:
LB2BE:  TAX                         ;SET UP VECTOR RAM POINTS TO
LB2BF:  ASL                         ;UNUSED BUFFER
LB2C0:  TAY
LB2C1:  LDA  BUFACT,X
LB2C4:  BNE  LB2CF                  ;IFEQ  BUFFER A OR B ACTIVE?
LB2C6:  LDX  BUFBSL,Y               ;A IS CTIVE. BUILD IN B
LB2C9:  LDA  BUFBSH,Y
LB2CC:  CLV                         ;ELSE
LB2CD:  BVC  LB2D5
LB2CF:  LDX  BUFASL,Y               ;B IS ACTIVE. BUILD IN A
LB2D2:  LDA  BUFASH,Y
LB2D5:  STX  VGLIST
LB2D7:  STA  VGLIST+1
LB2D9:  LDA  #$00
LB2DB:  STA  VGY
LB2DD:  RTS

;------------------------------------------------------------------------------
; SBCACT - OPPOSITE OF SBCLOG-PLACE PTR
;   TO ACTIVE BUFFER INTO INDYLO
;------------------------------------------------------------------------------
SBCACT:
LB2DE:  TAX
LB2DF:  ASL
LB2E0:  TAY
LB2E1:  LDA  BUFACT,X
LB2E4:  BNE  LB2EF                  ;IFEQ
LB2E6:  LDX  BUFASL,Y               ;A IS ACTIVE
LB2E9:  LDA  BUFASH,Y
LB2EC:  CLV                         ;ELSE
LB2ED:  BVC  LB2F5
LB2EF:  LDX  BUFBSL,Y               ;B IS ACTIVE
LB2F2:  LDA  BUFBSH,Y
LB2F5:  STX  INDYLO
LB2F7:  STA  INDYLO+1
LB2F9:  LDA  #$00
LB2FB:  STA  VGY
LB2FD:  RTS

;------------------------------------------------------------------------------
; SBCSWI - INPUT:ACC=SUBBUFFER GROUP INDEX CODE
;   OUTPUT:RTS ADDED TO END OF NEWLY BUILT BUFFER
;   SWITCH SET TO POINT TO NEW BUFFER
;   BUFACT(X)FLIPPED INDICATING NEW BUFFER ACTIVE
;------------------------------------------------------------------------------
SBCSWI:
LB2FE:  PHA
LB2FF:  JSR  VGRTSL                 ;INSERT RTSL AT END OF BUFFER
LB302:  PLA
LB303:  TAX
LB304:  ASL
LB305:  TAY
LB306:  LDA  BUFSWL,Y               ;SET UP SWITCH LOCATION
LB309:  STA  INDYLO
LB30B:  LDA  BUFSWH,Y
LB30E:  STA  INDYHI
LB310:  LDA  BUFACT,X
LB313:  EOR  #$01
LB315:  STA  BUFACT,X
LB318:  BNE  LB323                  ;IFEQ  WHICH IS THE NEW BUFFER TO DISLAY?
LB31A:  LDA  JMPALO,Y               ;BUFFER A
LB31D:  LDX  JMPAHI,Y
LB320:  CLV                         ;ELSE
LB321:  BVC  LB329
LB323:  LDA  JMPBLO,Y               ;BUFFER B
LB326:  LDX  JMPBHI,Y
LB329:  LDY  #$00                   ;UPDATE SWITCH TO PT TO
LB32B:  STA  (INDYLO),Y             ;NEW BUFFER
LB32D:  TXA
LB32E:  INY
LB32F:  STA  (INDYLO),Y
LB331:  RTS

;------------------------------------------------------------------------------
; BIGTEX - ASSIGN LARGE BUFFER FOR TEXT
;------------------------------------------------------------------------------
BIGTEX:
LB332:  LDA  JMPMAL+2
LB335:  CMP  VECRAM
LB338:  BEQ  LB33F                  ;IFNE  BEEN HERE BEFORE?
LB33A:  STA  VECRAM                 ;NO. SET UP MASTER POINTER FOR TEXT ONLY.
LB33D:  SEC
LB33E:  RTS                         ;EXIT
LB33F:  LDA  BUFACT                 ;YES. INSERT JMP TO AREA WITH MORE ROOM.
LB342:  BNE  LB349                  ;IFEQ
LB344:  LDX  #$02                   ;BIG AREA 1 (1ST HALF OF VECRAM)
LB346:  CLV                         ;ELSE
LB347:  BVC  LB34B
LB349:  LDX  #$08                   ;BIG AREA 2 (2ND HALF OF VECRAM)
LB34B:  LDA  JMPALO,X
LB34E:  LDY  #$00
LB350:  STY  SECUVY
LB353:  STA  (VGLIST),Y
LB355:  INY
LB356:  LDA  JMPAHI,X               ;INSERT JMPL TO AREA WITH MORE ROOM
LB359:  STA  (VGLIST),Y
LB35B:  LDA  BUFASL,X               ;POINT VGLIST AT NEW AREA
LB35E:  STA  VGLIST
LB360:  LDA  BUFASH,X
LB363:  STA  VGLIST+1
LB365:  CLC
LB366:  RTS

;------------------------------------------------------------------------------
; DSPWEL - DISPLAY-WELL
;------------------------------------------------------------------------------
DSPWEL:
LB367:  LDA  ROTDIS
LB36A:  BEQ  LB379                  ;IFNE  REBUILD WELL?
LB36C:  LDA  #BCWELL
LB36E:  JSR  SBCLOG
LB371:  JSR  BLDWEL                 ;YES
LB374:  LDA  #BCWELL
LB376:  JSR  SBCSWI
LB379:  LDA  #BCWELL                ;SET UP PTR TO ACTIVE WELL BUFFER
LB37B:  JSR  SBCACT

;.SBTTL DISPLAY-SPOKE PULSE STATUS
LB37E:  LDA  #$00
LB380:  LDX  #NLINES-1
LB382:  STA  SPOKST,X               ;CLEAR SPOKE PULSE STATUS
LB385:  DEX
LB386:  BPL  LB382                  ;MIEND
LB388:  LDA  CURMOD
LB38B:  BMI  LB3D6                  ;IFPL  CURSOR AT TOP?
LB38D:  LDX  WINVMX                 ;YES.
LB390:  LDA  INVAY,X
LB393:  BEQ  LB3D3                  ;IFNE  ACTIVE INVADER?
LB395:  LDY  #$00                   ;YES. DEFAULT
LB397:  LDA  INVAC1,X
LB39A:  AND  #INVABI
LB39C:  CMP  #ZABPUL
LB39E:  BNE  LB3D3                  ;IFEQ  PULSAR?
LB3A0:  INY                         ;YES. SET PULSAR BIT D0
LB3A1:  STY  TEMP0
LB3A3:  LDA  INVAC1,X               ;YES
LB3A6:  AND  #INVMOT
LB3A8:  BNE  LB3C6                  ;IFEQ  FLIPPING?
LB3AA:  LDA  PULSON                 ;NO.
LB3AD:  BMI  LB3BB                  ;IFPL  PULSARS ON?
LB3AF:  LDA  INVAY,X                ;YES.
LB3B2:  CMP  PULPOT
LB3B5:  BCS  LB3BB                  ;IFCC  POTENT PULSAR?
LB3B7:  INC  TEMP0                  ;YES. SET PULSE BIT D1
LB3B9:  INC  TEMP0
LB3BB:  LDA  TEMP0                  ;SET CCW LEG STATUS
LB3BD:  LDY  INVAL2,X
LB3C0:  ORA  SPOKST,Y
LB3C3:  STA  SPOKST,Y
LB3C6:  LDY  INVAL1,X
LB3C9:  LDA  TEMP0
LB3CB:  ORA  #$80                   ;SET BASE BIT
LB3CD:  ORA  SPOKST,Y
LB3D0:  STA  SPOKST,Y
LB3D3:  DEX
LB3D4:  BPL  LB390                  ;MIEND
LB3D6:  LDA  #WELCOL                ;DEFAULT WELL COLO
LB3D8:  LDY  SUZTIM
LB3DB:  BEQ  LB3E9                  ;IFNE
LB3DD:  BMI  LB3E9                  ;IFPL  SUPERZAPPER ACTIVE?
LB3DF:  LDA  QFRAME                 ;YES. SUPERZAPPER IS DEFAULT
LB3E1:  AND  #$07
LB3E3:  CMP  #$07
LB3E5:  BNE  LB3E9                  ;IFEQ
LB3E7:  LDA  #$01                   ;NO BLACK
LB3E9:  STA  TEMP0                  ;DEFAULT COLOR
LB3EB:  LDY  #$FF
LB3ED:  LDX  #$FF
LB3EF:  STX  TEMP3                  ;DEFAULT NO BONUS FLASH
LB3F1:  LDA  CURSY
LB3F4:  BEQ  LB401                  ;IFNE  C;URSOR ALIVE?
LB3F6:  LDA  CURSL2
LB3F9:  BMI  LB401                  ;IFPL
LB3FB:  LDX  CURSL1                 ;YES.
LB3FE:  LDY  CURSL2
LB401:  STX  TEMP1                  ;SAVE FLASLIGHT SPOKES
LB403:  STY  TEMP2
LB405:  LDA  BOFLASH
LB408:  BMI  LB412                  ;IFPL  BONUS FLASH?
LB40A:  AND  #$0E                   ;YES. SET BASE COLOR
LB40C:  LSR
LB40D:  STA  TEMP3
LB40F:  DEC  BOFLASH
LB412:  LDX  #NLINES-1
LB414:  LDY  #WELCOL                ;DEFAULT WELL COLOR
LB416:  LDA  SPOKST,X
LB419:  BEQ  LB427                  ;IFNE  PULSE?
LB41B:  AND  #$02
LB41D:  BEQ  LB424                  ;IFNE  YES. PULSING?
LB41F:  LDA  QFRAME
LB421:  AND  #$01
LB423:  TAY
LB424:  CLV                         ;ELSE
LB425:  BVC  LB44B
LB427:  CPX  TEMP1                  ;NO.
LB429:  BEQ  LB42D                  ;IFNE
LB42B:  CPX  TEMP2
LB42D:  BNE  LB434                  ;IFEQ  NO. CURSOR FLASHLIGHT?
LB42F:  LDY  #CURCOL                ;YES. CURSOR COLOR
LB431:  CLV                         ;ELSE
LB432:  BVC  LB44B
LB434:  LDA  BOFLASH                ;NO.
LB437:  BMI  LB449                  ;IFPL  BONUS FLASH?
LB439:  TXA                         ;YES. BONUS COLOR
LB43A:  CLC
LB43B:  ADC  TEMP3                  ;PLUS BASE COLOR
LB43D:  AND  #$07                   ;MOD 8
LB43F:  CMP  #$07
LB441:  BNE  LB445                  ;IFEQ
LB443:  LDA  #$03                   ;NO BLACK
LB445:  TAY
LB446:  CLV                         ;ELSE
LB447:  BVC  LB44B
LB449:  LDY  TEMP0                  ;NO. USE DEFALT COLOR
LB44B:  TYA
LB44C:  LDY  STALOC,X
LB44F:  STA  (INDYLO),Y
LB451:  DEX
LB452:  BPL  LB414                  ;MIEND
LB454:  LDX  #NLINES-1              ;YES. REDO TOP RUNGS
LB456:  BIT  WELTYP
LB459:  BPL  LB45C                  ;IFMI
LB45B:  DEX                         ;PLANAR, SO 1 LESS RUNG
LB45C:  LDY  #$C0                   ;DEFAULT ON
LB45E:  LDA  SPOKST,X
LB461:  BPL  LB465                  ;IFMI  PULSAR?
LB463:  LDY  #$00                   ;YES. TURN OFF
LB465:  STY  PZL
LB467:  LDY  RUNLOC,X
LB46A:  LDA  (RUNGVG),Y
LB46C:  AND  #$1F
LB46E:  ORA  PZL
LB470:  STA  (RUNGVG),Y
LB472:  DEX
LB473:  BPL  LB45C                  ;MIEND
LB475:  RTS

STALOC:
;OFFSETS INTO WELL SUBROUTINE OF COLOR STATS FOR EACH LINE
LB476:  .byte $A8, $9C, $92, $86, $7C, $70, $66, $5A
LB47E:  .byte $50, $44, $3A, $2E, $24, $18, $0E, $02
LB486:  .byte $B2

RUNLOC:
;OFFSETS INTO WELL SUBROUTINE (+0FE) OF COLOR STATS FOR EACH TOP RUNG
LB487:  .byte $3B, $37, $33, $2F, $2B, $27, $23, $1F
LB48F:  .byte $1B, $17, $13, $0F, $0B, $07, $03, $3F

CHKSM6:
LB497:  .byte QCHKS6

;------------------------------------------------------------------------------
; DSPNYM - DISPLAY-NYMPHS
;------------------------------------------------------------------------------
DSPNYM:
LB498:  LDY  #NYMCOL
LB49A:  STY  COLOR
LB49C:  LDA  #MZCOLO
LB49E:  JSR  VGSTAT
LB4A1:  LDX  #XADJL
LB4A3:  JSR  VGYAB1                 ;POSITION BEAM AT VANISH PT.
LB4A6:  LDA  #$12
LB4A8:  STA  PXL                    ;MAX # DISPLAYABLE
LB4AA:  LDX  #NNYMPH-1
LB4AC:  STX  INDEX1
LB4AE:  LDY  #$00
LB4B0:  LDX  INDEX1
LB4B2:  LDA  NYMPY,X
LB4B5:  BNE  LB4BA                  ;IFEQ  NYMPH ACTIVE?
LB4B7:  JMP  NONYM                  ;NO. SKIP IT
LB4BA:  CMP  #$50                   ;YES.
LB4BC:  BCC  LB4C0                  ;IFCS  SKIP EVERY OTHER ONE PAST THIS DEPTH
LB4BE:  DEC  INDEX1
LB4C0:  PHA
LB4C1:  AND  #$3F                   ;FAKE PROJECTION (USE NYMPH DEPTH TO GET SCALES)
LB4C3:  STA  (VGLIST),Y             ;LINEAR SCALE
LB4C5:  PLA
LB4C6:  ROL
LB4C7:  ROL
LB4C8:  ROL
LB4C9:  AND  #$03
LB4CB:  CLC
LB4CC:  ADC  #$01
LB4CE:  ORA  #$70
LB4D0:  INY
LB4D1:  STA  (VGLIST),Y             ;BINARY SCALE
LB4D3:  INY
LB4D4:  LDA  NYMPL,X                ;GET NYMPH LINE
LB4D7:  TAX
LB4D8:  LDA  LIFSZL,X               ;VECTOR TO NYMPH
LB4DB:  SEC
LB4DC:  SBC  ZADJL
LB4DE:  STA  SZL
LB4E0:  STA  (VGLIST),Y             ;Z LSB
LB4E2:  INY
LB4E3:  LDA  LIFSZH,X
LB4E6:  SBC  ZADJL+1
LB4E8:  STA  SZH
LB4EA:  AND  #$1F
LB4EC:  STA  (VGLIST),Y             ;Z MSB
LB4EE:  INY
LB4EF:  LDA  LIFSXL,X
LB4F2:  STA  SXL
LB4F4:  STA  (VGLIST),Y             ;X LSB
LB4F6:  INY
LB4F7:  LDA  LIFSXH,X
LB4FA:  STA  SXH
LB4FC:  AND  #$1F
LB4FE:  STA  (VGLIST),Y             ;X MSB
LB500:  INY                         ;DISPLAY A DOT
LB501:  LDA  #$00
LB503:  STA  (VGLIST),Y             ;0 Z LSB
LB505:  INY
LB506:  STA  (VGLIST),Y             ;0 Z MSB
LB508:  INY
LB509:  STA  (VGLIST),Y             ;0 X LSB
LB50B:  LDA  #$A0
LB50D:  INY
LB50E:  STA  (VGLIST),Y             ;BRIGHTNESS, 0 X MSB
LB510:  INY
LB511:  LDA  SZL                    ;DRAW VECTOR BACK TO FAKE V.P.
LB513:  EOR  #$FF
LB515:  CLC
LB516:  ADC  #$01
LB518:  STA  (VGLIST),Y             ;Z LSB
LB51A:  INY
LB51B:  LDA  SZH
LB51D:  EOR  #$FF
LB51F:  ADC  #$00
LB521:  AND  #$1F
LB523:  STA  (VGLIST),Y             ;Z MSB
LB525:  INY
LB526:  LDA  SXL
LB528:  EOR  #$FF
LB52A:  CLC
LB52B:  ADC  #$01
LB52D:  STA  (VGLIST),Y             ;X LSB
LB52F:  INY
LB530:  LDA  SXH
LB532:  EOR  #$FF
LB534:  ADC  #$00
LB536:  AND  #$1F
LB538:  STA  (VGLIST),Y             ;X MSB
LB53A:  INY
LB53B:  CPY  #$F0
LB53D:  BCC  LB545                  ;IFCS  VGLIST LSB INDEX MAXING OUT?
LB53F:  DEY
LB540:  JSR  VGADD                  ;YES. UPDATE VGLIST
LB543:  LDY  #$00                   ;RESET LSB INDEX
LB545:  DEC  PXL                    ;EXIT EARLY IF MAX
LB547:  BMI  EXCESS                 ;LIMIT REACHED

NONYM:
LB549:  DEC  INDEX1
LB54B:  BMI  EXCESS                 ;MIEND  EXIT LOOP AFTER LAST NYMPH
LB54D:  JMP  LB4B0

;------------------------------------------------------------------------------
; EXCESS - [note] End of the nymph display loop: VGADD Y-1 bytes if Y is non-zero, then the ZQATLI check
;   (QT1 non-zero and wave >= 10 -> FRTIMR = $7A) and VGSCA1 with scale 1.
;------------------------------------------------------------------------------
EXCESS:
LB550:  TYA
LB551:  BEQ  ZQATLI                 ;IFNE
LB553:  DEY
LB554:  JSR  VGADD                  ;UPDATE VGLIST

ZQATLI:
LB557:  LDA  QT1
LB559:  BEQ  LB565                  ;IFNE
LB55B:  LDA  WAVEN1                 ;[CS] Load current level for player 1.
LB55D:  CMP  #$0A
LB55F:  BCC  LB565                  ;IFCS
LB561:  LDA  #$7A                   ;[CS] Set the 0-9 timer as "7A".
LB563:  STA  FRTIMR                 ;[CS] NOTE: If it reaches 80, the interrupt code will do a BREAK.
LB565:  LDA  #$01
LB567:  JMP  VGSCA1

;------------------------------------------------------------------------------
; VGDOT - DOT  (comment at the call)
;   DRAW DOT IN SUBROUTINE  (another call)
;------------------------------------------------------------------------------
VGDOT:
LB56A:  PHA
LB56B:  LDY  #$00                   ;DRAW A DOT
LB56D:  TYA
LB56E:  STA  (VGLIST),Y
LB570:  INY
LB571:  STA  (VGLIST),Y
LB573:  INY
LB574:  STA  (VGLIST),Y
LB576:  INY
LB577:  PLA
LB578:  STA  (VGLIST),Y
LB57A:  LDA  #$04                   ;UPDATE DISPLAY POINTER
LB57C:  CLC
LB57D:  ADC  VGLIST
LB57F:  STA  VGLIST
LB581:  BCC  LB585                  ;IFCS
LB583:  INC  VGLIST+1
LB585:  RTS

;------------------------------------------------------------------------------
; DSPCUR - DISPLAY-CURSOR
;------------------------------------------------------------------------------
DSPCUR:
LB586:  LDA  #CURCOL
LB588:  STA  COLOR
LB58A:  LDA  CURSY
LB58D:  BEQ  LB5AC                  ;IFNE
LB58F:  CMP  #ILINDDY
LB591:  BCS  LB5AC                  ;IFCC  AT BOTTOM?
LB593:  STA  PYL                    ;NO. DEPTH
LB595:  STA  TEMPY
LB597:  LDA  CURSL2                 ;[CS] Is the player dead?
LB59A:  CMP  #$81
LB59C:  BEQ  LB5AC                  ;IFNE  DON'T DISPLAY BLASTED CURSOR
LB59E:  LDY  CURSL1                 ;CURSOR'S WELL LINE #S
LB5A1:  LDA  CURSPO                 ;GET CURSOR POSITION BETWEEN LINES  [CS] Grab the player's tunnel position
LB5A3:  LSR
LB5A4:  AND  #$07
LB5A6:  CLC
LB5A7:  ADC  #CNCURS                ;ADD IN BASE PIC #
LB5A9:  JSR  ONELIN                 ;DRAW LINE
LB5AC:  RTS

;------------------------------------------------------------------------------
; DSPINV - DISPLAY-INVADERS (MAINLINE)
;------------------------------------------------------------------------------
DSPINV:
LB5AD:  LDA  CURMOD
LB5B0:  BMI  LB5D6                  ;IFPL  CURSOR AT TOP?
LB5B2:  LDX  #NINVAD-1              ;YES
LB5B4:  STX  INDEX1
LB5B6:  LDX  INDEX1
LB5B8:  LDA  INVAY,X
LB5BB:  BEQ  LB5D2                  ;IFNE  ACTIVE?
LB5BD:  STA  PYL                    ;YES
LB5BF:  LDA  INVAC1,X
LB5C2:  AND  #INVSEQ                ;GET ANIMATION SEQUENCE
LB5C4:  LSR
LB5C5:  LSR
LB5C6:  LSR
LB5C7:  STA  OBJIND
LB5C9:  LDA  INVAC1,X
LB5CC:  AND  #INVABI
LB5CE:  ASL
LB5CF:  JSR  INVPIC                 ;DRAW INVADER PIC
LB5D2:  DEC  INDEX1
LB5D4:  BPL  LB5B6                  ;MIEND
LB5D6:  RTS

;------------------------------------------------------------------------------
; INVPIC - DISPLAY - INVADERS PICS
;------------------------------------------------------------------------------
INVPIC:
LB5D7:  TAY                         ;INDIRECT JSR TO PIC DRAW ROUTINE
LB5D8:  LDA  INVPIT+1,Y
LB5DB:  PHA
LB5DC:  LDA  INVPIT,Y
LB5DF:  PHA
LB5E0:  RTS

INVPIT:
LB5E1:  .word FLIPIC-1              ;FLIPPER
LB5E3:  .word PULPIC-1              ;PULSAR
LB5E5:  .word TANPIC-1              ;TANKER
LB5E7:  .word TRAPIC-1              ;TRALER
LB5E9:  .word FUSPIC-1              ;FUSE

;------------------------------------------------------------------------------
; INVPIE - DISPLAY - FLIPPERS
;   FLIPPER PIC
;   (also: FLIPIC)
;------------------------------------------------------------------------------
INVPIE:
FLIPIC:
LB5EB:  LDA  #FLICOL                ;[CS] Re-disassemble. Confused by data segment.
LB5ED:  STA  COLOR
LB5EF:  LDA  INVAC1,X
LB5F2:  BMI  LB602                  ;IFPL  FLIPPING?
LB5F4:  LDY  INVAL1,X               ;LINE #
LB5F7:  LDX  OBJIND
LB5F9:  LDA  FLITAB,X
LB5FC:  JSR  ONELIN                 ;NO. ON LINES
LB5FF:  CLV                         ;ELSE
LB600:  BVC  LB60A
LB602:  JSR  IJMPDS                 ;YES. SET UP SPECIAL COORDS
LB605:  LDY  #CINVA1
LB607:  JSR  ONELN2                 ;FLIPPING PIC
LB60A:  RTS

FLITAB:
;ANIMATION SEQUENCE
LB60B:  .byte CINVA1, CINVA1, CINVA1, CINVA1

;------------------------------------------------------------------------------
; TANPIC - DISPLAY - TANKERS
;------------------------------------------------------------------------------
TANPIC:
LB60F:  LDA  INVAC2,X
LB612:  AND  #INVCAR
LB614:  TAY                         ;INDEX FOR TYPE CARRIED
LB615:  LDA  TANTAB,Y
LB618:  LDY  INVAL1,X
LB61B:  JMP  SCAPIC                 ;DRAW TANKER PIC

TANTAB:
;ANIMATION SEQUENCE
LB61E:  .byte PTTANK, PTTANK, PTTANP, PTTANF

;------------------------------------------------------------------------------
; TRAPIC - DISPLAY - INVADERS (DRAW TRAILER)
;------------------------------------------------------------------------------
TRAPIC:
LB622:  LDY  INVAL1,X
LB625:  LDA  QFRAME                 ;CHOOSE BETWEEN 4 PICS
LB627:  AND  #$03
LB629:  ASL
LB62A:  CLC
LB62B:  ADC  #PTSPI1
LB62D:  JMP  SCAPIC                 ;DRAW TRALER PIC

TRATAB:
LB630:  .byte PTSPI1, PTSPI1+2
LB632:  .byte PTSPI1+4, PTSPI1+6

;------------------------------------------------------------------------------
; IJMPDS - DISPLAY-INVADERS (DRAW JUMP INVADER)
;------------------------------------------------------------------------------
IJMPDS:
LB634:  LDA  PYL                    ;SAME Y FOR BOTH PTS.
LB636:  STA  TEMPY
LB638:  LDY  INVAL1,X
LB63B:  LDA  LINEX,Y                ;X AND Z FOR BASE LEG
LB63E:  STA  PXL
LB640:  LDA  LINEZ,Y
LB643:  STA  PZL
LB645:  LDA  INVAL2,X
LB648:  AND  #$0F
LB64A:  TAY
LB64B:  LDA  PXL                    ;CALCULATE COORD OF JUMPING ENDPT
LB64D:  EOR  #$80
LB64F:  CLC
LB650:  ADC  JUMPX,Y
LB653:  BVC  LB65E                  ;IFVS  OVERFLOW?
LB655:  BPL  LB65C                  ;IFMI  YES
LB657:  LDA  #$7F                   ;MIN
LB659:  CLV                         ;ELSE
LB65A:  BVC  LB65E
LB65C:  LDA  #$80                   ;MAX
LB65E:  EOR  #$80
LB660:  STA  TEMPX
LB662:  LDA  PZL
LB664:  EOR  #$80
LB666:  CLC
LB667:  ADC  JUMPZ,Y
LB66A:  BVC  LB675                  ;IFVS  OVERFLOW?
LB66C:  BPL  LB673                  ;IFMI  YES.
LB66E:  LDA  #$7F
LB670:  CLV                         ;ELSE
LB671:  BVC  LB675
LB673:  LDA  #$80                   ;MAX
LB675:  EOR  #$80
LB677:  STA  TEMPZ
LB679:  LDY  WELLID
LB67C:  LDA  WELLIS,Y               ;LINEAR SCALE
LB67F:  STA  LINSCA
LB681:  LDA  WELBIN,Y               ;BINARY SCALE
LB684:  STA  BINSCA                 ;SET UP DOWN SCALE (APPROX 1/8)
LB686:  RTS

;.SBTTL TABLE-WORLD COORD OFFSETS (X,Z) FOR JUMPERS

JUMPZ:
LB687:  .byte $00                   ;(src: .BYTE DG900)
LB688:  .byte $10                   ;(src: .BYTE DG675)
LB689:  .byte $1F                   ;(src: .BYTE DG450)
LB68A:  .byte $28                   ;(src: .BYTE DG225)

JUMPX:
LB68B:  .byte DG000                 ;[CS] Strange. Nothing at that addr?
LB68C:  .byte $28                   ;(src: .BYTE DG225)
LB68D:  .byte $1F                   ;(src: .BYTE DG450)
LB68E:  .byte $10                   ;(src: .BYTE DG675)
LB68F:  .byte $00                   ;(src: .BYTE DG900)
LB690:  .byte $F0                   ;(src: .BYTE -DG675)
LB691:  .byte $E1                   ;(src: .BYTE -DG450)
LB692:  .byte $D8                   ;(src: .BYTE -DG225)
LB693:  .byte <[-DG000]
LB694:  .byte $D8                   ;(src: .BYTE -DG225)
LB695:  .byte $E1                   ;(src: .BYTE -DG450)
LB696:  .byte $F0                   ;(src: .BYTE -DG675)
LB697:  .byte $00                   ;(src: .BYTE DG900)
LB698:  .byte $10                   ;(src: .BYTE DG675)
LB699:  .byte $1F                   ;(src: .BYTE DG450)
LB69A:  .byte $28                   ;(src: .BYTE DG225)

;------------------------------------------------------------------------------
; FUSPIC - DISPLAY-INVADE FUSE PICTURE
;------------------------------------------------------------------------------
FUSPIC:
LB69B:  LDA  INVAY,X
LB69E:  STA  PYL
LB6A0:  LDY  INVAL1,X
LB6A3:  LDA  LINEX,Y
LB6A6:  STA  PXL
LB6A8:  LDA  LINEZ,Y
LB6AB:  STA  PZL
LB6AD:  LDA  INVAL2,X

M10:
LB6B0:  BPL  LB6D5                  ;IFMI  RUNGING?
LB6B2:  TYA                         ;YES.
LB6B3:  CLC
LB6B4:  ADC  #$01
LB6B6:  AND  #$0F
LB6B8:  TAY
LB6B9:  LDA  LINEX,Y
LB6BC:  SEC
LB6BD:  SBC  PXL
LB6BF:  JSR  DELTA8
LB6C2:  CLC
LB6C3:  ADC  PXL
LB6C5:  STA  PXL
LB6C7:  LDA  LINEZ,Y
LB6CA:  SEC
LB6CB:  SBC  PZL
LB6CD:  JSR  DELTA8
LB6D0:  CLC
LB6D1:  ADC  PZL
LB6D3:  STA  PZL
LB6D5:  JSR  WORSCR
LB6D8:  LDX  #SXL
LB6DA:  JSR  VGYAB1                 ;DRAW BLANK VECTOR TO FUSE
LB6DD:  LDA  #$00
LB6DF:  STA  VGY
LB6E1:  JSR  CASCAL                 ;SET PERSPECTIVE SCALE
LB6E4:  STY  VGY
LB6E6:  LDA  QFRAME
LB6E8:  AND  #$03
LB6EA:  ASL
LB6EB:  CLC
LB6EC:  ADC  #PTFUSE
LB6EE:  TAY
LB6EF:  LDX  PICHI,Y
LB6F2:  LDA  PICLO,Y
LB6F5:  LDY  VGY
LB6F7:  JMP  VGADD3                 ;ADD PIC TO DISPLAY LIST

;------------------------------------------------------------------------------
; DELTA8 - OUTPUT:X,Y PRESERVED
;   INPUT:ACC=DELTA BETWEEN LINES
;   X=INVADER INDEX
;   Y=LINE INDEX OF CCW PT
;   ACC=OFFSET FROM BASE FOR MIDPT
;------------------------------------------------------------------------------
DELTA8:
LB6FA:  STA  TEMP0
LB6FC:  LDA  INVAL2,X
LB6FF:  AND  #$07
LB701:  STA  TEMP3
LB703:  STX  TEMP2
LB705:  LDX  #$02
LB707:  LDA  #$00
LB709:  LSR  TEMP3
LB70B:  BCC  LB710                  ;IFCS
LB70D:  CLC
LB70E:  ADC  TEMP0
LB710:  ASL
LB711:  PHP
LB712:  ROR
LB713:  PLP
LB714:  ROR
LB715:  DEX
LB716:  BPL  LB709                  ;MIEND
LB718:  LDX  TEMP2
LB71A:  RTS

;------------------------------------------------------------------------------
; PULPIC - DISPLAY-PULSAR PIC
;------------------------------------------------------------------------------
PULPIC:
LB71B:  LDA  #TURQOI                ;PULSE OFF
LB71D:  LDY  PULSON
LB720:  BMI  LB724                  ;IFPL
LB722:  LDA  #WHITE                 ;PULSE ON
LB724:  STA  COLOR                  ;PULSAR COLOR
LB726:  LDA  PULSON                 ;CALCULATE PIC #
LB729:  CLC
LB72A:  ADC  #$40
LB72C:  LSR
LB72D:  LSR
LB72E:  LSR
LB72F:  LSR
LB730:  CMP  #$05
LB732:  BCC  LB736                  ;IFCS
LB734:  LDA  #$00
LB736:  TAY
LB737:  LDA  PULTAB,Y
LB73A:  STA  TEMP0
LB73C:  LDA  INVAC1,X
LB73F:  BMI  LB74C                  ;IFPL  FLIPPING?
LB741:  LDY  INVAL1,X               ;NO. ON LINES
LB744:  LDA  TEMP0                  ;GET PIC #
LB746:  JSR  ONELIN                 ;DRAW PIC
LB749:  CLV                         ;ELSE
LB74A:  BVC  LB754
LB74C:  JSR  IJMPDS                 ;YES. SET UP SPECIAL COORDS
LB74F:  LDY  TEMP0
LB751:  JSR  ONELN2                 ;FLIPPING PIC
LB754:  RTS

PULTAB:
LB755:  .byte CPULS0, CPULS1, CPULS2, CPULS3, CPULS4, CPULS4

;------------------------------------------------------------------------------
; DSPCHG - DISPLAY-CHARGES
;------------------------------------------------------------------------------
DSPCHG:
LB75B:  LDX  #NCHARG-1
LB75D:  STX  INDEX1
LB75F:  LDX  INDEX1
LB761:  LDA  CHARY,X
LB764:  BEQ  LB781                  ;IFNE  ACTIVE?
LB766:  STA  PYL                    ;YES. BOTH YS ARE SAME
LB768:  STA  TEMPY
LB76A:  CPX  #NPCHARG
LB76C:  LDY  CHARL1,X
LB76F:  BCS  LB776                  ;IFCC
LB771:  LDA  #PTCURS                ;PLAYER SHOT
LB773:  CLV                         ;ELSE
LB774:  BVC  LB77E
LB776:  LDA  QFRAME                 ;ENEMY SHOT
LB778:  ASL
LB779:  AND  #$06
LB77B:  CLC
LB77C:  ADC  #PTESHO
LB77E:  JSR  SCAPIC
LB781:  DEC  INDEX1
LB783:  BPL  LB75F                  ;MIEND
LB785:  LDY  #ZYELLO                ;PLENTY
LB787:  LDA  CHACOU
LB78A:  CMP  #NPCHARG-2
LB78C:  BCC  LB796                  ;IFCS
LB78E:  LDY  #ZBLUE                 ;LOW
LB790:  CMP  #NPCHARG
LB792:  BCC  LB796                  ;IFCS
LB794:  LDY  #ZRED                  ;OUT
LB796:  STY  COLPORT+PSHCTR         ;SET UP COLOR FOR CENTER OF PLAYER SOT
LB799:  RTS

;------------------------------------------------------------------------------
; DSPEXP - DISPLAY-EXPLOSIONS
;------------------------------------------------------------------------------
DSPEXP:
LB79A:  LDY  #EXPCOL
LB79C:  STY  COLOR
LB79E:  LDX  #NEXPLO-1
LB7A0:  STX  INDEX1
LB7A2:  LDX  INDEX1
LB7A4:  LDA  EXPLOY,X
LB7A7:  BEQ  LB7D2                  ;IFNE  ACTIVE BANG?
LB7A9:  STA  PYL                    ;YES SAVE DEPTH
LB7AB:  LDA  EXPLOL,X               ;SET UP GRID LINES
LB7AE:  STA  TEMP0
LB7B0:  LDY  EXPLOT,X               ;CALC. PICTURE TO USE
LB7B3:  CPY  #$01
LB7B5:  BNE  LB7BD                  ;IFEQ  CHARGE-PLAYER?
LB7B7:  JSR  CHPLKI                 ;YES.
LB7BA:  CLV                         ;ELSE  NO
LB7BB:  BVC  LB7D2
LB7BD:  LDA  EXPLOS,X
LB7C0:  LSR
LB7C1:  AND  #$FE
LB7C3:  CPY  #$02
LB7C5:  BCC  LB7C9                  ;IFCS
LB7C7:  LDA  #$00                   ;NO SEQUENCE TYPE
LB7C9:  CLC
LB7CA:  ADC  TEXTYP,Y
LB7CD:  LDY  TEMP0
LB7CF:  JSR  SCAPIC                 ;DO EXPLOSION PICTURE
LB7D2:  DEC  INDEX1
LB7D4:  BPL  LB7A2                  ;MIEND

ZQPOKS:
LB7D6:  LDA  QT4
LB7D9:  BEQ  LB7E4                  ;IFNE  POKEY DOESN'T STOP
LB7DB:  LDA  CURWAV                 ;[CS] Are we ??? [near] level
LB7DD:  CMP  #$0D                   ;[CS] 14?
LB7DF:  BCC  LB7E4                  ;IFCS
LB7E1:  STA  $01FF                  ;KILL TOP OF STACK
LB7E4:  RTS

TEXTYP:          ;START CODE FOR EACH BANG TYPE
LB7E5:  .byte PTEXP1                ;CHARGE CHARGE, CHARGE INVADER
LB7E6:  .byte $00                   ;CHARGE-PLAYER SEE SPECIAL
LB7E7:  .byte PTFUSX+4              ;BUSE EXPL 1
LB7E8:  .byte PTFUSX+2              ;FUSE EXPL 2
LB7E9:  .byte PTFUSX+0              ;FUSE EXPLOSIN 3
LB7EA:  .byte PTSPAR                ;INVADER - PLAYER COLLISION

;------------------------------------------------------------------------------
; CHPLKI - SPECIAL EXPLOSION CONTROL
;------------------------------------------------------------------------------
CHPLKI:
LB7EB:  LDY  TEMP0
LB7ED:  LDA  LINEXM,Y               ;SET UP MID PT
LB7F0:  STA  PXL
LB7F2:  LDA  LINEZM,Y
LB7F5:  STA  PZL
LB7F7:  JSR  WORSCR                 ;POSITION BEAM FOR EXPLOSION
LB7FA:  LDX  #SXL
LB7FC:  JSR  VGYAB1
;[CS] Start of ROM 136002.118 at $B800.
LB7FF:  LDX  SPXIND
LB802:  DEC  SPFTIM
LB805:  BNE  LB811                  ;IFEQ  UPDATE FRAME TIMER. DONE?
LB807:  INX                         ;YES. NEXT PICTURE
LB808:  STX  SPXIND
LB80B:  LDA  TSPTIM,X
LB80E:  STA  SPFTIM
LB811:  LDY  TSPCOD,X
LB814:  BMI  LB819                  ;IFPL  SPECIAL ROUTINE THIS FRAME?
LB816:  JSR  SPECIAL                ;YES. DO IT
LB819:  LDA  SPXIND
LB81C:  ASL
LB81D:  CLC
LB81E:  ADC  #PTSPLA                ;GET OFFSET INTO TABLE
LB820:  TAY
LB821:  LDX  PICHI,Y
LB824:  LDA  PICLO,Y
LB827:  JMP  VGADD2                 ;MOVE JSRL TO PICTURE TO DISPLAY LIST

;.SBTTL SPECIAL EXPLOSION DATABASE

TSPTIM:
;# OF FRAME/PICTURE
LB82A:  .byte $02                   ;SPLAT6;CHARGE PLAYER EXPLOSION START
LB82B:  .byte $02                   ;SLAT5
LB82C:  .byte $02                   ;SPLAT4
LB82D:  .byte $02                   ;SPLAT3
LB82E:  .byte $02                   ;SPLAT2
LB82F:  .byte $04                   ;SPLAT1
LB830:  .byte $03                   ;SPLAT3
LB831:  .byte $02                   ;SPLAT5

PPSTART:
LB832:  .byte $01                   ;SPLAT6;CHARGE PLAYER EXPLOSION FINISH;START PULSAR PLAYER BANG
LB833:  .byte $20

FPSTART:
LB834:  .byte $03                   ;FUSE PLAYER PICS
LB835:  .byte $03
LB836:  .byte $03
LB837:  .byte $03
LB838:  .byte $03
LB839:  .byte $03
LB83A:  .byte $03
LB83B:  .word LB83B                 ;SHRAP

TSPCOD:
;SPECIAL SUBROUTINE FOR PICTURE
LB83D:  .byte $00                   ;SPLAT6-ALTER COLORS
LB83E:  .byte $02                   ;SLAT5-ROTATE SPLAT COLORS
LB83F:  .byte $02                   ;4
LB840:  .byte $02                   ;3
LB841:  .byte $02                   ;2
LB842:  .byte $02                   ;1
LB843:  .byte $02                   ;3
LB844:  .byte $02                   ;5
LB845:  .byte $04                   ;6 GET SET FOR SHRAPNEL
LB846:  .byte $06                   ;SHRAP CHANGE SCALE VARIABLE
LB847:  .byte $FF                   ;FUSE PLAYER - JUST PICS
LB848:  .byte $FF
LB849:  .byte $FF
LB84A:  .byte $FF
LB84B:  .byte $FF
LB84C:  .byte $FF
LB84D:  .byte $FF

;------------------------------------------------------------------------------
; SPECIAL - SPECIAL EXPLOSION FUNCTION
;   INPUT:Y=INDEX INTO SUBROUTINE ADDRESS TABLE
;------------------------------------------------------------------------------
SPECIAL:
LB84E:  LDA  XSUBR+1,Y
LB851:  PHA
LB852:  LDA  XSUBR,Y
LB855:  PHA
LB856:  RTS

XSUBR:
;[CS] Data segment, per Ken Lui.
LB857:  .word ALTCOL-1              ;ALTER REGULAR COLORS
LB859:  .word ROTCOL-1              ;ROTATE EXPLOSIN COLORS
LB85B:  .word SETSHR-1              ;GET SET FOR SHRAPNEL
LB85D:  .word SHRSCA-1              ;CHANGE SCALE VARIABLE

;------------------------------------------------------------------------------
; ALTCOL - SPECIAL EXPLOSION SUBROUTINE
;   ALTER COLOR
;------------------------------------------------------------------------------
ALTCOL:
LB85F:  LDA  #ZRED                  ;SET UP SLAT COLORS
LB861:  STA  COLPORT+PDIRED
LB864:  STA  COLRAM+PDIRED
LB866:  LDA  #ZYELLO
LB868:  STA  COLPORT+PDIYEL
LB86B:  STA  COLRAM+PDIYEL
LB86D:  LDA  #ZWHITE
LB86F:  STA  COLRAM+PDIWHI
LB871:  STA  COLPORT+PDIWHI
LB874:  RTS

;------------------------------------------------------------------------------
; ROTCOL - ROTATE COLORS FOR PLAYER EXPLOSION
;------------------------------------------------------------------------------
ROTCOL:
LB875:  LDY  COLRAM+PDIWHI
LB877:  LDX  #$02
LB879:  LDA  COLRAM+PDIWHI,X
LB87B:  PHA
LB87C:  STY  COLRAM+PDIWHI,X
LB87E:  TYA
LB87F:  STA  COLPORT+PDIWHI,X
LB882:  PLA
LB883:  TAY
LB884:  DEX
LB885:  BPL  LB879                  ;MIEND
LB887:  RTS

;------------------------------------------------------------------------------
; SETSHR - GET SET FOR SHRAPNEL
;------------------------------------------------------------------------------
SETSHR:
LB888:  JSR  INICOL                 ;RESTORE COLORS
LB88B:  LDA  #$7F                   ;INITIALIZE LINEAR & BINARY SCALES
LB88D:  STA  SPLINE
LB890:  LDA  #$04
LB892:  STA  SPBINA
LB895:  RTS

;------------------------------------------------------------------------------
; SHRSCA - CHANGE SHRAPNEL SCALE VARIABLE
;------------------------------------------------------------------------------
SHRSCA:
LB896:  LDA  SPLINE
LB899:  STA  SCALE                  ;LINEAR SCALE
LB89C:  LDA  SPBINA
LB89F:  ORA  #$70                   ;SCALE OPCODE
LB8A1:  STA  SCALE+1                ;BINARY SCALE
LB8A4:  LDA  #$C0                   ;RTSL
LB8A6:  STA  SCALE+3
LB8A9:  LDA  SPLINE                 ;UPDATE SCALE (BIGGER)
LB8AC:  SEC
LB8AD:  SBC  #$20
LB8AF:  BPL  LB8B6                  ;IFMI  LINEAR OVERLFLOW?
LB8B1:  AND  #$7F                   ;YES.
LB8B3:  DEC  SPBINA                 ;UPDATE BINARY
LB8B6:  STA  SPLINE
LB8B9:  RTS

;------------------------------------------------------------------------------
; DSBOOM - DISPLAY BIG BOOM
;------------------------------------------------------------------------------
DSBOOM:
LB8BA:  LDA  #>[KILLER+1]           ;LAH KILLER+1
LB8BC:  LDX  #<KILLER               ;LXL KILLER
LB8BE:  JSR  VGJSRL                 ;KILL BEAM KILLER
LB8C1:  LDA  #$00                   ;CLEAR CURRENT SCREEN POSITION
LB8C3:  STA  CURNTX
LB8C5:  STA  CURNTX+1
LB8C7:  STA  CURNTY
LB8C9:  STA  CURNTY+1
LB8CB:  STA  CURSY
LB8CE:  STA  ZADJL
LB8D0:  STA  ZADJL+1
LB8D2:  LDA  #$E0
LB8D4:  STA  EYL
LB8D6:  LDA  #$FF
LB8D8:  STA  EYH
LB8DA:  JSR  WHICHB
LB8DD:  STA  SVGLIST+1
LB8DF:  STX  SVGLIST
;SET UP SUBROUTINE PC
LB8E1:  LDX  #NPARTI-1
LB8E3:  STX  INDEX1
LB8E5:  LDX  INDEX1
LB8E7:  LDA  PARTIY,X
LB8EA:  BEQ  LB935                  ;IFNE  ACTIVE PARTICLE?
LB8EC:  STA  PYL
LB8EE:  LDA  PARTIX,X
LB8F1:  STA  PXL
LB8F3:  LDA  PARTIZ,X
LB8F6:  STA  PZL
LB8F8:  JSR  WORSCR                 ;PROJECT PT.
LB8FB:  LDA  #$00
LB8FD:  STA  VGBRIT
LB8FF:  JSR  SWAPVG                 ;SWAP POINTERS TO VG MAINLINE & SUBROUTINE
LB902:  JSR  CONNEC                 ;DRAW VECTOR IN SUBROUTINE
LB905:  LDA  #$A0
LB907:  JSR  VGDOT                  ;DRAW DOT IN SUBROUTINE
LB90A:  JSR  SWAPVG                 ;SWAP MAINLINE TO VG PTRS.
LB90D:  LDX  #SXL
LB90F:  JSR  VGYABS
LB912:  JSR  CALMAG                 ;CALCULATE MAGNIF FACTOR
LB915:  JSR  VGSCAL                 ;Y=LINEAR;ACC=BINARY;PLACE INTO MAINLINE
LB918:  LDA  INDEX1
LB91A:  AND  #$07
LB91C:  CMP  #$07
LB91E:  BNE  LB922                  ;IFEQ
LB920:  LDA  #$00
LB922:  TAY
LB923:  STY  COLOR
LB925:  LDA  #MZCOLO
LB927:  JSR  VGSTAT                 ;PLACE INTO MAINLINE
LB92A:  LDA  #MZBRIT
LB92C:  JSR  VGSTA1                 ;SET INTENSITY
LB92F:  JSR  WHICHB
LB932:  JSR  VGJSRL                 ;PLACE JSRL TO SUBROUTINE INTO MAINLINE
LB935:  DEC  INDEX1
LB937:  BPL  LB8E5                  ;MIEND
LB939:  JSR  SWAPVG                 ;SWAP MAINLINE & SUBROUTINE PTRS.
LB93C:  LDA  #$01                   ;AT END OF SUBROUTINE:
LB93E:  JSR  VGSCA1                 ;RESTORE SCALE
LB941:  JSR  VGRTSL                 ;RTS

;------------------------------------------------------------------------------
; SWAPVG - SWAP POINTERS TO VG MAINLINE & SUBROUTINE  (comment at the call)
;   SWAP MAINLINE TO VG PTRS.  (another call)
;   SWAP MAINLINE & SUBROUTINE PTRS.  (another call)
;------------------------------------------------------------------------------
SWAPVG:
LB944:  LDX  VGLIST                 ;SWAP MAINLINE SUBROUTINE PTRS.
LB946:  LDY  VGLIST+1
LB948:  LDA  SVGLIST
LB94A:  STA  VGLIST
LB94C:  STX  SVGLIST
LB94E:  LDA  SVGLIST+1
LB950:  STA  VGLIST+1
LB952:  STY  SVGLIST+1
LB954:  RTS

;------------------------------------------------------------------------------
; CALMAG - CALCULATE MAGNIF FACTOR  (comment at the call)
;------------------------------------------------------------------------------
CALMAG:
LB955:  LDA  PYL
LB957:  LSR
LB958:  LSR
LB959:  LSR
LB95A:  LSR
LB95B:  LDY  #$00
LB95D:  INY
LB95E:  LSR
LB95F:  BNE  LB95D                  ;EQEND
LB961:  CLC
LB962:  ADC  #$02
LB964:  LDY  #$00
LB966:  RTS

;------------------------------------------------------------------------------
; WHICHB - [note] Return the info sub-buffer start address in A (high) / X (low): BFASTA when
;   BUFACT+BCINFO is non-zero, else BFBSTA.
;------------------------------------------------------------------------------
WHICHB:
LB967:  LDA  BUFACT+BCINFO
LB96A:  BEQ  LB975                  ;IFNE
LB96C:  LDA  BFASTA+1
LB96F:  LDX  BFASTA
LB972:  CLV                         ;ELSE
LB973:  BVC  LB97B
LB975:  LDA  BFBSTA+1
LB978:  LDX  BFBSTA
LB97B:  RTS

;.SBTTL TABLES-WELL COORDINATES(WORLD)

NEWLIX:
LB97C:  .byte DG0, $E7, $CF, $AA, $80 ;CIRCLE  (src: .BYTE DG0,DG225,DG450,DG675,DG900)
LB981:  .byte $56, $31, $19, <[-DG0] ;(src: .BYTE -DG675,-DG450,-DG225,-DG0)
LB985:  .byte $19, $31, $56         ;(src: .BYTE -DG225,-DG450,-DG675)
LB988:  .byte $80, $AA, $CF         ;(src: .BYTE DG900,DG675,DG450)

LCIRCL:
LB98B:  .byte $E7                   ;(src: .BYTE DG225)
LB98C:  .byte DI0, DI0, DI0, DI1    ;SQUARE
LB990:  .byte DI2, DI3, DI4, DI4
LB994:  .byte DI4, DI4, DI4, DI3
LB998:  .byte DI2, DI1, DI0

LDIAMO:
LB99B:  .byte DI0
LB99C:  .byte CR0, CR1, CR2, CR3, CR4 ;CROSS
LB9A1:  .byte <[-CR3], <[-CR2], <[-CR1], <[-CR0]
LB9A5:  .byte <[-CR1], <[-CR2], <[-CR3]
LB9A8:  .byte CR4, CR3, CR2

LCROSS:
LB9AB:  .byte CR1
LB9AC:  .byte PX0, PX1, PX2, PX3    ;PEANUT
LB9B0:  .byte <[-PX3], <[-PX2], <[-PX1], <[-PX0]
LB9B4:  .byte <[-PX0], <[-PX1], <[-PX2], <[-PX3]
LB9B8:  .byte PX3, PX2, PX1

LPEANU:
LB9BB:  .byte PX0
LB9BC:  .byte $F0, $C0, $A0, $94, $6C, $60, $40, $10 ;4 KEY
LB9C4:  .byte $10, $40, $60, $6C, $94, $A0, $C0, $F0
LB9CC:  .byte $D9, $C2, $AC, $97, $80, $69, $52, $3C ;TRIANGLE
LB9D4:  .byte $27, $10
LB9D6:  .byte $35, $5A, $80, $A6, $CA, $F0
LB9DC:  .byte $EA, $E0, $9C, $80, $64, $20, $16, $50 ;CLOVER
LB9E4:  .byte $16, $20, $64, $80, $9C, $E0, $EA, $B0
LB9EC:  .byte $10, $1E, $2C, $3A, $48, $56, $64, $70 ;V
LB9F4:  .byte $90, $9E, $AC, $BA, $C8, $D6, $E4, $F0
LB9FC:  .byte $10, $1E, $2D, $3C, $4B, $5A, $69, $78 ;PLANE
LBA04:  .byte $87
LBA05:  .byte $96, $A5, $B4, $C3, $D2, $E1, $F0
LBA0C:  .byte $10, $10, $10, $10, $16, $29, $46, $69 ;U
LBA14:  .byte $97
LBA15:  .byte $BA, $D7, $EA, $F0, $F0, $F0, $F0
LBA1C:  .byte $10, $24, $30, $36, $3E, $49, $5A, $75 ;JAGGED
LBA24:  .byte $94, $A4, $AC, $BA, $DA, $E2, $EA, $F0
LBA2C:  .byte $80, $70, $48, $20    ;LYING 8
LBA30:  .byte $10, $20, $48, $70
LBA34:  .byte $80, $90, $B8, $E0
LBA38:  .byte $F0, $E0, $B8, $90
LBA3C:  .byte $DA, $A4, $87, $80, $79, $5C, $26, $10 ;HEART
LBA44:  .byte $10, $20, $48, $80, $B8, $E0, $F0, $F0
LBA4C:  .byte $10, $10, $30, $30, $50, $50, $70, $70 ;STAIRCASE
LBA54:  .byte $90, $90, $B0, $B0, $D0, $D0, $F0, $F0
LBA5C:  .byte $B0, $80, $50, $47, $18, $30, $18, $47 ;STAR X
LBA64:  .byte $50, $80, $B0, $B9, $E8, $D4, $E8, $B9
LBA6C:  .byte $10, $1E, $21, $28, $3C, $55, $66, $73 ;WAVE X
LBA74:  .byte $8D, $9A, $AB, $C4, $D8, $DF, $E2, $F0

NEWLIZ:
LBA7C:  .byte $80, $AA, $CF, $E7, DG0 ;CIRCLE  (src: .BYTE DG900,DG675,DG450,DG225,DG0)
LBA81:  .byte $E7, $CF, $AA, $80    ;(src: .BYTE DG225,DG450,DG675,DG900)
LBA85:  .byte $56, $31, $19, <[-DG0] ;(src: .BYTE -DG675,-DG450,-DG225,-DG0)
LBA89:  .byte $19, $31, $56         ;(src: .BYTE -DG225,-DG450,-DG675)
LBA8C:  .byte DI2, DI1, DI0, DI0
LBA90:  .byte DI0, DI0, DI0, DI1
LBA94:  .byte DI2, DI3, DI4, DI4
LBA98:  .byte DI4, DI4, DI4, DI3
LBA9C:  .byte CR4, CR3, CR2, CR1, CR0 ;CROSS
LBAA1:  .byte CR1, CR2, CR3, CR4
LBAA5:  .byte <[-CR3], <[-CR2], <[-CR1], <[-CR0]
LBAA9:  .byte <[-CR1], <[-CR2], <[-CR3]
LBAAC:  .byte PZ0, PZ1, PZ2, PZ3    ;PEANUT
LBAB0:  .byte PZ3, PZ2, PZ1, PZ0
LBAB4:  .byte <[-PZ0], <[-PZ1], <[-PZ2], <[-PZ3]
LBAB8:  .byte <[-PZ3], <[-PZ2], <[-PZ1], <[-PZ0]
LBABC:  .byte $96, $A3, $C5, $F0, $F0, $C5, $A3, $96 ;4 KEY
LBAC4:  .byte $6A, $5D, $3B, $10, $10, $3B, $5D, $6A
LBACC:  .byte $3D, $6A, $97, $C4, $F0, $C4, $97, $6A ;TRIANGLE
LBAD4:  .byte $3D
LBAD5:  .byte $10, $10, $10, $10, $10, $10, $10
LBADC:  .byte $A0, $E0, $EA, $B0, $EA, $E0, $A0, $80 ;CLOVER
LBAE4:  .byte $60, $20, $16, $50, $16, $20, $60, $80
LBAEC:  .byte $F0, $D0, $B0, $90    ;V
LBAF0:  .byte $70, $50, $30, $10
LBAF4:  .byte $10, $30, $50, $70
LBAF8:  .byte $90, $B0, $D0, $F0
LBAFC:  .byte $40, $40, $40, $40, $40, $40, $40, $40 ;.REPT 10  PLANE (LOW)
LBB04:  .byte $40, $40, $40, $40, $40, $40, $40, $40
LBB0C:  .byte $F0, $CB, $A6, $80, $5C, $39, $20, $12 ;U
LBB14:  .byte $12, $20, $39, $5C, $80, $A6, $CB, $F0
LBB1C:  .byte $C0, $A6, $8A, $6A, $4A, $2F, $14, $24 ;JAGGED
LBB24:  .byte $20, $39, $59, $75, $72, $90, $B0, $D0
LBB2C:  .byte $80, $57, $48, $57    ;BIG 8
LBB30:  .byte $80, $A9, $BA, $A9
LBB34:  .byte $80, $57, $48, $57
LBB38:  .byte $80, $A9, $BA, $A9
LBB3C:  .byte $E4, $E8, $B7, $80, $B7, $E8, $E4, $B2 ;HEART
LBB44:  .byte $7A, $47, $20, $10, $20, $47, $7A, $B2
LBB4C:  .byte $90, $70, $70, $50, $50, $30, $30, $10 ;STAIRCASE
LBB54:  .byte $10, $30, $30, $50, $50, $70, $70, $90
LBB5C:  .byte $E6, $D0, $E6, $B9, $AE, $80, $52, $47 ;STAR Z
LBB64:  .byte $14, $30, $14, $47, $52, $80, $AE, $B9
LBB6C:  .byte $7E, $6A, $51, $3A, $2C, $2C, $38, $4E ;WAVE Z
LBB74:  .byte $4E, $38, $2C, $2C, $3A, $51, $6A, $7E

ILINANG:
LBB7C:  .byte $05, $06, $07, $08, $09, $0A, $0B, $0C ;CIRCLE
LBB84:  .byte $0D, $0E, $0F, $00, $01, $02, $03, $04
LBB8C:  .byte $04, $04, $08, $08, $08, $08, $0C, $0C ;SQUARE
LBB94:  .byte $0C, $0C, $00, $00, $00, $00, $04, $04
LBB9C:  .byte $04, $08, $04, $08, $08, $0C, $08, $0C ;CROSS
LBBA4:  .byte $0C, $00, $0C, $00, $00, $04, $00, $04
LBBAC:  .byte $06, $07, $09, $08, $07, $09, $0A, $0C ;PEANUT
LBBB4:  .byte $0E, $0F, $01, $00, $0F, $01, $02, $04
LBBBC:  .byte $07, $06, $05, $08, $0B, $0A, $09, $0C ;4 KEY
LBBC4:  .byte $0F, $0E, $0D, $00, $03, $02, $01, $04
LBBCC:  .byte $05, $05, $05, $05, $0B, $0B, $0B, $0B ;TRIANGLE
LBBD4:  .byte $0B, $00, $00, $00, $00, $00, $00, $05
LBBDC:  .byte $04, $08, $0B, $05, $08, $0C, $0E, $09 ;CLOVER
LBBE4:  .byte $0C, $00, $03, $0D, $00, $04, $07, $02
LBBEC:  .byte $0D, $0D, $0D, $0D, $0D, $0D, $0D, $00 ;V
LBBF4:  .byte $03, $03, $03, $03, $03, $03, $03, $00
LBBFC:  .byte $00, $00, $00, $00, $00, $00, $00, $00 ;FLAT
LBC04:  .byte $00, $00, $00, $00, $00, $00, $00, $00
LBC0C:  .byte $0C, $0C, $0C, $0D, $0E, $0F, $0F, $00 ;U
LBC14:  .byte $01, $01, $02, $03, $04, $04, $04, $00
LBC1C:  .byte $0E, $0D, $0C, $0D, $0D, $0D, $01, $0F ;JAGGED
LBC24:  .byte $02, $03, $03, $00, $03, $03, $03, $00
LBC2C:  .byte $0B, $09, $07, $05, $03, $01, $0F, $0D ;LYING 8
LBC34:  .byte $0D, $0F, $01, $03, $05, $07, $09, $0B
LBC3C:  .byte $08, $0B, $0C, $04, $05, $08, $0B, $0C ;HEART
LBC44:  .byte $0D, $0E, $0F, $01, $02, $03, $04, $05
LBC4C:  .byte $0C, $00, $0C, $00, $0C, $00, $0C, $00 ;STAIRCASE
LBC54:  .byte $04, $00, $04, $00, $04, $00, $04, $00
LBC5C:  .byte $0A, $06, $0C, $08, $0E, $0A, $00, $0C ;STAR ANGLES
LBC64:  .byte $02, $0E, $04, $00, $06, $02, $08, $04
LBC6C:  .byte $0E, $0C, $0D, $0E, $00, $02, $02, $00 ;WAVE ANGLES
LBC74:  .byte $0E, $0E, $00, $02, $03, $04, $02, $00

WELSEQ:
;OTHER WELL PARAMETERS
LBC7C:  .byte $00, $01, $02, $03, $04, $05, $06, $07 ;WELL ID SEQUENCE(WAVE)
LBC84:  .byte $0D, $09, $08, $0C, $0E, $0F, $0A, $0B

WELSEN:
HOLEYL:
LBC8C:  .byte $18, $1C, $18, $0F, $18, $18, $18, $18 ;EYE POSITION (Y)
LBC94:  .byte $0A, $18, $10, $0F, $18, $0C, $14, $0A

HOLEZL:
LBC9C:  .byte $50, $50, $50, $68, $50, $50, $68, $B0 ;EYE POSITION (Z)
LBCA4:  .byte $A0, $50, $90, $80, $20, $B0, $60, $A0

HOLZAD:
LBCAC:  .byte $40, $20, $40, $80, $40, $40, $70, $60 ;CENTER ADJUST
LBCB4:  .byte $00, $20, $40, $00, $A0, $40, $40, $00

HOLZDH:
LBCBC:  .byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $00
LBCC4:  .byte $01, $FF, $00, $00, $FE, $01, $FF, $01

HOLRAP:
LBCCC:  .byte $00, $00, $00, $00, $00, $00, $00, $FF ;PLANAR(-1)/CLOSED(0) FLAG
LBCD4:  .byte $FF, $FF, $FF, $00, $00, $FF, $00, $FF

WELLIS:
LBCDC:  .byte $00, $00, $60, $40, $00, $00, $48, $40 ;LINEAR SCALE FOR JUMPER
LBCE4:  .byte $50, $28, $50, $00, $00, $50, $00, $40

WELBIN:
LBCEC:  .byte $04, $04, $03, $04, $04, $04, $03, $04 ;BINARY SCALE FOR JUMPER
LBCF4:  .byte $05, $04, $04, $04, $04, $04, $04, $05

CHKSM7:
LBCFC:  .byte QCHKS7

;------------------------------------------------------------------------------
; SCAPIC - UTILITY - DISPLAY PIC BETWEEN PTS.
;   FUNCTION: DISPLAY A PICTURE CENTERED BETWEEN 2 POINTS AND SCALED
;   DOWN ACCORDING TO ITS DEPTH
;   INPUT: X = INDEX INTO LINEX,Z OF 1ST PT'S X & Z WC WORDS
;   Y = INDEX INTO LINEX,Z OF 2ND PT'S X & Z WC WORDS
;   COLOR=COLOR OF OBJECT
;   PYL = Y WC COORD FOR BOTH PTS.
;   ACC = CODE FOR PICTURE TO DISPLAY (INDEX INTO PICLO)
;------------------------------------------------------------------------------
SCAPIC:
LBCFD:  STA  OBJIND
LBCFF:  LDA  LINEXM,Y               ;CALCULATE X COORD. OF MIDWAY PT.
LBD02:  STA  PXL
LBD04:  LDA  LINEZM,Y               ;CALCULATE Z COORD OF MIDWAYPT.
LBD07:  STA  PZL

;------------------------------------------------------------------------------
; SCAPI2 - INPUT: PX,Y,Z=LOC OF OBJECT
;   OBJIND=INDEX INTO PTR. TABLE
;   COLOR=COLOR OF OBJECT
;------------------------------------------------------------------------------
SCAPI2:
LBD09:  JSR  WORSCR                 ;PROJECT MIDWAY PT. ONTO SCREEN.
LBD0C:  LDX  #SXL
LBD0E:  JSR  VGYAB1                 ;DRAW BLANK VECTOR TO MIDWAY PT.
LBD11:  LDA  #$00                   ;START AT VGLIST
LBD13:  STA  VGY
LBD15:  JSR  CASCAL                 ;CALCULATE SCALE FOR PT.
LBD18:  LDA  BFACTR
LBD1A:  EOR  #$07
LBD1C:  ASL
LBD1D:  CMP  #$0A
LBD1F:  BCS  LBD23                  ;IFCC
LBD21:  LDA  #$0A
LBD23:  ASL
LBD24:  ASL
LBD25:  ASL
LBD26:  ASL
LBD27:  STA  (VGLIST),Y             ;BRIGHTNESS
LBD29:  INY
LBD2A:  LDA  #$60
LBD2C:  STA  (VGLIST),Y
LBD2E:  INY
LBD2F:  STY  VGY
LBD31:  LDY  OBJIND
LBD33:  LDX  PICHI,Y
LBD36:  LDA  PICLO,Y
LBD39:  LDY  VGY
LBD3B:  JMP  VGADD3                 ;DRAW PIC AT PT.

;------------------------------------------------------------------------------
; CASCAL - UTILITY - DERIVE BINARY AND LINEAR SCALE FACTORS GIVEN DEPTH
;   RTS
;   INPUT: PYL = OBJECT DEPTH ;EYL,H=EYEPOSITION
;   VGY=OFFSET INTO VGLIST
;   OUTPUT:BFACTR,BINARY TO LINEAR SCALE FACTORS READY FOR VGSCAL
;   ACC = BFACTR ;Y=LFACTR
;------------------------------------------------------------------------------
CASCAL:
LBD3E:  LDA  PYL                    ;CALCULATE YDELTAS
LBD40:  CMP  #$10                   ;***
LBD42:  BCC  LBD8C                  ;IFCS
LBD44:  SEC
LBD45:  SBC  EYL
LBD47:  STA  MXPL
LBD4A:  LDA  #$00
LBD4C:  SBC  EYH
LBD4E:  STA  MXPH                   ;(Y DELTA FOR PT TO DISPLAY)
LBD51:  LDA  #$18                   ;SET UP MATH BOX TO GIVE FRACTIONAL PORTION
LBD53:  STA  MNL                    ;OF QUOTIENT IN MYHIGH AND MYLOW
LBD56:  LDA  YDEUNI                 ;***
LBD58:  STA  MZLH                   ;(Y DELTA FOR SCALE = 1)
LBD5B:  STA  MSZXD                  ;START DIVIDE (Z/X)
LBD5E:  BIT  MSTAT                  ;[CS] Check mathbox status
LBD61:  BMI  LBD5E                  ;PLEND  EXIT LOOP WHEN DIVIDE IS DONE
LBD63:  LDA  MYLOW                  ;RESULT IS SCALE FACTOR
LBD66:  STA  SCFL
LBD68:  LDA  MYHIGH
LBD6B:  STA  SCFL+1
LBD6D:  LDX  #$0F                   ;RESTORE MATH BOX QUOTIENT SIZE
LBD6F:  STX  MNL
LBD72:  SEC
LBD73:  SBC  #$01
LBD75:  BNE  LBD79                  ;IFEQ
LBD77:  LDA  #$01
LBD79:  LDX  #$00
LBD7B:  INX
LBD7C:  ASL  SCFL
LBD7E:  ROL
LBD7F:  BCC  LBD7B                  ;CSEND
LBD81:  LSR
LBD82:  EOR  #$7F
LBD84:  CLC
LBD85:  ADC  #$01
LBD87:  TAY
LBD88:  TXA
LBD89:  CLV                         ;ELSE
LBD8A:  BVC  LBD90
LBD8C:  LDA  #$01                   ;SET MAX SCALE FACTOR(1)
LBD8E:  LDY  #$00
LBD90:  STA  BFACTR
LBD92:  PHA
LBD93:  TYA
LBD94:  LDY  VGY
LBD96:  STA  (VGLIST),Y             ;LINEAR FACTOR
LBD98:  INY
LBD99:  PLA
LBD9A:  ORA  #$70                   ;SCALE OPCODE
LBD9C:  STA  (VGLIST),Y             ;BINARY FACTOR
LBD9E:  INY                         ;RETURN WITH Y PT TO NEXT VG SLOT
LBD9F:  RTS

;------------------------------------------------------------------------------
; ONELIN - UTILITY-DRAW OBJECT BETWEEN POINTS
;   INPUT:
;   Y=INDEX INTO LEXEX,LINEZ OF 2ND POINT'S X & Z WC COORDS
;   ACC =INDEX INTO PCOUNT & PINDEX, USED TO SET UP INDEX1 AND SUBCOU
;   INDEX1:OFFSET INTO SUBVEC
;   ARRAYS OF 1ST VECTOR PARAMETERE OF OBJECT
;   SUBCOU:# OF VECTORS TO BE DRAWN
;   PYL,=1ST POINT (WC)
;------------------------------------------------------------------------------
ONELIN:
LBDA0:  STA  SAVEY
LBDA2:  LDA  LINEX,Y
LBDA5:  STA  PXL
LBDA7:  LDA  LINEZ,Y
LBDAA:  STA  PZL
LBDAC:  LDA  PYL
LBDAE:  STA  TEMPY
LBDB0:  TYA
LBDB1:  CLC                         ;CALCULATE ADJACENT CW LINE #
LBDB2:  ADC  #$01
LBDB4:  AND  #$0F
LBDB6:  TAX
LBDB7:  LDA  LINEX,X
LBDBA:  STA  TEMPX
LBDBC:  LDA  LINEZ,X
LBDBF:  STA  TEMPZ
LBDC1:  LDA  #$00                   ;SET UP FOR 1,16. SCALE
LBDC3:  STA  LINSCA
LBDC5:  LDA  #$04
LBDC7:  STA  BINSCA
LBDC9:  LDY  SAVEY

;------------------------------------------------------------------------------
; ONELN2 - INPUT: Y=PIC #
;   INPUT: Y=PIC ID
;   TEMPX,TEMPY,TEMPZ=RIGHT PT.WC
;   PXL,PYL,PZL=LEFT PT.WC
;------------------------------------------------------------------------------
ONELN2:
LBDCB:  LDA  EYH
LBDCD:  BMI  LBDD6                  ;IFPL  IF LINE WOULD BE BEHIND EYE
LBDCF:  LDA  PYL
LBDD1:  CMP  EYL
LBDD3:  BCS  LBDD6                  ;IFCC
LBDD5:  RTS                         ;THEN ABORT LINE
LBDD6:  LDA  PCOUNT,Y
LBDD9:  STA  SUBCOU
LBDDB:  LDA  PINDEX,Y
LBDDE:  STA  INDEX2
LBDE0:  LDY  COLOR
LBDE2:  LDA  #MZCOLO
LBDE4:  JSR  VGSTAT                 ;SET BEAM COLOR
;JSR SETINT ;SET INTENSITY AS FUNC OF PYL
LBDE7:  JSR  WORSCR                 ;PROJECT 1ST POINT ONTO SCREEN
LBDEA:  LDX  #SXL
LBDEC:  JSR  VGYAB1                 ;POSITION BEAM AT 1ST POINT
;SAVE SCREEN COORDS OF 1ST POINT
LBDEF:  LDA  TEMPX
LBDF1:  STA  PXL
LBDF3:  LDA  TEMPY
LBDF5:  STA  PYL
LBDF7:  LDA  TEMPZ
LBDF9:  STA  PZL
LBDFB:  JSR  WORSCR                 ;PROJECT 2ND POINT ONTO SCREEN
;CALCULATE + AND - UNIT AND PERPENDICULAR
;UNIT VECTORS FOR THESE 2 POINTS
LBDFE:  LDY  LINSCA
LBE00:  LDA  BINSCA
LBE02:  JSR  VGSCAL                 ;REDUCE SCALE BY APPROX. 1/16.
LBE05:  LDA  SXL                    ;CALCULATE VECTOR FROM ONE ENDPT TO OTHER
LBE07:  SEC                         ;IN SCREEN UNITS (UNIT VECTOR)
LBE08:  SBC  CURNTX
LBE0A:  STA  X1L
LBE0C:  LDA  SXH
LBE0E:  SBC  CURNTX+1
LBE10:  STA  UNITXH
LBE12:  BMI  LBE1D                  ;IFPL  MAXIMIZE AT 1 BYTE
LBE14:  BEQ  LBE1A                  ;IFNE  PLUS. > 1 BYTE?
LBE16:  LDA  #$FF                   ;YES MAX OUT
LBE18:  STA  X1L
LBE1A:  CLV                         ;ELSE
LBE1B:  BVC  LBE33
LBE1D:  CMP  #$FF                   ;MINUS.
LBE1F:  BEQ  LBE26                  ;IFNE  > 1 BYTE?
LBE21:  LDA  #$FF                   ;YES. MAX OUT
LBE23:  CLV                         ;ELSE
LBE24:  BVC  LBE31
LBE26:  LDA  X1L                    ;NO. NEGATE FOR ABS VALUE
LBE28:  EOR  #$FF
LBE2A:  CLC
LBE2B:  ADC  #$01
LBE2D:  BCC  LBE31                  ;IFCS
LBE2F:  LDA  #$FF
LBE31:  STA  X1L
LBE33:  LDA  SZL
LBE35:  SEC
LBE36:  SBC  CURNTY
LBE38:  STA  Z1L
LBE3A:  LDA  SZH
LBE3C:  SBC  CURNTY+1
LBE3E:  STA  UNITZH
LBE40:  BMI  LBE4B                  ;IFPL  MAXIMIZE AT 1 BYTE
LBE42:  BEQ  LBE48                  ;IFNE  PLUS. > 1 BYTE?
LBE44:  LDA  #$FF                   ;YES. MAX OUT
LBE46:  STA  Z1L
LBE48:  CLV                         ;ELSE
LBE49:  BVC  LBE5D
LBE4B:  CMP  #$FF                   ;MINUS. > BYTE?
LBE4D:  BEQ  LBE54                  ;IFNE
LBE4F:  LDA  #$FF                   ;YES. MAX OUT
LBE51:  CLV                         ;ELSE
LBE52:  BVC  LBE5B
LBE54:  LDA  Z1L                    ;NO. NEGATE FOR ABS. VALUE
LBE56:  EOR  #$FF
LBE58:  CLC
LBE59:  ADC  #$01
LBE5B:  STA  Z1L
LBE5D:  LDA  #$00
LBE5F:  STA  X2H
LBE61:  STA  Z2H
;90 CYCLES FOR X ;CALCULATE UNITXL X 0 THRU 7
LBE63:  LDA  X1L
LBE65:  ASL
LBE66:  ROL  X2H                    ;X2
LBE68:  STA  X2L
LBE6A:  ASL
LBE6B:  STA  X4L                    ;X4
LBE6D:  LDA  X2H
LBE6F:  ROL
LBE70:  STA  X4H
LBE72:  LDA  X4L
;CLC
LBE74:  ADC  X1L
LBE76:  STA  X5L                    ;X5
LBE78:  LDA  X4H
LBE7A:  ADC  #$00
LBE7C:  STA  X5H
LBE7E:  LDA  X2L
;CLC
LBE80:  ADC  X1L
LBE82:  STA  X3L                    ;X3
LBE84:  LDA  X2H
LBE86:  ADC  #$00
LBE88:  STA  X3H
LBE8A:  STA  X6H                    ;X6
LBE8C:  LDA  X3L
LBE8E:  ASL
LBE8F:  STA  X6L
LBE91:  ROL  X6H
;CLC
LBE93:  ADC  X1L
LBE95:  STA  X7L                    ;X7
LBE97:  LDA  X6H
LBE99:  ADC  #$00
LBE9B:  STA  X7H
;90 CYCLES FOR Z
;CALCULATE UNITZL X 0 THRU 7
LBE9D:  LDA  Z1L
LBE9F:  ASL
LBEA0:  ROL  Z2H
LBEA2:  STA  Z2L                    ;X2
LBEA4:  ASL
LBEA5:  STA  Z4L
LBEA7:  LDA  Z2H
LBEA9:  ROL
LBEAA:  STA  Z4H                    ;X4
LBEAC:  LDA  Z4L
;CLC
LBEAE:  ADC  Z1L
LBEB0:  STA  Z5L                    ;X5
LBEB2:  LDA  Z4H
LBEB4:  ADC  #$00
LBEB6:  STA  Z5H
LBEB8:  LDA  Z2L
;CLC
LBEBA:  ADC  Z1L
LBEBC:  STA  Z3L                    ;X3
LBEBE:  LDA  Z2H
LBEC0:  ADC  #$00
LBEC2:  STA  Z3H
LBEC4:  STA  Z6H                    ;X6
LBEC6:  LDA  Z3L
LBEC8:  ASL
LBEC9:  STA  Z6L
LBECB:  ROL  Z6H
;CLC
LBECD:  ADC  Z1L
LBECF:  STA  Z7L                    ;X7
LBED1:  LDA  Z6H
LBED3:  ADC  #$00
LBED5:  STA  Z7H
LBED7:  LDY  #$00
LBED9:  STY  VGY
LBEDB:  LDY  INDEX2
LBEDD:  LDA  VBASE+1,Y
LBEE0:  CMP  #$01
LBEE2:  BNE  LBEE6                  ;IFEQ  USE DEPTH INTENSITY?
LBEE4:  LDA  #RATS                  ;YES.
LBEE6:  STA  VGBRIT
LBEE8:  LDA  VBASE,Y                ;GET MULTIPLIER'S
LBEEB:  STA  TEMP4                  ;SIGN FOR PERP. UNIT VECTOR MULT.
LBEED:  INY
LBEEE:  INY
LBEEF:  STY  INDEX2
LBEF1:  TAX
LBEF2:  AND  #$07                   ;GET UNIT VECTOR MULTIPLIER
LBEF4:  TAY                         ;ABS. VALUE
LBEF5:  TXA
LBEF6:  ASL
LBEF7:  STA  TEMP2                  ;SIGN FOR UNIT VEC MULT
LBEF9:  LSR
LBEFA:  LSR
LBEFB:  LSR
LBEFC:  LSR
LBEFD:  AND  #$07                   ;GET PERP UNIT VECTOR MULTIPLIER
LBEFF:  TAX                         ;ABSOLUTE VALUE
LBF00:  LDA  TEMP2
LBF02:  EOR  UNITXH
LBF04:  BMI  LBF11                  ;IFPL  ACC TO SIGNS, UPDATE VECTOR ACCUMULATOR
LBF06:  .byte $B9, $78, $00         ;LDA X0L,Y (forced absolute)  POSITIVE RESULTS
LBF09:  STA  SXL
LBF0B:  .byte $B9, $80, $00         ;LDA X0H,Y (forced absolute)
LBF0E:  CLV                         ;ELSE
LBF0F:  BVC  LBF22
LBF11:  .byte $B9, $78, $00         ;LDA X0L,Y (forced absolute)  NEGATIVE RESULTS
LBF14:  EOR  #$FF
LBF16:  CLC
LBF17:  ADC  #$01
LBF19:  STA  SXL
LBF1B:  .byte $B9, $80, $00         ;LDA X0H,Y (forced absolute)
LBF1E:  EOR  #$FF
LBF20:  ADC  #$00
LBF22:  STA  SXH
LBF24:  LDA  TEMP4
LBF26:  EOR  UNITZH
LBF28:  BPL  LBF38                  ;IFMI  ACC. TO SIGNS UPDATE VECTOR ACCUMULATOR
LBF2A:  LDA  Z0L,X                  ;POSITIVE RESULTS
LBF2C:  CLC
LBF2D:  ADC  SXL
LBF2F:  STA  SXL
LBF31:  LDA  Z0H,X
LBF33:  ADC  SXH
LBF35:  CLV                         ;ELSE
LBF36:  BVC  LBF43
LBF38:  LDA  SXL                    ;NEGATIVE RESULTS
LBF3A:  SEC
LBF3B:  SBC  Z0L,X
LBF3D:  STA  SXL
LBF3F:  LDA  SXH
LBF41:  SBC  Z0H,X
LBF43:  STA  SXH
;NOW CALCULATE Z VECTOR
LBF45:  LDA  TEMP2
LBF47:  EOR  UNITZH
LBF49:  BMI  LBF56                  ;IFPL
LBF4B:  .byte $B9, $88, $00         ;LDA Z0L,Y (forced absolute)
LBF4E:  STA  SZL
LBF50:  .byte $B9, $90, $00         ;LDA Z0H,Y (forced absolute)
LBF53:  CLV                         ;ELSE
LBF54:  BVC  LBF67
LBF56:  .byte $B9, $88, $00         ;LDA Z0L,Y (forced absolute)
LBF59:  EOR  #$FF
LBF5B:  CLC
LBF5C:  ADC  #$01
LBF5E:  STA  SZL
LBF60:  .byte $B9, $90, $00         ;LDA Z0H,Y (forced absolute)
LBF63:  EOR  #$FF
LBF65:  ADC  #$00
LBF67:  STA  SZH
LBF69:  LDA  TEMP4
LBF6B:  EOR  UNITXH
LBF6D:  BPL  LBF7D                  ;IFMI
LBF6F:  LDA  SZL
LBF71:  SEC
LBF72:  SBC  X0L,X
LBF74:  STA  SZL
LBF76:  LDA  SZH
LBF78:  SBC  X0H,X
LBF7A:  CLV                         ;ELSE
LBF7B:  BVC  LBF88
LBF7D:  LDA  SZL
LBF7F:  CLC
LBF80:  ADC  X0L,X
LBF82:  STA  SZL
LBF84:  LDA  SZH
LBF86:  ADC  X0H,X
LBF88:  STA  SZH
LBF8A:  LDY  VGY                    ;ADD VECTOR TO DISPLAY LIST
LBF8C:  LDA  SZL
LBF8E:  STA  (VGLIST),Y             ;Z LSB
LBF90:  INY
LBF91:  LDA  SZH
LBF93:  AND  #$1F
LBF95:  STA  (VGLIST),Y             ;Z MSB
LBF97:  INY
LBF98:  LDA  SXL
LBF9A:  STA  (VGLIST),Y             ;X LSB
LBF9C:  INY
LBF9D:  LDA  SXH
LBF9F:  AND  #$1F
LBFA1:  ORA  VGBRIT
LBFA3:  STA  (VGLIST),Y             ;X MSB AND INTENSITY
LBFA5:  INY
LBFA6:  STY  VGY
LBFA8:  DEC  SUBCOU
LBFAA:  BEQ  LBFAF                  ;EQEND
LBFAC:  JMP  LBEDB
LBFAF:  LDY  VGY
LBFB1:  DEY
LBFB2:  JMP  VGADD                  ;UPDATE VGLIST PC

C8:
LBFB5:  .byte $08

;.SBTTL PICTURES

PCOUNT:
;  PCOUNT/PINDEX/VBASE: 'between points' pictures. 2 bytes per vector. byte0: D7 sign of perpendicular multiplier, D6 sign of unit multiplier, D5-D3 |perp|, D2-D0 |unit|. Vector = unit*U + perp*P where U is the (scaled, ~1/16) vector from lane point 1 to point 2 and P = U rotated +90 degrees. byte1: 0 beam off, 1 depth-cue intensity, $10 dot, else intensity.
LBFB6:  .byte [INVA1E-INVA1S]/2     ;INVADER 1
LBFB7:  .byte [NCRS1E-NCRS1S]/2
LBFB8:  .byte [NCRS2E-NCRS2S]/2
LBFB9:  .byte [NCRS3E-NCRS3S]/2
LBFBA:  .byte [NCRS4E-NCRS4S]/2
LBFBB:  .byte [NCRS5E-NCRS5S]/2
LBFBC:  .byte [NCRS6E-NCRS6S]/2
LBFBD:  .byte [NCRS7E-NCRS7S]/2
LBFBE:  .byte [NCRS8E-NCRS8S]/2
LBFBF:  .byte [PULS4E-PULS4S]/2
LBFC0:  .byte [PULS3E-PULS3S]/2
LBFC1:  .byte [PULS2E-PULS2S]/2
LBFC2:  .byte [PULS1E-PULS1S]/2
LBFC3:  .byte [PULS0E-PULS0S]/2

PINDEX:
LBFC4:  .byte INVA1S-VBASE          ;MINDX INVA1S  INVADER 1
LBFC5:  .byte NCRS1S-VBASE          ;MINDX NCRS1S
LBFC6:  .byte NCRS2S-VBASE          ;MINDX NCRS2S
LBFC7:  .byte NCRS3S-VBASE          ;MINDX NCRS3S
LBFC8:  .byte NCRS4S-VBASE          ;MINDX NCRS4S
LBFC9:  .byte NCRS5S-VBASE          ;MINDX NCRS5S
LBFCA:  .byte NCRS6S-VBASE          ;MINDX NCRS6S
LBFCB:  .byte NCRS7S-VBASE          ;MINDX NCRS7S
LBFCC:  .byte NCRS8S-VBASE          ;MINDX NCRS8S
LBFCD:  .byte PULS4S-VBASE          ;MINDX PULS4S
LBFCE:  .byte PULS3S-VBASE          ;MINDX PULS3S
LBFCF:  .byte PULS2S-VBASE          ;MINDX PULS2S
LBFD0:  .byte PULS1S-VBASE          ;MINDX PULS1S
LBFD1:  .byte PULS0S-VBASE          ;MINDX PULS0S

VBASE:
CURS4E:
INVA1S:
;  picture INVA1 (8 vectors): Flipper (INVADER 1)
;BYTE 0: D7=SIGN FOR PERP. UNIT VECTOR MULTIPLIER
;D6=SIGN FOR UNIT VECTOR MULTIPLIER
;D5-D3= PERP UNIT VECTOR MULTIPLIER ABS. VALUE
;D2-D0= UNIT VECTOR MULTIPLIER ABS. VALUE
;BYTE 1: 1:USE DEPTH CUE INTENSITY
;0: BEAM OFF
;10:DRAW A DOT
;>10:USE VALUE FOR INTENSITY
LBFD2:  .byte $0C, $01              ;VEC 4,1,1
LBFD4:  .byte $8C, $01              ;VEC 4,-1,1
LBFD6:  .byte $4A, $01              ;VEC -2,1
LBFD8:  .byte $09, $01              ;VEC 1,1
LBFDA:  .byte $CB, $01              ;VEC -3,-1
LBFDC:  .byte $4B, $01              ;VEC -3,1
LBFDE:  .byte $89, $01              ;VEC 1,-1
LBFE0:  .byte $CA, $01              ;VEC -2,-1

INVA1E:
NCRS1S:
;  picture NCRS1 (8 vectors): Player claw (cursor), frame 1
LBFE2:  .byte $90, $01              ;VEC 0,-2
LBFE4:  .byte $8A, $01              ;VEC 2,-1
LBFE6:  .byte $23, $01              ;VEC 3,4
LBFE8:  .byte $DB, $01              ;VEC -3,-3
LBFEA:  .byte $41, $01              ;VEC -1,0
LBFEC:  .byte $10, $01              ;VEC 0,2
LBFEE:  .byte $0A, $01              ;VEC 2,1
LBFF0:  .byte $CB, $01              ;VEC -3,-1

NCRS1E:
NCRS2S:
;  picture NCRS2 (8 vectors): Player claw (cursor), frame 2
LBFF2:  .byte $91, $01              ;VEC 1,-2
LBFF4:  .byte $17, $01              ;VEC 7,2
LBFF6:  .byte $4B, $01              ;VEC -3,1
LBFF8:  .byte $8A, $01              ;VEC 2,-1
LBFFA:  .byte $CE, $01              ;VEC -6,-1
LBFFC:  .byte $08, $01              ;VEC 0,1
LBFFE:  .byte $0A, $01              ;VEC 2,1
LC000:  .byte $CB, $01              ;VEC -3,-1

NCRS2E:
NCRS3S:
;  picture NCRS3 (8 vectors): Player claw (cursor), frame 3
LC002:  .byte $92, $01              ;VEC 2,-2
LC004:  .byte $16, $01              ;VEC 6,2
LC006:  .byte $4B, $01              ;VEC -3,1
LC008:  .byte $8A, $01              ;VEC 2,-1
LC00A:  .byte $CD, $01              ;VEC -5,-1
LC00C:  .byte $49, $01              ;VEC -1,1
LC00E:  .byte $0A, $01              ;VEC 2,1
LC010:  .byte $CB, $01              ;VEC -3,-1

NCRS3E:
NCRS4S:
;  picture NCRS4 (8 vectors): Player claw (cursor), frame 4
LC012:  .byte $93, $01              ;VEC 3,-2
LC014:  .byte $15, $01              ;VEC 5,2
LC016:  .byte $4B, $01              ;VEC -3,1
LC018:  .byte $8A, $01              ;VEC 2,-1
LC01A:  .byte $CC, $01              ;VEC -4,-1
LC01C:  .byte $4A, $01              ;VEC -2,1
LC01E:  .byte $0A, $01              ;VEC 2,1
LC020:  .byte $CB, $01              ;VEC -3,-1

NCRS4E:
NCRS5S:
;  picture NCRS5 (8 vectors): Player claw (cursor), frame 5
LC022:  .byte $95, $01              ;VEC 5,-2
LC024:  .byte $13, $01              ;VEC 3,2
LC026:  .byte $4B, $01              ;VEC -3,1
LC028:  .byte $8A, $01              ;VEC 2,-1
LC02A:  .byte $CA, $01              ;VEC -2,-1
LC02C:  .byte $4C, $01              ;VEC -4,1
LC02E:  .byte $0A, $01              ;VEC 2,1
LC030:  .byte $CB, $01              ;VEC -3,-1

NCRS5E:
NCRS6S:
;  picture NCRS6 (8 vectors): Player claw (cursor), frame 6
LC032:  .byte $96, $01              ;VEC 6,-2
LC034:  .byte $12, $01              ;VEC 2,2
LC036:  .byte $4B, $01              ;VEC -3,1
LC038:  .byte $8A, $01              ;VEC 2,-1
LC03A:  .byte $C9, $01              ;VEC -1,-1
LC03C:  .byte $4D, $01              ;VEC -5,1
LC03E:  .byte $0A, $01              ;VEC 2,1
LC040:  .byte $CB, $01              ;VEC -3,-1

NCRS6E:
NCRS7S:
;  picture NCRS7 (8 vectors): Player claw (cursor), frame 7
LC042:  .byte $97, $01              ;VEC 7,-2
LC044:  .byte $11, $01              ;VEC 1,2
LC046:  .byte $4B, $01              ;VEC -3,1
LC048:  .byte $8A, $01              ;VEC 2,-1
LC04A:  .byte $88, $01              ;VEC 0,-1
LC04C:  .byte $4E, $01              ;VEC -6,1
LC04E:  .byte $0A, $01              ;VEC 2,1
LC050:  .byte $CB, $01              ;VEC -3,-1

NCRS7E:
NCRS8S:
;  picture NCRS8 (9 vectors): Player claw (cursor), frame 8
LC052:  .byte $0B, $00              ;VEC 3,1,0
LC054:  .byte $A3, $01              ;VEC 3,-4
LC056:  .byte $0A, $01              ;VEC 2,1
LC058:  .byte $10, $01              ;VEC 0,2
LC05A:  .byte $4B, $01              ;VEC -3,1
LC05C:  .byte $8A, $01              ;VEC 2,-1
LC05E:  .byte $90, $01              ;VEC 0,-2
LC060:  .byte $41, $01              ;VEC -1,0
LC062:  .byte $5B, $01              ;VEC -3,3

NCRS8E:
PULS4S:
;  picture PULS4 (6 vectors): Pulsar, frame PULS4 (tallest zigzag)
LC064:  .byte $9A, $01              ;VEC 2,-3
LC066:  .byte $31, $01              ;VEC 1,6
LC068:  .byte $B1, $01              ;VEC 1,-6
LC06A:  .byte $31, $01              ;VEC 1,6
LC06C:  .byte $B1, $01              ;VEC 1,-6
LC06E:  .byte $1A, $01              ;VEC 2,3

PULS4E:
PULS3S:
;  picture PULS3 (7 vectors): Pulsar, frame PULS3
LC070:  .byte $01, $00              ;VEC 1,0,0
LC072:  .byte $91, $01              ;VEC 1,-2
LC074:  .byte $21, $01              ;VEC 1,4
LC076:  .byte $A1, $01              ;VEC 1,-4
LC078:  .byte $21, $01              ;VEC 1,4
LC07A:  .byte $A1, $01              ;VEC 1,-4
LC07C:  .byte $11, $01              ;VEC 1,2

PULS3E:
PULS2S:
;  picture PULS2 (7 vectors): Pulsar, frame PULS2
LC07E:  .byte $01, $00              ;VEC 1,0,0
LC080:  .byte $89, $01              ;VEC 1,-1
LC082:  .byte $11, $01              ;VEC 1,2
LC084:  .byte $91, $01              ;VEC 1,-2
LC086:  .byte $11, $01              ;VEC 1,2
LC088:  .byte $91, $01              ;VEC 1,-2
LC08A:  .byte $09, $01              ;VEC 1,1

PULS2E:
PULS1S:
;  picture PULS1 (4 vectors): Pulsar, frame PULS1
LC08C:  .byte $01, $00              ;VEC 1,0,0
LC08E:  .byte $8A, $01              ;VEC 2,-1
LC090:  .byte $12, $01              ;VEC 2,2
LC092:  .byte $8A, $01              ;VEC 2,-1

PULS1E:
PULS0S:
;  picture PULS0 (2 vectors): Pulsar, frame PULS0 (flat line)
LC094:  .byte $01, $00              ;VEC 1,0,0
LC096:  .byte $06, $01              ;VEC 6,0

;------------------------------------------------------------------------------
; PULS0E - UTILITY: PROJECT POINT ONTO SCREEN
;   INPUT:
;   PXL,PYL,PZL = WORLD COORDINATES OF POINT TO PROJECT
;   EXL,EYL= WORLD COORDINATES OF EYE (EYL HAS AN
;   IMPLIED NEGATIVE SIGN)
;   OUTPUT:SXH,SZH= SCREEN COORDINATES OF PROJECTED POINT
;   MTEMPS DESTROYED
;   FORMULAE: SCREEN X = [FACTOR/(PY-EY)]*(PX-EX)+SXCENT
;   SCREEN Z = [FACTOR/(PY-EY)]*(PZ-EZ)+SZCENT
;   CALCULATE COMMON FACTOR: [FACTOR/(PY-EY)]
;   (also: WORSCR)
;------------------------------------------------------------------------------
PULS0E:
WORSCR:
LC098:  LDA  PYL
LC09A:  SEC
LC09B:  SBC  EYL
LC09D:  STA  MXPL
LC0A0:  LDA  #$00
LC0A2:  SBC  EYH
LC0A4:  STA  MXPH
LC0A7:  BPL  LC0B3                  ;IFMI  IS POINT BEHIND EYE?
LC0A9:  LDA  #$00                   ;YES. PUT IT AT EYE
LC0AB:  STA  MXPH
LC0AE:  LDA  #$01
LC0B0:  STA  MXPL
LC0B3:  LDA  PZL
LC0B5:  CMP  EZL
LC0B7:  BCC  LC0C0                  ;IFCS
LC0B9:  SBC  EZL
LC0BB:  LDX  #$00
LC0BD:  CLV                         ;ELSE
LC0BE:  BVC  LC0C7
LC0C0:  LDA  EZL
LC0C2:  SEC
LC0C3:  SBC  PZL
LC0C5:  LDX  #$FF
LC0C7:  STA  MZLH
LC0CA:  STA  MSZXD
LC0CD:  STX  MTEMP+2
LC0CF:  LDA  PXL
LC0D1:  CMP  EXL
LC0D3:  BCC  LC0DC                  ;IFCS
LC0D5:  SBC  EXL
LC0D7:  LDX  #$00
LC0D9:  CLV                         ;ELSE
LC0DA:  BVC  LC0E3
LC0DC:  LDA  EXL
LC0DE:  SEC
LC0DF:  SBC  PXL
LC0E1:  LDX  #$FF
LC0E3:  STA  MTEMP+1
LC0E5:  STX  MTEMP+3
LC0E7:  BIT  MSTAT                  ;[CS] Check mathbox status
LC0EA:  BMI  LC0E7                  ;PLEND
LC0EC:  LDA  MYLOW
LC0EF:  STA  SZL
LC0F1:  LDA  MYHIGH
LC0F4:  STA  SZH
LC0F6:  LDA  MTEMP+1
LC0F8:  STA  MZLH
LC0FB:  STA  MSZXD
LC0FE:  LDA  MTEMP+2
LC100:  BMI  LC11A                  ;IFPL
LC102:  LDA  SZL
LC104:  CLC
LC105:  ADC  ZADJL
LC107:  STA  SZL
LC109:  LDA  SZL+1
LC10B:  ADC  ZADJL+1
LC10D:  BVC  LC115                  ;IFVS
LC10F:  LDA  #$FF
LC111:  STA  SZL
LC113:  LDA  #$7F
LC115:  STA  SZL+1
LC117:  CLV                         ;ELSE
LC118:  BVC  LC12F
LC11A:  LDA  ZADJL
LC11C:  SEC
LC11D:  SBC  SZL
LC11F:  STA  SZL
LC121:  LDA  ZADJL+1
LC123:  SBC  SZL+1
LC125:  BVC  LC12D                  ;IFVS
LC127:  LDA  #$00
LC129:  STA  SZL
LC12B:  LDA  #$80
LC12D:  STA  SZL+1
LC12F:  BIT  MSTAT                  ;[CS] Check mathbox status
LC132:  BMI  LC12F                  ;PLEND
LC134:  LDA  MYLOW
LC137:  STA  SXL
LC139:  LDA  MYHIGH
LC13C:  STA  SXH
LC13E:  LDX  MTEMP+3
LC140:  BMI  LC158                  ;IFPL
LC142:  LDA  SXL
LC144:  CLC
LC145:  ADC  XADJL
LC147:  STA  SXL
LC149:  LDA  SXL+1
LC14B:  ADC  XADJL+1
LC14D:  BVC  LC155                  ;IFVS
LC14F:  LDA  #$FF
LC151:  STA  SXL
LC153:  LDA  #$7F
LC155:  STA  SXL+1
LC157:  RTS
LC158:  LDA  XADJL
LC15A:  SEC
LC15B:  SBC  SXL
LC15D:  STA  SXL
LC15F:  LDA  XADJL+1
LC161:  SBC  SXL+1
LC163:  BVC  LC16B                  ;IFVS
LC165:  LDA  #$00
LC167:  STA  SXL
LC169:  LDA  #$80
LC16B:  STA  SXL+1
LC16D:  RTS

;------------------------------------------------------------------------------
; INIDSP - INITIALIZE DISPLAY
;------------------------------------------------------------------------------
INIDSP:
LC16E:  JSR  INITEM                 ;COPY SCORE TEMPLATE TO VECTOR RAM
LC171:  LDA  #$80                   ;EYE CENTERED X WISE
LC173:  STA  EXL
LC175:  LDA  #$FF                   ;REG-WELL UPDATE FROM MAINLINE
LC177:  STA  ROTDIS
LC17A:  JSR  INIWLS                 ;INIT. WELL
LC17D:  LDA  SPARE3
LC180:  BNE  LC185                  ;IFEQ  VG HALT AS REQUESTED?
LC182:  STA  VGSTOP                 ;NO. STOP IT  [CS] Reset the vector state machine
LC185:  LDA  #$00                   ;[CS] Clear information counter for
LC187:  STA  SPARE3                 ;[CS] frame update rate. (Usually done when a new screen is presented.)
LC18A:  LDA  JMPMAL+4               ;REQUEST HALT
LC18D:  STA  VECRAM
LC190:  LDA  JMPMAH+4
LC193:  STA  VECRAM+1

;------------------------------------------------------------------------------
; INICOL - RESTORE COLORS  (comment at the call)
;------------------------------------------------------------------------------
INICOL:
LC196:  LDA  CURWAV
LC198:  AND  #$70
LC19A:  CMP  #$5F
LC19C:  BCC  LC1A0                  ;IFCS
LC19E:  LDA  #$5F
LC1A0:  LSR
LC1A1:  ORA  #$07                   ;COLOR TABLE INDEX
LC1A3:  TAX
LC1A4:  LDY  #$07
LC1A6:  LDA  COLTAB,X               ;[CS] Some sort of playing around
LC1A9:  AND  #$0F                   ;[CS] with the color RAM from a table.
LC1AB:  .byte $99, $19, $00         ;STA COLRAM,Y (forced absolute)
LC1AE:  STA  COLPORT,Y
LC1B1:  LDA  COLTAB,X
LC1B4:  LSR
LC1B5:  LSR
LC1B6:  LSR
LC1B7:  LSR
LC1B8:  .byte $99, $21, $00         ;STA COLRAM+8,Y (forced absolute)
LC1BB:  STA  COLPORT+8,Y
LC1BE:  DEX
LC1BF:  DEY
LC1C0:  BPL  LC1A6                  ;MIEND
LC1C2:  RTS

;------------------------------------------------------------------------------
; INIMAT - SET UP MATH BOX  (comment at the call)
;------------------------------------------------------------------------------
INIMAT:
LC1C3:  LDA  #$00                   ;INITIALIZE FOR ONELIN
LC1C5:  STA  X1H
LC1C7:  STA  Z1H
LC1C9:  STA  X0H
LC1CB:  STA  X0L
LC1CD:  STA  Z0H
LC1CF:  STA  Z0L
;[CS] This subroutine clears out the mathbox.
LC1D1:  LDA  #$00                   ;ZERO UNUSED MATH BOX REGISTERS
LC1D3:  STA  MAL
LC1D6:  STA  MAH
LC1D9:  STA  MEL
LC1DC:  STA  MEH
LC1DF:  STA  MFL
LC1E2:  STA  MFH
LC1E5:  STA  MXH
LC1E8:  STA  MBH
LC1EB:  STA  MZLL
LC1EE:  STA  MZLH
LC1F1:  STA  MZHL
LC1F4:  STA  MZHH
LC1F7:  LDA  #$0F
LC1F9:  STA  MNL
LC1FC:  RTS

;.SBTTL COLORS

COLTAB:
;  COLTAB: 8 bytes per wave colour set (INICOL picks by (CURWAV&$70)>>1|7): low nibble -> colour RAM 0-7, high nibble -> 8-15; active-low b0 red-low, b1 red, b2 blue, b3 green
;[CS] Data table loaded in from $C1B1.
LC1FD:  .byte ZWHITE                ;1 EXPLOSIONS (0);PLAYER SHOT CENTER(8)
LC1FE:  .byte ZYELLO                ;CURSOR, FLASHLIGHT(1);SPLAT (A)
LC1FF:  .byte ZPURPL                ;TANKERS(2);SPLAT (B)
LC200:  .byte ZRED                  ;FLIPPERS(3);SPLAT (C)
LC201:  .byte ZTURQOI|[ZRED*$10]    ;PULSARS(4);NYMPHS(0D)
LC202:  .byte ZGREEN                ;LETTERS(S)
LC203:  .byte ZBLUE                 ;WELL(6)
LC204:  .byte ZBLUE                 ;LETTERS(7);FLASH (0F)
LC205:  .byte ZWHITE                ;2
LC206:  .byte ZGREEN
LC207:  .byte ZBLUE
LC208:  .byte ZPURPL
LC209:  .byte ZYELLO|[ZYELLO*$10]
LC20A:  .byte ZTURQOI
LC20B:  .byte ZRED
LC20C:  .byte ZRED
LC20D:  .byte ZWHITE                ;3
LC20E:  .byte ZBLUE
LC20F:  .byte ZTURQOI
LC210:  .byte ZGREEN
LC211:  .byte ZPURPL|[ZRED*$10]
LC212:  .byte ZRED
LC213:  .byte ZYELLO
LC214:  .byte ZYELLO
LC215:  .byte ZWHITE                ;4
LC216:  .byte ZBLUE
LC217:  .byte ZPURPL
LC218:  .byte ZGREEN
LC219:  .byte ZYELLO|[ZRED*$10]
LC21A:  .byte ZRED
LC21B:  .byte ZTURQOI
LC21C:  .byte ZTURQOI
LC21D:  .byte ZWHITE                ;5
LC21E:  .byte ZYELLO
LC21F:  .byte ZPURPL
LC220:  .byte ZRED
LC221:  .byte ZTURQOI|[ZRED*$10]
LC222:  .byte ZGREEN
LC223:  .byte ZBLACK
LC224:  .byte ZBLUE
LC225:  .byte ZWHITE                ;6
LC226:  .byte ZRED
LC227:  .byte ZPURPL
LC228:  .byte ZYELLO
LC229:  .byte ZTURQOI|[ZRED*$10]
LC22A:  .byte ZBLUE
LC22B:  .byte ZGREEN
LC22C:  .byte ZGREEN

SPWECO:
LC22D:  .byte BLUE, RED, YELLOW, TURQOI, WHITE, GREEN, GREEN, GREEN ;SPECIAL WELL COLOR INDEX FOR RATE REQUEST

;------------------------------------------------------------------------------
; INIWLS - INITIALIZE WELL
;   INPUT: Y=INDEX INTO NEW LIX,Z OF LAST GRID LINE'S COORDINATES
;------------------------------------------------------------------------------
INIWLS:
LC235:  LDX  PLAYUP
LC237:  LDA  WAVEN1,X
LC239:  JSR  LVLWEL
LC23C:  PHA                         ;CONVERT CODE TO INDEX
LC23D:  LDY  WELLID
LC240:  LDA  HOLEYL,Y               ;EYE POSITION (Y)
LC243:  EOR  #$FF                   ;CONVERT+TABLE VALUE TO NEG.
LC245:  CLC
LC246:  ADC  #$01
LC248:  STA  EYL
LC24A:  STA  EYLDES
LC24C:  LDA  #$10
LC24E:  SEC
LC24F:  SBC  EYL                    ;DELTA FOR UNIT SCALE
LC251:  STA  YDEUNI
LC253:  LDA  #$FF
LC255:  STA  EYH
LC257:  LDA  HOLEZL,Y               ;EYE POSITION (Z)
LC25A:  STA  EZL
LC25C:  LDA  HOLRAP,Y               ;WELL TYPE (OPEN I CLOSED)
LC25F:  STA  WELTYP
LC262:  LDA  QNXTSTA
LC264:  CMP  #CNWLF2
LC266:  BNE  LC275                  ;IFEQ
LC268:  LDA  HOLZAD,Y               ;AT CENTER IMMEDIATELY (NEW LIFE)
LC26B:  STA  ZADJL
LC26D:  LDA  HOLZDH,Y
LC270:  STA  ZADJL+1
LC272:  CLV                         ;ELSE
LC273:  BVC  LC28D
LC275:  LDA  HOLZAD,Y               ;MOVE UP SLOWLY (NEW WAVE)
LC278:  SEC
LC279:  SBC  ZADJL
LC27B:  STA  ZADEST
LC27E:  LDA  HOLZDH,Y
LC281:  .byte $ED, $69, $00         ;SBC ZADJL+1 (forced absolute)
LC284:  LDX  #$03
LC286:  LSR
LC287:  ROR  ZADEST
LC28A:  DEX
LC28B:  BPL  LC286                  ;MIEND
LC28D:  LDA  #$00                   ;X SCREEN CENTER
LC28F:  STA  XADJL
LC291:  STA  XADJL+1
LC293:  LDA  #$00                   ;SAY TOP & BOTTOM ON SCREEN
LC295:  STA  LEVELY
LC298:  STA  LEVELY+1
LC29B:  LDA  #[VECRAM+$C00]/$100    ;SET UP SUBR BUFR PC
LC29D:  STA  ROTFLG
LC2A0:  PLA
LC2A1:  TAY
LC2A2:  LDX  #NLINES-1
LC2A4:  LDA  NEWLIX,Y
LC2A7:  STA  LINEX,X                ;SET UP X AND Z INTEGER PORTIONS
LC2AA:  LDA  NEWLIZ,Y
LC2AD:  STA  LINEZ,X
LC2B0:  LDA  #$00                   ;ZERO FRACTIONAL PORTION
LC2B2:  STA  LINSXH,X
LC2B5:  STA  LINSZH,X
LC2B8:  STA  LINSTA,X
LC2BB:  LDA  ILINANG,Y              ;LINE ANGLE
LC2BE:  STA  LINANG,X
LC2C1:  DEY
LC2C2:  DEX
LC2C3:  BPL  LC2A4                  ;MIEND
LC2C5:  LDY  #$00                   ;CALCULATE MIDPTS
LC2C7:  LDX  #$0F
LC2C9:  LDA  LINEX,Y
LC2CC:  SEC
LC2CD:  ADC  LINEX,X
LC2D0:  ROR
LC2D1:  STA  LINEXM,X
LC2D4:  LDA  LINEZ,Y
LC2D7:  SEC
LC2D8:  ADC  LINEZ,X
LC2DB:  ROR
LC2DC:  STA  LINEZM,X
LC2DF:  DEY
LC2E0:  BPL  LC2E4                  ;IFMI
LC2E2:  LDY  #$0F
LC2E4:  DEX
LC2E5:  BPL  LC2C9                  ;MIEND
LC2E7:  RTS

;------------------------------------------------------------------------------
; LVLWEL - DETERMINE WELL SEQUENCE INDES
;   INPUT:ACC=LEVEL #-1
;   OUTPUT:ACC=INDEX INTO WELL SEQUENCE TABLES
;   WELLID=WELL ID
;------------------------------------------------------------------------------
LVLWEL:
LC2E8:  LDX  #$00
LC2EA:  CMP  #$62
LC2EC:  BCC  LC2F3                  ;IFCS
LC2EE:  LDA  RANDOM                 ;[CS] Get a random number,
LC2F1:  AND  #$5F                   ;[CS] 1-5F.
LC2F3:  CMP  #<[WELSEN-WELSEQ]
LC2F5:  BCC  LC2FB                  ;IFCS
LC2F7:  INX
LC2F8:  SEC
LC2F9:  SBC  #<[WELSEN-WELSEQ]
LC2FB:  CMP  #<[WELSEN-WELSEQ]
LC2FD:  BCS  LC2F5                  ;CCEND
LC2FF:  TAY
LC300:  LDA  WELSEQ,Y               ;GET WELL CODE FOR THIS WAVE
LC303:  STA  WELLID
LC306:  ASL
LC307:  ASL
LC308:  ASL
LC309:  ASL
LC30A:  ORA  #$0F
LC30C:  RTS

;------------------------------------------------------------------------------
; BLDWEL - UTILITY-BUILD WELL DISPLAY BUFFER
;------------------------------------------------------------------------------
BLDWEL:
LC30D:  LDA  LEVELY+1
LC310:  BNE  WELPIC                 ;IFEQ  BOTTOM OF WELL ON SCREEN LAST TIME?
LC312:  LDA  #ILINDDY               ;YES
LC314:  STA  PYL                    ;BOTTOM OF WELL Y
LC316:  LDX  #$4F                   ;INDEX FOR SCREEN COORDS
LC318:  JSR  CALOUT                 ;CALCULATE SCREEN COORDS FOR BOTTOM OF WELL
LC31B:  STA  LEVELY+1               ;OFF SCREEN FLAG
LC31E:  BEQ  LC323                  ;IFNE  BOTTOM OFF SCREEN?
LC320:  STA  LEVELY                 ;YES. THEN SO IS TOP
LC323:  LDA  LEVELY
LC326:  BNE  WELPIC                 ;IFEQ  TOP OF WELL ON SCREEN LAST TIME?
LC328:  LDA  #ILINLIY               ;YES.
LC32A:  STA  PYL                    ;TOP OF WELL Y
LC32C:  JSR  CHKDEP
LC32F:  LDA  PYL
LC331:  LDX  #$0F                   ;INDEX FOR SCREEN COORDS
LC333:  JSR  CALOUT                 ;CALCULATE SCREEN COORDS FOR TOP OF WELL
LC336:  STA  LEVELY                 ;OFF SCREEN FLAG

;.SBTTL UTILITY-BUILD WELL PIAC

WELPIC:
LC339:  LDA  #$01
LC33B:  JSR  VGSCA1                 ;NORMAL SCAL
LC33E:  LDY  #WELCOL
LC340:  STY  COLOR
LC342:  LDX  LEVELY+1
LC345:  BEQ  LC348                  ;IFNE  OFF SCREEN?
LC347:  RTS                         ;YES. ABORT
LC348:  LDX  ROTFLG                 ;WELL ON?
LC34B:  BNE  LC34E                  ;IFEQ
LC34D:  RTS                         ;NO. NO SPOKES
;ABORT IF ANY OF FAR PTS ARE OFF SCREEN LDX I,NL S-1
LC34E:  LDX  #NLINES-1
LC350:  LDA  #RATS                  ;SPOKE INTENSITY
LC352:  JSR  SPOKE                  ;DRAW SPOKE
LC355:  DEX
LC356:  BPL  LC350                  ;MIEND

;.SBTTL DISPLAY-WELL RIM
LC358:  LDY  #WELCOL
LC35A:  STY  COLOR
LC35C:  LDA  #MZCOLO
LC35E:  JSR  VGSTAT
LC361:  LDY  #$4F
LC363:  LDA  LEVELY+1
LC366:  JSR  OUTLIN
LC369:  LDY  #$0F
LC36B:  LDA  LEVELY

;------------------------------------------------------------------------------
; OUTLIN - DRAW TOP (Y=0) OR BOTTOM (Y=40) OF WELL
;------------------------------------------------------------------------------
OUTLIN:
LC36E:  BNE  LC3B9                  ;IFEQ  ON SCREEN?
LC370:  STY  INDEX1                 ;YES
LC372:  LDA  LINSXL,Y
LC375:  STA  SXL
LC377:  LDA  LINSXH,Y
LC37A:  STA  SXH
LC37C:  LDA  LINSZL,Y
LC37F:  STA  SZL
LC381:  LDA  LINSZH,Y
LC384:  STA  SZH
LC386:  LDX  #SXL
LC388:  JSR  VGYABS                 ;UPDATE CURNTX,Y
LC38B:  LDA  VGLIST                 ;SAVE FOR RUNG CHANGES
LC38D:  STA  RUNGVG
LC38F:  LDA  VGLIST+1
LC391:  STA  RUNGVG+1
LC393:  LDX  #NLINES-1
LC395:  LDA  WELTYP
LC398:  BEQ  LC39B                  ;IFNE  PLANAR
LC39A:  DEX                         ;YES. BEAM OFF FOR 1ST LINE
LC39B:  LDA  #RATS
LC39D:  STA  VGBRIT                 ;TURN ON BEAM
LC39F:  STX  INDEX2
LC3A1:  DEC  INDEX1
LC3A3:  LDA  INDEX1
LC3A5:  AND  #$0F
LC3A7:  CMP  #$0F
LC3A9:  BNE  LC3B2                  ;IFEQ  INDEX WRAPPING?
LC3AB:  LDA  INDEX1
LC3AD:  CLC                         ;YES
LC3AE:  ADC  #$10
LC3B0:  STA  INDEX1
LC3B2:  JSR  LINTOS                 ;MOVE LINS TO SXL...SZH
LC3B5:  DEC  INDEX2
LC3B7:  BPL  LC3A1                  ;MIEND
LC3B9:  RTS

;------------------------------------------------------------------------------
; CONNEC - UTILITY-CONNECT CURRENT PT. WITH NEXT POINT
;   DRAW A LINE TO NEXT POINT (SX)
;------------------------------------------------------------------------------
CONNEC:
LC3BA:  LDA  SXL                    ;CURRENT POINT(CURNTX)AND
LC3BC:  SEC                         ;SET CURRENT POINT=NEXT POIN
LC3BD:  SBC  CURNTX
LC3BF:  STA  XCOMP
LC3C1:  LDA  SXH
LC3C3:  SBC  CURNTX+1
LC3C5:  STA  XCOMP+1                ;X PORTION OF VECTOR
LC3C7:  LDA  SZL
LC3C9:  SEC
LC3CA:  SBC  CURNTY
LC3CC:  STA  YCOMP
LC3CE:  LDA  SZH
LC3D0:  SBC  CURNTY+1
LC3D2:  STA  YCOMP+1                ;Z PORTION OF VECTOR
LC3D4:  LDX  #XCOMP

UPCURN:
LC3D6:  JSR  VGVCTR                 ;DRAW VECTOR
LC3D9:  LDA  SXL                    ;SET CURRENT PT=NEXT PT
LC3DB:  STA  CURNTX
LC3DD:  LDA  SXH
LC3DF:  STA  CURNTX+1
LC3E1:  LDA  SZL
LC3E3:  STA  CURNTY
LC3E5:  LDA  SZH
LC3E7:  STA  CURNTY+1
;MAKE SURE BEAM IS ON
LC3E9:  LDA  #RATS
LC3EB:  STA  VGBRIT
LC3ED:  RTS

;------------------------------------------------------------------------------
; SPOKE - DISPLAY-DRAW 2 SPOKES
;   INPUT: X=LINE # TO ILLUMINATE
;   ACC=INTENSITY
;   OUTPUT:X PRESERVED
;------------------------------------------------------------------------------
SPOKE:
LC3EE:  STX  INDEX1
LC3F0:  PHA
LC3F1:  LDY  COLOR
LC3F3:  LDA  #MZCOLO
LC3F5:  JSR  VGSTAT
;CENTER BEAM
LC3F8:  JSR  LIFTOS                 ;FAR PT SCREEN COORD
LC3FB:  LDX  #SXL                   ;DRAW BLANK VEC TO FAR PT.
LC3FD:  JSR  VGYABS                 ;CURRENT PT.=FAR PT.
LC400:  PLA
LC401:  STA  VGBRIT
LC403:  PHA
;NEAR PT COORD
LC404:  JSR  LINTOS                 ;DRAW FROM FAR PT TO NEAR PT.
LC407:  DEC  INDEX1
LC409:  LDY  COLOR
LC40B:  LDA  #$00
LC40D:  STA  VGBRIT
LC40F:  LDA  #MZCOLO
LC411:  JSR  VGSTAT
LC414:  JSR  LINTOS                 ;DRAW FROM NEAR PT. TO ADJ NEAR PT.
LC417:  PLA
LC418:  STA  VGBRIT
LC41A:  JSR  LIFTOS
LC41D:  JSR  CONNEC                 ;DRAW TO FAR PT.
LC420:  LDX  INDEX1
LC422:  RTS

;------------------------------------------------------------------------------
; LINTOS - MOVE LINS TO SXL...SZH  (comment at the call)
;   DRAW FROM FAR PT TO NEAR PT.  (another call)
;   DRAW FROM NEAR PT. TO ADJ NEAR PT.  (another call)
;------------------------------------------------------------------------------
LINTOS:
LC423:  LDX  INDEX1
LC425:  LDA  LINSXL,X
LC428:  STA  SXL
LC42A:  LDA  LINSXH,X
LC42D:  STA  SXH
LC42F:  LDA  LINSZL,X
LC432:  STA  SZL
LC434:  LDA  LINSZH,X
LC437:  STA  SZH
LC439:  JMP  CONNEC                 ;DRAW LINE

;------------------------------------------------------------------------------
; LIFTOS - FAR PT SCREEN COORD  (comment at the call)
;------------------------------------------------------------------------------
LIFTOS:
LC43C:  LDX  INDEX1
LC43E:  LDA  LIFSXL,X
LC441:  STA  SXL
LC443:  LDA  LIFSXH,X
LC446:  STA  SXH
LC448:  LDA  LIFSZL,X
LC44B:  STA  SZL
LC44D:  LDA  LIFSZH,X
LC450:  STA  SZH
LC452:  RTS

;------------------------------------------------------------------------------
; CHKDEP - CHECK FOR EYE PAST OBJECT ON WELL
;------------------------------------------------------------------------------
CHKDEP:
LC453:  LDA  EYH
LC455:  BNE  LC471                  ;IFEQ  EYE + ?
LC457:  LDA  PYL                    ;YES.
LC459:  SEC
LC45A:  SBC  EYL
LC45C:  BCC  LC460                  ;IFCS
LC45E:  CMP  #$0C
LC460:  BCS  LC471                  ;IFCC  EYE TO CLOSE?
LC462:  LDA  EYL                    ;YES. NUDGE PT. AWAY
LC464:  CLC
LC465:  ADC  #$0F
LC467:  BCS  LC46B                  ;IFCC
LC469:  CMP  #$F0
LC46B:  BCC  LC46F                  ;IFCS
LC46D:  LDA  #$F0                   ;BUT NOT PAST END OF WELL
LC46F:  STA  PYL
LC471:  RTS

CHKSM8:
LC472:  .byte QCHKS8

;------------------------------------------------------------------------------
; CALOUT - UTILITY-PROJECT OUTLINE
;   INPUT:ACC=Y COORDINATE FOR OUTLINE
;   X=0F OR 4F FOR NEAR OR FAR ARRAY
;   LINEX,Z(10)=OUTLINE'S X AND Z COORDINATES
;   OUTPUT:ACC:0 IF OUTLINE IS ONSCREEN
;   :NOT 0 IF ANY PT. IS OFF SCREEN
;------------------------------------------------------------------------------
CALOUT:
LC473:  STA  PYL                    ;SAVE Y FOR OUTLINE
LC475:  STX  INDEX2                 ;SAVE INDEX OF DEST IN ARRAY
LC477:  LDA  #$00
LC479:  STA  LINSCA                 ;START OFF SCREEN FLAG AT ON SCREEN
LC47B:  LDX  #$0F
LC47D:  STX  INDEX1
LC47F:  LDX  INDEX1
LC481:  LDA  LINEX,X
LC484:  STA  PXL
LC486:  LDA  LINEZ,X
LC489:  STA  PZL
LC48B:  JSR  WORSCR                 ;PROJECT PT.
LC48E:  LDX  INDEX2
LC490:  LDY  SXL
LC492:  LDA  SXH
LC494:  BMI  LC4A3                  ;IFPL  X OFF SCREEN?
LC496:  CMP  #$04
LC498:  BCC  LC4A0                  ;IFCS
LC49A:  LDY  #$FF
LC49C:  LDA  #$03
LC49E:  INC  LINSCA                 ;YES
LC4A0:  CLV                         ;ELSE
LC4A1:  BVC  LC4AD
LC4A3:  CMP  #$FC
LC4A5:  BCS  LC4AD                  ;IFCC
LC4A7:  LDY  #$01
LC4A9:  LDA  #$FC
LC4AB:  INC  LINSCA                 ;YES. SET OFF SCREEN FLAG
LC4AD:  STA  LINSXH,X
LC4B0:  TYA
LC4B1:  STA  LINSXL,X
LC4B4:  LDY  SZL
LC4B6:  LDA  SZH
LC4B8:  BMI  LC4C7                  ;IFPL  Z OFF SCREEN?
LC4BA:  CMP  #$04
LC4BC:  BCC  LC4C4                  ;IFCS
LC4BE:  LDY  #$FF
LC4C0:  LDA  #$03
LC4C2:  INC  LINSCA                 ;YES.
LC4C4:  CLV                         ;ELSE
LC4C5:  BVC  LC4D1
LC4C7:  CMP  #$FC
LC4C9:  BCS  LC4D1                  ;IFCC
LC4CB:  LDA  #$FC
LC4CD:  LDY  #$01
LC4CF:  INC  LINSCA                 ;YES
LC4D1:  STA  LINSZH,X
LC4D4:  TYA
LC4D5:  STA  LINSZL,X
LC4D8:  DEC  INDEX2
LC4DA:  DEC  INDEX1
LC4DC:  BPL  LC47F                  ;MIEND
LC4DE:  LDA  LINSCA
LC4E0:  RTS

;------------------------------------------------------------------------------
; DSPHOL - UTILITY-DRAW WELL SHAPE
;   INPUT:ACC=LEVEL #-1
;------------------------------------------------------------------------------
DSPHOL:
LC4E1:  JSR  LVLWEL                 ;SET UP WELL INDEX & ID
LC4E4:  STA  SAVEY                  ;WELL INDEX
LC4E6:  STX  SAVEX                  ;CYCLE
LC4E8:  LDA  #$00
LC4EA:  STA  VGBRIT
LC4EC:  LDA  #$05                   ;MAKE WELL REALLY SMALL
LC4EE:  JSR  VGSCA1
LC4F1:  LDA  SAVEX                  ;GET CYCLE (TIMES THRU ALL WELLS
LC4F3:  AND  #$07
LC4F5:  TAX
LC4F6:  LDY  SPWECO,X               ;GET SPECIAL WELL COLOR FOR CYCLE
LC4F9:  STY  COLOR
LC4FB:  LDA  #MZCOLO
LC4FD:  JSR  VGSTAT                 ;SET WELL COLOR
LC500:  LDX  WELLID
LC503:  LDA  SAVEY
LC505:  LDY  HOLRAP,X
LC508:  BNE  LC50D                  ;IFEQ  PLANAR?
LC50A:  SEC                         ;NO. START BEAM AT FIRST POINT
LC50B:  SBC  #$0F                   ;IN TABLE (FOR CLOSED WELLS)
LC50D:  TAY
LC50E:  LDA  NEWLIZ,Y
LC511:  STA  PYL
LC513:  EOR  #$80                   ;ADJUST Z SIGN
LC515:  TAX
LC516:  LDA  NEWLIX,Y               ;SAVE COORDS OF 1ST PT
LC519:  STA  PXL
LC51B:  EOR  #$80                   ;ADJUST X SIGN
LC51D:  JSR  VGVTR1                 ;POSITION BEAM AT 1ST PT ON WELL
LC520:  LDA  #$C0                   ;TURN BEAM ON
LC522:  STA  VGBRIT
LC524:  LDX  #NLINES-1
LC526:  STX  INDEX2
LC528:  LDY  SAVEY
LC52A:  LDA  NEWLIX,Y
LC52D:  TAX
LC52E:  SEC
LC52F:  SBC  PXL                    ;DELTA X
LC531:  PHA
LC532:  STX  PXL                    ;CURRENT X OLD X
LC534:  LDA  NEWLIZ,Y
LC537:  TAY
LC538:  SEC
LC539:  SBC  PYL                    ;DELTA Z
LC53B:  TAX
LC53C:  STY  PYL                    ;CURRENT Z>OLD Z
LC53E:  PLA
LC53F:  JSR  VGVTR1                 ;DRAW VECTOR TO NEXT PT.
LC542:  DEC  SAVEY
LC544:  DEC  INDEX2
LC546:  BPL  LC528                  ;MIEND
LC548:  LDA  #$01                   ;NORMAL SIZE AGAIN
LC54A:  JMP  VGSCA1

;------------------------------------------------------------------------------
; DSTARF - DISPLAY STAR FIELD
;------------------------------------------------------------------------------
DSTARF:
LC54D:  LDA  PLAGRO
LC550:  BEQ  ZQPONS                 ;IFNE
LC552:  LDA  EYL                    ;SAVE EYE POSITION
LC554:  PHA
LC555:  LDA  EYH
LC557:  PHA
LC558:  LDA  YDEUNI
LC55A:  PHA
LC55B:  LDA  #$E8
LC55D:  STA  EYL
LC55F:  LDA  #$FF
LC561:  STA  EYH
LC563:  LDA  #$28
LC565:  STA  YDEUNI

;.SBTTL DISPLAY-PLANES OF STARS
LC567:  LDX  #NPLANE-1
LC569:  STX  INDEX1
LC56B:  LDX  INDEX1
LC56D:  LDA  PLANEY,X
LC570:  BEQ  LC5A4                  ;IFNE  ACTIVE PLANE?
LC572:  STA  PYL                    ;YES
LC574:  LDA  #$80                   ;CENTER OF WORLD
LC576:  STA  PXL
LC578:  LDA  #$80
LC57A:  STA  PZL
LC57C:  LDA  CURWAV
LC57E:  CMP  #$05
LC580:  BCS  LC587                  ;IFCC
LC582:  LDA  #BLUE                  ;BLUE STARS IN WAVES 1-4
LC584:  CLV                         ;ELSE
LC585:  BVC  LC590
LC587:  TXA
LC588:  AND  #$07
LC58A:  CMP  #$07
LC58C:  BNE  LC590                  ;IFEQ
LC58E:  LDA  #$04
LC590:  STA  COLOR
LC592:  TAY
LC593:  LDA  #MZCOLO
LC595:  JSR  VGSTAT
LC598:  LDA  INDEX1
LC59A:  AND  #$03                   ;DETERMINE PICTURE SUBROUTINE CODE
LC59C:  ASL
LC59D:  ADC  #PTSTR1
LC59F:  STA  OBJIND
LC5A1:  JSR  SCAPI2                 ;DRAW PLANE OF STARS ACC TO SCALE
LC5A4:  DEC  INDEX1
LC5A6:  BPL  LC56B                  ;MIEND
LC5A8:  PLA
LC5A9:  STA  YDEUNI
LC5AB:  PLA                         ;RESTORE EYE POSITION
LC5AC:  STA  EYH
LC5AE:  PLA
LC5AF:  STA  EYL                    ;[CS] COPY PROTECTION CODE FOR POKEY Causes lockups.

ZQPONS:
LC5B1:  LDA  QT5                    ;[CS] Check pokey protection result.
LC5B4:  BEQ  LC5C1                  ;IFNE  [CS] If zero (good), then return.
LC5B6:  LDX  LSCORH                 ;[CS] Compare player one's score
LC5B8:  CPX  #$15                   ;[CS] to see if it is >= 16,000.
LC5BA:  BCC  LC5C1                  ;IFCS  [CS] If not, then return.
LC5BC:  LDX  LSCORL                 ;[CS] Load in the lower two digits current player's score.
LC5BE:  INC  $0200,X                ;[CS] Add that score to $200, and increment the value at that address in memory.
LC5C1:  RTS

;------------------------------------------------------------------------------
; DSPENL - DISPLAY - ENEMY LINES
;------------------------------------------------------------------------------
DSPENL:
LC5C2:  LDA  LEVELY+1               ;BOTTOM OF WELL
LC5C5:  BEQ  LC5C8                  ;IFNE  WELL ON?
LC5C7:  RTS                         ;NO. NO ENEMY LINES THEN
LC5C8:  LDA  EYH
LC5CA:  BNE  LC5D3                  ;IFEQ  EYE ON WELL?
LC5CC:  LDA  EYL                    ;YES.
LC5CE:  CMP  #$F0
LC5D0:  BCC  LC5D3                  ;IFCS  PAST END?
LC5D2:  RTS                         ;YES. ABORT
LC5D3:  LDA  #$01
LC5D5:  JSR  VGSCA1
LC5D8:  LDA  VGLIST                 ;SAVE FOR NEXT TIME
LC5DA:  PHA
LC5DB:  LDA  VGLIST+1
LC5DD:  PHA
LC5DE:  LDA  #$00                   ;LINE LOOP INDEX
LC5E0:  STA  INDEX2
LC5E2:  STA  VGY
LC5E4:  LDX  #NLINES-1
LC5E6:  LDA  WELTYP
LC5E9:  BEQ  LC5EC                  ;IFNE  PLANAR?
LC5EB:  DEX                         ;YES. 1 LESS LINE
LC5EC:  STX  INDEX1
LC5EE:  LDX  #$03
LC5F0:  LDY  VGY
LC5F2:  LDA  ENLFIX,X               ;(CSTATGREEN,CNTR)
LC5F5:  STA  (VGLIST),Y
LC5F7:  INY
LC5F8:  DEX
LC5F9:  BPL  LC5F2                  ;MIEND
LC5FB:  STY  VGY
LC5FD:  LDA  ROTDIS
LC600:  BNE  LC64C                  ;IFEQ  REDO WELL?
LC602:  LDX  INDEX2                 ;NO
LC604:  LDA  LINSTA,X
LC607:  BMI  LC61A                  ;IFPL  ACTION AT NEAR PT?
LC609:  LDX  #$0B
LC60B:  LDY  VGY
LC60D:  LDA  (OLDLLO),Y             ;COPY VECTOR TO FAR POINT AND
LC60F:  STA  (VGLIST),Y             ;VECTOR TO NEAR POINT
LC611:  INY
LC612:  DEX
LC613:  BPL  LC60D                  ;MIEND
LC615:  STY  VGY
LC617:  CLV                         ;ELSE
LC618:  BVC  LC649
LC61A:  LDY  VGY                    ;NO. SINCE FAR PT. NEED NOT BE
LC61C:  LDA  (OLDLLO),Y             ;RECALCULATED, COPY IT TO NEW BUFFER.
LC61E:  STA  (VGLIST),Y
LC620:  STA  CURNTY                 ;Z VECTOR (LSB)
LC622:  INY
LC623:  LDA  (OLDLLO),Y
LC625:  STA  (VGLIST),Y
LC627:  CMP  #$10
LC629:  BCC  LC62D                  ;IFCS
LC62B:  ORA  #$E0                   ;SIGN EXTEND
LC62D:  STA  CURNTY+1               ;Z VECTOR (MSB)
LC62F:  INY
LC630:  LDA  (OLDLLO),Y
LC632:  STA  (VGLIST),Y             ;X VECTOR (LSB)
LC634:  STA  CURNTX
LC636:  INY
LC637:  LDA  (OLDLLO),Y
LC639:  STA  (VGLIST),Y
LC63B:  CMP  #$10
LC63D:  BCC  LC641                  ;IFCS
LC63F:  ORA  #$E0                   ;SIGN EXTEND
LC641:  STA  CURNTX+1               ;X VECTOR (MSB)
LC643:  INY
LC644:  STY  VGY
LC646:  JSR  TIPACT                 ;YES. GENERATE TIP STUFF
LC649:  CLV                         ;ELSE
LC64A:  BVC  LC652
;YES (REDO WELL)
LC64C:  JSR  FIXSTU                 ;GENERATE FIXED STUFF
LC64F:  JSR  TIPACT                 ;GENERATE TIP STUFF
LC652:  LDX  INDEX2
LC654:  ASL  LINSTA,X               ;CLEAR LINE STATUS
LC657:  INC  INDEX2
LC659:  DEC  INDEX1
LC65B:  BPL  LC5EE                  ;MIEND
LC65D:  PLA                         ;SAVE LOC OF NEW BUFFER
LC65E:  STA  OLDLHI
LC660:  PLA
LC661:  STA  OLDLLO
LC663:  LDY  VGY
LC665:  DEY
LC666:  JMP  VGADD                  ;UPDATE VGLIST

;.SBTTL DISPLAY - ENEMY LINES (INITIAL FIXED VG CODES)

ENLFIX:
;PLACES COLOR STAT
;CNTR
;VCTR TO FAR PT.
;INTO VGLIST(VGY)
LC669:  .byte $80, $40, $68, $05

;------------------------------------------------------------------------------
; FIXSTU - CALCULATE SCREEN LOCATION OF
;   FAR POINT
;------------------------------------------------------------------------------
FIXSTU:
LC66D:  LDA  INDEX2
LC66F:  TAX
LC670:  CLC                         ;AVERAGING SCREEN COORDNATE
LC671:  ADC  #$01                   ;OF ADJACENT LINES
LC673:  AND  #$0F
LC675:  TAY
LC676:  LDA  LIFSXL,X
LC679:  SEC                         ;ROUND
LC67A:  ADC  LIFSXL,Y
LC67D:  STA  SXL
LC67F:  LDA  LIFSXH,X
LC682:  ADC  LIFSXH,Y
LC685:  STA  SXH
LC687:  ASL
LC688:  ROR  SXH
LC68A:  ROR  SXL
LC68C:  LDA  LIFSZL,X
LC68F:  SEC                         ;ROUND
LC690:  ADC  LIFSZL,Y
LC693:  STA  SZL
LC695:  LDA  LIFSZH,X
LC698:  ADC  LIFSZH,Y
LC69B:  STA  SZH
LC69D:  ASL
LC69E:  ROR  SZH
LC6A0:  ROR  SZL

;.SBTTL UTILITY - QUICK BLANK VECTOR FROM SX,SZ

YVGVCT:
;FALL INTO YVGVCT
;UPDATES CURNTX(2) AND CURNTY(2) WITH SXL(2) AND SZL(2).
;UPDATES VGY
LC6A2:  LDY  VGY
LC6A4:  LDA  SZL
LC6A6:  STA  (VGLIST),Y
LC6A8:  INY
LC6A9:  STA  CURNTY
LC6AB:  LDA  SZH
LC6AD:  STA  CURNTY+1
LC6AF:  AND  #$1F
LC6B1:  STA  (VGLIST),Y
LC6B3:  INY
LC6B4:  LDA  SXL
LC6B6:  STA  (VGLIST),Y
LC6B8:  INY
LC6B9:  STA  CURNTX
LC6BB:  LDA  SXH
LC6BD:  STA  CURNTX+1
LC6BF:  AND  #$1F
LC6C1:  STA  (VGLIST),Y
LC6C3:  INY
LC6C4:  STY  VGY
LC6C6:  RTS

;------------------------------------------------------------------------------
; TIPACT - DISPLAY - ENEMY LINES (TIP STUFF)
;   OR IF INACTIVE, 4 SCAL 1,05
;   PLACES VECTOR TO NEAR PT AND
;   (DOT STAT COLOR, JSRL DOT) OR
;   (SHATTER SCAL, SHATTER JSRL PIC)
;   INTO VGLIST (VGY)
;------------------------------------------------------------------------------
TIPACT:
LC6C7:  LDX  INDEX2
LC6C9:  LDA  LINEY,X
LC6CC:  BNE  LC6E4                  ;IFEQ  LINE ACTIVE?
LC6CE:  LDY  VGY                    ;NO. FILL WITH SCAL 1,0
LC6D0:  LDX  #$03
LC6D2:  LDA  #$00                   ;SCAL 1,0 LSB (NOOP)
LC6D4:  STA  (VGLIST),Y
LC6D6:  INY
LC6D7:  LDA  #$71                   ;SCAL 1,0 MSB (NOOP)
LC6D9:  STA  (VGLIST),Y
LC6DB:  INY
LC6DC:  DEX
LC6DD:  BPL  LC6D2                  ;MIEND
LC6DF:  STY  VGY
LC6E1:  CLV                         ;ELSE
LC6E2:  BVC  LC73B
LC6E4:  STA  PYL                    ;LINE IS ACTIVE
;CALCULATE NEAR PT.
LC6E6:  JSR  CHKDEP                 ;YES, CHECK EYE
LC6E9:  LDA  LINEXM,X               ;X COORD OF MIDWAY PT.
LC6EC:  STA  PXL
LC6EE:  LDA  LINEZM,X               ;Z COORD OF MIDWAY PT.
LC6F1:  STA  PZL
LC6F3:  JSR  WORSCR                 ;PROJECT ENEMY LIVE NEAR PT.
;SAVE NEW COORDINATES
LC6F6:  JSR  FCONNEC                ;DRAW VECTOR TO NEAR PT.
LC6F9:  LDX  INDEX2
LC6FB:  LDA  LINSTA,X
LC6FE:  AND  #$40
LC700:  BEQ  WHITIP                 ;IFNE  WHAT'S HAPPENING AT TIP?
LC702:  JSR  CASCAL                 ;SHATTERED
;SET PROJECTION SCALE
LC705:  LDA  RANDOM
LC708:  AND  #$02
LC70A:  CLC
LC70B:  ADC  #PTSPAR
LC70D:  TAX                         ;DETERMINE SHATTER PIC
LC70E:  LDA  PICHI,X
LC711:  INY                         ;INSERT JSRL TO SHATTER PIC
LC712:  STA  (VGLIST),Y
LC714:  DEY
LC715:  LDA  PICLO,X
LC718:  STA  (VGLIST),Y
LC71A:  INY
LC71B:  INY
LC71C:  STY  VGY
LC71E:  CLV                         ;ELSE
LC71F:  BVC  LC73B

WHITIP:
LC721:  LDY  VGY                    ;JUST A DOT AT TIP
LC723:  LDA  #WHITE                 ;COLOR (SET STAT WHITE)
LC725:  STA  (VGLIST),Y
LC727:  INY
LC728:  LDA  #$68
LC72A:  STA  (VGLIST),Y
LC72C:  INY
LC72D:  LDA  JSRDOT                 ;INSERT JSRL TO DOT
LC730:  STA  (VGLIST),Y
LC732:  INY
LC733:  LDA  JSRDOT+1
LC736:  STA  (VGLIST),Y
LC738:  INY
LC739:  STY  VGY
LC73B:  RTS

;------------------------------------------------------------------------------
; FCONNEC - DISPLAY UTILITY - FAST CONNECT
;   DRAW VECTOR OF INTENSITY 0A0
;   FROM CURNTX, Y, TO SX, SZ
;------------------------------------------------------------------------------
FCONNEC:
LC73C:  LDY  VGY
LC73E:  LDA  SZL
LC740:  SEC
LC741:  SBC  CURNTY
LC743:  STA  (VGLIST),Y
LC745:  INY
LC746:  LDA  SZH
LC748:  SBC  CURNTY+1
LC74A:  AND  #$1F
LC74C:  STA  (VGLIST),Y
LC74E:  INY
LC74F:  LDA  SXL
LC751:  SEC
LC752:  SBC  CURNTX
LC754:  STA  (VGLIST),Y
LC756:  INY
LC757:  LDA  SXH
LC759:  SBC  CURNTX+1
LC75B:  AND  #$1F
LC75D:  ORA  #$A0
LC75F:  STA  (VGLIST),Y
LC761:  INY
LC762:  STY  VGY
LC764:  RTS

;------------------------------------------------------------------------------
; VGYAB1 - UTILITY - VG ABS POS
;------------------------------------------------------------------------------
VGYAB1:
LC765:  LDY  #$00
LC767:  TYA
LC768:  STA  (VGLIST),Y
LC76A:  LDA  #$71
LC76C:  INY
LC76D:  STA  (VGLIST),Y             ;SCALE BINARY=1, LINEAR=0
LC76F:  INY
LC770:  BNE  NOLABS

;------------------------------------------------------------------------------
; VGYABS - UPDATE CURNTX,Y  (comment at the call)
;   CURRENT PT.=FAR PT.  (another call)
;------------------------------------------------------------------------------
VGYABS:
LC772:  LDY  #$00

NOLABS:
LC774:  LDA  #$40                   ;INPUT: X=BASE PAGE LOC OF SCREEN COORDINATE PAIR
LC776:  STA  (VGLIST),Y             ;VG CENTER
LC778:  LDA  #$80
LC77A:  INY
LC77B:  STA  (VGLIST),Y
LC77D:  INY
LC77E:  LDA  $02,X
LC780:  STA  CURNTY
LC782:  STA  (VGLIST),Y             ;VCTR DELTA Z
LC784:  INY
LC785:  LDA  $03,X
LC787:  STA  CURNTY+1
LC789:  AND  #$1F
LC78B:  STA  (VGLIST),Y
LC78D:  LDA  $00,X
LC78F:  STA  CURNTX
LC791:  INY
LC792:  STA  (VGLIST),Y             ;DELTA X
LC794:  LDA  $01,X
LC796:  STA  CURNTX+1
LC798:  AND  #$1F
LC79A:  INY
LC79B:  STA  (VGLIST),Y
LC79D:  JMP  VGADD                  ;OUTPUT: BEAM AT ABS. POS.

;==============================================================================
; MODULE ALEXEC   ALEXEC.MAC
;   Executive: MAINLN frame loop, state dispatch (ROUTAD), start/end of game,
;   attract mode, colour and pause handling.
;==============================================================================

;------------------------------------------------------------------------------
; MAINLN - MAINLOOP
;   ENTRY POINTS DEFINED HERE
;   ENTRY POINTS DEFINED IN OTHER MODULES
;   INPUT: POWER ON RESET PREPARATION
;   OUTPUT: NONE
;   [CS] Code jumps here after initialization
;------------------------------------------------------------------------------
MAINLN:
LC7A0:  JSR  INISOU                 ;INITIALIZE SOUNDS  [CS] Pokey Initialization Routine
LC7A3:  LDA  #CNEWGA                ;[CS] Zero out the game status
LC7A5:  STA  QSTATE                 ;[CS] indicator.
LC7A7:  LDA  FRTIMR                 ;[CS] Is the 0-9 timer at "9" or
LC7A9:  CMP  #$09                   ;[CS] greater?
LC7AB:  BCC  LC7A7                  ;CSEND  [CS] If not, loop.
LC7AD:  LDA  #$00                   ;RESTART FRAME TIMER  [CS] If so...
LC7AF:  STA  FRTIMR                 ;[CS] zero out the timer.
LC7B1:  JSR  EXSTAT                 ;EXECUTE APPROPRIATE GAME STATE
LC7B4:  JSR  NONSTA                 ;EXECUTE NON-STATE DEPENDENT CODE
LC7B7:  JSR  DISPLAY                ;EXECUTE CODE TO DISPLAY NEW SCREEN
LC7BA:  CLC
LC7BB:  BCC  LC7A7                  ;CSEND  LOOP ALWAYS

;------------------------------------------------------------------------------
; EXSTAT - STATE ROUTINE EXECUTOR
;   INPUT: QSTATE: CODE FOR STATE ROUTINE TO EXECUTE
;   OUTPUT: CONTROL PASSED TO ROUTINE
;------------------------------------------------------------------------------
EXSTAT:
LC7BD:  LDA  INOP0                  ;[CS] Are we in demonstration freeze
LC7C0:  AND  #$83                   ;[CS] mode?
LC7C2:  CMP  #$82
LC7C4:  BEQ  NOOPR_C7D9             ;IFNE  FREEZE & FREE PLAY?  [CS] If so, skip this code and return.
LC7C6:  JSR  PRSTAR                 ;PROCESS STAR FIELD
LC7C9:  LDX  QSTATE
LC7CB:  LDA  SWFINA                 ;SET MUST PROCESS FLAG
LC7CD:  ORA  #MFAKE
LC7CF:  STA  SWFINA
LC7D1:  LDA  ROUTAD+1,X
LC7D4:  PHA
LC7D5:  LDA  ROUTAD,X
LC7D8:  PHA

NOOPR_C7D9:
LC7D9:  RTS

ROUTAD:
;STATE ROUTINE ADDRESS
;[CS] Data segment, per Ken Lui.
LC7DA:  .word NEWGAM-1              ;NEW GAME
LC7DC:  .word NEWLIF-1              ;NEW LIFE (AFTER LOSING A BASE)
LC7DE:  .word PLAY-1                ;PLAY
LC7E0:  .word ENDLIF-1              ;LIFE LOST
LC7E2:  .word ENDGAM-1              ;END OF GAME
LC7E4:  .word PAUSE-1               ;PAUSE
LC7E6:  .word $0000                 ;NEW WAVE (AFTER SHOOTING ALL INVADERS)
LC7E8:  .word ENDWAV-1              ;END OF WAVE
LC7EA:  .word HISCHK-1              ;CHECK FOR HI SCORES
LC7EC:  .word GETINI-1              ;GET HI SCORE INITIALS
LC7EE:  .word DLADR-1               ;DISPLAY HI SCORE TABLE
LC7F0:  .word PRORAT-1              ;REQUEST PLAYER RATE
LC7F2:  .word NEWAV2-1              ;NEW WAVE PART 2
LC7F4:  .word LOGINI-1              ;LOGO INIT
LC7F6:  .word INIRAT-1              ;MONSTER DELAY/DISPLAY
LC7F8:  .word NEWLF2-1              ;NEW LIFE PART 2
LC7FA:  .word PLDROP-1              ;DROP MODE
LC7FC:  .word SYSTEM-1              ;END WAVE CLEAN UP AFTER BONUS
LC7FE:  .word PRBOOM-1              ;BOOM

;------------------------------------------------------------------------------
; ROUTEN - PAUSE STATE
;   INPUT: QNXTSTA: CODE FOR STATE ROUTINE TO EXECUTE AFTER PAUSE
;   QTMPAUS: PAUSE TIMER (# OF X SECOND UNITS TO WAIT)
;   QFRAME: FRAME COUNTER
;   OUTPUT: QTMPAUS,QSTATE UPDATED
;   [CS] Start of ROM 136002.120 at $C800.
;   (also: PAUSE)
;------------------------------------------------------------------------------
ROUTEN:
PAUSE:
LC800:  LDA  QFRAME                 ;[CS] Re-disassemble. Confused by data segment.
LC802:  AND  PSCALE
LC805:  BNE  LC818                  ;IFEQ
LC807:  LDA  QTMPAUS                ;YES
LC809:  BEQ  LC80D                  ;IFNE  AT 0? (STOP AT 0)
LC80B:  DEC  QTMPAUS                ;NO. DROP 1
LC80D:  BNE  LC818                  ;IFEQ  AT 0?
LC80F:  LDA  QNXTSTA                ;YES. GO TO NEXT STATE
LC811:  STA  QSTATE
LC813:  LDA  #$00                   ;RESET STANDARD TIMER SCALE
LC815:  STA  PSCALE
LC818:  JMP  MOVCUR                 ;UPDATE CURSOR (IF ALIVE)

;------------------------------------------------------------------------------
; PROCRE - PROCESS CREDITS
;------------------------------------------------------------------------------
PROCRE:
LC81B:  LDA  S_S_CRDT
LC81D:  LDY  #$00                   ;YES
LC81F:  CMP  #$02                   ;CC IF 1 CREDIT, CS IF 2 OR MORE
LC821:  LDA  SWFINA
LC823:  AND  #MSTRT2|MSTRT1
LC825:  STY  SWFINA
LC827:  BEQ  LC871                  ;IFNE  EITHER START PRESSED?
LC829:  BCS  LC830                  ;IFCC  YES. 1 CREDIT?
LC82B:  AND  #MSTRT1                ;YES.
LC82D:  CLV                         ;ELSE
LC82E:  BVC  LC835
LC830:  INY                         ;NO. 2 OR MORE CREDITS
LC831:  DEC  S_S_CRDT               ;REMOVE 1 CREDIT  [CS] Decrease the number of credits.
LC833:  AND  #MSTRT2
LC835:  BEQ  LC83A                  ;IFNE  START?
LC837:  DEC  S_S_CRDT               ;YES. REMOVE A CREDIT  [CS] Decrease the number of credits.
LC839:  INY
LC83A:  TYA
LC83B:  STA  NUMPLA                 ;SAVE # PLAYERS (0=ATTRACT)
LC83D:  BEQ  LC86E                  ;IFNE  GAME?
LC83F:  LDA  QSTATUS
LC841:  ORA  #MATRACT|MGTMOD        ;YES
LC843:  STA  QSTATUS                ;SET GAME MODE
LC845:  LDA  #$00                   ;ZERO BONUS COUNTER
LC847:  STA  S_BCCNT
LC849:  STA  S_BC
LC84B:  LDA  #CNEWGA                ;[CS] Reset the game-mode status
LC84D:  STA  QSTATE                 ;REQUEST NEW GAME STATE  [CS] indicator.
LC84F:  DEC  NUMPLA                 ;SET # PLAYERS=0 OR 1
LC851:  LDX  NUMPLA
LC853:  BEQ  LC857                  ;IFNE
LC855:  LDX  #$03                   ;2 PLAYERS
LC857:  INC  NGAMIL,X               ;UPDATE 1/2 GAME COUNT
LC85A:  BNE  LC85F                  ;IFEQ
LC85C:  INC  NGAMIH,X
LC85F:  LDA  NGAMES
LC862:  SEC
LC863:  ADC  NUMPLA
LC865:  CMP  #NRANKS
LC867:  BCC  LC86B                  ;IFCS  MAX OUT
LC869:  LDA  #NRANKS
LC86B:  STA  NGAMES                 ;COUNT # GAMES
LC86E:  CLV                         ;ELSE
LC86F:  BVC  LC890
LC871:  LDA  TBHD                   ;ATTRACT MODE D-CREDITS  [CS] Check the relative spinner
LC873:  BEQ  LC890                  ;IFNE  TRYING TO PLAY?  [CS] if no change, then skip a bit.
LC875:  BIT  QSTATUS                ;YES
LC877:  BMI  LC890                  ;IFPL  ATTRACT?
LC879:  LDA  #CDPRST                ;YES. PRESS START DISPLAY  [CS] Set game mode to ????
LC87B:  STA  QDSTATE
LC87D:  LDA  #$20
LC87F:  STA  QTMPAUS
LC881:  LDA  #CPAUSE                ;[CS] Game status = non-player input
LC883:  STA  QSTATE                 ;[CS] mode.
LC885:  LDA  #CDLADR                ;DISPLAY LADDER
LC887:  STA  QNXTSTA
LC889:  LDA  #$00                   ;[CS] Zero out the relative change in
LC88B:  STA  TBHD                   ;[CS] spinner position.
LC88D:  STA  ELICNT                 ;CLEAR AVOID SPIKES DISPLAY
LC890:  RTS

;------------------------------------------------------------------------------
; NONSTA - NON-STATE DEPENDENT PROCESSING
;------------------------------------------------------------------------------
NONSTA:
LC891:  LDA  IN1                    ;[CS] Check to see if the test swith
LC894:  AND  #MTEST                 ;[CS] is activated. If not, skip a bit.
LC896:  BNE  LC89F                  ;IFEQ  SYSTEM STATUS DISPLAY?
LC898:  LDA  #CSYSTM                ;YES  [CS] Set game status to "test menu".
LC89A:  STA  QSTATE                 ;[CS] (The first one.)
LC89C:  CLV                         ;ELSE
LC89D:  BVC  LC8E3
;NO. PROCESS CREDITS
LC89F:  BIT  QSTATUS
LC8A1:  BVS  LC8E3                  ;IFVC  ATTRACT?
LC8A3:  LDA  OPTIN2                 ;YES
LC8A5:  AND  #OM2GAM
LC8A7:  BEQ  LC8D2                  ;IFNE  2 GAME MIN OPTION?
LC8A9:  LDY  S_S_CRDT               ;YES.
LC8AB:  BNE  LC8B1                  ;IFEQ  CREDITS?
LC8AD:  LDA  #$80                   ;NO. SET 2 CREDITS MIN FLAG
LC8AF:  STA  TCMFLG
LC8B1:  BIT  TCMFLG                 ;Y=CREDITS
LC8B3:  BPL  LC8D2                  ;IFMI  2 GAME MIN?
LC8B5:  CPY  #$02                   ;YES.
LC8B7:  BCS  LC8CA                  ;IFCC  2 GAMES?
LC8B9:  TYA                         ;NO
LC8BA:  BEQ  LC8C4                  ;IFNE  1 CREDIT?
LC8BC:  LDA  #CD2GAM                ;YES
LC8BE:  STA  QDSTATE
LC8C0:  LDA  #CPAUSE                ;[CS] Game status = non-player input
LC8C2:  STA  QSTATE                 ;[CS] mode.
LC8C4:  JMP  NOSTART                ;DISABLE START
LC8C7:  CLV                         ;ELSE
LC8C8:  BVC  LC8D2
LC8CA:  LDA  #CDLADR
LC8CC:  STA  QSTATE
LC8CE:  LDA  #$00                   ;NOT ANY MORE. ENABLE START
LC8D0:  STA  TCMFLG
LC8D2:  LDA  S_S_CRDT               ;YES.  [CS] If there are no game credits,
LC8D4:  BEQ  NOSTART                ;IFNE  CREDITS?  [CS] skip a bit.
LC8D6:  JSR  PROCRE                 ;YES. PROCESS CREDITS

NOSTART:
LC8D9:  LDA  S_CMODE
LC8DB:  AND  #$03
LC8DD:  BNE  LC8E3                  ;IFEQ  FREE PLAY?
LC8DF:  LDA  #$02
LC8E1:  STA  S_S_CRDT
LC8E3:  INC  QFRAME                 ;UPDATE FRAME COUNTER
LC8E5:  LDA  QFRAME
LC8E7:  AND  #$01
LC8E9:  BEQ  LC8EE                  ;IFNE
LC8EB:  JSR  EAUPD                  ;PROCESS EAROM
LC8EE:  LDA  S_LMTIM
LC8F0:  BEQ  ZQAT4C                 ;IFNE  SLAM SWITCH ON?
LC8F2:  JSR  SSLAMS                 ;SLAM SOUND

ZQAT4C:
LC8F5:  LDA  QT2
LC8F8:  BEQ  LC901                  ;IFNE
LC8FA:  LDA  #$13
LC8FC:  CMP  CURWAV
LC8FE:  BCS  LC901                  ;IFCC
LC900:  SED
LC901:  LDA  SWFINA
LC903:  AND  #MFAKE                 ;SWITCH PROCESSED THIS FRAME?
LC905:  BEQ  LC90B                  ;IFNE
LC907:  LDA  #$00                   ;NO. FAKE PROCESS
LC909:  STA  SWFINA
LC90B:  RTS

;------------------------------------------------------------------------------
; NEWGAM - PREP-NEW GAME
;   FUNCTION
;------------------------------------------------------------------------------
NEWGAM:
LC90C:  JSR  INICHK                 ;INITIALIZE LANGUAGE PTRS, OPTIONS; CHECK FOR CHANGE
LC90F:  JSR  INIDSP                 ;INITIALIZE DISPLAY
LC912:  LDA  QSTATUS
LC914:  BPL  LC919                  ;IFMI  ATTRACT?
LC916:  JSR  CLRSCO                 ;NO. CLEAR SCORES  [CS] Clear out the player 1&2 scores.
LC919:  LDA  #$00
LC91B:  STA  LIVES2                 ;ONE PLAYER GAME (DEFAULT: PLAYER 2 DEAD)
LC91D:  LDX  NUMPLA                 ;GIVE EACH PLAYER "NEW GAME" EQUIP
LC91F:  STX  PLAYUP
LC921:  LDX  PLAYUP
LC923:  LDA  LVSGAM                 ;GET # LIVES
LC926:  .byte $9D, $48, $00         ;STA LIVES1,X (forced absolute)  INITIAL # OF LIVES (GUNS)  [CS] Set # of lives for the player
LC929:  LDA  #$FF                   ;[CS] Set level for player to FF.
LC92B:  .byte $9D, $46, $00         ;STA WAVEN1,X (forced absolute)  FORCE REQUEST RATE STATE
LC92E:  DEC  PLAYUP                 ;[CS] Do this for player one and two.
LC930:  BPL  LC921                  ;MIEND  ENDLOOP AFTER ALL PLAYERS PROCESSED
LC932:  LDA  #$00
LC934:  STA  NEWPLA                 ;START GAME WITH 1ST PLAYER UP.
LC936:  STA  PLAGRO                 ;DEACTIVATE STAR FIELD
LC939:  LDA  NUMPLA                 ;INDUCE "PLAY PLAYER 1" MESSAGE
LC93B:  STA  PLAYUP                 ;IF 2 PLAYER GAME.
LC93D:  JMP  INIRA0                 ;INITIALIZE FOR PLAYER RATE REQUEST  [CS] Go to level selection menu

;------------------------------------------------------------------------------
; NEWLIF - PREP-NEW LIFE
;------------------------------------------------------------------------------
NEWLIF:
LC940:  LDA  #CDPLAY                ;[CS] Set game play to attract
LC942:  STA  QDSTATE                ;DEFAULT  [CS] screen or gameplay mode.
LC944:  LDA  #CNWLF2
LC946:  STA  QSTATE
LC948:  STA  QNXTSTA
LC94A:  LDA  NEWPLA
LC94C:  CMP  PLAYUP
LC94E:  BEQ  LC96C                  ;IFNE  SAME PLAYER AS BEFORE?
LC950:  STA  PLAYUP                 ;NO
LC952:  LDA  QSTATUS
LC954:  BPL  LC96C                  ;IFMI  ATTRACT?
LC956:  LDA  #CDPLPL                ;NO.
LC958:  STA  QDSTATE                ;WARN PLAYER DISPLAY
LC95A:  LDA  #CPAUSE                ;[CS] Game status = non-player
LC95C:  STA  QSTATE                 ;FOR 2 SECONDS  [CS] input mode.
LC95E:  LDA  #4*SECOND              ;LONGER PAUSE
LC960:  LDY  COCTAL                 ;[CS] Is this game a cocktail?
LC963:  BEQ  LC967                  ;IFNE  COCKTAIL?  [CS] If so, branch.
LC965:  LDA  #2*SECOND              ;YES. NOT AS LONG
LC967:  STA  QTMPAUS                ;(SWITCH PLACES)
LC969:  JSR  SWAPEN                 ;SWAP ENEMIES  [CS] Swap some game variables betwen the active and inactive players.
LC96C:  JSR  COCFLI                 ;COCKTAIL FLIP
LC96F:  LDX  PLAYUP                 ;[CS] Load the level for the
LC971:  LDA  WAVEN1,X               ;[CS] current player, store in
LC973:  STA  CURWAV                 ;PLAYER'S WAVE #  [CS] $009F.
LC975:  JSR  INEWLI                 ;INITIALIZE OBJECTS(DEACTIVATE)  [CS] Game initialization routine
LC978:  JMP  INISOU                 ;SOUNDS OFF  [CS] Pokey initialization routine.

;------------------------------------------------------------------------------
; NEWLF2 - PREP-NEW LIFE PART 2
;------------------------------------------------------------------------------
NEWLF2:
LC97B:  LDA  #CPLAY                 ;PLAY STATE FOR
LC97D:  STA  QNXTSTA                ;GAME AFTER PAUSE
LC97F:  LDA  #CDPLAY
LC981:  STA  QDSTATE                ;AND DISPLAY NOW
LC983:  LDA  #CPAUSE                ;[CS] Game status = non-player
LC985:  STA  QSTATE                 ;[CS] input mode.
LC987:  LDA  #1*SECOND              ;PAUSE
LC989:  STA  QTMPAUS
LC98B:  RTS

;------------------------------------------------------------------------------
; ENDWAV - PREP-END OF WAVE SETUP STATE
;------------------------------------------------------------------------------
ENDWAV:
LC98C:  LDX  PLAYUP
LC98E:  LDA  WAVEN1,X               ;[CS] Get level for current player.
LC990:  CMP  #$62                   ;[CS] If we are below level 98,
LC992:  BCS  LC998                  ;IFCC  MAX AT 99  [CS] then increase the level.
LC994:  INC  WAVEN1,X               ;INCREMENT PLAYER'S WAVE #  [CS] Otherwise, do nothing
LC996:  INC  CURWAV                 ;[CS] with it.
LC998:  LDA  #CNEWV2                ;[CS] Game status = zooming in the
LC99A:  STA  QSTATE                 ;[CS] next level to be played.
LC99C:  LDA  BONUS,X                ;[CS] Which choice was selected by the player on the level selection screen? (This is the choice #, not level #.)
LC99F:  BEQ  LC9AC                  ;IFNE  BONUS?  [CS] Skip some stuff if they chose the first level.
LC9A1:  JSR  BONSCO                 ;DETERMINE BONUS & UPDATE SCORE
LC9A4:  LDX  #$FF                   ;INDICATE TEMPS HAVE BONUS
LC9A6:  JSR  UPSCOR                 ;UPDATE SCORE
LC9A9:  JSR  SAUSON                 ;MAKE NOISE

;.SBTTL PREP-NEW WAVE SETUP STATE
;FALL INTO NEW WAVE
LC9AC:  JMP  INEWAV                 ;INITIALIZE ENEMY POSITIONS

;------------------------------------------------------------------------------
; ENDLIF - PREP-LOSS OF BASE PROCESS STATE
;------------------------------------------------------------------------------
ENDLIF:
LC9AF:  LDA  #0*SECOND              ;NORMALLY NO PAUSE
LC9B1:  STA  QTMPAUS
LC9B3:  LDX  PLAYUP                 ;[CS] Load the current player number.
LC9B5:  DEC  LIVES1,X               ;DELETE ONE OF CURRENT PLAYER'S LIVES  [CS] Decrease player lives by one.
LC9B7:  LDA  LIVES1
LC9B9:  ORA  LIVES2
LC9BB:  BNE  LC9C3                  ;IFEQ  BOTH DEAD?
LC9BD:  JSR  ENDGAM                 ;YES. END GAME STATE (5 HI CHECK)
LC9C0:  CLV                         ;ELSE
LC9C1:  BVC  LC9F0
LC9C3:  LDX  PLAYUP
LC9C5:  LDA  LIVES1,X               ;NO. AT LEAST 1 PLAYER IS ALIVE  [CS] Player has any lives left?
LC9C7:  BNE  LC9D1                  ;IFEQ  CURRENT PLAYER DEAD?
LC9C9:  LDA  #CDGOVR                ;YES. "GAME OVER PLAYER X"
LC9CB:  STA  QDSTATE
LC9CD:  LDA  #2*SECOND              ;LONGER PAUSE
LC9CF:  STA  QTMPAUS
LC9D1:  LDA  NUMPLA
LC9D3:  BEQ  LC9DB                  ;IFNE  2 PLAYERS?
LC9D5:  LDA  NEWPLA                 ;YES. SWITCH TO OTHER PLAYER
LC9D7:  EOR  #$01
LC9D9:  STA  NEWPLA
LC9DB:  LDX  NEWPLA
LC9DD:  LDA  LIVES1,X               ;TEST # OF BASES FOR OTHER PLAYER  [CS] Player have any lives left?
LC9DF:  BEQ  LC9D1                  ;NEEND  EXIT IF PLAYER IS ALIVE
LC9E1:  LDA  #CNEWLI                ;THEN NEW LIFE SETUP
LC9E3:  LDY  WAVEN1,X
LC9E5:  INY
LC9E6:  BNE  LC9EA                  ;IFEQ  NEW GAME FOR NEXT PLAYER?
LC9E8:  LDA  #CINIRAT               ;YES. INITIALIZE RATE REQUEST STATE
LC9EA:  STA  QNXTSTA
LC9EC:  LDA  #CPAUSE                ;PAUSE FOR END OF LIFE TO SOAK IN  [CS] Game status = non-player input
LC9EE:  STA  QSTATE                 ;[CS] mode.
LC9F0:  RTS

;------------------------------------------------------------------------------
; ENDGAM - PREP-END OF GAME PROCESS STATE
;------------------------------------------------------------------------------
ENDGAM:
LC9F1:  LDA  #$00
LC9F3:  STA  HIWAVE
LC9F6:  LDX  NUMPLA
;LOOP FOR EACH PLAYER
LC9F8:  LDA  WAVEN1,X
LC9FA:  CMP  HIWAVE
;DETERMINE HIGHEST WAVE REACHED
LC9FD:  BCC  LCA02                  ;IFCS
LC9FF:  STA  HIWAVE
LCA02:  DEX
LCA03:  BPL  LC9F8                  ;MIEND
LCA05:  LDY  HIWAVE
LCA08:  BEQ  LCA0D                  ;IFNE
LCA0A:  DEC  HIWAVE
LCA0D:  LDA  #CDLADR
LCA0F:  BIT  QSTATUS
LCA11:  BPL  LCA15                  ;IFMI  ATTRACT?
LCA13:  LDA  #CHISCHK               ;NO. TEST FOR HI SCORE
LCA15:  STA  QSTATE                 ;REQUEST HI CHECK OR LADDER DISPLAY
LCA17:  RTS

;------------------------------------------------------------------------------
; DLADR - [note] No high score to enter: back to attract (clear MATRACT/MGTMOD, NUMPLA = 0), pause $A0 at
;   double time showing the high-score table (CDHITB), then the logo state (QNXTSTA = CLOGO).
;------------------------------------------------------------------------------
DLADR:
LCA18:  LDA  QSTATUS
LCA1A:  AND  #<[~[MATRACT|MGTMOD]]
;PUT INTO ATTRACT
LCA1C:  STA  QSTATUS                ;REQUEST DISPLAY OF LADDER
LCA1E:  LDA  #$00
LCA20:  STA  NUMPLA                 ;RETURN TO PLAYER
LCA22:  LDA  #CLOGO
LCA24:  STA  QNXTSTA                ;REQUEST NEW GAME AFTER
LCA26:  LDA  #CPAUSE                ;A LONG DELAY  [CS] Game status=non-player input
LCA28:  STA  QSTATE                 ;[CS] mode.
LCA2A:  LDA  #$A0
LCA2C:  STA  QTMPAUS
LCA2E:  LDA  #$01                   ;DOUBLE TIME
LCA30:  STA  PSCALE
LCA33:  LDA  #CDHITB                ;[CS] Set game mode to post high
LCA35:  STA  QDSTATE                ;[CS] score initial entry.
LCA37:  RTS

;.SBTTL UTILITY-MASKS

D70MSK:
;[CS] DATA segment of unknown size.
LCA38:  .byte $80, $40, $20, $10, $08, $04, $02, $01

D07MSK:
LCA40:  .byte $01, $02, $04, $08, $10, $20, $40, $80

;------------------------------------------------------------------------------
; COCFLI - COCKTAIL FLIP
;   INPUT: COCTAL NOT 0 IF COCKTAIL GAME
;   OUTPUT: FLIP BIT SET IF COCKTAIL & PLAYER?
;   OTHERWISE IT IS CLEARED.
;   [CS] Subroutine begins here.
;------------------------------------------------------------------------------
COCFLI:
LCA48:  LDY  #MVINVY
LCA4A:  LDA  COCTAL                 ;[CS] Is this game a cocktail?
LCA4D:  BEQ  LCA57                  ;IFNE  COCKTAIL GAME?  [CS] If not, branch.
LCA4F:  LDA  PLAYUP                 ;YES.  [CS] Is this player one?
LCA51:  BEQ  LCA57                  ;IFNE  PLAYER 2?  [CS] If so, branch.
LCA53:  LDA  #MFLIP                 ;YES. FLIP SCREEN.
LCA55:  LDY  #MVINVX                ;[CS] Invert (flip) X the Axis.
LCA57:  EOR  TNKOUT
LCA59:  AND  #MFLIP                 ;[CS] Documented as setting the
LCA5B:  EOR  TNKOUT                 ;[CS] unused coin cointer.
LCA5D:  STA  TNKOUT                 ;SET/CLEAR BIT
LCA5F:  STY  TOUT0                  ;[CS] Store the axis flip state.
LCA61:  RTS

;------------------------------------------------------------------------------
; CLRSCO - SCORE-CLEAR
;   CLEAR BOTH SCORES
;------------------------------------------------------------------------------
CLRSCO:
LCA62:  LDA  #$00                   ;[CS] Clear out the current
LCA64:  LDX  #$05                   ;[CS] score for player one
LCA66:  STA  LSCORL,X               ;[CS] and for player two.
LCA68:  DEX
LCA69:  BPL  LCA66                  ;MIEND
LCA6B:  RTS

;------------------------------------------------------------------------------
; UPSCOR - SCORE-ENEMY POINTS, GENERAL SCORE UPDATE
;   FUNCTION: GIVE POINTS FOR ENEMY SHOT DOWN
;   INPUT: X=INDEX OF PTS TO ADD (IF OUT OF TABLE THEN
;   ADD PTS IN TEMP0,1,&2
;------------------------------------------------------------------------------
UPSCOR:
LCA6C:  SED                         ;[CS] Decimal math mode used for scores.
LCA6D:  BIT  QSTATUS
LCA6F:  BPL  LCAEF                  ;IFMI  ATTRACT?
LCA71:  LDY  PLAYUP                 ;[CS] If we are dealing with player 2,
LCA73:  BEQ  LCA77                  ;IFNE  NO.  [CS] offset the memory location of the
LCA75:  LDY  #$03                   ;PLAYER 2?  [CS] score by three bytes.
LCA77:  CPX  #<[TUPSLE-TUPSCL]
LCA79:  BCC  LCA91                  ;IFCS  BONUS IN TABLE?
LCA7B:  LDA  TEMP0                  ;NO. IN TEMPS
LCA7D:  CLC
LCA7E:  .byte $79, $40, $00         ;ADC LSCORL,Y (forced absolute)  [CS] Update the score.
LCA81:  .byte $99, $40, $00         ;STA LSCORL,Y (forced absolute)
LCA84:  LDA  TEMP1
LCA86:  .byte $79, $41, $00         ;ADC LSCORM,Y (forced absolute)
LCA89:  .byte $99, $41, $00         ;STA LSCORM,Y (forced absolute)
LCA8C:  LDA  TEMP2
LCA8E:  CLV                         ;ELSE
LCA8F:  BVC  LCAA6
LCA91:  LDA  TUPSCL,X               ;ADD IN L,M AND H BYTES FROM
LCA94:  CLC                         ;SCORE TABLE TO CORRECT
LCA95:  .byte $79, $40, $00         ;ADC LSCORL,Y (forced absolute)  PLAYER'S SCORE  [CS] Update the score.
LCA98:  .byte $99, $40, $00         ;STA LSCORL,Y (forced absolute)
LCA9B:  LDA  TUPSCM,X
LCA9E:  .byte $79, $41, $00         ;ADC LSCORM,Y (forced absolute)
LCAA1:  .byte $99, $41, $00         ;STA LSCORM,Y (forced absolute)
LCAA4:  LDA  #$00
LCAA6:  PHP
LCAA7:  .byte $79, $42, $00         ;ADC LSCORH,Y (forced absolute)
LCAAA:  .byte $99, $42, $00         ;STA LSCORH,Y (forced absolute)
LCAAD:  PLP
;GIVE BONUS FOR BIG PTS.
LCAAE:  BEQ  LCABB                  ;IFNE  BIG BONUS?
LCAB0:  LDX  BLIFIN                 ;YES
LCAB3:  BEQ  LCABB                  ;IFNE  BONUS ALLOWED?
LCAB5:  CPX  TEMP2                  ;YES.
LCAB7:  BEQ  GIVBON
LCAB9:  BCC  GIVBON                 ;BONUS >= INTERVAL?
LCABB:  BCC  LCAEF                  ;IFCS  PASS 10K BOUNDARY?
LCABD:  LDX  BLIFIN                 ;YES. ET BONUS LIFE INTERVAL (IN 10 K UNITS)
LCAC0:  BEQ  LCAEE                  ;IFNE  BONUS ALLOWED?
LCAC2:  CPX  #$03                   ;YES.
LCAC4:  BCC  LCAD1                  ;IFCS  OVER 20 K INTERVAL?
LCAC6:  SEC
LCAC7:  SBC  BLIFIN
LCACA:  BEQ  GIVBON                 ;BRANCH IF NO REMAINDER
LCACC:  BCS  LCAC6                  ;CCEND  EXIT IF REMAINDER
LCACE:  CLV                         ;ELSE
LCACF:  BVC  LCAEE
LCAD1:  CPX  #$02                   ;20 K INTERVAL?
LCAD3:  BNE  GIVBON                 ;IFEQ
LCAD5:  AND  #$01                   ;YES.
LCAD7:  BEQ  GIVBON
LCAD9:  CLV                         ;ELSE
LCADA:  BVC  LCAEE

GIVBON:          ;10 K INTERVAL
LCADC:  LDX  PLAYUP                 ;YES. GIVE BONUS LIFE
LCADE:  LDA  LIVES1,X               ;[CS] Compare the number of player's
LCAE0:  CMP  #$06                   ;[CS] lives to "6".
LCAE2:  BCS  LCAEE                  ;IFCC  MAX AT 6
LCAE4:  INC  LIVES1,X               ;[CS] Increase player's lives by one.
LCAE6:  JSR  SAUSON                 ;MAKE BONUS SOUND
LCAE9:  LDA  #$20
LCAEB:  STA  BOFLASH                ;REQUEST BONUS LIFE FLASH
LCAEE:  SEC
LCAEF:  CLD                         ;[CS] Set normal math mode and
LCAF0:  RTS                         ;[CS] return.

TUPSCL:
;[CS] Multiple DATA segments of unknown size
;[CS] CAF1-CAF8  CAF9-CB00  CB01-
LCAF1:  .byte $00, $50, $00, $00, $50, $50, $00, $50

TUPSLE:
TUPSCM:
LCAF9:  .byte $00, $01, $02, $01, $00, $02, $05, $07

;==============================================================================
; MODULE ALSOUN   ALSOUN.MAC
;   Sounds: POKEY initialisation, sound start/stop entry points, sound tables
;   (frequency/amplitude envelopes) and the per-frame sound driver.
;==============================================================================

;.SBTTL SOUND TABLES

PNTRS:
;EX1: 0FF,1,-1,6 DESCRIBES THE FOLLOWING SEQUENCE
;0FF,0FE,0FD,0FC,0FB,0FA,0F9
;EX2: 0,45,0,1 WILL OUTPUT 0 FOR 46 FRAMES
;THE ABOVE MACRO GENERATES THE OFFSETS FROM THE 'SOUND' BASE ADDRESS
;FOR A CHANNEL OF DATA.
;IF LESS THAN 6 CHANNELS ARE USED, THE REMAINING POINTERS ARE SET TO 0
;A 0 VALUE POINTER INDICATES AN IDLE CHANNEL.
;EX: SOUND: .BYTE 0
;CH1: .BYTE 0,45,0,1
;CH2: .BYTE 0,45,0,2,3,7,9,1
;BY CALLING 'OFFSET CH' , THE FOLLOWING WILL BE PLACED IN LINE
;.BYTE CH1-SOUND
;.BYTE CH2-SOUND
;.BYTE 0,0,0,0
;TABLES OF OFFSET POINTER FOR SOUNDS. (6 BYTES PER SOUND NUMBER)
LCB01:  .byte $00, $00, $00, $00, $00, $00, $00, $00 ;OFFSET LO  CURSOR MOVES
LCB09:  .byte [[LO5F-SOUND]/2]-2, [[LO5A-SOUND]/2]-2, $00, $00, $00, $00, $00, $00
LCB11:  .byte $00, $00, [[EX2F-SOUND]/2]-2, [[EX2A-SOUND]/2]-2, $00, $00, $00, $00 ;OFFSET EX  ENEMY EXPLOSION
LCB19:  .byte $00, $00, $00, $00, $00, $00, $00, $00
LCB21:  .byte $00, $00, $00, $00, [[LA3F-SOUND]/2]-2, [[LA3A-SOUND]/2]-2, $00, $00 ;OFFSET LA  PLAYER FIRE
LCB29:  .byte $00, $00, $00, $00, $00, $00, $00, $00
LCB31:  .byte $00, $00, $00, $00, $00, $00, $00, $00 ;OFFSET PU  PULSATION
LCB39:  .byte $00, $00, [[PU6F-SOUND]/2]-2, [[PU6A-SOUND]/2]-2, $00, $00, $00, $00
LCB41:  .byte $00, $00, $00, $00, $00, $00, [[WP4F-SOUND]/2]-2, [[WP4A-SOUND]/2]-2 ;OFFSET WP  SPECIAL SCORE
LCB49:  .byte $00, $00, $00, $00, $00, $00, $00, $00
LCB51:  .byte [[DI1F-SOUND]/2]-2, [[DI1A-SOUND]/2]-2, $00, $00, $00, $00, $00, $00 ;OFFSET DI  PLAYER DIES
LCB59:  .byte $00, $00, $00, $00, $00, $00, $00, $00
LCB61:  .byte $00, $00, $00, $00, $00, $00, $00, $00 ;OFFSET T2  THRUST IN TUBE
LCB69:  .byte $00, $00, [[T26F-SOUND]/2]-2, [[T26A-SOUND]/2]-2, $00, $00, $00, $00
LCB71:  .byte $00, $00, $00, $00, $00, $00, $00, $00 ;OFFSET T3  THRUST IN SPACE
LCB79:  .byte $00, $00, [[T36F-SOUND]/2]-2, [[T36A-SOUND]/2]-2, $00, $00, $00, $00
LCB81:  .byte $00, $00, $00, $00, $00, $00, $00, $00 ;OFFSET ES  ENEMY SHOT
LCB89:  .byte $00, $00, $00, $00, $00, $00, [[ES8F-SOUND]/2]-2, [[ES8A-SOUND]/2]-2
LCB91:  .byte $00, $00, $00, $00, $00, $00, $00, $00 ;OFFSET EL  ENEMY LINE DESTRUCTION
LCB99:  .byte $00, $00, $00, $00, [[EL7F-SOUND]/2]-2, [[EL7A-SOUND]/2]-2, $00, $00
LCBA1:  .byte [[SL1F-SOUND]/2]-2, [[SL1A-SOUND]/2]-2, $00, $00, $00, $00, $00, $00 ;OFFSET SL  SLAM
LCBA9:  .byte $00, $00, $00, $00, $00, $00, $00, $00
LCBB1:  .byte [[S31F-SOUND]/2]-2, [[S31A-SOUND]/2]-2, $00, $00, $00, $00, $00, $00 ;OFFSET S3  3 SECONDS LEFT WARNING
LCBB9:  .byte $00, $00, $00, $00, $00, $00, $00, $00
LCBC1:  .byte $00, $00, $00, $00, $00, $00, $00, $00 ;OFFSET PO  PULSAR OFF
LCBC9:  .byte $00, $00

SOUND:
LCBCB:  .byte [[PO6F-SOUND]/2]-2, [[PO6A-SOUND]/2]-2, $00, $00, $00, $00

T51F:
;A GOOD PLACE FOR THE CHECKSUM
;DATA STRUCTURE:
;A CHANNEL CONSISTS OF A SERIES OF 4 BYTE SEQUENCES:
;BYTE FUNCTION
;1 STARTING VALUE OF SEQUENCE
;2 # OF FRAMES BEFORE NEXT CHANGE
;3 AMOUNT OF CHANGE (SEE NOTE 1)
;4 TOTAL NUMBER OF CHANGES+1 (# OF DIFFERENT VALUES)
;TO STOP A CHANNEL, PUT IN 0,0
;TO LOOP BACK, PUT IN X,0, WHERE X=
;<OFFSET FROM SOUND: OF RESTART LOC>/2
;NOTE 1: FOR NOISE/AMPLITUDE CHANNEL, AMPLITUDE WILL
;NOT OVERFLOW INTO NOISE
;EXPLOSION SOUND
LCBD1:  .byte $C0, $08, $04, $10
LCBD5:  .byte $00, $00

T51A:
LCBD7:  .byte $A6, $20, $F8, $04
LCBDB:  .byte $00, $00

T52F:
LCBDD:  .byte $40, $08, $04, $10
LCBE1:  .byte $00, $00

T52A:
LCBE3:  .byte $A6, $20, $FE, $04
LCBE7:  .byte $00, $00

LA3F:
;LAUNCH SOUND
LCBE9:  .byte $10, $01, $07, $20, $00, $00

LA3A:
LCBEF:  .byte $A2, $01, $F8, $20, $00, $00

DI1F:
;DIE
LCBF5:  .byte $08, $04, $20, $0A
LCBF9:  .byte $08, $04, $01, $09
LCBFD:  .byte $10, $0D, $04, $0C
LCC01:  .byte $00, $00

DI1A:
LCC03:  .byte $08, $04, $00, $0A
LCC07:  .byte $68, $04, $00, $09
LCC0B:  .byte $68, $12, $FF, $09
LCC0F:  .byte $00, $00

WP4F:
;BONUS SOUND
LCC11:  .byte $40, $01, $00, $01
LCC15:  .byte $40, $01, $FF, $40
LCC19:  .byte $30, $01, $FF, $30
LCC1D:  .byte $20, $01, $FF, $20
LCC21:  .byte $18, $01, $FF, $18
LCC25:  .byte $14, $01, $FF, $14
LCC29:  .byte $12, $01, $FF, $12
LCC2D:  .byte $10, $01, $FF, $10
LCC31:  .byte $00, $00

WP4A:
LCC33:  .byte $A8, $93, $00, $02
LCC37:  .byte $00, $00

LO5F:
;CURSOR CROSSED A LINE
LCC39:  .byte $0F, $04, $00, $01
LCC3D:  .byte $00, $00

LO5A:
LCC3F:  .byte $A2, $04, $40, $01
LCC43:  .byte $00, $00

ES8F:
LCC45:  .byte $00, $03, $02, $09    ;ENEMY SHOT
LCC49:  .byte $00, $00

ES8A:
LCC4B:  .byte $08, $03, $FF, $09
LCC4F:  .byte $00, $00

EL7F:
LCC51:  .byte $80, $01, $E8, $05    ;ENEMY LINE DESTRUCTION
LCC55:  .byte $00, $00

EL7A:
LCC57:  .byte $A1, $01, $01, $05
LCC5B:  .byte $00, $00

EX2F:
LCC5D:  .byte $01, $08, $02, $10    ;ENEMY EXPLOSION
LCC61:  .byte $00, $00

EX2A:
LCC63:  .byte $86, $20, $00, $04
LCC67:  .byte $00, $00

SL1F:
LCC69:  .byte $18, $04, $00, $FF    ;SLAM SOUND
LCC6D:  .byte $00, $00

SL1A:
LCC6F:  .byte $AF, $04, $00, $FF
LCC73:  .byte $00, $00

T26F:
LCC75:  .byte $C0, $02, $FF, $FF    ;THRUST SOUND ON TUBE
LCC79:  .byte $00, $00

T26A:
LCC7B:  .byte $28, $02, $00, $F0
LCC7F:  .byte $00, $00

T36F:
LCC81:  .byte $10, $0B, $01, $40    ;THRUST SOUND IN SPACE
LCC85:  .byte $00, $00

T36A:
LCC87:  .byte $86, $40, $00, $0B
LCC8B:  .byte $00, $00

S31F:
LCC8D:  .byte $20, $80, $00, $03, $00, $00 ;3 SECOND WARNING

S31A:
LCC93:  .byte $A8, $40, $F8, $06, $00, $00

PU6F:
;PULSATION SOUND ON
LCC99:  .byte $B0, $02, $00, $FF
LCC9D:  .byte $00, $00

PU6A:
LCC9F:  .byte $C8, $01, $02, $FF
LCCA3:  .byte $C8, $01, $02, $FF
LCCA7:  .byte $00, $00

PO6F:            ;TURN PULSATION OFF
PO6A:
LCCA9:  .byte $C0, $01, $00, $01
LCCAD:  .byte $00, $00

CHKSM9:
LCCAF:  .byte QCHKS9

;------------------------------------------------------------------------------
; IPEXPL - START NOISE  (comment at the call)
;   (also: CPEXPL)
;------------------------------------------------------------------------------
IPEXPL:
CPEXPL:
LCCB0:  LDA  #SIDDI                 ;PLAYER DIES
LCCB2:  JMP  SNDON

;------------------------------------------------------------------------------
; SBOING - YES. MAKE SOUND  (comment at the call)
;------------------------------------------------------------------------------
SBOING:
LCCB5:  LDA  #SIDLO                 ;CURSOR MOVES
LCCB7:  BNE  SNDON

;------------------------------------------------------------------------------
; SAUSON - MAKE NOISE  (comment at the call)
;   MAKE BONUS SOUND  (another call)
;------------------------------------------------------------------------------
SAUSON:
LCCB9:  LDA  #SIDWP                 ;SPECIAL SCORE SOUND
LCCBB:  BNE  SNDON

;------------------------------------------------------------------------------
; ESLSON - [note] Start the enemy-shot sound (sound SIDES) via SNDON.
;------------------------------------------------------------------------------
ESLSON:
LCCBD:  LDA  #SIDES                 ;ENEMY SHOT
LCCBF:  BNE  SNDON

;------------------------------------------------------------------------------
; CCEXPL - CHARGE-CHARGE  (comment at the call)
;   BANG SOUND  (another call)
;   MAKE NOISE  (another call)
;   (also: CIEXPL, EXSNON)
;------------------------------------------------------------------------------
CCEXPL:
CIEXPL:
EXSNON:
LCCC1:  LDA  #SIDEX                 ;EXPLOSION

SNDON:
LCCC3:  BIT  QSTATUS                ;ATTRACT MODE?
LCCC5:  BPL  NWSNON                 ;IFMI  NO, OK TO START A SOUND

FSNDON:
LCCC7:  STX  MTEMP
LCCC9:  STY  MTEMP+1
LCCCB:  TAY                         ;USE AS INDEX
LCCCC:  LDX  #NCHANL-1              ;NO.
LCCCE:  LDA  PNTRS,Y
LCCD1:  BEQ  LCCE1                  ;IFNE  IF POINTER 0 DONT TOUCH THIS CHANNEL
LCCD3:  STX  SINDEX
LCCD5:  STA  POINT,X                ;IF NOT SET UP POINTER
LCCD7:  LDA  #$01
LCCD9:  STA  FRAMES,X               ;DUMMY START, NO SOUND
LCCDB:  STA  COUNT,X                ;TILL MODSND STORES TO POKEY
LCCDD:  LDA  #$FF
LCCDF:  STA  SINDEX
LCCE1:  DEY
LCCE2:  DEX
LCCE3:  BPL  LCCCE                  ;MIEND
LCCE5:  LDX  MTEMP                  ;RESTORE X & Y UPON RETURN
LCCE7:  LDY  MTEMP+1

NWSNON:
LCCE9:  RTS

;------------------------------------------------------------------------------
; SLAUNC - LAUNCH SOUND  (comment at the call)
;------------------------------------------------------------------------------
SLAUNC:
LCCEA:  LDA  #SIDLA                 ;LAUNCH SOUND - PLAYER FIRE
LCCEC:  BNE  SNDON

;------------------------------------------------------------------------------
; SOUTS2 - YES. START RUMBLE  (comment at the call)
;------------------------------------------------------------------------------
SOUTS2:
LCCEE:  LDA  #SIDT2                 ;THRUST ON TUBE
LCCF0:  BNE  SNDON

;------------------------------------------------------------------------------
; SOUTS3 - START SPACE SOUND  (comment at the call)
;------------------------------------------------------------------------------
SOUTS3:
LCCF2:  LDA  #SIDT3                 ;THRUST IN SPACE
LCCF4:  BNE  SNDON

;------------------------------------------------------------------------------
; SELICO - MAKE SOUND  (comment at the call)
;------------------------------------------------------------------------------
SELICO:
LCCF6:  LDA  #SIDEL                 ;ENEMY LINE DESTRUCTION SOUND
LCCF8:  BNE  SNDON

;------------------------------------------------------------------------------
; SSLAMS - SLAM SOUND  (comment at the call)
;------------------------------------------------------------------------------
SSLAMS:
LCCFA:  LDA  #SIDSL                 ;SLAM SOUND
LCCFC:  BNE  FSNDON

;------------------------------------------------------------------------------
; S3SWAR - 3 SECONDS WARNING  (comment at the call)
;------------------------------------------------------------------------------
S3SWAR:
LCCFE:  LDA  #SIDS3                 ;3 SECONDS WARNING
LCD00:  BNE  SNDON

;------------------------------------------------------------------------------
; PULSTR - ACTIVE SO TURN ON  (comment at the call)
;------------------------------------------------------------------------------
PULSTR:
LCD02:  LDA  #SIDPU                 ;PULSATION
LCD04:  BNE  SNDON

;------------------------------------------------------------------------------
; PULSTO - TURN OFF THRUST SOUND  (comment at the call)
;   YES. TURN OFF  (another call)
;------------------------------------------------------------------------------
PULSTO:
LCD06:  LDA  #SIDPO                 ;PULSATION OFF
LCD08:  BNE  SNDON

;------------------------------------------------------------------------------
; MODSND - SOUND ROUTINE
;   CONTINUES A PREVIOUSLY STARTED SOUND
;   WHEN CHANNEL 1 GOES IDLE, ALL SOUND ENDS
;------------------------------------------------------------------------------
MODSND:
LCD0A:  LDX  #NCHANL-1              ;8 CHANNELS
LCD0C:  LDA  POINT,X
LCD0E:  BEQ  LCD8E                  ;IFNE  1 SET OF FRAMES EXPIRED?
LCD10:  CPX  SINDEX
LCD12:  BEQ  LCD8E                  ;IFNE
LCD14:  DEC  FRAMES,X               ;YES
LCD16:  BNE  LCD8E                  ;IFEQ  SUBCHANNEL ACTIVE?
LCD18:  DEC  COUNT,X                ;YES.
LCD1A:  BNE  LCD54                  ;IFEQ  MOVE TO NEXT CHANGE GROUP?

RESOUN:
LCD1C:  INC  POINT,X                ;YES. START VALUE
LCD1E:  INC  POINT,X
LCD20:  LDA  POINT,X
LCD22:  ASL
LCD23:  TAY
LCD24:  BCS  LCD36                  ;IFCC
LCD26:  LDA  SOUND+STVAL,Y
LCD29:  STA  CURRENT,X
LCD2B:  LDA  SOUND+NUMBER,Y
LCD2E:  STA  COUNT,X
LCD30:  LDA  SOUND+FRCNT,Y
LCD33:  CLV                         ;ELSE
LCD34:  BVC  LCD43
LCD36:  LDA  SOUND+STVAL+$100,Y
LCD39:  STA  CURRENT,X
LCD3B:  LDA  SOUND+NUMBER+$100,Y
LCD3E:  STA  COUNT,X
LCD40:  LDA  SOUND+FRCNT+$100,Y
LCD43:  STA  FRAMES,X
LCD45:  BNE  LCD51                  ;IFEQ  CHANNEL STILL ALIVE?
LCD47:  STA  POINT,X                ;NO. KILL IT
LCD49:  LDA  CURRENT,X
LCD4B:  BEQ  LCD51                  ;IFNE  RESTART CHANNEL?
LCD4D:  STA  POINT,X                ;YES. UPDATE PTR. WITH RESTART LOC
LCD4F:  BNE  RESOUN
LCD51:  CLV                         ;ELSE  NO. MAKE CHANGE IN OLD GROUP
LCD52:  BVC  LCD7F
LCD54:  ASL
LCD55:  TAY
LCD56:  BCS  LCD63                  ;IFCC
LCD58:  LDA  SOUND+FRCNT,Y
LCD5B:  STA  FRAMES,X
LCD5D:  LDA  SOUND+CHANGE,Y
LCD60:  CLV                         ;ELSE
LCD61:  BVC  LCD6B
LCD63:  LDA  SOUND+FRCNT+$100,Y
LCD66:  STA  FRAMES,X
LCD68:  LDA  SOUND+CHANGE+$100,Y
LCD6B:  LDY  CURRENT,X
LCD6D:  CLC
LCD6E:  ADC  CURRENT,X
LCD70:  STA  CURRENT,X
LCD72:  TXA
LCD73:  LSR
LCD74:  BCC  LCD7F                  ;IFCS
LCD76:  TYA
LCD77:  EOR  CURRENT,X
LCD79:  AND  #$F0
LCD7B:  EOR  CURRENT,X
LCD7D:  STA  CURRENT,X
LCD7F:  LDA  CURRENT,X              ;UPDATE POKEY AUDIO CHANNEL
LCD81:  CPX  #$08
LCD83:  BCC  LCD8B                  ;IFCS
LCD85:  STA  AUDF2-8,X
LCD88:  CLV                         ;ELSE
LCD89:  BVC  LCD8E
LCD8B:  STA  AUDF1,X
LCD8E:  DEX
LCD8F:  BMI  LCD94                  ;MIEND
LCD91:  JMP  LCD0C
LCD94:  RTS

;------------------------------------------------------------------------------
; INISOU - INITIALIZE SOUNDS  (comment at the call)
;   SOUNDS OFF  (another call)
;   [CS] Pokey initialization routine.
;------------------------------------------------------------------------------
INISOU:
LCD95:  LDA  #$00
LCD97:  STA  AUDF1+$F               ;[CS] Zero out Pokey #1 control reg
LCD9A:  STA  AUDF2+$F               ;[CS] Zero out Pokey #2 control reg
LCD9D:  STA  QT4

ZPOKST:
LCDA0:  LDX  #$04                   ;STOP POKEYS
LCDA2:  LDA  RANDOM                 ;[CS] Randomize the accumulator.
LCDA5:  LDY  RANDO2                 ;[CS] Randomize the Y register.
LCDA8:  CMP  RANDOM                 ;[CS] Compare ACC w/random number.
LCDAB:  BNE  LCDB0                  ;IFEQ  [CS] 254/255 chance of branching.
LCDAD:  CPY  RANDO2                 ;[CS] Compare w/random number.
LCDB0:  BEQ  LCDB7                  ;IFNE  HALTED POKEYS?  [CS] 1/255 chance of branching.
LCDB2:  STA  QT4
LCDB5:  LDX  #$00
LCDB7:  DEX
LCDB8:  BPL  LCDA8                  ;MIEND
LCDBA:  LDA  #$07                   ;[CS] Enable sound *and* fast potentiumeter scanning on the Pokey.
LCDBC:  STA  AUDF1+$F               ;[CS] Initialize Pokey #1
LCDBF:  STA  AUDF2+$F               ;[CS] Initialize Pokey #2
LCDC2:  LDX  #$07                   ;[CS] Zero out the volume/distortion
LCDC4:  LDA  #$00                   ;[CS] and pitch for all channels
LCDC6:  STA  AUDF1,X                ;[CS] on both Pokeys.
LCDC9:  STA  AUDF2,X
LCDCC:  STA  POINT,X
LCDCE:  STA  CURRENT,X
LCDD0:  DEX
LCDD1:  BPL  LCDC6                  ;MIEND  [CS] End of initialization loop
LCDD3:  LDA  #AUDCV                 ;[CS] Clear the master control
LCDD5:  STA  AUDCTL                 ;[CS] register (AUDCTL) for Pokey #1
LCDD8:  LDA  #AUDCV2                ;[CS] Clear the master control
LCDDA:  STA  AUD2CTL                ;[CS] register (AUDCTL) for Pokey #2
LCDDD:  RTS

;==============================================================================
; MODULE ALVROM   ALVROM.MAC
;   ROM tables for the vector pictures (.CSECT part of ALVROM): score
;   template, sub-buffer JSRL/JMPL tables, picture JSRL table PICLO.
;==============================================================================

;.SBTTL CODES FOR PICTURE TABLE

INVERS:
INVEXP:
PICLSB:
SCALOC:
;OFFSETS INTO SCORE TEMPLATE OF
LCDDE:  .byte SCP1SA+1-SCORES       ;SCOF SCP1SA+1  SCALE

PICMSB:
LCDDF:  .byte SCP2SA+1-SCORES       ;SCOF SCP2SA+1

LIVLOC:
LCDE0:  .byte SCP1LI-SCORES         ;SCOF SCP1LI  LIVES
LCDE1:  .byte SCP2LI-SCORES         ;SCOF SCP2LI

SCOLOC:
LCDE2:  .byte SCP1SC-SCORES         ;SCOF SCP1SC  SCORE
LCDE3:  .byte SCP2SC-SCORES         ;SCOF SCP2SC

HISLOC:
LCDE4:  .byte SCHISC-SCORES         ;SCOF SCHISC  HI SCORE

HIILOC:
LCDE5:  .byte SCHIIN-SCORES         ;SCOF SCHIIN  HI INITIALS

;.SBTTL SCORE/LIVES, HI SCORE TEMPLATE

SCORES:
LCDE6:  .word $7100                 ;SCAL 1,0
LCDE8:  .word $68C0+GREEN           ;CSTAT GREEN
LCDEA:  .word $8040                 ;CNTR  POSITION BEAM FOR PLAYER 1 SCORE/LIVES
LCDEC:  .word $016C, $1E40          ;VCTR -1C0,16C,0

SCP1SA:
LCDF0:  .word $7100                 ;SCAL 1,0

SCP1SC:
LCDF2:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.  DISPLAY SCORE
LCDF4:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.
LCDF6:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.
LCDF8:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.
LCDFA:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.
LCDFC:  .word [[CHAR_0&$1FFF]/2]+$A000 ;JSRL CHAR.0
LCDFE:  .word $0000, $1F70          ;VCTR -90,0,0
LCE02:  .word $7100                 ;SCAL 1,0
LCE04:  .word $5800                 ;VCTR 0,-10,0
LCE06:  .word $68C0+YELLOW          ;CSTAT YELLOW

SCP1LI:
LCE08:  .word [[LIFE0&$1FFF]/2]+$A000 ;JSRL LIFE0  DISPLAY LIVES
LCE0A:  .word [[LIFE0&$1FFF]/2]+$A000 ;JSRL LIFE0
LCE0C:  .word [[LIFE0&$1FFF]/2]+$A000 ;JSRL LIFE0
LCE0E:  .word [[LIFE0&$1FFF]/2]+$A000 ;JSRL LIFE0
LCE10:  .word [[LIFE0&$1FFF]/2]+$A000 ;JSRL LIFE0
LCE12:  .word [[LIFE0&$1FFF]/2]+$A000 ;JSRL LIFE0
LCE14:  .word $0030, $1FD0          ;VCTR -30,30,0  POSITION FOR HIGH SCORE
LCE18:  .word $68C0+GREEN           ;CSTAT GREEN

SCHISC:
LCE1A:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.  HIGH SCORE
LCE1C:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.
LCE1E:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.
LCE20:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.
LCE22:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.
LCE24:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.
LCE26:  .word $1FDC, $0000          ;VCTR 0,-24,0  POSITION FOR LEVEL
LCE2A:  .word $68C7                 ;CSTAT BLUE

SCLEVL:
LCE2C:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.  LEVEL #
LCE2E:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.
LCE30:  .word $68C0+GREEN           ;CSTAT GREEN
LCE32:  .word $0024, $1FE8          ;VCTR -18,24,0  POSITION FOR INITIALS

SCHIIN:
LCE36:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.  INITIALS
LCE38:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.
LCE3A:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.

SC1END:
LCE3C:  .word $7100                 ;SCAL 1,0  RTSL (1 PLAYER GAME) NOOP (2 PLAYER)
LCE3E:  .word $1FE0, $0028          ;VCTR 28,-20,0  POSITION FOR PLAYER 2 SCORE

SCP2SA:
LCE42:  .word $7100                 ;SCAL 1,0

SCP2SC:
LCE44:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.  PLAYER 2 SCORE
LCE46:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.
LCE48:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.
LCE4A:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.
LCE4C:  .word [[CHAR_&$1FFF]/2]+$A000 ;JSRL CHAR.
LCE4E:  .word [[CHAR_0&$1FFF]/2]+$A000 ;JSRL CHAR.0
LCE50:  .word $0000, $1F70          ;VCTR -90,0,0  POSITION FOR LIVES
LCE54:  .word $7100                 ;SCAL 1,0
LCE56:  .word $5800                 ;VCTR 0,-10,0
LCE58:  .word $68C0+YELLOW          ;CSTAT YELLOW

SCP2LI:
LCE5A:  .word [[LIFE0&$1FFF]/2]+$A000 ;JSRL LIFE0  LIVES DISPLAY
LCE5C:  .word [[LIFE0&$1FFF]/2]+$A000 ;JSRL LIFE0
LCE5E:  .word [[LIFE0&$1FFF]/2]+$A000 ;JSRL LIFE0
LCE60:  .word [[LIFE0&$1FFF]/2]+$A000 ;JSRL LIFE0
LCE62:  .word [[LIFE0&$1FFF]/2]+$A000 ;JSRL LIFE0
LCE64:  .word [[LIFE0&$1FFF]/2]+$A000 ;JSRL LIFE0

SCECOU:
SC2END:
LCE66:  .byte SC1END-SCORES-1
LCE67:  .byte SC2END-SCORES-1

BUFASL:
LCE68:  .word BAINFO
.alias BUFASH           $CE69    ;inside the line at LCE68 (LCE68+1)
LCE6A:  .word BAENEL
LCE6C:  .word BAWELL

BFASTA:
LCE6E:  .word BAINVA
LCE70:  .word BASHOT
LCE72:  .word BANYMP
LCE74:  .word BAEXPL
LCE76:  .word BACURS
LCE78:  .word BASTAR

BUFBSL:
LCE7A:  .word BBINFO
.alias BUFBSH           $CE7B    ;inside the line at LCE7A (LCE7A+1)
LCE7C:  .word BBENEL
LCE7E:  .word BBWELL
LCE80:  .word BBINVA
LCE82:  .word BBSHOT
LCE84:  .word BBNYMP

BFBSTA:
LCE86:  .word BBEXPL
LCE88:  .word BBCURS
LCE8A:  .word BBSTAR

BUFSWL:
LCE8C:  .word SWINFO
.alias BUFSWH           $CE8D    ;inside the line at LCE8C (LCE8C+1)
LCE8E:  .word SWENEL
LCE90:  .word SWWELL
LCE92:  .word SWINVA
LCE94:  .word SWSHOT
LCE96:  .word SWNYMP
LCE98:  .word SWEXPL
LCE9A:  .word SWCURS
LCE9C:  .word SWSTAR

JMPALO:
LCE9E:  .word [[BAINFO&$1FFF]/2]+$E000 ;JMPL BAINFO
.alias JMPAHI           $CE9F    ;inside the line at LCE9E (LCE9E+1)
LCEA0:  .word [[BAENEL&$1FFF]/2]+$E000 ;JMPL BAENEL
LCEA2:  .word [[BAWELL&$1FFF]/2]+$E000 ;JMPL BAWELL
LCEA4:  .word [[BAINVA&$1FFF]/2]+$E000 ;JMPL BAINVA
LCEA6:  .word [[BASHOT&$1FFF]/2]+$E000 ;JMPL BASHOT
LCEA8:  .word [[BANYMP&$1FFF]/2]+$E000 ;JMPL BANYMP
LCEAA:  .word [[BAEXPL&$1FFF]/2]+$E000 ;JMPL BAEXPL
LCEAC:  .word [[BACURS&$1FFF]/2]+$E000 ;JMPL BACURS
LCEAE:  .word [[BASTAR&$1FFF]/2]+$E000 ;JMPL BASTAR

JMPBLO:
LCEB0:  .word [[BBINFO&$1FFF]/2]+$E000 ;JMPL BBINFO
.alias JMPBHI           $CEB1    ;inside the line at LCEB0 (LCEB0+1)
LCEB2:  .word [[BBENEL&$1FFF]/2]+$E000 ;JMPL BBENEL
LCEB4:  .word [[BBWELL&$1FFF]/2]+$E000 ;JMPL BBWELL
LCEB6:  .word [[BBINVA&$1FFF]/2]+$E000 ;JMPL BBINVA
LCEB8:  .word [[BBSHOT&$1FFF]/2]+$E000 ;JMPL BBSHOT
LCEBA:  .word [[BBNYMP&$1FFF]/2]+$E000 ;JMPL BBNYMP
LCEBC:  .word [[BBEXPL&$1FFF]/2]+$E000 ;JMPL BBEXPL
LCEBE:  .word [[BBCURS&$1FFF]/2]+$E000 ;JMPL BBCURS
LCEC0:  .word [[BBSTAR&$1FFF]/2]+$E000 ;JMPL BBSTAR

JMPMAL:          ;MASTER POINTERS (MOVED TO VECRAM+0)
LCEC2:  .word [[SWNORM&$1FFF]/2]+$E000 ;JMPL SWNORM  PLAY DISPLAY STATE  [CS] Data loaded into Vector RAM?
.alias JMPMAH           $CEC3    ;inside the line at LCEC2 (LCEC2+1)
LCEC4:  .word [[SWMSGS&$1FFF]/2]+$E000 ;JMPL SWMSGS  MESSAGES ONLY
LCEC6:  .word [[SWHALT&$1FFF]/2]+$E000 ;JMPL SWHALT  HALT

;.SBTTL TABLE - POINTER TO PICTURES

PICLO:
LCEC8:  .word [[EXPL1&$1FFF]/2]+$A000 ;PITAB EXPL1,PTEXP1  EXPLOSION 1 SMALLEST
.alias PICHI            $CEC9    ;inside the line at LCEC8 (LCEC8+1)
LCECA:  .word [[EXPL2&$1FFF]/2]+$A000 ;PITAB EXPL2  EXPLOSION 2
LCECC:  .word [[EXPL3&$1FFF]/2]+$A000 ;PITAB EXPL3  EXPLOSION 3
LCECE:  .word [[EXPL4&$1FFF]/2]+$A000 ;PITAB EXPL4  EXPLOSION 4 LARGEST
LCED0:  .word [[DIARA2&$1FFF]/2]+$A000 ;PITAB DIARA2,PTCURS  DIAMON DOTS RADIUS 2
LCED2:  .word [[STAR1&$1FFF]/2]+$A000 ;PITAB STAR1,PTSTR1  STAR FIELD 1
LCED4:  .word [[STAR2&$1FFF]/2]+$A000 ;PITAB STAR2  STAR FIELD 2
LCED6:  .word [[STAR3&$1FFF]/2]+$A000 ;PITAB STAR3  STAR FIELD 3
LCED8:  .word [[STAR4&$1FFF]/2]+$A000 ;PITAB STAR4  STAR FIELD 4
LCEDA:  .word [[SPIRA1&$1FFF]/2]+$A000 ;PITAB SPIRA1,PTSPI1  SPIRALS
LCEDC:  .word [[SPIRA2&$1FFF]/2]+$A000 ;PITAB SPIRA2
LCEDE:  .word [[SPIRA3&$1FFF]/2]+$A000 ;PITAB SPIRA3
LCEE0:  .word [[SPIRA4&$1FFF]/2]+$A000 ;PITAB SPIRA4
LCEE2:  .word [[TANKR&$1FFF]/2]+$A000 ;PITAB TANKR,PTTANK  TANKER
LCEE4:  .word [[SPARK1&$1FFF]/2]+$A000 ;PITAB SPARK1,PTSPAR  SPARKLE
LCEE6:  .word [[SPARK2&$1FFF]/2]+$A000 ;PITAB SPARK2
LCEE8:  .word [[ESHOT1&$1FFF]/2]+$A000 ;PITAB ESHOT1,PTESHO  ENEMY SHOT
LCEEA:  .word [[ESHOT2&$1FFF]/2]+$A000 ;PITAB ESHOT2
LCEEC:  .word [[ESHOT3&$1FFF]/2]+$A000 ;PITAB ESHOT3
LCEEE:  .word [[ESHOT4&$1FFF]/2]+$A000 ;PITAB ESHOT4
;EXPLOSION DATABASE FOR PLAYER CHARGE COLLISION
LCEF0:  .word [[SPLAT6&$1FFF]/2]+$A000 ;PITAB SPLAT6,PTSPLA  SMALL SPLAT
LCEF2:  .word [[SPLAT5&$1FFF]/2]+$A000 ;PITAB SPLAT5
LCEF4:  .word [[SPLAT4&$1FFF]/2]+$A000 ;PITAB SPLAT4
LCEF6:  .word [[SPLAT3&$1FFF]/2]+$A000 ;PITAB SPLAT3
LCEF8:  .word [[SPLAT2&$1FFF]/2]+$A000 ;PITAB SPLAT2
LCEFA:  .word [[SPLAT1&$1FFF]/2]+$A000 ;PITAB SPLAT1  TARGET SPLAT
LCEFC:  .word [[SPLAT3&$1FFF]/2]+$A000 ;PITAB SPLAT3
LCEFE:  .word [[SPLAT5&$1FFF]/2]+$A000 ;PITAB SPLAT5
LCF00:  .word [[SPLAT6&$1FFF]/2]+$A000 ;PITAB SPLAT6  SMALL SPLAT
LCF02:  .word [[SHRAP&$1FFF]/2]+$A000 ;PITAB SHRAP  SHRAPNEL
LCF04:  .word [[SPLFU1&$1FFF]/2]+$A000 ;PITAB SPLFU1,PTSPLF  FUSE-PLAYER EXPLOSION
LCF06:  .word [[SPLFU2&$1FFF]/2]+$A000 ;PITAB SPLFU2
LCF08:  .word [[SPLFU3&$1FFF]/2]+$A000 ;PITAB SPLFU3
LCF0A:  .word [[SPLFU4&$1FFF]/2]+$A000 ;PITAB SPLFU4
LCF0C:  .word [[SPLFU5&$1FFF]/2]+$A000 ;PITAB SPLFU5
LCF0E:  .word [[SPLFU6&$1FFF]/2]+$A000 ;PITAB SPLFU6
LCF10:  .word [[SPLFU7&$1FFF]/2]+$A000 ;PITAB SPLFU7
LCF12:  .word [[TANKP&$1FFF]/2]+$A000 ;PITAB TANKP,PTTANP  PULSAR TANKER
LCF14:  .word [[TANKF&$1FFF]/2]+$A000 ;PITAB TANKF,PTTANF  FUSE TANKER
LCF16:  .word [[FUSE0&$1FFF]/2]+$A000 ;PITAB FUSE0,PTFUSE  FUSE
LCF18:  .word [[FUSE1&$1FFF]/2]+$A000 ;PITAB FUSE1
LCF1A:  .word [[FUSE2&$1FFF]/2]+$A000 ;PITAB FUSE2
LCF1C:  .word [[FUSE3&$1FFF]/2]+$A000 ;PITAB FUSE3
LCF1E:  .word [[FUSEX1&$1FFF]/2]+$A000 ;PITAB FUSEX1,PTFUSX  FUSE EXPLOSION
LCF20:  .word [[FUSEX2&$1FFF]/2]+$A000 ;PITAB FUSEX2
LCF22:  .word [[FUSEX3&$1FFF]/2]+$A000 ;PITAB FUSEX3

;ALVROM labels at its end address $CF24 (no data of their own):
.alias ANITAB           $CF24
.alias LSRTAB           $CF24
.alias ANDTAB           $CF24
.alias TOPSEQ           $CF24
.alias GUNSEQ           $CF24
.alias SAUSEQ           $CF24
.alias DSHSEQ           $CF24
.alias ESHSEQ           $CF24
.alias EXPSEQ           $CF24
.alias SHRSEQ           $CF24
.alias ASTSEQ           $CF24
.alias OBJPNT           $CF24
.alias OBJTBL           $CF24
.alias ROWBAR           $CF24
.alias JSRTAB           $CF24

;==============================================================================
; MODULE ALCOIN   COIN65 via ALCOIN.MAC
;   Coin routine: Atari's generic COIN65 (MOOLAH) included via ALCOIN.
;==============================================================================

;------------------------------------------------------------------------------
; MOOLAH - DETECT COIN
;   IF YOU USE CMZP=0, DEFINE EQUIVALENT MACROS
;   (GCM NOT NEEDED IF MODES=0)
;   GHM, GDM NOT NEEDED IF MULTS=0
;   ENTRY POINT
;   EXTERNAL REFERENCES
;   The coin routine assumes the presence of the following .GLOBL variables:
;   INSTRUCTIONS IN BRACKETS( "[" AND "]" ) ARE FOR ILLUSTRATION ONLY, AND ARE NOT
;   ACTUALLY ASSEMBLED.
;------------------------------------------------------------------------------
MOOLAH:
LCF24:  LDX  #$02                   ;(src: LDX I,OFFSET*<MECHS-1>)  X IS USED TO INDEX FROM RIGHT TO LEFT COIN MECH

S_DETCT:
LCF26:  .byte $AD, $08, $00         ;LDA S_COINA (forced absolute)  GET COIN SWITCHES
LCF29:  CPX  #$01                   ;(src: CPX I,OFFSET)  WHICH MECH ARE WE DOING
LCF2B:  BEQ  S_DETCT_11             ;MIDDLE (X=OFFSET) SHIFT TWICE
LCF2D:  BCS  S_DETCT_12             ;RIGHT (X=2*OFFSET) SHIFT ONCE
LCF2F:  LSR                         ;ELSE LEFT, SHIFT THRICE

S_DETCT_11:
LCF30:  LSR

S_DETCT_12:
LCF31:  LSR
LCF32:  LDA  S_CNSTT,X
LCF34:  AND  #$1F                   ;SHARED INST. SEE BELOW IN BRACKETS []
LCF36:  BCS  S_DETCT_5              ;BRANCH IF INPUT HIGH (COIN ABSENT)
;[AND I,31.] ISOLATE COIN-ON DOWN-COUNTER, RESET COIN-OFF UP-CTR.
LCF38:  BEQ  S_DETCT_1              ;STICK AT 0 (TERMINAL COUNT)
LCF3A:  CMP  #$1B                   ;IN FIRST FIVE SAMPLES?
LCF3C:  BCS  S_DETCT_10             ;YES, RUN FAST
LCF3E:  TAY                         ;ELSE SAVE STATUS
LCF3F:  LDA  S_INTCT                ;CHECK INTERUPT CTR
LCF41:  AND  #$07                   ;ARE D0-D2 ALL ONES?
LCF43:  CMP  #$07                   ;SET CARRY IF SO
LCF45:  TYA                         ;STATUS BACK INTO ACC
LCF46:  BCC  S_DETCT_1              ;SKIP IF NOT ALL ONES

S_DETCT_10:
LCF48:  SBC  #$01                   ;CARRY SET

S_DETCT_1:
LCF4A:  STA  S_CNSTT,X              ;SAVE UPDATED STATUS
LCF4C:  .byte $AD, $08, $00         ;LDA S_LAM (forced absolute)  CHECK SLAM SWITCH  [CS] If the SLAM switch is hit, then
LCF4F:  AND  #S_LMBIT               ;[CS] skip a little bit.
LCF51:  BNE  S_DETCT_2              ;BRANCH IF BIT HI (SWITCH OFF)
LCF53:  LDA  #$F0                   ;(src: LDA I,PRST*8)  ELSE SET PRE-COIN SLAM TIMER
LCF55:  STA  S_LMTIM                ;DECR. 8 TIMES/FRAME=PRST FRAMES

S_DETCT_2:
LCF57:  LDA  S_LMTIM                ;CHECK PRE-COIN SLAM TIMER
LCF59:  BEQ  S_DETCT_3              ;O.K.
LCF5B:  DEC  S_LMTIM                ;ELSE RUN TIMER
LCF5D:  LDA  #$00
LCF5F:  STA  S_CNSTT,X              ;CLEAR COIN STATUS
LCF61:  STA  S_PSTSL,X              ;CLEAR POST-COIN SLAM-TIMER

S_DETCT_3:
LCF63:  CLC                         ;DEFAULT "NO COIN DETECTED"
LCF64:  LDA  S_PSTSL,X              ;CHECK POST-COIN SLAM-TIMER
LCF66:  BEQ  S_DETCT_8              ;EMPTY, PROCEED
LCF68:  DEC  S_PSTSL,X              ;RUN TIMER
LCF6A:  BNE  S_DETCT_8              ;NOT DONE, PROCEED
LCF6C:  SEC                         ;WHEN IT BECOMES ZERO, INDICATE A COIN
LCF6D:  BCS  S_DETCT_8              ;(ALWAYS)

S_DETCT_5:       ;[AND I,31.] GET COIN-ON DOWN-CTR (ACTUALLY DONE BEFORE)
LCF6F:  CMP  #$1B                   ;IS COIN VALID YET (ON FOR >4 SAMPLES)
LCF71:  BCS  S_DETCT_6              ;NO, RESET IT
LCF73:  LDA  S_CNSTT,X              ;GET STATUS AGAIN
LCF75:  ADC  #$20                   ;BUMP COIN-OFF UP-CTR.
LCF77:  BCC  S_DETCT_1              ;IF IT DIDN'T WRAP, JUST STORE STATUS
LCF79:  BEQ  S_DETCT_6              ;IT WRAPPED BUT COIN WAS ON TOO LONG, JUST RESET
LCF7B:  CLC                         ;SET "VALIDITY" AGAIN

S_DETCT_6:
LCF7C:  LDA  #$1F                   ;RESET DOWN-COUNTER
LCF7E:  BCS  S_DETCT_1              ;BRANCH IF COIN TOO LONG OR TOO SHORT
LCF80:  STA  S_CNSTT,X              ;SAVE RESET STATUS
;[CLC] DEFAULT TO "NO COIN" (CARRY IS ALREADY CLEAR)
LCF82:  LDA  S_PSTSL,X              ;CHECK HOWIES ASSUMPTION
LCF84:  BEQ  S_DETCT_7              ;BRANCH IF $PSTSL VACANT
LCF86:  SEC                         ;ELSE GIVE CREDIT A LITTLE EARLY

S_DETCT_7:
LCF87:  LDA  #$78                   ;(src: LDA I,POST*4)  /(4 COUNTS/FRAME)="POST" FRAMES
LCF89:  STA  S_PSTSL,X              ;DELAY ACCEPTANCE FOR POST/60 SEC.

S_DETCT_8:       ;CARRY=1 IF COIN FALLS OUT
LCF8B:  BCC  S_DETCT_9

;.SBTTL MECH-MULTIPLIERS
;FALL THROUGH TO "MECH-MULTIPLIERS"
LCF8D:  LDA  #$00                   ;START WITH 0 (TO ADD 1)
LCF8F:  CPX  #$01                   ;(src: CPX I,OFFSET)  CHECK WHICH MECH
LCF91:  BCC  S_DETCT_85             ;IF LEFT, ALWAYS ADD 1
LCF93:  BEQ  S_DETCT_83             ;IF CENTER, CHECK HALF-MUL
LCF95:  LDA  S_CMODE                ;GDM  GET DOLLAR MUL
LCF97:  AND  #$0C
LCF99:  LSR
LCF9A:  LSR
LCF9B:  BEQ  S_DETCT_85             ;00-ADD 1
LCF9D:  ADC  #$02                   ;ELSE MAP 1,2,3 TO 3,4,5
LCF9F:  BNE  S_DETCT_85             ;(ALWAYS)

S_DETCT_83:
LCFA1:  LDA  S_CMODE                ;GHM
LCFA3:  AND  #$10
LCFA5:  BEQ  S_DETCT_85
LCFA7:  LDA  #$01

S_DETCT_85:
LCFA9:  SEC
LCFAA:  PHA
LCFAB:  ADC  S_BCCNT                ;UPDATE BONUS-ADDER COUNTER
LCFAD:  STA  S_BCCNT
LCFAF:  PLA
LCFB0:  SEC
LCFB1:  ADC  S_CNCT
LCFB3:  STA  S_CNCT
LCFB5:  INC  S_CCTIM,X              ;"QUEUE" PULSE FOR E.M. COUNTER

S_DETCT_9:
LCFB7:  DEX                         ;.REPT OFFSET
LCFB8:  BMI  S_BONUS
LCFBA:  JMP  S_DETCT

;------------------------------------------------------------------------------
; S_BONUS - BONUS-ADDER
;   FALL THRU TO BONUS-ADDER
;   (also: S_DETCT_120)
;------------------------------------------------------------------------------
S_BONUS:
S_DETCT_120:
LCFBD:  LDA  S_CMODE                ;GBAM  GET BONUS ADDER MODE
LCFBF:  LSR
LCFC0:  LSR
LCFC1:  LSR
LCFC2:  LSR
LCFC3:  LSR
LCFC4:  TAY
LCFC5:  LDA  S_BCCNT
LCFC7:  SEC
LCFC8:  SBC  S_MODLO,Y              ;SEE IF ENOUGH UNIT-COINS HAVE ACCUMULATED
LCFCB:  BMI  S_EXTB                 ;BRANCH IF NOT
LCFCD:  STA  S_BCCNT                ;ELSE UPDATE BONUS-ADDER AND...
LCFCF:  INC  S_BC                   ;GIVE ONE OR TWO BONUS UNIT-COINS
LCFD1:  CPY  #$03
LCFD3:  BNE  S_EXTB
LCFD5:  INC  S_BC                   ;MODE 3 YIELDS 2 BONUS COINS FOR 4 INSERTED
LCFD7:  BNE  S_EXTB                 ;BRA

S_MODLO:         ;THIS IS THE NUMBER OF UNIT-COINS REQUIRED TO RECEIVE BONUS
LCFD9:  .byte $7F, $02, $04, $04, $05, $03, $7F, $7F ;7F IS USED TO GENERATE ZERO BONUS COINS

;------------------------------------------------------------------------------
; S_EXTB - COINS TO CREDITS
;   FALL THROUGH TO CONVERT COINS TO CREDITS
;   (also: S_CNVRT)
;------------------------------------------------------------------------------
S_EXTB:
S_CNVRT:
LCFE1:  LDA  S_CMODE                ;GCM  GET COIN MODE IN 0,1
LCFE3:  AND  #$03
LCFE5:  TAY                         ;SAVE IT
LCFE6:  BEQ  S_CNVRT_2              ;IF FREE PLAY CMODE=0, DO NOTHING
LCFE8:  LSR                         ;ELSE FORM PRICE (0,1,1,2)
LCFE9:  ADC  #$00
LCFEB:  EOR  #$FF
LCFED:  SEC
LCFEE:  ADC  S_CNCT                 ;ACC <- COINCT-PRICE
LCFF0:  BCS  S_CNVRT_33             ;BRANCH IF NO BORROW
;[CLC]
LCFF2:  ADC  S_BC                   ;ADD IN BONUS COINS-SEE IF THEY HELP
LCFF4:  BMI  S_EXT                  ;BRANCH IF COINCT+BONUS COINS <PRICE
LCFF6:  STA  S_BC                   ;ELSE ACC=UNUSED BONUS COINS
LCFF8:  LDA  #$00                   ;ACC=NEW $CNCT

S_CNVRT_33:      ;GENERATE CREDITS: 1 OR 2
LCFFA:  CPY  #$02                   ;Y=COIN MODE-COIN MODE 2 OR 3?
LCFFC:  BCS  S_CNVRT_1              ;BRANCH IF MODE 2 OR 3-GIVE 1 CREDIT
LCFFE:  INC  S_S_CRDT               ;ELSE GIVE 2 FOR MODE 1  [CS] Increase the number of game

S_CNVRT_1:
;[CS] Start of ROM 136002.121 at $D000.
LD000:  INC  S_S_CRDT               ;[CS] credits by two.

S_CNVRT_2:
LD002:  STA  S_CNCT                 ;UPDAT COINCT

;.SBTTL ELECTRO-MECH. CTRS

S_EXT:
;FALL THROUGH TO HANDLE E.M. COUNTERS
LD004:  LDA  S_INTCT                ;GET $INTCT
LD006:  LSR                         ;USE LSB FOR PULSE=4 FRAMES
LD007:  BCS  S_EXT_99
LD009:  LDY  #$00                   ;START WITH FLAG OF 0
LD00B:  LDX  #$02                   ;(src: LDX I,OFFSET*<EMCTRS-1>)

S_EXT_1:
LD00D:  LDA  S_CCTIM,X              ;CHECK TIMER(X)
LD00F:  BEQ  S_EXT_3                ;NEITHER RUNNING NOR PENDING
LD011:  CMP  #$10                   ;IS IT RUNNING
LD013:  BCC  S_EXT_3                ;NO, SKIP
LD015:  ADC  #$EF                   ;ELSE DEC 4 MSB
LD017:  INY                         ;SET ON FLAG

S_EXT_2:
LD018:  STA  S_CCTIM,X

S_EXT_3:
LD01A:  DEX                         ;.REPT OFFSET
LD01B:  BPL  S_EXT_1
LD01D:  TYA                         ;CHECK "ON" FLAG
LD01E:  BNE  S_EXT_99               ;SKIP IF ANY ON
;IF NONE OF THE COUNTERS ARE CURRENTLY ON, WE CHECK TO SEE IF ANY CAN BE
;STARTED.
LD020:  LDX  #$02                   ;(src: LDX I,OFFSET*<EMCTRS-1>)

S_EXT_4:
LD022:  LDA  S_CCTIM,X              ;NEED WE START THIS ONE
LD024:  BEQ  S_EXT_5                ;NO, NO COUNTS PENDING
LD026:  CLC
LD027:  ADC  #$EF                   ;SET 4 MSB, DEC 4 LSB
LD029:  STA  S_CCTIM,X              ;START TIMER
LD02B:  BMI  S_EXT_99               ;EXIT, SO WE DON'T START MORE

S_EXT_5:
LD02D:  DEX                         ;.REPT OFFSET
LD02E:  BPL  S_EXT_4

S_EXT_99:
LD030:  RTS

;==============================================================================
; MODULE ALLANG   ALLANG.MAC
;   Messages in English, French, German and Spanish: pointer tables,
;   colour/scale/Y table, message text (vector character codes), language
;   selection.
;==============================================================================

;.SBTTL MESSAGES: COLOR, SCALE, Y POSITION, LANGUAGE PTRS

ENGMSG:
;  Message tables: per language: .WORD pointer per message (index = message number/2); literal = signed X byte, then ASCVG codes (VGMSGA index*2, bit7 = last). MSGLBS: byte (colour<<4 | scale), signed Y byte
LD031:  .word EGAMOV                ;MESS GAMOV,GREEN,1,56  GAME OVER
LD033:  .word EPLAYR                ;MESS PLAYR,WHITE,0,1A  PLAYER (BIG)
LD035:  .word EPLYR2                ;MESS PLYR2,WHITE,1,20  PLAYER (NORMAL)
LD037:  .word EPRESS                ;MESS PRESS,RED,1,56  PRESS START
LD039:  .word EPLAY                 ;MESS PLAY,WHITE,1,38  PLAY
LD03B:  .word EENTER                ;MESS ENTER,RED,1,0B0  ENTER
LD03D:  .word EPRMOV                ;MESS PRMOV,TURQOI,1,0  SPIN
LD03F:  .word EPRFIR                ;MESS PRFIR,YELLOW,1,-10.  PRESS FIRE
LD041:  .word EHIGHS                ;MESS HIGHS,RED,0,38  HIGH SCORE
LD043:  .word ERANK                 ;MESS RANK,RED,1,-50.  RANK
LD045:  .word ERATE                 ;MESS RATE,GREEN,1,10.  RATE YOURSELF
LD047:  .word ENOVIC                ;MESS NOVIC,RED,1,-30.  NOVICE
LD049:  .word EEXPER                ;MESS EXPER,RED,1,-30.  EXPERT
LD04B:  .word EBONUS                ;MESS BONUS,GREEN,1,-70.  BONUS
LD04D:  .word ETIME                 ;MESS TIME,GREEN,1,98  TIME
LD04F:  .word ELEVEL                ;MESS LEVEL,GREEN,1,-40.  LEVEL
LD051:  .word EHOLE                 ;MESS HOLE,GREEN,1,-55.  HOLE
LD053:  .word EINSER                ;MESS INSER,RED,1,56  INSERT COINS
LD055:  .word ECMODE                ;MESS CMODE,GREEN,1,80  FREE PLAY
LD057:  .word ECMOD1                ;MESS CMOD1,GREEN,1,80  1 COIN 2 PLAYS
LD059:  .word ECMOD2                ;MESS CMOD2,GREEN,1,80  1 COIN 1 PLAY
LD05B:  .word ECMOD3                ;MESS CMOD3,GREEN,1,80  2 COINS 1 PLAY
LD05D:  .word EATARI                ;MESS ATARI,BLULET,1,92  MCMLXXX ATARI
LD05F:  .word ECREDI                ;MESS CREDI,GREEN,1,80  CREDITS
LD061:  .word EBONPT                ;MESS BONPT,RED,1,0B0  BONUS PTS
LD063:  .word E2GAME                ;MESS 2GAME,GREEN,1,89  2 GAME MINIMUM
LD065:  .word EBOLIF                ;MESS BOLIF,TURQOI,1,89  BONUS EVERY
LD067:  .word ESPIKE                ;MESS SPIKE,WHITE,0,0  AVOID SPIKES
LD069:  .word EAPROA                ;MESS APROA,BLULET,1,5A  APPROACH
LD06B:  .word ESUPZA                ;MESS SUPZA,BLULET,1,0A0  NEW SUPER

FREMSG:
LD06D:  .word FGAMOV                ;MESS GAMOV,GREEN,1,56  GAME OVER
LD06F:  .word FPLAYR                ;MESS PLAYR,WHITE,0,1A  PLAYER (BIG)
LD071:  .word FPLYR2                ;MESS PLYR2,WHITE,1,20  PLAYER (NORMAL)
LD073:  .word FPRESS                ;MESS PRESS,RED,1,56  PRESS START
LD075:  .word FPLAY                 ;MESS PLAY,WHITE,1,38  PLAY
LD077:  .word FENTER                ;MESS ENTER,RED,1,0B0  ENTER
LD079:  .word FPRMOV                ;MESS PRMOV,TURQOI,1,0  SPIN
LD07B:  .word FPRFIR                ;MESS PRFIR,YELLOW,1,-10.  PRESS FIRE
LD07D:  .word FHIGHS                ;MESS HIGHS,RED,0,38  HIGH SCORE
LD07F:  .word FRANK                 ;MESS RANK,RED,1,-50.  RANK
LD081:  .word FRATE                 ;MESS RATE,GREEN,1,10.  RATE YOURSELF
LD083:  .word FNOVIC                ;MESS NOVIC,RED,1,-30.  NOVICE
LD085:  .word FEXPER                ;MESS EXPER,RED,1,-30.  EXPERT
LD087:  .word FBONUS                ;MESS BONUS,GREEN,1,-70.  BONUS
LD089:  .word FTIME                 ;MESS TIME,GREEN,1,98  TIME
LD08B:  .word FLEVEL                ;MESS LEVEL,GREEN,1,-40.  LEVEL
LD08D:  .word FHOLE                 ;MESS HOLE,GREEN,1,-55.  HOLE
LD08F:  .word FINSER                ;MESS INSER,RED,1,56  INSERT COINS
LD091:  .word FCMODE                ;MESS CMODE,GREEN,1,80  FREE PLAY
LD093:  .word FCMOD1                ;MESS CMOD1,GREEN,1,80  1 COIN 2 PLAYS
LD095:  .word FCMOD2                ;MESS CMOD2,GREEN,1,80  1 COIN 1 PLAY
LD097:  .word FCMOD3                ;MESS CMOD3,GREEN,1,80  2 COINS 1 PLAY
LD099:  .word FATARI                ;MESS ATARI,BLULET,1,92  MCMLXXX ATARI
LD09B:  .word FCREDI                ;MESS CREDI,GREEN,1,80  CREDITS
LD09D:  .word FBONPT                ;MESS BONPT,RED,1,0B0  BONUS PTS
LD09F:  .word F2GAME                ;MESS 2GAME,GREEN,1,89  2 GAME MINIMUM
LD0A1:  .word FBOLIF                ;MESS BOLIF,TURQOI,1,89  BONUS EVERY
LD0A3:  .word FSPIKE                ;MESS SPIKE,WHITE,0,0  AVOID SPIKES
LD0A5:  .word FAPROA                ;MESS APROA,BLULET,1,5A  APPROACH
LD0A7:  .word FSUPZA                ;MESS SUPZA,BLULET,1,0A0  NEW SUPER

GERMSG:
LD0A9:  .word GGAMOV                ;MESS GAMOV,GREEN,1,56  GAME OVER
LD0AB:  .word GPLAYR                ;MESS PLAYR,WHITE,0,1A  PLAYER (BIG)
LD0AD:  .word GPLYR2                ;MESS PLYR2,WHITE,1,20  PLAYER (NORMAL)
LD0AF:  .word GPRESS                ;MESS PRESS,RED,1,56  PRESS START
LD0B1:  .word GPLAY                 ;MESS PLAY,WHITE,1,38  PLAY
LD0B3:  .word GENTER                ;MESS ENTER,RED,1,0B0  ENTER
LD0B5:  .word GPRMOV                ;MESS PRMOV,TURQOI,1,0  SPIN
LD0B7:  .word GPRFIR                ;MESS PRFIR,YELLOW,1,-10.  PRESS FIRE
LD0B9:  .word GHIGHS                ;MESS HIGHS,RED,0,38  HIGH SCORE
LD0BB:  .word GRANK                 ;MESS RANK,RED,1,-50.  RANK
LD0BD:  .word GRATE                 ;MESS RATE,GREEN,1,10.  RATE YOURSELF
LD0BF:  .word GNOVIC                ;MESS NOVIC,RED,1,-30.  NOVICE
LD0C1:  .word GEXPER                ;MESS EXPER,RED,1,-30.  EXPERT
LD0C3:  .word GBONUS                ;MESS BONUS,GREEN,1,-70.  BONUS
LD0C5:  .word GTIME                 ;MESS TIME,GREEN,1,98  TIME
LD0C7:  .word GLEVEL                ;MESS LEVEL,GREEN,1,-40.  LEVEL
LD0C9:  .word GHOLE                 ;MESS HOLE,GREEN,1,-55.  HOLE
LD0CB:  .word GINSER                ;MESS INSER,RED,1,56  INSERT COINS
LD0CD:  .word GCMODE                ;MESS CMODE,GREEN,1,80  FREE PLAY
LD0CF:  .word GCMOD1                ;MESS CMOD1,GREEN,1,80  1 COIN 2 PLAYS
LD0D1:  .word GCMOD2                ;MESS CMOD2,GREEN,1,80  1 COIN 1 PLAY
LD0D3:  .word GCMOD3                ;MESS CMOD3,GREEN,1,80  2 COINS 1 PLAY
LD0D5:  .word GATARI                ;MESS ATARI,BLULET,1,92  MCMLXXX ATARI
LD0D7:  .word GCREDI                ;MESS CREDI,GREEN,1,80  CREDITS
LD0D9:  .word GBONPT                ;MESS BONPT,RED,1,0B0  BONUS PTS
LD0DB:  .word G2GAME                ;MESS 2GAME,GREEN,1,89  2 GAME MINIMUM
LD0DD:  .word GBOLIF                ;MESS BOLIF,TURQOI,1,89  BONUS EVERY
LD0DF:  .word GSPIKE                ;MESS SPIKE,WHITE,0,0  AVOID SPIKES
LD0E1:  .word GAPROA                ;MESS APROA,BLULET,1,5A  APPROACH
LD0E3:  .word GSUPZA                ;MESS SUPZA,BLULET,1,0A0  NEW SUPER

SPAMSG:
LD0E5:  .word SGAMOV                ;MESS GAMOV,GREEN,1,56  GAME OVER
LD0E7:  .word SPLAYR                ;MESS PLAYR,WHITE,0,1A  PLAYER (BIG)
LD0E9:  .word SPLYR2                ;MESS PLYR2,WHITE,1,20  PLAYER (NORMAL)
LD0EB:  .word SPRESS                ;MESS PRESS,RED,1,56  PRESS START
LD0ED:  .word SPLAY                 ;MESS PLAY,WHITE,1,38  PLAY
LD0EF:  .word SENTER                ;MESS ENTER,RED,1,0B0  ENTER
LD0F1:  .word SPRMOV                ;MESS PRMOV,TURQOI,1,0  SPIN
LD0F3:  .word SPRFIR                ;MESS PRFIR,YELLOW,1,-10.  PRESS FIRE
LD0F5:  .word SHIGHS                ;MESS HIGHS,RED,0,38  HIGH SCORE
LD0F7:  .word SRANK                 ;MESS RANK,RED,1,-50.  RANK
LD0F9:  .word SRATE                 ;MESS RATE,GREEN,1,10.  RATE YOURSELF
LD0FB:  .word SNOVIC                ;MESS NOVIC,RED,1,-30.  NOVICE
LD0FD:  .word SEXPER                ;MESS EXPER,RED,1,-30.  EXPERT
LD0FF:  .word SBONUS                ;MESS BONUS,GREEN,1,-70.  BONUS
LD101:  .word STIME                 ;MESS TIME,GREEN,1,98  TIME
LD103:  .word SLEVEL                ;MESS LEVEL,GREEN,1,-40.  LEVEL
LD105:  .word SHOLE                 ;MESS HOLE,GREEN,1,-55.  HOLE
LD107:  .word SINSER                ;MESS INSER,RED,1,56  INSERT COINS
LD109:  .word SCMODE                ;MESS CMODE,GREEN,1,80  FREE PLAY
LD10B:  .word SCMOD1                ;MESS CMOD1,GREEN,1,80  1 COIN 2 PLAYS
LD10D:  .word SCMOD2                ;MESS CMOD2,GREEN,1,80  1 COIN 1 PLAY
LD10F:  .word SCMOD3                ;MESS CMOD3,GREEN,1,80  2 COINS 1 PLAY
LD111:  .word SATARI                ;MESS ATARI,BLULET,1,92  MCMLXXX ATARI
LD113:  .word SCREDI                ;MESS CREDI,GREEN,1,80  CREDITS
LD115:  .word SBONPT                ;MESS BONPT,RED,1,0B0  BONUS PTS
LD117:  .word S2GAME                ;MESS 2GAME,GREEN,1,89  2 GAME MINIMUM
LD119:  .word SBOLIF                ;MESS BOLIF,TURQOI,1,89  BONUS EVERY
LD11B:  .word SSPIKE                ;MESS SPIKE,WHITE,0,0  AVOID SPIKES
LD11D:  .word SAPROA                ;MESS APROA,BLULET,1,5A  APPROACH
LD11F:  .word SSUPZA                ;MESS SUPZA,BLULET,1,0A0  NEW SUPER

MSGLBS:
LD121:  .byte [GREEN*$10]|1, $56    ;MESS GAMOV,GREEN,1,56  GAME OVER
LD123:  .byte [WHITE*$10]|0, $1A    ;MESS PLAYR,WHITE,0,1A  PLAYER (BIG)
LD125:  .byte [WHITE*$10]|1, $20    ;MESS PLYR2,WHITE,1,20  PLAYER (NORMAL)
LD127:  .byte [RED*$10]|1, $56      ;MESS PRESS,RED,1,56  PRESS START
LD129:  .byte [WHITE*$10]|1, $38    ;MESS PLAY,WHITE,1,38  PLAY
LD12B:  .byte [RED*$10]|1, $B0      ;MESS ENTER,RED,1,0B0  ENTER
LD12D:  .byte [TURQOI*$10]|1, $00   ;MESS PRMOV,TURQOI,1,0  SPIN
LD12F:  .byte [YELLOW*$10]|1, $F6   ;MESS PRFIR,YELLOW,1,-10.  PRESS FIRE
LD131:  .byte [RED*$10]|0, $38      ;MESS HIGHS,RED,0,38  HIGH SCORE
LD133:  .byte [RED*$10]|1, $CE      ;MESS RANK,RED,1,-50.  RANK
LD135:  .byte [GREEN*$10]|1, $0A    ;MESS RATE,GREEN,1,10.  RATE YOURSELF
LD137:  .byte [RED*$10]|1, $E2      ;MESS NOVIC,RED,1,-30.  NOVICE
LD139:  .byte [RED*$10]|1, $E2      ;MESS EXPER,RED,1,-30.  EXPERT
LD13B:  .byte [GREEN*$10]|1, $BA    ;MESS BONUS,GREEN,1,-70.  BONUS
LD13D:  .byte [GREEN*$10]|1, $98    ;MESS TIME,GREEN,1,98  TIME
LD13F:  .byte [GREEN*$10]|1, $D8    ;MESS LEVEL,GREEN,1,-40.  LEVEL
LD141:  .byte [GREEN*$10]|1, $C9    ;MESS HOLE,GREEN,1,-55.  HOLE
LD143:  .byte [RED*$10]|1, $56      ;MESS INSER,RED,1,56  INSERT COINS
LD145:  .byte [GREEN*$10]|1, $80    ;MESS CMODE,GREEN,1,80  FREE PLAY
LD147:  .byte [GREEN*$10]|1, $80    ;MESS CMOD1,GREEN,1,80  1 COIN 2 PLAYS
LD149:  .byte [GREEN*$10]|1, $80    ;MESS CMOD2,GREEN,1,80  1 COIN 1 PLAY
LD14B:  .byte [GREEN*$10]|1, $80    ;MESS CMOD3,GREEN,1,80  2 COINS 1 PLAY
LD14D:  .byte [BLULET*$10]|1, $92   ;MESS ATARI,BLULET,1,92  MCMLXXX ATARI
LD14F:  .byte [GREEN*$10]|1, $80    ;MESS CREDI,GREEN,1,80  CREDITS
LD151:  .byte [RED*$10]|1, $B0      ;MESS BONPT,RED,1,0B0  BONUS PTS
LD153:  .byte [GREEN*$10]|1, $89    ;MESS 2GAME,GREEN,1,89  2 GAME MINIMUM
LD155:  .byte [TURQOI*$10]|1, $89   ;MESS BOLIF,TURQOI,1,89  BONUS EVERY
LD157:  .byte [WHITE*$10]|0, $00    ;MESS SPIKE,WHITE,0,0  AVOID SPIKES
LD159:  .byte [BLULET*$10]|1, $5A   ;MESS APROA,BLULET,1,5A  APPROACH
LD15B:  .byte [BLULET*$10]|1, $A0   ;MESS SUPZA,BLULET,1,0A0  NEW SUPER

;.SBTTL LITERALS

EGAMOV:
;  English message MGAMOV (GAME OVER): "GAME OVER"  x=-27
LD15D:  .byte $E5, $22, $16, $2E, $1E, $00, $32, $40 ;ASCVH <GAME OVER>
LD165:  .byte $1E, $B8

FGAMOV:
;  French message MGAMOV (GAME OVER): "FIN DE PARTIE"  x=-39
LD167:  .byte $D9, $20, $26, $30, $00, $1C, $1E, $00 ;ASCVH <FIN DE PARTIE>
LD16F:  .byte $34, $16, $38, $3C, $26, $9E

GGAMOV:
;  German message MGAMOV (GAME OVER): "SPIELENDE"  x=-27
LD175:  .byte $E5, $3A, $34, $26, $1E, $2C, $1E, $30 ;ASCVH <SPIELENDE>
LD17D:  .byte $1C, $9E

SGAMOV:
;  Spanish message MGAMOV (GAME OVER): "JUEGO TERMINADO"  x=-45
LD17F:  .byte $D3, $28, $3E, $1E, $22, $32, $00, $3C ;ASCVH <JUEGO TERMINADO>
LD187:  .byte $1E, $38, $2E, $26, $30, $16, $1C, $B2

EPLYR2:
EPLAYR:
;  English message MPLAYR (PLAYER (BIG)): "PLAYER "  x=-51
;  English message MPLYR2 (PLAYER (NORMAL)): "PLAYER "  x=-51
LD18F:  .byte $CD, $34, $2C, $16, $46, $1E, $38, $80 ;ASCVH 0CD,<PLAYER >

FPLYR2:
FPLAYR:
;  French message MPLAYR (PLAYER (BIG)): "JOUEUR "  x=-58
;  French message MPLYR2 (PLAYER (NORMAL)): "JOUEUR "  x=-58
LD197:  .byte $C6, $28, $32, $3E, $1E, $3E, $38, $80 ;ASCVH 0C6,<JOUEUR >

GPLYR2:
GPLAYR:
;  German message MPLAYR (PLAYER (BIG)): "SPIELER "  x=-58
;  German message MPLYR2 (PLAYER (NORMAL)): "SPIELER "  x=-58
LD19F:  .byte $C6, $3A, $34, $26, $1E, $2C, $1E, $38 ;ASCVH 0C6,<SPIELER >
LD1A7:  .byte $80

SPLYR2:
SPLAYR:
;  Spanish message MPLAYR (PLAYER (BIG)): "JUGADOR "  x=-58
;  Spanish message MPLYR2 (PLAYER (NORMAL)): "JUGADOR "  x=-58
LD1A8:  .byte $C6, $28, $3E, $22, $16, $1C, $32, $38 ;ASCVH 0C6,<JUGADOR >
LD1B0:  .byte $80

EPRESS:
;  English message MPRESS (PRESS START): "PRESS START"  x=-33
LD1B1:  .byte $DF, $34, $38, $1E, $3A, $3A, $00, $3A ;ASCVH <PRESS START>
LD1B9:  .byte $3C, $16, $38, $BC

FPRESS:
;  French message MPRESS (PRESS START): "APPUYEZ SUR START"  x=-51
LD1BD:  .byte $CD, $16, $34, $34, $3E, $46, $1E, $48 ;ASCVH <APPUYEZ SUR START>
LD1C5:  .byte $00, $3A, $3E, $38, $00, $3A, $3C, $16
LD1CD:  .byte $38, $BC

GPRESS:
;  German message MPRESS (PRESS START): "START DRUECKEN"  x=-42
LD1CF:  .byte $D6, $3A, $3C, $16, $38, $3C, $00, $1C ;ASCVH <START DRUECKEN>
LD1D7:  .byte $38, $3E, $1E, $1A, $2A, $1E, $B0

SPRESS:
;  Spanish message MPRESS (PRESS START): "PULSAR START"  x=-36
LD1DE:  .byte $DC, $34, $3E, $2C, $3A, $16, $38, $00 ;ASCVH <PULSAR START>
LD1E6:  .byte $3A, $3C, $16, $38, $BC

EPLAY:
;  English message MPLAY (PLAY): "PLAY"  x=-12
LD1EB:  .byte $F4, $34, $2C, $16, $C6 ;ASCVH <PLAY>

FPLAY:
;  French message MPLAY (PLAY): "JOUEZ"  x=-15
LD1F0:  .byte $F1, $28, $32, $3E, $1E, $C8 ;ASCVH <JOUEZ>

GPLAY:
;  German message MPLAY (PLAY): "SPIEL"  x=-15
LD1F6:  .byte $F1, $3A, $34, $26, $1E, $AC ;ASCVH <SPIEL>

SPLAY:
;  Spanish message MPLAY (PLAY): "JUEGUE"  x=-18
LD1FC:  .byte $EE, $28, $3E, $1E, $22, $3E, $9E ;ASCVH <JUEGUE>

EENTER:
;  English message MENTER (ENTER): "ENTER YOUR INITIALS"  x=-57
LD203:  .byte $C7, $1E, $30, $3C, $1E, $38, $00, $46 ;ASCVH <ENTER YOUR INITIALS>
LD20B:  .byte $32, $3E, $38, $00, $26, $30, $26, $3C
LD213:  .byte $26, $16, $2C, $BA

FENTER:
;  French message MENTER (ENTER): "SVP ENTREZ VOS INITIALES"  x=-72
LD217:  .byte $B8, $3A, $40, $34, $00, $1E, $30, $3C ;ASCVH <SVP ENTREZ VOS INITIALES>
LD21F:  .byte $38, $1E, $48, $00, $40, $32, $3A, $00
LD227:  .byte $26, $30, $26, $3C, $26, $16, $2C, $1E
LD22F:  .byte $BA

GENTER:
;  German message MENTER (ENTER): "GEBEN SIE IHRE INITIALEN EIN"  x=-84
LD230:  .byte $AC, $22, $1E, $18, $1E, $30, $00, $3A ;ASCVH <GEBEN SIE IHRE INITIALEN EIN>
LD238:  .byte $26, $1E, $00, $26, $24, $38, $1E, $00
LD240:  .byte $26, $30, $26, $3C, $26, $16, $2C, $1E
LD248:  .byte $30, $00, $1E, $26, $B0

SENTER:
;  Spanish message MENTER (ENTER): "ENTRE SUS INICIALES"  x=-57
LD24D:  .byte $C7, $1E, $30, $3C, $38, $1E, $00, $3A ;ASCVH <ENTRE SUS INICIALES>
LD255:  .byte $3E, $3A, $00, $26, $30, $26, $1A, $26
LD25D:  .byte $16, $2C, $1E, $BA

EPRMOV:
;  English message MPRMOV (SPIN): "SPIN KNOB TO CHANGE"  x=-57
LD261:  .byte $C7, $3A, $34, $26, $30, $00, $2A, $30 ;ASCVH <SPIN KNOB TO CHANGE>
LD269:  .byte $32, $18, $00, $3C, $32, $00, $1A, $24
LD271:  .byte $16, $30, $22, $9E

FPRMOV:
;  French message MPRMOV (SPIN): "TOURNEZ LE BOUTON POUR CHANGER"  x=-90
LD275:  .byte $A6, $3C, $32, $3E, $38, $30, $1E, $48 ;ASCVH <TOURNEZ LE BOUTON POUR CHANGER>
LD27D:  .byte $00, $2C, $1E, $00, $18, $32, $3E, $3C
LD285:  .byte $32, $30, $00, $34, $32, $3E, $38, $00
LD28D:  .byte $1A, $24, $16, $30, $22, $1E, $B8

GPRMOV:
;  German message MPRMOV (SPIN): "KNOPF DREHEN ZUM WECHSELN"  x=-75
LD294:  .byte $B5, $2A, $30, $32, $34, $20, $00, $1C ;ASCVH <KNOPF DREHEN ZUM WECHSELN>
LD29C:  .byte $38, $1E, $24, $1E, $30, $00, $48, $3E
LD2A4:  .byte $2E, $00, $42, $1E, $1A, $24, $3A, $1E
LD2AC:  .byte $2C, $B0

SPRMOV:
;  Spanish message MPRMOV (SPIN): "GIRE LA PERILLA PARA CAMBIAR"  x=-84
LD2AE:  .byte $AC, $22, $26, $38, $1E, $00, $2C, $16 ;ASCVH <GIRE LA PERILLA PARA CAMBIAR>
LD2B6:  .byte $00, $34, $1E, $38, $26, $2C, $2C, $16
LD2BE:  .byte $00, $34, $16, $38, $16, $00, $1A, $16
LD2C6:  .byte $2E, $18, $26, $16, $B8

EPRFIR:
;  English message MPRFIR (PRESS FIRE): "PRESS FIRE TO SELECT"  x=-60
LD2CB:  .byte $C4, $34, $38, $1E, $3A, $3A, $00, $20 ;ASCVH <PRESS FIRE TO SELECT>
LD2D3:  .byte $26, $38, $1E, $00, $3C, $32, $00, $3A
LD2DB:  .byte $1E, $2C, $1E, $1A, $BC

FPRFIR:
;  French message MPRFIR (PRESS FIRE): "POUSSEZ FEU QUAND CORRECTE"  x=-78
LD2E0:  .byte $B2, $34, $32, $3E, $3A, $3A, $1E, $48 ;ASCVH <POUSSEZ FEU QUAND CORRECTE>
LD2E8:  .byte $00, $20, $1E, $3E, $00, $36, $3E, $16
LD2F0:  .byte $30, $1C, $00, $1A, $32, $38, $38, $1E
LD2F8:  .byte $1A, $3C, $9E

GPRFIR:
;  German message MPRFIR (PRESS FIRE): "FIRE DRUECKEN WENN RICHTIG"  x=-78
LD2FB:  .byte $B2, $20, $26, $38, $1E, $00, $1C, $38 ;ASCVH <FIRE DRUECKEN WENN RICHTIG>
LD303:  .byte $3E, $1E, $1A, $2A, $1E, $30, $00, $42
LD30B:  .byte $1E, $30, $30, $00, $38, $26, $1A, $24
LD313:  .byte $3C, $26, $A2

SPRFIR:
;  Spanish message MPRFIR (PRESS FIRE): "OPRIMA FIRE PARA SELECCIONAR"  x=-84
LD316:  .byte $AC, $32, $34, $38, $26, $2E, $16, $00 ;ASCVH <OPRIMA FIRE PARA SELECCIONAR>
LD31E:  .byte $20, $26, $38, $1E, $00, $34, $16, $38
LD326:  .byte $16, $00, $3A, $1E, $2C, $1E, $1A, $1A
LD32E:  .byte $26, $32, $30, $16, $B8

EHIGHS:
;  English message MHIGHS (HIGH SCORE): "HIGH SCORES"  x=-68
LD333:  .byte $BC, $24, $26, $22, $24, $00, $3A, $1A ;ASCVH 0BC,<HIGH SCORES>
LD33B:  .byte $32, $38, $1E, $BA

FHIGHS:
;  French message MHIGHS (HIGH SCORE): "MEILLEURS SCORES"  x=-98
LD33F:  .byte $9E, $2E, $1E, $26, $2C, $2C, $1E, $3E ;ASCVH 9E,<MEILLEURS SCORES>
LD347:  .byte $38, $3A, $00, $3A, $1A, $32, $38, $1E
LD34F:  .byte $BA

GHIGHS:
;  German message MHIGHS (HIGH SCORE): "HOECHSTZAHLEN"  x=-80
LD350:  .byte $B0, $24, $32, $1E, $1A, $24, $3A, $3C ;ASCVH 0B0,<HOECHSTZAHLEN>
LD358:  .byte $48, $16, $24, $2C, $1E, $B0

SHIGHS:
;  Spanish message MHIGHS (HIGH SCORE): "RECORDS"  x=-44
LD35E:  .byte $D4, $38, $1E, $1A, $32, $38, $1C, $BA ;ASCVH 0D4,<RECORDS>

ERANK:
;  English message MRANK (RANK): "RANKING FROM 1 TO "  x=-62
LD366:  .byte $C2, $38, $16, $30, $2A, $26, $30, $22 ;ASCVH 0C2,<RANKING FROM 1 TO >
LD36E:  .byte $00, $20, $38, $32, $2E, $00, $04, $00
LD376:  .byte $3C, $32, $80

FRANK:
;  French message MRANK (RANK): "PLACEMENT DE 1 A "  x=-62
LD379:  .byte $C2, $34, $2C, $16, $1A, $1E, $2E, $1E ;ASCVH 0C2,<PLACEMENT DE 1 A >
LD381:  .byte $30, $3C, $00, $1C, $1E, $00, $04, $00
LD389:  .byte $16, $80

GRANK:
;  German message MRANK (RANK): "RANGLISTE VON 1 ZUM "  x=-68
LD38B:  .byte $BC, $38, $16, $30, $22, $2C, $26, $3A ;ASCVH 0BC,<RANGLISTE VON 1 ZUM >
LD393:  .byte $3C, $1E, $00, $40, $32, $30, $00, $04
LD39B:  .byte $00, $48, $3E, $2E, $80

SRANK:
;  Spanish message MRANK (RANK): "RANKING DE 1 A "  x=-56
LD3A0:  .byte $C8, $38, $16, $30, $2A, $26, $30, $22 ;ASCVH 0C8,<RANKING DE 1 A >
LD3A8:  .byte $00, $1C, $1E, $00, $04, $00, $16, $80

ERATE:
;  English message MRATE (RATE YOURSELF): "RATE YOURSELF"  x=-39
LD3B0:  .byte $D9, $38, $16, $3C, $1E, $00, $46, $32 ;ASCVH <RATE YOURSELF>
LD3B8:  .byte $3E, $38, $3A, $1E, $2C, $A0

FRATE:
;  French message MRATE (RATE YOURSELF): "EVALUEZ-VOUS"  x=-36
LD3BE:  .byte $DC, $1E, $40, $16, $2C, $3E, $1E, $48 ;ASCVH <EVALUEZ\VOUS>
LD3C6:  .byte $4C, $40, $32, $3E, $BA

GRATE:
;  German message MRATE (RATE YOURSELF): "SELBST RECHNEN"  x=-42
LD3CB:  .byte $D6, $3A, $1E, $2C, $18, $3A, $3C, $00 ;ASCVH <SELBST RECHNEN>
LD3D3:  .byte $38, $1E, $1A, $24, $30, $1E, $B0

SRATE:
;  Spanish message MRATE (RATE YOURSELF): "CALIFIQUESE"  x=-33
LD3DA:  .byte $DF, $1A, $16, $2C, $26, $20, $26, $36 ;ASCVH <CALIFIQUESE>
LD3E2:  .byte $3E, $1E, $3A, $9E

ENOVIC:
FNOVIC:
;  English message MNOVIC (NOVICE): "NOVICE"  x=-86
;  French message MNOVIC (NOVICE): "NOVICE"  x=-86
LD3E6:  .byte $AA, $30, $32, $40, $26, $1A, $9E ;ASCVH 0AA,<NOVICE>

SNOVIC:
;  Spanish message MNOVIC (NOVICE): "NOVICIO"  x=-86
LD3ED:  .byte $AA, $30, $32, $40, $26, $1A, $26, $B2 ;ASCVH 0AA,<NOVICIO>

GNOVIC:
;  German message MNOVIC (NOVICE): "ANFAENGER"  x=-86
LD3F5:  .byte $AA, $16, $30, $20, $16, $1E, $30, $22 ;ASCVH 0AA,<ANFAENGER>
LD3FD:  .byte $1E, $B8

EEXPER:
FEXPER:
;  English message MEXPER (EXPERT): "EXPERT"  x=74
;  French message MEXPER (EXPERT): "EXPERT"  x=74
LD3FF:  .byte $4A, $1E, $44, $34, $1E, $38, $BC ;ASCVH 4A,<EXPERT>

SEXPER:
;  Spanish message MEXPER (EXPERT): "EXPERTO"  x=69
LD406:  .byte $45, $1E, $44, $34, $1E, $38, $3C, $B2 ;ASCVH 45,<EXPERTO>

GEXPER:
;  German message MEXPER (EXPERT): "ERFAHREN"  x=64
LD40E:  .byte $40, $1E, $38, $20, $16, $24, $38, $1E ;ASCVH 40,<ERFAHREN>
LD416:  .byte $B0

EBONUS:
FBONUS:
GBONUS:
SBONUS:
;  English message MBONUS (BONUS): "BONUS"  x=-117
;  French message MBONUS (BONUS): "BONUS"  x=-117
;  German message MBONUS (BONUS): "BONUS"  x=-117
;  Spanish message MBONUS (BONUS): "BONUS"  x=-117
LD417:  .byte $8B, $18, $32, $30, $3E, $BA ;ASCVH 8B,<BONUS>

ETIME:
;  English message MTIME (TIME): "TIME"  x=-24
LD41D:  .byte $E8, $3C, $26, $2E, $9E ;ASCVH 0E8,<TIME>

FTIME:
;  French message MTIME (TIME): "DUREE"  x=-32
LD422:  .byte $E0, $1C, $3E, $38, $1E, $9E ;ASCVH 0E0,<DUREE>

GTIME:
;  German message MTIME (TIME): "ZEIT"  x=-24
LD428:  .byte $E8, $48, $1E, $26, $BC ;ASCVH 0E8,<ZEIT>

STIME:
;  Spanish message MTIME (TIME): "TIEMPO"  x=-28
LD42D:  .byte $E4, $3C, $26, $1E, $2E, $34, $B2 ;ASCVH 0E4,<TIEMPO>

ELEVEL:
;  English message MLEVEL (LEVEL): "LEVEL"  x=-117
LD434:  .byte $8B, $2C, $1E, $40, $1E, $AC ;ASCVH 8B,<LEVEL>

FLEVEL:
;  French message MLEVEL (LEVEL): "NIVEAU"  x=-117
LD43A:  .byte $8B, $30, $26, $40, $1E, $16, $BE ;ASCVH 8B,<NIVEAU>

GLEVEL:
;  German message MLEVEL (LEVEL): "GRAD"  x=-117
LD441:  .byte $8B, $22, $38, $16, $9C ;ASCVH 8B,<GRAD>

SLEVEL:
;  Spanish message MLEVEL (LEVEL): "NIVEL"  x=-117
LD446:  .byte $8B, $30, $26, $40, $1E, $AC ;ASCVH 8B,<NIVEL>

EHOLE:
;  English message MHOLE (HOLE): "HOLE"  x=-117
LD44C:  .byte $8B, $24, $32, $2C, $9E ;ASCVH 8B,<HOLE>

FHOLE:
;  French message MHOLE (HOLE): "TROU"  x=-117
LD451:  .byte $8B, $3C, $38, $32, $BE ;ASCVH 8B,<TROU>

SHOLE:
;  Spanish message MHOLE (HOLE): "HOYO"  x=-117
LD456:  .byte $8B, $24, $32, $46, $B2 ;ASCVH 8B,<HOYO>

GHOLE:
;  German message MHOLE (HOLE): "LOCH"  x=-117
LD45B:  .byte $8B, $2C, $32, $1A, $A4 ;ASCVH 8B,<LOCH>

EINSER:
;  English message MINSER (INSERT COINS): "INSERT COINS"  x=-36
LD460:  .byte $DC, $26, $30, $3A, $1E, $38, $3C, $00 ;ASCVH <INSERT COINS>
LD468:  .byte $1A, $32, $26, $30, $BA

FINSER:
;  French message MINSER (INSERT COINS): "INTRODUIRE LES PIECES"  x=-63
LD46D:  .byte $C1, $26, $30, $3C, $38, $32, $1C, $3E ;ASCVH <INTRODUIRE LES PIECES>
LD475:  .byte $26, $38, $1E, $00, $2C, $1E, $3A, $00
LD47D:  .byte $34, $26, $1E, $1A, $1E, $BA

GINSER:
;  German message MINSER (INSERT COINS): "GELD EINWERFEN"  x=-42
LD483:  .byte $D6, $22, $1E, $2C, $1C, $00, $1E, $26 ;ASCVH <GELD EINWERFEN>
LD48B:  .byte $30, $42, $1E, $38, $20, $1E, $B0

SINSER:
;  Spanish message MINSER (INSERT COINS): "INSERTE FICHAS"  x=-42
LD492:  .byte $D6, $26, $30, $3A, $1E, $38, $3C, $1E ;ASCVH <INSERTE FICHAS>
LD49A:  .byte $00, $20, $26, $1A, $24, $16, $BA

ECMODE:
FCMODE:
GCMODE:
SCMODE:
;  English message MCMODE (FREE PLAY): "FREE PLAY"  x=0
;  French message MCMODE (FREE PLAY): "FREE PLAY"  x=0
;  German message MCMODE (FREE PLAY): "FREE PLAY"  x=0
;  Spanish message MCMODE (FREE PLAY): "FREE PLAY"  x=0
LD4A1:  .byte $00, $20, $38, $1E, $1E, $00, $34, $2C ;ASCVH 0,<FREE PLAY>
LD4A9:  .byte $16, $C6

ECMOD1:
;  English message MCMOD1 (1 COIN 2 PLAYS): "1 COIN 2 PLAYS"  x=14
LD4AB:  .byte $0E, $04, $00, $1A, $32, $26, $30, $00 ;ASCVH 0E,<1 COIN 2 PLAYS>
LD4B3:  .byte $06, $00, $34, $2C, $16, $46, $BA

FCMOD1:
;  French message MCMOD1 (1 COIN 2 PLAYS): "1 PIECE 2 JOUEURS"  x=-6
LD4BA:  .byte $FA, $04, $00, $34, $26, $1E, $1A, $1E ;ASCVH 0FA,<1 PIECE 2 JOUEURS>
LD4C2:  .byte $00, $06, $00, $28, $32, $3E, $1E, $3E
LD4CA:  .byte $38, $BA

GCMOD1:
;  German message MCMOD1 (1 COIN 2 PLAYS): "1 MUENZ 2 SPIELE"  x=0
LD4CC:  .byte $00, $04, $00, $2E, $3E, $1E, $30, $48 ;ASCVH 0,<1 MUENZ 2 SPIELE>
LD4D4:  .byte $00, $06, $00, $3A, $34, $26, $1E, $2C
LD4DC:  .byte $9E

SCMOD1:
;  Spanish message MCMOD1 (1 COIN 2 PLAYS): "1 MONEDA 2 JUEGOS"  x=-6
LD4DD:  .byte $FA, $04, $00, $2E, $32, $30, $1E, $1C ;ASCVH 0FA,<1 MONEDA 2 JUEGOS>
LD4E5:  .byte $16, $00, $06, $00, $28, $3E, $1E, $22
LD4ED:  .byte $32, $BA

ECMOD2:
;  English message MCMOD2 (1 COIN 1 PLAY): "1 COIN 1 PLAY"  x=20
LD4EF:  .byte $14, $04, $00, $1A, $32, $26, $30, $00 ;ASCVH 14,<1 COIN 1 PLAY>
LD4F7:  .byte $04, $00, $34, $2C, $16, $C6

FCMOD2:
;  French message MCMOD2 (1 COIN 1 PLAY): "1 PIECE 1 JOUEUR"  x=0
LD4FD:  .byte $00, $04, $00, $34, $26, $1E, $1A, $1E ;ASCVH 0,<1 PIECE 1 JOUEUR>
LD505:  .byte $00, $04, $00, $28, $32, $3E, $1E, $3E
LD50D:  .byte $B8

GCMOD2:
;  German message MCMOD2 (1 COIN 1 PLAY): "1 MUENZE 1 SPIEL"  x=0
LD50E:  .byte $00, $04, $00, $2E, $3E, $1E, $30, $48 ;ASCVH 0,<1 MUENZE 1 SPIEL>
LD516:  .byte $1E, $00, $04, $00, $3A, $34, $26, $1E
LD51E:  .byte $AC

SCMOD2:
;  Spanish message MCMOD2 (1 COIN 1 PLAY): "1 MONEDA 1 JUEGO"  x=0
LD51F:  .byte $00, $04, $00, $2E, $32, $30, $1E, $1C ;ASCVH 0,<1 MONEDA 1 JUEGO>
LD527:  .byte $16, $00, $04, $00, $28, $3E, $1E, $22
LD52F:  .byte $B2

ECMOD3:
;  English message MCMOD3 (2 COINS 1 PLAY): "2 COINS 1 PLAY"  x=14
LD530:  .byte $0E, $06, $00, $1A, $32, $26, $30, $3A ;ASCVH 0E,<2 COINS 1 PLAY>
LD538:  .byte $00, $04, $00, $34, $2C, $16, $C6

FCMOD3:
;  French message MCMOD3 (2 COINS 1 PLAY): "2 PIECES 1 JOUEUR"  x=-6
LD53F:  .byte $FA, $06, $00, $34, $26, $1E, $1A, $1E ;ASCVH 0FA,<2 PIECES 1 JOUEUR>
LD547:  .byte $3A, $00, $04, $00, $28, $32, $3E, $1E
LD54F:  .byte $3E, $B8

GCMOD3:
;  German message MCMOD3 (2 COINS 1 PLAY): "2 MUENZEN 1 SPIEL"  x=-6
LD551:  .byte $FA, $06, $00, $2E, $3E, $1E, $30, $48 ;ASCVH 0FA,<2 MUENZEN 1 SPIEL>
LD559:  .byte $1E, $30, $00, $04, $00, $3A, $34, $26
LD561:  .byte $1E, $AC

SCMOD3:
;  Spanish message MCMOD3 (2 COINS 1 PLAY): "2 MONEDAS 1 JUEGO"  x=-6
LD563:  .byte $FA, $06, $00, $2E, $32, $30, $1E, $1C ;ASCVH 0FA,<2 MONEDAS 1 JUEGO>
LD56B:  .byte $16, $3A, $00, $04, $00, $28, $3E, $1E
LD573:  .byte $22, $B2

ZATLIS:
EATARI:
FATARI:
GATARI:
SATARI:
;  English message MATARI (MCMLXXX ATARI): "© MCMLXXX ATARI"  x=-45
;  French message MATARI (MCMLXXX ATARI): "© MCMLXXX ATARI"  x=-45
;  German message MATARI (MCMLXXX ATARI): "© MCMLXXX ATARI"  x=-45
;  Spanish message MATARI (MCMLXXX ATARI): "© MCMLXXX ATARI"  x=-45
LD575:  .byte $D3, $50, $00, $2E, $1A, $2E, $2C, $44 ;ASCVH <^ MCMLXXX ATARI>
LD57D:  .byte $44, $44, $00, $16, $3C, $16, $38, $A6

ZATLIE:
ECREDI:
FCREDI:
;  English message MCREDI (CREDITS): "CREDITS "  x=-96
;  French message MCREDI (CREDITS): "CREDITS "  x=-96
LD585:  .byte $A0, $1A, $38, $1E, $1C, $26, $3C, $3A ;ASCVH 0A0,<CREDITS >
LD58D:  .byte $80

GCREDI:
;  German message MCREDI (CREDITS): "KREDITE "  x=-96
LD58E:  .byte $A0, $2A, $38, $1E, $1C, $26, $3C, $1E ;ASCVH 0A0,<KREDITE >
LD596:  .byte $80

SCREDI:
;  Spanish message MCREDI (CREDITS): "CREDITOS "  x=-96
LD597:  .byte $A0, $1A, $38, $1E, $1C, $26, $3C, $32 ;ASCVH 0A0,<CREDITOS >
LD59F:  .byte $3A, $80

EBONPT:
FBONPT:
GBONPT:
SBONPT:
;  English message MBONPT (BONUS PTS): "BONUS "  x=-38
;  French message MBONPT (BONUS PTS): "BONUS "  x=-38
;  German message MBONPT (BONUS PTS): "BONUS "  x=-38
;  Spanish message MBONPT (BONUS PTS): "BONUS "  x=-38
LD5A1:  .byte $DA, $18, $32, $30, $3E, $3A, $80 ;ASCVH 0DA,<BONUS >

E2GAME:
;  English message M2GAME (2 GAME MINIMUM): "2 CREDIT MINIMUM"  x=-48
LD5A8:  .byte $D0, $06, $00, $1A, $38, $1E, $1C, $26 ;ASCVH <2 CREDIT MINIMUM>
LD5B0:  .byte $3C, $00, $2E, $26, $30, $26, $2E, $3E
LD5B8:  .byte $AE

F2GAME:
;  French message M2GAME (2 GAME MINIMUM): "2 JEUX MINIMUM"  x=-42
LD5B9:  .byte $D6, $06, $00, $28, $1E, $3E, $44, $00 ;ASCVH <2 JEUX MINIMUM>
LD5C1:  .byte $2E, $26, $30, $26, $2E, $3E, $AE

G2GAME:
;  German message M2GAME (2 GAME MINIMUM): "2 SPIELE MINIMUM"  x=-48
LD5C8:  .byte $D0, $06, $00, $3A, $34, $26, $1E, $2C ;ASCVH <2 SPIELE MINIMUM>
LD5D0:  .byte $1E, $00, $2E, $26, $30, $26, $2E, $3E
LD5D8:  .byte $AE

S2GAME:
;  Spanish message M2GAME (2 GAME MINIMUM): "2 JUEGOS MINIMO"  x=-45
LD5D9:  .byte $D3, $06, $00, $28, $3E, $1E, $22, $32 ;ASCVH <2 JUEGOS MINIMO>
LD5E1:  .byte $3A, $00, $2E, $26, $30, $26, $2E, $B2

EBOLIF:
;  English message MBOLIF (BONUS EVERY): "BONUS EVERY "  x=-56
LD5E9:  .byte $C8, $18, $32, $30, $3E, $3A, $00, $1E ;ASCVH 0C8,<BONUS EVERY >
LD5F1:  .byte $40, $1E, $38, $46, $80

FBOLIF:
;  French message MBOLIF (BONUS EVERY): "BONUS CHAQUE "  x=-50
LD5F6:  .byte $CE, $18, $32, $30, $3E, $3A, $00, $1A ;ASCVH -50.,<BONUS CHAQUE >
LD5FE:  .byte $24, $16, $36, $3E, $1E, $80

GBOLIF:
;  German message MBOLIF (BONUS EVERY): "BONUS JEDE "  x=-50
LD604:  .byte $CE, $18, $32, $30, $3E, $3A, $00, $28 ;ASCVH -50.,<BONUS JEDE >
LD60C:  .byte $1E, $1C, $1E, $80

SBOLIF:
;  Spanish message MBOLIF (BONUS EVERY): "BONUS CADA "  x=-56
LD610:  .byte $C8, $18, $32, $30, $3E, $3A, $00, $1A ;ASCVH 0C8,<BONUS CADA >
LD618:  .byte $16, $1C, $16, $80

ESPIKE:
;  English message MSPIKE (AVOID SPIKES): "AVOID SPIKES"  x=-72
LD61C:  .byte $B8, $16, $40, $32, $26, $1C, $00, $3A ;ASCVH -72.,<AVOID SPIKES>
LD624:  .byte $34, $26, $2A, $1E, $BA

FSPIKE:
;  French message MSPIKE (AVOID SPIKES): "ATTENTION AUX LANCES"  x=-120
LD629:  .byte $88, $16, $3C, $3C, $1E, $30, $3C, $26 ;ASCVH -120.,<ATTENTION AUX LANCES>
LD631:  .byte $32, $30, $00, $16, $3E, $44, $00, $2C
LD639:  .byte $16, $30, $1A, $1E, $BA

GSPIKE:
;  German message MSPIKE (AVOID SPIKES): "SPITZEN AUSWEICHEN"  x=-106
LD63E:  .byte $96, $3A, $34, $26, $3C, $48, $1E, $30 ;ASCVH 96,<SPITZEN AUSWEICHEN>
LD646:  .byte $00, $16, $3E, $3A, $42, $1E, $26, $1A
LD64E:  .byte $24, $1E, $B0

SSPIKE:
;  Spanish message MSPIKE (AVOID SPIKES): "EVITE LAS PUNTAS"  x=-96
LD651:  .byte $A0, $1E, $40, $26, $3C, $1E, $00, $2C ;ASCVH -96.,<EVITE LAS PUNTAS>
LD659:  .byte $16, $3A, $00, $34, $3E, $30, $3C, $16
LD661:  .byte $BA

EAPROA:
;  English message MAPROA (APPROACH): "LEVEL"  x=-32
LD662:  .byte $E0, $2C, $1E, $40, $1E, $AC ;ASCVH 0E0,<LEVEL>

FAPROA:
;  French message MAPROA (APPROACH): "NIVEAU"  x=-38
LD668:  .byte $DA, $30, $26, $40, $1E, $16, $BE ;ASCVH 0DA,<NIVEAU>

GAPROA:
;  German message MAPROA (APPROACH): "GRAD"  x=-30
LD66F:  .byte $E2, $22, $38, $16, $9C ;ASCVH 0E2,<GRAD>

SAPROA:
;  Spanish message MAPROA (APPROACH): "NIVEL"  x=-32
LD674:  .byte $E0, $30, $26, $40, $1E, $AC ;ASCVH 0E0,<NIVEL>

ESUPZA:
FSUPZA:
;  English message MSUPZA (NEW SUPER): "SUPERZAPPER RECHARGE"  x=-60
;  French message MSUPZA (NEW SUPER): "SUPERZAPPER RECHARGE"  x=-60
LD67A:  .byte $C4, $3A, $3E, $34, $1E, $38, $48, $16 ;ASCVH <SUPERZAPPER RECHARGE>
LD682:  .byte $34, $34, $1E, $38, $00, $38, $1E, $1A
LD68A:  .byte $24, $16, $38, $22, $9E

GSUPZA:
;  German message MSUPZA (NEW SUPER): "NEUER SUPERZAPPER"  x=-51
LD68F:  .byte $CD, $30, $1E, $3E, $1E, $38, $00, $3A ;ASCVH <NEUER SUPERZAPPER>
LD697:  .byte $3E, $34, $1E, $38, $48, $16, $34, $34
LD69F:  .byte $1E, $B8

SSUPZA:
;  Spanish message MSUPZA (NEW SUPER): "NUEVO SUPERZAPPER"  x=-51
LD6A1:  .byte $CD, $30, $3E, $1E, $40, $32, $00, $3A ;ASCVH <NUEVO SUPERZAPPER>
LD6A9:  .byte $3E, $34, $1E, $38, $48, $16, $34, $34
LD6B1:  .byte $1E, $B8

LNGTAB:
LD6B3:  .word ENGMSG
LD6B5:  .word FREMSG
LD6B7:  .word GERMSG
LD6B9:  .word SPAMSG

;------------------------------------------------------------------------------
; INILIT - INITIALIZE LANGUAGE POINTER
;------------------------------------------------------------------------------
INILIT:
LD6BB:  LDA  INOP1                  ;SET UP BONUS LIFE INTERVAL
LD6BE:  STA  OPTIN2
LD6C0:  AND  #$38
LD6C2:  LSR
LD6C3:  LSR
LD6C4:  LSR
LD6C5:  TAX
LD6C6:  LDA  TBLIFI,X
LD6C9:  STA  BLIFIN
LD6CC:  LDA  INOP0                  ;READ OPTION SWITCHES  [CS] Get the contents of DIP N13,
LD6CF:  EOR  #$02                   ;[CS] EOR with 02 (reason unknown) and
LD6D1:  STA  S_CMODE                ;[CS] store into 0009.
LD6D3:  LDA  OPTIN2                 ;LIVES/GAME
LD6D5:  ROL
LD6D6:  ROL
LD6D7:  ROL
LD6D8:  AND  #$03
LD6DA:  TAX
LD6DB:  LDA  GAMLVS,X
LD6DE:  STA  LVSGAM
LD6E1:  LDA  OPTIN2                 ;LANGUAGE CHOICE
LD6E3:  AND  #$06
LD6E5:  TAY
LD6E6:  LDA  LNGTAB,Y               ;SET POINTER TO LANGUAGE PTRS.
LD6E9:  STA  LITRAL
LD6EB:  LDA  LNGTAB+1,Y
LD6EE:  STA  LITRAL+1
LD6F0:  JSR  GETOP3                 ;[CS] Read D/E2 switch (special format) and store into $0037.
LD6F3:  STA  OPTIN3
LD6F6:  RTS

TBLIFI:
LD6F7:  .byte $02, $01, $03, $04, $05, $06, $07, $00

GAMLVS:
;[CS] Table believed to store the number of lives to start with
LD6FF:  .byte $03, $04, $05, $02

;==============================================================================
; MODULE ALHAR2   ALHAR2.MAC
;   IRQ handler: watchdog, frame timer, switch debounce, spinner, VG restart;
;   checksum equates and the 6502 vectors.
;==============================================================================

;.SBTTL IRQ-ENTRY

CHKSMA:
LD703:  .byte QCHKSA

;------------------------------------------------------------------------------
; IRQ - FUNCTION: VERIFY STACK POINTER, PC AND SOFTWARE WATCHDOG
;   RESET IF NECESSARY
;   SWITCH DISPLAY BUFFER POINTER AND BUILD BUFFER POINTERS WHEN TIME
;   [CS] Interrupt handler. This handles both the maskable and the non-maskable
;   [CS] interrupts that are produced by the hardware.
;------------------------------------------------------------------------------
IRQ:
LD704:  PHA                         ;SAVE ACC,X,Y  [CS] Transfer the accumulator, X,
LD705:  TXA                         ;[CS] and Y registers to the stack.
LD706:  PHA                         ;[CS] Necessary to preserve these
LD707:  TYA                         ;[CS] registers upon return.
LD708:  PHA
LD709:  CLD                         ;SET HEX  [CS] Ensure we're in normal math mode.
LD70A:  TSX                         ;[CS] Look at the stack pointer. If it
LD70B:  CPX  #$D0                   ;[CS] is less than D0, then BREAK.
LD70D:  BCC  LD713                  ;IFCS  STACK TOO DEEP?
LD70F:  LDA  FRTIMR                 ;[CS] If the 0-9 timer is > #7f,
LD711:  BPL  SOFTOK                 ;OVERRUN TIME LIMIT (BR IF OK)  [CS] then BREAK!!!
LD713:  BRK
LD714:  JMP  RESET                  ;[CS] Perform a full reset of the game, as done on power-up.

;------------------------------------------------------------------------------
; SOFTOK - IRQ-PROCESSING
;------------------------------------------------------------------------------
SOFTOK:
LD717:  STA  WTCHDG                 ;KICK DOG  [CS] Clear the watchdog timer.

;.SBTTL SWITCHES
;[CS] Interrupt level handling of inputs on Pokey #1.
LD71A:  STA  POTGO                  ;READ POT  [CS] Rescan the inputs on Pokey #1.
LD71D:  LDA  ALLPOT                 ;[CS] Grab the inputs for Pokey #1 BITS 0-3: Encoder Wheel BIT  4  : Cocktail detection BIT  5  : Switch #1 at D/E2 BITS 6-7: Unused.
LD720:  EOR  #$0F                   ;[CS] Invert just the encoder bits.
LD722:  TAY
LD723:  AND  #COCKTA                ;[CS] Store in $0117 a 10 if the game
LD725:  STA  COCTAL                 ;[CS] is a cocktail table. 00 if not.
LD728:  TYA
LD729:  SEC                         ;[CS] Store the raw spinner position
LD72A:  SBC  OTB                    ;[CS] (0-F) $0052.
LD72C:  AND  #$0F
LD72E:  CMP  #$08
LD730:  BCC  LD734                  ;IFCS
LD732:  ORA  #$F0
LD734:  CLC
LD735:  ADC  TBHD                   ;[CS] Store the relative spinner
LD737:  STA  TBHD                   ;[CS] position (00-FF) since last read
LD739:  STY  OTB                    ;[CS] by the software into $0050.
;[CS] Interrupt level handling of input on Pokey #2 and hardware register $0C00.
LD73B:  STA  POTGO2                 ;KICK POKEY  [CS] Rescan the inputs on Pokey #2.
LD73E:  LDY  ALLPO2                 ;[CS] Grab the inputs of Pokey #2.
;READ POKEY SWITCHES
LD741:  LDA  IN1                    ;[CS] Load the 0C00 register.
LD744:  STA  S_COINA                ;READ COIN, SLAM, TEST SWITCHES  [CS] Store in shadow location $0008. BIT 0: Right Coin BIT 1: Center Coin BIT 2: Left Coin BIT 3: Slam Swith BIT 4: Test Switch BIT 5: Diagnoistic Step Pad BIT 6: HALT (from vector machine) BIT 7: 3kHz square wave
;DEBOUNCE SWITCHES (1 BIT/SWITCH):
;CONSIDERING EACH BIT INDIVIDUALLY,
;IF INPUT+DBSW+SWSTAT IS
;0 OR 1, NEW SWSTAT=0
;2 OR 3, NEW SWSTAT=1
LD746:  LDA  DBSW                   ;[CS] Make a copy of $004C.
LD748:  STY  DBSW                   ;[CS] Store Pokey #2 inputs in $004C. BIT 0: D/E2 switch #2 BIT 1: D/E2 switch #3 BIT 2: D/E2 switch #4 BIT 3: Fire Button BIT 4: Zapper Button BIT 5: Start Player 1 Button BIT 6: Start Player 2 Button BIT 7: Unused.
LD74A:  TAY                         ;[CS] WHAT A MESS! Here's the jist...
LD74B:  AND  DBSW                   ;((INPUT AND DBSW) OR SWSTAT)
LD74D:  ORA  SWSTAT                 ;[CS] Put into $004D and $004F the
LD74F:  STA  SWSTAT                 ;[CS] buttons with just a bit of
LD751:  TYA                         ;AND  [CS] debounce checking.
LD752:  ORA  DBSW                   ;(DBSW OR INPUT)
LD754:  AND  SWSTAT                 ;[CS] *AND* the buttons into $004E so
LD756:  STA  SWSTAT                 ;SAVE RESULT HERE (0=ON, 1=OFF)  [CS] you can maintain a list of
LD758:  TAY                         ;[CS] button events to process.
LD759:  EOR  SWRELE                 ;MUST RELEASE PROCESS
LD75B:  AND  SWSTAT                 ;[CS] Oh. Almost forgot. $004C will
LD75D:  ORA  SWFINA                 ;LATCH ON  [CS] have the raw button status with
LD75F:  STA  SWFINA                 ;[CS] no debounce checking whatsoever.
LD761:  STY  SWRELE

;.SBTTL OUTPUTS
;[CS] Interrupt level handling of LEDS, coin counters, and XY inversion register.
LD763:  LDA  TOUT0                  ;[CS] Load the X/Y axis flip state.
LD765:  LDY  S_CCTIM                ;[CS] Increment coin counter "A"?
LD767:  BPL  LD76B                  ;IFMI  COIN COUNTERS  [CS] If so, set the appropriate bit.
LD769:  ORA  #MLCCNT
LD76B:  LDY  S_CCTIM+1              ;[CS] Increment coin counter "B"?
LD76D:  BPL  LD771                  ;IFMI  [CS] If so, set the appropriate bit.
LD76F:  ORA  #MMCCNT
LD771:  LDY  S_CCTIM+2              ;[CS] Increment coin cointer "C"?
LD773:  BPL  LD777                  ;IFMI  [CS] If so, set the appropriate bit.
LD775:  ORA  #MRCCNT
LD777:  STA  OUT0                   ;[CS] Implement coin counter / video inversion (XY flip) register.
;DO START LIGHTS
LD77A:  LDX  NUMPLA                 ;INDEX FOR LIGHTS ON BIT IF IN GAME
LD77C:  INX
LD77D:  LDY  QSTATUS
LD77F:  BNE  LD791                  ;IFEQ  ATTRACT MODE?
LD781:  LDX  #$00                   ;YES. OFF  [CS] Haven't gone through this
LD783:  LDY  S_INTCT                ;[CS] extensively, but it appears to
LD785:  CPY  #$40                   ;[CS] be a handler for the LEDs.
LD787:  BCC  LD791                  ;IFCS  BLINK ON TIME?
LD789:  LDX  S_S_CRDT               ;YES.  [CS] Are there two or + game credits
LD78B:  CPX  #$02                   ;[CS] pending?
LD78D:  BCC  LD791                  ;IFCS
LD78F:  LDX  #$03
LD791:  LDA  LITSON,X
LD794:  EOR  TNKOUT
LD796:  AND  #MLED1|MLED2
LD798:  EOR  TNKOUT
LD79A:  STA  TNKOUT
LD79C:  STA  OUTANK                 ;[CS] Set the LED state.
LD79F:  JSR  MOOLAH                 ;PROCESS COINS
LD7A2:  JSR  MODSND                 ;PROCESS SOUNDS
LD7A5:  INC  FRTIMR                 ;UPDATE FRAME TIMER  [CS] Update the 0-9 timer
LD7A7:  INC  S_INTCT                ;INTERRUPT COUNTER  [CS] Update the 00-FF timer
LD7A9:  BNE  LD7C9                  ;IFEQ  ANOTHER SECOND?  [CS] Has a second elapsed?
LD7AB:  INC  SECOUL                 ;YES. UPDATE UP TIMER  [CS] Increase the counter which keeps
LD7AE:  BNE  LD7B8                  ;IFEQ  [CS] track of the number of seconds
LD7B0:  INC  SECOUM                 ;[CS] that the machine has been on.
LD7B3:  BNE  LD7B8                  ;IFEQ
LD7B5:  INC  SECOUH
LD7B8:  BIT  QSTATUS
LD7BA:  BVC  LD7C9                  ;IFVS  GAME TIMER MODE?
LD7BC:  INC  SECOPL                 ;YES. UPDATE PLAY TIMER  [CS] Increase the counter which keeps
LD7BF:  BNE  LD7C9                  ;IFEQ  [CS] track of the number of seconds
LD7C1:  INC  SECOPM                 ;[CS] that the machine has been played.
LD7C4:  BNE  LD7C9                  ;IFEQ
LD7C6:  INC  SECOPH
LD7C9:  BIT  IN1                    ;[CS] Is the vector machine in HALT state?
LD7CC:  BVC  LD7D7                  ;IFVS  VG DONE (HALTED)?  [CS] If not, exit the interrupt handler.
;[CS] Interrupt level handling of the Vector State Machine
LD7CE:  INC  SPARE3                 ;SET STOPPED FLAG  [CS] Update counter that is tied to vector refreshes. (informational)
LD7D1:  STA  VGSTOP                 ;[CS] Vector machine RESET
LD7D4:  STA  VGSTART                ;[CS] Vector machine GO
LD7D7:  PLA                         ;RESTORE Y,X,ACC  [CS] Restore the state of the
LD7D8:  TAY                         ;[CS] Y register, X register, and the
LD7D9:  PLA                         ;[CS] accumulator. The "RTI" command
LD7DA:  TAX                         ;[CS] will also restore the process
LD7DB:  PLA                         ;[CS] status register, which has the
LD7DC:  RTI                         ;[CS] proper math mode.

LITSON:
;[CS] End of interrupt handler code.
LD7DD:  .byte $FF, <[~MLED1], <[~MLED2], <[~[MLED1|MLED2]] ;[CS] Table referenced by $D791. Something about LED blinking.

;==============================================================================
; MODULE ALTES2   ALTES2.MAC
;   Self-test / diagnostics, RESET (power-on) code, RAM/ROM/EAROM tests,
;   bookkeeping display, switch/sound tests.
;==============================================================================

;------------------------------------------------------------------------------
; SYSTEM - SYSTEM INFO -
;------------------------------------------------------------------------------
SYSTEM:
LD7E1:  LDA  #$00                   ;STOP GAME
LD7E3:  STA  QSTATUS
LD7E5:  LDA  #CDSYST
LD7E7:  STA  QDSTATE                ;SYSTEM DISPLAY STATE
LD7E9:  LDA  EAFLG
LD7EC:  BNE  LD803                  ;IFEQ  EAROM BUSY?
LD7EE:  LDA  IN1                    ;NO  [CS] Check the TEST switch.
LD7F1:  AND  #MTEST
LD7F3:  BEQ  LD803                  ;IFNE  ALL DONE?  [CS] If not in TEST, skip ahead.
LD7F5:  LDA  #CNEWGA                ;YES. BACK TO GAME
LD7F7:  STA  QSTATE
LD7F9:  LDA  EABAD
LD7FC:  AND  #$03
LD7FE:  BEQ  LD803                  ;IFNE  INITIALIZE SCORE STUFF?
;[CS] Start of ROM 136002.222 at $D800.	 NOTE: VERSION 2+ ROMS, FOLKS.
LD800:  JSR  INIINI                 ;YES
LD803:  RTS

;------------------------------------------------------------------------------
; DSPSYS - SYSTEM DISPLAY STATE ROUTINE
;------------------------------------------------------------------------------
DSPSYS:
LD804:  JSR  INILIT                 ;SET UP LANGUAGE PTR
LD807:  JSR  DSPCRD                 ;COIN MODE STUFF
LD80A:  JSR  DOPSWI                 ;DISPLAY BINARY OPTION SWITCHES
LD80D:  JSR  DBOOKE                 ;BOOKEEPING
;DISPLAY LIVES/GAME
LD810:  LDA  LVSGAM                 ;GET # LIVES
LD813:  STA  INDEX1
LD815:  JSR  VGCNTR
LD818:  LDA  #$E8
LD81A:  LDX  #$C0
LD81C:  JSR  VGVTR1                 ;POSITION BEAM
LD81F:  LDA  #>[LIFEY+1]            ;LAH LIFEY+1
LD821:  LDX  #<LIFEY                ;LXL LIFEY
LD823:  JSR  VGJSRL                 ;DRAW LIFE PIC
LD826:  DEC  INDEX1
LD828:  BNE  LD81F                  ;EQEND
LD82A:  LDA  OPTIN3                 ;EASY/MED/HARD DISPLAY
LD82D:  AND  #$03
LD82F:  ASL
LD830:  TAY
LD831:  LDA  SYSOPT+9,Y
LD834:  LDX  SYSOPT+8,Y
LD837:  JSR  VGJSRL

;.SBTTL SYSTEM INFO - SPECIAL OPTIONS
LD83A:  LDA  CURSL1                 ;UPDATE CURSOR
LD83D:  JSR  GINICO
LD840:  STA  CURSL1
LD843:  AND  #$06
LD845:  PHA
LD846:  TAY
LD847:  LDA  SYSOPT+1,Y
LD84A:  LDX  SYSOPT,Y
LD84D:  JSR  VGJSRL                 ;DISPLAY OPTIONS (SELF TEST ZERO EAROM)
LD850:  PLA
LD851:  LSR
LD852:  TAX
LD853:  LDA  SWSTAT
LD855:  AND  OPTMSK,X
LD858:  CMP  OPTMSK,X
LD85B:  BNE  LD877                  ;IFEQ  OPTION SELECTED?
LD85D:  DEX                         ;YES.
LD85E:  DEX
LD85F:  BPL  LD864                  ;IFMI
LD861:  JMP  RESET                  ;SELF TEST OPTION  [CS] Perform a full reset of the game, as done on power-up.
LD864:  BNE  LD86C                  ;IFEQ
LD866:  JSR  EAZBOO                 ;ZERO TIMES OPTION
LD869:  CLV                         ;ELSE
LD86A:  BVC  LD877
LD86C:  JSR  EAZHIS                 ;ZERO HI SCORES OPTION
LD86F:  LDA  EABAD
LD872:  ORA  #$03
LD874:  STA  EABAD                  ;INDUCE HI SCORE INIT
LD877:  LDA  EAFLG
LD87A:  AND  EAZFLG
LD87D:  BEQ  LD886                  ;IFNE  EAROM BUSY?
LD87F:  LDA  #>[EASING+1]           ;LAH EASING+1
LD881:  LDX  #<EASING               ;LXL EASING  YES. ERASING
LD883:  JSR  VGJSRL

;.SBTTL SYSTEM INFO
;MECH MULTIPLIERS
LD886:  JSR  VGCNTR
LD889:  LDA  OPTIN1
LD88B:  AND  #$1C
LD88D:  LSR
LD88E:  LSR
LD88F:  TAX
LD890:  LDA  CRMECHT,X
LD893:  LDY  #$EE                   ;(src: LDY I,-72./4)
LD895:  LDX  #$1B
LD897:  JSR  POSDIG
;BONUS ADDER
LD89A:  LDA  OPTIN1
LD89C:  LSR
LD89D:  LSR
LD89E:  LSR
LD89F:  LSR
LD8A0:  LSR
LD8A1:  TAX
LD8A2:  LDA  BONADR,X
LD8A5:  LDY  #$32
LD8A7:  LDX  #$F8                   ;(src: LDX I,-32./4)

;------------------------------------------------------------------------------
; POSDIG - POSITION & DISPLAY CHECKSUM  (comment at the call)
;------------------------------------------------------------------------------
POSDIG:
LD8A9:  STA  TEMP0
LD8AB:  TYA
LD8AC:  JSR  VGVTR1                 ;POSITION BEAM
LD8AF:  LDA  #TEMP0
LD8B1:  LDY  #$01
LD8B3:  JMP  DIGTYS

OPTMSK:
LD8B6:  .byte MFIRE|MSUZA, MFIRE|MSUZA, MFIRE|MSTRT1, MFIRE|MSTRT2

CRMECHT:
LD8BA:  .byte $11, $14, $15, $16, $21, $24, $25, $26

BONADR:
LD8C2:  .byte $00, $12, $14, $24, $15, $13, $00, $00

;------------------------------------------------------------------------------
; HIBAD - REPORT BAD RAM
;   BAD 0 PAGE RAM
;------------------------------------------------------------------------------
HIBAD:
LD8CA:  TAY                         ;BAD BITS
LD8CB:  LDA  #$00                   ;BAD BLOCK #

BRAMREP:
;AC=BAD RAM BLOCK
;Y=BAD BITS (NOT 0)
LD8CD:  STY  RAMCND
LD8CF:  LSR
LD8D0:  LSR
LD8D1:  ASL                         ;*2
LD8D2:  TAX
LD8D3:  TYA
LD8D4:  AND  #$0F
LD8D6:  BNE  LD8D9                  ;IFEQ
LD8D8:  INX                         ;+1 IF MSB NIBBLE BAD AND LSB GOOD
LD8D9:  TXS
LD8DA:  LDA  #$A2                   ;SET UP POKEY AMPLITUDE/NOISE
LD8DC:  STA  AUDC1
;LOOP UNTIL BAD NIBBLE IS PROCESSED
LD8DF:  TSX
LD8E0:  BNE  LD8E9                  ;IFEQ  BAD NIBBLE?
LD8E2:  LDA  #$60                   ;YES. BAD (HI) TONE
LD8E4:  LDY  #$09                   ;BAD (LONG) DELAY
LD8E6:  CLV                         ;ELSE
LD8E7:  BVC  LD8ED
LD8E9:  LDA  #$C0                   ;NO. GOOD (LO) TONE
LD8EB:  LDY  #$01                   ;GOOD (SHOT) DELY
LD8ED:  STA  AUDF1                  ;SOUND ON
LD8F0:  LDA  #LEDOFF                ;LED OFF  [CS] Turn on player 1 and player 2 LED.
LD8F2:  STA  OUTANK
LD8F5:  LDX  #$00
LD8F7:  BIT  IN1                    ;[CS] Loop until the signal from the
LD8FA:  BMI  LD8F7                  ;PLEND  [CS] 3kHz square wave is negative.
LD8FC:  BIT  IN1                    ;[CS] Loop until the signal from the
LD8FF:  BPL  LD8FC                  ;MIEND  [CS] 3kHz square wave is positive.
LD901:  STA  WTCHDG                 ;[CS] Clear the watchdog timer.
LD904:  DEX
LD905:  BNE  LD8F7                  ;EQEND
LD907:  DEY
LD908:  BNE  LD8F7                  ;EQEND
LD90A:  STX  AUDC1                  ;SOUND OFF
LD90D:  LDA  #$00
LD90F:  STA  OUTANK                 ;LED ON  [CS] Turn off player 1 and player 2 LED.
LD912:  LDY  #$09
LD914:  BIT  IN1                    ;[CS] Loop until the signal from the
LD917:  BMI  LD914                  ;PLEND  [CS] 3khz square wave is negative.
LD919:  BIT  IN1                    ;[CS] Loop until the signal from the
LD91C:  BPL  LD919                  ;MIEND  [CS] 3kHz square wave is positive.
LD91E:  STA  WTCHDG                 ;[CS] Clear the watchdog timer.
LD921:  DEX
LD922:  BNE  LD914                  ;EQEND
LD924:  DEY
LD925:  BNE  LD914                  ;EQEND
LD927:  TSX                         ;UPDATE RAM COUNT
LD928:  DEX
LD929:  TXS
LD92A:  BPL  LD8DA                  ;MIEND  EXIT AFTER BAD NIBBLE IS PROCESSED
LD92C:  JMP  ROMTST

;------------------------------------------------------------------------------
; HIRBAD - ACC=TEST PATTERN
;   BAD NON 0 PAGE RAM
;------------------------------------------------------------------------------
HIRBAD:
LD92F:  EOR  ($00),Y                ;PUT BAD BITS INTO Y

HIRBD2:
LD931:  TAY
LD932:  LDA  $01
LD934:  CMP  #VECRAM/$100
LD936:  BCC  LD93A                  ;IFCS  VECTOR RAM?
LD938:  SBC  #[VECRAM-$800]/$100    ;YES.
LD93A:  AND  #$1F                   ;BAD RAM BLOCK
LD93C:  JMP  BRAMREP                ;REPORT BAD RAM

;------------------------------------------------------------------------------
; RESET - SELF TEST ENTRY
;   [CS] Boot code. Executed as the very first instructions.
;   [CS] Aka "reset vector".
;   (also: SFTEST)
;------------------------------------------------------------------------------
RESET:
SFTEST:
LD93F:  SEI                         ;NO INTERRUPTS  [CS] Disable interrupts and clear
LD940:  STA  WTCHDG                 ;KICK DOG  [CS] the watchdog timer.
LD943:  STA  VGSTOP                 ;[CS] Reset the vector state machine.
LD946:  LDX  #$FF                   ;[CS] Set the stack pointer to 01FF.
LD948:  TXS                         ;SET STACK PTR TO TOS
LD949:  CLD                         ;[CS] Set normal math mode.
LD94A:  INX
LD94B:  TXA
LD94C:  TAY
LD94D:  STY  $00
LD94F:  STX  $01
LD951:  LDY  #$00                   ;[CS] Clear out a page of RAM.
LD953:  STA  ($00),Y                ;ZERO CELL
LD955:  INY
LD956:  BNE  LD953                  ;EQEND
LD958:  INX
LD959:  CPX  #$08
LD95B:  BNE  LD95F                  ;IFEQ
LD95D:  LDX  #VECRAM/$100
LD95F:  CPX  #[VECRAM+$1000]/$100   ;DONE WITH VECTOR RAM?
LD961:  STA  WTCHDG                 ;[CS] Reset the watchdog timer.
LD964:  BCC  LD94D                  ;CSEND
;YES. ZERO INDIRECT PTRS.
LD966:  STA  $01
LD968:  STA  OUTANK                 ;TURN ON LEDS  [CS] Reset the LED/control state.
LD96B:  STA  AUDF1+$F               ;INITIALIZE POKEYS  [CS] Initialize the POKEYs?
LD96E:  STA  AUDF2+$F
LD971:  LDX  #$07
LD973:  STX  AUDF1+$F               ;[CS] Ready Pokey #1 for sound.
LD976:  STX  AUDF2+$F               ;[CS] Ready Pokey #2 for sound.
LD979:  INX                         ;[CS] Clear out the volume/distortion
LD97A:  STA  AUDF1,X                ;[CS] and frequency for all sound
LD97D:  STA  AUDF2,X                ;[CS] channels on both Pokeys.
LD980:  DEX
LD981:  BPL  LD97A                  ;MIEND
LD983:  LDA  IN1                    ;[CS] Check the TEST switch.
LD986:  AND  #MTEST
LD988:  BEQ  LD9A9                  ;IFNE  CONTINUE SELF TEST?  [CS] If TEST, skip down a bit.
LD98A:  STA  WTCHDG                 ;[CS] Reset the watchdog timer.
LD98D:  DEC  $0100
LD990:  BNE  LD98A                  ;EQEND
LD992:  DEC  $0101
LD995:  BNE  LD98A                  ;EQEND  [CS] Delay loop.
LD997:  LDA  #MVINVY                ;INIT SCREEN FLIP  [CS] Set the "Y" axis as being
LD999:  STA  TOUT0                  ;[CS] flipped.
LD99B:  JSR  REHIIN                 ;NO. READ EAROM
LD99E:  JSR  INIINI                 ;INITIALIZE HI SCORE STUFF
LD9A1:  JSR  INIDSP                 ;SET UP DISPLAY
LD9A4:  CLI                         ;[CS] Enable interrupt processing.
LD9A5:  JMP  MAINLN                 ;GO TO ATTRACT MODE
LD9A8:  .byte $A0                   ;*****FILLER  [CS] WRONG -- RE-DISASSEMBLE

;.SBTTL ZERO PAGE TEST
;ON ENTRY ALL RAM IS 0
LD9A9:  LDX  #$11                   ;STARTING PATTERN  [CS] $D9A9 has first command.
LD9AB:  TXS
LD9AC:  LDY  #$00
LD9AE:  TSX
LD9AF:  STX  $00,Y                  ;PATTERN TO TEST CALL
LD9B1:  LDX  #$01
LD9B3:  INY
LD9B4:  .byte $B9, $00, $00         ;LDA $0000,Y (forced absolute)
LD9B7:  BEQ  LD9BC                  ;IFNE  NOT 0?

JMPHIB:
LD9B9:  JMP  HIBAD                  ;YES. ERROR
LD9BC:  INX
LD9BD:  BNE  LD9B3                  ;EQEND
LD9BF:  TSX
LD9C0:  TXA
LD9C1:  STA  WTCHDG                 ;KICK DOG  [CS] Clear the watchdog timer.
LD9C4:  INY
LD9C5:  .byte $59, $00, $00         ;EOR $0000,Y (forced absolute)
LD9C8:  BNE  JMPHIB                 ;ERROR IF NOT 0
LD9CA:  .byte $99, $00, $00         ;STA $0000,Y (forced absolute)  CLEAR TEST CELL
LD9CD:  INY
LD9CE:  BNE  LD9AE                  ;EQEND
LD9D0:  TSX
LD9D1:  TXA
LD9D2:  ASL                         ;SHIFT PATTERN
LD9D3:  TAX
LD9D4:  BCC  LD9AB                  ;CSEND

;.SBTTL NON ZERO PAGE RAM TEST
LD9D6:  LDY  #$00                   ;START AT PAGE 1
LD9D8:  LDX  #$01
LD9DA:  STY  $00                    ;SET INDIRECT PTR. TO 1ST TEST CELL IN PAGE
LD9DC:  STX  $01
LD9DE:  LDY  #$00
LD9E0:  LDA  ($00),Y
LD9E2:  BEQ  LD9E7                  ;IFNE  NOT 0?
LD9E4:  JMP  HIRBD2                 ;YES. ERROR
LD9E7:  LDA  #$11
LD9E9:  STA  ($00),Y                ;STORE PATTERN TO TEST CELL
LD9EB:  CMP  ($00),Y                ;COMPARE TEST CELL WITH PATTERN
LD9ED:  BEQ  LD9F2                  ;IFNE  NOT 0?
LD9EF:  JMP  HIRBAD                 ;YES. ERROR
LD9F2:  ASL                         ;NEXT PATTERN
LD9F3:  BCC  LD9E9                  ;CSEND
LD9F5:  LDA  #$00
LD9F7:  STA  ($00),Y                ;CLEAR TEST CELL
LD9F9:  INY                         ;NEXT TEST CELL
LD9FA:  BNE  LD9E0                  ;EQEND
LD9FC:  STA  WTCHDG                 ;[CS] Clear the watchdog timer.
LD9FF:  INX                         ;POINT TO NEXT PAGE
LDA00:  CPX  #$08
LDA02:  BNE  LDA06                  ;IFEQ  PAGES 1 TO 7
LDA04:  LDX  #VECRAM/$100
LDA06:  CPX  #[VECRAM+$1000]/$100   ;AND 20 TO 2F
LDA08:  BCC  LD9DA                  ;CSEND

;.SBTTL ROM TEST

ROMTST:
LDA0A:  LDA  #$00
LDA0C:  TAY                         ;INDEX INTO PAGE
LDA0D:  TAX                         ;CHECKSUM INDEX & ROM COUNTER
LDA0E:  STA  INDYLO
LDA10:  LDA  #ROMSTART/$100         ;SET POINTER TO START OF 1ST ROM
LDA12:  STA  INDYHI
LDA14:  LDA  #$08                   ;SET COUNTER FOR 8 PAGES IN ROM
LDA16:  STA  INDEX2
LDA18:  TXA                         ;GET SEED FOR ROM
LDA19:  EOR  (INDYLO),Y             ;UPDATE CHECKSUM
LDA1B:  INY
LDA1C:  BNE  LDA19                  ;EQEND
LDA1E:  INC  INDYHI                 ;NEXT PAGE IN ROM
LDA20:  STA  WTCHDG                 ;KICK DOG  [CS] Clear the watchdog timer.
LDA23:  DEC  INDEX2
LDA25:  BNE  LDA19                  ;EQEND
LDA27:  STA  CHKSMS,X               ;SAVE CHECKUM OF ROM
LDA29:  INX
LDA2A:  CPX  #$02
LDA2C:  BNE  LDA32                  ;IFEQ  PROGRAM ROM NOW?
LDA2E:  LDA  #<[PROG/$100]          ;YES
LDA30:  STA  INDYHI
LDA32:  CPX  #NROMS                 ;DONE YET?
LDA34:  BCC  LDA14                  ;CSEND
;MAKE SOUND IF VG ROM IS BAD
LDA36:  LDA  CHKSMS
LDA38:  BEQ  LDA44                  ;IFNE  BAD VG ROM?
LDA3A:  LDA  #$40                   ;YES. MAKE CONTINUIOUS TONE
LDA3C:  LDX  #$A4
LDA3E:  STA  AUDF1+4
LDA41:  STX  AUDC1+4

;.SBTTL TEST POKEYS,EAROM
;TEST BOTH POKEYS FOR RANDOM #S
LDA44:  LDX  #$05
LDA46:  LDA  RANDOM                 ;[CS] Grab a random number 0-FF
LDA49:  CMP  RANDOM                 ;[CS] Comapre to random 0-FF
LDA4C:  BNE  OK1                    ;[CS] 254/255 chance of skipping
LDA4E:  DEX
LDA4F:  BPL  LDA49                  ;MIEND
LDA51:  STA  PK1CND                 ;BAD POKEY 1

OK1:
LDA53:  LDX  #$05
LDA55:  LDA  RANDO2                 ;[CS] Randomize the accumulator
LDA58:  CMP  RANDO2                 ;[CS] Compare w/random number.
LDA5B:  BNE  OK2                    ;[CS] 254/255 chance of skipping
LDA5D:  DEX
LDA5E:  BPL  LDA58                  ;MIEND
LDA60:  STA  PK2CND                 ;BAD POKEY 2

OK2:
LDA62:  JSR  REHIIN                 ;READ IN EAROM
LDA65:  LDY  #$02                   ;DEFAULT GOOD
LDA67:  LDA  EABAD
LDA6A:  BEQ  LDA76                  ;IFNE  BAD EAROM?
LDA6C:  STA  EARCND                 ;BAD EAROM FLAG
LDA6E:  JSR  EAZERO                 ;YES. ERASE EAROM
LDA71:  LDY  #$00
LDA73:  STY  EABAD
LDA76:  STY  QSTATE
LDA78:  LDX  #$07                   ;SET UP COLOR RAM
LDA7A:  LDA  TABCOL,X               ;[CS] Load the Color RAM from a table.
LDA7D:  STA  COLPORT,X              ;[CS] (8 bytes)
LDA80:  DEX
LDA81:  BPL  LDA7A                  ;MIEND
LDA83:  LDA  #$00                   ;INIT CONTROLS FLIP
LDA85:  STA  OUTANK                 ;[CS] Turn off player 1 and player 2 LED.
LDA88:  LDA  #MVINVY                ;INIT SCREEN FLIP
LDA8A:  STA  OUT0                   ;[CS] Invert video Y axis.

;.SBTTL MAIN DIAG LOOP
LDA8D:  LDY  #$04
LDA8F:  LDX  #$14
LDA91:  BIT  IN1                    ;[CS] Loop until the 3kHz square
LDA94:  BPL  LDA91                  ;MIEND  [CS] wave signal is positive.
LDA96:  BIT  IN1                    ;[CS] Loop until the 3kHz square
LDA99:  BMI  LDA96                  ;PLEND  [CS] wave signal is negative.
LDA9B:  DEX
LDA9C:  BPL  LDA91                  ;MIEND
LDA9E:  DEY
LDA9F:  BMI  TIMEST                 ;BR ABORT IF TOO LONG
LDAA1:  STA  WTCHDG                 ;[CS] Reset the watchdog timer.
LDAA4:  BIT  IN1
LDAA7:  BVC  LDA8F                  ;VSEND

TIMEST:
LDAA9:  STA  VGSTOP
LDAAC:  LDA  #VECRAM&$FF            ;SET POINTER TO TOP OF VECTOR RAM
LDAAE:  STA  VGLIST
LDAB0:  LDA  #VECRAM/$100
LDAB2:  STA  VGLIST+1
LDAB4:  STA  POTGO                  ;[CS] Rescan Pokey #1's pots.
LDAB7:  LDA  ALLPOT                 ;[CS] Grab the binary pot values.
LDABA:  STA  OTB                    ;[CS] Store in $0052.
LDABC:  AND  #$0F                   ;[CS] Put the encoder wheel contents
LDABE:  STA  TBHD                   ;READ POT  [CS] into $0050.
LDAC0:  LDA  IN1                    ;[CS] Invert the 0C00 register and
LDAC3:  EOR  #$FF                   ;[CS] remove the HALT and 3kHz square.
LDAC5:  AND  #MCOINL|MCOINC|MCOINR|S_LMBIT|MDITES ;[CS] Store in 004E.
LDAC7:  STA  SWFINA
LDAC9:  AND  #S_LMBIT|MDITES
LDACB:  BEQ  LDAD8                  ;IFNE  TEST DIAGNOSTIC SWITCH PRESSED?
LDACD:  ASL  DBSW                   ;YES.
LDACF:  BCC  LDAD5                  ;IFCS  DEPRESSED LONG ENOUGH?
LDAD1:  INC  QSTATE                 ;YES. INCREMENT DIAGNOSTIC STATE
LDAD3:  INC  QSTATE
LDAD5:  CLV                         ;ELSE
LDAD6:  BVC  LDADC
LDAD8:  LDA  #$20                   ;NOT PRESSED. RESTART PRESSED TIMER
LDADA:  STA  DBSW
LDADC:  JSR  SSTATE                 ;EXECUTE APPROPRIATE SELF TEST STATE
LDADF:  JSR  VGHALT                 ;PLACE HALT AT END OF DISPLAY LIST
LDAE2:  STA  VGSTART                ;[CS] Vector machine GO
LDAE5:  INC  QFRAME
LDAE7:  LDA  QFRAME
LDAE9:  AND  #$03
LDAEB:  BNE  LDAF0                  ;IFEQ
LDAED:  JSR  EAUPD                  ;UPDATE EAROM EVERY 4*16 MS IF NECESSARY
LDAF0:  LDA  IN1                    ;[CS] Grab the TEST SWITCH status.
LDAF3:  AND  #MTEST                 ;[CS] Is the game in test mode?
LDAF5:  BEQ  LDA8D                  ;NEEND  EXIT LOOP IF SELF TEST SWITCH IS OFF  [CS] If not, $DA8D.

WDGTST:
LDAF7:  BNE  WDGTST                 ;GO BACK TO RESET VIA WATCH DOG RESET  [CS] If so, Infinite loop! YOW!

TABCOL:
;[CS] Data used to populate Color RAM (code at $DA7A)
LDAF9:  .byte ZWHITE, ZYELLO, ZPURPL, ZRED, ZTURQOI, ZGREEN, ZBLUE, ZBLUE

;.SBTTL SELF TEST SUBROUTINE ON CASE GOSUB

SFTJSR:
;[CS] Data to put onto stack via subroutine at $DB0F.
LDB01:  .word BADEAR-1              ;BAD EAROM
LDB03:  .word ROMREP-1              ;REPORT ON ROM, MATH BOX
LDB05:  .word SHATCH-1              ;CROSS LATCH, ALPHABET
LDB07:  .word SHYSTER-1             ;HYSTERESIS
LDB09:  .word SINTEN-1
LDB0B:  .word SCHEKR-1              ;CHECKERS
LDB0D:  .word SIGANA-1              ;SIGNATURE ANALYSIS

;------------------------------------------------------------------------------
; SFTJSE - EXECUTE APPROPRIATE SELF TEST STATE  (comment at the call)
;   (also: SSTATE)
;------------------------------------------------------------------------------
SFTJSE:
SSTATE:
LDB0F:  LDX  QSTATE
LDB11:  CPX  #<[SFTJSE-SFTJSR]
LDB13:  BCC  LDB19                  ;IFCS
LDB15:  LDX  #$02                   ;[CS] Set game mode to ROM TEST
LDB17:  STX  QSTATE                 ;[CS] screen.
LDB19:  LDA  SFTJSR+1,X
LDB1C:  PHA
LDB1D:  LDA  SFTJSR,X
LDB20:  PHA

;------------------------------------------------------------------------------
; NOOPR_DB21 - [note] Lone RTS: completes the push-address jump through SFTJSR set up just above; also used as
;   a do-nothing entry.
;------------------------------------------------------------------------------
NOOPR_DB21:
LDB21:  RTS

;------------------------------------------------------------------------------
; SIGANA - SIGNATURE ANALYSIS
;   [CS] Unknown initialization routine.
;------------------------------------------------------------------------------
SIGANA:
LDB22:  LDA  #$00                   ;CLOSE SIGNATURE WINDOW
LDB24:  STA  OUTANK                 ;SAEN23 (ENABLE FOR SA)  [CS] Reset the player 1 and 2 LEDs.
LDB27:  STA  MBSTAR                 ;CLOCK IT
LDB2A:  STA  POKEY
LDB2D:  STA  POKEY2
LDB30:  STA  EADAL                  ;[CS] Latch the EEPROM onto the bus?
LDB33:  STA  EACTL                  ;[CS] Huh? Write to mathbox status reg?
LDB36:  LDA  MSTAT                  ;[CS] Read mathbox status.
LDB39:  LDA  MYLOW
LDB3C:  LDA  MYHIGH
LDB3F:  LDA  EAIN
LDB42:  LDA  #$08                   ;****
LDB44:  STA  OUTANK                 ;OPEN SIGNATURE WINDOW
LDB47:  LDA  #$01
LDB49:  LDX  #$1F
LDB4B:  CLC
LDB4C:  STA  MBSTAR,X               ;SCAN MB MAPPING PROM
LDB4F:  ROL
LDB50:  DEX
LDB51:  BPL  LDB4C                  ;MIEND
LDB53:  LDA  #>[BONDRY+1]           ;LAH BONDRY+1
LDB55:  LDX  #<BONDRY               ;LXL BONDRY
LDB57:  JMP  VGJSRL                 ;DRAW BIG BOX ON SCREEN

;------------------------------------------------------------------------------
; BADEAR - BAD EAROM RECOVERY
;------------------------------------------------------------------------------
BADEAR:
LDB5A:  LDA  EAFLG
LDB5D:  ORA  EAREQU
LDB60:  BNE  LDB6E                  ;IFEQ  DONE ERASING?
LDB62:  JSR  REHIIN                 ;YES. TRY TO READ AGAIN
LDB65:  LDA  EABAD
;STILL BAD?
LDB68:  STA  EARCND                 ;YES. SET BAD EAROM FLAG
LDB6A:  LDA  #$02                   ;GO TO REPORT STATUS STATE  [CS] Set game mode to ROM TEST
LDB6C:  STA  QSTATE                 ;[CS] screen.
LDB6E:  RTS

;------------------------------------------------------------------------------
; SCHEKR - CROSS HATCH, INTENSITY TEST PATTERNS
;------------------------------------------------------------------------------
SCHEKR:
LDB6F:  LDA  TBHD
LDB71:  LSR
LDB72:  TAY
LDB73:  LDA  #$68
LDB75:  JSR  VGSTAT                 ;SET COLOR
LDB78:  LDX  #<CHEKER               ;LXL CHEKER
LDB7A:  LDA  #>[CHEKER+1]           ;LAH CHEKER+1
LDB7C:  BNE  JSRVGJ                 ;JSRL TO CHECKER BOARD

;------------------------------------------------------------------------------
; SINTEN - [note] Self-test picture: JSRL the intensity test pattern INTEST, then turn off all sounds.
;------------------------------------------------------------------------------
SINTEN:
LDB7E:  LDX  #<INTEST               ;LXL INTEST
LDB80:  LDA  #>[INTEST+1]           ;LAH INTEST+1
LDB82:  BNE  JSRVGJ                 ;INTENSITY TEST

;------------------------------------------------------------------------------
; SHATCH - [note] Self-test picture: JSRL the cross-hatch pattern HATCH, then turn off all sounds.
;------------------------------------------------------------------------------
SHATCH:
LDB84:  LDA  #>[HATCH+1]            ;LAH HATCH+1
LDB86:  LDX  #<HATCH                ;LXL HATCH

JSRVGJ:
LDB88:  JSR  VGJSRL                 ;CROSS HATCH & ALPHABET

NOSOUN:
LDB8B:  LDX  #$06                   ;TURN OFF ALL SUNDS
LDB8D:  LDA  #$00
LDB8F:  STA  AUDC1,X
LDB92:  STA  AUDC2,X
LDB95:  DEX
LDB96:  DEX
LDB97:  BPL  LDB8F                  ;MIEND
LDB99:  RTS

;------------------------------------------------------------------------------
; SHYSTER - SOUND TEST
;------------------------------------------------------------------------------
SHYSTER:
LDB9A:  LDA  QFRAME
LDB9C:  AND  #$3F
LDB9E:  BNE  LDBA2                  ;IFEQ
LDBA0:  INC  INDEX3                 ;UPDATE TIMER
LDBA2:  LDA  INDEX3
LDBA4:  AND  #$07
LDBA6:  TAX
LDBA7:  LDY  SNDTBL-1,X
LDBAA:  LDA  #$00
LDBAC:  STA  AUDC1,Y                ;TURN OFF OLD CHANNEL
LDBAF:  LDY  SNDTBL,X
LDBB2:  LDA  SNDFRQ,X
LDBB5:  STA  AUDF1,Y                ;TURN ON NEW CHANNEL
LDBB8:  LDA  #$A8
LDBBA:  STA  AUDC1,Y
LDBBD:  LDA  #>[HYSTER+1]           ;LAH HYSTER+1
LDBBF:  LDX  #<HYSTER               ;LXL HYSTER
LDBC1:  JSR  VGJSRL                 ;HYSTERESIS
LDBC4:  LDA  QFRAME                 ;TEST LINEAR SCALE
LDBC6:  AND  #$7F
LDBC8:  TAY                         ;RAMP LINEAR SCALE
LDBC9:  LDA  #$01                   ;BINARY SCALE
LDBCB:  JSR  VGSCAL
LDBCE:  LDA  #>[VORBOX+1]           ;LAH VORBOX+1
LDBD0:  LDX  #<VORBOX               ;LXL VORBOX
LDBD2:  JMP  VGJSRL                 ;VARY BOX SIZE
;[CS] DATA Table
LDBD5:  .byte $16                   ;PART OF SNDTBL (UNDERFLOW)

SNDTBL:
LDBD6:  .byte $00, $10, $02, $12, $04, $14, $06, $16
LDBDE:  .byte $00
LDBDF:  NOP

;------------------------------------------------------------------------------
; GETOP3 - GET OPTION SWITCH 3 INTO ACC
;   *****IT IS IMPERATIVE THAT GETOP3 BE AT OFFSET 3FF
;   [CS] Obtain the settings for D/E2 and store into scratchpad address $0037.
;   [CS] Bit 0=Switch 2   Bit 1=Switch 2  Bit 2=Switch 3  Bit 3=Switch 1
;------------------------------------------------------------------------------
GETOP3:
LDBE0:  STA  POTGO2                 ;[CS] POTGO - Rescan Pokey #2's POTs
LDBE3:  LDA  ALLPO2                 ;READ LOW 3 BITS OF 3RD SWITCH  [CS] ALLPOT - Read Pokey #2''s POTs
LDBE6:  AND  #MOPT13                ;[CS] Just grab switches 2-4 at switch D/E2 on the AUX board.
LDBE8:  STA  INDEX1                 ;[CS] Store into a memory address.
LDBEA:  STA  POTGO                  ;[CS] POTGO - Rescan Pokey #1's POTs
LDBED:  LDA  ALLPOT                 ;READ HIGH BIT OF 4 IN 3RD SWITCH  [CS] ALLPOT - Read Pokey #1's POTs.
LDBF0:  AND  #MOPTI4                ;[CS] Isolate switch #1 and shift
LDBF2:  LSR                         ;[CS] to the right so it has the
LDBF3:  LSR                         ;[CS] value of 8 when set.
LDBF4:  ORA  INDEX1                 ;MERGE  [CS] OR is into #0037,
LDBF6:  RTS                         ;[CS] Return.

;------------------------------------------------------------------------------
; ROMREP - REPORT ROM,MATHBOX,EAROM PROBLEMS
;   START MATH BOX TEST
;------------------------------------------------------------------------------
ROMREP:
LDBF7:  LDA  TEMPX                  ;DIVIDE TEMPX,Y BY TEMPX,Y
LDBF9:  BEQ  LDC19                  ;IFNE  NO DIVIDE BY 0 PLEASE
LDBFB:  STA  MXPL
LDBFE:  STA  MZLL
LDC01:  LDA  TEMPY
LDC03:  STA  MXPH
LDC06:  LDX  #$00
LDC08:  JSR  READMB                 ;DO DIVIDE
LDC0B:  CMP  #$01
LDC0D:  BNE  BADBOX                 ;BRANCH IF RESULTS NOT CORREST
LDC0F:  TYA
LDC10:  BNE  BADBOX                 ;BRANCH IF RESULTS WRONG
LDC12:  TXA
LDC13:  BPL  LDC19                  ;IFMI  TIMED OUT?

BADBOX:
;YES
LDC15:  LDA  #$FF                   ;NO
LDC17:  STA  MBCOND                 ;BAD MATHBOX
;UPDATE DIVISOR & DIVIDEND
LDC19:  LDX  #$00                   ;(USE LATER FOR SOUNDS)
LDC1B:  STX  VGBRIT
LDC1D:  INC  TEMPX
LDC1F:  BNE  LDC27                  ;IFEQ
LDC21:  INC  TEMPY
LDC23:  BPL  LDC27                  ;IFMI
LDC25:  STX  TEMPY
;END MATH BOX TEST
;START SWITCH TEST
LDC27:  STA  POTGO2
LDC2A:  LDA  ALLPO2
LDC2D:  AND  #MSTRT1|MSTRT2|MSUZA|MFIRE
LDC2F:  STA  SWSTAT
LDC31:  BEQ  LDC38                  ;IFNE  ANY SWITCHES PRESSED?
LDC33:  STA  AUDF1                  ;YES. MAKE SOUND
LDC36:  LDX  #$A4
LDC38:  STX  AUDC1
LDC3B:  LDX  #$00
LDC3D:  LDA  SWFINA
LDC3F:  BEQ  LDC47                  ;IFNE  SWITCHES PRESSED?
LDC41:  ASL
LDC42:  STA  AUDF1+2                ;YES. MAKE SOUND
LDC45:  LDX  #$A4
LDC47:  STX  AUDC1+2
LDC4A:  JSR  DOPSWI                 ;DISPLAY OPTION SWITCHES
LDC4D:  LDY  SWSTAT
LDC4F:  LDA  #$D0
LDC51:  LDX  #$F0
LDC53:  JSR  GENOPD                 ;DISPLAY SWITCHES
LDC56:  LDY  SWFINA
LDC58:  JSR  BITS2                  ;DISPLAY SWITCHES
;END SWITCH TEST
LDC5B:  LDA  OTB
LDC5D:  AND  #COCKTA
LDC5F:  BEQ  LDC7E                  ;IFNE  COCKTAIL?
LDC61:  LDA  #>[COCMSG+1]           ;LAH COCMSG+1  YES. DISPLAY A C IN CORNER
LDC63:  LDX  #<COCMSG               ;LXL COCMSG
LDC65:  JSR  VGJSRL
LDC68:  LDY  #MVINVY                ;DEFAULT NO XY FLIP
LDC6A:  LDA  SWSTAT
LDC6C:  AND  #MSTRT1|MSTRT2
LDC6E:  BEQ  LDC7E                  ;IFNE  EITHER START PRESSED?
LDC70:  EOR  #MSTRT1                ;YES.
LDC72:  BEQ  LDC78                  ;IFNE  START 2?
LDC74:  LDA  #MFLIP                 ;YES. FLIP SCREEN TO PLAYER 2
LDC76:  LDY  #MVINVX
LDC78:  STA  OUTANK
LDC7B:  STY  OUT0                   ;[CS] Invert video X axis.
LDC7E:  LDA  #>[ROMRPI+1]           ;LAH ROMRPI+1
LDC80:  LDX  #<ROMRPI               ;LXL ROMRPI
LDC82:  JSR  VGJSRL                 ;HORIZ LINE & POSITION FOR ROM REPEAT
LDC85:  LDX  #NROMS-1
LDC87:  LDA  CHKSMS,X
LDC89:  BEQ  LDCA4                  ;IFNE  BAD CHECKSUM?
LDC8B:  STA  SAVEX                  ;YES.
LDC8D:  STX  INDEX2
LDC8F:  TXA
LDC90:  JSR  VGHEX                  ;DISPLAY ROM #
LDC93:  LDY  #$F4                   ;(src: LDY I,-48./4)
LDC95:  LDX  #$F4                   ;(src: LDX I,-48./4)
LDC97:  LDA  SAVEX
LDC99:  JSR  POSDIG                 ;POSITION & DISPLAY CHECKSUM
LDC9C:  LDA  #$0C                   ;(src: LDA I,48./4)
LDC9E:  TAX
LDC9F:  JSR  VGVTR1                 ;SPACE BACK & DOWN
LDCA2:  LDX  INDEX2
LDCA4:  DEX
LDCA5:  BPL  LDC87                  ;MIEND
LDCA7:  JSR  VGCNTR
LDCAA:  LDA  #$00
LDCAC:  LDX  #$16                   ;(src: LDX I,88./4)
LDCAE:  JSR  VGVTR1                 ;POSITION BEAM
LDCB1:  LDX  #$04
LDCB3:  STX  INDEX1
LDCB5:  LDX  INDEX1
LDCB7:  LDY  #$00                   ;DEFAULT GOOD (BLANK)
LDCB9:  LDA  MBCOND,X
LDCBB:  BEQ  LDCC0                  ;IFNE  GOOD?
LDCBD:  LDY  BADNWS,X               ;NO. BAD. GET LETTER
LDCC0:  LDA  VGMSGA,Y
LDCC3:  LDX  VGMSGA+1,Y
LDCC6:  JSR  VGADD2                 ;OUTPUT LETTER
LDCC9:  DEC  INDEX1
LDCCB:  BPL  LDCB5                  ;MIEND
;DISLAY C IF COCKTAIL
LDCCD:  LDX  #$AC                   ;DRAW VECTOR TO CLOCK POSITION
LDCCF:  LDA  #$30                   ;INDICATED BY TBHD
LDCD1:  JSR  VGVTR1
LDCD4:  LDY  TBHD
LDCD6:  LDA  POTXTA,Y
LDCD9:  LDX  POTYTA,Y
LDCDC:  LDY  #$C0
LDCDE:  JMP  VGVTR

BADNWS:
LDCE1:  .byte $2E, $38, $34, $36, $1E ;M,R,P,Q,E

;------------------------------------------------------------------------------
; READMB - READ MATH BOX
;------------------------------------------------------------------------------
READMB:
LDCE6:  LDY  #$00
LDCE8:  STY  VGBRIT
LDCEA:  STY  NGAVGZ                 ;(USEFUL FORDBOOKE ONLY)
;DONE FOR SQUEEZE PURPOSES
LDCED:  STA  MZLH                   ;[CS] Mathbox stuff.
LDCF0:  STX  MZHL
LDCF3:  STY  MZHH
LDCF6:  LDX  #$10
LDCF8:  STX  MNL
LDCFB:  STX  MSZXD
LDCFE:  DEX
LDCFF:  BMI  TOOSLO
LDD01:  LDA  MSTAT                  ;[CS] Read mathbox status reg
LDD04:  BMI  LDCFE                  ;PLEND
LDD06:  LDA  MYLOW
LDD09:  LDY  MYHIGH

TOOSLO:
LDD0C:  RTS

;------------------------------------------------------------------------------
; DOPSWI - SWITCH TEST
;------------------------------------------------------------------------------
DOPSWI:
LDD0D:  JSR  VGCNTR                 ;OPTION SWITCH DISLAY
LDD10:  LDA  #$00
LDD12:  JSR  VGSCA1                 ;BIG DIGITS
LDD15:  LDA  #$E8                   ;(src: LDA I,-96./4)  SPACE OVER FOR 1ST OPTION SWITCH
LDD17:  LDY  INOP0
LDD1A:  JSR  BITS3                  ;DISPLAY 1ST OPTION SWITCH
LDD1D:  LDY  INOP1                  ;SPACE OVER FOR SECOND OPTION SWITCH
LDD20:  JSR  BITS2                  ;DISPLAY 2ND SWITCH
LDD23:  JSR  GETOP3                 ;GET OPTION SWITCH 3  [CS] Read switch D/E2 (special format) and store into $0037.
LDD26:  TAY                         ;DISPLAY

;------------------------------------------------------------------------------
; BITS2 - DISPLAY SWITCHES  (comment at the call)
;   DISPLAY 2ND SWITCH  (another call)
;------------------------------------------------------------------------------
BITS2:
LDD27:  LDA  #$D0                   ;(src: LDA I,-192./4)

;------------------------------------------------------------------------------
; BITS3 - DISPLAY 1ST OPTION SWITCH  (comment at the call)
;------------------------------------------------------------------------------
BITS3:
LDD29:  LDX  #$F8

;------------------------------------------------------------------------------
; GENOPD - DISPLAY SWITCHES  (comment at the call)
;------------------------------------------------------------------------------
GENOPD:
LDD2B:  STY  SAVEX
LDD2D:  JSR  VGVTR1                 ;GENERAL OPTIONS DISPLAY ROUTINE
LDD30:  LDX  #$07                   ;ACC=OPTION BYTE
LDD32:  STX  INDEX1
LDD34:  ASL  SAVEX
LDD36:  LDA  #$00
LDD38:  ROL
LDD39:  JSR  VGHEX                  ;DISPLAY 0 OR 1
LDD3C:  DEC  INDEX1
LDD3E:  BPL  LDD34                  ;MIEND
LDD40:  RTS

;------------------------------------------------------------------------------
; DBOOKE - DISPLAY LIST OF TRIPLE PRECISION #S
;   SET UP TO DISLAY LIST OF #S
;------------------------------------------------------------------------------
DBOOKE:
LDD41:  LDA  NGAM2L
LDD44:  ASL
LDD45:  STA  TEMP0
LDD47:  LDA  NGAM2H
LDD4A:  ROL
LDD4B:  STA  TEMP1
LDD4D:  LDA  NGAMIL
LDD50:  CLC
LDD51:  ADC  TEMP0
LDD53:  STA  MXPL
LDD56:  STA  TEMP0
LDD58:  LDA  NGAMIH
LDD5B:  ADC  TEMP1
LDD5D:  STA  MXPH
LDD60:  ORA  TEMP0
LDD62:  BNE  LDD69                  ;IFEQ  DIVIDE BY 0?
LDD64:  LDA  #$01                   ;YES. MAKE IT 1
LDD66:  STA  MXPL                   ;[CS] More mathbox stuff.
LDD69:  LDA  SECOPL
LDD6C:  STA  MZLL
LDD6F:  LDA  SECOPM                 ;GET TIME FOR DIVIDEND
LDD72:  LDX  SECOPH
LDD75:  JSR  READMB                 ;DO DIVIDE
LDD78:  STA  NGAVGL                 ;RESULTS
LDD7B:  STY  NGAVGH
LDD7E:  LDA  #>[BOKLIT+1]           ;LAH BOKLIT+1  BOOKKEEPING LITERALS
LDD80:  LDX  #<BOKLIT               ;LXL BOKLIT
LDD82:  JSR  VGJSRL
LDD85:  LDA  #BOOKKS&$FF            ;POINT TO 1ST #
LDD87:  STA  INDYLO
LDD89:  LDA  #BOOKKS/$100
LDD8B:  STA  INDYHI
;LDA I,4 ;DISPLAY N NUMBERS
LDD8D:  STA  INDEX1
;JSR TPLIST ;DISPLAY LIST (FALL IN)
;RTS
;INPUT:INDYLO, HI=BASE ADDRESS OF LIST OF TRIPLE PRECISION #S
;INDEX1=# OF #'S TO DISLAY -1
LDD8F:  LDY  #$00                   ;SET UP BCD HEX IN TEMP0,1,2
LDD91:  STY  MTEMP
LDD93:  STY  MTEMP+1
LDD95:  STY  MTEMP+2
LDD97:  STY  MTEMP+3
LDD99:  LDA  (INDYLO),Y
LDD9B:  STA  PXL
LDD9D:  INC  INDYLO
LDD9F:  LDA  (INDYLO),Y
LDDA1:  STA  PXL+1
LDDA3:  INC  INDYLO
LDDA5:  LDA  (INDYLO),Y
LDDA7:  STA  PXL+2
LDDA9:  INC  INDYLO
;CONVERT PXL(3) FROM HEX TO BCD
;IN TEMP0(4)
LDDAB:  SED
LDDAC:  LDY  #$17
LDDAE:  STY  INDEX2
LDDB0:  ROL  PXL
LDDB2:  ROL  PXL+1
LDDB4:  ROL  PXL+2
LDDB6:  LDY  #$03
LDDB8:  LDX  #$00
LDDBA:  LDA  MTEMP,X
LDDBC:  ADC  MTEMP,X
LDDBE:  STA  MTEMP,X
LDDC0:  INX
LDDC1:  DEY
LDDC2:  BPL  LDDBA                  ;MIEND
LDDC4:  DEC  INDEX2
LDDC6:  BPL  LDDB0                  ;MIEND
LDDC8:  CLD
LDDC9:  LDA  #MTEMP
LDDCB:  LDY  #$04
LDDCD:  JSR  DIGTYS                 ;OUTPUT #
LDDD0:  LDA  #$D0
LDDD2:  LDX  #$F8
LDDD4:  JSR  VGVTR1                 ;POSITION FOR NEXT # DISPLAY
LDDD7:  DEC  INDEX1
LDDD9:  BPL  LDD8F                  ;MIEND
LDDDB:  RTS

CHKSMB:
LDDDC:  .byte QCHKSB

;==============================================================================
; MODULE ALEARO   ALEARO.MAC
;   EAROM: high scores, initials and bookkeeping read/write/erase state
;   machine.
;==============================================================================

;.SBTTL TABLE FOR RAM-EAROM TRANSFER

TEAX:            ;EAROM OFFSET OF LOWEST BYTE IN GROUP
LDDDD:  .byte $00                   ;ROML 9  INITIALS

TEACNT:
LDDDE:  .byte $09
LDDDF:  .byte $0A, $15              ;ROML 11.  HI SCORES & GAME PLAY OPTIONS
LDDE1:  .byte $16, $22              ;ROML BOOKKE-BOOKKS  BOOKKEEPING

TEASRL:
LDDE3:  .word INITAL+15
.alias TEASRH           $DDE4    ;inside the line at LDDE3 (LDDE3+1)
LDDE5:  .word HSCORL+15
LDDE7:  .word BOOKKS

;------------------------------------------------------------------------------
; EAZBOO - EAROM APPLICATIONS
;   ZERO EAROM
;------------------------------------------------------------------------------
EAZBOO:
LDDE9:  LDA  #$04
LDDEB:  BNE  GENZER                 ;ZERO BOOKKEEPING ONLY

;------------------------------------------------------------------------------
; EAZHIS - ZERO HI SCORES OPTION  (comment at the call)
;------------------------------------------------------------------------------
EAZHIS:
LDDED:  LDA  #$03
LDDEF:  BNE  GENZER                 ;ZERO HI SCORES/INITIALS ONLY

;------------------------------------------------------------------------------
; EAZERO - YES. ERASE EAROM  (comment at the call)
;------------------------------------------------------------------------------
EAZERO:
LDDF1:  LDA  #$07

GENZER:
LDDF3:  LDY  #$FF                   ;REQUEST ZERO EAROM
LDDF5:  BNE  GENREQ                 ;REQUEST ALL BATCHES

;------------------------------------------------------------------------------
; WRHIIN - REQUEST WRITE
;------------------------------------------------------------------------------
WRHIIN:
LDDF7:  LDA  #$03                   ;WRITE HIGH SCORES & INITIALS
LDDF9:  BNE  NOZERO

;------------------------------------------------------------------------------
; WRBOOK - WRITE OUT BOOKKEEPING INFO  (comment at the call)
;------------------------------------------------------------------------------
WRBOOK:
LDDFB:  LDA  #$04                   ;REQUEST BOOKKEEPING UPDATE

NOZERO:
LDDFD:  LDY  #$00

GENREQ:
LDDFF:  STY  EAZFLG                 ;DO NOT ZERO EAROM
LDE02:  PHA
LDE03:  ORA  EAREQU
LDE06:  STA  EAREQU
LDE09:  PLA
LDE0A:  ORA  EARWRQ
LDE0D:  STA  EARWRQ
LDE10:  RTS

;------------------------------------------------------------------------------
; REHIIN - NO. READ EAROM  (comment at the call)
;   READ IN EAROM  (another call)
;   YES. TRY TO READ AGAIN  (another call)
;------------------------------------------------------------------------------
REHIIN:
LDE11:  LDA  #$07                   ;READ IN EVERYTHING
LDE13:  STA  EAREQU
LDE16:  LDA  #$00
LDE18:  STA  EARWRQ

;------------------------------------------------------------------------------
; EAUPD - EAROM IO MAINLINE
;   JMP EAUPD ;GO GET IT NOW.
;   INPUT:EAFLG:0=NO ACTIVITY;80=ERASE;40=WRITE;20=READ
;   EAX:INDEX INTO EADAL OF LOC TO ACCESS IN EAROM
;   EABC:OFFSET FROM @EASRCE OF RAM DATA TO ACCESS
;   EACNT:EAROM OFFSET OF LAST BYTE TO MODIFY (STOP WHEN EAX>EACNT)
;   OUTPUT:EAROM ERASED, WRITTEN TOO, OR READ
;------------------------------------------------------------------------------
EAUPD:
LDE1B:  LDA  EAFLG
LDE1E:  BNE  LDE6B                  ;IFEQ  EA ACTIVITY?
LDE20:  LDA  EAREQU                 ;NO.
LDE23:  BEQ  LDE6B                  ;IFNE  ANY REQUESTED?
LDE25:  LDX  #$00                   ;YES
LDE27:  STX  EABC                   ;ZERO SOURCE INDEX
LDE2A:  STX  EACS                   ;ZERO CHECKSUM
LDE2D:  STX  EASEL                  ;ZERO SELECT BIT
LDE30:  LDX  #$08
LDE32:  SEC
LDE33:  ROR  EASEL
LDE36:  ASL
LDE37:  DEX
LDE38:  BCC  LDE33                  ;CSEND  EXIT WHEN SET BIT IS FOUND
LDE3A:  LDY  #EAERAS                ;DEFAULT TO ERASE/WRITE
LDE3C:  LDA  EASEL
LDE3F:  AND  EARWRQ
LDE42:  BNE  LDE46                  ;IFEQ  READ OR ERASE/WRITE?
LDE44:  LDY  #EAREAD                ;READ
LDE46:  STY  EAFLG                  ;SAVE REQUEST
LDE49:  LDA  EASEL
LDE4C:  EOR  EAREQU
LDE4F:  STA  EAREQU                 ;TURN OFF REQUEST BIT
LDE52:  TXA
LDE53:  ASL
LDE54:  TAX
LDE55:  LDA  TEAX,X                 ;SET UP PARAMETERS FOR EAROM WRITE
LDE58:  STA  EAX
LDE5B:  LDA  TEACNT,X
LDE5E:  STA  EACNT
LDE61:  LDA  TEASRL,X
LDE64:  STA  EASRCE
LDE66:  LDA  TEASRH,X
LDE69:  STA  EASRCE+1
LDE6B:  LDY  #$00                   ;DESELECT CHIP
LDE6D:  STY  EACTL
LDE70:  LDA  EAFLG
LDE73:  BNE  LDE76                  ;IFEQ  ANY ACTIVITY?
LDE75:  RTS                         ;NO. EXIT
LDE76:  LDY  EABC                   ;YES.
LDE79:  LDX  EAX
LDE7C:  ASL
LDE7D:  BCC  LDE8C                  ;IFCS  YES. R/W OR ERASE?
;ERASE
LDE7F:  STA  EADAL,X                ;STORE ADDRESS  [CS] Store something on the EEPROM?
LDE82:  LDA  #EAWRIT                ;REQUEST WRITE
LDE84:  STA  EAFLG
LDE87:  LDY  #EAC1+EAC2+EACE        ;ERASE & SELECT CHIP
LDE89:  CLV                         ;ELSE
LDE8A:  BVC  LDEFF
LDE8C:  BPL  LDEB3                  ;IFMI  NO. READ OR WRITE?
LDE8E:  LDA  #EAERAS                ;WRITE A BYTE
LDE90:  STA  EAFLG                  ;REQUEST ERASE FOR NEXT BYTE
LDE93:  LDA  EAZFLG
LDE96:  BEQ  LDE9C                  ;IFNE  ZERO EAROM?
LDE98:  LDA  #$00                   ;YES.
LDE9A:  STA  (EASRCE),Y             ;CLEAR RAM TOO
LDE9C:  LDA  (EASRCE),Y             ;GET RAM DATA (DEFAULT)
LDE9E:  CPX  EACNT
LDEA1:  BCC  LDEAB                  ;IFCS
LDEA3:  LDA  #$00                   ;ALL DONE. SET DONE FLAG
LDEA5:  STA  EAFLG
LDEA8:  LDA  EACS                   ;GET CHECKSUM
LDEAB:  STA  EADAL,X                ;WRITE DATA  [CS] Load something on the EEPROM?
LDEAE:  LDY  #EAC1+EACE             ;SELECT WRITE MODE & CHIP SELECT
LDEB0:  CLV                         ;ELSE
LDEB1:  BVC  LDEF2
;READ
LDEB3:  LDA  #EACE
LDEB5:  STA  EACTL                  ;SELECT CHIP AND READ FUNCTION  [CS] Work the EEPROM control
LDEB8:  STA  EADAL,X                ;SELECT ADDRESS  [CS] Load something on the EEPROM?
LDEBB:  LDA  #EACE+EACK             ;[CS] Work the eeprom control.
LDEBD:  STA  EACTL                  ;SELECT CHIP & CLOCK & READ
LDEC0:  NOP                         ;[CS] Wait.
LDEC1:  LDA  #EACE                  ;[CS] Work the EEPROM control.
LDEC3:  STA  EACTL                  ;SELECT CHIP
LDEC6:  CPX  EACNT
LDEC9:  LDA  EAIN                   ;READ EAROM  [CS] Read the EEPROM read control.
LDECC:  BCC  LDEEE                  ;IFCS  CHECKSUM?
;YES
LDECE:  EOR  EACS                   ;MATCH CHECKSUM?
LDED1:  BEQ  LDEE6                  ;IFNE
LDED3:  LDA  #$00                   ;NO.
LDED5:  LDY  EABC
LDED8:  STA  (EASRCE),Y
LDEDA:  DEY
LDEDB:  BPL  LDED8                  ;MIEND
LDEDD:  LDA  EASEL                  ;SET BAD FLAG
LDEE0:  ORA  EABAD
LDEE3:  STA  EABAD
LDEE6:  LDA  #$00
LDEE8:  STA  EAFLG                  ;ALL DONE
LDEEB:  CLV                         ;ELSE
LDEEC:  BVC  LDEF0
;NO. RAW DATA
LDEEE:  STA  (EASRCE),Y             ;SAVE DATA IN RAM
LDEF0:  LDY  #$00                   ;DESELECT
LDEF2:  CLC
LDEF3:  ADC  EACS
LDEF6:  STA  EACS                   ;UPDATE CHECKSUM
LDEF9:  INC  EABC
LDEFC:  INC  EAX
LDEFF:  STY  EACTL
LDF02:  TYA
LDF03:  BNE  LDF08                  ;IFEQ  READ?
LDF05:  JMP  EAUPD                  ;YES. DO ALL READS AT ONCE
LDF08:  RTS

;==============================================================================
; MODULE ALVGUT   ALVGUT.MAC
;   Vector generator utilities: VGRTSL, VGJSRL, VGVCTR, VGSCAL, VGHEX, VGCNTR
;   ...
;==============================================================================

;------------------------------------------------------------------------------
; VGRTSL - VGRTSL - ADD RTSL TO VECTOR LIST
;   ZERO PAGE GLOBALS
;   EXTERNAL ENTRY POINTS
;   ENTRY POINTS
;   DATE INITIATED: 10-OCT-79
;   PROJECT CHARGE #: 23803
;   DISK #: 105, B46
;   HARWARE REQUIREMENTS: ANALOG AUTO-NORMALIZING VECTOR GENERATOR
;   MEMORY REQUIREMENTS:
;   NOT APPLICABLE - SUBROUTINE
;   INTERRUPT REQUIREMENTS:
;   NOT APPLICABLE - SUBROUTINE
;   ASSEMBLY COMMAND STRING:
;   R MAC65
;   DX1:VGUT,DK1:VGUT=DX1:VGUT/C
;   LINK COMMAND STRING:
;   NOT APPLICABLE - SUBROUTINE
;   PROGRAM DESCRIPTION:
;   A SET OF UTILITY ROUTINES FOR GENERATING
;   VECTORS USING THE ANALOG AUTO-NORMALIZING VECTOR GENERATOR.
;   ZERO PAGE GLOBALS REQUIRED:
;   VGLIST: THIS 2 BYTE VARIABLE CONTAINS THE CURRENT VECTOR GENERATOR RAM
;   ADDRESS USED TO BUILD INSTRUCTIONS FOR THE VECTOR GENERATOR.
;   IT SHOULD BE INITIALIZED BEFORE CALLING ANY OF THESE ROUTINES.
;   XCOMP: THIS 4 BYTE VARIABLE IS USED TO CONTAIN THE X (LSB,MSB) COMPONENT
;   AND Y (LSB,MSB) COMPONENT USED IN SEVERAL OF THE VECTOR INSTRUC-
;   TIONS. FOR THE VGVCTR ROUTINE THESE FIELDS ARE SIGNED 2'S
;   COMPLEMENT NUMBERS.
;   VGBRIT: THIS VARIABLE IS USED BY VGVCTR AND VGSTAT TO GENERATE VECTORS
;   WITH THE GIVEN BRIGHTNESS. IT'S VALUES ARE 0,10,20,30,
;   40,...F0 WHERE 0 IS OFF AND F0 IS MAX BRIGHTNESS.
;   IN THE VECTOR INSTRUCTIONS ONLY THE UPPER 3 BITS IS USED
;   IF Z=1 IN THE VECTOR INSTRUCTIONS THEN THE LAST NON-ZERO
;   Z IS USED.
;   EXTERNAL ENTRY POINTS REQUIRED:
;   VGMSGA: THIS ENTRY POINT PROVIDES JSRL INSTRUCTIONS TO THE CHAR.X
;   ROUTINES. THIS ENTRY POINT IS PROVIDED BY VECAN.MAC.
;   THESE ROUTINES WERE WRITTEN TO PROVIDE PROGRAMMERS USING LYLE RAIN'S VECTOR
;   GENERATOR A MEANS OF:
;   1) DYNAMICALLY GENERATING VECTORS JUST AS VECMAC ALLOWS STATIC
;   GENERATION OF VECTORS.
;   2) DISPLAY NUMBERS WITH OR WITHOUT ZERO SUPPRESSION.
;   EXAMPLE 1:
;   LDA I,VECRAM&0FF ;INITIALIZE VECTOR RAM POINTER
;   STA VGLIST
;   LDA I,VECRAM/100
;   STA VGLIST+1
;   JSR CENTER ;CENTER BEAM IN MIDDLE OF SCREEN.
;   LDA I,256./4 ;POSITION BEAM AT (256,256)
;   LDX I,256./4
;   JSR VGSVTR
;   ETC....
;   EXAMPLE 2:
;   LDA PLAYER ;DISPLAY PLAYER NUMBER
;   JSR VGHEX ;REMEMBER BEAM MUST BE POSITIONED CORRECTLY
;   LDA SCORE1 ;MSB OF SCORE
;   LSR
;   LSR
;   LSR
;   LSR
;   SEC
;   JSR VGHEXZ ;DISPLAY UPPER DIGIT WITH ZERO SUPPRESSION
;   LDA SCORE1
;   JSR VGHEXZ ;DISPLAY SECOND DIGIT WITH ZERO SUPPRESSION
;   LDA SCORE0 ;LSB OF SCORE
;   PHP ;SAVE ZERO SUPPRESSION FLAG
;   LSR
;   LSR
;   LSR
;   LSR
;   PLP
;   JSR VGHEXZ ;DISPLAY THIRD DIGIT WITH ZERO SUPPRESSION
;   LDA SCORE0
;   JSR VGHEX ;DISPLAY LAST DIGIT
;   ETC....
;   VGRTSL - ADD RTSL TO VECTOR LIST
;   ENTRY (VGLIST,VGLIST+1) = VECTOR LIST ADDRESS
;   EXIT (VGLIST,VGLIST+1) = NEW VECTOR LIST ADDRESS
;   USES A,Y,(VGLIST,VGLIST+1)
;------------------------------------------------------------------------------
VGRTSL:
LDF09:  LDA  #$C0                   ;DXXX IS RTSL
LDF0B:  BNE  VGHAL1                 ;ALWASY

;------------------------------------------------------------------------------
; VGHALT - VGHALT - CENTER AND HALT VECTOR GENERATOR
;   VHALT - CENTER AND HALT VECTOR GENERATOR
;   ENTRY (VGLIST,VGLIST+1) = VECTOR LIST ADDRESS
;   EXIT (VGLIST,VGLIST+1) = NEW VECTOR LIST ADDRESS
;   USES A,X,Y,(VGLIST,VGLIST+1)
;------------------------------------------------------------------------------
VGHALT:
LDF0D:  JSR  VGCNTR                 ;CENTER FIRST
LDF10:  LDA  #$20                   ;BXXX IS HALT

VGHAL1:
LDF12:  LDY  #$00
LDF14:  STA  (VGLIST),Y
LDF16:  JMP  VGWAI1                 ;ADD LAST BYTE

;------------------------------------------------------------------------------
; VGHEXZ - VGHEXZ - DISPLAY DIGIT WITH ZERO SUPPRESSION
;   VGHEXZ - DISPLAY DIGIT WITH ZERO SUPPRESSION
;   THIS ROUTINE WILL DISPLAY A DIGIT USING THE DEFAULT CHARACTER SIZE.
;   NO ATTEMPT IS MADE TO USE THE VARIABLE VGSIZE.
;   ENTRY (A) = LOWER 4 BITS TO BE DISPLAYED
;   (C) = CARRY CLEAR IF NO ZERO SUPPRESSION
;   EXIT (C) = CARRY CLEARED IF NON-ZERO DIGIT DISPLAYED
;   USES A,X,Y,(VGLIST,VGLIST+1)
;------------------------------------------------------------------------------
VGHEXZ:
LDF19:  BCC  VGHEX                  ;IF NO ZERO SUPPRESSION
LDF1B:  AND  #$0F
LDF1D:  BEQ  VGHEX1                 ;LEAVE C SET

;------------------------------------------------------------------------------
; VGHEX - VGHEX - DISPLAY DIGIT
;   VGHEX - DISPLAY DIGIT
;   THIS ROUTINE WILL DISPLAY A DIGIT USING THE DEFAULT CHARACTER SIZE.
;   NO ATTEMPT IS MADE TO USE THE VARIABLE VGSIZE.
;   ENTRY (VGLIST,VGLIST+1) = VECTOR LIST ADDRESS
;   (A) = LOWER 4 BITS TO BE DISPLAYED
;   EXIT (VGLIST,VGLIST+1) = NEW VECTOR LIST ADDRESS
;   (C) = CARRY IS CLEAR
;   USES A,X,Y,(VGLIST,VGLIST+1)
;------------------------------------------------------------------------------
VGHEX:
LDF1F:  AND  #$0F
LDF21:  CLC
LDF22:  ADC  #$01                   ;CLEARS C BIT

VGHEX1:
LDF24:  PHP                         ;SAVE C FLAG
LDF25:  ASL
LDF26:  LDY  #$00
LDF28:  TAX
LDF29:  LDA  VGMSGA,X
LDF2C:  STA  (VGLIST),Y
LDF2E:  LDA  VGMSGA+1,X             ;COPY JSRL TO CHARACTER ROUTINE
LDF31:  INY
LDF32:  STA  (VGLIST),Y
LDF34:  JSR  VGADD                  ;UPDATE VECTOR LIST POINTER
LDF37:  PLP                         ;RESTORE C FLAG
LDF38:  RTS

;------------------------------------------------------------------------------
; VGJSRL - VGJSRL - ADD JSRL TO VECTOR LIST
;   VGJSRL - ADD JSRL TO VECTOR LIST
;   ENTRY (VGLIST,VGLIST+1) = VECTOR LIST ADDRESS
;   (A) = MSB OF ADDRESS
;   (X) = LSB OF ADDRESS
;   EXIT (VGLIST,VGLIST+1) = NEW VECTOR LIST ADDRESS
;   USES A,Y,(VGLIST,VGLIST+1)
;------------------------------------------------------------------------------
VGJSRL:
LDF39:  LSR
LDF3A:  AND  #$0F                   ;BASE ADDRESS IS RELATIVE
LDF3C:  ORA  #$A0
LDF3E:  LDY  #$01
LDF40:  STA  (VGLIST),Y             ;SAVE MSB + OPCODE
LDF42:  DEY
LDF43:  TXA
LDF44:  ROR
LDF45:  STA  (VGLIST),Y             ;LSB OF ADDRESS
LDF47:  INY
LDF48:  BNE  VGADD                  ;UPDATE VECTOR POINTER

;------------------------------------------------------------------------------
; VGSTA1 - VGSTAT - SET VECTOR GENERATOR STATUS
;   VGSTAT - SET VECTOR GENERATOR STATUS
;   ENTRY (A)=HI/LOW AND IN/OUT FLAGS AND ENABLE (0,1,2,3,OR 4)
;   (Y)=VECTOR BRIGHTNESS (0,10,20,...,F0)
;   (VGLIST,VGLIST+1)=VECTOR LIST ADDRESS
;   EXIT (VGLIST,VGLIST+1)=NEW VECTOR LIST ADDRESS
;   USES A,X,Y,(VGLIST,VGLIST+1)
;------------------------------------------------------------------------------
VGSTA1:
LDF4A:  LDY  VGBRIT

;------------------------------------------------------------------------------
; VGSTAT - SET COLOR  (comment at the calls)
;   PLACE INTO MAINLINE  (another call)
;   SET BEAM COLOR  (another call)
;------------------------------------------------------------------------------
VGSTAT:
LDF4C:  ORA  #$60
LDF4E:  TAX                         ;OPCODE + LIMIT BITS
LDF4F:  TYA
LDF50:  JMP  VGADD2                 ;ADD 2 BYTES TO VECTOR LIST

;------------------------------------------------------------------------------
; VGCNTR - VGCNTR - CENTER BEAM IN MIDDLE OF SCREEN
;   VGCNTR - CENTER BEAM IN MIDDLE OF SCREEN
;   ENTRY (VGLIST,VGLIST+1)=VECTOR LIST ADDRESS
;   USES A,X,Y,(VGLIST,VGLIST+1)
;------------------------------------------------------------------------------
VGCNTR:
LDF53:  LDA  #$40                   ;TIMER + SCALE
LDF55:  LDX  #$80                   ;OPCODE

;------------------------------------------------------------------------------
; VGADD2 - VGADD2 - ADD 2 WORDS TO VECTOR LIST
;   JMP VGADD2
;   VGADD2 - ADD 2 WORDS TO VECTOR LIST
;   ENTRY (A)=FIRST BYTE
;   (X)=SECOND BYTE
;   EXIT (VGLIST,VGLIST+1)=NEW VECTOR LIST POINTER
;   USES A,Y,(VGLIST,VGLIST+1)
;------------------------------------------------------------------------------
VGADD2:
LDF57:  LDY  #$00

VGADD3:
LDF59:  STA  (VGLIST),Y             ;LSB BYTE
LDF5B:  INY
LDF5C:  TXA
LDF5D:  STA  (VGLIST),Y             ;MSB BYTE

;------------------------------------------------------------------------------
; VGADD - VGADD - ADD Y+1 TO VECTOR ADDRESS
;   JMP VGADD ;UPDATE VECTOR LIST POINTER
;   VGADD - ADD Y+1 TO VECTOR LIST ADDRESS
;   ENTRY (VGLIST,VGLIST+1) = VECTOR LIST ADDRESS
;   (Y) = VALUE+1 TO BE ADDED TO VECTOR LIST
;   EXIT (VGLIST,VGLIST+1) = NEW VECTOR LIST ADDRESS
;   USES A,(VGLIST,VGLIST+1)
;------------------------------------------------------------------------------
VGADD:
LDF5F:  TYA                         ;ADD 1+(Y) TO VGLIST
LDF60:  SEC
LDF61:  ADC  VGLIST
LDF63:  STA  VGLIST
LDF65:  BCC  VGADD_10
LDF67:  INC  VGLIST+1

VGADD_10:
LDF69:  RTS

;------------------------------------------------------------------------------
; VGSCA1 - VGSCAL-SET VECTOR GENERATOR SCALE
;   VGSCAL-SET VECTOR GENERATOR SCALE
;   ENTRY (Y)=LINEAR SCALE FACTOR (0=FULL SIZE, FF=1/256 SIZE)
;   (A)=POWER OF 2 SCALE FACTOR (0=FULL SIZE,1=1/2 SIZE,...)
;   (VGLIST,VGLIST+1)=VECTOR LIST ADDRESS
;   EXIT (VGLIST,VGLIST+1)=NEW VECTOR LIST ADDRESS
;   USES A,X,Y(VGLIST,VGLIST+1)
;------------------------------------------------------------------------------
VGSCA1:
LDF6A:  LDY  #$00                   ;USE FULL SIZE

;------------------------------------------------------------------------------
; VGSCAL - Y=LINEAR;ACC=BINARY;PLACE INTO MAINLINE  (comment at the call)
;   REDUCE SCALE BY APPROX. 1/16.  (another call)
;------------------------------------------------------------------------------
VGSCAL:
LDF6C:  ORA  #$70                   ;OPCODE + SCALE SIZE
LDF6E:  TAX
LDF6F:  TYA
LDF70:  JMP  VGADD2                 ;ADD 2 BYTES TO VECTOR LIST

;------------------------------------------------------------------------------
; VGVTR - VGVTR - SHORT FORM VGVCTR CALL
;   VGVTR - SHORT FORM VGVCTR CALL
;   ENTRY (VGLIST,VGLIST+1) = VECTOR LIST ADDRESS
;   (A) = CHANGE IN X/4 (-80 TO +7F)
;   (X) = CHANGE IN Y/4 (-80 TO +7F)
;   (Y) = VECTOR BRIGHTNESS (0,10,20,...,F0)
;   EXIT (VGLIST,VGLIST+1) = NEW VECTOR LIST ADDRESS
;   (VGBRIT)=NEW VECTOR BRIGHTNESS
;   USES A,X,Y,(VGLIST,VGLIST+1),(XCOMP,XCOMP+3)
;------------------------------------------------------------------------------
VGVTR:
LDF73:  STY  VGBRIT

;------------------------------------------------------------------------------
; VGVTR1 - POSITION BEAM  (comment at the calls)
;   -GET INTO POSITION  (another call)
;   C POSITION BEAM (USE VGBRIT)  (another call)
;------------------------------------------------------------------------------
VGVTR1:
LDF75:  LDY  #$00
LDF77:  ASL
LDF78:  BCC  VGVTR1_10              ;SIGN EXTEND
LDF7A:  DEY                         ;Y=-1

VGVTR1_10:
LDF7B:  STY  XCOMP+1                ;WITH SCALE=0 A=1 MEANS=DOTS ON XY
LDF7D:  ASL
LDF7E:  ROL  XCOMP+1
LDF80:  STA  XCOMP
LDF82:  TXA                         ;WRITE Y VALUE TO XCOMP+2,XCOMP+3
LDF83:  ASL
LDF84:  LDY  #$00
LDF86:  BCC  VGVTR1_20              ;SIGN EXTEND
LDF88:  DEY                         ;Y=-1

VGVTR1_20:
LDF89:  STY  XCOMP+3
LDF8B:  ASL
LDF8C:  ROL  XCOMP+3
LDF8E:  STA  XCOMP+2

VGVTR2:
LDF90:  LDX  #XCOMP

;------------------------------------------------------------------------------
; VGVCTR - VGVCTR - ADD VECTOR TO VECTOR LIST
;   JMP VGVCTR ;GENERATE VECTOR
;   VGVCTR - ADD VECTOR TO VECTOR LIST
;   NOTE: IF THE NUMBERS GIVEN ARE MORE THAN
;   15 BITS IN SIGNIFICANCE THEN AN
;   INCORRECT VECTOR WILL BE GENERATED
;   ENTRY (X) =4 ZERO PAGE LOCN. CONTAINING (X LSB,X MSB,Y MSB,Y LSB)
;   (VGLIST,VGLIST+1) = VECTOR LIST ADDRESS
;   (VGBRIT)=VECTOR BRIGHTNESS (ONLY THE UPPER 3 BITS ARE USED)
;   EXIT (VGLIST,VGLIST+1) = NEW VECTOR LIST ADDRESS
;   USES A,X,Y,(VGLIST,VGLIST+1)
;------------------------------------------------------------------------------
VGVCTR:
LDF92:  LDY  #$00
LDF94:  LDA  $02,X                  ;Y LSB
LDF96:  STA  (VGLIST),Y
LDF98:  LDA  $03,X                  ;Y MSB
LDF9A:  AND  #$1F                   ;CLEAR SIGN EXTENSION
LDF9C:  INY
LDF9D:  STA  (VGLIST),Y
LDF9F:  LDA  $00,X                  ;X LSB
LDFA1:  INY
LDFA2:  STA  (VGLIST),Y
LDFA4:  LDA  $01,X                  ;X MSB
LDFA6:  EOR  VGBRIT
LDFA8:  AND  #$1F                   ;CLEAR SIGN EXTENSION
LDFAA:  EOR  VGBRIT                 ;COMBINE UPPER 3 BITS OF VGBRIT WITH X MSB

VGWAI1:
LDFAC:  INY
LDFAD:  STA  (VGLIST),Y             ;SET INTENSITY
LDFAF:  BNE  VGADD                  ;ALWAYS - UPDATE VGLIST POINTER

;------------------------------------------------------------------------------
; DIGTYS - DISPLAY DIGITS
;   DIGITS - DISPLAY 2Y DIGIT NUMBER
;   ENTRY (C)=CARRY SET FOR ZERO SUPPRESSION
;   (A)=ADDRESS OF (Y) ZERO PAGE LOCATIONS CONTAINING NUMBER (LSB TO MSB)
;   (Y)=NUMBER OF ZERO PAGE LOCATIONS TO USE (1 TO 256).
;------------------------------------------------------------------------------
DIGTYS:
LDFB1:  SEC

DIGITS:
LDFB2:  PHP                         ;SAVE INPUT PARAMETERS
LDFB3:  DEY
LDFB4:  STY  ZPNLOC
LDFB6:  CLC
LDFB7:  ADC  ZPNLOC
LDFB9:  PLP
LDFBA:  TAX                         ;MSB OF DIGITS
LDFBB:  PHP
LDFBC:  STX  ZPOFFS                 ;SAVE DIGIT ZP OFFSET
LDFBE:  LDA  $00,X
LDFC0:  LSR
LDFC1:  LSR
LDFC2:  LSR
LDFC3:  LSR
LDFC4:  PLP
LDFC5:  JSR  VGHEXZ                 ;FIRST DIGIT
LDFC8:  LDA  ZPNLOC
LDFCA:  BNE  LDFCD                  ;IFEQ
LDFCC:  CLC                         ;DISPLAY LAST DIGIT (EVEN 0)
LDFCD:  LDX  ZPOFFS
LDFCF:  LDA  $00,X
LDFD1:  JSR  VGHEXZ                 ;SECOND DIGIT
LDFD4:  LDX  ZPOFFS
LDFD6:  DEX
LDFD7:  DEC  ZPNLOC
LDFD9:  BPL  LDFBB                  ;MIEND  LOOP FOR EACH SET OF DIGITS
LDFDB:  RTS

;==============================================================================
; MODULE ALTES2 (continued)   ALTES2.MAC
;   Self-test / diagnostics, RESET (power-on) code, RAM/ROM/EAROM tests,
;   bookkeeping display, switch/sound tests.
;==============================================================================

;.SBTTL DISPLAY LIST OF TRIPLE PRECISION #S

SNDFRQ:
LDFDC:  .word $1010, $4040, $9090, $FFFF

POTYTA:
LDFE4:  .word $0C00, $1E16

POTXTA:
LDFE8:  .word $1E20, $0C16, $F400, $E2EA
LDFF0:  .word $E2E0, $F4EA, $0C00, $1E16
LDFF8:  .byte $00, $00              ;not assembled by any source statement (ROM fill)

;==============================================================================
; MODULE ALHAR2 (continued)   ALHAR2.MAC
;   IRQ handler: watchdog, frame timer, switch debounce, spinner, VG restart;
;   checksum equates and the 6502 vectors.
;==============================================================================

;.SBTTL OUTPUTS
LDFFA:  .word IRQ, RESET, IRQ       ;[CS] This is the non-maskable interrupt vector. $D704. This is the reset vector. When the CPU powers up, it goes to this address to find the location of the first instruction to execute, at $D93F. This is the maskable interrupt vector. $D704.

;==============================================================================
; $E000-$FFFF: no ROM of its own.  The address decoder mirrors the top program
; ROM there (4K sets: $D000-$DFFF at $F000; 2K sets: $D800-$DFFF at $F800; the
; Commented Source describes $C000-$DFFF appearing at $E000-$FFFF), so the 6502
; vectors read at $FFFA-$FFFF are the .VCTRS words at $DFFA above:
;   NMI $FFFA -> IRQ ($D704)   RESET $FFFC -> RESET ($D93F)   IRQ $FFFE -> IRQ
; The game code itself only ever addresses $9000-$DFFF.
;==============================================================================
