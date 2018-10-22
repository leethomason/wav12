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
    int32_t prev3 = 0;
    int32_t prev2 = 0;
    int32_t prev1 = 0;
    for (int i = 0; i < nSamples; i++) {
        //int32_t guess = prev1 + prev1 - prev2;                                                // 0.67 on the test set
        //int32_t guess = prev1 + (prev1 - prev2) + ((prev1 - prev2) - (prev2 - prev3)) / 2;    // 0.65 1614 (correct)
        int32_t guess = 3 * prev1 - 3 * prev2 + prev3;                                          // 0.65 1616 simplified (no division)

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

        prev3 = prev2;
        prev2 = prev1;
        prev1 = sample;
    }
    writer.close();
    *nCompressed = writer.length();
}

template<typename T, int CHANNELS>
void innerLinearExpand(BitReader& reader, wav12::Context& context,
    int shiftBits, T* target, int n, T volume)
{
    for (int i = 0; i < n; ++i) {
        int32_t guess = 3 * context.prev1 - 3 * context.prev2 + context.prev3;

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

        context.prev3 = context.prev2;
        context.prev2 = context.prev1;
        context.prev1 = sample;
    }
}


void linearExpand(const uint8_t* compressed, int nCompressed,
    int16_t* data, int32_t nSamples,
    int shiftBits)
{
    BitReader reader(compressed, nCompressed);
    Context context;
    innerLinearExpand<int16_t, 1>(reader, context, shiftBits, data, nSamples, 1);
}


Expander::Expander()
{
    init(0, 0, 0, 0);
}


Expander::Expander(IStream* stream, uint32_t nSamples, int format, int shiftBits)
{
    init(stream, nSamples, format, shiftBits);
}


void Expander::init(IStream* stream, uint32_t nSamples, int format, int shiftBits)
{
    m_stream = stream;
    m_nSamples = nSamples;
    m_pos = 0;
    m_format = format;
    m_shiftBits = shiftBits;
    m_bitReader.init(stream);
}


void Expander::expand(int16_t* target, uint32_t nTarget)
{
    assert(nTarget <= (m_nSamples - m_pos));
    m_pos += nTarget;

    if (m_format == 0) {
        while (nTarget--) {
            *target++ = m_stream->get16();
        }
    }
    else {
        innerLinearExpand<int16_t, 1>(m_bitReader, m_context, m_shiftBits, target, nTarget, 1);
    }
}


void Expander::expand2(int32_t* target, uint32_t nTarget, int32_t volume)
{
    m_pos += nTarget;
    if (m_format == 0) {
        while (nTarget--) {
            int32_t v = m_stream->get16() * volume;
            *target++ = v;
            *target++ = v;
        }
    }
    else {
        innerLinearExpand<int32_t, 2>(m_bitReader, m_context, m_shiftBits, target, nTarget, volume);
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
    printf("shift bits=%d\n", shift);
}
