@echo off
setlocal enabledelayedexpansion

set INPUT=config\r_config.fsal
set OUTPUT=fsal\config_embedded.h

if not exist "%INPUT%" (
    echo Error: %INPUT% not found
    exit /b 1
)

(
    echo #ifndef CONFIG_EMBEDDED_H
    echo #define CONFIG_EMBEDDED_H
    echo.
    echo static const char EMBEDDED_RECOMMENDED_FSAL[] = 
) > "%OUTPUT%"

for /f "usebackq delims=" %%A in ("%INPUT%") do (
    set "line=%%A"
    setlocal enabledelayedexpansion
    set "line=!line:\=\\!"
    set "line=!line:"=\"!"
    echo "!line!\n" >> "%OUTPUT%"
    endlocal
)

(
    echo ;
    echo.
    echo #endif
) >> "%OUTPUT%"

echo Generated %OUTPUT%
