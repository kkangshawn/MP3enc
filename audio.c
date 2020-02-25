/**
 * @file        audio.c
 * @version     0.6
 * @brief       derived from get_audio.c and parse.c of lame encoder frontend application.
 * @date        Feb 25, 2020
 * @author      Siwon Kang (kkangshawn@gmail.com)
 */

#include "audio.h"


/* global data for get_audio.c. */

typedef enum ByteOrder { ByteOrderLittleEndian, ByteOrderBigEndian } ByteOrder;
typedef enum sound_file_format_e {
    sf_unknown,
    sf_raw,
    sf_wave,
    sf_aiff,
    sf_mp1,                  /* MPEG Layer 1, aka mpg */
    sf_mp2,                  /* MPEG Layer 2 */
    sf_mp3,                  /* MPEG Layer 3 */
    sf_mp123,                /* MPEG Layer 1,2 or 3; whatever .mp3, .mp2, .mp1 or .mpg contains */
    sf_ogg
} sound_file_format;


struct PcmBuffer {
    void   *ch[2];           /* buffer for each channel */
    int     w;               /* sample width */
    int     n;               /* number samples allocated */
    int     u;               /* number samples used */
    int     skip_start;      /* number samples to ignore at the beginning */
    int     skip_end;        /* number samples to ignore at the end */
};
typedef struct PcmBuffer PcmBuffer;

typedef struct RawPCMConfig
{
    int     in_bitwidth;
    int     in_signed;
    ByteOrder in_endian;
} RawPCMConfig;
RawPCMConfig global_raw_pcm =
{ /* in_bitwidth */ 16
, /* in_signed   */ -1
, /* in_endian   */ ByteOrderLittleEndian
};

typedef struct get_audio_global_data_struct {
    int     count_samples_carefully;
    int     pcmbitwidth;
    int     pcmswapbytes;
    int     pcm_is_unsigned_8bit;
    int     pcm_is_ieee_float;
    unsigned int num_samples_read;
    FILE   *music_in;
    hip_t   hip;
    PcmBuffer pcm32;
    PcmBuffer pcm16;
    size_t  in_id3v2_size;
    unsigned char* in_id3v2_tag;
} get_audio_global_data;
static get_audio_global_data audio_data[NAME_MAX];

typedef struct ReaderConfig
{
    sound_file_format input_format;
    int   swapbytes;                /* force byte swapping   default=0 */
    int   swap_channel;             /* 0: no-op, 1: swaps input channels */
    int   input_samplerate;
} ReaderConfig;
ReaderConfig reader_config[NAME_MAX];

typedef struct WriterConfig
{
    int   flush_write;
} WriterConfig;
WriterConfig writer_config[NAME_MAX];

typedef struct UiConfig
{
    int   silent;                   /* Verbosity */
    int   brhist;
    int   print_clipping_info;      /* print info whether waveform clips */
    float update_interval;          /* to use Frank's time status display */
} UiConfig;
UiConfig ui_config[NAME_MAX];


static void
initPcmBuffer(PcmBuffer * b, int w)
{
    b->ch[0] = 0;
    b->ch[1] = 0;
    b->w = w;
    b->n = 0;
    b->u = 0;
    b->skip_start = 0;
    b->skip_end = 0;
}

static void
freePcmBuffer(PcmBuffer * b)
{
    if (b != 0) {
        free(b->ch[0]);
        free(b->ch[1]);
        b->ch[0] = 0;
        b->ch[1] = 0;
        b->n = 0;
        b->u = 0;
    }
}

static int
addPcmBuffer(PcmBuffer * b, void *a0, void *a1, int read)
{
    int a_n;

    if (b == 0) {
        return 0;
    }
    if (read < 0) {
        return b->u - b->skip_end;
    }
    if (b->skip_start >= read) {
        b->skip_start -= read;
        return b->u - b->skip_end;
    }
    a_n = read - b->skip_start;

    if (b != 0 && a_n > 0) {
        int const a_skip = b->w * b->skip_start;
        int const a_want = b->w * a_n;
        int const b_used = b->w * b->u;
        int const b_have = b->w * b->n;
        int const b_need = b->w * (b->u + a_n);
        if (b_have < b_need) {
            b->n = b->u + a_n;
            b->ch[0] = realloc(b->ch[0], b_need);
            b->ch[1] = realloc(b->ch[1], b_need);
        }
        b->u += a_n;
        if (b->ch[0] != 0 && a0 != 0) {
            char   *src = a0;
            char   *dst = b->ch[0];
            memcpy(dst + b_used, src + a_skip, a_want);
        }
        if (b->ch[1] != 0 && a1 != 0) {
            char   *src = a1;
            char   *dst = b->ch[1];
            memcpy(dst + b_used, src + a_skip, a_want);
        }
    }
    b->skip_start = 0;
    return b->u - b->skip_end;
}

