/**
 * Based on NBasicBin2Text.cpp : ƒRƒ“ƒ\[ƒ‹ ƒAƒvƒŠƒP[ƒVƒ‡ƒ“—p‚ÌƒGƒ“ƒgƒŠ ƒ|ƒCƒ“ƒg‚Ì’è‹`
 * BASIC format
 * bas file = Line*
 *
 * Line format
 * - header link: 0x00 0x00 for last line
 *    - low byte
 *    - high byte
 * - Line number
 *    - low byte
 *    - high byte
 * - (octalConstant|hexConstant|absAddr|lineNumber|singleByteConstant|oneDigitConstant|twoByteInteger|fourByteFloat|eightByteFloat|quote|rem|keyword|dataByte)
 * - End of line: 0x00
 *
 * octalConstant:      0x0B low high
 * hexConstant:        0x0C low high
 * absAddr: !warning!  0x0D byte byte
 * lineNumber:         0x0E low high
 * singleByteConstant: 0x0F byte
 * oneDigitConstant:   [0x11-0x1A] // value = byte-0x11
 * twoByteInteger:     0x1C low high
 * fourByteFloat:      0x1D v1 v2 v3 v4
 * eightByteFloat:     0x1F v1 v2 v3 v4 v5 v6 v7 v8
 * quote:
 *   0x84 [ 0x20 - 0x7E ]* 0x84
 * rem:
 *   0x8F [^0x00]*
 * keyword:            [ 0x81 - 0xFD ] // keywors[ token - 0x81 ]
 * keywordFF:          0xFF tokenFfByte
 *
 * dataByte:           byte
 *
 * tokenFfByte:        [ 0x81 - 0xFD ] // keyworsFF[ token - 0x81 ]
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

/* helyettesÃ­tÅ‘ itoa */
void itoa(int value, char *str, int base) {
    sprintf(str, (base == 8) ? "%o" :
                  (base == 10) ? "%d" :
                  (base == 16) ? "%x" : "%d", value);
}

/* helyettesÃ­tÅ‘ stricmp */
int stricmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        char c1 = tolower(*s1);
        char c2 = tolower(*s2);
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
    }
    return tolower(*s1) - tolower(*s2);
}

char * keywordsBase[126] = {
	"END",
	"FOR",
	"NEXT",
	"DATA",
	"INPUT",
	"DIM",
	"READ",
	"LET",
	"GOTO",
	"RUN",
	"IF",
	"RESTORE",
	"GOSUB",
	"RETURN",
	"REM",

	"STOP",
	"PRINT",
	"CLEAR",
	"LIST",
	"NEW",
	"ON",
	"WAIT",
	"DEF",
	"POKE",
	"CONT",
	"CSAVE",
	"CLOAD",
	"OUT",
	"LPRINT",
	"LLIST",
	"CONSOLE",

	"WIDTH",
	"ELSE",
	"TRON",
	"TROFF",
	"SWAP",
	"ERASE",
	"ERROR",
	"RESUME",
	"DELETE",
	"AUTO",
	"RENUM",
	"DEFSTR",
	"DEFINT",
	"DEFSNG",
	"DEFDBL",
	"LINE",

	"PRESET",
	"PSET",
	"BEEP",
	"FORMAT",
	"KEY",
	"COLOR",
	"TERM",
	"MON",
	"CMD",
	"MOTOR",
	"POLL",
	"RBYTE",
	"WBYTE",
	"ISET",
	"IRESET",
	"TALK",

	"MAT",
	"LISTEN",
	"DSKO$",
	"REMOVE",
	"MOUNT",
	"OPEN",
	"FIELD",
	"GET",
	"PUT",
	"SET",
	"CLOSE",
	"LOAD",
	"MERGE",
	"FILES",
	"NAME",
	"KILL",

	"LSET",
	"RSET",
	"SAVE",
	"LFILES",
	"INIT",
	"LOCATE",
	"???[0xd6]???",
	"TO",
	"THEN",
	"TAB(",
	"STEP",
	"USR",
	"FN",
	"SPC(",
	"NOT",
	"ERL",

	"ERR",
	"STRING$",
	"USING",
	"INSTR",
	"'",
	"VARPTR",
	"CSRLIN",
	"ATTR$",
	"DSKI$",
	"INKEY$",
	"TIME$",
	"DATE$",
	"???[0xec]???",
	"SQR",
	"STATUS",
	"POINT",

	">",
	"=",
	"<",
	"+",
	"-",
	"*",
	"/",
	"^",
	"AND",
	"OR",
	"XOR",
	"EQV",
	"IMP",
	"MOD",
	"\\"
};

