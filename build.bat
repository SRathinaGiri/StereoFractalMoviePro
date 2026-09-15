@echo off
setlocal enabledelayedexpansion

set "PATH=D:\Qt\Tools\mingw1310_64\bin;D:\Qt\6.11.0\mingw_64\bin;D:\Qt\Tools\Ninja;D:\Qt\Tools\CMake_64\bin;%PATH%"

echo ============================================
echo Configuring StereoMoviePro CMake Project...
echo ============================================
cmake -S "%~dp0." -B "%~dp0build" -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="D:/Qt/6.11.0/mingw_64" -DCMAKE_CXX_COMPILER="g++.exe" -DCMAKE_C_COMPILER="gcc.exe" -DCMAKE_MAKE_PROGRAM="ninja.exe"
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed!
    exit /b %ERRORLEVEL%
)

echo.
echo ============================================
echo Compiling StereoMoviePro with Ninja...
echo ============================================
ninja -C "%~dp0build"
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Ninja build failed!
    exit /b %ERRORLEVEL%
)

echo.
echo ============================================
echo Deploying Standalone Binaries...
echo ============================================
if not exist "%~dp0dist" mkdir "%~dp0dist"
copy /Y "%~dp0build\StereoMoviePro.exe" "%~dp0dist\StereoMoviePro.exe" >nul
windeployqt --compiler-runtime --release "%~dp0dist\StereoMoviePro.exe"

echo.
echo ============================================
echo Build ^& Deployment Complete!
echo Standalone executable: %~dp0dist\StereoMoviePro.exe
echo ============================================