static int
takePcmBuffer(PcmBuffer * b, void *a0, void *a1, int a_n, int mm)
{
    if (a_n > mm) {
        a_n = mm;
    }
    if (b != 0 && a_n > 0) {
        int const a_take = b->w * a_n;
        if (a0 != 0 && b->ch[0] != 0) {
            memcpy(a0, b->ch[0], a_take);
        }
        if (a1 != 0 && b->ch[1] != 0) {
            memcpy(a1, b->ch[1], a_take);
        }
        b->u -= a_n;
        if (b->u < 0) {
            b->u = 0;
            return a_n;
        }
        if (b->ch[0] != 0) {
            memmove(b->ch[0], (char *) b->ch[0] + a_take, b->w * b->u);
        }
        if (b->ch[1] != 0) {
            memmove(b->ch[1], (char *) b->ch[1] + a_take, b->w * b->u);
        }
    }

    return a_n;
}

static int
get_audio_common(lame_t gfp, int buffer[2][1152], short buffer16[2][1152], int num_file);

/************************************************************************
unpack_read_samples - read and unpack signed low-to-high byte or unsigned
                      single byte input. (used for read_samples function)
                      Output integers are stored in the native byte order
                      (little or big endian).  -jd
  in: samples_to_read
      bytes_per_sample
      swap_order    - set for high-to-low byte order input stream
 i/o: pcm_in
 out: sample_buffer  (must be allocated up to samples_to_read upon call)
returns: number of samples read
*/
static int
unpack_read_samples(const int samples_to_read, const int bytes_per_sample,
                    const int swap_order, int *sample_buffer, FILE * pcm_in, int num_file)
{
    size_t  samples_read;
    int     i;
    int    *op;              /* output pointer */
    unsigned char *ip = (unsigned char *) sample_buffer; /* input pointer */
    const int b = sizeof(int) * 8;

#define GA_URS_IFLOOP( ga_urs_bps ) \
    if( bytes_per_sample == ga_urs_bps ) \
      for( i = samples_read * bytes_per_sample; (i -= bytes_per_sample) >=0;)

    samples_read = fread(sample_buffer, bytes_per_sample, samples_to_read, pcm_in);
    op = sample_buffer + samples_read;

    if (swap_order == 0) {
        GA_URS_IFLOOP(1)
            * --op = ip[i] << (b - 8);
        GA_URS_IFLOOP(2)
            * --op = ip[i] << (b - 16) | ip[i + 1] << (b - 8);
        GA_URS_IFLOOP(3)
            * --op = ip[i] << (b - 24) | ip[i + 1] << (b - 16) | ip[i + 2] << (b - 8);
        GA_URS_IFLOOP(4)
            * --op =
            ip[i] << (b - 32) | ip[i + 1] << (b - 24) | ip[i + 2] << (b - 16) | ip[i + 3] << (b -
                                                                                              8);
    }
    else {
        GA_URS_IFLOOP(1)
            * --op = (ip[i] ^ 0x80) << (b - 8) | 0x7f << (b - 16); /* convert from unsigned */
        GA_URS_IFLOOP(2)
            * --op = ip[i] << (b - 8) | ip[i + 1] << (b - 16);
        GA_URS_IFLOOP(3)
            * --op = ip[i] << (b - 8) | ip[i + 1] << (b - 16) | ip[i + 2] << (b - 24);
        GA_URS_IFLOOP(4)
            * --op =
            ip[i] << (b - 8) | ip[i + 1] << (b - 16) | ip[i + 2] << (b - 24) | ip[i + 3] << (b -
                                                                                             32);
    }
#undef GA_URS_IFLOOP
    if (audio_data[num_file].pcm_is_ieee_float) {
        float const m_max = INT_MAX;
        float const m_min = -(float) INT_MIN;
        float *x = (float *) sample_buffer;
        assert(sizeof(float) == sizeof(int));
        for (i = 0; i < samples_to_read; ++i) {
            float const u = x[i];
            int     v;
            if (u >= 1) {
                v = INT_MAX;
            }
            else if (u <= -1) {
                v = INT_MIN;
            }
            else if (u >= 0) {
                v = (int) (u * m_max + 0.5f);
            }
            else {
                v = (int) (u * m_min - 0.5f);
            }
            sample_buffer[i] = v;
        }
    }

    return (samples_read);
}

