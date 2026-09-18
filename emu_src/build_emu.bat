@echo off
rem Build tempest_emu.exe: the real Tempest ROM on AAE's cpu_6502 core behind
rem the C port's unchanged Windows backend (x64, VS2022 cl).  Modelled on
rem ..\c_src\build_win.bat, and built AGAINST ..\c_src in place: every shared
rem file is compiled from there by relative path, nothing is copied.
rem
rem Target:
rem   tempest_emu.exe   (emu_src\, next to tempest_win.ini / tempest_win.log /
rem                     tempest.nv - the backend's fixed file names, here in
rem                     emu_src\ so they never meet c_src's):
rem                     emu_main.cpp (the seam) + emu_roms.c (ROMs from
rem                     ..\roms\tempest.zip) + cpu\ (the vendored core)
rem                     + from ..\c_src: state.c avg.c mathbox.c c012294.c
rem                     er2055.c + platform\windows\*
rem   NOT linked: c_src's progrom.c / vecrom.c / mbprom.c (emu_roms.c defines
rem   those symbols), app_loop.c and the translated game modules.
rem
rem Warning levels as build_win.bat: the vendored framework, the chip models
rem and the vendored CPU core at /W3 (not ours to rewrite); our own files and
rem the shared c_src files that build_win.bat holds to /W4 stay /W4.
rem Objects go to obj\ (tests\build_tests.bat uses obj\klaus\, obj\romcheck\).
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 -no_logo
cd /d "%~dp0"

set CSRC=..\c_src
set WIN=%CSRC%\platform\windows
for %%f in (%CSRC%\state.c %CSRC%\state_defs.h %CSRC%\avg.c %CSRC%\mathbox.c %CSRC%\c012294.c %CSRC%\er2055.c %WIN%\plat_win.c cpu\cpu_6502.cpp) do if not exist %%f (echo MISSING %%f & exit /b 1)
if not exist obj mkdir obj

rem ---- vendored framework + chip models (their own warning level) ---------
cl /nologo /O2 /W3 /MD /D_CRT_SECURE_NO_WARNINGS /DUNICODE /D_UNICODE /c ^
   %WIN%\sys_gl.c %WIN%\glew.c ^
   %WIN%\log.c %WIN%\vector_draw.c ^
   %WIN%\mat4.c %WIN%\rawinput.c ^
   %WIN%\mixer.c %WIN%\fileio.c ^
   %WIN%\miniz.c %WIN%\ini.c ^
   %WIN%\joystick.c ^
   /Foobj\ || exit /b 1
cl /nologo /O2 /W3 /std:c11 /MD /D_CRT_SECURE_NO_WARNINGS /I%CSRC% /c %CSRC%\c012294.c %CSRC%\er2055.c /Foobj\ || exit /b 1

rem ---- the vendored 6502 core + its log shim -------------------------------
cl /nologo /O2 /W3 /EHsc /std:c++17 /MD /D_CRT_SECURE_NO_WARNINGS /DEMU_LOG_TO_BACKEND /I%CSRC% /c ^
   cpu\cpu_6502.cpp cpu\sys_log_shim.cpp /Foobj\ || exit /b 1

rem ---- shared c_src machine files, the backend, the ROM loader (/W4) -------
cl /nologo /O2 /W4 /std:c11 /MD /D_CRT_SECURE_NO_WARNINGS /DUNICODE /D_UNICODE /I%CSRC% /c ^
   %CSRC%\state.c %CSRC%\avg.c %CSRC%\mathbox.c ^
   %WIN%\plat_win.c %WIN%\win_probe.c ^
   emu_roms.c ^
   /Foobj\ || exit /b 1

rem ---- the seam, and the link ---------------------------------------------
echo === tempest_emu.exe
cl /nologo /O2 /W4 /EHsc /std:c++17 /MD /D_CRT_SECURE_NO_WARNINGS /I%CSRC% /Foobj\ ^
   emu_main.cpp ^
   obj\emu_roms.obj obj\cpu_6502.obj obj\sys_log_shim.obj ^
   obj\state.obj obj\avg.obj obj\mathbox.obj ^
   obj\plat_win.obj obj\win_probe.obj ^
   obj\c012294.obj obj\er2055.obj ^
   obj\sys_gl.obj obj\glew.obj obj\log.obj ^
   obj\vector_draw.obj obj\mat4.obj obj\rawinput.obj ^
   obj\mixer.obj obj\fileio.obj obj\miniz.obj ^
   obj\ini.obj obj\joystick.obj ^
   /Fe:tempest_emu.exe ^
   /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib winmm.lib ole32.lib || exit /b 1

echo EMU BUILD OK
