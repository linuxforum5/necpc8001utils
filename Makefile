# Simple makefile for utils
CC=gcc
WCC=i686-w64-mingw32-gcc
SRC=src
WBIN=win32
BIN=bin
INSTALL_DIR=~/.local/bin

all: cmt2wav

cmt2wav: $(SRC)/cmt2wav.c
	$(CC) -o $(BIN)/cmt2wav $(SRC)/cmt2wav.c -lm
	$(WCC) -o $(WBIN)/cmt2wav $(SRC)/cmt2wav.c -lm

clean:
	rm -f *~ $(SRC)/*~ 

install:
	cp $(BIN)/* $(INSTALL_DIR)/
