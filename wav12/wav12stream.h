#ifndef WAV12_STREAM_INCLUDED
#define WAV12_STREAM_INCLUDED

#include <stdint.h>

namespace wav12 {

    class IStream {
    public:
        virtual uint8_t get() = 0;
        virtual int16_t get16() = 0;
    };

}


#endif // WAV12_STREAM_INCLUDED
