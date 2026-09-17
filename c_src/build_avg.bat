@echo off
rem Build the AVG walker test tools (M5 part B, Gate V), VS2022 cl /W4 /std:c11.
rem Kept apart from build_all.bat while ALDIS2 is in progress; merge later.
rem
rem   tests\avgshapes.exe   Gate V: every vector-ROM shape vs tests\ref\avg_shapes_ref.txt
rem   tests\avgframe.exe    walks tests\ref\frame_NNNN.vram, writes segments + SVG
rem
rem Reference data:  python tools\avg_ref.py shapes
rem                  python tools\avg_ref.py frame tests\avg_out 0064 0256 0576
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 -no_logo
cd /d "%~dp0"

for %%f in (state_defs.h progrom.c vecrom.c) do if not exist %%f (echo MISSING %%f - run the generators & exit /b 1)
if not exist obj\avg mkdir obj\avg
if not exist tests\avg_out mkdir tests\avg_out

set FLAGS=/nologo /O2 /W4 /std:c11 /D_CRT_SECURE_NO_WARNINGS /I.

echo === tests\avgshapes.exe
cl %FLAGS% /Foobj\avg\ tests\avgshapes.c avg.c state.c progrom.c vecrom.c /Fe:tests\avgshapes.exe || exit /b 1

echo === tests\avgframe.exe
cl %FLAGS% /Foobj\avg\ tests\avgframe.c avg.c state.c progrom.c vecrom.c /Fe:tests\avgframe.exe || exit /b 1

echo AVG BUILDS OK
