#include <assert.h>
#include <string.h>

#include "memimage.h"
#include "./wav12/compress.h"

MemImageUtil::MemImageUtil()
{
    assert(sizeof(MemImage) == 1024);
    memset(dir, 0, sizeof(MemUnit) * NUM_DIR);
    memset(file, 0, sizeof(MemUnit) * NUM_FILES);
}


void MemImageUtil::addDir(const char* name)
{
    currentDir++;
    assert(currentDir < NUM_DIR);
    strncpy(dir[currentDir].name, name, MemUnit::NAME_LEN);
    dir[currentDir].offset = currentFile + 1;
}


void MemImageUtil::addFile(const char* name, void* data, int size)
{
    assert(currentDir >= 0);
    currentFile++;
    assert(currentFile < NUM_FILES);
    
    dir[currentDir].size += 1;
    strncpy(file[currentFile].name, name, MemUnit::NAME_LEN);
    file[currentFile].offset = uint32_t(sizeof(MemImage) + dataVec.size());
    file[currentFile].size = size;

    size_t t = dataVec.size();
    dataVec.resize(dataVec.size() + size);
    memcpy(&dataVec[t], data, size);
}


void MemImageUtil::dumpConsole()
{
    char buf[9] = { 0 };
    uint32_t totalUncompressed = 0, totalSize = 0;

    for (int d = 0; d < NUM_DIR; ++d) {
        if (dir[d].name[0]) {
            strcpy(buf, dir[d].name);
            printf("Dir: %s\n", buf);
             
            for (unsigned f = 0; f < dir[d].size; ++f) {
                const MemUnit& fileUnit = file[dir[d].offset + f];
                std::string sName;
                sName.append(fileUnit.name, fileUnit.name + MemUnit::NAME_LEN);

                const wav12::Wav12Header* header =
                    (const wav12::Wav12Header*)(&dataVec[fileUnit.offset - sizeof(MemImage)]);

                printf("   %8s at %8d size=%6d comp=%d ratio=%5.2f shift=%d\n", 
                    sName.c_str(), 
                    fileUnit.offset, fileUnit.size,
                    header->format,
                    float(header->lenInBytes) / (float)(header->nSamples*2),
                    header->shiftBits);

                totalUncompressed += header->nSamples * 2;
                totalSize += header->lenInBytes;

                //const wav12::Wav12Header* header = (const wav12::Wav12Header*)(&buf[dir[d].offset]);
                //printf("    %8s compressed=%d\n", header.)
            }
        }
    }
    size_t totalImageSize = sizeof(MemImage) + dataVec.size();
    printf("Overall ratio=%5.2f\n", (float)totalSize / (float)(totalUncompressed));
    printf("Image size=%d bytes, %d k\n", int(totalImageSize), int(totalImageSize / 1024));
}

