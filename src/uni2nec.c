/**
 * Convert unicode text file to Nec PC-8001 charcest
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "inc/uni2.c"

void uni2nec( FILE *fin, FILE *fout ) {
    for( unsigned char c = fgetc( fin ); !feof( fin ); c = fgetc( fin ) ) {
        if ( (c==0xE2) || (c==0xE3) || (c==0xE5) || (c==0xE6) || (c==0xE7) || (c==0xEF) ) {
            unsigned char bytes[3] = { c, fgetc( fin ), fgetc( fin ) };
            unsigned char curPos = 0; // [ 0 .. 3 ]
            bool found = false;
            for( unsigned int i = 0; (!found) && (i<256); i++ ) {
                unsigned char maxPos = codes[ i*4 + 3 ];
                if ( maxPos == 2 ) {
                    found = true;
                    unsigned char curPos = 0;
                    while( ( curPos<=maxPos ) && ( codes[ i*4 + curPos ] == bytes[ curPos ] ) ) curPos++;
                    if ( found = curPos > maxPos ) fputc( i, fout );
                }
            }
            if ( !found ) {
                fprintf( stderr, "Char not found: 0x%02X\n", c );
                exit(1);
            }
        } else {
            if ( ( codes[ c*4 ] == c ) && ( codes[ c*4 + 3 ] == 0 )  ) {
                fputc( c, fout );
            } else {
                fprintf( stderr, "Invalid character code in convesion table: 0x%02X (%d)\n", c, c );
                exit(1);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    uni2nec( stdin, stdout );
    return 0;
}
