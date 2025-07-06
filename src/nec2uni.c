/**
 * Convert Nec PC-8001 charset to unicode text file
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "inc/uni2.c"

void nec2uni( FILE *fin, FILE *fout ) {
    for( unsigned int c = fgetc( fin ); !feof( fin ); c = fgetc( fin ) ) {
        unsigned int baseIndex = c * 4;
        for( unsigned char p = 0; p <= codes[ baseIndex + 3 ]; p++ ) {
            fputc( codes[ baseIndex + p ], fout );
        }
    }
}

int main(int argc, char* argv[]) {
    nec2uni( stdin, stdout );
    return 0;
}
