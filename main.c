/**
 * @file		main.c
 * @version		0.5
 * @brief		MP3enc main source code
 * @date		Aug 17, 2016
 * @author		Siwon Kang (kkangshawn@gmail.com)
 */

#include "main.h"


/**
 * @brief	Check given argument has 'wav' filename extension
 */
int isWAV(const char *filename)
{
	int len = strlen(filename);
	if (len > 4
			&& filename[len - 4] == '.'
			&& filename[len - 3] == 'w'
			&& filename[len - 2] == 'a'
			&& filename[len - 1] == 'v')
		return 1;
	return 0;
}

/**
 * @brief	Set output file list with given '.wav' filename then change its extension to 'mp3'
 * @param [out]	outlist		An output filename with mp3 extension
 * @param [in]	filename	A name of input wav file
 */
void set_outlist(char outlist[PATH_MAX + 1], const char *filename)
{
	if (isWAV(filename)) {
		int len = strlen(filename);
		strcpy(outlist, filename);
		strcpy(outlist + len - 3, "mp3");
	}
}

/**
 * @brief	Initialize option set
 * @return	Option set
 */
opt_set_t * init_optset()
{
	opt_set_t *optset;
	optset = (opt_set_t *)malloc(sizeof(opt_set_t));

	optset->szSrcfile = NULL;
	optset->szDstfile = NULL;
	optset->bRecursion = 0;
	optset->nQualityLevel = 0;
	optset->bVerbose = 0;

	return optset;
}

/**
 * @brief	Deinitialize option set
 */
void deinit_optset(opt_set_t *param)
{
	if (param) {
		if (param->szSrcfile) {
			free(param->szSrcfile);
			param->szSrcfile = NULL;
		}
		if (param->szDstfile) {
			free(param->szDstfile);
			param->szDstfile = NULL;
		}
		param->bRecursion = 0;
		param->nQualityLevel = 0;
		param->bVerbose = 0;

		free(param);
		param = NULL;
	}
}

/**
 * @brief	Get file list from argument.
 *		Set input file list and output file list.
 *		If the argument is directory, search all the files in it.
 *		Ignore files if the filename extension is not 'wav'
 *		A sub-directory can be searched recursively with option '-r'.
 *		Linux uses dirent, Windows uses WIN32_FIND_DATA.
 * @param [out]	inlist		Input filename list
 * @param [out]	outlist		Output filename list
 * @param [out]	nFiles		Total number of wav files to be encoded
 * @param [in]	param		Option set containing name of input and output and option parameters
 */
