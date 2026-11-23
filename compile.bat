@echo off
rem Compile Universal Dataset Manager backend using MSYS2 UCRT64 GCC

set "GCC=C:\\msys64\\ucrt64\\bin\\g++.exe"
set "SRC=backend\\server.cpp"
set "OUT=backend\\server.exe"

if not exist "%GCC%" (
    echo GCC not found at %GCC% && exit /b 1
)

rem Build with static linking of the C++ runtime to avoid missing DLLs
"%GCC%" -std=c++17 -Wall -Wextra -static -static-libgcc -static-libstdc++ -Ibackend "%SRC%" -o "%OUT%" -lws2_32

echo Build finished. Run the server with:
echo   %OUT%
