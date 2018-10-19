#include <assert.h>
#include <string.h>

#include "memimage.h"

MemImage::MemImage()
{
    assert(sizeof(*this) == 1024);
    memset(dir, 0, sizeof(MemUnit) * NUM_DIR);
}