/************************************************************************
*
* read_samples()
*
* PURPOSE:  reads the PCM samples from a file to the buffer
*
*  SEMANTICS:
* Reads #samples_read# number of shorts from #musicin# filepointer
* into #sample_buffer[]#.  Returns the number of samples read.
*
************************************************************************/
static int
read_samples_pcm(FILE * musicin, int sample_buffer[2304], int samples_to_read, int num_file)
{
    int samples_read;
    int bytes_per_sample = audio_data[num_file].pcmbitwidth / 8;
    int swap_byte_order; /* byte order of input stream */

    switch (audio_data[num_file].pcmbitwidth) {
    case 32:
    case 24:
    case 16:
        if (global_raw_pcm.in_signed == 0) {
            printf("Unsigned input only supported with bitwidth 8\n");
            return -1;
        }
        swap_byte_order = (global_raw_pcm.in_endian != ByteOrderLittleEndian) ? 1 : 0;
        if (audio_data[num_file].pcmswapbytes) {
            swap_byte_order = !swap_byte_order;
        }
        break;

    case 8:
        swap_byte_order = audio_data[num_file].pcm_is_unsigned_8bit;
        break;

    default:
        printf("Only 8, 16, 24 and 32 bit input files supported \n");
        return -1;
    }
    samples_read = unpack_read_samples(samples_to_read, bytes_per_sample, swap_byte_order,
                                       sample_buffer, musicin, num_file);
    if (ferror(musicin)) {
        printf("Error reading input file\n");
        return -1;
    }

    return samples_read;
}

/************************************************************************
*
* get_audio()
*
* PURPOSE:  reads a frame of audio data from a file to the buffer,
*   aligns the data for future processing, and separates the
*   left and right channels
*
************************************************************************/
int
get_audio(lame_t gfp, int buffer[2][1152], int num_file)
{
    int used = 0, read = 0;
    do {
        read = get_audio_common(gfp, buffer, NULL, num_file);
        used = addPcmBuffer(&audio_data[num_file].pcm32, buffer[0], buffer[1], read);
    } while (used <= 0 && read > 0);
    if (read < 0) {
        return read;
    }
    if (reader_config[num_file].swap_channel == 0)
        return takePcmBuffer(&audio_data[num_file].pcm32, buffer[0], buffer[1], used, 1152);
    else
        return takePcmBuffer(&audio_data[num_file].pcm32, buffer[1], buffer[0], used, 1152);
}

/************************************************************************
  get_audio_common - central functionality of get_audio*
    in: gfp
        buffer    output to the int buffer or 16-bit buffer
   out: buffer    int output    (if buffer != NULL)
        buffer16  16-bit output (if buffer == NULL)
returns: samples read
note: either buffer or buffer16 must be allocated upon call
*/
static int
get_audio_common(lame_t gfp, int buffer[2][1152], short buffer16[2][1152], int num_file)
{
    int num_channels = lame_get_num_channels(gfp);
    int insamp[2 * 1152];
    int samples_read;
    int framesize;
    int samples_to_read;
    unsigned int remaining, tmp_num_samples;
    int i;
    int *p;

    /*
     * NOTE: LAME can now handle arbritray size input data packets,
     * so there is no reason to read the input data in chuncks of
     * size "framesize".  EXCEPT:  the LAME graphical frame analyzer
     * will get out of sync if we read more than framesize worth of data.
     */

    samples_to_read = framesize = lame_get_framesize(gfp);
    assert(framesize <= 1152);

    /* get num_samples */
    tmp_num_samples = lame_get_num_samples(gfp);

    /* if this flag has been set, then we are carefull to read
     * exactly num_samples and no more.  This is useful for .wav and .aiff
     * files which have id3 or other tags at the end.  Note that if you
     * are using LIBSNDFILE, this is not necessary
     */
    if (audio_data[num_file].count_samples_carefully) {
        if (audio_data[num_file].num_samples_read < tmp_num_samples) {
            remaining = tmp_num_samples - audio_data[num_file].num_samples_read;
        }
        else {
            remaining = 0;
        }
        if (remaining < (unsigned int) framesize && 0 != tmp_num_samples)
            /* in case the input is a FIFO (at least it's reproducible with
               a FIFO) tmp_num_samples may be 0 and therefore remaining
               would be 0, but we need to read some samples, so don't
               change samples_to_read to the wrong value in this case */
            samples_to_read = remaining;
    }

    samples_read =
        read_samples_pcm(audio_data[num_file].music_in, insamp, num_channels * samples_to_read, num_file);
    if (samples_read < 0) {
        return samples_read;
    }
    p = insamp + samples_read;
    samples_read /= num_channels;
    if (buffer != NULL) { /* output to int buffer */
        if (num_channels == 2) {
            for (i = samples_read; --i >= 0;) {
                buffer[1][i] = *--p;
                buffer[0][i] = *--p;
            }
        }
        else if (num_channels == 1) {
            memset(buffer[1], 0, samples_read * sizeof(int));
            for (i = samples_read; --i >= 0;) {
                buffer[0][i] = *--p;
            }
        }
        else
            assert(0);
    }
    else {          /* convert from int; output to 16-bit buffer */
        if (num_channels == 2) {
            for (i = samples_read; --i >= 0;) {
                buffer16[1][i] = *--p >> (8 * sizeof(int) - 16);
                buffer16[0][i] = *--p >> (8 * sizeof(int) - 16);
            }
        }
        else if (num_channels == 1) {
            memset(buffer16[1], 0, samples_read * sizeof(short));
            for (i = samples_read; --i >= 0;) {
                buffer16[0][i] = *--p >> (8 * sizeof(int) - 16);
            }
        }
        else
            assert(0);
    }

    /* if num_samples = MAX_U_32_NUM, then it is considered infinitely long.
       Don't count the samples */
    if (tmp_num_samples != MAX_U_32_NUM)
        audio_data[num_file]. num_samples_read += samples_read;

    return samples_read;
}


