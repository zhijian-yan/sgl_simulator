#!/bin/sh

gcc demo.c sgl/*.c -o demo $(sdl2-config --cflags --libs)
./demo
