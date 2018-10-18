#ifndef WAV12_STREAM_INCLUDED
#define WAV12_STREAM_INCLUDED

#include <stdint.h>

namespace wav12 {

    class IStream {
    public:
        virtual uint8_t get() = 0;
        virtual int32_t size() const = 0;
        virtual int32_t pos() = 0;
    };

}


#endif // WAV12_STREAM_INCLUDED