static void
setSkipStartAndEnd(lame_t gfp, int enc_delay, int enc_padding, int num_file)
{
    int skip_start = 0, skip_end = 0;

    switch (reader_config[num_file].input_format) {
    case sf_mp123:
        break;

    case sf_mp3:
        if (skip_start == 0) {
            if (enc_delay > -1 || enc_padding > -1) {
                if (enc_delay > -1)
                    skip_start = enc_delay + 528 + 1;
                if (enc_padding > -1)
                    skip_end = enc_padding - (528 + 1);
            }
            else
                skip_start = lame_get_encoder_delay(gfp) + 528 + 1;
        }
        else {
            /* user specified a value of skip. just add for decoder */
            skip_start += 528 + 1; /* mp3 decoder has a 528 sample delay, plus user supplied "skip" */
        }
        break;
    case sf_mp2:
        skip_start += 240 + 1;
        break;
    case sf_mp1:
        skip_start += 240 + 1;
        break;
    case sf_raw:
        skip_start += 0; /* other formats have no delay *//* is += 0 not better ??? */
        break;
    case sf_wave:
        skip_start += 0; /* other formats have no delay *//* is += 0 not better ??? */
        break;
    case sf_aiff:
        skip_start += 0; /* other formats have no delay *//* is += 0 not better ??? */
        break;
    default:
        skip_start += 0; /* other formats have no delay *//* is += 0 not better ??? */
        break;
    }
    skip_start = skip_start < 0 ? 0 : skip_start;
    skip_end = skip_end < 0 ? 0 : skip_end;
    audio_data[num_file]. pcm16.skip_start = audio_data[num_file].pcm32.skip_start = skip_start;
    audio_data[num_file]. pcm16.skip_end = audio_data[num_file].pcm32.skip_end = skip_end;
}

static int
read_16_bits_low_high(FILE * fp)
{
    unsigned char bytes[2] = { 0, 0 };
    fread(bytes, 1, 2, fp);
    {
        int32_t const low = bytes[0];
        int32_t const high = (signed char) (bytes[1]);
        return (high << 8) | low;
    }
}

static int
read_32_bits_low_high(FILE * fp)
{
    unsigned char bytes[4] = { 0, 0, 0, 0 };
    fread(bytes, 1, 4, fp);
    {
        int32_t const low = bytes[0];
        int32_t const medl = bytes[1];
        int32_t const medh = bytes[2];
        int32_t const high = (signed char) (bytes[3]);
        return (high << 24) | (medh << 16) | (medl << 8) | low;
    }
}

/*
static int
read_16_bits_high_low(FILE * fp)
{
    unsigned char bytes[2] = { 0, 0 };
    fread(bytes, 1, 2, fp);
    {
        int32_t const low = bytes[1];
        int32_t const high = (signed char) (bytes[0]);
        return (high << 8) | low;
    }
}
*/

static int
read_32_bits_high_low(FILE * fp)
{
    unsigned char bytes[4] = { 0, 0, 0, 0 };
    fread(bytes, 1, 4, fp);
    {
        int32_t const low = bytes[3];
        int32_t const medl = bytes[2];
        int32_t const medh = bytes[1];
        int32_t const high = (signed char) (bytes[0]);
        return (high << 24) | (medh << 16) | (medl << 8) | low;
    }
}

static size_t
min_size_t(size_t a, size_t b)
{
    if (a < b) {
        return a;
    }
    return b;
}

/* Replacement for forward fseek(,,SEEK_CUR), because fseek() fails on pipes */