#if defined (__linux)
void get_filelist_linux(char inlist[][PATH_MAX + 1], char outlist[][PATH_MAX + 1], int *nFiles, const opt_set_t *param)
{
	DIR *pDir;
	struct dirent *pDirEnt;
	int nArgLength = strlen(param->szSrcfile);
	char szTemp[PATH_MAX];

	if ((pDir = opendir(param->szSrcfile)) != NULL) {
		/* if param->szSrcfile is directory */

		while ((pDirEnt = readdir(pDir)) != NULL) {
			memset(szTemp, 0x0, PATH_MAX);
			strcpy(szTemp, param->szSrcfile);
			if (param->szSrcfile[nArgLength - 1] != '/')
				strcat(szTemp, "/");
			strcat(szTemp, pDirEnt->d_name);

			if (pDirEnt->d_type == DIRENT_TYPE_FILE && isWAV(pDirEnt->d_name)) {
				if (strlen(szTemp) > PATH_MAX) {
					printf("%s is too long. Maximum length is %d\n",
							szTemp, PATH_MAX);

					continue;
				}
				strcpy(inlist[*nFiles], szTemp);

				set_outlist(outlist[*nFiles], inlist[*nFiles]);
				(*nFiles)++;
			}
			/* for recursive(-r) option */
			else if ((pDirEnt->d_type == DIRENT_TYPE_DIRECTORY)
					&& param->bRecursion) {
				/* ignore directory name './' and '../' to prevent infinite loop */
				if (strcmp(pDirEnt->d_name, ".") && strcmp(pDirEnt->d_name, "..")) {
					opt_set_t *subdir_param;
					subdir_param = init_optset();
					subdir_param->szSrcfile = strdup(szTemp);
					subdir_param->bRecursion = 1;

					//printf("[DEBUG] Get into %s\n", subdir_param->szSrcfile);
					get_filelist_linux(inlist, outlist, nFiles, subdir_param);

					deinit_optset(subdir_param);
				}
			}
		}

		closedir(pDir);
	}
	else {
		/* if param->szSrcfile is file */

		if (nArgLength > PATH_MAX)
			fprintf(stderr, "ERROR: %s is too long. Maximum length is %d\n",
					param->szSrcfile, PATH_MAX);
		else {
			strcpy(inlist[*nFiles], param->szSrcfile);
			if (param->szDstfile)
				strcpy(outlist[*nFiles], param->szDstfile);
			else
				set_outlist(outlist[*nFiles], param->szSrcfile);
			(*nFiles)++;
		}
	}
}
#elif defined (_WIN32)
void get_filelist_windows(char inlist[][PATH_MAX + 1], char outlist[][PATH_MAX + 1], int *nFiles, const opt_set_t *param)
{
	WIN32_FIND_DATA ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	TCHAR szDir[PATH_MAX];
	char szTemp[PATH_MAX];
	size_t len = 0;
	DWORD dwError = 0;

	memset(szDir, 0x0, PATH_MAX);
	len = strlen(param->szSrcfile);

	if (len > (PATH_MAX - 3)) {
		fprintf(stderr, "ERROR: The length of file name or directory path is too long.\n"
			"\tMaximum length is %d\n", PATH_MAX);
		return;
	}

#ifdef _UNICODE
	mbstowcs(szDir, param->szSrcfile, len);
#else
	strcpy(szDir, param->szSrcfile);
#endif

	if (len > 1 && szDir[len - 1] == '\\') {
		szDir[len - 1] = '\0';
		len--;
	}

	hFind = FindFirstFile(szDir, &ffd);
	if (hFind == INVALID_HANDLE_VALUE)
		fprintf(stderr, "ERROR: File or directory not found.\n");
	else {
		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) {
			/* if param->szSrcfile is a file */

			strcpy(inlist[*nFiles], param->szSrcfile);
			if (param->szDstfile)
				strcpy(outlist[*nFiles], param->szDstfile);
			else
				set_outlist(outlist[*nFiles], param->szSrcfile);
			(*nFiles)++;
		}
		else if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			/* if param->szSrcfile is a directory */

			_tcscat(szDir, TEXT("\\"));
			_tcscat(szDir, TEXT("*"));
			hFind = FindFirstFile(szDir, &ffd);
			do {
				memset(szDir + (len + 1), 0x0, PATH_MAX - len);
				_tcscat(szDir, ffd.cFileName);
				memset(szTemp, 0x0, PATH_MAX);
#ifdef _UNICODE
				wcstombs(szTemp, szDir, PATH_MAX);
#else
				strcpy(szTemp, szDir);
#endif
				if (isWAV(szTemp)) {
					strcpy(inlist[*nFiles], szTemp);
					set_outlist(outlist[*nFiles], szTemp);
					(*nFiles)++;
				}

				/* for recursive(-r) option */
				if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					&& param->bRecursion) {
					/* ignore directory name '.\' and '..\' to prevent infinite loop */
					if (_tcscmp(ffd.cFileName, TEXT(".")) && _tcscmp(ffd.cFileName, TEXT(".."))) {
						opt_set_t *subdir_param;
						subdir_param = init_optset();
						subdir_param->szSrcfile = strdup(szTemp);
						subdir_param->bRecursion = 1;

						//printf("[DEBUG] Get into %s\n", subdir_param->szSrcfile);
						get_filelist_windows(inlist, outlist, nFiles, subdir_param);

						deinit_optset(subdir_param);
					}
				}
			} while (FindNextFile(hFind, &ffd) != 0);

			dwError = GetLastError();
			if (dwError != ERROR_NO_MORE_FILES)
			{
				_tprintf(TEXT("ERROR: FindFirstFile(), %d"), dwError);
			}
		}
		else
			fprintf(stderr, "ERROR: Unknown type - argument is neither a file nor a directory.\n");

		FindClose(hFind);
	}
}
#endif

