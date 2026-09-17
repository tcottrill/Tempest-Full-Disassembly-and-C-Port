;Tempest (Atari, 1981) - vector ROM $3000-$3FFF (136002-123/124, or 138),
;byte-exact listing. Identical in every Tempest ROM set.
;
;Source: ALVROM.MAC (Dave Theurer) with ANVGAN.MAC (Ed Logg's character
;set) included at $3000; VGMC.MAC macros. The source was walked statement
;by statement and locked onto the ROM (disasm/vromsrc.py): 1713 of 1716
;statements re-encode exactly as VGMC.MAC would assemble them, 0 differ,
;3 cannot be evaluated (QCHKS0/QCHKS1 checksum equates live in ALHAR2, and
;BOKLIT's 'VCTR -24.*15.,-6C.,0' - '6C.' is not a legal decimal; MAC65 took
;C as digit value 12 -> -72, which is what the ROM holds).
;
;AVG (see avg.py): VCTR dx, dy, z = long vector; SVEC = short vector
;(VGMC picks it automatically for even deltas <= 30); z 0 = blank, 1 = use
;STAT intensity, else brightness z*2. CSTAT c = .WORD $68C0+c: on Tempest a
;STAT with bit 11 set loads the COLOUR latch (colour RAM $0800+c); without
;bit 11 it loads the intensity ((w>>4)&$F). Tempest has no sparkle/flip STAT
;bits (those are Major Havoc). SCAL b, $ll = binary / linear scale.
;JSRL/JMPL operand = (target-$2000)/2; targets below $3000 are vector RAM
;sub-buffers set up by ALDIS2 (named from ALVROM's equates below).
;Labels are Atari's names with '.' -> '_' (CHAR.A -> CHAR_A; CHAR. = CHAR_
;is the blank). Program-ROM tables that belong to ALVROM's .CSECT ($CDDE-
;$CF23: SCALOC, SCORES template, BUFASL.., JMPMAL, PICLO) are listed in
;build/vector_tables.json for the program-ROM track.
;Program-ROM references come from build/srclines.json + symbols.json (the
;byte-exact assembly of every module): each 6502 statement whose operand
;names a vector ROM label or a PT* picture code, shown by routine name.
;Vector-ROM and ALVROM .CSECT table references come from the JSRL/JMPL words.
;Re-verify with: python disasm/verify_vrom.py

.org $3000

;----------------------------[ vector RAM equates (ALVROM.MAC) ]----------------------------
.alias VECRAM           $2000
.alias SWINFO           $2004
.alias BAINFO           $2006
.alias BBINFO           $2104
.alias SWENEL           $2200
.alias BAENEL           $2202
.alias BBENEL           $2306
.alias SWWELL           $240A
.alias BAWELL           $240C
.alias BBWELL           $254E
.alias SWINVA           $2690
.alias BAINVA           $2692
.alias BBINVA           $27C8
.alias SWSHOT           $28FE
.alias BASHOT           $2900
.alias BBSHOT           $29AA
.alias SWNYMP           $2A54
.alias BANYMP           $2A56
.alias BBNYMP           $2B96
.alias SWEXPL           $2CD6
.alias BAEXPL           $2CD8
.alias BBEXPL           $2D4A
.alias SWCURS           $2DBC
.alias BACURS           $2DBE
.alias BBCURS           $2DF0
.alias SWSTAR           $2E22
.alias BASTAR           $2E24
.alias BBSTAR           $2EA6
.alias SCOBUF           $2F60
.alias SCALE            $2FFC

;----------------------------[ AVG display lists ]----------------------------

CHAR_A:          ;JSRL operand $800  (source name CHAR.A, ANVGAN.MAC:34)
;  Character 'A'
;  refs: vector ROM JSRL at $31FA
;  refs: vector ROM JSRL at $3478
;  refs: vector ROM JSRL at $3DEA
;  refs: vector ROM JSRL at $3E08
;  refs: ... and 10 more (see cross reference)
V3000:  SVEC   0, 16, 6               ;48C0
V3002:  SVEC   8, 8, 6                ;44C4
V3004:  SVEC   8, -8, 6               ;5CC4
V3006:  SVEC   0, -16, 6              ;58C0
V3008:  SVEC   -16, 8, 0              ;4418
V300A:  SVEC   16, 0, 6               ;40C8
V300C:  SVEC   8, -8, 0               ;5C04
V300E:  RTSL                          ;C000

CHAR_B:          ;JSRL operand $808  (source name CHAR.B, ANVGAN.MAC:43)
;  Character 'B'
;  refs: vector ROM JSRL at $31FC
;  refs: vector ROM JSRL at $3E24
V3010:  SVEC   0, 24, 6               ;4CC0
V3012:  SVEC   12, 0, 6               ;40C6
V3014:  SVEC   4, -4, 5               ;5EA2  THESE VECTORS ARE BRIGHTER THAN THE OTHERS
V3016:  SVEC   0, -4, 5               ;5EA0
V3018:  SVEC   -4, -4, 5              ;5EBE
V301A:  SVEC   -12, 0, 6              ;40DA
V301C:  SVEC   12, 0, 0               ;4006
V301E:  SVEC   4, -4, 5               ;5EA2
V3020:  SVEC   0, -4, 5               ;5EA0
V3022:  SVEC   -4, -4, 5              ;5EBE
V3024:  SVEC   -12, 0, 6              ;40DA
V3026:  SVEC   24, 0, 0               ;400C
V3028:  RTSL                          ;C000

CHAR_C:          ;JSRL operand $815  (source name CHAR.C, ANVGAN.MAC:57)
;  Character 'C'
;  refs: vector ROM JSRL at $31FE
;  refs: vector ROM JMPL at $3488
;  refs: vector ROM JSRL at $3E46
;  refs: vector ROM JSRL at $3F08
V302A:  SVEC   0, 24, 6               ;4CC0
V302C:  SVEC   16, 0, 6               ;40C8
V302E:  SVEC   -16, -24, 0            ;5418
V3030:  SVEC   16, 0, 6               ;40C8
V3032:  SVEC   8, 0, 0                ;4004
V3034:  RTSL                          ;C000

CHAR_D:          ;JSRL operand $81B  (source name CHAR.D, ANVGAN.MAC:64)
;  Character 'D'
;  refs: vector ROM JSRL at $3200
;  refs: vector ROM JSRL at $3DF0
;  refs: vector ROM JSRL at $3E32
;  refs: vector ROM JSRL at $3E34
;  refs: ... and 4 more (see cross reference)
V3036:  SVEC   0, 24, 6               ;4CC0
V3038:  SVEC   8, 0, 6                ;40C4
V303A:  SVEC   8, -8, 6               ;5CC4
V303C:  SVEC   0, -8, 6               ;5CC0
V303E:  SVEC   -8, -8, 6              ;5CDC
V3040:  SVEC   -8, 0, 6               ;40DC
V3042:  SVEC   24, 0, 0               ;400C
V3044:  RTSL                          ;C000

CHAR_E:          ;JSRL operand $823  (source name CHAR.E, ANVGAN.MAC:73)
;  Character 'E'
;  refs: vector ROM JSRL at $3202
;  refs: vector ROM JSRL at $3474
;  refs: vector ROM JSRL at $3DEE
;  refs: vector ROM JSRL at $3E0C
;  refs: ... and 14 more (see cross reference)
V3046:  SVEC   0, 24, 6               ;4CC0
V3048:  SVEC   16, 0, 6               ;40C8
V304A:  SVEC   -4, -12, 0             ;5A1E
V304C:  SVEC   -12, 0, 6              ;40DA
V304E:  SVEC   0, -12, 0              ;5A00
V3050:  SVEC   16, 0, 6               ;40C8
V3052:  SVEC   8, 0, 0                ;4004
V3054:  RTSL                          ;C000

CHAR_F:          ;JSRL operand $82B  (source name CHAR.F, ANVGAN.MAC:82)
;  Character 'F'
;  refs: vector ROM JSRL at $3204
;  refs: vector ROM JSRL at $3E84
;  refs: vector ROM JSRL at $3EB6
;  refs: vector ROM JSRL at $3EC4
V3056:  SVEC   0, 24, 6               ;4CC0
V3058:  SVEC   16, 0, 6               ;40C8
V305A:  SVEC   -4, -12, 0             ;5A1E
V305C:  SVEC   -12, 0, 6              ;40DA
V305E:  SVEC   0, -12, 0              ;5A00
V3060:  SVEC   24, 0, 0               ;400C
V3062:  RTSL                          ;C000

CHAR_G:          ;JSRL operand $832  (source name CHAR.G, ANVGAN.MAC:90)
;  Character 'G'
;  refs: vector ROM JSRL at $3206
;  refs: vector ROM JMPL at $3480
;  refs: vector ROM JSRL at $3E12
;  refs: vector ROM JSRL at $3E64
V3064:  SVEC   0, 24, 6               ;4CC0
V3066:  SVEC   16, 0, 6               ;40C8
V3068:  SVEC   0, -8, 6               ;5CC0
V306A:  SVEC   -8, -8, 0              ;5C1C
V306C:  SVEC   8, 0, 6                ;40C4
V306E:  SVEC   0, -8, 6               ;5CC0
V3070:  SVEC   -16, 0, 6              ;40D8
V3072:  SVEC   24, 0, 0               ;400C
V3074:  RTSL                          ;C000

CHAR_H:          ;JSRL operand $83B  (source name CHAR.H, ANVGAN.MAC:100)
;  Character 'H'
;  refs: vector ROM JSRL at $3208
;  refs: vector ROM JSRL at $3F46
V3076:  SVEC   0, 24, 6               ;4CC0
V3078:  SVEC   0, -12, 0              ;5A00
V307A:  SVEC   16, 0, 6               ;40C8
V307C:  SVEC   0, 12, 0               ;4600
V307E:  SVEC   0, -24, 6              ;54C0
V3080:  SVEC   8, 0, 0                ;4004
V3082:  RTSL                          ;C000

CHAR_I:          ;JSRL operand $842  (source name CHAR.I, ANVGAN.MAC:108)
;  Character 'I'
;  refs: vector ROM JSRL at $320A
;  refs: vector ROM JSRL at $347C
;  refs: vector ROM JSRL at $3E86
;  refs: vector ROM JSRL at $3EE8
;  refs: ... and 1 more (see cross reference)
V3084:  SVEC   16, 0, 6               ;40C8
V3086:  SVEC   -8, 0, 0               ;401C
V3088:  SVEC   0, 24, 6               ;4CC0
V308A:  SVEC   8, 0, 0                ;4004
V308C:  SVEC   -16, 0, 6              ;40D8
V308E:  SVEC   24, -24, 0             ;540C
V3090:  RTSL                          ;C000

CHAR_J:          ;JSRL operand $849  (source name CHAR.J, ANVGAN.MAC:116)
;  Character 'J'
;  refs: vector ROM JSRL at $320C
V3092:  SVEC   0, 8, 0                ;4400
V3094:  SVEC   8, -8, 6               ;5CC4
V3096:  SVEC   8, 0, 6                ;40C4
V3098:  SVEC   0, 24, 6               ;4CC0
V309A:  SVEC   8, -24, 0              ;5404
V309C:  RTSL                          ;C000

CHAR_K:          ;JSRL operand $84F  (source name CHAR.K, ANVGAN.MAC:123)
;  Character 'K'
;  refs: vector ROM JSRL at $320E
V309E:  SVEC   0, 24, 6               ;4CC0
V30A0:  SVEC   12, 0, 0               ;4006
V30A2:  SVEC   -12, -12, 6            ;5ADA
V30A4:  SVEC   12, -12, 6             ;5AC6
V30A6:  SVEC   12, 0, 0               ;4006
V30A8:  RTSL                          ;C000

CHAR_L:          ;JSRL operand $855  (source name CHAR.L, ANVGAN.MAC:130)
;  Character 'L'
;  refs: vector ROM JSRL at $3210
;  refs: vector ROM JSRL at $3DE8
;  refs: vector ROM JSRL at $3E58
;  refs: vector ROM JSRL at $3EC2
V30AA:  SVEC   0, 24, 0               ;4C00
V30AC:  SVEC   0, -24, 6              ;54C0
V30AE:  SVEC   16, 0, 6               ;40C8
V30B0:  SVEC   8, 0, 0                ;4004
V30B2:  RTSL                          ;C000

CHAR_M:          ;JSRL operand $85A  (source name CHAR.M, ANVGAN.MAC:136)
;  Character 'M'
;  refs: vector ROM JSRL at $3212
;  refs: vector ROM JSRL at $3E68
;  refs: vector ROM JSRL at $3EEA
;  refs: vector ROM JSRL at $3F36
;  refs: ... and 1 more (see cross reference)
V30B4:  SVEC   0, 24, 6               ;4CC0
V30B6:  SVEC   8, -8, 6               ;5CC4
V30B8:  SVEC   8, 8, 6                ;44C4
V30BA:  SVEC   0, -24, 6              ;54C0
V30BC:  SVEC   8, 0, 0                ;4004
V30BE:  RTSL                          ;C000

CHAR_N:          ;JSRL operand $860  (source name CHAR.N, ANVGAN.MAC:143)
;  Character 'N'
;  refs: vector ROM JSRL at $3214
;  refs: vector ROM JSRL at $347E
;  refs: vector ROM JSRL at $3DDE
;  refs: vector ROM JSRL at $3E28
;  refs: ... and 2 more (see cross reference)
V30C0:  SVEC   0, 24, 6               ;4CC0
V30C2:  SVEC   16, -24, 6             ;54C8
V30C4:  SVEC   0, 24, 6               ;4CC0
V30C6:  SVEC   8, -24, 0              ;5404
V30C8:  RTSL                          ;C000

CHAR_O:          ;JSRL operand $865  (source name CHAR.O, ANVGAN.MAC:149)
;  Character 'O'
;  refs: vector ROM JSRL at $31E6
;  refs: vector ROM JSRL at $3216
;  refs: vector ROM JSRL at $3DA0
;  refs: vector ROM JMPL at $3DB0
;  refs: ... and 9 more (see cross reference)
V30CA:  SVEC   0, 24, 6               ;4CC0
V30CC:  SVEC   16, 0, 6               ;40C8
V30CE:  SVEC   0, -24, 6              ;54C0
V30D0:  SVEC   -16, 0, 6              ;40D8
V30D2:  SVEC   24, 0, 0               ;400C
V30D4:  RTSL                          ;C000

CHAR_P:          ;JSRL operand $86B  (source name CHAR.P, ANVGAN.MAC:156)
;  Character 'P'
;  refs: vector ROM JSRL at $3218
;  refs: vector ROM JSRL at $3DE6
;  refs: vector ROM JSRL at $3E56
;  refs: vector ROM JSRL at $3E78
;  refs: ... and 1 more (see cross reference)
V30D6:  SVEC   0, 24, 6               ;4CC0
V30D8:  SVEC   16, 0, 6               ;40C8
V30DA:  SVEC   0, -12, 6              ;5AC0
V30DC:  SVEC   -16, 0, 6              ;40D8
V30DE:  SVEC   12, -12, 0             ;5A06
V30E0:  SVEC   12, 0, 0               ;4006
V30E2:  RTSL                          ;C000

CHAR_Q:          ;JSRL operand $872  (source name CHAR.Q, ANVGAN.MAC:164)
;  Character 'Q'
;  refs: vector ROM JSRL at $321A
V30E4:  SVEC   0, 24, 6               ;4CC0
V30E6:  SVEC   16, 0, 6               ;40C8
V30E8:  SVEC   0, -16, 6              ;58C0
V30EA:  SVEC   -8, -8, 6              ;5CDC
V30EC:  SVEC   -8, 0, 6               ;40DC
V30EE:  SVEC   8, 8, 0                ;4404
V30F0:  SVEC   8, -8, 6               ;5CC4
V30F2:  SVEC   8, 0, 0                ;4004
V30F4:  RTSL                          ;C000

CHAR_R:          ;JSRL operand $87B  (source name CHAR.R, ANVGAN.MAC:174)
;  Character 'R'
;  refs: vector ROM JSRL at $321C
;  refs: vector ROM JSRL at $3476
;  refs: vector ROM JSRL at $3E0E
;  refs: vector ROM JSRL at $3E38
;  refs: ... and 9 more (see cross reference)
V30F6:  SVEC   0, 24, 6               ;4CC0
V30F8:  SVEC   16, 0, 6               ;40C8
V30FA:  SVEC   0, -12, 6              ;5AC0
V30FC:  SVEC   -16, 0, 6              ;40D8
V30FE:  SVEC   4, 0, 0                ;4002
V3100:  SVEC   12, -12, 6             ;5AC6
V3102:  SVEC   8, 0, 0                ;4004
V3104:  RTSL                          ;C000

CHAR_S:          ;JSRL operand $883  (source name CHAR.S, ANVGAN.MAC:183)
;  Character 'S'
;  refs: vector ROM JSRL at $321E
;  refs: vector ROM JSRL at $347A
;  refs: vector ROM JSRL at $3E2C
;  refs: vector ROM JSRL at $3E42
;  refs: ... and 12 more (see cross reference)
V3106:  SVEC   16, 0, 6               ;40C8
V3108:  SVEC   0, 12, 6               ;46C0
V310A:  SVEC   -16, 0, 6              ;40D8
V310C:  SVEC   0, 12, 6               ;46C0
V310E:  SVEC   16, 0, 6               ;40C8
V3110:  SVEC   8, -24, 0              ;5404
V3112:  RTSL                          ;C000

CHAR_T:          ;JSRL operand $88A  (source name CHAR.T, ANVGAN.MAC:191)
;  Character 'T'
;  refs: vector ROM JSRL at $3220
;  refs: vector ROM JSRL at $3E9A
;  refs: vector ROM JSRL at $3EC8
;  refs: vector ROM JSRL at $3ECE
;  refs: ... and 5 more (see cross reference)
V3114:  SVEC   8, 0, 0                ;4004
V3116:  SVEC   0, 24, 6               ;4CC0
V3118:  SVEC   -8, 0, 0               ;401C
V311A:  SVEC   16, 0, 6               ;40C8
V311C:  SVEC   8, -24, 0              ;5404
V311E:  RTSL                          ;C000

CHAR_U:          ;JSRL operand $890  (source name CHAR.U, ANVGAN.MAC:198)
;  Character 'U'
;  refs: vector ROM JSRL at $3222
;  refs: vector ROM JSRL at $3E2A
;  refs: vector ROM JSRL at $3F3E
V3120:  SVEC   0, 24, 0               ;4C00
V3122:  SVEC   0, -24, 6              ;54C0
V3124:  SVEC   16, 0, 6               ;40C8
V3126:  SVEC   0, 24, 6               ;4CC0
V3128:  SVEC   8, -24, 0              ;5404
V312A:  RTSL                          ;C000

CHAR_V:          ;JSRL operand $896  (source name CHAR.V, ANVGAN.MAC:205)
;  Character 'V'
;  refs: vector ROM JSRL at $3224
;  refs: vector ROM JSRL at $3E0A
V312C:  SVEC   0, 24, 0               ;4C00
V312E:  SVEC   8, -24, 6              ;54C4
V3130:  SVEC   8, 24, 6               ;4CC4
V3132:  SVEC   8, -24, 0              ;5404
V3134:  RTSL                          ;C000

CHAR_W:          ;JSRL operand $89B  (source name CHAR.W, ANVGAN.MAC:211)
;  Character 'W'
;  refs: vector ROM JSRL at $3226
V3136:  SVEC   0, 24, 0               ;4C00
V3138:  SVEC   0, -24, 6              ;54C0
V313A:  SVEC   8, 8, 6                ;44C4
V313C:  SVEC   8, -8, 6               ;5CC4
V313E:  SVEC   0, 24, 6               ;4CC0
V3140:  SVEC   8, -24, 0              ;5404
V3142:  RTSL                          ;C000

CHAR_X:          ;JSRL operand $8A2  (source name CHAR.X, ANVGAN.MAC:219)
;  Character 'X'
;  refs: vector ROM JSRL at $3228
;  refs: vector ROM JSRL at $3E1C
V3144:  SVEC   16, 24, 6              ;4CC8
V3146:  SVEC   -16, 0, 0              ;4018
V3148:  SVEC   16, -24, 6             ;54C8
V314A:  SVEC   8, 0, 0                ;4004
V314C:  RTSL                          ;C000

CHAR_Y:          ;JSRL operand $8A7  (source name CHAR.Y, ANVGAN.MAC:225)
;  Character 'Y'
;  refs: vector ROM JSRL at $322A
;  refs: vector ROM JSRL at $3DEC
;  refs: vector ROM JSRL at $3E5C
;  refs: vector ROM JMPL at $3F30
V314E:  SVEC   8, 0, 0                ;4004
V3150:  SVEC   0, 16, 6               ;48C0
V3152:  SVEC   -8, 8, 6               ;44DC
V3154:  SVEC   16, 0, 0               ;4008
V3156:  SVEC   -8, -8, 6              ;5CDC
V3158:  SVEC   16, -16, 0             ;5808
V315A:  RTSL                          ;C000

CHAR_Z:          ;JSRL operand $8AE  (source name CHAR.Z, ANVGAN.MAC:233)
;  Character 'Z'
;  refs: vector ROM JSRL at $322C
;  refs: vector ROM JSRL at $3EA0
;  refs: vector ROM JSRL at $3EAE
V315C:  SVEC   0, 24, 0               ;4C00
V315E:  SVEC   16, 0, 6               ;40C8
V3160:  SVEC   -16, -24, 6            ;54D8
V3162:  SVEC   16, 0, 6               ;40C8
V3164:  SVEC   8, 0, 0                ;4004
V3166:  RTSL                          ;C000

