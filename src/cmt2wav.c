/***************************************************
 * cmt2wav, 2025.06. Princz László
 * Convert CMT or CMT containde T88 files to Nec PC-8001 wav file
 * Nec PC-8001 tape dataformat / CMT fileformat:
 *  BASIC file:
 *   10 x 0xD3 bytes
 *   6 bytes : 0 closed name for CLOAD command.
 *  MON (SYSTEM) - Thanks for retroabandon https://gitlab.com/retroabandon/pc8001-re
 *   Header block
 *     0x3A
 *     Load address high byte
 *     Load address low byte
 *     Start address high byte
 *     Start address low byte
 *   Data block
 *     0x3A
 *     length (1 byte)
 *     data bytes
 *     checksum byte
 *   End block
 *     0x3A
 *     0x00
 *     0x00
 * Nec PC-8001 tape format (600 baud)
 * - silence
 * - leading signal = many short (2400Hz) wave
 * - data bytes
 *     - byte format:
 *        Zero (space) bit: 0
 *        0. bit of byte
 *        1. bit of byte
 *        ...
 *        7. bit of byte
 *        One (mark) bit: 1
 *     - Bit format
 *       - 0 : 2 tick length low, 2 tick length high, 2 tick length low, 2 tick length high
 *       - 1 : 1 tick length low, 1 tick length high, 1 tick length low, 1 tick length high, 1 tick length low, 1 tick length high, 1 tick length low, 1 tick length high
 *     - 1 tick length = 1/4800 s
 *     - bit length = 8 tick
 ****************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "getopt.h"
#include "math.h"

#define VM 0
#define VS 2
#define VB 'b'

#define WAV_SAMPLE_RATE 44100
#define HIGH 255
#define LOW 0
#define SILENCE 128

bool verbose = false;
bool t88_mode = false;
bool all = false; // Ha hamis, akkor csak az adatblokk tartalma kerül ki

void write_samples( FILE *fout, unsigned char value, int counter ) {
    for( int i=0; i<counter; i++ ) {
        fputc( value, fout );
    }
}

void write_silence( FILE *fout, int counter ) {
    for( int i=0; i<counter; i++ ) {
        fputc( SILENCE, fout );
    }
}

#define TICK 10

void write_bit( FILE* fout, int bit ) {
    if ( bit ) { // S4
        write_samples( fout, HIGH, TICK );
        write_samples( fout, LOW, TICK );
        write_samples( fout, HIGH, TICK );
        write_samples( fout, LOW, TICK );
        write_samples( fout, HIGH, TICK );
        write_samples( fout, LOW, TICK );
        write_samples( fout, HIGH, TICK );
        write_samples( fout, LOW, TICK );
    } else { // L2
        write_samples( fout, HIGH, 2*TICK );
        write_samples( fout, LOW, 2*TICK );
        write_samples( fout, HIGH, 2*TICK );
        write_samples( fout, LOW, 2*TICK );
    }
}

void write_leading( FILE *fout, int counter ) {
    for( int i=0; i<counter; i++ ) {
        write_bit( fout, 1 );
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Nec PC 8001 functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int b_cnt = 0;
unsigned char first_byte = 0;
unsigned char CLOAD_NAME[ 7 ] = {0};
uint16_t MON_START = 0;
#define BASIC 0xD3
#define MON 0x3A
void info( unsigned char b ) {
    if ( b_cnt == 0 ) {
        first_byte = b; // D3=BASIC, 3A=MON
        if ( first_byte != BASIC && first_byte != MON ) {
            fprintf( stderr, "Invalid CMT file, unknown first byte: 0x%02X\n", b );
            exit(1);
        }
    }
    if ( b_cnt < 64 ) {
        if ( first_byte == BASIC && ( b_cnt > 9 && b_cnt < 16 ) ) CLOAD_NAME[ b_cnt-10 ] = b;
        if ( first_byte == MON && ( b_cnt > 0 && b_cnt < 3 ) ) MON_START = 256 * MON_START + b;
        if ( verbose ) fprintf( stdout, "0x%02X ", b );
    } else if ( b_cnt == 64 ) {
        if ( verbose ) fprintf( stdout, "\n" );
        if ( first_byte == BASIC ) fprintf( stdout, "  CLOAD \"%s\" then RUN\n", CLOAD_NAME );
        if ( first_byte == MON ) fprintf( stdout, "  MON[Ret] L[Ret] G%0X[Ret]\n", MON_START );
    }
    b_cnt++;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Tick functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned char last_output_level = 0;
/*
void toggle_tick( FILE *fout, int tick_counter, int length ) {
    for( int cnt = 0; cnt < length / tick_counter; cnt++ ) {
        last_output_level = 255 - last_output_level;
        for( int i=0; i<tick_counter * 10; i++ ) fputc( last_output_level, fout );
    }
}
*/

