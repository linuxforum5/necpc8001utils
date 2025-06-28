#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "WavReader.h"

u_int32_t WavReader_get_sample_counter( WavReader *self ) { return self->header.data_size / ( self->header.nBitsPerSample / 8 ) / self->header.nChannels; }
int WavReader_read_sample_rate( WavReader *self ) { return self->header.nSamplesPerSec; }
void WavReader_check_sample_rate( WavReader *self, u_int32_t sr ) {
    if ( sr != self->readSampleRate( self ) ) {
        fprintf( stderr, "Sample rate is not valid! %d != %d\n", sr, self->readSampleRate( self ) );
        exit(1);
    }
}

int WavReader_read_next_sample_8( WavReader *self ) { int8_t sample = 0; fread( &sample, sizeof( sample ), 1, self->file_descriptor ); return sample; }
int WavReader_read_next_sample_16( WavReader *self ) { int16_t sample = 0; fread( &sample, sizeof( sample ), 1, self->file_descriptor ); return sample; }
int WavReader_read_next_sample_32( WavReader *self ) { int32_t sample = 0; fread( &sample, sizeof( sample ), 1, self->file_descriptor ); return sample; }

int WavReader_read_next_sample( WavReader *self, u_int16_t channel_index0 ) {
    int samples[ self->header.nChannels ];
    for( u_int16_t chn = 0; chn < self->header.nChannels; chn++ ) {
        switch ( self->header.nBitsPerSample ) {
            case 8 : samples[ chn ] = WavReader_read_next_sample_8( self ); break;
            case 16 : samples[ chn ] = WavReader_read_next_sample_16( self ); break;
            case 32 : samples[ chn ] = WavReader_read_next_sample_32( self ); break;
            default:
                fprintf( stderr, "Invalid bit length per sample: %d\n", self->header.nBitsPerSample );
                exit(1);
        }
    }
    return samples[ channel_index0 ];
}

unsigned char WavReader_read_next_sample8( WavReader *self, u_int16_t channel_index0 ) { // 8 bitesre konvertált érték
    int sample = WavReader_read_next_sample( self, channel_index0 );
    switch ( self->header.nBitsPerSample ) {
        case 8 : return sample; break;
        case 16 : return sample / 256; break;
        case 32 : return sample / 65536 / 256; break;
        default:
            fprintf( stderr, "Invalid bit length per sample: %d\n", self->header.nBitsPerSample );
            exit(1);
    }
}

u_int32_t WavReader_get_length_in_seconds( WavReader *self ) { return self->header.data_size / self->header.nAvgBytesPerSec; }

void WavReader_read_header( WavReader *self ) {
    if ( fread( &self->header, sizeof( wavHeader ), 1, self->file_descriptor ) != 1 ) {
        fprintf( stderr, "Wave header read error: %d\n" );
        exit(1);
    }
    if ( self->header.wFormatTag != 1 ) { fprintf( stderr, "Signed PCM format required!\n" ); exit(1); }
    if ( self->header.nChannels > 2 ) { fprintf( stderr, "Only 1 or 2 channes file accepted!\n" ); exit(1); }
    if ( ( self->header.nBitsPerSample != 8 ) && ( self->header.nBitsPerSample != 16 ) && ( self->header.nBitsPerSample != 32 ) ) {
        fprintf( stderr, "Only 8, 16 or 32 bit per sample accepted!\n" );
        exit(1);
    }
}

void WavReader_show_info( WavReader *self, FILE* f ) {
    fprintf( f, "Sample rate: %d, %d bits per sample. Channels %d\n", self->header.nSamplesPerSec, self->header.nBitsPerSample, self->header.nChannels );
}

bool WavReader_eof( WavReader *self ) { return feof( self->file_descriptor ); }
unsigned long WavReader_ftell( WavReader *self ) { return ftell( self->file_descriptor ); }

// Constructor
WavReader *WavReader_open( const char *filename ) {
    WavReader *c = ( WavReader * )malloc( sizeof( WavReader ) );
    if ( c == NULL ) {
        perror( "Failed to allocate memory" );
        exit(1);
    }
    if ( c->file_descriptor = fopen( filename, "rb" ) ) {
        WavReader_read_header( c );
        c->getSampleCounter = WavReader_get_sample_counter;
        c->readNextSample = WavReader_read_next_sample;
        c->readNextSample8 = WavReader_read_next_sample8;
        c->readSampleRate = WavReader_read_sample_rate;
        c->checkSampleRate = WavReader_check_sample_rate;
        c->eof = WavReader_eof;
        c->ftell = WavReader_ftell;
        c->showInfo = WavReader_show_info;
        return c;
    } else {
        fprintf( stderr, "File cannot be opened: '%s'\n", filename );
        exit(1);
    }
}

// Destructor
void WavReader_close( WavReader *c ) {
    fclose( c->file_descriptor );
    c->file_descriptor = 0;
    free(c);
}
