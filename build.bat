@echo off
REM Initialize MSVC 2022 environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo [ERROR] Failed to initialize MSVC environment
    exit /b 1
)

REM Add MySQL bin to PATH so libmysql.dll can be located at runtime
set "PATH=C:\Program Files\MySQL\MySQL Server 8.0\bin;%PATH%"

REM Add Qt 6.11.0 msvc2022_64 to environment (for reference; project doesn't link Qt)
set "CMAKE_PREFIX_PATH=D:\Qt\6.11.0\msvc2022_64;%CMAKE_PREFIX_PATH%"

REM Ensure CMake and conan are on PATH
set "PATH=D:\cmake\cmake-4.3.1-windows-x86_64\bin;D:\miniconda3;D:\miniconda3\Scripts;%PATH%"

REM Set ASIO include dir from conan cache
if not defined ASIO_INCLUDE_DIR (
    set "ASIO_INCLUDE_DIR=%USERPROFILE%\.conan2\p\asioad7b630cd44e4\p\include"
)

echo ===== Environment =====
echo CXX compiler:
where cl.exe
echo CMake:
where cmake
echo Conan:
where conan
echo ASIO: %ASIO_INCLUDE_DIR%
echo =======================

if not exist build mkdir build
cd build

REM Configure with MSVC
echo.
echo ===== CMake configure =====
cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DMYSQL_INCLUDE_DIR="C:/Program Files/MySQL/MySQL Server 8.0/include" ^
    -DMYSQL_LIB_DIR="C:/Program Files/MySQL/MySQL Server 8.0/lib" ^
    -DASIO_INCLUDE_DIR="%ASIO_INCLUDE_DIR%" ^
    ..
if errorlevel 1 (
    echo [ERROR] CMake configure failed
    exit /b 1
)

REM Build
echo.
echo ===== CMake build =====
cmake --build . --config Release --parallel
if errorlevel 1 (
    echo [ERROR] CMake build failed
    exit /b 1
)

echo.
echo ===== BUILD SUCCESS =====
dir Release\smart_parking.exe