::  This batch file was made with AI
:: i do NOT know how to make batch files

@echo off
setlocal ENABLEDELAYEDEXPANSION

:: Build script for FSAL and Welt (Windows)
:: Single fsal.exe with integrated installer

set ROOT=%~dp0
set OUT=%ROOT%bin
if not exist "%OUT%" mkdir "%OUT%"

:: Generate config header from r_config.fsal
call w_config.bat
if errorlevel 1 goto :build_fail

:: Paths to folders
set SRC_FSAL=%ROOT%fsal
set SRC_WELT=%ROOT%src

:: Includes
set INC_FSAL=-I"%SRC_FSAL%" -I"%SRC_FSAL%\internal_platform" -I"%SRC_FSAL%\internal_core" -I"%SRC_FSAL%\internal_net" -I"%SRC_FSAL%\internal_archive"
set INC_WELT=-I"%SRC_WELT%\core" -I"%SRC_WELT%\tokenizer" -I"%SRC_WELT%\compiler" -I"%SRC_WELT%\runtime" -I"%SRC_WELT%\diag"

:: Source files
set WELT_OBJS=src\core\core.c src\tokenizer\lexer.c src\runtime\variable.c src\diag\diag.c src\compiler\interpreter.c
set FSAL_DEPS=fsal\internal_platform\platform_win.c fsal\internal_archive\zipwrap_ps.c fsal\internal_core\config.c fsal\internal_core\ui.c fsal\internal_net\fsnet.c

where cl >nul 2>nul
if %ERRORLEVEL%==0 (
  echo Building with MSVC cl...
  cl /D_CRT_SECURE_NO_WARNINGS /nologo /W3 /O2 %INC_FSAL% %INC_WELT% ^
    %FSAL_DEPS% %WELT_OBJS% fsal\fsal.c /Fe:"%OUT%\fsal.exe" /link Ws2_32.lib Winhttp.lib
  if errorlevel 1 goto :build_fail
  goto :build_ok
) else (
  set "GCC_BIN="
  for /f "delims=" %%I in ('where gcc 2^>nul') do set "GCC_BIN=%%I"
  if not defined GCC_BIN for /f "delims=" %%I in ('where x86_64-w64-mingw32-gcc 2^>nul') do set "GCC_BIN=%%I"
  if not defined GCC_BIN goto :no_gcc

  echo Building with GCC: !GCC_BIN!
  set CFLAGS=-O2 -Wall -D_CRT_SECURE_NO_WARNINGS %INC_FSAL% %INC_WELT%
  set LDFLAGS=-lwinhttp -lws2_32 -lole32 -lshell32 -luser32 -ladvapi32
  
  "!GCC_BIN!" !CFLAGS! %FSAL_DEPS:\=/% %WELT_OBJS:\=/% fsal/fsal.c -o "%OUT%\i-fsal.exe" !LDFLAGS!
  if errorlevel 1 goto :build_fail
  
  goto :build_ok
)

:no_gcc
echo No supported C compiler found (cl or gcc).
exit /b 1

:build_ok
echo Build succeeded.
exit /b 0

:build_fail
echo Build failed.
exit /b 1
