@echo off
setlocal enabledelayedexpansion

:: ======================================================
:: Usage:
::   cmake_build.bat all
::   cmake_build.bat reader filegen dupescanner
:: ======================================================

if "%~1"=="" (
    echo:
    echo USAGE: cmake_build.bat [all] or cmake_build.bat [subfolder1] [subfolder2]...
    echo Example: cmake_build.bat reader dupescanner filegen
    echo:
    exit /b 1
)

set "BATCH_FOLDER=%~dp0"
set "BUILD_TYPE=Release"
set "DO_BUILD=0"


:: Parse arguments: detect -build, -debug, and collect targets
for %%A in (%*) do (
    if /I "%%~A"=="-build" (
        set "DO_BUILD=1"
    ) else if /I "%%~A"=="-debug" (
        set "BUILD_TYPE=Debug"
    ) else (
        set "TARGETS=!TARGETS! %%~A"
    )
)

:: If "all" is passed, build every subfolder that has a CMakeLists.txt
if /I "%~1"=="all" (
    echo:
    echo Building ALL tools in %BATCH_FOLDER%...
    echo:
    for /d %%D in ("%BATCH_FOLDER%\*") do (
        if exist "%%~fD\CMakeLists.txt" (
            call :buildOne "%%~nD"
        )
    )
    goto done
)

:: Otherwise, build only those explicitly passed
for %%i in (%*) do (
    call :buildOne "%%~i"
)
goto done

:buildOne
set "TARGET=%~1"
set "BUILD_DIR=%BATCH_FOLDER%\build\%TARGET%"

echo:
echo =====================================================
echo Building Solution for %TARGET%
echo =====================================================

cmake -S "%BATCH_FOLDER%\%TARGET%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 (
    echo *** CMake configuration failed for %TARGET% ***
    exit /b 1
)

if "%DO_BUILD%"=="1" (
    echo Building "%TARGET%"...
    echo command: cmake --build "%BUILD_DIR%" --config %BUILD_TYPE%
    cmake --build "%BUILD_DIR%" --config %BUILD_TYPE%
    if errorlevel 1 (
        echo *** BUILD FAILED for "%TARGET%" ***
        endlocal
        exit /b 1
    )
) else (
    echo Opening folder: "%BUILD_DIR%"
    start "" "%BUILD_DIR%"
)
exit /b 0

:done
echo:
echo [32m*** ALL REQUESTED BUILDS COMPLETE ***[0m
echo:
endlocal
exit /b 0