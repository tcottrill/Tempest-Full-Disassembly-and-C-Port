@echo off
rem Build the Tempest C port (x64, VS2022 cl), modelled on the Space Duel
rem port's build scripts: our code /W4 /std:c11, shared foreign cores at /W3.
rem
rem Targets:
rem   tests\refrun.exe    reference harness: the real rev-3 ROM on the shared
rem                       ref6502 core, per-pass RAM / vector-RAM dumps
rem   tests\lockstep.exe  the same run, verifying every call of the translated
rem                       modules against the ROM (tests\lockstep.c)
rem   tests\skeleton.exe  the translated program on the M8 game seam, headless
rem   tests\tempest_selftest.exe  M8 headless self-test of the seam (NOTES_m8.md)
rem   tests\gate.exe      Gates 1/N: the C modules alone on a lockstep
rem                       --trace-out trace (tests\gate.c)
rem
rem Generated inputs (checked in; rerun after regenerating ..\disasm):
rem   python tools\gen_state.py     python tools\gen_roms.py     python tools\gen_allang.py
rem   python tools\gen_aldis2.py    python tools\gen_alwelg.py    python tools\gen_altes2.py
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 -no_logo
cd /d "%~dp0"

rem the oracle's 6502 core: the copy in tests\ref6502 (from the shared
rem ref6502 project, Klaus-tested there), so the tree builds on its own
set REF6502=tests\ref6502
if not exist "%REF6502%\ref6502.c" (echo MISSING %REF6502%\ref6502.c & exit /b 1)
for %%f in (state_defs.h progrom.c vecrom.c mbprom.c allang_data.c allang_data.h aldis2_data.h alwelg_data.h altes2_data.h) do if not exist %%f (echo MISSING %%f - run the generators & exit /b 1)
if not exist obj mkdir obj
if not exist obj\lockstep mkdir obj\lockstep

rem ---- shared cores (verbatim; their own warning level) --------------------
cl /nologo /O2 /W3 /std:c11 /c "%REF6502%\ref6502.c" /Foobj\ || exit /b 1
cl /nologo /O2 /W3 /std:c11 /D_CRT_SECURE_NO_WARNINGS /I. /c er2055.c /Foobj\ || exit /b 1
cl /nologo /O2 /W3 /std:c11 /D_CRT_SECURE_NO_WARNINGS /I. /c c012294.c /Foobj\ || exit /b 1

set HW=mathbox.c mbprom.c progrom.c vecrom.c
set FLAGS=/nologo /O2 /W4 /std:c11 /D_CRT_SECURE_NO_WARNINGS /I.

rem modules translated and verified so far (PLAN.md)
set PORTED=alvgut.c alhar2.c alcoin.c alsoun.c alexec.c alearo.c alsco2.c allang.c allang_data.c aldis2.c alwelg.c altes2.c

echo === tests\refrun.exe
cl %FLAGS% /Foobj\ /I"%REF6502%" tests\refrun.c %HW% obj\ref6502.obj obj\er2055.obj /Fe:tests\refrun.exe || exit /b 1