static int
fskip(FILE * fp, long offset, int whence)
{
#ifndef PIPE_BUF
    char buffer[4096];
#else
    char buffer[PIPE_BUF];
#endif

/* S_ISFIFO macro is defined on newer Linuxes */
#ifndef S_ISFIFO
# ifdef _S_IFIFO
    /* _S_IFIFO is defined on Win32 and Cygwin */
#  define S_ISFIFO(m) (((m)&_S_IFIFO) == _S_IFIFO)
# endif
#endif

#ifdef S_ISFIFO
    /* fseek is known to fail on pipes with several C-Library implementations
       workaround: 1) test for pipe
       2) for pipes, only relatvie seeking is possible
       3)            and only in forward direction!
       else fallback to old code
     */
    {
        int const fd = fileno(fp);
        struct stat file_stat;

        if (fstat(fd, &file_stat) == 0) {
            if (S_ISFIFO(file_stat.st_mode)) {
                if (whence != SEEK_CUR || offset < 0) {
                    return -1;
                }
                while (offset > 0) {
                    size_t const bytes_to_skip = min_size_t(sizeof(buffer), offset);
                    size_t const read = fread(buffer, 1, bytes_to_skip, fp);
                    if (read < 1) {
                        return -1;
                    }
                    offset -= read;
                }
                return 0;
            }
        }
    }
#endif
    if (0 == fseek(fp, offset, whence)) {
        return 0;
    }

    if (whence != SEEK_CUR || offset < 0) {
        fprintf(stderr,
                "fskip problem: "
                "Mostly the return status of functions is not evaluate so it is more secure to polute <stderr>.\n");
        return -1;
    }

    while (offset > 0) {
        size_t const bytes_to_skip = min_size_t(sizeof(buffer), offset);
        size_t const read = fread(buffer, 1, bytes_to_skip, fp);
        if (read < 1) {
            return -1;
        }
        offset -= read;
    }

    return 0;
}


static off_t
lame_get_file_size(FILE * fp)
{
    struct stat sb;
    int fd = fileno(fp);

    if (0 == fstat(fd, &sb))
        return sb.st_size;

    return (off_t) - 1;
}

static long
make_even_number_of_bytes_in_length(long x)
{
    if ((x & 0x01) != 0)
        return x + 1;

    return x;
}

/*****************************************************************************
 *
 *  Read Microsoft Wave headers
 *
 *  By the time we get here the first 32-bits of the file have already been
 *  read, and we're pretty sure that we're looking at a WAV file.
 *
 *****************************************************************************/

static int
parse_wave_header(lame_global_flags * gfp, FILE * sf, int num_file)
{
    int     format_tag = 0;
    int     channels = 0;
    int     bits_per_sample = 0;
    int     samples_per_sec = 0;
    int     is_wav = 0;
    long    data_length = 0, subSize = 0;
    int     loop_sanity = 0;

    /* Below variables are not used but declared for buffer read operation */
    /* int     block_align = 0; */
    /* int     avg_bytes_per_sec = 0; */
    /* long    file_length; */

    /*file_length = */read_32_bits_high_low(sf);
    if (read_32_bits_high_low(sf) != WAV_ID_WAVE)
        return -1;

    for (loop_sanity = 0; loop_sanity < 20; ++loop_sanity) {
        int     type = read_32_bits_high_low(sf);

        if (type == WAV_ID_FMT) {
            subSize = read_32_bits_low_high(sf);
            subSize = make_even_number_of_bytes_in_length(subSize);
            if (subSize < 16) {
                /* DEBUGF(
                   "'fmt' chunk too short (only %ld bytes)!", subSize);  */
                return -1;
            }

            format_tag = read_16_bits_low_high(sf);
            subSize -= 2;
            channels = read_16_bits_low_high(sf);
            subSize -= 2;
            samples_per_sec = read_32_bits_low_high(sf);
            subSize -= 4;
            /* avg_bytes_per_sec = */read_32_bits_low_high(sf);
            subSize -= 4;
            /* block_align = */read_16_bits_low_high(sf);
            subSize -= 2;
            bits_per_sample = read_16_bits_low_high(sf);
            subSize -= 2;

            /* WAVE_FORMAT_EXTENSIBLE support */
            if ((subSize > 9) && (format_tag == WAVE_FORMAT_EXTENSIBLE)) {
                read_16_bits_low_high(sf); /* cbSize */
                read_16_bits_low_high(sf); /* ValidBitsPerSample */
                read_32_bits_low_high(sf); /* ChannelMask */
                /* SubType coincident with format_tag for PCM int or float */
                format_tag = read_16_bits_low_high(sf);
                subSize -= 10;
            }

            /* DEBUGF("   skipping %d bytes\n", subSize); */

            if (subSize > 0) {
                if (fskip(sf, (long) subSize, SEEK_CUR) != 0)
                    return -1;
            };

        }
        else if (type == WAV_ID_DATA) {
            subSize = read_32_bits_low_high(sf);
            data_length = subSize;
            is_wav = 1;
            /* We've found the audio data. Read no further! */
            break;

        }
        else {
            subSize = read_32_bits_low_high(sf);
            subSize = make_even_number_of_bytes_in_length(subSize);
            if (fskip(sf, (long) subSize, SEEK_CUR) != 0) {
                return -1;
            }
        }
    }
    if (is_wav) {
        if (format_tag != WAVE_FORMAT_PCM && format_tag != WAVE_FORMAT_IEEE_FLOAT) {
            if (ui_config[num_file].silent < 10) {
                printf("Unsupported data format: 0x%04X\n", format_tag);
            }
            return 0;   /* oh no! non-supported format  */
        }

        /* make sure the header is sane */
        if (-1 == lame_set_num_channels(gfp, channels)) {
            if (ui_config[num_file].silent < 10) {
                printf("Unsupported number of channels: %u\n", channels);
            }
            return 0;
        }
        if (reader_config[num_file].input_samplerate == 0) {
            (void) lame_set_in_samplerate(gfp, samples_per_sec);
        }
        else {
            (void) lame_set_in_samplerate(gfp, reader_config[num_file].input_samplerate);
        }
        audio_data[num_file]. pcmbitwidth = bits_per_sample;
        audio_data[num_file]. pcm_is_unsigned_8bit = 1;
        audio_data[num_file]. pcm_is_ieee_float = (format_tag == WAVE_FORMAT_IEEE_FLOAT ? 1 : 0);
        (void) lame_set_num_samples(gfp, data_length / (channels * ((bits_per_sample + 7) / 8)));

        return 1;
    }

    return -1;
}

