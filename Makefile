# Simple makefile for utils
CC=gcc
WCC=i686-w64-mingw32-gcc
SRC=src
WBIN=win32
BIN=bin
INSTALL_DIR=~/.local/bin

all: cmt2wav nbas2txt txt2cmt uni2nec nec2uni

cmt2wav: $(SRC)/cmt2wav.c
	$(CC) -o $(BIN)/cmt2wav $(SRC)/cmt2wav.c -lm
	$(WCC) -o $(WBIN)/cmt2wav $(SRC)/cmt2wav.c -lm

nbas2txt: $(SRC)/nbas2txt.c
	$(CC) -o $(BIN)/nbas2txt $(SRC)/nbas2txt.c -lm
	$(WCC) -o $(WBIN)/nbas2txt $(SRC)/nbas2txt.c -lm

txt2cmt: $(SRC)/txt2cmt.c
	$(CC) -o $(BIN)/txt2cmt $(SRC)/txt2cmt.c -lm
	$(WCC) -o $(WBIN)/txt2cmt $(SRC)/txt2cmt.c -lm

uni2nec: $(SRC)/uni2nec.c
	$(CC) -o $(BIN)/uni2nec $(SRC)/uni2nec.c -lm
	$(WCC) -o $(WBIN)/uni2nec $(SRC)/uni2nec.c -lm

nec2uni: $(SRC)/nec2uni.c
	$(CC) -o $(BIN)/nec2uni $(SRC)/nec2uni.c -lm
	$(WCC) -o $(WBIN)/nec2uni $(SRC)/nec2uni.c -lm

clean:
	rm -f *~ $(SRC)/*~ 

install:
	cp $(BIN)/* $(INSTALL_DIR)/
