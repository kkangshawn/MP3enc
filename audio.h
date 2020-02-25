/**
 * @file		audio.h
 * @version		0.6
 * @brief		header for audio.c
 * @date		Feb 25, 2020
 * @author		Siwon Kang (kkangshawn@gmail.com)
 */

#ifndef AUDIO_H_
#define AUDIO_H_

#include "main.h"

#ifdef _WIN32
#include <stdint.h>
#endif

#define MAX_U_32_NUM    0xFFFFFFFF

/**
 * @brief	Constant values for parsing wave header
 * @see		parse_wave_header()
 */
static int const WAV_ID_RIFF = 0x52494646;  /* "RIFF" */
static int const WAV_ID_WAVE = 0x57415645;  /* "WAVE" */
static int const WAV_ID_FMT = 0x666d7420;   /* "fmt " */
static int const WAV_ID_DATA = 0x64617461;  /* "data" */
#ifndef WAVE_FORMAT_PCM
static short const WAVE_FORMAT_PCM = 0x0001;
#endif
#ifndef WAVE_FORMAT_IEEE_FLOAT
static short const WAVE_FORMAT_IEEE_FLOAT = 0x0003;
#endif
#ifndef WAVE_FORMAT_EXTENSIBLE
static short const WAVE_FORMAT_EXTENSIBLE = 0xFFFE;
#endif

int   init_infile(lame_t gfp, char const *inPath, const int num_file);
FILE *init_outfile(const char *outFile);
void  close_infile(int num_file);
void *lame_encoder_loop(void *data);

#endif /* AUDIO_H_ */
