@echo off
setlocal EnableExtensions
set "SKETCH=%~dp0"
set "OUT=%SKETCH%GitVersion.generated.h"
set "TMP=%SKETCH%GitVersion.generated.tmp"
set "GITTMP=%TEMP%\GpmGitVersion_%RANDOM%_%RANDOM%.txt"
set "FAIL_DEFINE=INVALID_GIT_BUILD"
git -C "%SKETCH%." rev-parse --show-toplevel > "%GITTMP%" 2>nul
if errorlevel 1 goto fail
set /p REPO=<"%GITTMP%"
if not defined REPO goto fail
git -C "%REPO%" branch --show-current > "%GITTMP%" 2>nul
if errorlevel 1 goto fail
set /p BRANCH=<"%GITTMP%"
if not defined BRANCH goto fail
echo Git branch: %BRANCH%
if /I not "%BRANCH:~0,8%"=="release/" goto development
set "FAIL_DEFINE=INVALID_RELEASE_BUILD"
git -C "%REPO%" status --porcelain -- . ":(exclude)**/GitVersion.generated.h" ":(exclude,glob)**/*.vcxproj" > "%GITTMP%"
if errorlevel 1 goto fail
for %%A in ("%GITTMP%") do if not "%%~zA"=="0" goto fail
git -C "%REPO%" fetch origin "%BRANCH%" --quiet
if errorlevel 1 goto fail
git -C "%REPO%" rev-parse HEAD > "%GITTMP%"
if errorlevel 1 goto fail
set /p LOCAL=<"%GITTMP%"
git -C "%REPO%" rev-parse FETCH_HEAD > "%GITTMP%"
if errorlevel 1 goto fail
set /p REMOTE=<"%GITTMP%"
if /I not "%LOCAL%"=="%REMOTE%" goto fail
git -C "%REPO%" rev-parse --short=7 HEAD > "%GITTMP%"
if errorlevel 1 goto fail
set /p HASH=<"%GITTMP%"
if not defined HASH goto fail
> "%TMP%" echo #pragma once
>> "%TMP%" echo #define GIT_VERSION "%HASH%"
echo Git version: %HASH%
goto finish
:development
REM Use the exact locally checked-out firmware commit on development branches.
REM Mark tracked/untracked source changes explicitly rather than claiming a clean commit.
git -C "%REPO%" rev-parse --short=7 HEAD > "%GITTMP%"
if errorlevel 1 goto fail
set /p HASH=<"%GITTMP%"
if not defined HASH goto fail
git -C "%REPO%" status --porcelain -- . ":(exclude)**/GitVersion.generated.h" ":(exclude,glob)**/*.vcxproj" > "%GITTMP%"
if errorlevel 1 goto fail
for %%A in ("%GITTMP%") do if not "%%~zA"=="0" set "HASH=%HASH%-dirty"
> "%TMP%" echo #pragma once
>> "%TMP%" echo #define GIT_VERSION "%HASH%"
echo Git version: %HASH%
:finish
REM Synchronize local Git library revisions before completing this firmware build.
REM The shared .gpm/avr-libraries.json travels with commits through PRs and releases.
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%SKETCH%SyncAvrLibraries.ps1"
if errorlevel 1 goto fail
move /y "%TMP%" "%OUT%" >nul
if errorlevel 1 goto fail
del "%GITTMP%" >nul 2>&1
exit /b 0
:fail
> "%OUT%" echo #pragma once
>> "%OUT%" echo #error %FAIL_DEFINE%
del "%TMP%" >nul 2>&1
del "%GITTMP%" >nul 2>&1
echo ERROR: Cannot generate GitVersion.generated.h from Git state.
exit /b 1
