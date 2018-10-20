#ifndef MEMORY_IMAGE_INCLUDE
#define MEMORY_IMAGE_INCLUDE

#include <vector>
#include <stdint.h>

/*
    Directory (16 bytes):
        8 bytes:     name
        uint32_t:    offset
        uint32_t:    count

        1k:
        4 directories
        60 files

*/

struct MemUnit {
    static const int NAME_LEN = 8;

    char name[NAME_LEN];   // NOT null terminated.
    uint32_t offset;
    uint32_t size;
};


struct MemImage {
    static const int NUM_DIR = 4;
    static const int NUM_FILES = 60;

    MemUnit dir[NUM_DIR];
    MemUnit file[NUM_FILES];
};

class MemImageUtil : public MemImage
{
public:
    MemImageUtil();
    void addDir(const char* name);
    void addFile(const char* name, void* data, int size);
    void dumpConsole();

private:
    std::vector<uint8_t> dataVec;
    int currentDir = -1;
    int currentFile = -1;
};

#endif // MEMORY_IMAGE_INCLUDE