static int
parse_file_header(lame_global_flags * gfp, FILE * sf, int num_file)
{
    int type = read_32_bits_high_low(sf);
    /*
       DEBUGF(
       "First word of input stream: %08x '%4.4s'\n", type, (char*) &type);
     */
    audio_data[num_file]. count_samples_carefully = 0;
    audio_data[num_file]. pcm_is_unsigned_8bit = global_raw_pcm.in_signed == 1 ? 0 : 1;
    /*global_reader.input_format = sf_raw; commented out, because it is better to fail
       here as to encode some hundreds of input files not supported by LAME
       If you know you have RAW PCM data, use the -r switch
     */

    if (type == WAV_ID_RIFF) {
        /* It's probably a WAV file */
        int const ret = parse_wave_header(gfp, sf, num_file);
        if (ret > 0) {
            audio_data[num_file]. count_samples_carefully = 1;
            return sf_wave;
        }
        if (ret < 0) {
            printf("Warning: corrupt or unsupported WAVE format\n");
        }
    }
    else {
        printf("Warning: unsupported audio format\n");
    }

    return sf_unknown;
}

static FILE *
open_wave_file(lame_t gfp, char const *in_path, int num_file)
{
    FILE *musicin;

    /* set the defaults from info incase we cannot determine them from file */
    lame_set_num_samples(gfp, MAX_U_32_NUM);

    if ((musicin = fopen(in_path, "rb")) == NULL) {
        if (ui_config[num_file].silent < 10) {
            printf("Could not find \"%s\".\n", in_path);
        }
        exit(1);
    }

    if (reader_config[num_file].input_format == sf_raw) {
        /* assume raw PCM */
        if (ui_config[num_file].silent < 9) {
            printf("Assuming raw pcm input file");
            if (reader_config[num_file].swapbytes)
                printf(" : Forcing byte-swapping\n");
            else
                printf("\n");
        }
        audio_data[num_file]. pcmswapbytes = reader_config[num_file].swapbytes;
    }
    else {
        reader_config[num_file].input_format = parse_file_header(gfp, musicin, num_file);
    }
    if (reader_config[num_file].input_format == sf_unknown) {
        return NULL;
    }

    if (lame_get_num_samples(gfp) == MAX_U_32_NUM && musicin != stdin) {
        double const flen = lame_get_file_size(musicin); /* try to figure out num_samples */
        if (flen >= 0) {
            /* try file size, assume 2 bytes per sample */
            unsigned long fsize = (unsigned long) (flen / (2 * lame_get_num_channels(gfp)));
            (void) lame_set_num_samples(gfp, fsize);
        }
    }

    return musicin;
}

