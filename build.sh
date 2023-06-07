#!/usr/bin/sh

gcc $(find . -name "*.c") -Iinclude -lSDL2 -lGL -lm -Ofast -s -o inspc
# gcc $(find . -name "*.c") -Iinclude -lSDL2 -lGL -lm -Og -g -o inspc -fsanitize=address,undefined
