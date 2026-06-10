@echo off

gcc sgl_simulator.c sgl\*.c ^
    -Isdl2\include ^
    -Lsdl2\lib ^
    -lmingw32 ^
    -lSDL2 ^
    -mwindows ^
    -o sgl_simulator.exe

if errorlevel 1 (
    pause
    exit /b %errorlevel%
)

start "" sgl_simulator.exe
