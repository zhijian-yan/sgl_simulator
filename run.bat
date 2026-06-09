@echo off

gcc demo.c sgl\*.c ^
    -Isdl2\include ^
    -Lsdl2\lib ^
    -lmingw32 ^
    -lSDL2main ^
    -lSDL2 ^
    -mwindows ^
    -o demo.exe

if errorlevel 1 (
    pause
    exit /b %errorlevel%
)

start "" demo.exe
