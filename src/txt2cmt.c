/**
 * Based on NBasicBin2Text.cpp : ÉRÉìÉ\Å[Éã ÉAÉvÉäÉPÅ[ÉVÉáÉìópÇÃÉGÉìÉgÉä É|ÉCÉìÉgÇÃíËã`
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
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "getopt.h"
#include <math.h>

#define VM 0
#define VS 1
#define VB 'b'

bool verbose = false;
bool debug = false;
bool autoLineNumberIfNotExist = false;
bool show_extra_spaces = false;

/* helyettes√≠t≈ë stricmp */
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

unsigned char last_keyword[20] = {0};

unsigned char * keywordsBase[126] = {
	"END",
	"FOR",
	"NEXT",
	"DATA",
	"INPUT",
	"DIM",
	"READ",
	"LET",
	"GOTO",       // LN THEN
	"RUN",        // LN
	"IF",
	"RESTORE",    // LN
	"GOSUB",      // LN
	"RETURN",
	"REM",

	"STOP",
	"PRINT",
	"CLEAR",
	"LIST",       // LN
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
	"LLIST",      // LN
	"CONSOLE",

	"WIDTH",
	"ELSE",       // LN
	"TRON",
	"TROFF",
	"SWAP",
	"ERASE",
	"ERROR",
	"RESUME",
	"DELETE",     // LN
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
	"???,???",            // 0xD6
	"TO",
	"THEN",        // LN
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
	"'",                  // 0xE4
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

unsigned char * keywordsFF[45] = {
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

unsigned char lineBuffer[1024] = {0}; // Tokenised line max length is 255

unsigned char searchKeyword( int *pos, unsigned char ** keywords, int cnt ) {
// fprintf( stdout, "Pos: %d, Search in string: '%s'\n", *pos, lineBuffer+*pos );
    unsigned char token = 0;
    last_keyword[0] = 0;
// fprintf( stdout, "Sum %d keywords\n", cnt );
    for( int i=0; i<cnt && !token; i++ ) {
        int keywordLen = strlen( keywords[ i ] );
// fprintf( stdout, "check index: %d keyword: '%s' len=%d\n", i, keywords[ i ], keywordLen );
        int j=0;
        while ( j<keywordLen && ( keywords[ i ][ j ] == lineBuffer[ (*pos) + j ] ) ) {
// fprintf( stdout, "%d. keyword %d. char: '%c', String %d. char: '%c'\n", i, j, keywords[ i ][ j ], (*pos) + j, lineBuffer[ (*pos) + j ] );
            last_keyword[ j ] = keywords[ i ][ j ];
            j++;
            last_keyword[ j ] = 0;
        }
        if ( j == keywordLen ) { // Teljes egyez√©s a kulcssz√≥ra
            token = i + 0x81;
            *pos += keywordLen;
        }
    }
// fprintf( stdout, "Found token: %d, Pos: %d, word: '%s'\n", token, *pos, last_keyword );
//exit(1);
    return token;
}

int nextAutoLineNumber = 10;
int autoLineNumberIncrease = 10;

void ltrim( int *pos ) { while ( lineBuffer[ *pos ] && ( ( lineBuffer[ *pos ] == 32 ) || ( lineBuffer[ *pos ] == 9 ) ) ) (*pos)++; } // drop leading spaces
void rtrim( int *pos ) { while ( lineBuffer[ *pos ] && ( ( lineBuffer[ *pos ] == 32 ) || ( lineBuffer[ *pos ] == 9 ) ) ) (*pos)++; } // drop postfix spaces

uint16_t readLineNumber( int *pos ) {
    uint16_t num = 0;
    ltrim( pos );
    while ( lineBuffer[ *pos ] && ( lineBuffer[ *pos ] >= '0' ) && ( lineBuffer[ *pos ] <= '9' ) ) {
        // fprintf( stdout, "Pos: %d chr=%c num=%d\n", *pos, lineBuffer[ *pos ], num );
        num = num * 10 + lineBuffer[ (*pos)++ ] - '0'; // Read number
    }
    rtrim( pos );
    return num;
}

void convertLineNumberList( int *pos, FILE *fout ) { // Sorsz√°mlista
    if ( debug ) fprintf( stdout, "Start line number list conversion\n" );
    bool inList = true;
    if ( debug ) fprintf( stdout, "LNL=" );
    if ( show_extra_spaces ) fputc( ' ', fout );
    while ( inList ) {
        uint16_t lineNumber = readLineNumber( pos );
        if ( lineNumber ) {
            fputc( 0x0E, fout );
            fputc( lineNumber % 256, fout );
            fputc( lineNumber / 256, fout );
            if ( debug ) fprintf( stdout, "%d", lineNumber );
            if ( inList = lineBuffer[ *pos ] == ',' ) {
                (*pos)++;
                fputc( ',', fout );
                if ( debug ) fprintf( stdout, "," );
            }
        } else {
            inList = false;
        }
    }
    if ( show_extra_spaces ) fputc( ' ', fout );
    if ( debug ) fprintf( stdout, "\n" );
}

unsigned int readBigInteger( unsigned int pos, uint64_t *num ) { // √ñsszef√ºgg≈ë sz√°msor beolvas√°sa
    unsigned int pos2 = pos;
    ltrim( &pos2 );
    *num = 0;
    unsigned int pos3 = pos2;
    while ( lineBuffer[ pos3 ] && lineBuffer[ pos3 ] >= '0' && lineBuffer[ pos3 ] <= '9' ) *num = *num * 10 + lineBuffer[ pos3++ ] - '0'; // Read number
// fprintf( stdout, "Big pos %d,%d,%d val=%d\n", pos, pos2, pos3, *num );
    if ( pos3 > pos2 ) { // Valid numbers
        return pos3-pos;
    } else {
        return 0;
    }
}

unsigned int readInteger( unsigned int pos, uint32_t *num ) {
    uint64_t val = 0;
    unsigned int dpos = readBigInteger( pos, &val );
    *num = val;
    return dpos;
fprintf( stdout, "Found big integer: %d in pos %d. Length: %d\n", val, pos, dpos );
    if ( dpos ) {
        if ( val >= INT16_MIN && val <= INT16_MAX ) { // Valid integer
            unsigned int pos2 = pos + dpos;
            if ( lineBuffer[ pos2 ] != '.' && 
                 lineBuffer[ pos2 ] != 'E' && 
                 lineBuffer[ pos2 ] != 'D' &&
                 lineBuffer[ pos2 ] != '!' &&
                 lineBuffer[ pos2 ] != '#' ) {
                uint32_t cnum = val;
                *num = cnum;
            } else {
                dpos = 0;
            }
        } else { // Too big or too small integer
            dpos = 0;
        }
    }
    return dpos;
}

void writeUInt8( FILE *fout, uint8_t ui8 ) {
    fputc( 0x0F, fout );
    fputc( ui8, fout );
}

void writeUInt16( FILE *fout, uint16_t ui16 ) {
    fputc( 0x1C, fout );
    fputc( ui16 % 256, fout );
    fputc( ui16 / 256, fout );
}
/*
void split_float( float value, uint32_t *_int_part, uint32_t *_frac_part, int8_t *_exponent ) {
    // Sz√©tbontjuk 10-es norm√°l alakra: mantissa * 10^exponent
    int exponent = 0;
    float mantissa = value; // abs

    if ( value != 0.0) {
        exponent = (int)floor(log10(abs_num));
        mantissa = abs_num / pow(10, exponent);
    }
    if ( exponent < INT8_MIN_VALUE || exponent > INT8_MAX_VALUE ) {
        fprintf( stderr, "T√∫lm√©retes kitev≈ë" );exit(1);
    }

    // integer part √©s fractional part a mantiss√°ra
    float int_part, frac_part;
    frac_part = modf( mantissa, &int_part );

    _int_part = int_part;
    _fract_part = frac_part;
    _exponent = exponent;
}
*/
/**
 * Single precision fixed pointig storage format
 * 3 bytes fact part 0.xxxxxx
 * 1 byte exponent 2^(n-0x81)
 * 3.14E-06 : 1D CA B8 52 6E
 *     1E-6 : 1D BC 37 06 6D
 *  .1 1E-1 : 1D CD CC 4C 7D 11001101 11001100 01001100 -4
 * .01 1E-2 : 1D 0A D7 23 7A
 *     1E-3 : 1D 6E 12 03 77
 */
void writeFixed4( float abs_val, FILE *fout ) {
    fputc( 0x1D, fout );
    if ( abs_val == 0.0 ) { // Write 0.0
        fputc( 0, fout );
        fputc( 0, fout );
        fputc( 0, fout );
        fputc( 0, fout );
    } else { // sisu = e, r = exp_val
        uint16_t e = 0x81;
        float exp_val = 1; // 2^(e-0x81)
        if ( abs_val < exp_val ) { // < 1
            while( abs_val < exp_val ) {
                exp_val /= 2.0;
                e--;
            }
        } else { // abs_val >= 1
            while( abs_val >= exp_val ) {
                exp_val *= 2.0;
                e++;
            }
            exp_val /= 2.0;
            e--;
        }
        // 2*exp_val > abs_val >= exp_val && exp_val == 2^e
        uint32_t bits = 0;
        for( int i=0; i<24; i++ ) {
            bits *= 2;
            if ( abs_val >= exp_val ) {
                bits += 1;
                abs_val -= exp_val;
            }
            exp_val /= 2.0;
        }
        fputc( bits & 0xFF, fout );
        fputc( (bits >> 8) & 0xFF, fout );
        fputc( (bits >> 16) & 0x7F, fout );
        fputc( e & 0xFF, fout );
    }
}

/**
 * Double precision float pointig storage format
 * 7 bytes fractional part
 * 1 byte exponent
 *        2.3D5 1F 00 00 00 00 00 9C 60 92        0x81+17
 * 7654321.1234 1F EA 48 2E 3F 62 97 69 97        0x81+22
 */
void writeDouble8( double abs_val, FILE *fout ) {
    fputc( 0x1F, fout );
    if ( abs_val == 0.0 ) { // Write 0.0
        fputc( 0, fout );
        fputc( 0, fout );
        fputc( 0, fout );
        fputc( 0, fout );
        fputc( 0, fout );
        fputc( 0, fout );
        fputc( 0, fout );
        fputc( 0, fout );
    } else { // sisu = e, r = exp_val
        uint16_t e = 0x81;
        double exp_val = 1; // 2^(e-0x81)
        if ( abs_val < exp_val ) { // < 1
            while( abs_val < exp_val ) {
                exp_val /= 2.0;
                e--;
            }
        } else { // abs_val >= 1
            while( abs_val >= exp_val ) {
                exp_val *= 2.0;
                e++;
            }
            exp_val /= 2.0;
            e--;
        }
        // 2*exp_val > abs_val >= exp_val && exp_val == 2^e
        uint64_t bits = 0;
        for( int i=0; i<56; i++ ) {
            bits *= 2;
            if ( abs_val >= exp_val ) {
                bits += 1;
                abs_val -= exp_val;
            }
            exp_val /= 2.0;
        }
        fputc( bits & 0xFF, fout );
        fputc( (bits >> 8) & 0xFF, fout );
        fputc( (bits >> 16) & 0xFF, fout );
        fputc( (bits >> 24) & 0xFF, fout );
        fputc( (bits >> 32) & 0xFF, fout );
        fputc( (bits >> 40) & 0xFF, fout );
        fputc( (bits >> 48) & 0x7F, fout );
        fputc( e & 0xFF, fout );
    }
}

bool isOct( unsigned char c ) { return ( c >= '0' && c <='7' ); }

bool isHex( unsigned char c ) { return ( c >= '0' && c <='9' ) || ( c >= 'A' && c <='F' ) || ( c >= 'a' && c <='f' ); }

uint8_t octToDec( unsigned char c ) {
    if ( c >= '0' && c <='7' ) {
        return c-'0';
    } else {
        fprintf( stderr, "Invalid octal number: %c\n", c );
        exit(1);
    }
}

uint8_t hexToDec( unsigned char c ) {
    if ( c >= '0' && c <='9' ) {
        return c-'0';
    } else if ( c >= 'A' && c <='F' ) {
        return c-'A' + 10;
    } else if ( c >= 'a' && c <='f' ) {
        return c-'a' + 10;
    } else {
        fprintf( stderr, "Invalid hex number: %c\n", c );
        exit(1);
    }
}

void readDouble( unsigned int *pos, uint32_t egesz, double *num, bool *float_p ) {
    unsigned int dpos = 0;
    int64_t tizedes = 0;
    int64_t kitevo = 0;
    // fprintf( stdout, "	read double: %.8f, float: '%c' (%d.%d)\n", *num, ((*float_p) ? 'I' : 'N'), egesz, tizedes );
    // fprintf( stdout, "	read double: %0.8f, (%d . %d)\n", *num, egesz, tizedes );
    if ( lineBuffer[ *pos ] == '.' ) {
        (*pos)++;
        dpos = readBigInteger( *pos, &tizedes );
        if ( !dpos ) tizedes = 0;
        *pos += dpos;
    }
    *float_p = ( lineBuffer[ *pos ] == 'D' );
    if ( ( lineBuffer[ *pos ] == 'D' ) || ( lineBuffer[ *pos ] == 'E' ) ) {
        (*pos)++;
        // *float_p = false;
        bool minus = false;
        if ( lineBuffer[ *pos ] == '-' ) {
            minus = true;
            (*pos)++;
        }
        dpos = readBigInteger( *pos, &kitevo );
        if ( !dpos ) kitevo = 0;
        *pos += dpos;
        if ( minus ) kitevo = -kitevo;
    }
    if ( lineBuffer[ *pos ] == '!' ) {
        *float_p = false;
        *pos += dpos;
    } else if ( lineBuffer[ *pos ] == '#' ) {
        *float_p = true;
        *pos += dpos;
    }
    *num = 0;
    while( tizedes > 0 ) {
        *num += tizedes % 10;
        tizedes /= 10;
        *num /= 10.0;
    }
    if ( egesz < 0 ) {
        *num = (double)egesz - *num;
    } else {
        *num = (double)egesz + *num;
    }
    while( kitevo ) {
        if ( kitevo < 0 ) {
            *num /= 10.0;
            kitevo++;
        } else {
            *num *= 10.0;
            kitevo--;
        }
    }
    // fprintf( stdout, "	read double: %.8f, float: '%c' (%d.%d)\n", *num, (*float_p) ? 'I' : 'N', egesz, tizedes );
}

void readAndWriteFixed4Double8( unsigned int *pos, FILE *fout, uint32_t egesz ) { // . D E
    bool float_p = false;
    double dbl8 = 0;
    readDouble( pos, egesz, &dbl8, &float_p );
//fprintf( stdout, "Double = %.8f\n", dbl8 );
    if ( float_p ) { // Double precision, <=17 digits, D, ., #
        writeDouble8( dbl8, fout );
    } else { // Single precision,  <=7 digits, E, ., !
        writeFixed4( dbl8, fout );
    }
}

// 0xE2 0x96 
// ‚ñÅ‚ñÇ‚ñÉ‚ñÑ‚ñÖ‚ñÜ‚ñá‚ñà‚ñè‚ñé‚ñç‚ñå‚ñã‚ñä‚ñâ‚îº‚î¥‚î¨‚î§‚îú ‚îÄ‚îÇ ‚îå‚îê‚îî‚îò    "

unsigned int writeCharFromBuffer( unsigned int pos, FILE *fout, uint16_t lineNumber ) {
    unsigned char c = lineBuffer[ pos++ ];
    if ( c == 0xE2 ) {
        unsigned char c1 = lineBuffer[ pos++ ];
        unsigned char c2 = lineBuffer[ pos++ ];
        if ( ( c1 == 0x94 ) ) { // Unicode first char
            if      ( c2 == 0x82 ) fputc( 150, fout ); // '‚îÇ'
            else if ( c2 == 0x80 ) fputc( 149, fout ); // '‚îÄ'
            else if ( c2 == 0x82 ) fputc( 'X', fout ); // 
            else if ( c2 == 0x94 ) fputc( 154, fout ); // '‚îî'
            else if ( c2 == 0x98 ) fputc( 155, fout ); // '‚îò'
            else if ( c2 == 0x8C ) fputc( 152, fout ); // '‚îå'
            else if ( c2 == 0x90 ) fputc( 153, fout ); // '‚îê'
            else {
                fputc( '?', fout );
                fprintf( stdout, "Invalid unicode chars: 0x%02X 0x%02X 0x%02X line %d pos %d\n", c, c1, c2, lineNumber, pos-4 );
            }
        } else {
            fputc( '?', fout );
            fprintf( stdout, "Invalid unicode chars: 0x%02X 0x%02X 0x%02X line %d pos %d\n", c, c1, c2, lineNumber, pos-4 );
        }
    } else if ( c == 0xC2 ) {
        unsigned char c1 = lineBuffer[ pos++ ];
        if ( c1 == 0xB7 ) fputc( 165, fout ); // '¬∑'
        else {
            fputc( '?', fout );
            fprintf( stdout, "Invalid unicode chars: 0x%02X 0x%02X line %d pos %d\n", c, c1, lineNumber, pos-2 );
        }
    } else if ( ( c < 0x20 ) || ( c > 0x7E ) ) {
        fputc( '?', fout );
        if ( debug ) fprintf( stderr, "Invalid character: 0x%02X line %d pos %d '%s'\n", c, lineNumber, pos-1, lineBuffer + pos-1 );
    } else {
        fputc( c, fout );
    }
    return pos;
}

void convertLineBuffer( FILE *fout ) {
    int len = strlen( lineBuffer );
    unsigned int pos = 0; // A k√∂vetkez≈ë, m√©g fel nem dolgozott karakterre mutat
    bool remOrDataMode = false;
    bool quoteMode = false;
    bool variableNameMode = false;
    unsigned char token = 0;
    int8_t i8 = 0;
    int16_t i16 = 0;
    uint16_t ui16 = 0;
    uint32_t ui32 = 0;
    unsigned int dpos = 0;
    bool firstTokenFound = false;
    if ( len ) {
        // if ( verbose ) fprintf( stdout, "Converted line length: %d\n", len );
        uint16_t lineNumber = readLineNumber( &pos );
        if ( !lineNumber ) {
            if ( autoLineNumberIfNotExist ) lineNumber = nextAutoLineNumber;
        }
        nextAutoLineNumber = lineNumber + autoLineNumberIncrease;
        if ( lineNumber ) {
            if ( verbose ) fprintf( stdout, "Converted line number: %d\n", lineNumber );
            uint16_t RAMADDR = 0x8000; // 0x8046 - 0x10 + ftell( fout );
            fputc( RAMADDR % 256, fout );
            fputc( RAMADDR / 256, fout );
            fputc( lineNumber % 256, fout );
            fputc( lineNumber / 256, fout );
            // if ( verbose ) fprintf( stdout, "Current line: %d\n", lineNumber );
            while( lineBuffer[pos] ) {
// fprintf( stdout, "Pos: %d Char=0x%02X, last key='%s'\n", pos, lineBuffer[pos], last_keyword );
                if ( lineBuffer[pos] == 0x0D ) { // Skip
                    pos++;
                    variableNameMode = false;
                } else if ( quoteMode ) {
                    if ( lineBuffer[pos] == '"' ) {
                        quoteMode = false;
// printf( "--- quote mode off line %d pos %d\n", lineNumber, pos );
                        fputc( lineBuffer[pos++], fout );
                    } else {
                        pos = writeCharFromBuffer( pos, fout, lineNumber );
                    }
                } else if ( remOrDataMode ) {
                    if ( lineBuffer[pos] == ':' ) {
                        remOrDataMode = false;
                        firstTokenFound = false;
                    }
                    pos = writeCharFromBuffer( pos, fout, lineNumber );
                } else if ( lineBuffer[pos] == '"' ) {
                    quoteMode = true;
// printf( "+++ quote mode on line %d pos %d\n", lineNumber, pos );
                    fputc( lineBuffer[pos++], fout );
                    variableNameMode = false;
                } else if ( lineBuffer[pos] == '&' && lineBuffer[pos+1] == 'H' && isHex( lineBuffer[pos+2] ) ) { // Hex constant
                    fputc( 0x0C, fout );
                    pos+=2;
                    uint16_t hexVal = 0;
                    while( isHex( lineBuffer[pos] ) ) hexVal = 16 * hexVal + hexToDec( lineBuffer[pos++] );
                    fputc( hexVal%256, fout );
                    fputc( hexVal/256, fout );
                    variableNameMode = false;
                } else if ( lineBuffer[pos] == '&' && isOct( lineBuffer[pos+1] ) ) { // Octal constant
                    fputc( 0x0B, fout );
                    pos++;
                    uint16_t octVal = 0;
                    while( isHex( lineBuffer[pos] ) ) octVal = 8 * octVal + octToDec( lineBuffer[pos++] );
                    fputc( octVal%256, fout );
                    fputc( octVal/256, fout );
                    variableNameMode = false;
                } else if ( ( lineBuffer[ pos ] == '.' ) || ( (!variableNameMode) && ( dpos = readInteger( pos, &ui32 ) ) ) ) {
// fprintf( stdout, "Found integer: %d in pos %d. Length: %d, next char: '%c'\n", ui32, pos, dpos, lineBuffer[ pos+dpos ] );
                    pos += dpos;
                    switch( lineBuffer[ pos ] ) {
                        case '!' : pos++; writeFixed4( (float)ui32, fout ); break;
                        case '#' : pos++; writeDouble8( (float)ui32, fout ); break;
                        case '%' : pos++; writeUInt16( fout, ui32 ); break;
                        case 'E' : readAndWriteFixed4Double8( &pos, fout, ui32 ); break;
                        case 'D' : readAndWriteFixed4Double8( &pos, fout, ui32 ); break;
                        case '.' : readAndWriteFixed4Double8( &pos, fout, ui32 ); break;
                        default: // Val√≥di integer
                            if ( ui32 <= 9 ) {
                                fputc( 0x11 + ui32, fout );
                            } else if ( ui32 <= 255 ) {
                                writeUInt8( fout, ui32 );
                            } else if ( ui32 <= 65535 ) {
                                writeUInt16( fout, ui32 );
                            } else {
                                fprintf( stderr, "T√∫l nagy integer konstans: %d\n", ui32 );
                                exit(1);
                            }
                    }
                } else if ( token = searchKeyword( &pos, keywordsBase, sizeof(keywordsBase)/sizeof( keywordsBase[0] ) ) ) {
// fprintf( stdout, "Found token: 0x%02X, '%s' New pos=%d\n", token, last_keyword, pos );
//exit(1);
                    if ( token == 0x84 ) remOrDataMode = true; // DATA
                    if ( token == 0x8F ) remOrDataMode = true; // REM
                    if ( token == 0xE4 ) {
                        fputc( 0x3A, fout );
                        fputc( 0x8F, fout );
                        remOrDataMode = true; // ' == REM
                    }
                    if ( show_extra_spaces ) fputc( ' ', fout ); // Token prefix space 
                    if ( token == 0xA1 ) fputc( 0x3A, fout ); // : ELSE el√©
                    fputc( token, fout );
                    firstTokenFound = true;
                    variableNameMode = false;
                    if (  !strcmp( last_keyword, "THEN" )
                       || !strcmp( last_keyword, "GOTO" )
                       || !strcmp( last_keyword, "GOSUB" )
                       || !strcmp( last_keyword, "RUN" )
                       || !strcmp( last_keyword, "DELETE" )
                       || !strcmp( last_keyword, "ELSE" )
                       || !strcmp( last_keyword, "LIST" )
                       || !strcmp( last_keyword, "LLIST" )
                       || !strcmp( last_keyword, "RESTORE" ) ) {
                        convertLineNumberList( &pos, fout ); // Sorsz√°mlista
                    }
                } else if ( token = searchKeyword( &pos, keywordsFF, sizeof( keywordsFF )/sizeof( keywordsFF[0] ) ) ) {
// fprintf( stdout, "Found token FF: 0x%02X, '%s' New pos=%d\n", token, last_keyword, pos );
                    fputc( 0xFF, fout );
                    fputc( token, fout );
                    firstTokenFound = true;
                    variableNameMode = false;
                } else {
                    unsigned char c = lineBuffer[pos++];
                    if ( (c<=' ')||(c==',')||(c=='(')||(c==')')||(c==':')||(c==';')||(c=='@')||(c=='$') ) { // validStorableCharacter( lineBuffer[pos++] ) ) { // ",", ";", "(", ")", " "
                        variableNameMode = false;
                        if ( c >= ' ' ) fputc( c, fout );
                    } else if ( !variableNameMode ) {
                        variableNameMode = ( c>='A' ) && ( c<='Z' );
                        if ( !variableNameMode ) {
                            fprintf( stderr, "Invalid first character in varibalename: 0x%02X (%c)\n", c, c );
                            exit(1);
                        } else {
                            fputc( c, fout );
                        }
                    } else { // variableNameMode
                        if ( ( c>='A' ) && ( c<='Z' ) ) {
                            fputc( c, fout );
                        } else if ( ( c>='0' ) && ( c<='9' ) ) {
                            fputc( c, fout );
                        } else if ( ( c=='$' ) || ( c=='%' ) || ( c=='!' ) || ( c=='#' ) ) {
                            fputc( c, fout );
                            variableNameMode = true;
                        } else {
                            fprintf( stderr, "Invalid character in varibalename: 0x%02X (%c)\n", c, c );
                            exit(1);
                            }
                        }
                    }
                }
                fputc( 0, fout ); // End of basic line terminator
            } else {
                if ( verbose ) fprintf( stdout, "Skip %d line: %s\n", pos, lineBuffer );
            }
    }
}

void convert( FILE *fin, FILE *fout );

void includeNewSourceFile( unsigned char *filename, FILE *fout ) {
    FILE *inc = fopen( filename, "r" );
    if ( verbose ) fprintf( stdout, "Include source from %s\n", filename );
    if ( inc ) {
        convert( inc, fout );
        fclose( inc );
    } else {
        fprintf( stderr, "Include file '%s' not found!", filename );
        exit(1);
    }
}

#define INCLUDE_CMD "#include "

void convert( FILE *fin, FILE *fout ) {
    // Soronk√©nt beolvassuk
    int incLen = strlen( INCLUDE_CMD );
    while ( fgets( lineBuffer, sizeof( lineBuffer ), fin) != NULL) {
        if ( lineBuffer[ strlen( lineBuffer ) - 1 ] == '\n' ) lineBuffer[ strlen( lineBuffer ) - 1 ] = 0;
        if ( !strncmp( lineBuffer, INCLUDE_CMD, incLen ) ) {
            includeNewSourceFile( lineBuffer + incLen , fout );
        } else {
            convertLineBuffer( fout );
        }
    }
}

void cmtWriteHeader( FILE *fout, char* cloadname ) {
    for( int i=0; i<10; i++ ) fputc( 0xD3, fout );
    for( int i=0; i<6; i++ ) fputc( cloadname[i], fout );
}

void cmtWriteLastLine( FILE *fout ) {
    for( int i=0; i<12; i++ ) fputc( 0, fout );
}

void print_usage() {
    printf( "txt2cmt v%d.%d%c (build: %s)\n", VM, VS, VB, __DATE__ );
    printf( "Text to Nec PC-8001 CMT file converter.\n");
    printf( "Copyright 2025 by L√°szl√≥ Princz\n");
    printf( "Usage:\n");
    printf( "txt2cmt <input_filename> [<output_filename_without_wav_extension>]\n" );
    printf( "Convert txt file to Nec PC-8001 CMT format.\n" );
    printf( "Command line option:\n");
    printf( "-v          : set verbose mode\n" );
    printf( "-d          : enable debug info\n" );
    printf( "-n name     : name for CLOAD. Max 6 characters. The default name is 'PC'.\n" );
    printf( "-b          : binary bas output instead of CMT (drop the first 16 byte.)\n" );
    printf( "-a          : auto line number if not found\n" );
    printf( "-h          : prints this text\n");
    exit(1);
}

int main( int argc, char *argv[] ) {
    bool finished = false;
    int argd = argc;
    char cloadname[7] = {0};
    strcpy( cloadname, "PC" ); // Default tape name for CLOAD
    bool dropCmtHeader = false;

    while ( !finished ) {
        switch ( getopt ( argc, argv, "?hvbdan:" ) ) {
            case -1:
            case ':':
                finished = true;
                break;
            case '?':
            case 'h':
                print_usage();
                break;
            case 'b':
                dropCmtHeader = true;
                break;
            case 'n' :
                if ( strlen(optarg) > 6 ) optarg[6]=0;
                strcpy( cloadname, optarg );
                break;
            case 'v':
                verbose = true;
                break;
            case 'd':
                debug = true;
                break;
            default:
                break;
        }
    }
    if ( argc - optind > 0 ) {
        char inname[ 80 ]; // Input filename
        char outname[ 80 ]; // Output filename
        FILE *fin,*fout;
        strcpy( inname, argv[ optind ] );
        strcpy( outname, inname ); // The default output name is input filename + .cmt or bas
        if ( argc - optind > 1 ) strcpy( outname, argv[ optind + 1 ] );
        if ( argc - optind > 2 ) print_usage();
        strcat( outname, dropCmtHeader ? ".bas" : ".cmt" );
        if ( fin = fopen( inname, "r" ) ) {
            if ( fout = fopen( outname, "wb" ) ) {
                fprintf( stdout, "Start conversion from '%s' to '%s'\n", inname, outname );
                if ( !dropCmtHeader ) cmtWriteHeader( fout, cloadname );
                convert( fin, fout );
                cmtWriteLastLine( fout );
                fclose( fout );
                fclose( fin );
            } else {
                fprintf( stderr, "Error creating %s.\n", outname );
                exit(4);
            }
        } else {
            fprintf( stderr, "Error opening %s.\n", inname );
            exit(4);
        }
    } else {
        print_usage();
    }
}