CHAR_:           ;JSRL operand $8B4  (source name CHAR., ANVGAN.MAC:240)
;  Character blank (space): advance only
;  no lit vectors - beam positioning / advance / vector-RAM list
;  refs: vector ROM JSRL at $31E4
;  refs: vector ROM JSRL at $322E
;  refs: vector ROM JSRL at $3E2E
;  refs: vector ROM JSRL at $3E50
;  refs: ... and 34 more (see cross reference)
V3168:  SVEC   24, 0, 0               ;400C
V316A:  RTSL                          ;C000

CHAR_1:          ;JSRL operand $8B6  (source name CHAR.1, ANVGAN.MAC:245)
;  Character '1'
;  refs: vector ROM JSRL at $31E8
;  refs: vector ROM JSRL at $3262
;  refs: vector ROM JSRL at $3DF6
;  refs: vector ROM JSRL at $3E1E
;  refs: ... and 1 more (see cross reference)
V316C:  SVEC   8, 0, 0                ;4004
V316E:  SVEC   0, 24, 6               ;4CC0
V3170:  SVEC   16, -24, 0             ;5408
V3172:  RTSL                          ;C000

CHAR_2:          ;JSRL operand $8BA  (source name CHAR.2, ANVGAN.MAC:250)
;  Character '2'
;  refs: vector ROM JSRL at $31EA
;  refs: vector ROM JSRL at $3266
;  refs: vector ROM JSRL at $3DAC
;  refs: vector ROM JSRL at $3DFE
;  refs: ... and 1 more (see cross reference)
V3174:  SVEC   0, 24, 0               ;4C00
V3176:  SVEC   16, 0, 6               ;40C8
V3178:  SVEC   0, -12, 6              ;5AC0
V317A:  SVEC   -16, 0, 6              ;40D8
V317C:  SVEC   0, -12, 6              ;5AC0
V317E:  SVEC   16, 0, 6               ;40C8
V3180:  SVEC   8, 0, 0                ;4004
V3182:  RTSL                          ;C000

CHAR_3:          ;JSRL operand $8C2  (source name CHAR.3, ANVGAN.MAC:259)
;  Character '3'
;  refs: vector ROM JSRL at $31EC
V3184:  SVEC   16, 0, 6               ;40C8
V3186:  SVEC   0, 24, 6               ;4CC0
V3188:  SVEC   -16, 0, 6              ;40D8
V318A:  SVEC   0, -12, 0              ;5A00
V318C:  SVEC   16, 0, 6               ;40C8
V318E:  SVEC   8, -12, 0              ;5A04
V3190:  RTSL                          ;C000

CHAR_4:          ;JSRL operand $8C9  (source name CHAR.4, ANVGAN.MAC:267)
;  Character '4'
;  refs: vector ROM JSRL at $31EE
V3192:  SVEC   0, 24, 0               ;4C00
V3194:  SVEC   0, -12, 6              ;5AC0
V3196:  SVEC   16, 0, 6               ;40C8
V3198:  SVEC   0, 12, 0               ;4600
V319A:  SVEC   0, -24, 6              ;54C0
V319C:  SVEC   8, 0, 0                ;4004
V319E:  RTSL                          ;C000

CHAR_5:          ;JSRL operand $8D0  (source name CHAR.5, ANVGAN.MAC:275)
;  Character '5'
;  refs: vector ROM JSRL at $31F0
;  refs: vector ROM JSRL at $3D9E
;  refs: vector ROM JSRL at $3DAE
V31A0:  SVEC   16, 0, 6               ;40C8
V31A2:  SVEC   0, 12, 6               ;46C0
V31A4:  SVEC   -16, 0, 6              ;40D8
V31A6:  SVEC   0, 12, 6               ;46C0
V31A8:  SVEC   16, 0, 6               ;40C8
V31AA:  SVEC   8, -24, 0              ;5404
V31AC:  RTSL                          ;C000

CHAR_6:          ;JSRL operand $8D7  (source name CHAR.6, ANVGAN.MAC:283)
;  Character '6'
;  refs: vector ROM JSRL at $31F2
V31AE:  SVEC   0, 12, 0               ;4600
V31B0:  SVEC   16, 0, 6               ;40C8
V31B2:  SVEC   0, -12, 6              ;5AC0
V31B4:  SVEC   -16, 0, 6              ;40D8
V31B6:  SVEC   0, 24, 6               ;4CC0
V31B8:  SVEC   24, -24, 0             ;540C
V31BA:  RTSL                          ;C000

CHAR_7:          ;JSRL operand $8DE  (source name CHAR.7, ANVGAN.MAC:291)
;  Character '7'
;  refs: vector ROM JSRL at $31F4
;  refs: vector ROM JSRL at $3D92
V31BC:  SVEC   0, 24, 0               ;4C00
V31BE:  SVEC   16, 0, 6               ;40C8
V31C0:  SVEC   0, -24, 6              ;54C0
V31C2:  SVEC   8, 0, 0                ;4004
V31C4:  RTSL                          ;C000

CHAR_8:          ;JSRL operand $8E3  (source name CHAR.8, ANVGAN.MAC:297)
;  Character '8'
;  refs: vector ROM JSRL at $31F6
V31C6:  SVEC   16, 0, 6               ;40C8
V31C8:  SVEC   0, 24, 6               ;4CC0
V31CA:  SVEC   -16, 0, 6              ;40D8
V31CC:  SVEC   0, -24, 6              ;54C0
V31CE:  SVEC   0, 12, 0               ;4600
V31D0:  SVEC   16, 0, 6               ;40C8
V31D2:  SVEC   8, -12, 0              ;5A04
V31D4:  RTSL                          ;C000

CHAR_9:          ;JSRL operand $8EB  (source name CHAR.9, ANVGAN.MAC:306)
;  Character '9'
;  refs: vector ROM JSRL at $31F8
V31D6:  SVEC   16, 0, 0               ;4008
V31D8:  SVEC   0, 24, 6               ;4CC0
V31DA:  SVEC   -16, 0, 6              ;40D8
V31DC:  SVEC   0, -12, 6              ;5AC0
V31DE:  SVEC   16, 0, 6               ;40C8
V31E0:  SVEC   8, -12, 0              ;5A04
V31E2:  RTSL                          ;C000

