/**
 * @file		main.h
 * @version		0.5
 * @brief		MP3enc header
 * @date		Aug 17, 2016
 * @author		Siwon Kang (kkangshawn@gmail.com)
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

#if !defined (_WIN32)
#include <dirent.h>
#else
#include <Windows.h>
#include <tchar.h>

#pragma warning(disable:4996)

/* uncomment this if a system does not have struct timespec */
#define HAVE_STRUCT_TIMESPEC

#endif

#include <pthread.h>
#include "lame.h"
#include "audio.h"

#define VERSION "0.5"

#ifndef PATH_MAX
#ifdef MAX_PATH
#define PATH_MAX MAX_PATH
#else
#define PATH_MAX 1024
#endif
#endif
#ifndef NAME_MAX
#define NAME_MAX 255
#endif

#define DIRENT_TYPE_DIRECTORY    4
#define DIRENT_TYPE_FILE         8

/**
 * @enum	quality_mode
 * @brief	enum for quality level option set
 * @see		init_file()
 * @see		parseopt()
 */
enum quality_mode {
	QL_SET = (1 << 4),
	QL_MODE_FAST,
	QL_MODE_STANDARD,
	QL_MODE_BEST,
};

/**
 * @typedef	th_param_t
 * @brief	thread parameter structure to be passed as a pthread argument
 * @see		lame_encoder_loop()
 */
typedef struct th_param {
	lame_global_flags *gf;
	FILE *outf;
	char *inPath;
	char *outPath;
	int nFile;
	unsigned int bVerbose;
} th_param_t;

/**
 * @typedef	opt_set_t
 * @brief	option set structure given from the application arguments
 * @see		init_file()
 * @see		parseopt()
 * @see		get_filelist()
 */
typedef struct opt_set {
	char *szSrcfile;
	char *szDstfile;
	unsigned int bRecursion;
	int nQualityLevel;
	unsigned int bVerbose;
} opt_set_t;

#endif /* MAIN_H_ */
