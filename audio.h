/*
 * audio.h
 *
 *  Created on: Jul 29, 2016
 *      Author: shawn
 */

#ifndef AUDIO_H_
#define AUDIO_H_

#include "main.h"

int init_infile(lame_t gfp, char const *inPath, int nFile);
FILE * init_outfile(const char *outFile);
void close_infile(int nFile);
void * lame_encoder_loop(void *data);
//int lame_encoder_loop(lame_global_flags * gf, FILE * outf, int nogap, char *inPath, char *outPath, int nFile);


#endif /* AUDIO_H_ */