VGMSGA:          ;JSRL operand $8F2  (source name VGMSGA, ANVGAN.MAC:315)
;  Character JSRL table: index 0 blank, 1-10 digits, 11-36 A-Z (ASCVG code = index*2)
;  refs: vector ROM JMPL at $334C
;  refs: program ROM INFO+$2A (ALSCO2): LDA VGMSGA at $A8DE
;  refs: program ROM ZATC4V+$1B (ALSCO2): LDA Y,VGMSGA+22. at $A937
;  refs: program ROM NWHEXZ+$D (ALSCO2): LDA Y,VGMSGA at $AA09
;  refs: ... and 8 more (see cross reference)
V31E4:  JSRL   CHAR_                  ;A8B4  ADDRESS OF LETTER ROUTINES
V31E6:  JSRL   CHAR_O                 ;A865
V31E8:  JSRL   CHAR_1                 ;A8B6
V31EA:  JSRL   CHAR_2                 ;A8BA
V31EC:  JSRL   CHAR_3                 ;A8C2
V31EE:  JSRL   CHAR_4                 ;A8C9
V31F0:  JSRL   CHAR_5                 ;A8D0
V31F2:  JSRL   CHAR_6                 ;A8D7
V31F4:  JSRL   CHAR_7                 ;A8DE
V31F6:  JSRL   CHAR_8                 ;A8E3
V31F8:  JSRL   CHAR_9                 ;A8EB
V31FA:  JSRL   CHAR_A                 ;A800
V31FC:  JSRL   CHAR_B                 ;A808
V31FE:  JSRL   CHAR_C                 ;A815
V3200:  JSRL   CHAR_D                 ;A81B
V3202:  JSRL   CHAR_E                 ;A823
V3204:  JSRL   CHAR_F                 ;A82B
V3206:  JSRL   CHAR_G                 ;A832
V3208:  JSRL   CHAR_H                 ;A83B
V320A:  JSRL   CHAR_I                 ;A842
V320C:  JSRL   CHAR_J                 ;A849
V320E:  JSRL   CHAR_K                 ;A84F
V3210:  JSRL   CHAR_L                 ;A855
V3212:  JSRL   CHAR_M                 ;A85A
V3214:  JSRL   CHAR_N                 ;A860
V3216:  JSRL   CHAR_O                 ;A865
V3218:  JSRL   CHAR_P                 ;A86B
V321A:  JSRL   CHAR_Q                 ;A872
V321C:  JSRL   CHAR_R                 ;A87B
V321E:  JSRL   CHAR_S                 ;A883
V3220:  JSRL   CHAR_T                 ;A88A
V3222:  JSRL   CHAR_U                 ;A890
V3224:  JSRL   CHAR_V                 ;A896
V3226:  JSRL   CHAR_W                 ;A89B
V3228:  JSRL   CHAR_X                 ;A8A2
V322A:  JSRL   CHAR_Y                 ;A8A7
V322C:  JSRL   CHAR_Z                 ;A8AE
V322E:  JSRL   CHAR_                  ;A8B4
V3230:  JSRL   DASH                   ;A91B
V3232:  JSRL   HALF                   ;A92E  1/2 (USE QUOTES)
V3234:  JSRL   COPYR                  ;A91F  CIRCLE C (USE #)

DASH:            ;JSRL operand $91B  (source name DASH, ALVROM.MAC:33)
;  Character '-' (VGMSGA index 38; ASCVG '\')
;  refs: vector ROM JSRL at $3230
V3236:  SVEC   0, 12, 0               ;4600
V3238:  SVEC   16, 0, 6               ;40C8
V323A:  SVEC   8, -12, 0              ;5A04
V323C:  RTSL                          ;C000

COPYR:           ;JSRL operand $91F  (source name COPYR, ALVROM.MAC:38)
;  Copyright circle-C (VGMSGA index 40; ASCVG '^')
;  refs: vector ROM JSRL at $3234
V323E:  SVEC   0, 4, 0                ;4200  OUTER CIRCLE
V3240:  SVEC   0, 16, 6               ;48C0
V3242:  SVEC   4, 4, 6                ;42C2
V3244:  SVEC   8, 0, 6                ;40C4
V3246:  SVEC   4, -4, 6               ;5EC2
V3248:  SVEC   0, -16, 6              ;58C0
V324A:  SVEC   -4, -4, 6              ;5EDE
V324C:  SVEC   -8, 0, 6               ;40DC
V324E:  SVEC   -4, 4, 6               ;42DE
V3250:  SVEC   12, 4, 0               ;4206
V3252:  SVEC   -8, 0, 6               ;40DC  C
V3254:  SVEC   0, 8, 6                ;44C0
V3256:  SVEC   8, 0, 6                ;40C4
V3258:  SVEC   12, -16, 0             ;5806
V325A:  RTSL                          ;C000

HALF:            ;JSRL operand $92E  (source name HALF, ALVROM.MAC:54)
;  '1/2' glyph (VGMSGA index 39)
;  refs: vector ROM JSRL at $3232
;  refs: program ROM IHALF (ALSCO2): .WORD HALF at $AAF3
V325C:  SVEC   16, 24, 6              ;4CC8
V325E:  SVEC   -14, -10, 0            ;5B19
V3260:  SCAL   2, $00                 ;7200
V3262:  JSRL   CHAR_1                 ;A8B6
V3264:  SVEC   -8, -28, 0             ;521C
V3266:  JSRL   CHAR_2                 ;A8BA
V3268:  SCAL   1, $00                 ;7100

CHKSM0:          ;JSRL operand $935  (source name CHKSM0, ALVROM.MAC:170)
;  checksum byte QCHKS0 hidden in an RTSL word ($C0xx)
V326A:  .byte $2A, $C0          ;.BYTE QCHKS0,0C0 - decodes as RTSL $C02A

LIFEY:           ;JSRL operand $936  (source name LIFEY, ALVROM.MAC:172)
;  Player life symbol in yellow (CSTAT YELLOW + LIFE1)
;  colour: 1 YELLOW
;  refs: program ROM DSPSYS+$1B (ALTES2): LAH LIFEY+1 at $D81F
;  refs: program ROM DSPSYS+$1E (ALTES2): LXL LIFEY at $D822
V326C:  CSTAT  1                      ;68C1  YELLOW

LIFE1:           ;JSRL operand $937  (source name LIFE1, ALVROM.MAC:174)
;  Player life symbol (small claw)
;  refs: vector ROM JSRL at $3284
V326E:  SVEC   24, -12, 6             ;5ACC  [SCVEC 4,-2,CB]
V3270:  SVEC   -18, -6, 6             ;5DD7  [SCVEC 1,-3,CB]
V3272:  SVEC   12, 6, 6               ;43C6  [SCVEC 3,-2,CB]
V3274:  SVEC   -18, 6, 6              ;43D7  [SCVEC 0,-1,CB]
V3276:  SVEC   -18, -6, 6             ;5DD7  [SCVEC -3,-2,CB]
V3278:  SVEC   12, -6, 6              ;5DC6  [SCVEC -1,-3,CB]
V327A:  SVEC   -18, 6, 6              ;43D7  [SCVEC -4,-2,CB]
V327C:  SVEC   24, 12, 6              ;46CC  [SCVEC 0,0,CB]

LIFE0:           ;JSRL operand $93F  (source name LIFE0, ALVROM.MAC:182)
;  Blank life slot (advance only)
;  no lit vectors - beam positioning / advance / vector-RAM list
;  refs: vector ROM JSRL at $3286
;  refs: program ROM table JSRL at $CE08
;  refs: program ROM table JSRL at $CE0A
;  refs: program ROM table JSRL at $CE0C
;  refs: ... and 9 more (see cross reference)
V327E:  VCTR   60, 0, 0               ;0000 003C  [SCVEC 0A,0,0]
V3282:  RTSL                          ;C000

LSYMBL:          ;JSRL operand $942  (source name LSYMBL, ALVROM.MAC:184)
;  JSRL word copied by the 6502 into the score template (life symbol)
;  refs: program ROM UPSCLI+$2A (ALSCO2): LDA LSYMBL at $A9A9
V3284:  JSRL   LIFE1                  ;A937

LSYMB0:          ;JSRL operand $943  (source name LSYMB0, ALVROM.MAC:185)
;  JSRL word copied by the 6502 (blank life slot)
;  no lit vectors - beam positioning / advance / vector-RAM list
;  refs: program ROM UPSCLI+$33 (ALSCO2): LDA LSYMB0 at $A9B2
V3286:  JSRL   LIFE0                  ;A93F

SEVEN:           ;JSRL operand $944  (source name SEVEN, ALVROM.MAC:196)
;  Self test: six intensity levels (lines at z=2..7)
;  refs: vector ROM JSRL at $32C0
;  refs: vector ROM JSRL at $32CA
;  refs: vector ROM JSRL at $32D4
;  refs: vector ROM JSRL at $32DE
;  refs: ... and 2 more (see cross reference)
V3288:  VCTR   0, 256, 2              ;0100 4000  [RVCTR INLEN,0,2]
V328C:  VCTR   16, -256, 0            ;1F00 0010  [RVCTR -INLEN,SEPY,0]
V3290:  VCTR   0, 256, 3              ;0100 6000  [RVCTR INLEN,0,3]

SEVEN2:          ;JSRL operand $94A  (source name SEVEN2, ALVROM.MAC:199)
;  Self test: entry inside SEVEN
;  refs: vector ROM JMPL at $3308
V3294:  VCTR   16, -256, 0            ;1F00 0010  [RVCTR -INLEN,SEPY,0]
V3298:  VCTR   0, 256, 4              ;0100 8000  [RVCTR INLEN,0,4]
V329C:  VCTR   16, -256, 0            ;1F00 0010  [RVCTR -INLEN,SEPY,0]
V32A0:  VCTR   0, 256, 5              ;0100 A000  [RVCTR INLEN,0,5]
V32A4:  VCTR   16, -256, 0            ;1F00 0010  [RVCTR -INLEN,SEPY,0]
V32A8:  VCTR   0, 256, 6              ;0100 C000  [RVCTR INLEN,0,6]
V32AC:  VCTR   16, -256, 0            ;1F00 0010  [RVCTR -INLEN,SEPY,0]
V32B0:  VCTR   0, 256, 7              ;0100 E000  [RVCTR INLEN,0,7]
V32B4:  RTSL                          ;C000

INTEST:          ;JSRL operand $95B  (source name INTEST, ALVROM.MAC:209)
;  Self test: colour / intensity bars picture
;  colour: 0 WHITE, 3 RED, 7 BLUE (BLULET), 5 GREEN, 1 YELLOW, 4 TURQOI, 2 PURPLE
;  refs: program ROM SINTEN (ALTES2): LXL INTEST at $DB7E
;  refs: program ROM SINTEN+$3 (ALTES2): LAH INTEST+1 at $DB81
V32B6:  JSRL   BONDRY                 ;AA53
V32B8:  CNTR                          ;8040
V32BA:  VCTR   64, -384, 0            ;1E80 0040  [RVCTR -<INLEN/2*3>,SEPY*4,0]
V32BE:  CSTAT  3                      ;68C3  RED
V32C0:  JSRL   SEVEN                  ;A944
V32C2:  CNTR                          ;8040
V32C4:  VCTR   64, -128, 0            ;1F80 0040  [RVCTR -<INLEN/2>,SEPY*4,0]
V32C8:  CSTAT  7                      ;68C7  BLUE (BLULET)
V32CA:  JSRL   SEVEN                  ;A944
V32CC:  CNTR                          ;8040
V32CE:  VCTR   64, 128, 0             ;0080 0040  [RVCTR <INLEN/2>,SEPY*4,0]
V32D2:  CSTAT  5                      ;68C5  GREEN
V32D4:  JSRL   SEVEN                  ;A944
V32D6:  CNTR                          ;8040
V32D8:  VCTR   -160, -384, 0          ;1E80 1F60  [RVCTR -<INLEN/2*3>,-SEPY*10.,0]
V32DC:  CSTAT  1                      ;68C1  YELLOW
V32DE:  JSRL   SEVEN                  ;A944
V32E0:  CNTR                          ;8040
V32E2:  VCTR   -160, -128, 0          ;1F80 1F60  [RVCTR -<INLEN/2>,-SEPY*10.,0]
V32E6:  CSTAT  4                      ;68C4  TURQOI
V32E8:  JSRL   SEVEN                  ;A944
V32EA:  CNTR                          ;8040
V32EC:  VCTR   -160, 128, 0           ;0080 1F60  [RVCTR <INLEN/2>,-SEPY*10.,0]
V32F0:  CSTAT  2                      ;68C2  PURPLE
V32F2:  JSRL   SEVEN                  ;A944
V32F4:  CNTR                          ;8040
V32F6:  VCTR   -48, -128, 0           ;1F80 1FD0  [RVCTR -<INLEN/2>,-SEPY*3,0]
V32FA:  CSTAT  0                      ;68C0  WHITE
V32FC:  VCTR   0, 256, 2              ;0100 4000  [RVCTR INLEN,0,2]
V3300:  VCTR   16, -320, 0            ;1EC0 0010  [RVCTR -INLEN-40,SEPY,0]
V3304:  VCTR   0, 320, 3              ;0140 6000  [RVCTR INLEN+40,0,3]
V3308:  JMPL   SEVEN2                 ;E94A

HATCH:           ;JSRL operand $985  (source name HATCH, ALVROM.MAC:248)
;  Self test: cross-hatch pattern, then the alphabet (VGMSGA)
;  colour: 0 WHITE
;  refs: program ROM SHATCH (ALTES2): LAH HATCH+1 at $DB84
;  refs: program ROM SHATCH+$3 (ALTES2): LXL HATCH at $DB87
V330A:  JSRL   BONDRY                 ;AA53  EDGE OF SCREEN (END UP IN LOWER LEFT CORNER)
V330C:  CNTR                          ;8040
V330E:  VCTR   -500, -540, 0          ;1DE4 1E0C  [SCVEC -MAX,-MAY,0]
V3312:  VCTR   1000, 810, 4           ;032A 83E8  [SCVEC MAX,MAY/2,4]
V3316:  VCTR   -334, 270, 4           ;010E 9EB2  [SCVEC MAX/3,MAY,4]
V331A:  VCTR   -666, -540, 4          ;1DE4 9D66  [SCVEC -MAX,0,4]
V331E:  VCTR   666, -540, 4           ;1DE4 829A  [SCVEC MAX/3,-MAY,4]
V3322:  VCTR   334, 270, 4            ;010E 814E  [SCVEC MAX,-MAY/2,4]
V3326:  VCTR   -1000, 810, 4          ;032A 9C18  [SCVEC -MAX,MAY,4]
V332A:  VCTR   1000, 0, 0             ;0000 03E8  [SCVEC MAX,MAY,0]
V332E:  VCTR   -1000, -810, 4         ;1CD6 9C18  [SCVEC -MAX,-MAY/2,4]
V3332:  VCTR   334, -270, 4           ;1EF2 814E  [SCVEC -MAX/3,-MAY,4]
V3336:  VCTR   666, 540, 4            ;021C 829A  [SCVEC MAX,0,4]
V333A:  VCTR   -666, 540, 4           ;021C 9D66  [SCVEC -MAX/3,MAY,4]
V333E:  VCTR   -334, -270, 4          ;1EF2 9EB2  [SCVEC -MAX,MAY/2,4]
V3342:  VCTR   1000, -810, 4          ;1CD6 83E8  [SCVEC MAX,-MAY,4]
V3346:  CNTR                          ;8040
V3348:  VCTR   -520, -358, 0          ;1E9A 1DF8
V334C:  JMPL   VGMSGA                 ;E8F2  GO DO ALPHABET

CHEKER:          ;JSRL operand $9A7  (source name CHEKER, ALVROM.MAC:275)
;  Self test: grid (15 horizontal + 11 vertical lines)
;  refs: program ROM SCHEKR+$9 (ALTES2): LXL CHEKER at $DB78
;  refs: program ROM SCHEKR+$C (ALTES2): LAH CHEKER+1 at $DB7B
V334E:  SCAL   1, $00                 ;7100
V3350:  CNTR                          ;8040  [.REPT NLINES]
V3352:  VCTR   500, -539, 0           ;1DE5 01F4
V3356:  VCTR   -1000, 0, 5            ;0000 BC18
V335A:  CNTR                          ;8040
V335C:  VCTR   500, -462, 0           ;1E32 01F4
V3360:  VCTR   -1000, 0, 5            ;0000 BC18
V3364:  CNTR                          ;8040
V3366:  VCTR   500, -385, 0           ;1E7F 01F4
V336A:  VCTR   -1000, 0, 5            ;0000 BC18
V336E:  CNTR                          ;8040
V3370:  VCTR   500, -308, 0           ;1ECC 01F4
V3374:  VCTR   -1000, 0, 5            ;0000 BC18
V3378:  CNTR                          ;8040
V337A:  VCTR   500, -231, 0           ;1F19 01F4
V337E:  VCTR   -1000, 0, 5            ;0000 BC18
V3382:  CNTR                          ;8040
V3384:  VCTR   500, -154, 0           ;1F66 01F4
V3388:  VCTR   -1000, 0, 5            ;0000 BC18
V338C:  CNTR                          ;8040
V338E:  VCTR   500, -77, 0            ;1FB3 01F4
V3392:  VCTR   -1000, 0, 5            ;0000 BC18
V3396:  CNTR                          ;8040
V3398:  VCTR   500, 0, 0              ;0000 01F4
V339C:  VCTR   -1000, 0, 5            ;0000 BC18
V33A0:  CNTR                          ;8040
V33A2:  VCTR   500, 77, 0             ;004D 01F4
V33A6:  VCTR   -1000, 0, 5            ;0000 BC18
V33AA:  CNTR                          ;8040
V33AC:  VCTR   500, 154, 0            ;009A 01F4
V33B0:  VCTR   -1000, 0, 5            ;0000 BC18
V33B4:  CNTR                          ;8040
V33B6:  VCTR   500, 231, 0            ;00E7 01F4
V33BA:  VCTR   -1000, 0, 5            ;0000 BC18
V33BE:  CNTR                          ;8040
V33C0:  VCTR   500, 308, 0            ;0134 01F4
V33C4:  VCTR   -1000, 0, 5            ;0000 BC18
V33C8:  CNTR                          ;8040
V33CA:  VCTR   500, 385, 0            ;0181 01F4
V33CE:  VCTR   -1000, 0, 5            ;0000 BC18
V33D2:  CNTR                          ;8040
V33D4:  VCTR   500, 462, 0            ;01CE 01F4
V33D8:  VCTR   -1000, 0, 5            ;0000 BC18
V33DC:  CNTR                          ;8040
V33DE:  VCTR   500, 539, 0            ;021B 01F4
V33E2:  VCTR   -1000, 0, 5            ;0000 BC18
V33E6:  CNTR                          ;8040  [.REPT NLINES]
V33E8:  VCTR   -499, 540, 0           ;021C 1E0D
V33EC:  VCTR   0, -1080, 5            ;1BC8 A000
V33F0:  CNTR                          ;8040
V33F2:  VCTR   -399, 540, 0           ;021C 1E71
V33F6:  VCTR   0, -1080, 5            ;1BC8 A000
V33FA:  CNTR                          ;8040
V33FC:  VCTR   -299, 540, 0           ;021C 1ED5
V3400:  VCTR   0, -1080, 5            ;1BC8 A000
V3404:  CNTR                          ;8040
V3406:  VCTR   -199, 540, 0           ;021C 1F39
V340A:  VCTR   0, -1080, 5            ;1BC8 A000
V340E:  CNTR                          ;8040
V3410:  VCTR   -99, 540, 0            ;021C 1F9D
V3414:  VCTR   0, -1080, 5            ;1BC8 A000
V3418:  CNTR                          ;8040
V341A:  VCTR   1, 540, 0              ;021C 0001
V341E:  VCTR   0, -1080, 5            ;1BC8 A000
V3422:  CNTR                          ;8040
V3424:  VCTR   101, 540, 0            ;021C 0065
V3428:  VCTR   0, -1080, 5            ;1BC8 A000
V342C:  CNTR                          ;8040
V342E:  VCTR   201, 540, 0            ;021C 00C9
V3432:  VCTR   0, -1080, 5            ;1BC8 A000
V3436:  CNTR                          ;8040
V3438:  VCTR   301, 540, 0            ;021C 012D
V343C:  VCTR   0, -1080, 5            ;1BC8 A000
V3440:  CNTR                          ;8040
V3442:  VCTR   401, 540, 0            ;021C 0191
V3446:  VCTR   0, -1080, 5            ;1BC8 A000
V344A:  CNTR                          ;8040
V344C:  VCTR   501, 540, 0            ;021C 01F5
V3450:  VCTR   0, -1080, 5            ;1BC8 A000
V3454:  RTSL                          ;C000

HYSTER:          ;JSRL operand $A2B  (source name HYSTER, ALVROM.MAC:301)
;  Self test: hysteresis box
;  colour: 0 WHITE
;  refs: program ROM SHYSTE+$23 (ALTES2): LAH HYSTER+1 at $DBBD
;  refs: program ROM SHYSTE+$26 (ALTES2): LXL HYSTER at $DBC0
V3456:  JSRL   CNWHSC                 ;AA45
V3458:  VCTR   768, 0, 4              ;0000 8300  [CVEC MAX,0,.HATCH]
V345C:  VCTR   -768, 576, 0           ;0240 1D00  [CVEC 0,MAY,0]
V3460:  VCTR   0, -1152, 4            ;1B80 8000  [CVEC 0,-MAY,.HATCH]
V3464:  VCTR   -768, 576, 0           ;0240 1D00  [CVEC -MAX,0,0]
V3468:  VCTR   768, 0, 4              ;0000 8300  [CVEC 0,0,.HATCH]
V346C:  RTSL                          ;C000

EASING:          ;JSRL operand $A37  (source name EASING, ALVROM.MAC:309)
;  Self test: 'ERASING' (EAROM) message
;  colour: 0 WHITE
;  refs: program ROM DSPSYS+$7B (ALTES2): LAH EASING+1 at $D87F
;  refs: program ROM DSPSYS+$7E (ALTES2): LXL EASING at $D882
V346E:  JSRL   CNWHSC                 ;AA45
V3470:  VCTR   -320, 140, 0           ;008C 1EC0
V3474:  JSRL   CHAR_E                 ;A823  ERASING EAROMS MESSAGE
V3476:  JSRL   CHAR_R                 ;A87B
V3478:  JSRL   CHAR_A                 ;A800
V347A:  JSRL   CHAR_S                 ;A883
V347C:  JSRL   CHAR_I                 ;A842
V347E:  JSRL   CHAR_N                 ;A860
V3480:  JMPL   CHAR_G                 ;E832

COCMSG:          ;JSRL operand $A41  (source name COCMSG, ALVROM.MAC:318)
;  Self test: cocktail game marker 'C'
;  colour: 0 WHITE
;  refs: program ROM BADBOX+$4C (ALTES2): LAH COCMSG+1 at $DC61
;  refs: program ROM BADBOX+$4F (ALTES2): LXL COCMSG at $DC64
V3482:  JSRL   CNWHSC                 ;AA45  COCKTAIL GAME MESSAGE
V3484:  VCTR   -300, -40, 0           ;1FD8 1ED4
V3488:  JMPL   CHAR_C                 ;E815

CNWHSC:          ;JSRL operand $A45  (source name CNWHSC, ALVROM.MAC:321)
;  Common init: CSTAT WHITE, CNTR, SCAL 1,0
;  colour: 0 WHITE
;  no lit vectors - beam positioning / advance / vector-RAM list
;  refs: vector ROM JSRL at $3456
;  refs: vector ROM JSRL at $346E
;  refs: vector ROM JSRL at $3482
V348A:  CSTAT  0                      ;68C0  WHITE  COMMON INITIALIZE
V348C:  CNTR                          ;8040
V348E:  SCAL   1, $00                 ;7100
V3490:  RTSL                          ;C000

ROMRPI:          ;JSRL operand $A49  (source name ROMRPI, ALVROM.MAC:325)
;  Self test: bad-ROM report frame (midline, then position for text)
;  colour: 0 WHITE
;  refs: program ROM BADBOX+$69 (ALTES2): LAH ROMRPI+1 at $DC7E
;  refs: program ROM BADBOX+$6C (ALTES2): LXL ROMRPI at $DC81
V3492:  JSRL   BONDRY                 ;AA53
V3494:  CNTR                          ;8040
V3496:  SCAL   1, $00                 ;7100
V3498:  VCTR   500, 0, 0              ;0000 01F4  HORIZONTAL LINE ACCROSS MIDDLE
V349C:  VCTR   -1000, 0, 6            ;0000 DC18
V34A0:  VCTR   64, 448, 0             ;01C0 0040  POSITION FOR REPORT OF BAD ROM
V34A4:  RTSL                          ;C000  FALL INTO SCREEN BOUNDARIES

BONDRY:          ;JSRL operand $A53  (source name BONDRY, ALVROM.MAC:334)
;  Screen boundary rectangle (white)
;  colour: 0 WHITE
;  refs: vector ROM JSRL at $32B6
;  refs: vector ROM JSRL at $330A
;  refs: vector ROM JSRL at $3492
;  refs: vector ROM JSRL at $3DCE
;  refs: ... and 2 more (see cross reference)
V34A6:  CSTAT  0                      ;68C0  WHITE  SCREEN BOUNDARY
V34A8:  SCAL   1, $00                 ;7100

VORBOX:          ;JSRL operand $A55  (source name VORBOX, ALVROM.MAC:336)
;  Screen boundary rectangle, entry after colour/scale
;  refs: program ROM BOXPRO (ALSCO2): LDAH VORBOX+1 at $B102
;  refs: program ROM BOXPRO+$3 (ALSCO2): LXL VORBOX at $B105
;  refs: program ROM SHYSTE+$34 (ALTES2): LAH VORBOX+1 at $DBCE
;  refs: program ROM SHYSTE+$37 (ALTES2): LXL VORBOX at $DBD1
V34AA:  CNTR                          ;8040
V34AC:  VCTR   -500, -540, 0          ;1DE4 1E0C
V34B0:  VCTR   1000, 0, 6             ;0000 C3E8
V34B4:  VCTR   0, 1080, 6             ;0438 C000
V34B8:  VCTR   -1000, 0, 6            ;0000 DC18
V34BC:  VCTR   0, -1080, 6            ;1BC8 C000
V34C0:  RTSL                          ;C000

EXPL1:           ;JSRL operand $A61  (source name EXPL1, ALVROM.MAC:350)
;  Explosion, frame 1 of 4 (16 spokes; 1 = smallest) - PICLO PTEXP1+
;  colour: 0 WHITE
;  refs: program ROM table JSRL at $CEC8
;  refs: program ROM TEXTYP (ALDIS2): .BYTE PTEXP1 at $B7E5
V34C2:  CSTAT  0                      ;68C0  WHITE
V34C4:  VCTR   7, 3, 0                ;0003 0007  16 SPOKES 1 SCALE FULL INTENSITY  [SPOK16]
V34C8:  SVEC   -14, -6, 7             ;5DF9
V34CA:  VCTR   1, -3, 0               ;1FFD 0001
V34CE:  SVEC   12, 12, 7              ;46E6
V34D0:  VCTR   -3, 1, 0               ;0001 1FFD
V34D4:  SVEC   -6, -14, 7             ;59FD
V34D6:  VCTR   3, -1, 0               ;1FFF 0003
V34DA:  SVEC   0, 16, 7               ;48E0
V34DC:  VCTR   -3, -1, 0              ;1FFF 1FFD
V34E0:  SVEC   6, -14, 7              ;59E3
V34E2:  VCTR   3, 1, 0                ;0001 0003
V34E6:  SVEC   -12, 12, 7             ;46FA
V34E8:  VCTR   -1, -3, 0              ;1FFD 1FFF
V34EC:  SVEC   14, -6, 7              ;5DE7
V34EE:  VCTR   1, 3, 0                ;0003 0001
V34F2:  SVEC   -16, 0, 7              ;40F8
V34F4:  SVEC   8, 0, 0                ;4004
V34F6:  RTSL                          ;C000

EXPL2:           ;JSRL operand $A7C  (source name EXPL2, ALVROM.MAC:358)
;  Explosion, frame 2 of 4 (16 spokes; 1 = smallest) - PICLO PTEXP1+
;  colour: 0 WHITE
;  refs: program ROM table JSRL at $CECA
V34F8:  CSTAT  0                      ;68C0  WHITE
V34FA:  SVEC   14, 6, 0               ;4307  16 SPOKES 2 SCALE FULL INTENSITY  [SPOK16]
V34FC:  SVEC   -28, -10, 6            ;5BD2
V34FE:  SVEC   2, -6, 0               ;5D01
V3500:  SVEC   24, 26, 6              ;4DCC
V3502:  SVEC   -6, 2, 0               ;411D
V3504:  SVEC   -12, -26, 6            ;53DA
V3506:  SVEC   6, -2, 0               ;5F03
V3508:  VCTR   0, 32, 6               ;0020 C000
V350C:  SVEC   -6, -2, 0              ;5F1D
V350E:  SVEC   12, -26, 6             ;53C6
V3510:  SVEC   6, 2, 0                ;4103
V3512:  SVEC   -24, 26, 6             ;4DD4
V3514:  SVEC   -2, -6, 0              ;5D1F
V3516:  SVEC   28, -10, 6             ;5BCE
V3518:  SVEC   2, 6, 0                ;4301
V351A:  VCTR   -32, 0, 6              ;0000 DFE0
V351E:  SVEC   16, 0, 0               ;4008
V3520:  RTSL                          ;C000

EXPL3:           ;JSRL operand $A91  (source name EXPL3, ALVROM.MAC:366)
;  Explosion, frame 3 of 4 (16 spokes; 1 = smallest) - PICLO PTEXP1+
;  colour: 0 WHITE
;  refs: program ROM table JSRL at $CECC
V3522:  CSTAT  0                      ;68C0  WHITE
V3524:  SVEC   28, 12, 0              ;460E  16 SPOKES 4 SCALE FULL INTENSITY  [SPOK16]
V3526:  VCTR   -56, -24, 6            ;1FE8 DFC8
V352A:  SVEC   4, -12, 0              ;5A02
V352C:  VCTR   48, 48, 6              ;0030 C030
V3530:  SVEC   -12, 4, 0              ;421A
V3532:  VCTR   -24, -56, 6            ;1FC8 DFE8
V3536:  SVEC   12, -4, 0              ;5E06
V3538:  VCTR   0, 64, 6               ;0040 C000
V353C:  SVEC   -12, -4, 0             ;5E1A
V353E:  VCTR   24, -56, 6             ;1FC8 C018
V3542:  SVEC   12, 4, 0               ;4206
V3544:  VCTR   -48, 48, 6             ;0030 DFD0
V3548:  SVEC   -4, -12, 0             ;5A1E
V354A:  VCTR   56, -24, 6             ;1FE8 C038
V354E:  SVEC   4, 12, 0               ;4602
V3550:  VCTR   -64, 0, 6              ;0000 DFC0
V3554:  VCTR   32, 0, 0               ;0000 0020
V3558:  RTSL                          ;C000

EXPL4:           ;JSRL operand $AAD  (source name EXPL4, ALVROM.MAC:374)
;  Explosion, frame 4 of 4 (largest; no ICVEC) - PTEXP1+6
;  colour: 0 WHITE
;  refs: program ROM table JSRL at $CECE
V355A:  CSTAT  0                      ;68C0  WHITE
V355C:  VCTR   56, 24, 0              ;0018 0038  16 SPOKES 8 SCALE FULL INTENSITY  [SPOK16]
V3560:  VCTR   -112, -48, 6           ;1FD0 DF90
V3564:  SVEC   8, -24, 0              ;5404
V3566:  VCTR   96, 96, 6              ;0060 C060
V356A:  SVEC   -24, 8, 0              ;4414
V356C:  VCTR   -48, -112, 6           ;1F90 DFD0
V3570:  SVEC   24, -8, 0              ;5C0C
V3572:  VCTR   0, 128, 6              ;0080 C000
V3576:  SVEC   -24, -8, 0             ;5C14
V3578:  VCTR   48, -112, 6            ;1F90 C030
V357C:  SVEC   24, 8, 0               ;440C
V357E:  VCTR   -96, 96, 6             ;0060 DFA0
V3582:  SVEC   -8, -24, 0             ;541C
V3584:  VCTR   112, -48, 6            ;1FD0 C070
V3588:  SVEC   8, 24, 0               ;4C04
V358A:  VCTR   -128, 0, 6             ;0000 DF80
V358E:  VCTR   64, 0, 0               ;0000 0040
V3592:  RTSL                          ;C000

DIARA2:          ;JSRL operand $ACA  (source name DIARA2, ALVROM.MAC:384)
;  Player shot / charge: diamond of dots (PTCURS)
;  colour: 8 PSHCTR, 1 YELLOW
;  refs: program ROM table JSRL at $CED0
;  refs: program ROM DSPCHG+$16 (ALDIS2): LDA I,PTCURS at $B771
V3594:  CSTAT  8                      ;68C8  PSHCTR
V3596:  VCTR   0, 0, 0                ;0000 0000  [SCDOT 0,0]
V359A:  VCTR   0, 0, 1                ;0000 2000
V359E:  VCTR   7, 0, 0                ;0000 0007  [SCDOT 7,0]
V35A2:  VCTR   0, 0, 1                ;0000 2000
V35A6:  VCTR   -2, 5, 0               ;0005 1FFE  [SCDOT 5,5]
V35AA:  VCTR   0, 0, 1                ;0000 2000
V35AE:  VCTR   -5, 2, 0               ;0002 1FFB  [SCDOT 0,7]
V35B2:  VCTR   0, 0, 1                ;0000 2000
V35B6:  VCTR   -5, -2, 0              ;1FFE 1FFB  [SCDOT -5,5]
V35BA:  VCTR   0, 0, 1                ;0000 2000
V35BE:  VCTR   -2, -5, 0              ;1FFB 1FFE  [SCDOT -7,0]
V35C2:  VCTR   0, 0, 1                ;0000 2000
V35C6:  VCTR   2, -5, 0               ;1FFB 0002  [SCDOT -5,-5]
V35CA:  VCTR   0, 0, 1                ;0000 2000
V35CE:  VCTR   5, -2, 0               ;1FFE 0005  [SCDOT 0,-7]
V35D2:  VCTR   0, 0, 1                ;0000 2000
V35D6:  VCTR   5, 2, 0                ;0002 0005  [SCDOT 5,-5]
V35DA:  VCTR   0, 0, 1                ;0000 2000
V35DE:  CSTAT  1                      ;68C1  YELLOW
V35E0:  VCTR   10, 5, 0               ;0005 000A  [SCDOT 0F,0]
V35E4:  VCTR   0, 0, 1                ;0000 2000
V35E8:  VCTR   -4, 11, 0              ;000B 1FFC  [SCDOT 0B,0B]
V35EC:  VCTR   0, 0, 1                ;0000 2000
V35F0:  VCTR   -11, 4, 0              ;0004 1FF5  [SCDOT 0,0F]
V35F4:  VCTR   0, 0, 1                ;0000 2000
V35F8:  VCTR   -11, -4, 0             ;1FFC 1FF5  [SCDOT -0B,0B]
V35FC:  VCTR   0, 0, 1                ;0000 2000
V3600:  VCTR   0, -11, 0              ;1FF5 0000  [SCDOT -0B,0]
V3604:  VCTR   0, 0, 1                ;0000 2000
V3608:  VCTR   0, -11, 0              ;1FF5 0000  [SCDOT -0B,-0B]
V360C:  VCTR   0, 0, 1                ;0000 2000
V3610:  VCTR   11, 0, 0               ;0000 000B  [SCDOT 0,-0B]
V3614:  VCTR   0, 0, 1                ;0000 2000
V3618:  VCTR   11, 0, 0               ;0000 000B  [SCDOT 0B,-0B]
V361C:  VCTR   0, 0, 1                ;0000 2000
V3620:  RTSL                          ;C000

STAR1B:          ;JSRL operand $B11  (source name STAR1B, ALVROM.MAC:509)
;  Star field burst: intensity 15, STAR1 + STAR2
;  refs: vector ROM JMPL at $3B88
;  refs: vector ROM JMPL at $3B8E
;  refs: vector ROM JMPL at $3B94
;  refs: vector ROM JMPL at $3B9A
;  refs: ... and 3 more (see cross reference)
V3622:  STAT   $60F0                  ;60F0  intensity 15
V3624:  JSRL   STAR1                  ;AB14
V3626:  JMPL   STAR2                  ;EB6F

STAR1:           ;JSRL operand $B14  (source name STAR1, ALVROM.MAC:512)
;  Star field, frame 1 of 4 (warp dots) - PTSTR1+
;  refs: vector ROM JSRL at $3624
;  refs: program ROM table JSRL at $CED2
;  refs: program ROM DSTARF+$50 (ALDIS2): ADC I,PTSTR1 at $C59D
V3628:  VCTR   -32, 0, 0              ;0000 1FE0  [MSTAR1]
V362C:  VCTR   0, 0, 1                ;0000 2000
V3630:  VCTR   64, 48, 0              ;0030 0040
V3634:  VCTR   0, 0, 1                ;0000 2000
V3638:  VCTR   32, -48, 0             ;1FD0 0020
V363C:  VCTR   0, 0, 1                ;0000 2000
V3640:  VCTR   -32, 192, 0            ;00C0 1FE0
V3644:  VCTR   0, 0, 1                ;0000 2000
V3648:  VCTR   -224, -64, 0           ;1FC0 1F20
V364C:  VCTR   0, 0, 1                ;0000 2000
V3650:  VCTR   -16, -256, 0           ;1F00 1FF0
V3654:  VCTR   0, 0, 1                ;0000 2000
V3658:  VCTR   176, -160, 0           ;1F60 00B0
V365C:  VCTR   0, 0, 1                ;0000 2000
V3660:  VCTR   320, 160, 0            ;00A0 0140
V3664:  VCTR   0, 0, 1                ;0000 2000
V3668:  VCTR   -16, 288, 0            ;0120 1FF0
V366C:  VCTR   0, 0, 1                ;0000 2000
V3670:  VCTR   -176, 160, 0           ;00A0 1F50
V3674:  VCTR   0, 0, 1                ;0000 2000
V3678:  VCTR   -320, -48, 0           ;1FD0 1EC0
V367C:  VCTR   0, 0, 1                ;0000 2000
V3680:  VCTR   -64, -304, 0           ;1ED0 1FC0
V3684:  VCTR   0, 0, 1                ;0000 2000
V3688:  VCTR   32, -288, 0            ;1EE0 0020
V368C:  VCTR   0, 0, 1                ;0000 2000
V3690:  VCTR   320, -128, 0           ;1F80 0140
V3694:  VCTR   0, 0, 1                ;0000 2000
V3698:  VCTR   288, 128, 0            ;0080 0120
V369C:  VCTR   0, 0, 1                ;0000 2000
V36A0:  VCTR   64, 288, 0             ;0120 0040
V36A4:  VCTR   0, 0, 1                ;0000 2000
V36A8:  VCTR   -64, 352, 0            ;0160 1FC0
V36AC:  VCTR   0, 0, 1                ;0000 2000
V36B0:  VCTR   -320, 128, 0           ;0080 1EC0
V36B4:  VCTR   0, 0, 1                ;0000 2000
V36B8:  VCTR   -288, -32, 0           ;1FE0 1EE0
V36BC:  VCTR   0, 0, 1                ;0000 2000
V36C0:  VCTR   -224, -256, 0          ;1F00 1F20
V36C4:  VCTR   0, 0, 1                ;0000 2000
V36C8:  VCTR   32, -320, 0            ;1EC0 0020
V36CC:  VCTR   0, 0, 1                ;0000 2000
V36D0:  VCTR   0, -256, 0             ;1F00 0000
V36D4:  VCTR   0, 0, 1                ;0000 2000
V36D8:  VCTR   448, 416, 0            ;01A0 01C0
V36DC:  RTSL                          ;C000

STAR2:           ;JSRL operand $B6F  (source name STAR2, ALVROM.MAC:513)
;  Star field, frame 2 of 4 (warp dots) - PTSTR1+
;  refs: vector ROM JMPL at $3626
;  refs: program ROM table JSRL at $CED4
V36DE:  VCTR   32, 32, 0              ;0020 0020  [MSTAR2]
V36E2:  VCTR   0, 0, 1                ;0000 2000
V36E6:  VCTR   -64, 32, 0             ;0020 1FC0
V36EA:  VCTR   0, 0, 1                ;0000 2000
V36EE:  VCTR   -64, -128, 0           ;1F80 1FC0
V36F2:  VCTR   0, 0, 1                ;0000 2000
V36F6:  VCTR   128, -96, 0            ;1FA0 0080
V36FA:  VCTR   0, 0, 1                ;0000 2000
V36FE:  VCTR   128, 128, 0            ;0080 0080
V3702:  VCTR   0, 0, 1                ;0000 2000
V3706:  VCTR   -32, 160, 0            ;00A0 1FE0
V370A:  VCTR   0, 0, 1                ;0000 2000
V370E:  VCTR   -96, 96, 0             ;0060 1FA0
V3712:  VCTR   0, 0, 1                ;0000 2000
V3716:  VCTR   -160, -32, 0           ;1FE0 1F60
V371A:  VCTR   0, 0, 1                ;0000 2000
V371E:  VCTR   -128, -160, 0          ;1F60 1F80
V3722:  VCTR   0, 0, 1                ;0000 2000
V3726:  VCTR   96, -288, 0            ;1EE0 0060
V372A:  VCTR   0, 0, 1                ;0000 2000
V372E:  VCTR   368, -16, 0            ;1FF0 0170
V3732:  VCTR   0, 0, 1                ;0000 2000
V3736:  VCTR   144, 368, 0            ;0170 0090
V373A:  VCTR   0, 0, 1                ;0000 2000
V373E:  VCTR   -128, 224, 0           ;00E0 1F80
V3742:  VCTR   0, 0, 1                ;0000 2000
V3746:  VCTR   -288, 96, 0            ;0060 1EE0
V374A:  VCTR   0, 0, 1                ;0000 2000
V374E:  VCTR   -288, -256, 0          ;1F00 1EE0
V3752:  VCTR   0, 0, 1                ;0000 2000
V3756:  VCTR   0, -352, 0             ;1EA0 0000
V375A:  VCTR   0, 0, 1                ;0000 2000
V375E:  VCTR   128, -224, 0           ;1F20 0080
V3762:  VCTR   0, 0, 1                ;0000 2000
V3766:  VCTR   224, -64, 0            ;1FC0 00E0
V376A:  VCTR   0, 0, 1                ;0000 2000
V376E:  VCTR   384, 64, 0             ;0040 0180
V3772:  VCTR   0, 0, 1                ;0000 2000
V3776:  VCTR   32, 224, 0             ;00E0 0020
V377A:  VCTR   0, 0, 1                ;0000 2000
V377E:  RTSL                          ;C000

STAR3:           ;JSRL operand $BC0  (source name STAR3, ALVROM.MAC:514)
;  Star field, frame 3 of 4 (warp dots) - PTSTR1+
;  refs: program ROM table JSRL at $CED6
V3780:  VCTR   64, 64, 0              ;0040 0040  [MSTAR3]
V3784:  VCTR   0, 0, 1                ;0000 2000
V3788:  VCTR   -64, 32, 0             ;0020 1FC0
V378C:  VCTR   0, 0, 1                ;0000 2000
V3790:  VCTR   -192, -64, 0           ;1FC0 1F40
V3794:  VCTR   0, 0, 1                ;0000 2000
V3798:  VCTR   96, -208, 0            ;1F30 0060
V379C:  VCTR   0, 0, 1                ;0000 2000
V37A0:  VCTR   256, 80, 0             ;0050 0100
V37A4:  VCTR   0, 0, 1                ;0000 2000
V37A8:  VCTR   32, 128, 0             ;0080 0020
V37AC:  VCTR   0, 0, 1                ;0000 2000
V37B0:  VCTR   32, 192, 0             ;00C0 0020
V37B4:  VCTR   0, 0, 1                ;0000 2000
V37B8:  VCTR   -224, 64, 0            ;0040 1F20
V37BC:  VCTR   0, 0, 1                ;0000 2000
V37C0:  VCTR   -224, -128, 0          ;1F80 1F20
V37C4:  VCTR   0, 0, 1                ;0000 2000
V37C8:  VCTR   -32, -192, 0           ;1F40 1FE0
V37CC:  VCTR   0, 0, 1                ;0000 2000
V37D0:  VCTR   128, -320, 0           ;1EC0 0080
V37D4:  VCTR   0, 0, 1                ;0000 2000
V37D8:  VCTR   256, 0, 0              ;0000 0100
V37DC:  VCTR   0, 0, 1                ;0000 2000
V37E0:  VCTR   192, 160, 0            ;00A0 00C0
V37E4:  VCTR   0, 0, 1                ;0000 2000
V37E8:  VCTR   64, 320, 0             ;0140 0040
V37EC:  VCTR   0, 0, 1                ;0000 2000
V37F0:  VCTR   -208, 272, 0           ;0110 1F30
V37F4:  VCTR   0, 0, 1                ;0000 2000
V37F8:  VCTR   -304, 16, 0            ;0010 1ED0
V37FC:  VCTR   0, 0, 1                ;0000 2000
V3800:  VCTR   -272, -192, 0          ;1F40 1EF0
V3804:  VCTR   0, 0, 1                ;0000 2000
V3808:  VCTR   -16, -256, 0           ;1F00 1FF0
V380C:  VCTR   0, 0, 1                ;0000 2000
V3810:  VCTR   32, -256, 0            ;1F00 0020
V3814:  VCTR   0, 0, 1                ;0000 2000
V3818:  VCTR   224, -192, 0           ;1F40 00E0
V381C:  VCTR   0, 0, 1                ;0000 2000
V3820:  VCTR   416, 0, 0              ;0000 01A0
V3824:  VCTR   0, 0, 1                ;0000 2000
V3828:  RTSL                          ;C000

STAR4:           ;JSRL operand $C15  (source name STAR4, ALVROM.MAC:515)
;  Star field, frame 4 of 4 (warp dots) - PTSTR1+
;  refs: program ROM table JSRL at $CED8
V382A:  VCTR   -32, -64, 0            ;1FC0 1FE0  [MSTAR4]
V382E:  VCTR   0, 0, 1                ;0000 2000
V3832:  VCTR   160, 0, 0              ;0000 00A0
V3836:  VCTR   0, 0, 1                ;0000 2000
V383A:  VCTR   0, 160, 0              ;00A0 0000
V383E:  VCTR   0, 0, 1                ;0000 2000
V3842:  VCTR   -224, 32, 0            ;0020 1F20
V3846:  VCTR   0, 0, 1                ;0000 2000
V384A:  VCTR   -96, -192, 0           ;1F40 1FA0
V384E:  VCTR   0, 0, 1                ;0000 2000
V3852:  VCTR   256, -160, 0           ;1F60 0100
V3856:  VCTR   0, 0, 1                ;0000 2000
V385A:  VCTR   192, 224, 0            ;00E0 00C0
V385E:  VCTR   0, 0, 1                ;0000 2000
V3862:  VCTR   -128, 224, 0           ;00E0 1F80
V3866:  VCTR   0, 0, 1                ;0000 2000
V386A:  VCTR   -256, 64, 0            ;0040 1F00
V386E:  VCTR   0, 0, 1                ;0000 2000
V3872:  VCTR   -192, -224, 0          ;1F20 1F40
V3876:  VCTR   0, 0, 1                ;0000 2000
V387A:  VCTR   96, -320, 0            ;1EC0 0060
V387E:  VCTR   0, 0, 1                ;0000 2000
V3882:  VCTR   320, -48, 0            ;1FD0 0140
V3886:  VCTR   0, 0, 1                ;0000 2000
V388A:  VCTR   160, 80, 0             ;0050 00A0
V388E:  VCTR   0, 0, 1                ;0000 2000
V3892:  VCTR   160, 288, 0            ;0120 00A0
V3896:  VCTR   0, 0, 1                ;0000 2000
V389A:  VCTR   -256, 288, 0           ;0120 1F00
V389E:  VCTR   0, 0, 1                ;0000 2000
V38A2:  VCTR   -384, -32, 0           ;1FE0 1E80
V38A6:  VCTR   0, 0, 1                ;0000 2000
V38AA:  VCTR   -224, -224, 0          ;1F20 1F20
V38AE:  VCTR   0, 0, 1                ;0000 2000
V38B2:  VCTR   32, -192, 0            ;1F40 0020
V38B6:  VCTR   0, 0, 1                ;0000 2000
V38BA:  VCTR   64, -384, 0            ;1E80 0040
V38BE:  VCTR   0, 0, 1                ;0000 2000
V38C2:  VCTR   608, 96, 0             ;0060 0260
V38C6:  VCTR   0, 0, 1                ;0000 2000
V38CA:  RTSL                          ;C000

SPIRA1:          ;JSRL operand $C66  (source name SPIRA1, ALVROM.MAC:522)
;  Spiker/trailer spiral, frame 1 of 4 - PTSPI1+
;  colour: 5 GREEN
;  refs: program ROM table JSRL at $CEDA
;  refs: program ROM TRAPIC+$9 (ALDIS2): ADC I,PTSPI1 at $B62B
;  refs: program ROM TRATAB (ALDIS2): .BYTE PTSPI1,PTSPI1+2 at $B630
;  refs: program ROM TRATAB+$2 (ALDIS2): .BYTE PTSPI1+4,PTSPI1+6 at $B632
V38CC:  CSTAT  5                      ;68C5  GREEN
V38CE:  SVEC   2, -2, 1               ;5F21  [SCVEC 1,-1,CB]
V38D0:  SVEC   -2, -2, 1              ;5F3F  [SCVEC 0,-2,CB]
V38D2:  SVEC   -4, 0, 1               ;403E  [SCVEC -2,-2,CB]
V38D4:  SVEC   -4, 4, 1               ;423E  [SCVEC -4,0,CB]
V38D6:  SVEC   0, 8, 1                ;4420  [SCVEC -4,4,CB]
V38D8:  SVEC   8, 4, 1                ;4224  [SCVEC 0,6,CB]
V38DA:  SVEC   10, -2, 1              ;5F25  [SCVEC 5,5,CB]
V38DC:  SVEC   6, -10, 1              ;5B23  [SCVEC 8,0,CB]
V38DE:  SVEC   -2, -14, 1             ;593F  [SCVEC 7,-7,CB]
V38E0:  SVEC   -14, -6, 1             ;5D39  [SCVEC 0,-0A,CB]
V38E2:  SVEC   -16, 4, 1              ;4238  [SCVEC -8,-8,CB]
V38E4:  SVEC   -8, 16, 1              ;483C  [SCVEC -0C,0,CB]
V38E6:  SVEC   6, 18, 1               ;4923  [SCVEC -9,9,CB]
V38E8:  SVEC   18, 10, 1              ;4529  [SCVEC 0,0E,CB]
V38EA:  SVEC   22, -6, 1              ;5D2B  [SCVEC 0B,0B,CB]
V38EC:  SVEC   10, -22, 1             ;5525  [SCVEC 10,0,CB]
V38EE:  SVEC   -8, -24, 1             ;543C  [SCVEC 0C,-0C,CB]
V38F0:  SVEC   -24, -12, 1            ;5A34  [SCVEC 0,-12,CB]
V38F2:  SVEC   -28, 8, 1              ;4432  [SCVEC -0E,-0E,CB]
V38F4:  SVEC   -12, 28, 1             ;4E3A  [SCVEC -14,0,CB]
V38F6:  SVEC   10, 30, 1              ;4F25  [SCVEC -0F,0F,CB]
V38F8:  RTSL                          ;C000

SPIRA2:          ;JSRL operand $C7D  (source name SPIRA2, ALVROM.MAC:547)
;  Spiker/trailer spiral, frame 2 of 4 - PTSPI1+
;  colour: 5 GREEN
;  refs: program ROM table JSRL at $CEDC
V38FA:  CSTAT  5                      ;68C5  GREEN
V38FC:  SVEC   2, 2, 1                ;4121  [SCVEC 1,1,CB]
V38FE:  SVEC   2, -2, 1               ;5F21  [SCVEC 2,0,CB]
V3900:  SVEC   0, -4, 1               ;5E20  [SCVEC 2,-2,CB]
V3902:  SVEC   -4, -4, 1              ;5E3E  [SCVEC 0,-4,CB]
V3904:  SVEC   -8, 0, 1               ;403C  [SCVEC -4,-4,CB]
V3906:  SVEC   -4, 8, 1               ;443E  [SCVEC -6,0,CB]
V3908:  SVEC   2, 10, 1               ;4521  [SCVEC -5,5,CB]
V390A:  SVEC   10, 6, 1               ;4325  [SCVEC 0,8,CB]
V390C:  SVEC   14, -2, 1              ;5F27  [SCVEC 7,7,CB]
V390E:  SVEC   6, -14, 1              ;5923  [SCVEC 0A,0,CB]
V3910:  SVEC   -4, -16, 1             ;583E  [SCVEC 8,-8,CB]
V3912:  SVEC   -16, -8, 1             ;5C38  [SCVEC 0,-0C,CB]
V3914:  SVEC   -18, 6, 1              ;4337  [SCVEC -9,-9,CB]
V3916:  SVEC   -10, 18, 1             ;493B  [SCVEC -0E,0,CB]
V3918:  SVEC   6, 22, 1               ;4B23  [SCVEC -0B,0B,CB]
V391A:  SVEC   22, 10, 1              ;452B  [SCVEC 0,10,CB]
V391C:  SVEC   24, -8, 1              ;5C2C  [SCVEC 0C,0C,CB]
V391E:  SVEC   12, -24, 1             ;5426  [SCVEC 12,0,CB]
V3920:  SVEC   -8, -28, 1             ;523C  [SCVEC 0E,-0E,CB]
V3922:  SVEC   -28, -12, 1            ;5A32  [SCVEC 0,-14,CB]
V3924:  SVEC   -30, 10, 1             ;4531  [SCVEC -0F,-0F,CB]
V3926:  RTSL                          ;C000

SPIRA3:          ;JSRL operand $C94  (source name SPIRA3, ALVROM.MAC:572)
;  Spiker/trailer spiral, frame 3 of 4 - PTSPI1+
;  colour: 5 GREEN
;  refs: program ROM table JSRL at $CEDE
V3928:  CSTAT  5                      ;68C5  GREEN
V392A:  SVEC   -2, 2, 1               ;413F  [SCVEC -1,1,CB]
V392C:  SVEC   2, 2, 1                ;4121  [SCVEC 0,2,CB]
V392E:  SVEC   4, 0, 1                ;4022  [SCVEC 2,2,CB]
V3930:  SVEC   4, -4, 1               ;5E22  [SCVEC 4,0,CB]
V3932:  SVEC   0, -8, 1               ;5C20  [SCVEC 4,-4,CB]
V3934:  SVEC   -8, -4, 1              ;5E3C  [SCVEC 0,-6,CB]
V3936:  SVEC   -10, 2, 1              ;413B  [SCVEC -5,-5,CB]
V3938:  SVEC   -6, 10, 1              ;453D  [SCVEC -8,0,CB]
V393A:  SVEC   2, 14, 1               ;4721  [SCVEC -7,7,CB]
V393C:  SVEC   14, 6, 1               ;4327  [SCVEC 0,0A,CB]
V393E:  SVEC   16, -4, 1              ;5E28  [SCVEC 8,8,CB]
V3940:  SVEC   8, -16, 1              ;5824  [SCVEC 0C,0,CB]
V3942:  SVEC   -6, -18, 1             ;573D  [SCVEC 9,-9,CB]
V3944:  SVEC   -18, -10, 1            ;5B37  [SCVEC 0,-0E,CB]
V3946:  SVEC   -22, 6, 1              ;4335  [SCVEC -0B,-0B,CB]
V3948:  SVEC   -10, 22, 1             ;4B3B  [SCVEC -10,0,CB]
V394A:  SVEC   8, 24, 1               ;4C24  [SCVEC -0C,0C,CB]
V394C:  SVEC   24, 12, 1              ;462C  [SCVEC 0,12,CB]
V394E:  SVEC   28, -8, 1              ;5C2E  [SCVEC 0E,0E,CB]
V3950:  SVEC   12, -28, 1             ;5226  [SCVEC 14,0,CB]
V3952:  SVEC   -10, -30, 1            ;513B  [SCVEC 0F,-0F,CB]
V3954:  RTSL                          ;C000

SPIRA4:          ;JSRL operand $CAB  (source name SPIRA4, ALVROM.MAC:597)
;  Spiker/trailer spiral, frame 4 of 4 - PTSPI1+
;  colour: 5 GREEN
;  refs: program ROM table JSRL at $CEE0
V3956:  CSTAT  5                      ;68C5  GREEN
V3958:  SVEC   -2, -2, 1              ;5F3F  [SCVEC -1,-1,CB]
V395A:  SVEC   -2, 2, 1               ;413F  [SCVEC -2,0,CB]
V395C:  SVEC   0, 4, 1                ;4220  [SCVEC -2,2,CB]
V395E:  SVEC   4, 4, 1                ;4222  [SCVEC 0,4,CB]
V3960:  SVEC   8, 0, 1                ;4024  [SCVEC 4,4,CB]
V3962:  SVEC   4, -8, 1               ;5C22  [SCVEC 6,0,CB]
V3964:  SVEC   -2, -10, 1             ;5B3F  [SCVEC 5,-5,CB]
V3966:  SVEC   -10, -6, 1             ;5D3B  [SCVEC 0,-8,CB]
V3968:  SVEC   -14, 2, 1              ;4139  [SCVEC -7,-7,CB]
V396A:  SVEC   -6, 14, 1              ;473D  [SCVEC -0A,0,CB]
V396C:  SVEC   4, 16, 1               ;4822  [SCVEC -8,8,CB]
V396E:  SVEC   16, 8, 1               ;4428  [SCVEC 0,0C,CB]
V3970:  SVEC   18, -6, 1              ;5D29  [SCVEC 9,9,CB]
V3972:  SVEC   10, -18, 1             ;5725  [SCVEC 0E,0,CB]
V3974:  SVEC   -6, -22, 1             ;553D  [SCVEC 0B,-0B,CB]
V3976:  SVEC   -22, -10, 1            ;5B35  [SCVEC 0,-10,CB]
V3978:  SVEC   -24, 8, 1              ;4434  [SCVEC -0C,-0C,CB]
V397A:  SVEC   -12, 24, 1             ;4C3A  [SCVEC -12,0,CB]
V397C:  SVEC   8, 28, 1               ;4E24  [SCVEC -0E,0E,CB]
V397E:  SVEC   28, 12, 1              ;462E  [SCVEC 0,14,CB]
V3980:  SVEC   30, -10, 1             ;5B2F  [SCVEC 0F,0F,CB]
V3982:  RTSL                          ;C000

TANKP:           ;JSRL operand $CC2  (source name TANKP, ALVROM.MAC:626)
;  Pulsar tanker (tanker body + pulsar zigzag) - PTTANP
;  colour: 4 TURQOI, 2 PURPLE
;  refs: program ROM table JSRL at $CF12
;  refs: program ROM TANTAB (ALDIS2): .BYTE PTTANK,PTTANK,PTTANP,PTTANF at $B61E
V3984:  CSTAT  4                      ;68C4  TURQOI
V3986:  SVEC   -10, -4, 0             ;5E1B  [SCVEC -5,-2,0]
V3988:  SVEC   4, 16, 1               ;4822  [SCVEC -3,6,CB]
V398A:  SVEC   6, -24, 1              ;5423  [SCVEC 0,-6,CB]
V398C:  SVEC   6, 24, 1               ;4C23  [SCVEC 3,6,CB]
V398E:  SVEC   4, -16, 1              ;5822  [SCVEC 5,-2,CB]
V3990:  VCTR   54, 4, 0               ;0004 0036  [SCVEC 20,0,0]
V3994:  JMPL   GENTNK                 ;ECDA

TANKF:           ;JSRL operand $CCB  (source name TANKF, ALVROM.MAC:636)
;  Fuseball tanker (tanker body + coloured cross) - PTTANF
;  colour: 7 BLUE (BLULET), 3 RED, 5 GREEN, 1 YELLOW, 2 PURPLE
;  refs: program ROM table JSRL at $CF14
;  refs: program ROM TANTAB (ALDIS2): .BYTE PTTANK,PTTANK,PTTANP,PTTANF at $B61E
V3996:  CSTAT  7                      ;68C7  BLUE (BLULET)
V3998:  SVEC   -24, 0, 1              ;4034  [SCVEC -0C,0,CB]
V399A:  SVEC   24, 24, 0              ;4C0C  [SCVEC 0,0C,0]
V399C:  CSTAT  3                      ;68C3  RED
V399E:  SVEC   0, -24, 1              ;5420  [SCVEC 0,0,CB]
V39A0:  CSTAT  5                      ;68C5  GREEN
V39A2:  SVEC   0, -24, 1              ;5420  [SCVEC 0,-0C,CB]
V39A4:  SVEC   0, 24, 0               ;4C00  [SCVEC 0,0,0]
V39A6:  CSTAT  1                      ;68C1  YELLOW
V39A8:  SVEC   24, 0, 1               ;402C  [SCVEC 0C,0,CB]
V39AA:  VCTR   40, 0, 0               ;0000 0028  [SCVEC 20,0,0]
V39AE:  JMPL   GENTNK                 ;ECDA

TANKR:           ;JSRL operand $CD8  (source name TANKR, ALVROM.MAC:650)
;  Tanker (plain flipper tanker) - PTTANK
;  colour: 2 PURPLE
;  refs: program ROM table JSRL at $CEE2
;  refs: program ROM TANTAB (ALDIS2): .BYTE PTTANK,PTTANK,PTTANP,PTTANF at $B61E
V39B0:  VCTR   64, 0, 0               ;0000 0040  [SCVEC 20,0,0]

GENTNK:          ;JSRL operand $CDA  (source name GENTNK, ALVROM.MAC:651)
;  Tanker body (purple), shared tail of TANKR/TANKP/TANKF
;  colour: 2 PURPLE
;  refs: vector ROM JMPL at $3994
;  refs: vector ROM JMPL at $39AE
V39B4:  CSTAT  2                      ;68C2  PURPLE
V39B6:  VCTR   -64, 64, 1             ;0040 3FC0  [SCVEC 0,20,CB]
V39BA:  VCTR   0, -40, 1              ;1FD8 2000  [SCVEC 0,0C,CB]
V39BE:  VCTR   64, -24, 1             ;1FE8 2040  [SCVEC 20,0,CB]
V39C2:  VCTR   -40, 0, 1              ;0000 3FD8  [SCVEC 0C,0,CB]
V39C6:  SVEC   -24, 24, 1             ;4C34  [SCVEC 0,0C,CB]
V39C8:  SVEC   -24, -24, 1            ;5434  [SCVEC -0C,0,CB]
V39CA:  VCTR   24, 64, 1              ;0040 2018  [SCVEC 0,20,CB]
V39CE:  VCTR   -64, -64, 1            ;1FC0 3FC0  [SCVEC -20,0,CB]
V39D2:  VCTR   40, 0, 1               ;0000 2028  [SCVEC -0C,0,CB]
V39D6:  SVEC   24, -24, 1             ;542C  [SCVEC 0,-0C,CB]
V39D8:  VCTR   -64, 24, 1             ;0018 3FC0  [SCVEC -20,0,CB]
V39DC:  VCTR   64, -64, 1             ;1FC0 2040  [SCVEC 0,-20,CB]
V39E0:  VCTR   0, 40, 1               ;0028 2000  [SCVEC 0,-0C,CB]
V39E4:  SVEC   24, 24, 1              ;4C2C  [SCVEC 0C,0,CB]
V39E6:  VCTR   -24, -64, 1            ;1FC0 3FE8  [SCVEC 0,-20,CB]
V39EA:  VCTR   64, 64, 1              ;0040 2040  [SCVEC 20,0,CB]
V39EE:  VCTR   -40, 0, 1              ;0000 3FD8  [SCVEC 0C,0,CB]
V39F2:  RTSL                          ;C000

SPARK1:          ;JSRL operand $CFA  (source name SPARK1, ALVROM.MAC:673)
;  Sparkle (shot hit), frame 1 of 2 - PTSPAR+
;  colour: 1 YELLOW
;  refs: program ROM table JSRL at $CEE4
;  refs: program ROM TEXTYP+$5 (ALDIS2): .BYTE PTSPAR at $B7EA
;  refs: program ROM TIPACT+$44 (ALDIS2): ADC I,PTSPAR at $C70B
V39F4:  CSTAT  1                      ;68C1  YELLOW
V39F6:  VCTR   32, 0, 0               ;0000 0020  [SCVEC 10,0,0]
V39FA:  VCTR   0, 0, 7                ;0000 E000
V39FE:  VCTR   -64, 0, 0              ;0000 1FC0  [SCVEC -10,0,0]
V3A02:  VCTR   0, 0, 7                ;0000 E000
V3A06:  VCTR   32, 32, 0              ;0020 0020  [SCVEC 0,10,0]
V3A0A:  VCTR   0, 0, 7                ;0000 E000
V3A0E:  VCTR   0, -64, 0              ;1FC0 0000  [SCVEC 0,-10,0]
V3A12:  VCTR   0, 0, 7                ;0000 E000
V3A16:  SCAL   1, $00                 ;7100
V3A18:  RTSL                          ;C000

SPARK2:          ;JSRL operand $D0D  (source name SPARK2, ALVROM.MAC:686)
;  Sparkle (shot hit), frame 2 of 2 - PTSPAR+
;  colour: 1 YELLOW
;  refs: program ROM table JSRL at $CEE6
V3A1A:  CSTAT  1                      ;68C1  YELLOW
V3A1C:  VCTR   32, 32, 0              ;0020 0020  [SCVEC 10,10,0]
V3A20:  VCTR   0, 0, 7                ;0000 E000
V3A24:  VCTR   0, -64, 0              ;1FC0 0000  [SCVEC 10,-10,0]
V3A28:  VCTR   0, 0, 7                ;0000 E000
V3A2C:  VCTR   -64, 64, 0             ;0040 1FC0  [SCVEC -10,10,0]
V3A30:  VCTR   0, 0, 7                ;0000 E000
V3A34:  VCTR   0, -64, 0              ;1FC0 0000  [SCVEC -10,-10,0]
V3A38:  VCTR   0, 0, 7                ;0000 E000
V3A3C:  SCAL   1, $00                 ;7100
V3A3E:  RTSL                          ;C000

ESHOT1:          ;JSRL operand $D20  (source name ESHOT1, ALVROM.MAC:725)
;  Enemy shot, frame 1 of 4 - PTESHO+
;  colour: 0 WHITE, 3 RED
;  refs: program ROM table JSRL at $CEE8
;  refs: program ROM DSPCHG+$21 (ALDIS2): ADC I,PTESHO at $B77C
V3A40:  CSTAT  0                      ;68C0  WHITE  [MESHO1]
V3A42:  VCTR   -11, 11, 0             ;000B 1FF5
V3A46:  SVEC   -6, 6, 6               ;43DD
V3A48:  SVEC   0, -28, 0              ;5200
V3A4A:  SVEC   6, -6, 6               ;5DC3
V3A4C:  SVEC   28, 0, 0               ;400E
V3A4E:  SVEC   -6, 6, 6               ;43DD
V3A50:  SVEC   0, 28, 0               ;4E00
V3A52:  SVEC   6, -6, 6               ;5DC3
V3A54:  VCTR   -11, -5, 0             ;1FFB 1FF5
V3A58:  CSTAT  3                      ;68C3  RED
V3A5A:  VCTR   0, 0, 6                ;0000 C000
V3A5E:  SVEC   -12, 0, 0              ;401A
V3A60:  VCTR   0, 0, 6                ;0000 C000
V3A64:  SVEC   0, -12, 0              ;5A00
V3A66:  VCTR   0, 0, 6                ;0000 C000
V3A6A:  SVEC   12, 0, 0               ;4006
V3A6C:  VCTR   0, 0, 6                ;0000 C000
V3A70:  RTSL                          ;C000

ESHOT2:          ;JSRL operand $D39  (source name ESHOT2, ALVROM.MAC:728)
;  Enemy shot, frame 2 of 4 - PTESHO+
;  colour: 0 WHITE, 3 RED
;  refs: program ROM table JSRL at $CEEA
V3A72:  CSTAT  0                      ;68C0  WHITE
V3A74:  SVEC   -18, 18, 0             ;4917  [SCVEC -18.,12,0]
V3A76:  SVEC   0, -14, 6              ;59C0  [SCVEC -18.,4,CB]
V3A78:  SVEC   10, -18, 0             ;5705  [SCVEC -8,-14.,0]
V3A7A:  SVEC   0, -8, 6               ;5CC0  [SCVEC -8,-22.,CB]
V3A7C:  SVEC   26, 10, 0              ;450D  [SCVEC 18.,-12.,0]
V3A7E:  SVEC   0, 8, 6                ;44C0  [SCVEC 18.,-4,CB]
V3A80:  SVEC   -10, 18, 0             ;491B  [SCVEC 8,14.,0]
V3A82:  SVEC   0, 8, 6                ;44C0  [SCVEC 8,22.,CB]
V3A84:  VCTR   -11, -15, 0            ;1FF1 1FF5  [SCVEC -3,7,0]
V3A88:  CSTAT  3                      ;68C3  RED
V3A8A:  VCTR   0, 0, 6                ;0000 C000
V3A8E:  SVEC   -4, -4, 0              ;5E1E  [SCVEC -7,3,0]
V3A90:  VCTR   0, 0, 6                ;0000 C000
V3A94:  SVEC   10, -10, 0             ;5B05  [SCVEC 3,-7,0]
V3A96:  VCTR   0, 0, 6                ;0000 C000
V3A9A:  SVEC   4, 10, 0               ;4502  [SCVEC 7,3,0]
V3A9C:  VCTR   0, 0, 6                ;0000 C000
V3AA0:  RTSL                          ;C000

ESHOT3:          ;JSRL operand $D51  (source name ESHOT3, ALVROM.MAC:749)
;  Enemy shot, frame 3 of 4 - PTESHO+
;  colour: 0 WHITE, 3 RED
;  refs: program ROM table JSRL at $CEEC
V3AA2:  CSTAT  0                      ;68C0  WHITE
V3AA4:  VCTR   -17, 3, 0              ;0003 1FEF  [SCVEC -17.,3,0]
V3AA8:  SVEC   -6, -6, 6              ;5DDD  [SCVEC -23.,-3,CB]
V3AAA:  SVEC   20, -20, 0             ;560A  [SCVEC -3,-23.,0]
V3AAC:  SVEC   6, 6, 6                ;43C3  [SCVEC 3,-17.,CB]
V3AAE:  SVEC   14, 14, 0              ;4707  [SCVEC 17.,-3,0]
V3AB0:  SVEC   6, 6, 6                ;43C3  [SCVEC 23.,3,CB]
V3AB2:  SVEC   -20, 20, 0             ;4A16  [SCVEC 3,23.,0]
V3AB4:  SVEC   -6, 0, 6               ;40DD  [SCVEC -3,17,CB]
V3AB6:  VCTR   3, -15, 0              ;1FF1 0003  [SCVEC 0,8,0]
V3ABA:  CSTAT  3                      ;68C3  RED
V3ABC:  VCTR   0, 0, 6                ;0000 C000
V3AC0:  SVEC   -8, -8, 0              ;5C1C  [SCVEC -8,0,0]
V3AC2:  VCTR   0, 0, 6                ;0000 C000
V3AC6:  SVEC   8, -8, 0               ;5C04  [SCVEC 0,-8,0]
V3AC8:  VCTR   0, 0, 6                ;0000 C000
V3ACC:  SVEC   8, 8, 0                ;4404  [SCVEC 8,0,0]
V3ACE:  VCTR   0, 0, 6                ;0000 C000
V3AD2:  RTSL                          ;C000

ESHOT4:          ;JSRL operand $D6A  (source name ESHOT4, ALVROM.MAC:770)
;  Enemy shot, frame 4 of 4 - PTESHO+
;  colour: 0 WHITE, 3 RED
;  refs: program ROM table JSRL at $CEEE
V3AD4:  CSTAT  0                      ;68C0  WHITE
V3AD6:  SVEC   -22, -8, 0             ;5C15  [SCVEC -22.,-8,0]
V3AD8:  SVEC   8, 0, 6                ;40C4  [SCVEC -14.,-8,CB]
V3ADA:  SVEC   18, -10, 0             ;5B09  [SCVEC 4,-18.,0]
V3ADC:  SVEC   8, 0, 6                ;40C4  [SCVEC 12.,-18.,CB]
V3ADE:  SVEC   2, 26, 0               ;4D01  [SCVEC 14.,8,0]
V3AE0:  SVEC   8, 0, 6                ;40C4  [SCVEC 22.,8,CB]
V3AE2:  SVEC   -26, 10, 0             ;4513  [SCVEC -4,18.,0]
V3AE4:  SVEC   -8, 0, 6               ;40DC  [SCVEC -12.,18.,CB]
V3AE6:  VCTR   5, -15, 0              ;1FF1 0005  [SCVEC -7,3,0]
V3AEA:  CSTAT  3                      ;68C3  RED
V3AEC:  VCTR   0, 0, 6                ;0000 C000
V3AF0:  SVEC   4, -10, 0              ;5B02  [SCVEC -3,-7,0]
V3AF2:  VCTR   0, 0, 6                ;0000 C000
V3AF6:  SVEC   10, 10, 0              ;4505  [SCVEC 7,3,0]
V3AF8:  VCTR   0, 0, 6                ;0000 C000
V3AFC:  SVEC   -4, 4, 0               ;421E  [SCVEC 3,7,0]

JADOT:           ;JSRL operand $D7F  (source name JADOT, ALVROM.MAC:787)
;  Single dot (VCTR 0,0,CB)
;  refs: vector ROM JSRL at $3BD0
;  refs: vector ROM JSRL at $3BEE
;  refs: vector ROM JSRL at $3C14
;  refs: vector ROM JSRL at $3C1C
;  refs: ... and 3 more (see cross reference)
V3AFE:  VCTR   0, 0, 6                ;0000 C000
V3B02:  RTSL                          ;C000

SPLAT1:          ;JSRL operand $D82  (source name SPLAT1, ALVROM.MAC:792)
;  Player-death splat entry 1: sets scale then JSRL SPLAT and falls through the rest (PTSPLA)
;  colour: 9 PDIWHI, 11 PDIRED, 10 PDIYEL
;  refs: program ROM table JSRL at $CEFA
V3B04:  SCAL   0, $00                 ;7000
V3B06:  JSRL   SPLAT                  ;AD8D

SPLAT2:          ;JSRL operand $D84  (source name SPLAT2, ALVROM.MAC:794)
;  Player-death splat entry 2: sets scale then JSRL SPLAT and falls through the rest (PTSPLA)
;  colour: 9 PDIWHI, 11 PDIRED, 10 PDIYEL
;  refs: program ROM table JSRL at $CEF8
V3B08:  SCAL   0, $40                 ;7040
V3B0A:  JSRL   SPLAT                  ;AD8D

SPLAT3:          ;JSRL operand $D86  (source name SPLAT3, ALVROM.MAC:796)
;  Player-death splat entry 3: sets scale then JSRL SPLAT and falls through the rest (PTSPLA)
;  colour: 9 PDIWHI, 11 PDIRED, 10 PDIYEL
;  refs: program ROM table JSRL at $CEF6
;  refs: program ROM table JSRL at $CEFC
V3B0C:  SCAL   1, $00                 ;7100
V3B0E:  JSRL   SPLAT                  ;AD8D

SPLAT4:          ;JSRL operand $D88  (source name SPLAT4, ALVROM.MAC:798)
;  Player-death splat entry 4: sets scale then JSRL SPLAT and falls through the rest (PTSPLA)
;  colour: 9 PDIWHI, 11 PDIRED, 10 PDIYEL
;  refs: program ROM table JSRL at $CEF4
V3B10:  SCAL   1, $40                 ;7140
V3B12:  JSRL   SPLAT                  ;AD8D

SPLAT5:          ;JSRL operand $D8A  (source name SPLAT5, ALVROM.MAC:800)
;  Player-death splat entry 5: sets scale then JSRL SPLAT and falls through the rest (PTSPLA)
;  colour: 9 PDIWHI, 11 PDIRED, 10 PDIYEL
;  refs: program ROM table JSRL at $CEF2
;  refs: program ROM table JSRL at $CEFE
V3B14:  SCAL   2, $00                 ;7200
V3B16:  JSRL   SPLAT                  ;AD8D

SPLAT6:          ;JSRL operand $D8C  (source name SPLAT6, ALVROM.MAC:802)
;  Player-death splat entry 6: sets scale then JSRL SPLAT and falls through the rest (PTSPLA)
;  colour: 9 PDIWHI, 11 PDIRED, 10 PDIYEL
;  refs: program ROM table JSRL at $CEF0
;  refs: program ROM table JSRL at $CF00
;  refs: program ROM CHPLKI+$33 (ALDIS2): ADC I,PTSPLA at $B81E
V3B18:  SCAL   2, $40                 ;7240

SPLAT:           ;JSRL operand $D8D  (source name SPLAT, ALVROM.MAC:811)
;  Player-death splat (multi-coloured, PDIWHI/PDIYEL/PDIRED)
;  colour: 9 PDIWHI, 11 PDIRED, 10 PDIYEL
;  refs: vector ROM JSRL at $3B06
;  refs: vector ROM JSRL at $3B0A
;  refs: vector ROM JSRL at $3B0E
;  refs: vector ROM JSRL at $3B12
;  refs: ... and 1 more (see cross reference)
V3B1A:  CSTAT  9                      ;68C9  PDIWHI
V3B1C:  VCTR   48, -16, 0             ;1FF0 0030  [SCVEC 18,-8,0]
V3B20:  VCTR   64, 32, 7              ;0020 E040  [SCVEC 38,8,CB]
V3B24:  VCTR   -48, 8, 7              ;0008 FFD0  [SCVEC 20,0C,CB]
V3B28:  CSTAT  11                     ;68CB  PDIRED
V3B2A:  SVEC   8, 16, 7               ;48E4  [SCVEC 24,14,CB]
V3B2C:  SVEC   -16, 2, 7              ;41F8  [SCVEC 1C,15,CB]
V3B2E:  CSTAT  10                     ;68CA  PDIYEL
V3B30:  SVEC   12, 22, 7              ;4BE6  [SCVEC 22,20,CB]
V3B32:  VCTR   -36, -20, 7            ;1FEC FFDC  [SCVEC 10,16,CB]
V3B36:  CSTAT  9                      ;68C9  PDIWHI
V3B38:  VCTR   4, 52, 7               ;0034 E004  [SCVEC 12,30,CB]
V3B3C:  SVEC   -28, -16, 7            ;58F2  [SCVEC 4,28,CB]
V3B3E:  CSTAT  11                     ;68CB  PDIRED
V3B40:  SVEC   -28, 10, 7             ;45F2  [SCVEC -0A,2D,CB]
V3B42:  VCTR   -4, -54, 7             ;1FCA FFFC  [SCVEC -0C,12,CB]
V3B46:  CSTAT  10                     ;68CA  PDIYEL
V3B48:  VCTR   -68, 12, 7             ;000C FFBC  [SCVEC -2E,18,CB]
V3B4C:  VCTR   36, -48, 7             ;1FD0 E024  [SCVEC -1C,0,CB]
V3B50:  CSTAT  9                      ;68C9  PDIWHI
V3B52:  SVEC   -20, -16, 7            ;58F6  [SCVEC -26,-8,CB]
V3B54:  SVEC   12, -4, 7              ;5EE6  [SCVEC -20,-0A,CB]
V3B56:  CSTAT  11                     ;68CB  PDIRED
V3B58:  SVEC   -12, -26, 7            ;53FA  [SCVEC -26,-17,CB]
V3B5A:  VCTR   50, 18, 7              ;0012 E032  [SCVEC -0D,-0E,CB]
V3B5E:  CSTAT  9                      ;68C9  PDIWHI
V3B60:  VCTR   -6, -40, 7             ;1FD8 FFFA  [SCVEC -10,-22,CB]
V3B64:  SVEC   8, 4, 7                ;42E4  [SCVEC -0C,-20,CB]
V3B66:  CSTAT  10                     ;68CA  PDIYEL
V3B68:  SVEC   8, -24, 7              ;54E4  [SCVEC -8,-2C,CB]
V3B6A:  SVEC   24, 24, 7              ;4CEC  [SCVEC 4,-20,CB]
V3B6C:  CSTAT  11                     ;68CB  PDIRED
V3B6E:  SVEC   24, -24, 7             ;54EC  [SCVEC 10,-2C,CB]
V3B70:  VCTR   4, 40, 7               ;0028 E004  [SCVEC 12,-18,CB]
V3B74:  CSTAT  10                     ;68CA  PDIYEL
V3B76:  VCTR   32, -12, 7             ;1FF4 E020  [SCVEC 22,-1E,CB]
V3B7A:  VCTR   -20, 44, 7             ;002C FFEC  [SCVEC 18,-8,CB]
V3B7E:  VCTR   -48, 16, 0             ;0010 1FD0  [SCVEC 0,0,0]
V3B82:  RTSL                          ;C000

SPLFU1:          ;JSRL operand $DC2  (source name SPLFU1, ALVROM.MAC:853)
;  Fuseball-kills-player explosion, frame 1 of 7 (star field STAR1B at shrinking scale) - PTSPLF+
;  colour: 0 WHITE
;  refs: program ROM table JSRL at $CF04
V3B84:  CSTAT  0                      ;68C0  WHITE
V3B86:  SCAL   5, $00                 ;7500
V3B88:  JMPL   STAR1B                 ;EB11

SPLFU2:          ;JSRL operand $DC5  (source name SPLFU2, ALVROM.MAC:856)
;  Fuseball-kills-player explosion, frame 2 of 7 (star field STAR1B at shrinking scale) - PTSPLF+
;  colour: 0 WHITE
;  refs: program ROM table JSRL at $CF06
V3B8A:  CSTAT  0                      ;68C0  WHITE
V3B8C:  SCAL   4, $60                 ;7460
V3B8E:  JMPL   STAR1B                 ;EB11

SPLFU3:          ;JSRL operand $DC8  (source name SPLFU3, ALVROM.MAC:859)
;  Fuseball-kills-player explosion, frame 3 of 7 (star field STAR1B at shrinking scale) - PTSPLF+
;  colour: 1 YELLOW
;  refs: program ROM table JSRL at $CF08
V3B90:  CSTAT  1                      ;68C1  YELLOW
V3B92:  SCAL   4, $40                 ;7440
V3B94:  JMPL   STAR1B                 ;EB11

SPLFU4:          ;JSRL operand $DCB  (source name SPLFU4, ALVROM.MAC:862)
;  Fuseball-kills-player explosion, frame 4 of 7 (star field STAR1B at shrinking scale) - PTSPLF+
;  colour: 1 YELLOW
;  refs: program ROM table JSRL at $CF0A
V3B96:  CSTAT  1                      ;68C1  YELLOW
V3B98:  SCAL   4, $20                 ;7420
V3B9A:  JMPL   STAR1B                 ;EB11

SPLFU5:          ;JSRL operand $DCE  (source name SPLFU5, ALVROM.MAC:865)
;  Fuseball-kills-player explosion, frame 5 of 7 (star field STAR1B at shrinking scale) - PTSPLF+
;  colour: 3 RED
;  refs: program ROM table JSRL at $CF0C
V3B9C:  CSTAT  3                      ;68C3  RED
V3B9E:  SCAL   4, $00                 ;7400
V3BA0:  JMPL   STAR1B                 ;EB11

SPLFU6:          ;JSRL operand $DD1  (source name SPLFU6, ALVROM.MAC:868)
;  Fuseball-kills-player explosion, frame 6 of 7 (star field STAR1B at shrinking scale) - PTSPLF+
;  colour: 3 RED
;  refs: program ROM table JSRL at $CF0E
V3BA2:  CSTAT  3                      ;68C3  RED
V3BA4:  SCAL   3, $60                 ;7360
V3BA6:  JMPL   STAR1B                 ;EB11

SPLFU7:          ;JSRL operand $DD4  (source name SPLFU7, ALVROM.MAC:871)
;  Fuseball-kills-player explosion, frame 7 of 7 (star field STAR1B at shrinking scale) - PTSPLF+
;  colour: 7 BLUE (BLULET)
;  refs: program ROM table JSRL at $CF10
V3BA8:  CSTAT  7                      ;68C7  BLUE (BLULET)
V3BAA:  SCAL   3, $40                 ;7340
V3BAC:  JMPL   STAR1B                 ;EB11

SHRAP:           ;JSRL operand $DD7  (source name SHRAP, ALVROM.MAC:880)
;  Player-death shrapnel pieces (uses vector-RAM SCALE sub-list)
;  colour: 1 YELLOW
;  calls vector RAM (built at run time by ALDIS2)
;  refs: program ROM table JSRL at $CF02
V3BAE:  JSRL   SCALE                  ;A7FE  SCALE FOR POSITION OF PIECE 1
V3BB0:  VCTR   128, -56, 0            ;1FC8 0080  POSITION FOR PIECE 1  [SCVEC 20,-0E,0]
V3BB4:  SCAL   1, $00                 ;7100  SCALE FOR PIECE 1
V3BB6:  CSTAT  1                      ;68C1  YELLOW
V3BB8:  SVEC   20, 0, 7               ;40EA  PIECE 1  [SCVEC 25,-0E,CB]
V3BBA:  SVEC   0, -20, 7              ;56E0  [SCVEC 25,-13,CB]
V3BBC:  VCTR   84, 48, 7              ;0030 E054  [SCVEC 3A,-7,CB]
V3BC0:  VCTR   -48, 4, 7              ;0004 FFD0  [SCVEC 2E,-6,CB]
V3BC4:  SVEC   8, -8, 7               ;5CE4  [SCVEC 30,-8,CB]
V3BC6:  VCTR   -64, -24, 7            ;1FE8 FFC0  [SCVEC 20,-0E,CB]
V3BCA:  JSRL   SCALE                  ;A7FE  SHOT 1
V3BCC:  VCTR   -64, 80, 0             ;0050 1FC0  [SCVEC 10,6,0]
V3BD0:  JSRL   JADOT                  ;AD7F
V3BD2:  JSRL   SCALE                  ;A7FE  PIECE 2
V3BD4:  VCTR   8, 96, 0               ;0060 0008  [SCVEC 12,1E,0]
V3BD8:  SCAL   1, $00                 ;7100
V3BDA:  CSTAT  1                      ;68C1  YELLOW
V3BDC:  VCTR   64, -32, 7             ;1FE0 E040  [SCVEC 22,16,CB]
V3BE0:  VCTR   40, 0, 7               ;0000 E028  [SCVEC 2C,16,CB]
V3BE4:  VCTR   -104, 32, 7            ;0020 FF98  [SCVEC 12,1E,CB]
V3BE8:  JSRL   SCALE                  ;A7FE  SHOT 2
V3BEA:  VCTR   -8, 72, 0              ;0048 1FF8  [SCVEC 10,30,0]
V3BEE:  JSRL   JADOT                  ;AD7F
V3BF0:  JSRL   SCALE                  ;A7FE  PIECE 3
V3BF2:  VCTR   -80, 0, 0              ;0000 1FB0  [SCVEC -4,30,0]
V3BF6:  SCAL   1, $00                 ;7100
V3BF8:  CSTAT  1                      ;68C1  YELLOW
V3BFA:  VCTR   -112, -32, 7           ;1FE0 FF90  [SCVEC -20,28,CB]
V3BFE:  VCTR   52, -32, 7             ;1FE0 E034  [SCVEC -13,20,CB]
V3C02:  SVEC   -8, 16, 7              ;48FC  [SCVEC -15,24,CB]
V3C04:  SVEC   28, 0, 7               ;40EE  [SCVEC -0E,24,CB]
V3C06:  VCTR   -32, 8, 7              ;0008 FFE0  [SCVEC -16,26,CB]
V3C0A:  VCTR   72, 40, 7              ;0028 E048  [SCVEC -4,30,CB]
V3C0E:  JSRL   SCALE                  ;A7FE  SHOT 3
V3C10:  VCTR   -32, -128, 0           ;1F80 1FE0  [SCVEC -0C,10,0]
V3C14:  JSRL   JADOT                  ;AD7F
V3C16:  JSRL   SCALE                  ;A7FE  SHOT 4
V3C18:  VCTR   -128, 32, 0            ;0020 1F80  [SCVEC -2C,18,0]
V3C1C:  JSRL   JADOT                  ;AD7F
V3C1E:  JSRL   SCALE                  ;A7FE  PIECE 4
V3C20:  VCTR   16, -80, 0             ;1FB0 0010  [SCVEC -28,4,0]
V3C24:  SCAL   1, $00                 ;7100
V3C26:  CSTAT  1                      ;68C1  YELLOW
V3C28:  SVEC   8, -16, 7              ;58E4  [SCVEC -26,0,CB]
V3C2A:  SVEC   24, -20, 7             ;56EC  [SCVEC -20,-5,CB]
V3C2C:  VCTR   32, 4, 7               ;0004 E020  [SCVEC -18,-4,CB]
V3C30:  SVEC   -8, 28, 7              ;4EFC  [SCVEC -1A,3,CB]
V3C32:  VCTR   -32, 4, 7              ;0004 FFE0  [SCVEC -22,4,CB]
V3C36:  SVEC   -24, 0, 7              ;40F4  [SCVEC -28,4,CB]
V3C38:  JSRL   SCALE                  ;A7FE  SHOT 1
V3C3A:  VCTR   112, -64, 0            ;1FC0 0070  [SCVEC -0C,-0C,0]
V3C3E:  JSRL   JADOT                  ;AD7F
V3C40:  JSRL   SCALE                  ;A7FE  PIECE 5
V3C42:  VCTR   -8, -68, 0             ;1FBC 1FF8  [SCVEC -0E,-1D,0]
V3C46:  SCAL   1, $00                 ;7100
V3C48:  CSTAT  1                      ;68C1  YELLOW
V3C4A:  SVEC   4, -12, 7              ;5AE2  [SCVEC -0D,-20,CB]
V3C4C:  VCTR   -36, -4, 7             ;1FFC FFDC  [SCVEC -16,-21,CB]
V3C50:  VCTR   56, -28, 7             ;1FE4 E038  [SCVEC -8,-28,CB]
V3C54:  VCTR   48, 24, 7              ;0018 E030  [SCVEC 4,-22,CB]
V3C58:  SVEC   4, 16, 7               ;48E2  [SCVEC 5,-1E,CB]
V3C5A:  SVEC   -20, 4, 7              ;42F6  [SCVEC 0,-1D,CB]
V3C5C:  VCTR   -32, -12, 7            ;1FF4 FFE0  [SCVEC -8,-20,CB]
V3C60:  SVEC   -24, 12, 7             ;46F4  [SCVEC -0E,-1D,CB]
V3C62:  JSRL   SCALE                  ;A7FE  SHOT 2
V3C64:  VCTR   152, -28, 0            ;1FE4 0098  [SCVEC 18,-24,0]
V3C68:  JMPL   JADOT                  ;ED7F

FUSE0:           ;JSRL operand $E35  (source name FUSE0, ALVROM.MAC:953)
;  Fuseball, frame 0 (0-3) - PTFUSE+
;  colour: 3 RED, 1 YELLOW, 5 GREEN, 2 PURPLE, 4 TURQOI
;  refs: program ROM table JSRL at $CF16
;  refs: program ROM M10+$3C (ALDIS2): ADC I,PTFUSE at $B6EC
V3C6A:  CSTAT  3                      ;68C3  RED
V3C6C:  SVEC   -8, 12, 7              ;46FC  [SCVEC -4,6,CB]
V3C6E:  SVEC   10, 12, 7              ;46E5  [SCVEC 1,0C,CB]
V3C70:  SVEC   -12, 4, 7              ;42FA  [SCVEC -5,0E,CB]
V3C72:  SVEC   12, 8, 7               ;44E6  [SCVEC 1,12,CB]
V3C74:  SVEC   -4, 12, 7              ;46FE  [SCVEC -1,18,CB]
V3C76:  CSTAT  1                      ;68C1  YELLOW
V3C78:  SVEC   18, -2, 0              ;5F09  [SCVEC 8,17,0]
V3C7A:  SVEC   4, -6, 7               ;5DE2  [SCVEC 0A,14,CB]
V3C7C:  SVEC   4, -8, 7               ;5CE2  [SCVEC 0C,10,CB]
V3C7E:  SVEC   -12, -8, 7             ;5CFA  [SCVEC 6,0C,CB]
V3C80:  SVEC   4, -8, 7               ;5CE2  [SCVEC 8,8,CB]
V3C82:  SVEC   -16, -16, 7            ;58F8  [SCVEC 0,0,CB]
V3C84:  CSTAT  5                      ;68C5  GREEN
V3C86:  SVEC   20, 4, 7               ;42EA  [SCVEC 0A,2,CB]
V3C88:  SVEC   -4, -16, 7             ;58FE  [SCVEC 8,-6,CB]
V3C8A:  SVEC   12, 0, 7               ;40E6  [SCVEC 0E,-6,CB]
V3C8C:  SVEC   -12, -12, 7            ;5AFA  [SCVEC 8,-0C,CB]
V3C8E:  SVEC   8, -14, 7              ;59E4  [SCVEC 0C,-13,CB]
V3C90:  SVEC   8, 0, 7                ;40E4  [SCVEC 10,-13,CB]
V3C92:  CSTAT  2                      ;68C2  PURPLE
V3C94:  VCTR   -40, -14, 0            ;1FF2 1FD8  [SCVEC -4,-1A,0]
V3C98:  SVEC   0, 12, 7               ;46E0  [SCVEC -4,-14,CB]
V3C9A:  SVEC   -12, 0, 7              ;40FA  [SCVEC -0A,-14,CB]
V3C9C:  SVEC   6, 14, 7               ;47E3  [SCVEC -7,-0D,CB]
V3C9E:  SVEC   -4, 14, 7              ;47FE  [SCVEC -9,-6,CB]
V3CA0:  SVEC   12, -4, 7              ;5EE6  [SCVEC -3,-8,CB]
V3CA2:  SVEC   6, 16, 7               ;48E3  [SCVEC 0,0,CB]
V3CA4:  CSTAT  4                      ;68C4  TURQOI
V3CA6:  SVEC   -16, -4, 7             ;5EF8  [SCVEC -8,-2,CB]
V3CA8:  SVEC   -4, 10, 7              ;45FE  [SCVEC -0A,3,CB]
V3CAA:  SVEC   -8, -8, 7              ;5CFC  [SCVEC -0E,-1,CB]
V3CAC:  SVEC   -4, 10, 7              ;45FE  [SCVEC -10,4,CB]
V3CAE:  SVEC   -24, -16, 7            ;58F4  [SCVEC -1C,-4,CB]
V3CB0:  RTSL                          ;C000

FUSE1:           ;JSRL operand $E59  (source name FUSE1, ALVROM.MAC:989)
;  Fuseball, frame 1 (0-3) - PTFUSE+
;  colour: 3 RED, 1 YELLOW, 5 GREEN, 2 PURPLE, 4 TURQOI
;  refs: program ROM table JSRL at $CF18
V3CB2:  CSTAT  3                      ;68C3  RED
V3CB4:  SVEC   -2, 16, 7              ;48FF  [SCVEC -1,8,CB]
V3CB6:  SVEC   -8, 0, 7               ;40FC  [SCVEC -5,8,CB]
V3CB8:  SVEC   0, 4, 7                ;42E0  [SCVEC -5,0A,CB]
V3CBA:  SVEC   -10, -2, 7             ;5FFB  [SCVEC -0A,9,CB]
V3CBC:  SVEC   6, 14, 7               ;47E3  [SCVEC -7,10,CB]
V3CBE:  SVEC   -10, 0, 7              ;40FB  [SCVEC -0C,10,CB]
V3CC0:  SVEC   -4, -8, 7              ;5CFE  [SCVEC -0E,0C,CB]
V3CC2:  CSTAT  1                      ;68C1  YELLOW
V3CC4:  VCTR   68, 8, 0               ;0008 0044  [SCVEC 14,10,0]
V3CC8:  SVEC   -12, 4, 7              ;42FA  [SCVEC 0E,12,CB]
V3CCA:  SVEC   -10, -10, 7            ;5BFB  [SCVEC 9,0D,CB]
V3CCC:  SVEC   2, -12, 7              ;5AE1  [SCVEC 0A,7,CB]
V3CCE:  SVEC   -8, 2, 7               ;41FC  [SCVEC 6,8,CB]
V3CD0:  SVEC   -12, -16, 7            ;58FA  [SCVEC 0,0,CB]
V3CD2:  CSTAT  5                      ;68C5  GREEN
V3CD4:  SVEC   2, -2, 7               ;5FE1  [SCVEC 1,-1,CB]
V3CD6:  SVEC   16, 2, 7               ;41E8  [SCVEC 9,0,CB]
V3CD8:  SVEC   4, -10, 7              ;5BE2  [SCVEC 0B,-5,CB]
V3CDA:  SVEC   10, -2, 7              ;5FE5  [SCVEC 10,-6,CB]
V3CDC:  SVEC   -4, -8, 7              ;5CFE  [SCVEC 0E,-0A,CB]
V3CDE:  SVEC   12, -2, 7              ;5FE6  [SCVEC 14,-0B,CB]
V3CE0:  CSTAT  2                      ;68C2  PURPLE
V3CE2:  VCTR   -56, -22, 0            ;1FEA 1FC8  [SCVEC -8,-16,0]
V3CE6:  SVEC   0, 8, 7                ;44E0  [SCVEC -8,-12,CB]
V3CE8:  SVEC   8, 12, 7               ;46E4  [SCVEC -4,-0C,CB]
V3CEA:  SVEC   -8, 0, 7               ;40FC  [SCVEC -8,-0C,CB]
V3CEC:  SVEC   4, 12, 7               ;46E2  [SCVEC -6,-6,CB]
V3CEE:  SVEC   12, 12, 7              ;46E6  [SCVEC 0,0,CB]
V3CF0:  CSTAT  4                      ;68C4  TURQOI
V3CF2:  SVEC   -16, 0, 7              ;40F8  [SCVEC -8,0,CB]
V3CF4:  SVEC   -8, -8, 7              ;5CFC  [SCVEC -0C,-4,CB]
V3CF6:  SVEC   -8, 4, 7               ;42FC  [SCVEC -10,-2,CB]
V3CF8:  SVEC   -16, -8, 7             ;5CF8  [SCVEC -18,-6,CB]
V3CFA:  RTSL                          ;C000

FUSE2:           ;JSRL operand $E7E  (source name FUSE2, ALVROM.MAC:1026)
;  Fuseball, frame 2 (0-3) - PTFUSE+
;  colour: 3 RED, 1 YELLOW, 5 GREEN, 2 PURPLE, 4 TURQOI
;  refs: program ROM table JSRL at $CF1A
V3CFC:  CSTAT  3                      ;68C3  RED
V3CFE:  SVEC   0, 14, 7               ;47E0  [SCVEC 0,7,CB]
V3D00:  SVEC   6, 4, 7                ;42E3  [SCVEC 3,9,CB]
V3D02:  SVEC   -4, 8, 7               ;44FE  [SCVEC 1,0D,CB]
V3D04:  SVEC   10, 6, 7               ;43E5  [SCVEC 6,10,CB]
V3D06:  SVEC   -4, 8, 7               ;44FE  [SCVEC 4,14,CB]
V3D08:  SVEC   8, 16, 7               ;48E4  [SCVEC 8,1C,CB]
V3D0A:  CSTAT  1                      ;68C1  YELLOW
V3D0C:  VCTR   32, -28, 0             ;1FE4 0020  [SCVEC 18,0E,0]
V3D10:  SVEC   -12, 0, 7              ;40FA  [SCVEC 12,0E,CB]
V3D12:  SVEC   -4, -16, 7             ;58FE  [SCVEC 10,6,CB]
V3D14:  SVEC   -12, -8, 7             ;5CFA  [SCVEC 0A,2,CB]
V3D16:  SVEC   -4, 8, 7               ;44FE  [SCVEC 8,6,CB]
V3D18:  SVEC   -16, -12, 7            ;5AF8  [SCVEC 0,0,CB]
V3D1A:  CSTAT  5                      ;68C5  GREEN
V3D1C:  SVEC   8, -8, 7               ;5CE4  [SCVEC 4,-4,CB]
V3D1E:  SVEC   8, 0, 7                ;40E4  [SCVEC 8,-4,CB]
V3D20:  SVEC   2, -8, 7               ;5CE1  [SCVEC 9,-8,CB]
V3D22:  SVEC   14, -2, 7              ;5FE7  [SCVEC 10,-9,CB]
V3D24:  SVEC   2, -14, 7              ;59E1  [SCVEC 11,-10,CB]
V3D26:  SVEC   14, 0, 7               ;40E7  [SCVEC 18,-10,CB]
V3D28:  CSTAT  2                      ;68C2  PURPLE
V3D2A:  VCTR   -72, -16, 0            ;1FF0 1FB8  [SCVEC -0C,-18,0]
V3D2E:  SVEC   8, 8, 7                ;44E4  [SCVEC -8,-14,CB]
V3D30:  SVEC   -8, 16, 7              ;48FC  [SCVEC -0C,-0C,CB]
V3D32:  SVEC   14, 4, 7               ;42E7  [SCVEC -5,-0A,CB]
V3D34:  SVEC   10, 20, 7              ;4AE5  [SCVEC 0,0,CB]
V3D36:  CSTAT  4                      ;68C4  TURQOI
V3D38:  SVEC   -8, 4, 7               ;42FC  [SCVEC -4,2,CB]
V3D3A:  SVEC   -8, -4, 7              ;5EFC  [SCVEC -8,0,CB]
V3D3C:  SVEC   -4, 4, 7               ;42FE  [SCVEC -0A,2,CB]
V3D3E:  SVEC   -16, -4, 7             ;5EF8  [SCVEC -12,0,CB]
V3D40:  SVEC   -8, -12, 7             ;5AFC  [SCVEC -16,-6,CB]
V3D42:  RTSL                          ;C000

FUSE3:           ;JSRL operand $EA2  (source name FUSE3, ALVROM.MAC:1061)
;  Fuseball, frame 3 (0-3) - PTFUSE+
;  colour: 3 RED, 1 YELLOW, 5 GREEN, 2 PURPLE, 4 TURQOI
;  refs: program ROM table JSRL at $CF1C
V3D44:  CSTAT  3                      ;68C3  RED
V3D46:  SVEC   -8, 8, 7               ;44FC  [SCVEC -4,4,CB]
V3D48:  SVEC   2, 12, 7               ;46E1  [SCVEC -3,0A,CB]
V3D4A:  SVEC   -6, 8, 7               ;44FD  [SCVEC -6,0E,CB]
V3D4C:  SVEC   -12, 0, 7              ;40FA  [SCVEC -0C,0E,CB]
V3D4E:  SVEC   0, 8, 7                ;44E0  [SCVEC -0C,12,CB]
V3D50:  CSTAT  1                      ;68C1  YELLOW
V3D52:  VCTR   56, -4, 0              ;1FFC 0038  [SCVEC 10,10,0]
V3D56:  SVEC   -12, -4, 7             ;5EFA  [SCVEC 0A,0E,CB]
V3D58:  SVEC   6, -6, 7               ;5DE3  [SCVEC 0D,0B,CB]
V3D5A:  SVEC   -10, -6, 7             ;5DFB  [SCVEC 8,8,CB]
V3D5C:  SVEC   4, -8, 7               ;5CE2  [SCVEC 0A,4,CB]
V3D5E:  SVEC   -20, -8, 7             ;5CF6  [SCVEC 0,0,CB]
V3D60:  CSTAT  5                      ;68C5  GREEN
V3D62:  SVEC   16, -6, 7              ;5DE8  [SCVEC 8,-3,CB]
V3D64:  SVEC   2, -8, 7               ;5CE1  [SCVEC 9,-7,CB]
V3D66:  SVEC   10, 6, 7               ;43E5  [SCVEC 0E,-4,CB]
V3D68:  SVEC   8, 0, 7                ;40E4  [SCVEC 12,-4,CB]
V3D6A:  SVEC   4, -20, 7              ;56E2  [SCVEC 14,-0E,CB]
V3D6C:  CSTAT  2                      ;68C2  PURPLE
V3D6E:  VCTR   -40, -20, 0            ;1FEC 1FD8  [SCVEC 0,-18,0]
V3D72:  SVEC   -8, 8, 7               ;44FC  [SCVEC -4,-14,CB]
V3D74:  SVEC   8, 8, 7                ;44E4  [SCVEC 0,-10,CB]
V3D76:  SVEC   -8, 8, 7               ;44FC  [SCVEC -4,-0C,CB]
V3D78:  SVEC   12, 8, 7               ;44E6  [SCVEC 2,-8,CB]
V3D7A:  SVEC   -4, 16, 7              ;48FE  [SCVEC 0,0,CB]
V3D7C:  CSTAT  4                      ;68C4  TURQOI
V3D7E:  SVEC   -18, -8, 7             ;5CF7  [SCVEC -9,-4,CB]
V3D80:  SVEC   -2, 6, 7               ;43FF  [SCVEC -0A,-1,CB]
V3D82:  SVEC   -8, 0, 7               ;40FC  [SCVEC -0E,-1,CB]
V3D84:  SVEC   -2, -12, 7             ;5AFF  [SCVEC -0F,-7,CB]
V3D86:  SVEC   -12, -4, 7             ;5EFA  [SCVEC -15,-9,CB]
V3D88:  RTSL                          ;C000

FUSEX1:          ;JSRL operand $EC5  (source name FUSEX1, ALVROM.MAC:1096)
;  Fuseball kill score '750'
;  colour: 0 WHITE
;  refs: program ROM table JSRL at $CF1E
;  refs: program ROM TEXTYP+$2 (ALDIS2): .BYTE PTFUSX+4 at $B7E7
;  refs: program ROM TEXTYP+$3 (ALDIS2): .BYTE PTFUSX+2 at $B7E8
;  refs: program ROM TEXTYP+$4 (ALDIS2): .BYTE PTFUSX+0 at $B7E9
V3D8A:  CSTAT  0                      ;68C0  WHITE
V3D8C:  SCAL   1, $20                 ;7120
V3D8E:  VCTR   -36, 0, 0              ;0000 1FDC
V3D92:  JSRL   CHAR_7                 ;A8DE
V3D94:  JMPL   FIFTY                  ;EED7

FUSEX2:          ;JSRL operand $ECB  (source name FUSEX2, ALVROM.MAC:1101)
;  Fuseball kill score '500'
;  colour: 0 WHITE
;  refs: program ROM table JSRL at $CF20
V3D96:  CSTAT  0                      ;68C0  WHITE
V3D98:  SCAL   1, $20                 ;7120
V3D9A:  VCTR   -36, 0, 0              ;0000 1FDC
V3D9E:  JSRL   CHAR_5                 ;A8D0
V3DA0:  JSRL   CHAR_O                 ;A865
V3DA2:  JMPL   ZERO                   ;EED8

FUSEX3:          ;JSRL operand $ED2  (source name FUSEX3, ALVROM.MAC:1107)
;  Fuseball kill score '250'
;  colour: 0 WHITE
;  refs: program ROM table JSRL at $CF22
V3DA4:  CSTAT  0                      ;68C0  WHITE
V3DA6:  SCAL   1, $20                 ;7120
V3DA8:  VCTR   -36, 0, 0              ;0000 1FDC
V3DAC:  JSRL   CHAR_2                 ;A8BA

FIFTY:           ;JSRL operand $ED7  (source name FIFTY, ALVROM.MAC:1111)
;  Digits '50' (tail of FUSEX1/FUSEX3)
;  refs: vector ROM JMPL at $3D94
V3DAE:  JSRL   CHAR_5                 ;A8D0

ZERO:            ;JSRL operand $ED8  (source name ZERO, ALVROM.MAC:1112)
;  Digit '0' (tail JMPL CHAR.0)
;  refs: vector ROM JMPL at $3DA2
V3DB0:  JMPL   CHAR_O                 ;E865

JSRDOT:          ;JSRL operand $ED9  (source name JSRDOT, ALVROM.MAC:1116)
;  JSRL word copied by the 6502 (ALDIS2 reads $3DB2/$3DB3) - a dot
;  refs: program ROM WHITIP+$C (ALDIS2): LDA JSRDOT at $C72D
;  refs: program ROM WHITIP+$12 (ALDIS2): LDA JSRDOT+1 at $C733
V3DB2:  JSRL   JADOT                  ;AD7F

SWNORM:          ;JSRL operand $EDA  (source name SWNORM, ALVROM.MAC:1169)
;  Master display list for play: JSRL every vector-RAM sub-buffer, JMPL VECRAM
;  no lit vectors - beam positioning / advance / vector-RAM list
;  calls vector RAM (built at run time by ALDIS2)
;  refs: program ROM table JMPL at $CEC2
V3DB4:  JSRL   SWINFO                 ;A002
V3DB6:  JSRL   SWWELL                 ;A205
V3DB8:  JSRL   SWNYMP                 ;A52A
V3DBA:  JSRL   SWCURS                 ;A6DE
V3DBC:  JSRL   SWINVA                 ;A348
V3DBE:  JSRL   SWSHOT                 ;A47F
V3DC0:  JSRL   SWEXPL                 ;A66B
V3DC2:  JSRL   SWENEL                 ;A100
V3DC4:  JSRL   SWSTAR                 ;A711
V3DC6:  JMPL   VECRAM                 ;E000

SWMSGS:          ;JSRL operand $EE4  (source name SWMSGS, ALVROM.MAC:1182)
;  Master display list for messages only (JSRL SWINFO, JMPL VECRAM)
;  no lit vectors - beam positioning / advance / vector-RAM list
;  calls vector RAM (built at run time by ALDIS2)
;  refs: program ROM table JMPL at $CEC4
V3DC8:  JSRL   SWINFO                 ;A002
V3DCA:  JMPL   VECRAM                 ;E000

SWHALT:          ;JSRL operand $EE6  (source name SWHALT, ALVROM.MAC:1184)
;  HALT the vector generator
;  no lit vectors - beam positioning / advance / vector-RAM list
;  refs: program ROM table JMPL at $CEC6
V3DCC:  HALT                          ;2000

BOKLIT:          ;JSRL operand $EE7  (source name BOKLIT, ALVROM.MAC:1188)
;  Bookkeeping screen literals (SECONDS ON / PLAYED / 1 & 2 PLAYER GAMES / AVERAGE / X1 BONUS ADDER)
;  colour: 0 WHITE, 4 TURQOI
;  refs: program ROM DBOOKE+$3D (ALTES2): LAH BOKLIT+1 at $DD7E
;  refs: program ROM DBOOKE+$40 (ALTES2): LXL BOKLIT at $DD81
V3DCE:  JSRL   BONDRY                 ;AA53  SCREEN BOUNDARY
V3DD0:  CNTR                          ;8040
V3DD2:  SCAL   1, $00                 ;7100
V3DD4:  CSTAT  0                      ;68C0  WHITE
V3DD6:  VCTR   -120, 300, 0           ;012C 1F88
V3DDA:  JSRL   SECONDS                ;AF21
V3DDC:  JSRL   CHAR_O                 ;A865
V3DDE:  JSRL   CHAR_N                 ;A860
V3DE0:  VCTR   -240, -30, 0           ;1FE2 1F10
V3DE4:  JSRL   SECONDS                ;AF21
V3DE6:  JSRL   CHAR_P                 ;A86B  [.IRPC X,PLAYED]
V3DE8:  JSRL   CHAR_L                 ;A855
V3DEA:  JSRL   CHAR_A                 ;A800
V3DEC:  JSRL   CHAR_Y                 ;A8A7
V3DEE:  JSRL   CHAR_E                 ;A823
V3DF0:  JSRL   CHAR_D                 ;A81B
V3DF2:  VCTR   -336, -30, 0           ;1FE2 1EB0
V3DF6:  JSRL   CHAR_1                 ;A8B6
V3DF8:  JSRL   PLYGAM                 ;AF2A
V3DFA:  VCTR   -336, -30, 0           ;1FE2 1EB0
V3DFE:  JSRL   CHAR_2                 ;A8BA
V3E00:  JSRL   PLYGAM                 ;AF2A
V3E02:  VCTR   -336, -30, 0           ;1FE2 1EB0
V3E06:  JSRL   SECONDS                ;AF21
V3E08:  JSRL   CHAR_A                 ;A800  [.IRPC X,AVERAGE]
V3E0A:  JSRL   CHAR_V                 ;A896
V3E0C:  JSRL   CHAR_E                 ;A823
V3E0E:  JSRL   CHAR_R                 ;A87B
V3E10:  JSRL   CHAR_A                 ;A800
V3E12:  JSRL   CHAR_G                 ;A832
V3E14:  JSRL   CHAR_E                 ;A823
V3E16:  VCTR   -360, -72, 0           ;1FB8 1E98
V3E1A:  CSTAT  4                      ;68C4  TURQOI
V3E1C:  JSRL   CHAR_X                 ;A8A2
V3E1E:  JSRL   CHAR_1                 ;A8B6
V3E20:  VCTR   -48, -32, 0            ;1FE0 1FD0
V3E24:  JSRL   CHAR_B                 ;A808  [.IRPC X,<BONUS ADDER>]
V3E26:  JSRL   CHAR_O                 ;A865
V3E28:  JSRL   CHAR_N                 ;A860
V3E2A:  JSRL   CHAR_U                 ;A890
V3E2C:  JSRL   CHAR_S                 ;A883
V3E2E:  JSRL   CHAR_                  ;A8B4
V3E30:  JSRL   CHAR_A                 ;A800
V3E32:  JSRL   CHAR_D                 ;A81B
V3E34:  JSRL   CHAR_D                 ;A81B
V3E36:  JSRL   CHAR_E                 ;A823
V3E38:  JSRL   CHAR_R                 ;A87B
V3E3A:  CSTAT  0                      ;68C0  WHITE
V3E3C:  VCTR   -496, 224, 0           ;00E0 1E10
V3E40:  RTSL                          ;C000

SECONDS:         ;JSRL operand $F21  (source name SECONDS, ALVROM.MAC:1224)
;  Literal 'SECONDS '
;  refs: vector ROM JSRL at $3DDA
;  refs: vector ROM JSRL at $3DE4
;  refs: vector ROM JSRL at $3E06
V3E42:  JSRL   CHAR_S                 ;A883  [.IRPC X,<SECONDS >]
V3E44:  JSRL   CHAR_E                 ;A823
V3E46:  JSRL   CHAR_C                 ;A815
V3E48:  JSRL   CHAR_O                 ;A865
V3E4A:  JSRL   CHAR_N                 ;A860
V3E4C:  JSRL   CHAR_D                 ;A81B
V3E4E:  JSRL   CHAR_S                 ;A883
V3E50:  JSRL   CHAR_                  ;A8B4
V3E52:  RTSL                          ;C000

PLYGAM:          ;JSRL operand $F2A  (source name PLYGAM, ALVROM.MAC:1229)
;  Literal ' PLAYER GAMES'
;  refs: vector ROM JSRL at $3DF8
;  refs: vector ROM JSRL at $3E00
V3E54:  JSRL   CHAR_                  ;A8B4  [.IRPC X,< PLAYER GAMES>]
V3E56:  JSRL   CHAR_P                 ;A86B
V3E58:  JSRL   CHAR_L                 ;A855
V3E5A:  JSRL   CHAR_A                 ;A800
V3E5C:  JSRL   CHAR_Y                 ;A8A7
V3E5E:  JSRL   CHAR_E                 ;A823
V3E60:  JSRL   CHAR_R                 ;A87B
V3E62:  JSRL   CHAR_                  ;A8B4
V3E64:  JSRL   CHAR_G                 ;A832
V3E66:  JSRL   CHAR_A                 ;A800
V3E68:  JSRL   CHAR_M                 ;A85A
V3E6A:  JSRL   CHAR_E                 ;A823
V3E6C:  JSRL   CHAR_S                 ;A883
V3E6E:  RTSL                          ;C000

FIRED:           ;JSRL operand $F38  (source name FIRED, ALVROM.MAC:1233)
;  Literal 'PRESS FIRE AND ' (red)
;  colour: 3 RED
;  refs: vector ROM JSRL at $3EAC
;  refs: vector ROM JSRL at $3ED4
;  refs: vector ROM JSRL at $3EF4
V3E70:  CNTR                          ;8040
V3E72:  CSTAT  3                      ;68C3  RED
V3E74:  VCTR   -412, 380, 0           ;017C 1E64
V3E78:  JSRL   CHAR_P                 ;A86B  [.IRPC X,<PRESS FIRE AND >]
V3E7A:  JSRL   CHAR_R                 ;A87B
V3E7C:  JSRL   CHAR_E                 ;A823
V3E7E:  JSRL   CHAR_S                 ;A883
V3E80:  JSRL   CHAR_S                 ;A883
V3E82:  JSRL   CHAR_                  ;A8B4
V3E84:  JSRL   CHAR_F                 ;A82B
V3E86:  JSRL   CHAR_I                 ;A842
V3E88:  JSRL   CHAR_R                 ;A87B
V3E8A:  JSRL   CHAR_E                 ;A823
V3E8C:  JSRL   CHAR_                  ;A8B4
V3E8E:  JSRL   CHAR_A                 ;A800
V3E90:  JSRL   CHAR_N                 ;A860
V3E92:  JSRL   CHAR_D                 ;A81B
V3E94:  JSRL   CHAR_                  ;A8B4
V3E96:  RTSL                          ;C000

TOZERO:          ;JSRL operand $F4C  (source name TOZERO, ALVROM.MAC:1240)
;  Literal ' TO ZERO '
;  refs: vector ROM JSRL at $3EE4
;  refs: vector ROM JSRL at $3F04
V3E98:  JSRL   CHAR_                  ;A8B4  [.IRPC X,< TO ZERO >]
V3E9A:  JSRL   CHAR_T                 ;A88A
V3E9C:  JSRL   CHAR_O                 ;A865
V3E9E:  JSRL   CHAR_                  ;A8B4
V3EA0:  JSRL   CHAR_Z                 ;A8AE
V3EA2:  JSRL   CHAR_E                 ;A823
V3EA4:  JSRL   CHAR_R                 ;A87B
V3EA6:  JSRL   CHAR_O                 ;A865
V3EA8:  JSRL   CHAR_                  ;A8B4
V3EAA:  RTSL                          ;C000

ENTEST:          ;JSRL operand $F56  (source name ENTEST, ALVROM.MAC:1245)
;  Literal 'PRESS FIRE AND ZAP FOR SELF TEST'
;  colour: 3 RED, 4 TURQOI
;  refs: vector ROM .WORD at $3F16
;  refs: vector ROM .WORD at $3F18
V3EAC:  JSRL   FIRED                  ;AF38
V3EAE:  JSRL   CHAR_Z                 ;A8AE  [.IRPC X,<ZAP FOR SELF TEST>]
V3EB0:  JSRL   CHAR_A                 ;A800
V3EB2:  JSRL   CHAR_P                 ;A86B
V3EB4:  JSRL   CHAR_                  ;A8B4
V3EB6:  JSRL   CHAR_F                 ;A82B
V3EB8:  JSRL   CHAR_O                 ;A865
V3EBA:  JSRL   CHAR_R                 ;A87B
V3EBC:  JSRL   CHAR_                  ;A8B4
V3EBE:  JSRL   CHAR_S                 ;A883
V3EC0:  JSRL   CHAR_E                 ;A823
V3EC2:  JSRL   CHAR_L                 ;A855
V3EC4:  JSRL   CHAR_F                 ;A82B
V3EC6:  JSRL   CHAR_                  ;A8B4
V3EC8:  JSRL   CHAR_T                 ;A88A
V3ECA:  JSRL   CHAR_E                 ;A823
V3ECC:  JSRL   CHAR_S                 ;A883
V3ECE:  JSRL   CHAR_T                 ;A88A
V3ED0:  CSTAT  4                      ;68C4  TURQOI
V3ED2:  RTSL                          ;C000

ZTIMES:          ;JSRL operand $F6A  (source name ZTIMES, ALVROM.MAC:1252)
;  Literal 'PRESS FIRE AND START 1 TO ZERO TIMES'
;  colour: 3 RED, 4 TURQOI
;  refs: vector ROM .WORD at $3F1A
V3ED4:  JSRL   FIRED                  ;AF38
V3ED6:  JSRL   CHAR_S                 ;A883  [.IRPC X,<START 1>]
V3ED8:  JSRL   CHAR_T                 ;A88A
V3EDA:  JSRL   CHAR_A                 ;A800
V3EDC:  JSRL   CHAR_R                 ;A87B
V3EDE:  JSRL   CHAR_T                 ;A88A
V3EE0:  JSRL   CHAR_                  ;A8B4
V3EE2:  JSRL   CHAR_1                 ;A8B6
V3EE4:  JSRL   TOZERO                 ;AF4C
V3EE6:  JSRL   CHAR_T                 ;A88A  [.IRPC X,TIMES]
V3EE8:  JSRL   CHAR_I                 ;A842
V3EEA:  JSRL   CHAR_M                 ;A85A
V3EEC:  JSRL   CHAR_E                 ;A823
V3EEE:  JSRL   CHAR_S                 ;A883
V3EF0:  CSTAT  4                      ;68C4  TURQOI
V3EF2:  RTSL                          ;C000

ZHISCO:          ;JSRL operand $F7A  (source name ZHISCO, ALVROM.MAC:1263)
;  Literal 'PRESS FIRE AND START 2 TO ZERO SCORES'
;  colour: 3 RED, 4 TURQOI
;  refs: vector ROM .WORD at $3F1C
V3EF4:  JSRL   FIRED                  ;AF38
V3EF6:  JSRL   CHAR_S                 ;A883  [.IRPC X,<START 2>]
V3EF8:  JSRL   CHAR_T                 ;A88A
V3EFA:  JSRL   CHAR_A                 ;A800
V3EFC:  JSRL   CHAR_R                 ;A87B
V3EFE:  JSRL   CHAR_T                 ;A88A
V3F00:  JSRL   CHAR_                  ;A8B4
V3F02:  JSRL   CHAR_2                 ;A8BA
V3F04:  JSRL   TOZERO                 ;AF4C
V3F06:  JSRL   CHAR_S                 ;A883  [.IRPC X,SCORES]
V3F08:  JSRL   CHAR_C                 ;A815
V3F0A:  JSRL   CHAR_O                 ;A865
V3F0C:  JSRL   CHAR_R                 ;A87B
V3F0E:  JSRL   CHAR_E                 ;A823
V3F10:  JSRL   CHAR_S                 ;A883
V3F12:  CSTAT  4                      ;68C4  TURQOI
V3F14:  RTSL                          ;C000

SYSOPT:          ;JSRL operand $F8B  (source name SYSOPT, ALVROM.MAC:1273)
;  6502 pointer table (.WORD) of self-test option literals
;  refs: program ROM DSPSYS+$2D (ALTES2): LDA Y,SYSOPT+9 at $D831
;  refs: program ROM DSPSYS+$30 (ALTES2): LDX Y,SYSOPT+8 at $D834
;  refs: program ROM DSPSYS+$43 (ALTES2): LDA Y,SYSOPT+1 at $D847
;  refs: program ROM DSPSYS+$46 (ALTES2): LDX Y,SYSOPT at $D84A
V3F16:  .word ENTEST            ;option 0 literal
V3F18:  .word ENTEST            ;option 1 literal
V3F1A:  .word ZTIMES            ;option 2 literal
V3F1C:  .word ZHISCO            ;option 3 literal
V3F1E:  .word ZMED              ;option 4 literal
V3F20:  .word ZEASY             ;option 5 literal
V3F22:  .word ZHARD             ;option 6 literal
V3F24:  .word ZMED              ;option 7 literal

ZEASY:           ;JSRL operand $F93  (source name ZEASY, ALVROM.MAC:1276)
;  Literal 'EASY'
;  refs: vector ROM .WORD at $3F20
;  refs: program ROM TEXIT+$14 (ALWELG): CMP I,ZEASY at $932D
V3F26:  VCTR   0, -32, 0              ;1FE0 0000
V3F2A:  JSRL   CHAR_E                 ;A823  [.IRPC X,<EAS>]
V3F2C:  JSRL   CHAR_A                 ;A800
V3F2E:  JSRL   CHAR_S                 ;A883
V3F30:  JMPL   CHAR_Y                 ;E8A7

ZMED:            ;JSRL operand $F99  (source name ZMED, ALVROM.MAC:1282)
;  Literal 'MEDIUM'
;  refs: vector ROM .WORD at $3F1E
;  refs: vector ROM .WORD at $3F24
V3F32:  VCTR   0, -32, 0              ;1FE0 0000
V3F36:  JSRL   CHAR_M                 ;A85A  [.IRPC X,<MEDIU>]
V3F38:  JSRL   CHAR_E                 ;A823
V3F3A:  JSRL   CHAR_D                 ;A81B
V3F3C:  JSRL   CHAR_I                 ;A842
V3F3E:  JSRL   CHAR_U                 ;A890
V3F40:  JMPL   CHAR_M                 ;E85A

ZHARD:           ;JSRL operand $FA1  (source name ZHARD, ALVROM.MAC:1288)
;  Literal 'HARD'
;  refs: vector ROM .WORD at $3F22
;  refs: program ROM TEXIT+$34 (ALWELG): CMP I,ZHARD at $934D
V3F42:  VCTR   0, -32, 0              ;1FE0 0000
V3F46:  JSRL   CHAR_H                 ;A83B
V3F48:  JSRL   CHAR_A                 ;A800
V3F4A:  JSRL   CHAR_R                 ;A87B
V3F4C:  JMPL   CHAR_D                 ;E81B

VORLIT:          ;JSRL operand $FA7  (source name VORLIT, ALVROM.MAC:1300)
TEMLIT:          ;same address (source name TEMLIT)
;  TEMPEST logo (global name; same address as TEMLIT)
;  TEMPEST logo
;  refs: program ROM LOGPRO (ALSCO2): LDAH VORLIT+1 at $B131
;  refs: program ROM LOGPRO+$3 (ALSCO2): LXL VORLIT at $B134
V3F4E:  CNTR                          ;8040
V3F50:  VCTR   -432, 256, 0           ;0100 1E50
V3F54:  JSRL   T                      ;AFBD
V3F56:  VCTR   96, 0, 0               ;0000 0060
V3F5A:  JSRL   E                      ;AFC4
V3F5C:  VCTR   36, 0, 0               ;0000 0024
V3F60:  JSRL   M                      ;AFD1
V3F62:  VCTR   52, 0, 0               ;0000 0034
V3F66:  JSRL   P                      ;AFE0
V3F68:  VCTR   248, 72, 0             ;0048 00F8
V3F6C:  JSRL   E                      ;AFC4
V3F6E:  VCTR   22, 40, 0              ;0028 0016
V3F72:  JSRL   S                      ;AFEA
V3F74:  VCTR   96, -96, 0             ;1FA0 0060
V3F78:  JMPL   T                      ;EFBD

T:               ;JSRL operand $FBD  (source name T, ALVROM.MAC:1315)
;  Logo letter T
;  refs: vector ROM JSRL at $3F54
;  refs: vector ROM JMPL at $3F78
V3F7A:  VCTR   0, 128, 6              ;0080 C000  T
V3F7E:  VCTR   -80, 0, 0              ;0000 1FB0
V3F82:  VCTR   160, 0, 6              ;0000 C0A0
V3F86:  RTSL                          ;C000

E:               ;JSRL operand $FC4  (source name E, ALVROM.MAC:1319)
;  Logo letter E
;  refs: vector ROM JSRL at $3F5A
;  refs: vector ROM JSRL at $3F6C
V3F88:  VCTR   -80, 0, 6              ;0000 DFB0  E
V3F8C:  VCTR   -20, -64, 6            ;1FC0 DFEC
V3F90:  VCTR   112, 0, 6              ;0000 C070
V3F94:  VCTR   -112, 0, 0             ;0000 1F90
V3F98:  VCTR   -20, -64, 6            ;1FC0 DFEC
V3F9C:  VCTR   132, 0, 6              ;0000 C084
V3FA0:  RTSL                          ;C000

M:               ;JSRL operand $FD1  (source name M, ALVROM.MAC:1326)
;  Logo letter M
;  refs: vector ROM JSRL at $3F60
V3FA2:  VCTR   -32, 0, 6              ;0000 DFE0  M
V3FA6:  VCTR   48, 128, 6             ;0080 C030
V3FAA:  SVEC   16, 0, 6               ;40C8
V3FAC:  VCTR   32, -88, 6             ;1FA8 C020
V3FB0:  VCTR   32, 88, 6              ;0058 C020
V3FB4:  SVEC   16, 0, 6               ;40C8
V3FB6:  VCTR   48, -128, 6            ;1F80 C030
V3FBA:  VCTR   -32, 0, 6              ;0000 DFE0
V3FBE:  RTSL                          ;C000

P:               ;JSRL operand $FE0  (source name P, ALVROM.MAC:1335)
;  Logo letter P
;  refs: vector ROM JSRL at $3F66
V3FC0:  SVEC   -16, 0, 6              ;40D8  P
V3FC2:  VCTR   0, 128, 6              ;0080 C000
V3FC6:  VCTR   92, 0, 6               ;0000 C05C
V3FCA:  VCTR   26, -72, 6             ;1FB8 C01A
V3FCE:  VCTR   -118, 0, 6             ;0000 DF8A
V3FD2:  RTSL                          ;C000

S:               ;JSRL operand $FEA  (source name S, ALVROM.MAC:1341)
;  Logo letter S
;  refs: vector ROM JSRL at $3F72
V3FD4:  VCTR   -16, -40, 6            ;1FD8 DFF0  S
V3FD8:  VCTR   144, 0, 6              ;0000 C090
V3FDC:  VCTR   0, 56, 6               ;0038 C000
V3FE0:  VCTR   -112, 32, 6            ;0020 DF90
V3FE4:  VCTR   16, 40, 6              ;0028 C010
V3FE8:  VCTR   100, 0, 6              ;0000 C064
V3FEC:  VCTR   -12, -32, 6            ;1FE0 DFF4
V3FF0:  RTSL                          ;C000

KILLER:          ;JSRL operand $FF9  (source name KILLER, ALVROM.MAC:1350)
;  Beam to the corners with blank vectors, then RTSL (CHKSM1 word)
;  no lit vectors - beam positioning / advance / vector-RAM list
;  refs: program ROM ZATC1S+$13 (ALSCO2): LDAH KILLER+1 at $B1AF
;  refs: program ROM ZATC1S+$16 (ALSCO2): LXL KILLER at $B1B2
;  refs: program ROM DSBOOM (ALDIS2): LAH KILLER+1 at $B8BA
;  refs: program ROM DSBOOM+$3 (ALDIS2): LXL KILLER at $B8BD
V3FF2:  CNTR                          ;8040
V3FF4:  SCAL   1, $00                 ;7100
V3FF6:  VCTR   500, 540, 0            ;021C 01F4
V3FFA:  VCTR   -1000, -1080, 0        ;1BC8 1C18

CHKSM1:          ;JSRL operand $FFF  (source name CHKSM1, ALVROM.MAC:1354)
;  checksum byte QCHKS1 hidden in an RTSL word ($C0xx)
V3FFE:  .byte $69, $C0          ;.BYTE QCHKS1,0C0 - decodes as RTSL $C069

;----------------------------[ cross reference ]----------------------------
;name              addr   referenced from
;CHAR_A            $3000  $31FA, $3478, $3DEA, $3E08, $3E10, $3E30, $3E5A, $3E66, $3E8E, $3EB0, $3EDA, $3EFA, $3F2C,
;                         $3F48
;CHAR_B            $3010  $31FC, $3E24
;CHAR_C            $302A  $31FE, $3488, $3E46, $3F08
;CHAR_D            $3036  $3200, $3DF0, $3E32, $3E34, $3E4C, $3E92, $3F3A, $3F4C
;CHAR_E            $3046  $3202, $3474, $3DEE, $3E0C, $3E14, $3E36, $3E44, $3E5E, $3E6A, $3E7C, $3E8A, $3EA2, $3EC0,
;                         $3ECA, $3EEC, $3F0E, $3F2A, $3F38
;CHAR_F            $3056  $3204, $3E84, $3EB6, $3EC4
;CHAR_G            $3064  $3206, $3480, $3E12, $3E64
;CHAR_H            $3076  $3208, $3F46
;CHAR_I            $3084  $320A, $347C, $3E86, $3EE8, $3F3C
;CHAR_J            $3092  $320C
;CHAR_K            $309E  $320E
;CHAR_L            $30AA  $3210, $3DE8, $3E58, $3EC2
;CHAR_M            $30B4  $3212, $3E68, $3EEA, $3F36, $3F40
;CHAR_N            $30C0  $3214, $347E, $3DDE, $3E28, $3E4A, $3E90
;CHAR_O            $30CA  $31E6, $3216, $3DA0, $3DB0, $3DDC, $3E26, $3E48, $3E9C, $3EA6, $3EB8, $3F0A, $CDFC, $CE4E
;CHAR_P            $30D6  $3218, $3DE6, $3E56, $3E78, $3EB2
;CHAR_Q            $30E4  $321A
;CHAR_R            $30F6  $321C, $3476, $3E0E, $3E38, $3E60, $3E7A, $3E88, $3EA4, $3EBA, $3EDC, $3EFC, $3F0C, $3F4A
;CHAR_S            $3106  $321E, $347A, $3E2C, $3E42, $3E4E, $3E6C, $3E7E, $3E80, $3EBE, $3ECC, $3ED6, $3EEE, $3EF6,
;                         $3F06, $3F10, $3F2E
;CHAR_T            $3114  $3220, $3E9A, $3EC8, $3ECE, $3ED8, $3EDE, $3EE6, $3EF8, $3EFE
;CHAR_U            $3120  $3222, $3E2A, $3F3E
;CHAR_V            $312C  $3224, $3E0A
;CHAR_W            $3136  $3226
;CHAR_X            $3144  $3228, $3E1C
;CHAR_Y            $314E  $322A, $3DEC, $3E5C, $3F30
;CHAR_Z            $315C  $322C, $3EA0, $3EAE
;CHAR_             $3168  $31E4, $322E, $3E2E, $3E50, $3E54, $3E62, $3E82, $3E8C, $3E94, $3E98, $3E9E, $3EA8, $3EB4,
;                         $3EBC, $3EC6, $3EE0, $3F00, $CDF2, $CDF4, $CDF6, $CDF8, $CDFA, $CE1A, $CE1C, $CE1E, $CE20,
;                         $CE22, $CE24, $CE2C, $CE2E, $CE36, $CE38, $CE3A, $CE44, $CE46, $CE48, $CE4A, $CE4C
;CHAR_1            $316C  $31E8, $3262, $3DF6, $3E1E, $3EE2
;CHAR_2            $3174  $31EA, $3266, $3DAC, $3DFE, $3F02
;CHAR_3            $3184  $31EC
;CHAR_4            $3192  $31EE
;CHAR_5            $31A0  $31F0, $3D9E, $3DAE
;CHAR_6            $31AE  $31F2
;CHAR_7            $31BC  $31F4, $3D92
;CHAR_8            $31C6  $31F6
;CHAR_9            $31D6  $31F8
;VGMSGA            $31E4  $334C, $A8DE INFO, $A937 ZATC4V, $AA09 NWHEXZ, $AB7C MSGNOP, $AB84 MSGNOP, $AF10 OUTINI,
;                         $AF16 OUTINI, $DCC0 BADBOX, $DCC3 BADBOX, $DF29 VGHEX1, $DF2E VGHEX1
;DASH              $3236  $3230
;COPYR             $323E  $3234
;HALF              $325C  $3232, $AAF3 IHALF
;CHKSM0            $326A  (no static reference)
;LIFEY             $326C  $D81F DSPSYS, $D822 DSPSYS
;LIFE1             $326E  $3284
;LIFE0             $327E  $3286, $CE08, $CE0A, $CE0C, $CE0E, $CE10, $CE12, $CE5A, $CE5C, $CE5E, $CE60, $CE62, $CE64
;LSYMBL            $3284  $A9A9 UPSCLI
;LSYMB0            $3286  $A9B2 UPSCLI
;SEVEN             $3288  $32C0, $32CA, $32D4, $32DE, $32E8, $32F2
;SEVEN2            $3294  $3308
;INTEST            $32B6  $DB7E SINTEN, $DB81 SINTEN
;HATCH             $330A  $DB84 SHATCH, $DB87 SHATCH
;CHEKER            $334E  $DB78 SCHEKR, $DB7B SCHEKR
;HYSTER            $3456  $DBBD SHYSTE, $DBC0 SHYSTE
;EASING            $346E  $D87F DSPSYS, $D882 DSPSYS
;COCMSG            $3482  $DC61 BADBOX, $DC64 BADBOX
;CNWHSC            $348A  $3456, $346E, $3482
;ROMRPI            $3492  $DC7E BADBOX, $DC81 BADBOX
;BONDRY            $34A6  $32B6, $330A, $3492, $3DCE, $DB53 SIGANA, $DB56 SIGANA
;VORBOX            $34AA  $B102 BOXPRO, $B105 BOXPRO, $DBCE SHYSTE, $DBD1 SHYSTE
;EXPL1             $34C2  $CEC8, $B7E5 TEXTYP
;EXPL2             $34F8  $CECA
;EXPL3             $3522  $CECC
;EXPL4             $355A  $CECE
;DIARA2            $3594  $CED0, $B771 DSPCHG
;STAR1B            $3622  $3B88, $3B8E, $3B94, $3B9A, $3BA0, $3BA6, $3BAC
;STAR1             $3628  $3624, $CED2, $C59D DSTARF
;STAR2             $36DE  $3626, $CED4
;STAR3             $3780  $CED6
;STAR4             $382A  $CED8
;SPIRA1            $38CC  $CEDA, $B62B TRAPIC, $B630 TRATAB, $B632 TRATAB
;SPIRA2            $38FA  $CEDC
;SPIRA3            $3928  $CEDE
;SPIRA4            $3956  $CEE0
;TANKP             $3984  $CF12, $B61E TANTAB
;TANKF             $3996  $CF14, $B61E TANTAB
;TANKR             $39B0  $CEE2, $B61E TANTAB
;GENTNK            $39B4  $3994, $39AE
;SPARK1            $39F4  $CEE4, $B7EA TEXTYP, $C70B TIPACT
;SPARK2            $3A1A  $CEE6
;ESHOT1            $3A40  $CEE8, $B77C DSPCHG
;ESHOT2            $3A72  $CEEA
;ESHOT3            $3AA2  $CEEC
;ESHOT4            $3AD4  $CEEE
;JADOT             $3AFE  $3BD0, $3BEE, $3C14, $3C1C, $3C3E, $3C68, $3DB2
;SPLAT1            $3B04  $CEFA
;SPLAT2            $3B08  $CEF8
;SPLAT3            $3B0C  $CEF6, $CEFC
;SPLAT4            $3B10  $CEF4
;SPLAT5            $3B14  $CEF2, $CEFE
;SPLAT6            $3B18  $CEF0, $CF00, $B81E CHPLKI
;SPLAT             $3B1A  $3B06, $3B0A, $3B0E, $3B12, $3B16
;SPLFU1            $3B84  $CF04
;SPLFU2            $3B8A  $CF06
;SPLFU3            $3B90  $CF08
;SPLFU4            $3B96  $CF0A
;SPLFU5            $3B9C  $CF0C
;SPLFU6            $3BA2  $CF0E
;SPLFU7            $3BA8  $CF10
;SHRAP             $3BAE  $CF02
;FUSE0             $3C6A  $CF16, $B6EC M10
;FUSE1             $3CB2  $CF18
;FUSE2             $3CFC  $CF1A
;FUSE3             $3D44  $CF1C
;FUSEX1            $3D8A  $CF1E, $B7E7 TEXTYP, $B7E8 TEXTYP, $B7E9 TEXTYP
;FUSEX2            $3D96  $CF20
;FUSEX3            $3DA4  $CF22
;FIFTY             $3DAE  $3D94
;ZERO              $3DB0  $3DA2
;JSRDOT            $3DB2  $C72D WHITIP, $C733 WHITIP
;SWNORM            $3DB4  $CEC2
;SWMSGS            $3DC8  $CEC4
;SWHALT            $3DCC  $CEC6
;BOKLIT            $3DCE  $DD7E DBOOKE, $DD81 DBOOKE
;SECONDS           $3E42  $3DDA, $3DE4, $3E06
;PLYGAM            $3E54  $3DF8, $3E00
;FIRED             $3E70  $3EAC, $3ED4, $3EF4
;TOZERO            $3E98  $3EE4, $3F04
;ENTEST            $3EAC  $3F16, $3F18
;ZTIMES            $3ED4  $3F1A
;ZHISCO            $3EF4  $3F1C
;SYSOPT            $3F16  $D831 DSPSYS, $D834 DSPSYS, $D847 DSPSYS, $D84A DSPSYS
;ZEASY             $3F26  $3F20, $932D TEXIT
;ZMED              $3F32  $3F1E, $3F24
;ZHARD             $3F42  $3F22, $934D TEXIT
;VORLIT            $3F4E  $B131 LOGPRO, $B134 LOGPRO
;T                 $3F7A  $3F54, $3F78
;E                 $3F88  $3F5A, $3F6C
;M                 $3FA2  $3F60
;P                 $3FC0  $3F66
;S                 $3FD4  $3F72
;KILLER            $3FF2  $B1AF ZATC1S, $B1B2 ZATC1S, $B8BA DSBOOM, $B8BD DSBOOM
;CHKSM1            $3FFE  (no static reference)