rem M8: the same oracle run as a per-pass CPU-cost observer (app_loop.c's pass-cost fit)
if not exist obj\passcost mkdir obj\passcost
echo === tests\passcost.exe
cl %FLAGS% /DPASSCOST /Foobj\passcost\ /I"%REF6502%" tests\refrun.c %HW% obj\ref6502.obj obj\er2055.obj /Fe:tests\passcost.exe || exit /b 1

echo === tests\lockstep.exe
rem M9 B3: altes2.c is in PORTED; lockstep.c implements hw_reset / hw_watchdog_hang
cl %FLAGS% /Foobj\lockstep\ /DLOCKSTEP /DALTES2_SEAM_HAS_RESET_HOOKS /I"%REF6502%" tests\refrun.c tests\lockstep.c state.c %PORTED% %HW% obj\ref6502.obj obj\er2055.obj /Fe:tests\lockstep.exe || exit /b 1

set GAME=state.c alexec.c alhar2.c altes2.c aldis2.c alsoun.c alwelg.c alsco2.c alcoin.c allang.c allang_data.c alearo.c alvgut.c

rem ---- M8: the game seam (app_loop.c) over the headless backend ------------
rem skeleton.exe: N passes of the real boot + loop on the synthetic clock.
rem tempest_selftest.exe: the headless self-test (NOTES_m8.md); altes2.c is
rem compiled with SYSTEM / DSPSYS renamed so app_loop.c can count them.
set SEAM=avg.c platform\headless\plat_headless.c obj\c012294.obj obj\er2055.obj
rem M9 B5: app_loop.c implements hw_reset / hw_watchdog_hang (/DALTES2_SEAM_HAS_RESET_HOOKS).
echo === tests\skeleton.exe
cl %FLAGS% /DTEMPEST_SKELETON_MAIN /DALTES2_SEAM_HAS_RESET_HOOKS /Foobj\ app_loop.c %GAME% %HW% %SEAM% /Fe:tests\skeleton.exe || exit /b 1

if not exist obj\selftest mkdir obj\selftest
echo === tests\tempest_selftest.exe
cl %FLAGS% /DALTES2_SEAM_HAS_RESET_HOOKS /Dsystem_=altes2_system_ /Ddspsys=altes2_dspsys /Foobj\selftest\ /c altes2.c || exit /b 1
set STMODS=state.c alexec.c alhar2.c aldis2.c alsoun.c alwelg.c alsco2.c alcoin.c allang.c allang_data.c alearo.c alvgut.c
cl %FLAGS% /DTEMPEST_SELFTEST_MAIN /Foobj\selftest\ app_loop.c %STMODS% %HW% %SEAM% obj\selftest\altes2.obj /Fe:tests\tempest_selftest.exe || exit /b 1

rem ---- Gates 1 / N (M6): the GAME set alone, replayed on a lockstep trace ----
rem /DLOCKSTEP only turns the modules' CK() checkpoints into lk_ck() calls
rem (defined in tests\gate.c); SYSTEM / DSPSYS are renamed so gate.c can count
rem them; M9 B3: gate.c implements hw_reset / hw_watchdog_hang and boots
rem through altes2.c's reset() (the gate_boot / GETOP3 stand-ins are gone).
if not exist obj\gate mkdir obj\gate
echo === tests\gate.exe
cl %FLAGS% /DLOCKSTEP /DALTES2_SEAM_HAS_RESET_HOOKS /Dsystem_=altes2_system_ /Ddspsys=altes2_dspsys /Foobj\gate\ /c altes2.c || exit /b 1
set GATEMODS=state.c alexec.c alhar2.c aldis2.c alsoun.c alwelg.c alsco2.c alcoin.c allang.c allang_data.c alearo.c alvgut.c
cl %FLAGS% /DLOCKSTEP /Foobj\gate\ tests\gate.c %GATEMODS% %HW% obj\gate\altes2.obj /Fe:tests\gate.exe || exit /b 1

rem ---- AVG display-list walker (Gate V, frame check; see build_avg.bat) ----
if not exist obj\avg mkdir obj\avg
if not exist tests\avg_out mkdir tests\avg_out
echo === tests\avgshapes.exe
cl %FLAGS% /Foobj\avg\ tests\avgshapes.c avg.c state.c progrom.c vecrom.c /Fe:tests\avgshapes.exe || exit /b 1
echo === tests\avgframe.exe
cl %FLAGS% /Foobj\avg\ tests\avgframe.c avg.c state.c progrom.c vecrom.c /Fe:tests\avgframe.exe || exit /b 1
echo === tests\avgtime.exe
cl %FLAGS% /Foobj\avg\ tests\avgtime.c avg.c state.c progrom.c vecrom.c /Fe:tests\avgtime.exe || exit /b 1

echo ALL BUILDS OK
