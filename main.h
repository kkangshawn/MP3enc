/**
 * @file		main.h
 * @version		0.6
 * @brief		MP3enc header
 * @date		Feb 25, 2020
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

#define VERSION "0.6"

#ifndef PATH_MAX
#ifdef MAX_PATH
#define PATH_MAX				MAX_PATH
#else
#define PATH_MAX				1024
#endif
#endif
#ifndef NAME_MAX
#define NAME_MAX				255
#endif

#define DIRENT_TYPE_DIRECTORY	4
#define DIRENT_TYPE_FILE		8

/**
 * @enum	quality_mode
 * @brief	enum for quality level option set
 * @param	QL_SET				Setting bit to check duplicated option
 * @param	QL_MODE_FAST		Fast encoding mode
 * @param	QL_MODE_STANDARD	Standard encoding mode, default
 * @param	QL_MODE_BEST		Best encoding mode
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
 * @param	gf					Global flags for lame encoder library
 * @param	in_path				Input file path
 * @param	out_path			Output file path
 * @param	idx_file			Thread index number
 * @param	verbose				Verbose option flag to be used in encoding loop
 * @see		lame_encoder_loop()
 */
typedef struct th_param {
	lame_global_flags *gf;
	FILE *outf;
	char *in_path;
	char *out_path;
	int idx_file;
	char verbose;
} th_param_t;

/**
 * @typedef	opt_set_t
 * @brief	option set structure given from the application arguments
 * @param	srcfile				Name of input file or directory
 * @param	dstfile				Name of output file
 * @param	recursion			Option flag for recursive subdirectory search
 * @param	quality				Quality level
 * @param	verbose				Verbose option flag to be used in encoding loop
 * @see		init_file()
 * @see		parseopt()
 * @see		get_filelist()
 */
typedef struct opt_set {
	char *srcfile;
	char *dstfile;
	unsigned int recursion;
	int quality;
	char verbose;
} opt_set_t;

#endif /* MAIN_H_ */
