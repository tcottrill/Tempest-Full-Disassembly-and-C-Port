@echo off
rem Record everything the gates compare against: the oracle dumps, the EAROM
rem image and the 12 lockstep traces.  None of it is distributed - it is all
rem recorded from the running ROM - and all of it is deterministic: recording
rem twice gives identical bytes.  Run build_all.bat first.  Takes about a minute.
rem
rem   tests\ref            34 attract dumps (frame_NNNN.ram / .vram / .col, ref_index.json)
rem   tests\ref_selftest   the self-test dumps
rem   tests\earom\earom.nv the EAROM image the ROM writes in the earom scenario
rem   tests\gate\*.trc     the traces tests\gate.exe replays (about 54 MB)
rem   tests\ref\avg_shapes_ref.txt   Gate V's reference, from the disassembly's
rem                        Python AVG model: needs ..\disasm\build (gen_from_roms.py)
cd /d "%~dp0"
for %%f in (tests\refrun.exe tests\lockstep.exe) do if not exist %%f (echo MISSING %%f - run build_all.bat & exit /b 1)
if not exist tests\gate mkdir tests\gate
if not exist tests\earom mkdir tests\earom
set SC=tests\scenarios
set LS=tests\lockstep.exe --no-dumps

echo === tests\ref
tests\refrun.exe --frames 600 > nul || (echo FAILED tests\ref & exit /b 1)
rem This capture is cut at pass 1361, just after the watchdog reboot that ends
rem the self test, when the display list is still empty - so refrun's verdict
rem on the last frame (it wants vectors on screen) is FAIL by construction.
rem The whole scenario is judged by the selftest_boot lockstep run below.
echo === tests\ref_selftest
if exist tests\ref_selftest\ref_index.json del tests\ref_selftest\ref_index.json
tests\refrun.exe --frames 1361 --capture-every 20 --outdir tests\ref_selftest --script %SC%\selftest_boot.txt > nul
if not exist tests\ref_selftest\ref_index.json (echo FAILED tests\ref_selftest & exit /b 1)

call :trace attract_600          --frames 600
call :trace attract_5000         --frames 5000
call :trace coin_start_3000      --frames 3000 --script %SC%\coin_start.txt
call :trace high_score_3000      --frames 3000 --script %SC%\high_score.txt
call :trace two_player_3000      --frames 3000 --script %SC%\two_player.txt
call :trace superzapper_3000     --frames 3000 --script %SC%\superzapper.txt
call :trace fuseball_pulsar_4000 --frames 4000 --script %SC%\fuseball_pulsar.txt
call :trace slam_1500            --frames 1500 --script %SC%\slam.txt
call :trace selftest_boot_1800   --frames 1800 --script %SC%\selftest_boot.txt
call :trace selftest_midrun_2500 --frames 2500 --script %SC%\selftest_midrun.txt
rem the earom run writes the image the readback run boots from: keep the order
call :trace earom_2600           --frames 2600 --script %SC%\earom.txt --earom-out tests\earom\earom.nv
call :trace earom_readback_600   --frames 600 --script %SC%\earom_readback.txt --earom-in tests\earom\earom.nv
if defined FAILED (echo RECORD FAILED:%FAILED% & exit /b 1)

if exist ..\disasm\build\symbols.json (
    echo === tests\ref\avg_shapes_ref.txt
    python tools\avg_ref.py shapes > nul || (echo FAILED avg_shapes_ref.txt & exit /b 1)
) else (
    echo --- skipped tests\ref\avg_shapes_ref.txt ^(Gate V^): no ..\disasm\build - run disasm\gen_from_roms.py first
)
echo RECORD OK
exit /b 0

:trace
echo === tests\gate\%1.trc
set NAME=%1
shift
set ARGS=
:trace_args
if not "%1"=="" (set ARGS=%ARGS% %1& shift & goto trace_args)
%LS%%ARGS% --trace-out tests\gate\%NAME%.trc > nul
if errorlevel 1 set FAILED=%FAILED% %NAME%
exit /b 0
