#!/bin/bash
gcc -I/Library/Frameworks/SDL.framework/Headers blocks.c SDLmain.m -framework SDL -framework Cocoa -o blocks