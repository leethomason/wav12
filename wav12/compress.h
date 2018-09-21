#ifndef WAV_COMPRESSION
#define WAV_COMPRESSION

#include <stdint.h>

void linearCompress(const int16_t* data, int32_t nSamples,
    uint8_t** compressed, int32_t* nCompressed,
    int shiftBits = 0,
    int* buckets = 0);

void linearExpand(const uint8_t* compressed, int nCompressed,
    int16_t* data, int32_t nSamples,
    int shiftBits = 0);

#endif

