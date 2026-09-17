@echo off
rem Build the playable Tempest C port with the Windows backend (x64, VS2022 cl).
rem Modelled on the Space Duel port's build_win.bat: the vendored framework
rem files compile at /W3 (harvested copies, not ours to rewrite); the game,
rem the seam and the Tempest backend stay /W4 /std:c11.
rem
rem Target:
rem   tempest_win.exe   (c_src\, next to tempest_win.ini / tempest_win.log /
rem                     tempest.nv): app_loop.c + every game module + avg.c,
rem                     mathbox.c, the ROM images, the chip models (c012294.c,
rem                     er2055.c) + platform\windows\* (NOTES_m8.md, Part 2)
rem
rem Objects go to obj\win\ so they never collide with build_all.bat's
rem (obj\app_loop.obj there is the skeleton build).
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 -no_logo
cd /d "%~dp0"

for %%f in (state_defs.h progrom.c vecrom.c mbprom.c allang_data.c allang_data.h aldis2_data.h alwelg_data.h altes2_data.h) do if not exist %%f (echo MISSING %%f - run the generators & exit /b 1)
if not exist obj mkdir obj
if not exist obj\win mkdir obj\win

rem ---- vendored framework + chip models (their own warning level) ---------
cl /nologo /O2 /W3 /MD /D_CRT_SECURE_NO_WARNINGS /DUNICODE /D_UNICODE /c ^
   platform\windows\sys_gl.c platform\windows\glew.c ^
   platform\windows\log.c platform\windows\vector_draw.c ^
   platform\windows\mat4.c platform\windows\rawinput.c ^
   platform\windows\mixer.c platform\windows\fileio.c ^
   platform\windows\miniz.c platform\windows\ini.c ^
   platform\windows\joystick.c ^
   /Foobj\win\ || exit /b 1
cl /nologo /O2 /W3 /std:c11 /MD /D_CRT_SECURE_NO_WARNINGS /I. /c c012294.c er2055.c /Foobj\win\ || exit /b 1

rem ---- the game -------------------------------------------------------------
set GAME=state.c alexec.c alhar2.c altes2.c aldis2.c alsoun.c alwelg.c alsco2.c alcoin.c allang.c allang_data.c alearo.c alvgut.c
set HW=mathbox.c mbprom.c progrom.c vecrom.c avg.c

rem M9 B5: app_loop.c implements hw_reset / hw_watchdog_hang (the self test)
echo === tempest_win.exe
cl /nologo /O2 /W4 /std:c11 /MD /D_CRT_SECURE_NO_WARNINGS /DALTES2_SEAM_HAS_RESET_HOOKS /DUNICODE /D_UNICODE /I. /Foobj\win\ ^
   app_loop.c %GAME% %HW% ^
   platform\windows\plat_win.c platform\windows\win_probe.c ^
   obj\win\c012294.obj obj\win\er2055.obj ^
   obj\win\sys_gl.obj obj\win\glew.obj obj\win\log.obj ^
   obj\win\vector_draw.obj obj\win\mat4.obj obj\win\rawinput.obj ^
   obj\win\mixer.obj obj\win\fileio.obj obj\win\miniz.obj ^
   obj\win\ini.obj obj\win\joystick.obj ^
   /Fe:tempest_win.exe ^
   /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib winmm.lib ole32.lib || exit /b 1

echo WIN BUILD OK
