#!/bin/bash
gcc -o tictactoe main.c \
    -I/opt/homebrew/Cellar/libmicrohttpd/1.0.1/include \
    -L/opt/homebrew/Cellar/libmicrohttpd/1.0.1/lib \
    -lmicrohttpd