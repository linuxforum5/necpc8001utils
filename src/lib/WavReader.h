#ifndef WAV_READER_H
#define WAV_READER_H
/**
 * Usage:
 *
 * #include <stdio.h>
 * #include "WavReader.h"
 * 
 * int main() {
 *     WavReader *reader = WavReader_open( "sample.wav" ); // Objektum létrehozása
 *
 *     unsigned long sampleCounter = reader->readSampleCounter( reader );
 *     for( unsigned long i = 0; i < sampleCounter; i++ ) {
 *         int sample = reader->readNextSample( reader ); // Read next sample
 *     }
 *     WavReader_close( reader ); // Destructor, memória felszabadítása
 *     return 0;
 * }
 *
 */

#include <stdbool.h>

/* WAV file header structure */
typedef struct wavHeader {
    char           riff[ 4 ];       // 4 bytes
    unsigned int   rLen;            // 4 bytes
    char           WAVE[ 4 ];       // 4 bytes
    char           fmt[ 4 ];        // 4 bytes
    unsigned int   fLen;            /* 0x1020 */
    unsigned short wFormatTag;      /* 0x0001 */
    unsigned short nChannels;       /* 0x0001 */
    unsigned int   nSamplesPerSec;
    unsigned int   nAvgBytesPerSec; // nSamplesPerSec*nChannels*(nBitsPerSample%8)
    unsigned short nBlockAlign;     /* 0x0001 */
    unsigned short nBitsPerSample;  /* 0x0008 */
    char           datastr[ 4 ];    // 4 bytes
    unsigned int   data_size;       // 4 bytes
} wavHeader;/* = {
    'R','I','F','F', //     Chunk ID - konstans, 4 byte hosszú, értéke 0x52494646, ASCII kódban "RIFF"
    0,               //     Chunk Size - 4 byte hosszú, a fájlméretet tartalmazza bájtokban a fejléccel együtt, értéke 0x01D61A72
    'W','A','V','E', //     Format - konstans, 4 byte hosszú,értéke 0x57415645, ASCII kódban "WAVE"
    'f','m','t',' ', //     SubChunk1 ID - konstans, 4 byte hosszú, értéke 0x666D7420, ASCII kódban "fmt "
    16,              //     SubChunk1 Size - 4 byte hosszú, a fejléc méretét tartalmazza, esetünkben 0x00000010
    1,               //     Audio Format - 2 byte hosszú, PCM esetében 0x0001
    1,               //     Num Channels - 2 byte hosszú, csatornák számát tartalmazza, esetünkben 0x0002
    0              , //     Sample Rate - 4 byte hosszú, mintavételezési frekvenciát tartalmazza, esetünkben 0x00007D00 (decimálisan 32000)
    0              , //     Byte Rate - 4 byte hosszú, értéke 0x0000FA00 (decmálisan 64000)
    1,               //     Block Align - 2 byte hosszú, az 1 mintában található bájtok számát tartalmazza - 0x0002
    8,               //     Bits Per Sample - 2 byte hosszú, felbontást tartalmazza bitekben, értéke 0x0008
    'd','a','t','a', //     Sub Chunk2 ID - konstans, 4 byte hosszú, értéke 0x64617461, ASCII kódban "data"
    0                //     Sub Chunk2 Size - 4 byte hosszú, az adatblokk méretét tartalmazza bájtokban, értéke 0x01D61A1E
};*/

typedef struct WavReader { // "WavReader" osztály
    // Private area
    FILE *file_descriptor;
    struct wavHeader header; // Privát wavHeader típusú változó

    // Public area
    u_int32_t ( *getSampleCounter )( struct WavReader *self );
    int ( *readNextSample )( struct WavReader *self, u_int16_t channel_index0 );
    unsigned char ( *readNextSample8 )( struct WavReader *self, u_int16_t channel_index0 );
    int ( *readSampleRate )( struct WavReader *self );
    void ( *checkSampleRate )( struct WavReader *self, u_int32_t sampleRate );
    bool ( *eof )( struct WavReader *self );
    unsigned long ( *ftell )( struct WavReader *self );
    void ( *showInfo )( struct WavReader *self, FILE* f );
} WavReader;

WavReader *WavReader_open( const char *filename ); // Konstruktor (create a new WavReader object )
void WavReader_close( WavReader *c ); // Destruktor ( WavReader object freeing )

#endif // WAV_READER_H
