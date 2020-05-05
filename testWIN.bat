@echo off
cls

REM ################
REM # Enable delayed expansion on script

setlocal ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

REM ################
REM # Initialize environment

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"
if %errorlevel% EQU 0 goto ENVOK
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars32.bat"
if %errorlevel% NEQ 0 (
   echo Failed to find Visual Studio Developer Native tools!
   echo Please install the latest version of
   echo Microsoft Visual Studio 2017 Community Edition.
   goto END
)
:ENVOK

echo.
echo   testWIN.bat - Mochimo Algorithm compilation and execution script
echo                 Trigg, Peach
echo.

REM ################
REM # Remove old error log

set LOG="algotest.log"
del /f /q %LOG% 1>NUL 2>&1

REM ################
REM # Verify Dependencies

echo Dependencies...
git --version 1>NUL 2>&1
if %errorlevel% EQU 0 echo   - git OK && goto DEPENDENCIES
echo   - git NOT OK
echo Failed to check the version of 'git' on this system.
echo Please check git is installed and included in PATH.
goto END

:DEPENDENCIES
mkdir dependencies 1>NUL 2>&1

if not exist dependencies\hash-functions\ goto NOHASH
goto CHECKHASH

:NOHASH
git clone https://github.com/Zalamanda/hash-functions.git dependencies\hash-functions >>%LOG% 2>&1
if %errorlevel% NEQ 0 (
   echo   - hash-functions NOT OK. *Unable to git clone repository.
   echo Please try downloading https://github.com/Zalamanda/hash-functions.git
   echo to the dependencies folder.
   goto END
)
echo   - hash-functions OK [cloned]
goto HASHOK

:CHECKHASH
git --git-dir="dependencies\hash-functions\.git" --work-tree="dependencies\hash-functions" update-index --refresh 1>NUL 2>&1
git --git-dir="dependencies\hash-functions\.git" --work-tree="dependencies\hash-functions" diff-index --quiet HEAD --
if %errorlevel% NEQ 0 (
   echo   - hash-functions OK? [local changes]
   goto HASHOK
)
git --git-dir="dependencies\hash-functions\.git" --work-tree="dependencies\hash-functions" pull >>%LOG% 2>&1
echo   - hash-functions OK [latest]

:HASHOK
mkdir hash 1>NUL 2>&1
xcopy /Y /D dependencies\hash-functions\src\* hash\ 2>%LOG% 1>NUL

REM ################
REM # Build test software

echo | set /p="Building Algotest... "
cl /nologo /WX /Fealgotest.exe test\algotest.c >>%LOG% 2>&1

if %errorlevel% NEQ 0 (
   echo Error
   echo.
   more %LOG%
) else (
   echo OK
   echo.
   echo Done.

REM ################
REM # Run software on success

   start "" /b /wait "algotest.exe"

)

echo.
echo ********

REM ################
REM # Rebuild test software with alternate config

echo | set /p="Building Algotest (with static Peach semaphores)... "
cl /nologo /WX /DSTATIC_PEACH_MAP /Fealgotest.exe test\algotest.c >>%LOG% 2>&1

if %errorlevel% NEQ 0 (
   echo Error
   echo.
   more %LOG%
) else (
   echo OK
   echo.
   echo Done.

REM ################
REM # Run software on success

   start "" /b /wait "algotest.exe"

)

REM ################
REM # Cleanup

if exist "algotest.obj" del /f /q "algotest.obj"
if exist %LOG% del /f /q %LOG%


:END

echo.
pause
EXIT
