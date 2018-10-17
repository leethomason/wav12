#ifndef BITS_INCLUDED
#define BITS_INCLUDED

#include <stdint.h>
#include <assert.h>

class BitAccum
{
public:
    BitAccum() { value = 0; nUsed = 0; }

    bool empty() const { return nUsed == 0; }
    int bitsUsed() const { return nUsed; }
    bool full() const { return nUsed == 32; }
    void clear() { value = 0; nUsed = 0; }
    uint32_t get() const { return value; }
    void set(uint32_t val, int nBits) { this->value = val; this->nUsed = nBits; }

    void push(uint32_t v, int nBits);
    uint32_t pop(int nBits);

    static int bitsNeeded(uint32_t v) {
        for (int i = 1; i < 32; ++i) {
            if ((1U << i) > v)
                return i;
        }
        return 32;
    }

    static bool Test();

private:
    int nUsed;
    uint32_t value;
};

class BitWriter
{
public:
    BitWriter(uint8_t* data, int n) {
        this->target = data;
        this->start = data;
        this->size = n;
    }

    void write(uint32_t value, int nBits);
    void close();
    int length() const { return int(target - start + (accum.empty() ? 0 : 1)); }

private:
    const uint8_t* start = 0;
    uint8_t* target = 0;
    int size = 0;

    BitAccum accum;
};

class BitReader
{
public:
    BitReader(const uint8_t* src, int n) {
        this->src = src;
        this->start = src;
        this->nBytes = n;
    }

    uint32_t read(int nBits);

    const uint8_t* srcPtr() const { return src; }

    void attach(const uint8_t* newSrc, int newN) {
        src = newSrc;
        start = newSrc;
        nBytes = newN;
    }

    static bool TestReaderAndWriter();

private:
    const uint8_t* src = 0;
    const uint8_t* start = 0;   // only used for error checking.
    int nBytes = 0;             // only used for error checking.
    BitAccum accum;
};


#endif // BITS_INCLUDED
