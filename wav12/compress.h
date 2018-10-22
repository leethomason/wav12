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

    struct Wav12Header
    {
        char id[4];             // 'wv12'
        uint32_t lenInBytes;    // after header, compressed size
        uint32_t nSamples;
        uint8_t  format;        // 0 uncompressed, 1 compressed
        uint8_t  shiftBits;     // only if compressed
        uint8_t  unused[2];
    };

    struct CompressStat
    {
        // 0: 0-1
        // 1: 2-3
        // 2: 4-7 etc.
        int buckets[16] = { 0 };
        int edgeWrites = 0;
        int shift = 0;

        void consolePrint() const;
    };

    struct Context
    {
        uint32_t prev1 = 0;
        uint32_t prev2 = 0;
        uint32_t prev3 = 0;
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

        int16_t get16() {
            assert((m_nBytes & 1) == 0);
            assert(m_ptr + 1 < m_mem + m_nBytes);
            uint16_t v = m_ptr[0] + m_ptr[1] * 256; // prevent un-aligned reads on the M0.
            m_ptr += 2;
            return (int16_t)v;
        }

        int32_t size() const { return m_nBytes; }
        int32_t pos() { return int32_t(m_ptr - m_mem); }

    private:
        const uint8_t* m_mem;
        const uint8_t* m_ptr;
        int32_t m_nBytes;
    };

    
    class ChunkStream : public wav12::IStream
    {
    public:
        ChunkStream(uint8_t* subBuffer, int subBufferSize) {
            m_subBuffer = subBuffer;
            m_subBufferSize = subBufferSize;
            m_subBufferPos = m_subBufferSize;   // buffer is empty
        }
        
        uint8_t get() {
            if (m_subBufferPos == m_subBufferSize) fillSubBuffer();
            return m_subBuffer[m_subBufferPos++];
        }

        int16_t get16() {
            if (m_subBufferPos == m_subBufferSize) fillSubBuffer();
            uint16_t v = m_subBuffer[m_subBufferPos] + m_subBuffer[m_subBufferPos + 1] * 256;
            m_subBufferPos += 2;
            return (int16_t)v;
        }

        virtual void fillSubBuffer() = 0;

    protected:
        uint8_t* m_subBuffer;
        int m_subBufferSize;
        int m_subBufferPos;
    };


    class Expander
    {
    public:
        Expander();
        Expander(IStream* stream, uint32_t nSamples, int format, int shiftBits);
        void init(IStream* stream, uint32_t nSamples, int format, int shiftBits);

        // Expand to the target buffer with a length of nTarget.
        // Returns number of samples actually expanded.
        void expand(int16_t* target, uint32_t nTarget);

        // Does a stereo expansion (both channels the same, of course)
        // to 32 bits. nTarget is the samples per channel.
        // Volume max is 65536
        void expand2(int32_t* target, uint32_t nTarget, int32_t volume);

        bool done() const { return m_nSamples == m_pos; }
        
        uint32_t samples() const { return m_nSamples; }
        uint32_t pos() const     { return m_pos; }

    private:
        IStream* m_stream;
        uint32_t m_nSamples;
        uint32_t m_pos;
        Context m_context;
        int m_format;
        int m_shiftBits;
        BitReader m_bitReader;
    };
}
#endif