void write_byte( FILE *fout, unsigned char b ) {
    write_bit( fout, 0 );
    for( int i=0; i<8; i++ ) {
        write_bit( fout, b & 1<<i );
    }
    write_bit( fout, 1 );
    write_bit( fout, 1 );
    info( b );
}

void write_byte0( FILE *fout, unsigned char b ) { // 88 tick, 0 + 8 bit + 1 + 1
    // fprintf( stdout, "HEX: 0x%02X\n", b );
    write_bit( fout, 0 ); // 1 bit 8 tick
    for( int i=0; i<8; i++ ) {
        write_bit( fout, b & 1<<i );
    }
    write_bit( fout, 1 );
    write_bit( fout, 1 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// T88 functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool check_string0( FILE *fin, const char* prefix ) {
    size_t prefix_len = strlen(prefix);
    unsigned char buffer[ 256 ];  // ideiglenes puffer
    if ( prefix_len >= sizeof(buffer) ) {
        fprintf( stderr, "Prefix túl hosszú\n");
        exit(1);
    }
    size_t read_bytes = fread( buffer, 1, prefix_len, fin );
    if ( read_bytes < prefix_len ) {
        fprintf( stderr, "A fájl túl rövid\n");
        exit(1);
    }
    if ( memcmp( buffer, prefix, prefix_len ) == 0 ) { // ok
        return !fgetc( fin );
    } else {
        fprintf( stderr, "Hibás prefix\n");
        exit(1);
        return false;
    }
}

uint32_t tick_pos = 0;
// 1 tick = 1/4800 sec
// Space block 1200Hz: egy félhullám hossza 2 tick
// Mark block 2400Hz: egy félhullám hossza 1 tick

uint16_t t88_close_block( FILE *fin, FILE *fout, uint16_t block_data_length, const char* name ) {
    // fprintf( stdout, "*** Close block\n" );
    // fprintf( stdout, "\t%s block\n", name );
    if ( block_data_length ) {
        fprintf( stderr, "Hibás záróblokk, hossza nem 0, hanem: 0x%04X\n", block_data_length );
        exit(1);
    } else {
        fprintf( stdout, "Full length is %d s\n", tick_pos / 4800 );
        return 0;
    }
}

uint16_t t88_version_block( FILE *fin, FILE *fout, uint16_t block_data_length, const char* name ) {
    // fprintf( stdout, "\t%s block\n", name );
    if ( block_data_length == 2 ) {
        unsigned char sub = fgetc( fin );
        unsigned char main = fgetc( fin );
        fprintf( stdout, "  T88 verzio: %d.%d\n", main, sub );
        return 1;
    } else {
        fprintf( stderr, "Hibás verzió blokk, hossza nem 2, hanem: 0x%04X\n", block_data_length );
        exit(1);
    }
}

// empty=0, space=2, mark=3 block
uint16_t t88_typed_block( FILE *fin, FILE *fout, uint16_t block_data_length, unsigned char type, const char* name ) {
    // fprintf( stdout, "\t%s block\n", name );
    if ( block_data_length >= 8 ) {
        uint32_t tick_start = fgetc( fin ) + 256 * fgetc( fin ) + 256 * 256 * fgetc( fin ) + 256 * 256 * 256 * fgetc( fin );
        uint32_t tick_length = fgetc( fin ) + 256 * fgetc( fin ) + 256 * 256 * fgetc( fin ) + 256 * 256 * 256 * fgetc( fin );
        if ( tick_pos != tick_start ) {
            fprintf( stderr, "Tick start error\n" );
            exit(1);
        }
        tick_pos += tick_length;
        // fprintf( stdout, "\t\t%d + %d tick\n", tick_start, tick_length );
        if ( type == 1 ) { // DATA block
            uint16_t payload_byte_counter = fgetc( fin ) + 256 * fgetc( fin );
            uint16_t payload_data_type = fgetc( fin ) + 256 * fgetc( fin );
            if ( payload_byte_counter > 32768 ) {
                fprintf( stderr, "Paylod byte counter too big: %d\n", payload_byte_counter );
                exit(1);
            }
            long baud = 52800 / ( tick_length / payload_byte_counter );
            // fprintf( stdout, "Counted speed: %d baud\n", baud );
            uint16_t BD = ( payload_data_type >> 8 ) & 1; // 0 = 600 baud, 1 = 1200 baud
            uint16_t stopBitLength = ( payload_data_type >> 6 ) & 3; // [ 0,3 ]
            uint16_t EP = ( payload_data_type >> 5 ) & 1; // Paritás megadása
            uint16_t PEN = ( payload_data_type >> 4 ) & 1; // Paritás engedélyezése
            uint16_t dataBitLength = ( payload_data_type >> 2 ) & 3; // Adatbit hossza
            // fprintf( stdout, "T: %d baud\n", BD ? 1200 : 600 );
            // fprintf( stdout, "T: %d stop bit\n", stopBitLength );
            // fprintf( stdout, "T: EP=%d\n", EP );
            // fprintf( stdout, "T: PEN=%d\n", PEN );
            // fprintf( stdout, "T: %d data bit\n", dataBitLength );
            if ( tick_length % payload_byte_counter ) {
                fprintf( stderr, "Hibás data tick length\n" );
                exit(1);
            }
            // fprintf( stdout, "\t%d tick / byte\n", tick_length / payload_byte_counter );
            for( int i=0; i<payload_byte_counter; i++ ) write_byte( fout, fgetc( fin ) );
        } else if ( block_data_length > 8 ) {
            fprintf( stderr, "Hibás typed ( %d ) blokk, hossza nem 8, hanem: 0x%04X (%d). Pos: 0x%04X\n", type, block_data_length, block_data_length, ftell( fin ) );
            exit(1);
        } else if ( type == 2 ) { // 0, 1200Hz, 2 tick hosszú
            if ( tick_length % 2 ) {
                fprintf( stderr, "Hibás space length, nem páros szám: %d.\n", tick_length );
                exit(1);
            } else {
//                if ( all ) toggle_tick( fout, 2, tick_length );
            }
        } else if ( type == 3 ) { // 0, 2400Hz, 1 tick hosszú
//            if ( all ) toggle_tick( fout, 1, tick_length );
        }
    } else if ( type == 0 && block_data_length == 2 ) {
        fgetc( fin );
        fgetc( fin );
    } else {
        fprintf( stderr, "Hibás typed ( %d ) blokk, hossza nem 8, hanem: 0x%04X (%d). Pos: 0x%04X\n", type, block_data_length, block_data_length, ftell( fin ) );
        exit(1);
    }
    return 1;
}

int block_counter = 0;

int copy_block( FILE *fin, FILE *fout ) { // return block size
    unsigned long start_pos = ftell( fin );
    uint16_t ID = fgetc( fin ) + 256 * fgetc( fin );
    uint16_t block_data_length = fgetc( fin ) + 256 * fgetc( fin );
    // fprintf( stdout, "%d. block id: 0x%04X, start 0x%04X data length = %d\n", ++block_counter, ID, start_pos, block_data_length );
    switch( ID ) {
        case 0x0000 : return t88_close_block( fin, fout, block_data_length, "Close" ); break;
        case 0x0001 : return t88_version_block( fin, fout, block_data_length, "Version" ); break;
        case 0x0100 : return t88_typed_block( fin, fout, block_data_length, 0, "Empty" ); break; // EMPTY
        case 0x0101 : return t88_typed_block( fin, fout, block_data_length, 1, "Data" ); break; // DATA
        case 0x0102 : return t88_typed_block( fin, fout, block_data_length, 2, "Space" ); break; // SPACE
        case 0x0103 : return t88_typed_block( fin, fout, block_data_length, 3, "Mark" ); break; // MARK
        default:
            fprintf( stderr, "Ismeretlen blokk típus: 0x%04X\n", ID );
            exit(1);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Main functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void convert( FILE *fin, FILE *fout ) { // return sample counter
    fseek( fin, 0L, SEEK_END );
    unsigned long data_size_0 = ftell( fin );
    fseek( fin, 0L, SEEK_SET );
    write_silence( fout, 1024 );
    write_leading( fout, 1024 );
    if ( t88_mode ) {
        if ( check_string0( fin, "PC-8801 Tape Image(T88)" ) ) { // Ok, T88
            while( ftell( fin ) < data_size_0 ) copy_block( fin, fout );
        } else {
            fprintf( stderr, "Nem T88 fájl\n" );
            exit( 1 );
        }
    } else {
        for( int i = 0; i<data_size_0; i++ ) {
            write_byte( fout, fgetc( fin ) );
        }
        fprintf( stdout, "%d bytes in cmt file converted\n", data_size_0 );
    }
}

/**
 * Wav header size
 */
void wavheaderout( FILE *f, int numsamp ) {
    fprintf( f, "%s", "RIFF" );         //     Chunk ID - konstans, 4 byte hosszú, értéke 0x52494646, ASCII kódban "RIFF"
    int totlen = 44 + numsamp;
    fputc( (totlen & 0xff), f );  //     Chunk Size - 4 byte hosszú, a fájlméretet tartalmazza bájtokban a fejléccel együtt, értéke 0x01D61A72 (decimálisan 30808690, vag
    fputc( (totlen >> 8) & 0xff, f );
    fputc( (totlen >> 16) & 0xff, f );
    fputc( (totlen >> 24) & 0xff, f );
    int srate = WAV_SAMPLE_RATE;
    fprintf( f, "%s", "WAVE" );         //     Format - konstans, 4 byte hosszú,értéke 0x57415645, ASCII kódban "WAVE"
    fprintf( f, "%s", "fmt " );         //     SubChunk1 ID - konstans, 4 byte hosszú, értéke 0x666D7420, ASCII kódban "fmt "
    fprintf( f, "%c%c%c%c", 0x10, 0, 0, 0 );  //     SubChunk1 Size - 4 byte hosszú, a fejléc méretét tartalmazza, esetünkben 0x00000010
    fprintf( f, "%c%c", 0x1, 0x0 );     //     Audio Format - 2 byte hosszú, PCM esetében 0x0001
    fprintf( f, "%c%c", 0x1, 0x0 );     //     Num Channels - 2 byte hosszú, csatornák számát tartalmazza, esetünkben 0x0002
    fprintf( f, "%c%c%c%c",srate&255, srate>>8, 0, 0 ); //     Sample Rate - 4 byte hosszú, mintavételezési frekvenciát tartalmazza, esetünkben 0x00007D00 (decimálisan 32000)
    fprintf( f, "%c%c%c%c",srate&255, srate>>8, 0, 0 ); //     Byte Rate - 4 byte hosszú, értéke 0x0000FA00 (decmálisan 64000)
    fprintf( f, "%c%c", 0x01, 0 ); //	Bytes Per Sample: 1=8 bit Mono, 2=8 bit Stereo or 16 bit Mono, 4=16 bit Stereo
    fprintf( f, "%c%c", 0x08, 0 ); //	//     Bits Per Sample - 2 byte hosszú, felbontást tartalmazza bitekben, értéke 0x0008
    fprintf( f, "%s", "data");     //     Sub Chunk2 ID - konstans, 4 byte hosszú, értéke 0x64617461, ASCII kódban "data"
    fprintf( f, "%c%c%c%c", numsamp &0xff, (numsamp >>8)&0xff, (numsamp >>16)&0xff, (numsamp >>24)&0xff ); //	Length Of Data To Follow : Sub Chunk2 Size - 4 byte hosszú, az adatblokk méretét tartalmazza bájtokban, értéke 0x01D61A1E
}

void print_usage() {
    printf( "cmt2wav v%d.%d%c (build: %s)\n", VM, VS, VB, __DATE__ );
    printf( "Cmt to Nec PC-8001 wav file converter.\n");
    printf( "Copyright 2025 by László Princz\n");
    printf( "Usage:\n");
    printf( "cmt2wav <input_filename> [<output_filename_without_wav_extension>]\n" );
    printf( "Convert cmt or cmt contained t88 file to %d sampled 8 bit mono wav format.\n", WAV_SAMPLE_RATE );
    printf( "Command line option:\n");
    printf( "-v          : set verbose mode\n" );
    printf( "-h          : prints this text\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    bool finished = false;
    int argd = argc;

    while ( !finished ) {
        switch ( getopt ( argc, argv, "?hvc:" ) ) {
            case -1:
            case ':':
                finished = true;
                break;
            case '?':
            case 'h':
                print_usage();
                break;
/*            case 'c' :
                if ( !sscanf( optarg, "%i", &afterLineZeroCounter ) ) {
                    fprintf( stderr, "Error parsing argument for '-c'.\n");
                    exit(2);
                }
                break;*/
            case 'v':
                verbose = true;
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
        strcpy( outname, inname ); // The default output name is input filename + .wav
        if ( argc - optind > 1 ) strcpy( outname, argv[ optind + 1 ] );
        if ( argc - optind > 2 ) print_usage();
        strcat( outname, ".wav" );
        if ( fin = fopen( inname, "rb" ) ) {
            if ( fout = fopen( outname, "wb" ) ) {
                t88_mode = ( inname[ strlen( inname )-1 ] == '8' ) && ( inname[ strlen( inname )-2 ] == '8' );
                fprintf( stdout, "Start conversion from '%s' to '%s'\n", inname, outname );
                if ( t88_mode ) {
                    fprintf( stdout, "  Unpack CMT data blocks from T88 file.\n" );
                }
                wavheaderout( fout, 0 );
                convert( fin, fout );
                unsigned long sample_counter = ftell( fout ) - 44;
                fseek( fout, 0, SEEK_SET );
                wavheaderout( fout, sample_counter );
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
