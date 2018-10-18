#ifndef WAV_COMPRESSION
#define WAV_COMPRESSION

#include "wav12stream.h"
#include "bits.h"

#include <stdint.h>
#include <assert.h>
#include <string.h>

namespace wav12 {

    template<class T>
    T wMin(const T& a, const T& b) { return a < b ? a : b; }

    struct CompressStat
    {
        // 0: 0-1
        // 1: 2-3
        // 2: 4-7 etc.
        int buckets[16] = { 0 };
        int edgeWrites = 0;
        bool shift = 0;

        void consolePrint() const;
    };

    void linearCompress(const int16_t* data, int32_t nSamples,
        uint8_t** compressed, int32_t* nCompressed,
        int shiftBits = 0,
        CompressStat* stats = 0);

    void linearExpand(const uint8_t* compressed, int32_t nCompressed,
        int16_t* data, int32_t nSamples,
        int shiftBits = 0);

    class MemStream : public wav12::IStream
    {
    public:
        MemStream(const uint8_t* mem, int32_t nBytes) {
            m_mem = mem;
            m_ptr = m_mem;
            m_nBytes = nBytes;
        }

        uint8_t get() {
            assert(m_ptr < m_mem + m_nBytes);
            return *m_ptr++;
        }

        int32_t size() const { return m_nBytes; }
        int32_t pos() { return int32_t(m_ptr - m_mem); }

    private:
        const uint8_t* m_mem;
        const uint8_t* m_ptr;
        int32_t m_nBytes;
    };

    class Expander
    {
    public:
        Expander(IStream* compressed, int32_t nSamples, int shiftBits);

        // Expand to the target buffer with a length of nTarget.
        // Returns number of samples actually expanded.
        void expand(int16_t* target, int nTarget);

        // Does a stereo expansion (both channels the same, of course)
        // to 32 bits. nTarget is the samples per channel.
        // Volume max is 65536
        void expand2(int32_t* target, int nTarget, int32_t volume);

        bool done() const { return m_nSamples == m_pos; }

    private:
        IStream* m_compressed;
        int32_t m_nSamples;
        int32_t m_pos;
        int32_t m_prev1, m_prev2;
        int m_shiftBits;
        BitReader m_bitReader;
    };
}
#endif

