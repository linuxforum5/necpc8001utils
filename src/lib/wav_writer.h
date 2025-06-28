/**
 * Create WAV file
 */
#ifndef WAV_WRITEWR_H_INCLUDED
#define WAV_WRITEWR_H_INCLUDED

#include <stdio.h>

#define WAV_SAMPLE_RATE 44100

void wav_set_verbose( unsigned int v );
void wav_write_byte( FILE *wav, int value );
void wav_write_samples( FILE *wav, int value, unsigned int sample_counter );
void wav_close( FILE *outfile );
void wav_init( FILE *wavfile );

#endif /* WAV_WRITEWR_H_INCLUDE */
