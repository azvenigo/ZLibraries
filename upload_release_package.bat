@echo off

if not defined GITHUB_TOKEN (
    echo ERROR: GITHUB_TOKEN environment variable not set.
    echo GITHUB_TOKEN=%GITHUB_TOKEN%
    echo Generate one here: https://github.com/settings/tokens
    echo Scope needed: public_repo 
    exit /b 1
)

:: ==========================================================
:: Upload the latest ZLibraries_Release_(YYYY-MM-DD).7z to GitHub Releases
::
:: Requires:
::   - GITHUB_TOKEN environment variable (setx GITHUB_TOKEN your_token)
::   - curl (built into Windows 10+)
::   - repo info configured below
:: ==========================================================

setlocal enabledelayedexpansion

set "OWNER=azvenigo"
set "REPO=ZLibraries"
set "BATCH_FOLDER=%~dp0"
set "BUILD_DIR=%BATCH_FOLDER%build"

echo here now
:: ==========================================================
:: Step 1 — Auto-generate date-based tag (vYYYY-MM-DD)
:: ==========================================================
for /f "tokens=2-4 delims=/ " %%a in ("%date%") do (
    set "MONTH=%%a"
    set "DAY=%%b"
    set "YEAR=%%c"
)
set "DATESTAMP=!YEAR!-!MONTH!-!DAY!"
set "TAG=v!DATESTAMP!"

echo Detected date: !DATESTAMP!
echo Using tag: !TAG!
echo.

:: ==========================================================
:: Step 2 — Find the newest archive to upload
:: ==========================================================
pushd "%BUILD_DIR%" >nul
for /f "delims=" %%F in ('dir /b /o-d "ZLibraries_Release_(*).7z" 2^>nul') do (
    set "ARCHIVE=%%F"
    goto foundfile
)
:foundfile
popd >nul

if not defined ARCHIVE (
    echo ERROR: No archive found matching ZLibraries_Release_^(*^).7z in "%BUILD_DIR%"
    exit /b 1
)

set "ARCHIVE_PATH=%BUILD_DIR%\!ARCHIVE!"
echo Found archive: !ARCHIVE_PATH!
echo.

:: ==========================================================
:: Step 3 — Create or update the GitHub release
:: ==========================================================
echo Creating or updating release %TAG% on GitHub...

curl -s -H "Authorization: token %GITHUB_TOKEN%"  -H "Content-Type: application/json"  --data "{\"tag_name\":\"%TAG%\",\"name\":\"ZLibraries %TAG%\",\"body\":\"Automated release for %TAG%\",\"draft\":false,\"prerelease\":false}"  "https://api.github.com/repos/%OWNER%/%REPO%/releases" > release.json

:: ==========================================================
:: Step 4 — Extract upload URL
:: ==========================================================
echo Extracting upload URL...

for /f "delims=" %%U in ('powershell -NoProfile -Command "(Get-Content release.json | ConvertFrom-Json).upload_url -replace '\{\?name,label\}',''"') do set "UPLOAD_URL=%%U"

if not defined UPLOAD_URL (
    echo ERROR: Could not find upload URL in release.json
    type release.json
    del release.json >nul 2>&1
    exit /b 1
)

echo Upload URL: !UPLOAD_URL!
echo.
:: ==========================================================
:: Step 5 — Upload the .7z file
:: ==========================================================
echo Uploading %ARCHIVE% ...
curl -v -s -H "Authorization: token %GITHUB_TOKEN%" ^
     -H "Content-Type: application/x-7z-compressed" ^
     --data-binary @"%ARCHIVE_PATH%" ^
     "%UPLOAD_URL%?name=%ARCHIVE%" 

if errorlevel 1 (
    echo ERROR: Upload failed.
    del release.json >nul 2>&1
    exit /b 1
)

echo.
echo ? Upload complete!
echo Release: https://github.com/%OWNER%/%REPO%/releases/tag/%TAG%

del release.json >nul 2>&1
endlocal
exit /b 0