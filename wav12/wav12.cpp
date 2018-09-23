#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>

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

    if (argc < 2) {
        printf("Usage: wav12 filename <outfile>\n");
        printf("Analyze an audio file. Will create <outfile> if specified.\n");
        return 1;
    }
    printf("input=%s\n", argv[1]);

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
    //static const int SHIFT = 1;
    //static const int SHIFT_MASK = ~1;
    static const int SHIFT = 0;
    static const int SHIFT_MASK = 0xffffffff;

    CompressStat stat;
    uint8_t* compressed = 0;
    int nCompressed = 0;
    linearCompress(data, nSamples, &compressed, &nCompressed, SHIFT, &stat);

    if (argc == 2) {
        stat.consolePrint();
    }
    int bytesOriginal = nSamples * 2;
    printf("bytes orig=%d bytes comp=%d ratio=%.2f\n", bytesOriginal, nCompressed,
        float(nCompressed) / float(bytesOriginal));

    int16_t* newData = new int16_t[nSamples];
    linearExpand(compressed, nCompressed, newData, nSamples, SHIFT);

    for (int i = 0; i < nSamples; i++) {
        assert(newData[i] == (data[i] & SHIFT_MASK));
    }

    if (argc == 3) {
        FILE* fout = fopen(argv[2], "wb");
        if (!fout) return 3;
        fwrite(compressed, 1, nCompressed, fout);
        fclose(fout);
    }

    delete[] compressed;
    delete[] data;
    delete[] newData;

    wave_reader_close(wr);
    return 0;
}

