/**
 * Convert commodore raw tap to wav
 */
#include <stdio.h>
#include <stdlib.h>
#include "wav_writer.h"

static unsigned char verbose = 0;

void wav_set_verbose( unsigned int v ) { verbose = v; }

/* WAV file header structure */
/* should be 1-byte aligned */
#pragma pack(1)
struct wav_header {
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
} wave = {
    'R','I','F','F', //     Chunk ID - konstans, 4 byte hosszú, értéke 0x52494646, ASCII kódban "RIFF"
    0,               //     Chunk Size - 4 byte hosszú, a fájlméretet tartalmazza bájtokban a fejléccel együtt, értéke 0x01D61A72 (decimálisan 30808690, vagyis a fájl mérete ~30,8 MB)
    'W','A','V','E', //     Format - konstans, 4 byte hosszú,értéke 0x57415645, ASCII kódban "WAVE"
    'f','m','t',' ', //     SubChunk1 ID - konstans, 4 byte hosszú, értéke 0x666D7420, ASCII kódban "fmt "
    16,              //     SubChunk1 Size - 4 byte hosszú, a fejléc méretét tartalmazza, esetünkben 0x00000010
    1,               //     Audio Format - 2 byte hosszú, PCM esetében 0x0001
    1,               //     Num Channels - 2 byte hosszú, csatornák számát tartalmazza, esetünkben 0x0002
    WAV_SAMPLE_RATE, //     Sample Rate - 4 byte hosszú, mintavételezési frekvenciát tartalmazza, esetünkben 0x00007D00 (decimálisan 32000)
    WAV_SAMPLE_RATE, //     Byte Rate - 4 byte hosszú, értéke 0x0000FA00 (decmálisan 64000)
    1,               //     Block Align - 2 byte hosszú, az 1 mintában található bájtok számát tartalmazza - 0x0002
    8,               //     Bits Per Sample - 2 byte hosszú, felbontást tartalmazza bitekben, értéke 0x0008
    'd','a','t','a', //     Sub Chunk2 ID - konstans, 4 byte hosszú, értéke 0x64617461, ASCII kódban "data"
    0                //     Sub Chunk2 Size - 4 byte hosszú, az adatblokk méretét tartalmazza bájtokban, értéke 0x01D61A1E
};
#pragma pack()

void wav_write_byte( FILE *wav, int value ) { fputc( value, wav ); }

void wav_write_samples( FILE *wav, int value, unsigned int sample_counter ) {
    for(int i=0; i<sample_counter; i++) wav_write_byte( wav, value );
}

void wav_close( FILE *outfile ) {
    int full_size = ftell( outfile );
    fseek( outfile, 4, SEEK_SET );
    fwrite( &full_size, sizeof( full_size ), 1, outfile ); // Wave header 2. field : filesize with header. First the lowerest byte

    int data_size = full_size - sizeof( wave );
    fseek( outfile, sizeof( wave ) - 4 ,SEEK_SET ); // data chunk size position
    fwrite( &data_size, sizeof( data_size ), 1, outfile );

    fclose(outfile);
}

void wav_init( FILE *wavfile ) {
    fwrite( &wave, sizeof( wave ), 1, wavfile );
}
