/*
 * main.c
 *
 *  Created on: Jul 21, 2016
 *      Author: shawn
 */


#include "main.h"

int isWAV(const char *filename)
{
	int len = strlen(filename);
	if (filename[len - 4] == '.'
			&& filename[len - 3] == 'w'
			&& filename[len - 2] == 'a'
			&& filename[len - 1] == 'v')
		return 1;
	return 0;
}

void set_outlist(char outlist[PATH_MAX + 1], const char *filename)
{
	int len = strlen(filename);
	strcpy(outlist, filename);
	strcpy(outlist + len - 3, "mp3");
}

#if defined (_LINUX)
int get_filelist_linux(char inlist[][PATH_MAX + 1], char outlist[][PATH_MAX + 1], int argc, char *argv[])
{
	DIR *pDir;
	struct dirent *pDirEnt;
	int nFileCnt = 0;
	int nArgLength = strlen(argv[1]);

	/* if argv[1] is directory */
	if ((pDir = opendir(argv[1])) != NULL) {
		while ((pDirEnt = readdir(pDir)) != NULL) {
			if (pDirEnt->d_type == DIRENT_TYPE_FILE && isWAV(pDirEnt->d_name)) {
				int nFileLength = strlen(pDirEnt->d_name);
				if (nArgLength + nFileLength > PATH_MAX) {
					printf("%s", argv[1]);
					if (argv[1][nArgLength - 1] != '/')
						printf("/");
					printf("%s is too long. Maximum length is %d\n",
							pDirEnt->d_name, PATH_MAX);

					continue;
				}
				strcpy(inlist[nFileCnt], argv[1]);
				if (argv[1][nArgLength - 1] != '/')
					strcat(inlist[nFileCnt], "/");
				strcat(inlist[nFileCnt], pDirEnt->d_name);

				set_outlist(outlist[nFileCnt], inlist[nFileCnt]);
				nFileCnt++;
			}
		}
		closedir(pDir);
	}
	/* if argv[1] is file */
	else {
		if (isWAV(argv[1])) {
			if (nArgLength > PATH_MAX)
				fprintf(stderr, "ERROR: %s is too long. Maximum length is %d\n",
						argv[1], PATH_MAX);
			else {
				strcpy(inlist[nFileCnt], argv[1]);
				if (argc == 3)
					strcpy(outlist[nFileCnt], argv[2]);
				else
					set_outlist(outlist[nFileCnt], argv[1]);
				nFileCnt++;				
			}
		}
	}

	return nFileCnt;
}

#elif defined (_WIN32)
int get_filelist_windows(char inlist[][PATH_MAX + 1], char outlist[][PATH_MAX + 1], int argc, char *argv[])
{
	WIN32_FIND_DATA ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	TCHAR szDir[PATH_MAX];
	size_t len = 0;
	DWORD dwError = 0;
	int nFileCnt = 0;

	memset(szDir, 0x0, PATH_MAX);
	len = strlen(argv[1]);

	fprintf(stdout, "PATH_MAX is %d\n", PATH_MAX);
	if (len > (PATH_MAX - 3))
	{
		fprintf(stderr, "ERROR: Directory path is too long.\n");
		return -1;
	}
	printf("Came to get_filelist_windows\n");
	return nFileCnt;
}
#endif

int get_filelist(char inlist[][PATH_MAX + 1], char outlist[][PATH_MAX + 1], int argc, char *argv[])
{
	int ret;
#if defined (_LINUX)
	ret = get_filelist_linux(inlist, outlist, argc, argv);
#elif defined (_WIN32)
	ret = get_filelist_windows(inlist, outlist, argc, argv);
#endif
	return ret;
}

static FILE * init_file(lame_t *pgf, const char *inFile, char *outFile, int nFile)
{
	FILE *outf;

	*pgf = lame_init();

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

	/* turn off automatic writing of ID3 tag data into mp3 stream
	 * we have to call it before 'lame_init_params', because that
	 * function would spit out ID3v2 tag data.
	 */
	lame_set_write_id3tag_automatic(*pgf, 0);
	if (lame_init_params(*pgf) < 0) {
		fprintf(stderr, "ERROR: lame_init_params() error.\n");
		return NULL;
	}

	return outf;
}

void usage()
{
	printf("Too few arguments. See below usage:\n"
			"\tMP3Enc <input_filename [output_filename] | input_directory>\n");
	exit(0);
}

int main(int argc, char *argv[])
{
	char inFileList[NAME_MAX][PATH_MAX + 1];
	char outFileList[NAME_MAX][PATH_MAX + 1];

	FILE *outf[NAME_MAX];
	lame_t gf[NAME_MAX];
	int nFiles;
	int ret;
	int i;

	printf("MP3Enc v" VERSION "\n");

	if (argc < 2)
		usage();

	nFiles = get_filelist(inFileList, outFileList, argc, argv);
	if (nFiles < 1) {
		fprintf(stderr, "No files to encoding.\n");
		return -1;
	}

	for (i = 0; i < nFiles; i++) {
		if ((outf[i] = init_file(&gf[i], inFileList[i], outFileList[i], i)) == NULL) {
			fprintf(stderr, "ERROR: init_file failed (#%d, %s)\n", i, inFileList[i]);
			return -1;
		}
	}
	printf("init_file succeeded\n");

	pthread_t *tid = malloc(sizeof(pthread_t) * nFiles);
	th_param_t *params = malloc(sizeof(th_param_t) * nFiles);
	for (i = 0; i < nFiles; i++) {
		(params + i)->gf = gf[i];
		(params + i)->outf = outf[i];
		(params + i)->inPath = inFileList[i];
		(params + i)->outPath = outFileList[i];
		(params + i)->nFile = i;

		printf("Thread %d creates\n", i);
//		printf(" %s, %s, #%02d\n", (params + i)->inPath, (params + i)->outPath, (params + i)->nFile);

		pthread_create(&tid[i], NULL, lame_encoder_loop, (void *)(params + i));
		lame_init_bitstream(gf[i]);
	}

	for	(i = 0; i < nFiles; i++) {
		printf("Thread %d joins\n", i);
		pthread_join(tid[i], (void **)&ret);
	}
	if (ret)
		fprintf(stderr, "ERROR: Encoding is failed\n");

	for	(i = 0; i < nFiles; i++) {
		fclose(outf[i]);
		close_infile(i);
	}

	return 0;
}
