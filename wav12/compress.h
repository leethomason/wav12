#ifndef WAV_COMPRESSION
#define WAV_COMPRESSION

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

    class IStream
    {
    public:
        virtual void get(uint8_t* target, int n) = 0;
        virtual int32_t size() = 0;
        virtual int32_t pos() = 0;
        virtual int32_t remaining() = 0;
    };

    class MemStream : public IStream
    {
    public:
        MemStream(const uint8_t* mem, int32_t nBytes) {
            m_mem = mem;
            m_ptr = m_mem;
            m_nBytes = nBytes;
        }

        void get(uint8_t* target, int n) {
            assert(m_ptr + n < m_mem + m_nBytes);
            memcpy(target, m_ptr, n);
            m_ptr += n;
        }

        int32_t size() const { return m_nBytes; }
        int32_t pos() { return m_ptr - m_mem; }

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
        int32_t expand(int16_t* target, int nTarget);

        bool done() const { return m_nSamples == m_pos; }

    private:
        IStream* m_compressed;
        int32_t m_nSamples;
        int32_t m_pos;
        int m_shiftBits;

        static const int BUFSIZE = 16;
        uint8_t m_buffer[BUFSIZE];
        BitReader m_bitReader;
    };
}
#endif