void get_filelist(char inlist[][PATH_MAX + 1], char outlist[][PATH_MAX + 1], int *nFiles, const opt_set_t *param)
{
#if defined (__linux)
	get_filelist_linux(inlist, outlist, nFiles, param);
#elif defined (_WIN32)
	get_filelist_windows(inlist, outlist, nFiles, param);
#endif
}

/**
 * @brief	Initialize each file and lame library.
 *		Set encoding quality level as set in an option parameter.
 * @param [out]	pgf	Pointer to lame global flag
 * @param [in]	param	Option set
 * @param [in]	inFile	Input filename
 * @param [in]	outFile	Output filename
 * @param [in]	nFile	The ID number of the file to be used for get_audio_global_data
 * @return	Pointer of FILE structure
 */
FILE * init_file(lame_t *pgf, const opt_set_t *param, const char *inFile, const char *outFile, const int nFile)
{
	FILE *outf;

	*pgf = lame_init();
	if (param->nQualityLevel == QL_MODE_BEST) {
		lame_set_preset(*pgf, INSANE);
		lame_set_quality(*pgf, 0);
	}
	else if (param->nQualityLevel == QL_MODE_FAST) {
		lame_set_force_ms(*pgf, 1);
		lame_set_mode(*pgf, JOINT_STEREO);
		lame_set_quality(*pgf, 7);
	}
	else {
        lame_set_VBR_q(*pgf, 2);
        lame_set_VBR(*pgf, vbr_default);;
	}

	if (strcmp(inFile, outFile) == 0) {
		fprintf(stderr, "ERROR: The input file name is same with output file name. Abort.\n");
		return NULL;
	}

	if (!isWAV(inFile)) {
		fprintf(stderr, "ERROR: Input file is not wav file.\n");
		return NULL;
	}

	if (init_infile(*pgf, inFile, nFile) < 0) {
		fprintf(stderr, "ERROR: Initializing input file failed.\n");
		return NULL;
	}

	if ((outf = init_outfile(outFile)) == NULL) {
		fprintf(stderr, "ERROR: Initializing output file failed.\n");
		return NULL;
	}

	lame_set_write_id3tag_automatic(*pgf, 0);
	if (lame_init_params(*pgf) < 0) {
		fprintf(stderr, "ERROR: lame_init_params() error.\n");
		return NULL;
	}

	return outf;
}

void usage()
{
	printf("Usage:\n"
		"   MP3enc <input_filename [-o <output_filename>] | input_directory> [OPTIONS]\n"
		"\nOptions:\n"
		"\t-h\t\t Show help\n"
		"\t-r\t\t Search subdirectories recursively\n"
		"\t-q <mode>\t Set quality level\n"
		"\t    fast\t   fast encoding with small file size\n"
		"\t    standard\t   standard quality - default\n"
		"\t    best\t   best quality\n"
		"\t-v\t\t Show verbose encoding details\n"

		"\nExample:\n"
		"   MP3enc input.wav -o output.mp3\n"
		"   MP3enc wav_dir"
#if defined (__linux)
		"/"
#else
		"\\"
#endif
		" -r -q fast -v\n"
		);
	exit(0);
}

/**
 * @brief	Parse application arguments and set option set parameter.
 * @remark	Not use getopt() for the benefit of compatibility for Windows
 */
