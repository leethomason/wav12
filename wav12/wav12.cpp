#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <chrono>
#include <stdio.h>
#include <iostream>

extern "C" {
#include "../wave_reader.h"
}

#include "compress.h"
#include "bits.h"


using namespace wav12;

int main(int argc, const char* argv[])
{
    bool testOkay = BitAccum::Test();
    assert(testOkay);
    testOkay = BitReader::TestReaderAndWriter();
    assert(testOkay);

    if (argc < 2) {
        printf("Usage: wav12 filename <outfile> <options>\n");
        printf("Analyze an audio file. Will create <outfile> if specified.\n");
        printf("-1 1 bit loss, -2 2 bit loss.\n");
        printf("-r never compress\n");
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
        printf("Input must be 22050 Hz Mono\n");
        return 2;
    }
    FILE* outputFP = 0;
    if (argc >= 3) {
        outputFP = fopen(argv[2], "wb");
        if (!outputFP) {
            printf("Could not open '%s'\n", argv[2]);
            return 2;
        }
    }
    int shift = 0;
    bool raw = false;

    for (int i = 3; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == '1') shift = 1;
            if (argv[i][1] == '2') shift = 2;
            if (argv[i][1] == 'r') raw = true;
        }
    }

    int16_t* data = new int16_t[nSamples];
    wave_reader_get_samples(wr, nSamples, data);

    CompressStat stat;
    uint8_t* compressed = 0;
    int nCompressed = 0;
    std::chrono::time_point<std::chrono::steady_clock> startCompress, endCompress, startExpand, endExpand, startChunk, endChunk;

    if (!raw) {
        startCompress = std::chrono::high_resolution_clock::now();
        wav12::linearCompress(data, nSamples, &compressed, &nCompressed, shift, &stat);
        endCompress = std::chrono::high_resolution_clock::now();

        int bytesOriginal = nSamples * 2;
        printf("bytes orig=%d bytes comp=%d ratio=%.2f\n", bytesOriginal, nCompressed,
            float(nCompressed) / float(bytesOriginal));
        if (nCompressed >= bytesOriginal) {
            printf("Didn't compress.\n");
            raw = true;
        }

        if (argc == 2) {
            stat.consolePrint();
        }
    }

    // Test code.
    if (argc == 2 && shift == 0) {
        int16_t* newData = new int16_t[nSamples];
        startExpand = std::chrono::high_resolution_clock::now();
        wav12::linearExpand(compressed, nCompressed, newData, nSamples, 0);
        endExpand = std::chrono::high_resolution_clock::now();

        bool okay = true;
        for (int i = 0; i < nSamples; i++) {
            assert(newData[i] == data[i]);
            if (newData[i] != data[i])
                okay = false;
        }
        printf("Test full stream: %s\n", okay ? "PASS" : "FAIL");

        int16_t buffer[17];
        MemStream memStream(compressed, nCompressed);
        Expander expander(&memStream, nSamples, 0);

        okay = true;
        startChunk = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < nSamples; i += 17) {
            int n = wMin(17, nSamples - i);
            expander.expand(buffer, n);
            for (int j = i; j < i + n; ++j) {
                assert(buffer[j - i] == data[j]);
                if (newData[i] != data[i])
                    okay = false;
            }
        }
        endChunk = std::chrono::high_resolution_clock::now();
        printf("Test chunk stream: %s\n", okay ? "PASS" : "FAIL");
        delete[] newData;

        std::cout << "compress: " << std::chrono::duration_cast<std::chrono::microseconds>(endCompress - startCompress).count() << "\n";
        std::cout << "expand: " << std::chrono::duration_cast<std::chrono::microseconds>(endExpand - startExpand).count() << "\n";
        std::cout << "chunk + verify: " << std::chrono::duration_cast<std::chrono::microseconds>(endChunk - startChunk).count() << "\n";

    }

    if (outputFP) {
        uint8_t compressed8 = raw ? 0 : 1;
        uint8_t shift8 = uint8_t(shift);
        uint32_t samples32 = uint32_t(nSamples);

        fwrite(&compressed8, 1, 1, outputFP);
        fwrite(&shift8, 1, 1, outputFP);
        fwrite(&samples32, 4, 1, outputFP);

        if (raw)
            fwrite(data, 2, nSamples, outputFP);
        else
            fwrite(compressed, 1, nCompressed, outputFP);

        fclose(outputFP);
    }

    delete[] compressed;
    delete[] data;

    wave_reader_close(wr);
    return 0;
}

