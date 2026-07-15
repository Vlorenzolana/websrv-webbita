@echo off
REM Build script - uses Makefile with available make tool
REM Usage: build.bat [target]
REM Default: all

setlocal enabledelayedexpansion

set MAKE_TARGET=%1
if "!MAKE_TARGET!"=="" set MAKE_TARGET=all

echo.
echo ==========================================
echo WebServ Build System
echo ==========================================
echo.
echo Build target: !MAKE_TARGET!
echo.

REM Try to find a make tool
for /f "delims=" %%i in ('where mingw32-make.exe 2^>nul') do set "MAKE_CMD=mingw32-make.exe" && goto found_make
for /f "delims=" %%i in ('where make.exe 2^>nul') do set "MAKE_CMD=make.exe" && goto found_make
for /f "delims=" %%i in ('where nmake.exe 2^>nul') do set "MAKE_CMD=nmake.exe" && goto found_nmake

echo [ERROR] No make tool found!
echo Please install:
echo   - MinGW (mingw32-make.exe)
echo   - MSYS2 (make.exe)
echo   - Visual Studio Build Tools (nmake.exe)
echo.
echo Fallback: compile manually
echo Or use: powershell -File build_powershell.ps1
pause
exit /b 1

:found_make
echo [FOUND] !MAKE_CMD!
echo.
!MAKE_CMD! !MAKE_TARGET!
goto done

:found_nmake
echo [FOUND] nmake (Visual Studio)
echo Note: nmake syntax may differ. Using standard make format.
echo.
nmake !MAKE_TARGET!
goto done

:done
echo.
echo ==========================================
echo Available targets:
echo   build.bat              (default: compile)
echo   build.bat clean        (remove .o files)
echo   build.bat fclean       (remove .o + binary)
echo   build.bat re           (full rebuild)
echo   build.bat help         (show all targets)
echo ==========================================
echo.

endlocal