int
init_infile(lame_t gfp, char const *in_path, const int num_file)
{
    int enc_delay = 0, enc_padding = 0;

    audio_data[num_file]. count_samples_carefully = 0;
    audio_data[num_file]. num_samples_read = 0;
    audio_data[num_file]. pcmbitwidth = global_raw_pcm.in_bitwidth;
    audio_data[num_file]. pcmswapbytes = reader_config[num_file].swapbytes;
    audio_data[num_file]. pcm_is_unsigned_8bit = global_raw_pcm.in_signed == 1 ? 0 : 1;
    audio_data[num_file]. pcm_is_ieee_float = 0;
    audio_data[num_file]. hip = 0;
    audio_data[num_file]. music_in = 0;
    audio_data[num_file]. in_id3v2_size = 0;
    audio_data[num_file]. in_id3v2_tag = 0;

    audio_data[num_file]. music_in = open_wave_file(gfp, in_path, num_file);

    initPcmBuffer(&audio_data[num_file].pcm32, sizeof(int));
    initPcmBuffer(&audio_data[num_file].pcm16, sizeof(short));
    setSkipStartAndEnd(gfp, enc_delay, enc_padding, num_file);
    {
        unsigned long n = lame_get_num_samples(gfp);
        if (n != MAX_U_32_NUM) {
            unsigned long const discard = audio_data[num_file].pcm32.skip_start + audio_data[num_file].pcm32.skip_end;
            lame_set_num_samples(gfp, n > discard ? n - discard : 0);
        }
    }

    return (audio_data[num_file].music_in != NULL) ? 1 : -1;
}

FILE *
init_outfile(const char *out_path)
{
    FILE *outf;
    outf = fopen(out_path, "w+b");

    return outf;
}

void
close_infile(int num_file)
{
    if ((audio_data[num_file].music_in != 0)
            && (audio_data[num_file].music_in != stdin)
            && (fclose(audio_data[num_file].music_in) != 0)
            )
        fprintf(stderr, "Could not close audio input file\n");

    audio_data[num_file].music_in = 0;
    freePcmBuffer(&audio_data[num_file].pcm16);
    freePcmBuffer(&audio_data[num_file].pcm32);

    if (audio_data[num_file].in_id3v2_tag) {
        free(audio_data[num_file].in_id3v2_tag);
        audio_data[num_file].in_id3v2_tag = 0;
        audio_data[num_file].in_id3v2_size = 0;
    }
}

static int
write_xing_frame(lame_global_flags * gf, FILE * outf, size_t offset)
{
    unsigned char mp3buffer[LAME_MAXMP3BUFFER];
    size_t imp3, owrite;

    imp3 = lame_get_lametag_frame(gf, mp3buffer, sizeof(mp3buffer));
    if (imp3 <= 0) {
        return 0;       /* nothing to do */
    }
    if (imp3 > sizeof(mp3buffer)) {
        printf
            ("Error writing LAME-tag frame: buffer too small: buffer size=%lu  frame size=%lu\n",
             (unsigned long)sizeof(mp3buffer), (unsigned long)imp3);
        return -1;
    }
    if (fseek(outf, offset, SEEK_SET) != 0) {
        printf("fatal error: can't update LAME-tag frame!\n");
        return -1;
    }
    owrite = (int) fwrite(mp3buffer, 1, imp3, outf);
    if (owrite != imp3) {
        printf("Error writing LAME-tag \n");
        return -1;
    }

    return imp3;
}

static int
write_id3v1_tag(lame_t gf, FILE * outf)
{
    unsigned char mp3buffer[128];
    int imp3, owrite;

    imp3 = lame_get_id3v1_tag(gf, mp3buffer, sizeof(mp3buffer));
    if (imp3 <= 0) {
        return 0;
    }
    if ((size_t) imp3 > sizeof(mp3buffer)) {
        printf("Error writing ID3v1 tag: buffer too small: buffer size=%lu  ID3v1 size=%d\n",
                     (unsigned long)sizeof(mp3buffer), imp3);
        return 0;       /* not critical */
    }
    owrite = (int) fwrite(mp3buffer, 1, imp3, outf);
    if (owrite != imp3) {
        printf("Error writing ID3v1 tag \n");
        return 1;
    }
    return 0;
}

size_t
sizeOfOldTag(lame_t gf, int num_file)
{
    (void) gf;
    return audio_data[num_file].in_id3v2_size;
}

unsigned char*
getOldTag(lame_t gf, int num_file)
{
    (void) gf;
    return audio_data[num_file].in_id3v2_tag;
}