char * keywordsFF[45] = {
	"LEFT$",
	"RIGHT$",
	"MID$",
	"SGN",
	"INT",
	"ABS",
	"SQR",
	"RND",
	"SIN",
	"LOG",
	"EXP",
	"COS",
	"TAN",
	"ATN",
	"FRE",
	"INP",
	"POS",
	"LEN",
	"STR$",
	"VAL",
	"ASC",
	"CHR$",
	"PEEK",
	"SPACE$",
	"OCT$",
	"HEX$",
	"LPOS",
	"PORT",
	"DEC",
	"BCD$",
	"CINT",
	"CSNG",
	"CDBL",
	"FIX",
	"CVI",
	"CVS",
	"CVD",
	"DSKF",
	"EOF",
	"LOC",
	"LOF",
	"FPOS",
	"MKI$",
	"MKS$",
	"MKD$"
};

void usage() {
	puts("N-BASIC Binary or CMT to Text converter");
	puts("usage; nbas2txt [-p] [-c] INPUTFILE >OUTPUTFILE");
};

int main(int argc, char* argv[]) {
	if( argc < 2 && argc > 4 ) {
		usage();
		return 2;
	}

	bool bPretty = false;
	bool skipCmtPrefix = false;
	const char * filename = NULL;
	for( int i=1; i<argc-1; i++ ) {
		if( stricmp(argv[ i ],"-p") == 0 ) {
			bPretty = true;
		} else if( stricmp(argv[ i ],"-c") == 0 ) {
			skipCmtPrefix = true;
		} else {
			usage();
			return 2;
		}
	}
	filename = argv[ argc - 1 ];

	FILE * fp = fopen( filename, "rb" );
	if( fp == NULL ) {
		perror(filename);
		return 2;
	}

	if ( skipCmtPrefix ) {
            // fprintf( stderr, "SKIP CMT PREFIX\n" );
            for( int i=0; i<16; i++ ) fgetc(fp);
        }
	while( true ) {
		// get line header
		int linkLow = fgetc(fp);
		if( linkLow == EOF ) {
			fputs("Encounted unexpected EOF", stderr);
			fclose(fp);
			return 2;
		}
		int linkHigh = fgetc(fp);
		if( linkLow == 0 && linkHigh == 0 ) break;

		int lineNumberLow = fgetc(fp);
		int lineNumberHigh = fgetc(fp);
		int lineNumber = (lineNumberLow & 0xff) + ((lineNumberHigh & 0xff) << 8);
		printf("%d ",lineNumber);

		bool remOrDataMode = false;
		bool quoteMode = false;
		// process a line
		while( true ) {
			int ch = fgetc(fp);
			if( ch == EOF ) {
				fputs("Encounted unexpected EOF", stderr);
				fclose(fp);
				return 2;
			}
			ch = ch & 0xff;	// must be unsigned value
			if( ch == 0 ) {	// end of line
				printf("\n");
				break;
			} else if( ch == 0x0b ) {	// octal constatnt
				int vl = fgetc(fp);
				int vh = fgetc(fp);
				int val = (vl & 0xff) + ((vh & 0xff) << 8);
				char buf[16];
				// itoa( val, buf, 8 );
				snprintf( buf, sizeof(buf), "%o", val );
				printf("&O%s",(char*)buf);
			} else if( ch == 0x0c ) {	// hexa constant
				int vl = fgetc(fp);
				int vh = fgetc(fp);
				int val = (vl & 0xff) + ((vh & 0xff) << 8);
				char buf[16];
				// itoa( val, buf, 16 );
				snprintf( buf, sizeof(buf), "%x", val );
				printf("&H%s",(char*)buf);
			} else if( ch == 0x0d ) {	// absolute address replaced from line number
				fputs("Warning: Encounted Absolute Address (0x0d)", stderr);
				fgetc(fp);
				fgetc(fp);
			} else if( ch == 0x0e ) {	// line number
				int vl = fgetc(fp);
				int vh = fgetc(fp);
				int val = (vl & 0xff) + ((vh & 0xff) << 8);
				printf("%d",val);
			} else if( ch == 0x0f ) {	// single byte constant
				int val = (fgetc(fp) & 0xff);
				printf("%d",val);
			} else if( ch >= 0x11 && ch <= 0x1a ) {	// one digit constant
				int val = ch - 0x11;
				printf("%d",val);
			} else if( ch == 0x1c ) {	// two byte integer
				int vl = fgetc(fp);
				int vh = fgetc(fp);
				int val = (vl & 0xff) + ((vh & 0xff) << 8);
				if( (val & 0x8000 ) != 0 ) {
					val |= 0xffff0000;
				}
				printf("%d",val);
			} else if( ch == 0x1d ) {	// four byte float
				int v1 = fgetc(fp);
				int v2 = fgetc(fp);
				int v3 = fgetc(fp);
				int v4 = fgetc(fp);
				int kasu = (v1 & 0xff) + ((v2 & 0xff) << 8) + ((v3 & 0x7f) << 16);
				int sisu = v4 - 0x81;
				float r = 1;
				float d = 0.5;
				int mask = 0x400000;
				for( int i=0; i<23; i++ ) {
					if( (kasu & mask) != 0 ) r = r + d;
					mask >>= 1;
					d /= 2.0;
				}
				if( sisu == 0 ) {
					r = (float)0.0;
				} else if( sisu < 0 ) {
					for( int i=0; i<abs(sisu); i++ ) r /= 2.0;
				} else {
					for( int i=0; i<sisu; i++ ) r *= 2.0;
				}
				printf( "%f", r );
			} else if( ch == 0x1f ) {	// eight byte float
				uint64_t kasu = 0;
				for( int i=0; i<7; i++ ) kasu += (uint64_t)fgetc(fp) << i*8;
				int sisu = fgetc(fp) - 0x81;
				double r = 1;
				double d = 0.5;
				uint64_t mask = 0x40000000000000;
				for( int i=0; i<55; i++ ) {
					if ( (kasu & mask) != 0 ) r = r + d;
					mask >>= 1;
					d /= 2.0;
				}
				if( sisu == 0 ) {
					r = 0.0;
				} else if( sisu < 0 ) {
					for( int i=0; i<abs(sisu); i++ ) r /= 2.0;
				} else {
					for( int i=0; i<sisu; i++ ) r *= 2.0;
				}
				printf( "%f", r );
			} else if( ch >= 0x81 && ch <= 0xfe && quoteMode == false && remOrDataMode == false ) {
				printf(" %s ",keywordsBase[ch-0x81]);
				if( ch == 0x8f ) {	// REM
					remOrDataMode = true;
				}
				if( ch == 0x84 ) {	// DATA
					remOrDataMode = true;
				}
			} else if( ch == 0xff && quoteMode == false && remOrDataMode == false ) {
				int ch2 = (fgetc(fp) & 0xff);
				if( ch2 >= 0x81 && ch2 <= 0xfd ) {
					printf(" %s ",keywordsFF[ch2-0x81]);
				} else if( ch2 == 0xec ) {
					printf(" IEEE ");
				}
			} else if( ch >= 0x20 && ch <= 0x7e ) {
				if( ch == '"' ) {
					if( quoteMode ) {
						quoteMode = false;
					} else {
						quoteMode = true;
					}
					putchar( ch );
				} else if( quoteMode == false && ch == ':' ) {
					remOrDataMode = false;
					if( bPretty ) {
						printf("\n\t");
					} else {
						putchar( ch );
					}
				} else {
					putchar( ch );
				}
			} else if( ch >= 0xa1 && ch <= 0xdf ) {
				putchar( ch );
			} else {
				printf("???[0x%02x]???",ch);
			}
		}
	}

	return 0;
}

// end of NBasic2Text.cpp
