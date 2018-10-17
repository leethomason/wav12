#ifndef WAV_COMPRESSION
#define WAV_COMPRESSION

#include <stdint.h>
#include <assert.h>

namespace wav12 {

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
        virtual uint8_t get8() = 0;
        virtual int32_t size() = 0;
        virtual int32_t pos() = 0;
    };

    class MemStream : public IStream
    {
    public:
        MemStream(const uint8_t* mem, int32_t nBytes) {
            m_mem = mem;
            m_ptr = m_mem;
            m_nBytes = nBytes;
            m_pos = 0;
        }

        uint8_t get8() {
            assert(m_ptr < m_mem + m_nBytes);
            return *m_ptr++;
        }

        int32_t size() const { return m_nBytes; }
        int32_t pos() { return m_pos; }

    private:
        const uint8_t* m_mem;
        const uint8_t* m_ptr;
        int32_t m_nBytes;
        int32_t m_pos;
    };

    class Expander
    {
    public:
        Expander(IStream* compressed, int32_t nCompressed, int32_t nSamples, int shiftBits);

        // Expand to the target buffer with a length of nTarget.
        // Returns number of samples actually expanded.
        int32_t expand(int16_t* target, int nTarget);

        bool done() const;
    private:
        IStream* m_compressed;
        int32_t m_nCompressed;
        int32_t m_nSamples;
    };
}
#endif

