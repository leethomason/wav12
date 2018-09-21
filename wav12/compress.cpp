#include "compress.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "bits.h"

void linearCompress(const int16_t* data, int32_t nSamples, 
    uint8_t** compressed, int32_t* nCompressed, 
    int shiftBits,
    int* buckets)
{
    *compressed = new uint8_t[nSamples * 2];
    BitWriter writer(*compressed, nSamples * 2);

    int16_t prev2 = 0;
    int16_t prev1 = 0;
    for (int i = 0; i < nSamples; i++) {
        int32_t guess = prev1 + (prev1 - prev2);
        int32_t sample = data[i] >> shiftBits;
        int32_t delta = sample - guess;
        int sign = 1;
        if (delta < 0) {
            sign = 0;
            delta *= -1;
        }

        int bits = BitAccum::bitsNeeded(delta);
        assert(bits > 0 && bits <= 15);
        writer.write(bits, 4);
        writer.write(sign, 1);
        writer.write(delta, bits);

        if (buckets) {
            buckets[bits] += 1;
        }

        prev2 = prev1;
        prev1 = sample;
    }
    writer.close();
    *nCompressed = writer.length();
}


void linearExpand(const uint8_t* compressed, int nCompressed,
    int16_t* data, int32_t nSamples,
    int shiftBits)
{
    BitReader reader(compressed, nCompressed);

    int16_t prev2 = 0;
    int16_t prev1 = 0;
    for (int i = 0; i < nSamples; ++i) {
        int32_t guess = prev1 + (prev1 - prev2);
        uint32_t nBits = reader.read(4);
        uint32_t sign = reader.read(1);
        uint32_t error = reader.read(nBits);

        int16_t delta = int16_t(error) * (sign == 1 ? 1 : -1);
        int16_t sample = guess + delta;

        data[i] = sample << shiftBits;

        prev2 = prev1;
        prev1 = sample;
    }
}

