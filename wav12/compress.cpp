#include "compress.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include "bits.h"

using namespace wav12;

void wav12::linearCompress(const int16_t* data, int32_t nSamples, 
    uint8_t** compressed, int32_t* nCompressed, 
    int shiftBits,
    CompressStat* stats)
{
    const int SIZE = nSamples * 4;      // *4 is double size
    *compressed = new uint8_t[SIZE];
    BitWriter writer(*compressed, SIZE);

    if (stats) 
        stats->shift = shiftBits;

    int32_t prev2 = 0;
    int32_t prev1 = 0;
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
        assert(bits > 0);
        if (bits > 15) {
            // Edge case: it's possible to have a delta that needs 16 bits OR
            // the guess has gone "out of range". In either case, all bits set
            // indicates just read a value, not a delta.
            writer.write(15, 4);
            writer.write(uint16_t(sample), 16);

            if (stats) 
                stats->edgeWrites += 1;
        }
        else {
            writer.write(bits - 1, 4); // Bits can be [1, 15], write out [0, 14]
            writer.write(sign, 1);
            writer.write(delta, bits);

            if (stats) 
                stats->buckets[bits - 1] += 1;
        }

        prev2 = prev1;
        prev1 = sample;
    }
    writer.close();
    *nCompressed = writer.length();
}

template<typename T, int CHANNELS>
void innerLinearExpand(BitReader& reader, int32_t& prev1, int32_t& prev2, 
    int shiftBits, T* target, int n, T volume)
{
    for (int i = 0; i < n; ++i) {
        int32_t guess = prev1 + (prev1 - prev2);

        uint32_t nBits = reader.read(4);
        int16_t sample = 0;
        if (nBits == 15) {
            sample = int16_t(reader.read(16));
        }
        else {
            nBits++;
            assert(nBits > 0 && nBits < 16);
            uint32_t sign = reader.read(1);
            uint32_t scalar = reader.read(nBits);

            int16_t delta = int16_t(scalar) * (sign == 1 ? 1 : -1);
            sample = guess + delta;
        }
        for (int c = 0; c < CHANNELS; ++c) {
            *target = (sample << shiftBits) * volume;
            ++target;
        }

        prev2 = prev1;
        prev1 = sample;
    }
}


void wav12::linearExpand(const uint8_t* compressed, int nCompressed,
    int16_t* data, int32_t nSamples,
    int shiftBits)
{
    BitReader reader(compressed, nCompressed);
    int32_t prev2 = 0;
    int32_t prev1 = 0;
    innerLinearExpand<int16_t, 1>(reader, prev1, prev2, shiftBits, data, nSamples, 1);
}


Expander::Expander(IStream* compressed, int32_t nSamples, int shiftBits) :
    m_bitReader(compressed)
{
    m_compressed = compressed;
    m_nSamples = nSamples;
    m_pos = 0;
    m_prev1 = 0;
    m_prev2 = 0;
    m_shiftBits = shiftBits;
}


void Expander::expand(int16_t* target, int nTarget)
{
    assert(nTarget <= (m_nSamples - m_pos));
    innerLinearExpand<int16_t, 1>(m_bitReader, m_prev1, m_prev2, m_shiftBits, target, nTarget, 1);
    m_pos += nTarget;
}


void Expander::expand2(int32_t* target, int nTarget, int32_t volume)
{
    innerLinearExpand<int32_t, 2>(m_bitReader, m_prev1, m_prev2, m_shiftBits, target, nTarget, volume);
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
    printf("shift bits=%d\n", shift);
}


