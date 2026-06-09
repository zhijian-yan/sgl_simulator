#!/bin/sh

gcc sgl_simulator.c sgl/*.c -o sgl_simulator $(sdl2-config --cflags --libs)
./sgl_simulator
