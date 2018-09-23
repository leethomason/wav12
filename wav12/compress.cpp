#include "compress.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include "bits.h"

static const int32_t A = 1;
static const int32_t B = 1;

void linearCompress(const int16_t* data, int32_t nSamples, 
    uint8_t** compressed, int32_t* nCompressed, 
    int shiftBits,
    CompressStat* stats)
{
    static const int SIZE = nSamples * 3;
    *compressed = new uint8_t[SIZE];
    BitWriter writer(*compressed, SIZE);

    int16_t prev2 = 0;
    int16_t prev1 = 0;
    for (int i = 0; i < nSamples; i++) {
        int32_t guess = prev1 + (prev1 - prev2) * A / B;
        int32_t sample = data[i] >> shiftBits;
        int32_t delta = sample - guess;
        int sign = 1;
        if (delta < 0) {
            sign = 0;
            delta *= -1;
        }

        int bits = BitAccum::bitsNeeded(delta);
        assert(bits > 0);
        if (bits > 15) {
            // Edge case: it's possible to have a delta that needs 16 bits OR
            // the guess has gone "out of range". In either case, all bits set
            // indicates just read a value, not a delta.
            writer.write(15, 4);
            writer.write(uint16_t(sample), 16);

            if (stats) {
                stats->edgeWrites += 1;
            }
        }
        else {
            writer.write(bits - 1, 4); // Bits can be [1, 15], write out [0, 14]
            writer.write(sign, 1);
            writer.write(delta, bits);

            if (stats) {
                stats->buckets[bits - 1] += 1;
            }
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
        int32_t guess = prev1 + (prev1 - prev2) * A / B;
        uint32_t nBits = reader.read(4);
        int16_t sample = 0;
        if (nBits == 15) {
            sample = int16_t(reader.read(16));
        }
        else {
            nBits++;
            assert(nBits > 0 && nBits < 16);
            uint32_t sign = reader.read(1);
            uint32_t error = reader.read(nBits);

            int16_t delta = int16_t(error) * (sign == 1 ? 1 : -1);
            sample = guess + delta;
        }
        data[i] = sample << shiftBits;

        prev2 = prev1;
        prev1 = sample;
    }
}


void CompressStat::consolePrint() const
{
    for (int b = 0; b < 16; ++b) {
        printf("bucket[%02d][%6d - %6d)=%d\n", 
            b,
            b == 0 ? 0 : (1 << b),
            (1 << (b+1)),
            buckets[b]);
    }
    printf("edgeWrites=%d\n", edgeWrites);
}