void *
lame_encoder_loop(void *data)
{
    unsigned char mp3buffer[LAME_MAXMP3BUFFER];
    int buf[2][1152];
    int iread, imp3, owrite;
    size_t id3v2_size;

    th_param_t *param = (th_param_t *)data;
    lame_global_flags *gf = param->gf;
    FILE *outf = param->outf;
    char *inPath = param->in_path;
    char *outPath = param->out_path;
    int num_file = param->idx_file;

    id3v2_size = lame_get_id3v2_tag(gf, 0, 0);
    if (id3v2_size > 0) {
        unsigned char *id3v2tag = malloc(id3v2_size);
        if (id3v2tag != 0) {
            imp3 = lame_get_id3v2_tag(gf, id3v2tag, id3v2_size);
            owrite = (int) fwrite(id3v2tag, 1, imp3, outf);
            free(id3v2tag);
            if (owrite != imp3) {
                printf("Error writing ID3v2 tag \n");
                return (void *)1;
            }
        }
    }
    else {
        unsigned char* id3v2tag = getOldTag(gf, num_file);
        id3v2_size = sizeOfOldTag(gf, num_file);
        if ( id3v2_size > 0 ) {
            size_t owrite = fwrite(id3v2tag, 1, id3v2_size, outf);
            if (owrite != id3v2_size) {
                printf("Error writing ID3v2 tag \n");
                return (void *)1;
            }
        }
    }
    if (writer_config[num_file].flush_write == 1) {
        fflush(outf);
    }

    /* print encoding information */
    printf(" %2d: %-25s -> %-25s\n", num_file + 1, inPath, outPath);
    if (param->verbose)
    {
        printf("    Encoding as %g kHz ", 1.e-3 * lame_get_out_samplerate(gf));
        static const char *mode_names[2][4] = {
            {"stereo", "j-stereo", "dual-ch", "single-ch"},
            {"stereo", "force-ms", "dual-ch", "single-ch"}
        };
        switch (lame_get_VBR(gf)) {
        case vbr_rh:
            printf("%s MPEG-%u%s Layer III VBR(q=%g) qval=%i\n",
                           mode_names[lame_get_force_ms(gf)][lame_get_mode(gf)],
                           2 - lame_get_version(gf),
                           lame_get_out_samplerate(gf) < 16000 ? ".5" : "",
                           lame_get_VBR_quality(gf),
                           lame_get_quality(gf));
            break;
        case vbr_mt:
        case vbr_mtrh:
            printf("%s MPEG-%u%s Layer III VBR(q=%g)\n",
                           mode_names[lame_get_force_ms(gf)][lame_get_mode(gf)],
                           2 - lame_get_version(gf),
                           lame_get_out_samplerate(gf) < 16000 ? ".5" : "",
                           lame_get_VBR_quality(gf));
            break;
        case vbr_abr:
            printf("%s MPEG-%u%s Layer III (%gx) average %d kbps qval=%i\n",
                           mode_names[lame_get_force_ms(gf)][lame_get_mode(gf)],
                           2 - lame_get_version(gf),
                           lame_get_out_samplerate(gf) < 16000 ? ".5" : "",
                           0.1 * (int) (10. * lame_get_compression_ratio(gf) + 0.5),
                           lame_get_VBR_mean_bitrate_kbps(gf),
                           lame_get_quality(gf));
            break;
        default:
            printf("%s MPEG-%u%s Layer III (%gx) %3d kbps qval=%i\n",
                           mode_names[lame_get_force_ms(gf)][lame_get_mode(gf)],
                           2 - lame_get_version(gf),
                           lame_get_out_samplerate(gf) < 16000 ? ".5" : "",
                           0.1 * (int) (10. * lame_get_compression_ratio(gf) + 0.5),
                           lame_get_brate(gf),
                           lame_get_quality(gf));
            break;
        }
    }

    /* encode until we hit eof */
    do {
        /* read in 'iread' samples */
        iread = get_audio(gf, buf, num_file);

        if (iread >= 0) {

            /* encode */
            imp3 = lame_encode_buffer_int(gf, buf[0], buf[1], iread,
                                          mp3buffer, sizeof(mp3buffer));

            /* was our output buffer big enough? */
            if (imp3 < 0) {
                if (imp3 == -1)
                    printf("mp3 buffer is not big enough... \n");
                else
                    printf("mp3 internal error:  error code=%i\n", imp3);
                return (void *)1;
            }
            owrite = (int) fwrite(mp3buffer, 1, imp3, outf);
            if (owrite != imp3) {
                printf("Error writing mp3 output \n");
                return (void *)1;
            }
        }
        if (writer_config[num_file].flush_write == 1) {
            fflush(outf);
        }
    } while (iread > 0);

    imp3 = lame_encode_flush(gf, mp3buffer, sizeof(mp3buffer)); /* may return one more mp3 frame */

    if (imp3 < 0) {
        if (imp3 == -1)
            printf("mp3 buffer is not big enough... \n");
        else
            printf("mp3 internal error:  error code=%i\n", imp3);
        return (void *)1;

    }

    owrite = (int) fwrite(mp3buffer, 1, imp3, outf);
    if (owrite != imp3) {
        printf("Error writing mp3 output \n");
        return (void *)1;
    }
    if (writer_config[num_file].flush_write == 1) {
        fflush(outf);
    }
    imp3 = write_id3v1_tag(gf, outf);
    if (writer_config[num_file].flush_write == 1) {
        fflush(outf);
    }
    if (imp3) {
        return (void *)1;
    }

    write_xing_frame(gf, outf, id3v2_size);
    if (writer_config[num_file].flush_write == 1) {
        fflush(outf);
    }

    printf(" %2d: Done\n", num_file + 1);

    return (void *)0;
}
