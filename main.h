/*
 * main.h
 *
 *  Created on: Jul 23, 2016
 *      Author: shawn
 */

#ifndef MAIN_H_
#define MAIN_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <assert.h>
#include <dirent.h>
#include <pthread.h>

#include "lame.h"
#include "audio.h"

#define VERSION "0.1"
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
#ifndef NAME_MAX
#define NAME_MAX 255
#endif

#define MAX_U_32_NUM    0xFFFFFFFF

static int const WAV_ID_RIFF = 0x52494646; /* "RIFF" */
static int const WAV_ID_WAVE = 0x57415645; /* "WAVE" */
static int const WAV_ID_FMT = 0x666d7420; /* "fmt " */
static int const WAV_ID_DATA = 0x64617461; /* "data" */

#ifndef WAVE_FORMAT_PCM
static short const WAVE_FORMAT_PCM = 0x0001;
#endif
#ifndef WAVE_FORMAT_IEEE_FLOAT
static short const WAVE_FORMAT_IEEE_FLOAT = 0x0003;
#endif
#ifndef WAVE_FORMAT_EXTENSIBLE
static short const WAVE_FORMAT_EXTENSIBLE = 0xFFFE;
#endif

typedef struct th_param {
	lame_global_flags *gf;
	FILE *outf;
	char *inPath;
	char *outPath;
	int nFile;
} th_param_t;


#endif /* MAIN_H_ */