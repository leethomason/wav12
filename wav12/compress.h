#ifndef WAV_COMPRESSION
#define WAV_COMPRESSION

#include <stdint.h>

struct CompressStat
{
    // 0: 0-1
    // 1: 2-3
    // 2: 4-7 etc.
    int buckets[16] = { 0 };
    int edgeWrites = 0;

    void consolePrint() const;
};

void linearCompress(const int16_t* data, int32_t nSamples,
    uint8_t** compressed, int32_t* nCompressed,
    int shiftBits = 0,
    CompressStat* stats = 0);

void linearExpand(const uint8_t* compressed, int nCompressed,
    int16_t* data, int32_t nSamples,
    int shiftBits = 0);

#endif