void parseopt(int argc, char *argv[], opt_set_t *param)
{
	if (argc < 2) {
		fprintf(stderr, "ERROR: Too few arguments. See below usage:\n");
		deinit_optset(param);
		usage();
	}
	else {
		int i;

		for (i = 1; i < argc; i++) {
			if (!strcmp(argv[i], "-h")) {
				deinit_optset(param);
				usage();
			}
			else if (!strcmp(argv[i], "-o")) {
				if (!param->szDstfile) {
					i++;
					if (i < argc) {
						param->szDstfile = strdup(argv[i]);
					}
					else {
						fprintf(stderr, "ERROR: Output filename is missing."
								" See below usage:\n");
						deinit_optset(param);
						usage();
					}
				}
				else {
					fprintf(stderr, "ERROR: Duplicated parameter '-o'\n");
					deinit_optset(param);
					exit(0);
				}
			}
			else if (!strcmp(argv[i], "-r")) {
				param->bRecursion = 1;
			}
			else if (!strcmp(argv[i], "-q")) {
				if (!(param->nQualityLevel & QL_SET)) {
					i++;
					if (i < argc) {
						if (!strcmp(argv[i], "fast")) {
							param->nQualityLevel = QL_MODE_FAST;
						}
						else if (!strcmp(argv[i], "standard")) {
							param->nQualityLevel = QL_MODE_STANDARD;
						}
						else if (!strcmp(argv[i], "best")) {
							param->nQualityLevel = QL_MODE_BEST;
						}
						else {
							fprintf(stderr, "ERROR: Invalid options."
									" See below usage:\n");
							deinit_optset(param);
							usage();
						}
					}
					else {
						fprintf(stderr, "ERROR: '-q' option requires mode name."
								" See below usage:\n");
						deinit_optset(param);
						usage();
					}
				}
				else {
					fprintf(stderr, "ERROR: Duplicated parameter '-q'\n");
					deinit_optset(param);
					exit(0);
				}
			}
			else if (!strcmp(argv[i], "-v")) {
				param->bVerbose = 1;
			}
			else {
				/* Any arguments except for defined options are regarded as src file */
				if (!param->szSrcfile)
					param->szSrcfile = strdup(argv[i]);
				else {
					fprintf(stderr, "ERROR: Invalid options."
							" See below usage:\n");
					deinit_optset(param);
					usage();
				}
			}
		}

		if (!param->szSrcfile) {
			fprintf(stderr, "ERROR: Input file or directory is missing."
					" See below usage:\n");
			deinit_optset(param);
			usage();
		}
	}
}

int main(int argc, char *argv[])
{
	char inFileList[NAME_MAX][PATH_MAX + 1];
	char outFileList[NAME_MAX][PATH_MAX + 1];
	opt_set_t *opt_param = NULL;
	FILE *outf[NAME_MAX];
	lame_t gf[NAME_MAX];
	int nFiles = 0;
	int ret = 0;
	int i, j;

	printf("MP3enc v" VERSION "\n");
	opt_param = init_optset();
	parseopt(argc, argv, opt_param);

	get_filelist(inFileList, outFileList, &nFiles, opt_param);
	if (nFiles < 1) {
		fprintf(stderr, "No files to encoding.\n");
		return -1;
	}
	else if (nFiles > NAME_MAX) {
		fprintf(stderr, "ERROR: Input files are too many.\n"
				"The number of maximum input files is %d", NAME_MAX);
		return -1;
	}

	for (i = 0; i < nFiles; i++) {
		if ((outf[i] = init_file(&gf[i], opt_param, inFileList[i], outFileList[i], i)) == NULL) {
			fprintf(stderr, "ERROR: init_file() failed, (%s)\n", inFileList[i]);
			return -1;
		}
	}
	printf("init_file succeeded\n");

	pthread_t *tid = malloc(sizeof(pthread_t) * nFiles);
	th_param_t *params = malloc(sizeof(th_param_t) * nFiles);
	if (nFiles > 1)
		printf("%d threads created\n", nFiles);
	for (i = 0; i < nFiles; i++) {
		(params + i)->gf = gf[i];
		(params + i)->outf = outf[i];
		(params + i)->inPath = inFileList[i];
		(params + i)->outPath = outFileList[i];
		(params + i)->nFile = i;
		(params + i)->bVerbose = opt_param->bVerbose;

		pthread_create(&tid[i], NULL, lame_encoder_loop, (void *)(params + i));
		lame_init_bitstream(gf[i]);
	}

	for	(j = 0; j < nFiles; j++) {
		pthread_join(tid[j], (void *)&ret);
		if (ret)
			fprintf(stderr, "ERROR: Encoding #%d is failed\n", j + 1);
	}

	for	(i = 0; i < nFiles; i++) {
		fclose(outf[i]);
		close_infile(i);
		lame_close(gf[i]);
	}
	deinit_optset(opt_param);

	return 0;
}
