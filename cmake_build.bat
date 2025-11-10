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
set "BUILD_ALL=0"
set "DO_PACKAGE=0"
set "INSTALL_DIR=c:\tools\ZLibraries"



:: Parse arguments: detect -build, -debug, and collect targets
for %%A in (%*) do (
    if /I "%%~A"=="-build" (
        set "DO_BUILD=1"
    ) else if /I "%%~A"=="-debug" (
        set "BUILD_TYPE=Debug"
    ) else if /I "%%~A"=="all" (
    	  set "BUILD_ALL=1"
    ) else (
        set "TARGETS=!TARGETS! %%~A"
    )
)

set "DEST_DIR=%BATCH_FOLDER%build\ZLibraries_Release"
if not exist "%DEST_DIR%" mkdir "%DEST_DIR%"

:: If "all" is passed, build every subfolder that has a CMakeLists.txt
if "%BUILD_ALL%"=="1" (
    echo:
    echo Building ALL tools in %BATCH_FOLDER%...
    echo:
    cmake -S %BATCH_FOLDER% -B %BATCH_FOLDER%\build -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
    
    if "%DO_BUILD%"=="1" (
	    :: Find MSBuild using vswhere
	    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
	        set "MSBUILD=%%i"
	    )
	    
	    "!MSBUILD!" "%BATCH_FOLDER%\build\ZLibraries.sln" /p:Configuration=%BUILD_TYPE% /m
	    
	    echo:
	    echo Done Building ALL
	    echo:
    	goto copyBuilds
  	) else (
  		start "" "build"
  		exit /b 0
  	)
)




:: Otherwise, build only those explicitly passed
for %%i in (%*) do (
		if exist "%%~i\CMakeLists.txt" (
    	 call :buildOne "%%~i"
    )
)

:copyBuilds
if "%DO_BUILD%"=="1" (
	echo Copying ALL Builds in %BATCH_FOLDER%
	for /d %%i in ("%BATCH_FOLDER%*") do (
			echo Checking: %%i
    	if exist "%%i\CMakeLists.txt" (
        	echo Found project: %%~nxi
        	call :copyOne "%%~nxi"
    	)
	)
	echo Done Copying ALL Builds
) 

goto doneBuild

:buildOne
set "TARGET=%~1"
set "BUILD_DIR=%BATCH_FOLDER%\build\%TARGET%"

echo:
echo =====================================================
echo Building Solution for %TARGET%
echo =====================================================
echo ~1=%~1
echo ~2=%~2

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
		if "%BUILD_TYPE%"=="Release" (
			echo BUILD_TYPE=!BUILD_TYPE!
			echo BUILD_DIR=!BUILD_DIR!
			echo TARGET=!TARGET!
		
		
			echo:
			echo =====================================================
			echo Copying Release Build of %TARGET% to Packaging dir
			set "SRC_DIR=!BUILD_DIR!\Release"
			xcopy "!SRC_DIR!" "!DEST_DIR!" /E /I /Y >nul
			
			echo:
			echo =====================================================
			echo Copying Release Build of %TARGET% to Installation dir %INSTALL_DIR%
			xcopy "!SRC_DIR!" "%INSTALL_DIR%" /E /I /Y >nul
		)
    
) else (
    echo Opening folder: "%BUILD_DIR%"
    start "" "%BUILD_DIR%"
)

exit /b 0






:copyOne
set "TARGET=%~1"
set "BUILD_DIR=%BATCH_FOLDER%build\%TARGET%\"
set "SRC_DIR=!BUILD_DIR!Release"
set "DEST_DIR=!BATCH_FOLDER!build\ZLibraries_Release\"

if "%DO_BUILD%"=="1" (
		if "%BUILD_TYPE%"=="Release" (
			echo:
			echo =====================================================
			echo Copying Release Build of %TARGET% to Packaging dir
			echo SRC_DIR=!SRC_DIR!
			echo DEST_DIR=!DEST_DIR!
			if not exist "!DEST_DIR!" mkdir "!DEST_DIR!"
			xcopy "!SRC_DIR!" "!DEST_DIR!" /E /I /Y >nul
			
			echo:
			echo =====================================================
			echo Copying Release Build of %TARGET% to Installation dir %INSTALL_DIR%
			xcopy "!SRC_DIR!" "%INSTALL_DIR%" /E /I /Y >nul
		)  
)

exit /b 0















:doneBuild
if "%BUILD_TYPE%"=="Release" if "%DO_BUILD%"=="1" (
    echo:
    echo =====================================================
    echo Packaging
		
		for /f "tokens=2-4 delims=/ " %%a in ("%date%") do (
		    set "MONTH=%%a"
		    set "DAY=%%b"
		    set "YEAR=%%c"
		)
		set "DATESTAMP=!YEAR!-!MONTH!-!DAY!"
		
		set "RELEASES=!BATCH_FOLDER!build\ZLibraries_Release"
    set "ARCHIVE=!BATCH_FOLDER!build\ZLibraries_Release_(!DATESTAMP!).7z"
    echo ARCHIVE=!ARCHIVE!
    if exist "!ARCHIVE!" del "!ARCHIVE!"
        if exist "%ProgramFiles%\7-Zip\7z.exe" (
            echo Creating archive: !ARCHIVE!
            "%ProgramFiles%\7-Zip\7z.exe" a -t7z "!ARCHIVE!" "!RELEASES!\*" >nul
        ) else (
            echo 7-Zip not found, skipping archive step.
        )
exit /b 0
		
		
)



echo:
echo [32m*** ALL REQUESTED BUILDS COMPLETE ***[0m
echo:
endlocal
exit /b 0