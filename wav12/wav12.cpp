#include <stdint.h>

extern "C" {
#include "../wave_reader.h"
}
#include <stdio.h>
#include "compress.h"
#include "bits.h"

#include <stdio.h>
#include <assert.h>

int main(int argc, const char* argv[])
{
    bool testOkay = BitAccum::Test();
    assert(testOkay);
    testOkay = BitReader::TestReaderAndWriter();
    assert(testOkay);

    printf("argc=%d\n", argc);
    if (argc < 2) {
        printf("Usage: wav12 filename <outfile>\n");
        printf("Analyze an audio file. Will create <outfile> if specified.\n");
        return 1;
    }

    wave_reader_error error;
    wave_reader* wr = wave_reader_open(argv[1], &error);

    int format = wave_reader_get_format(wr);
    int nChannels = wave_reader_get_num_channels(wr);
    int rate = wave_reader_get_sample_rate(wr);
    int nSamples = wave_reader_get_num_samples(wr);

    printf("Format=%d channels=%d rate=%d\n", format, nChannels, rate);

    if (format != 1
        || nChannels != 1
        || rate != 22050)
    {
        return 2;
    }
    int16_t* data = new int16_t[nSamples];
    wave_reader_get_samples(wr, nSamples, data);
    static const int SHIFT = 1;
    static const int SHIFT_MASK = ~1;

    int buckets[16] = { 0 };
    uint8_t* compressed = 0;
    int nCompressed = 0;
    linearCompress(data, nSamples, &compressed, &nCompressed, SHIFT, buckets);

    for (int b = 0; b < 16; ++b) {
        printf("bucket[%8d]=%d\n", (1 << (b+1)), buckets[b]);
    }
    int bytesOriginal = nSamples * 2;
    printf("bytes orig=%d bytes comp=%d ratio=%.2f\n", bytesOriginal, nCompressed,
        float(nCompressed) / float(bytesOriginal));

    int16_t* newData = new int16_t[nSamples];
    linearExpand(compressed, nCompressed, newData, nSamples, SHIFT);

    for (int i = 0; i < nSamples; i++) {
        assert(newData[i] == (data[i] & SHIFT_MASK));
    }

    delete[] compressed;
    delete[] data;
    delete[] newData;
    return 0;

    /*
    if (argc == 3) {
        FILE* fp = fopen(argv[2], "wb");

        uint16_t* data = new uint16_t[nSamples];
        wave_reader_get_samples(wr, nSamples, data);

        for (int i = 0; i < nSamples; i += 2) {
            bool has2nd = (i + 1) < nSamples;
            uint16_t s0 = data[i];
            uint16_t s1 = has2nd ? data[i + 1] : 0;

            // Shift down to 12 bits.
            s0 >>= 4;
            s1 >>= 4;

            fputc(s0 >> 8, fp);                 // s0 bits 8-15
            uint16_t sub0 = (s0 & 0xf) >> 4;    // s0 bits 4-7, s1 bits 
            uint16_t sub1 = s1 >> 12;
            fputc((sub0 << 4) | sub1, fp);

            if (has2nd) {
                fputc((s1 & 0xf) >> 4, fp);
            }
        }

        fclose(fp);
    }
    */
    wave_reader_close(wr);
    return 0;
}

