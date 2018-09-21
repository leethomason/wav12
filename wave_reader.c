/*
 * Base documents:
 * https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
 * http://en.wikipedia.org/wiki/WAV
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "wave_reader.h"

#define FOUR_CC(a,b,c,d) (((a)<<24) | ((b)<<16) | ((c)<<8) | ((d)<<0))

struct wave_reader {
    int format;
    int num_channels;
    int sample_rate;
    int sample_bits;
    int num_samples;
    FILE *fp;
};

static int
read_byte(FILE *fp, unsigned char *out)
{
    if (1 > fread(out, 1, 1, fp)) {
        return feof(fp) ? 0 : -1;
    }
    return 1;
}

static int
read_int32_b(FILE *fp, int *out)
{
    int result;
    unsigned char a, b, c, d;

    if ((result=read_byte(fp, &a)) != 1) return result;
    if ((result=read_byte(fp, &b)) != 1) return result;
    if ((result=read_byte(fp, &c)) != 1) return result;
    if ((result=read_byte(fp, &d)) != 1) return result;

    *out = (a<<24) | (b<<16) | (c<<8) | (d<<0);
    return 1;
}

static int
read_int32_l(FILE *fp, int *out)
{
    int result;
    unsigned char a, b, c, d;

    if ((result=read_byte(fp, &a)) != 1) return result;
    if ((result=read_byte(fp, &b)) != 1) return result;
    if ((result=read_byte(fp, &c)) != 1) return result;
    if ((result=read_byte(fp, &d)) != 1) return result;

    *out = (d<<24) | (c<<16) | (b<<8) | (a<<0);
    return 1;
}

static int
read_int16_l(FILE *fp, int *out)
{
    int result;
    unsigned char a, b;

    if ((result=read_byte(fp, &a)) != 1) return result;
    if ((result=read_byte(fp, &b)) != 1) return result;

    *out = (b<<8) | (a<<0);
    return 1;
}

static int
read_wave_chunk(struct wave_reader *wr, wave_reader_error *error)
{
    int result;
    int sub1_id, sub1_len, sub2_id, sub2_len, byte_rate, block_align;

    if ((result=read_int32_b(wr->fp, &sub1_id)) != 1) {
        *error = result == 0 ? WR_BAD_CONTENT : WR_IO_ERROR;
        return 0;
    }

    if (sub1_id != FOUR_CC('f','m','t',' ')) {
        *error = WR_BAD_CONTENT;
        return 0;
    }

    if ((result=read_int32_l(wr->fp, &sub1_len)) != 1) {
        *error = result == 0 ? WR_BAD_CONTENT : WR_IO_ERROR;
        return 0;
    }

    if ((result=read_int16_l(wr->fp, &wr->format)) != 1) {
        *error = result == 0 ? WR_BAD_CONTENT : WR_IO_ERROR;
        return 0;
    }

    if ((result=read_int16_l(wr->fp, &wr->num_channels)) != 1) {
        *error = result == 0 ? WR_BAD_CONTENT : WR_IO_ERROR;
        return 0;
    }

    if ((result=read_int32_l(wr->fp, &wr->sample_rate)) != 1) {
        *error = result == 0 ? WR_BAD_CONTENT : WR_IO_ERROR;
        return 0;
    }

    if ((result=read_int32_l(wr->fp, &byte_rate)) != 1) {
        *error = result == 0 ? WR_BAD_CONTENT : WR_IO_ERROR;
        return 0;
    }

    if ((result=read_int16_l(wr->fp, &block_align)) != 1) {
        *error = result == 0 ? WR_BAD_CONTENT : WR_IO_ERROR;
        return 0;
    }

    if ((result=read_int16_l(wr->fp, &wr->sample_bits)) != 1) {
        *error = result == 0 ? WR_BAD_CONTENT : WR_IO_ERROR;
        return 0;
    }

    if ((result=read_int32_b(wr->fp, &sub2_id)) != 1) {
        *error = result == 0 ? WR_BAD_CONTENT : WR_IO_ERROR;
        return 0;
    }

    if (sub2_id != FOUR_CC('d','a','t','a')) {
        *error = WR_BAD_CONTENT;
        return 0;
    }

    if ((result=read_int32_l(wr->fp, &sub2_len)) != 1) {
        *error = result == 0 ? WR_BAD_CONTENT : WR_IO_ERROR;
        return 0;
    }

    wr->num_samples = sub2_len / (wr->num_channels * wr->sample_bits / 8);

    return 1;
}

static int
skip(struct wave_reader *wr, wave_reader_error *error)
{
    int len;

    if (!read_int32_l(wr->fp, &len)) {
        *error = WR_IO_ERROR;
        return 0;
    }

    if (fseek(wr->fp, len, SEEK_CUR) == -1) {
        *error = WR_IO_ERROR;
        return 0;
    }

    return 1;
}

struct wave_reader *
wave_reader_open(const char *filename, wave_reader_error *error)
{
    int root_id, root_len, format_id;
    int continue_reading = 1;
    struct wave_reader *wr = NULL;

    assert(filename != NULL);
    assert(error != NULL);

    wr = (struct wave_reader *)calloc(1, sizeof(struct wave_reader));
    if (!wr) {
        *error = WR_ALLOC_ERROR;
        goto alloc_error;
    }

    wr->fp = fopen(filename, "rb");
    if (!wr->fp) {
        *error = WR_OPEN_ERROR;
        goto open_error;
    }

    if (!read_int32_b(wr->fp, &root_id)) {
        *error = WR_IO_ERROR;
        goto reading_error;
    }

    if (root_id != FOUR_CC('R','I','F','F')) {
        *error = WR_BAD_CONTENT;
        goto reading_error;
    }

    if (!read_int32_l(wr->fp, &root_len)) {
        *error = WR_IO_ERROR;
        goto reading_error;
    }

    while (continue_reading) {
        if (!read_int32_b(wr->fp, &format_id)) {
            *error = WR_IO_ERROR;
            goto reading_error;
        }

        switch (format_id) {
        case FOUR_CC('W','A','V','E'):
            if (!read_wave_chunk(wr, error)) {
                goto reading_error;
            }
            continue_reading = 0;
            break;
        default:
            if (!skip(wr, error)) {
                goto reading_error;
            }
        }
    }

    return wr;

reading_error:
    fclose(wr->fp);
open_error:
    free(wr);
alloc_error:

    return NULL;
}

void
wave_reader_close(struct wave_reader *wr)
{
    if (wr) {
        fclose(wr->fp);
        free(wr);
    }
}

int
wave_reader_get_format(struct wave_reader *wr)
{
    assert(wr != NULL);

    return wr->format;
}

int
wave_reader_get_num_channels(struct wave_reader *wr)
{
    assert(wr != NULL);

    return wr->num_channels;
}

int
wave_reader_get_sample_rate(struct wave_reader *wr)
{
    assert(wr != NULL);

    return wr->sample_rate;
}

int
wave_reader_get_sample_bits(struct wave_reader *wr)
{
    assert(wr != NULL);

    return wr->sample_bits;
}

int
wave_reader_get_num_samples(struct wave_reader *wr)
{
    assert(wr != NULL);

    return wr->num_samples;
}

int
wave_reader_get_samples(struct wave_reader *wr, int n, void *buf)
{
    int ret;

    assert(wr != NULL);
    assert(buf != NULL);

    ret = fread(buf, wr->num_channels * wr->sample_bits / 8, n, wr->fp);
    if (ret < n && ferror(wr->fp)) {
        return -1;
    }

    return ret;
}

