#include "bits.h"

void BitAccum::push(uint32_t v, int nBits) {
    assert(nUsed + nBits <= 32);
    assert(nBits == 32 || (v < (1U << nBits)));
    value <<= nBits;
    value |= v;
    nUsed += nBits;
}

uint32_t BitAccum::pop(int nBits) {
    assert(nBits <= nUsed);
    uint32_t result = value >> (nUsed - nBits);
    nUsed -= nBits;
    if (nBits == 32) {
        value = 0;
    }
    else {
        uint32_t mask = (1 << nBits) - 1;
        mask <<= nUsed;
        value &= ~mask;
    }
    return result;
}


void BitWriter::write(uint32_t value, int nBits) {
    assert(nBits == 32 || (value < (1U << nBits)));
    while (nBits) {
        if (accum.bitsUsed() == 8) {
            *target = uint8_t(accum.get());
            target++;
            assert(target < start + size);
            accum.clear();
        }
        int avail = 8 - accum.bitsUsed();
        if (avail >= nBits) {
            accum.push(value, nBits);
            nBits = 0;
        }
        else {
            int n = (nBits - avail);
            uint32_t high = value >> n;
            accum.push(high, avail);
            nBits -= avail;
            value &= ((1 << nBits) - 1);
        }
    }
}

void BitWriter::close() 
{
    if (!accum.empty()) {
        accum.push(0, 8 - accum.bitsUsed());
        *target = uint8_t(accum.get());
    }
}


uint32_t BitReader::read(int nBits) {
    assert(nBits <= 32);
    uint32_t result = 0;

    while (nBits) {
        if (accum.bitsUsed() == 0) {
            assert(accum.empty());
            accum.clear();

            uint8_t val = 0;
            if (stream) {
                val = stream->get();
            }
            else {
                assert(src < start + nBytes);
                val = *src;
                ++src;
            }
            accum.set(val, 8);
        }
        if (nBits <= accum.bitsUsed()) {
            result <<= nBits;
            result |= accum.pop(nBits);
            nBits = 0;
        }
        else {
            int n = accum.bitsUsed();
            result <<= n;
            result |= accum.pop(n);
            nBits -= n;
        }
    }
    return result;
}



#define TEST_TRUE(x) \
    if (!(x)) return false;

/*static*/ bool BitAccum::Test()
{
    {
        BitAccum ba;
        TEST_TRUE(ba.empty());
        ba.push(31, 5);
        TEST_TRUE(!ba.empty());
        int32_t v = ba.pop(5);
        TEST_TRUE(v == 31);
        TEST_TRUE(ba.empty());
        TEST_TRUE(ba.get() == 0);
        
        ba.push(0xaa, 8);
        ba.push(0xbb, 8);
        ba.push(0xcc, 8);
        ba.push(0xdd, 8);

        TEST_TRUE(ba.pop(8) == 0xaa);
        TEST_TRUE(ba.pop(8) == 0xbb);
        TEST_TRUE(ba.pop(8) == 0xcc);
        TEST_TRUE(ba.pop(8) == 0xdd);

        ba.push(1, 3);
        ba.push(10, 4);
        ba.push(0xff, 10);

        TEST_TRUE(ba.pop(3) == 1);
        TEST_TRUE(ba.pop(4) == 10);
        TEST_TRUE(ba.pop(10) == 0xff);
        TEST_TRUE(ba.empty());
    }
    {
        BitAccum ba;
        ba.push(0xf0f0f0f0, 32);
        TEST_TRUE(ba.full());
        TEST_TRUE(ba.pop(32) == 0xf0f0f0f0);
    }

    return true;
}


/*static*/ bool BitReader::TestReaderAndWriter()
{
    static const int NUM_CLEAR = 32;
    uint8_t clearText[NUM_CLEAR];

    for (int i = 0; i < NUM_CLEAR; i++) {
        clearText[i] = i;
    }

    uint8_t store[NUM_CLEAR] = { 0 };
    BitWriter writer(store, NUM_CLEAR);
    for (int i = 0; i < NUM_CLEAR; ++i) {
        writer.write(clearText[i], 5 + i % 3);
    }
    writer.close();

    BitReader reader(store, NUM_CLEAR);
    for (int i = 0; i < NUM_CLEAR; ++i) {
        uint32_t v = reader.read(5 + i % 3);
        TEST_TRUE(v == i);
    }
    return true;
}

