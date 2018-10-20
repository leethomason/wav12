#include <assert.h>
#include <string.h>

#include "memimage.h"
#include "./wav12/compress.h"

MemImageUtil::MemImageUtil()
{
    assert(sizeof(MemImage) == 1024);
    dataVec = new uint8_t[DATA_VEC_SIZE];
    memset(dataVec, 0, DATA_VEC_SIZE);
    currentPos = sizeof(MemImage);  // move the write head past the header.
}


MemImageUtil::~MemImageUtil()
{
    delete[] dataVec;
}


void MemImageUtil::addDir(const char* name)
{
    currentDir++;
    assert(currentDir < MemImage::NUM_DIR);
    MemImage* image = (MemImage*)dataVec;
    strncpy(image->dir[currentDir].name, name, MemUnit::NAME_LEN);
    image->dir[currentDir].offset = currentFile + 1;
}


void MemImageUtil::addFile(const char* name, void* data, int size)
{
    assert(currentDir >= 0);
    currentFile++;
    assert(currentFile < MemImage::NUM_FILES);
    
    MemImage* image = (MemImage*)dataVec;
    image->dir[currentDir].size += 1;
    strncpy(image->file[currentFile].name, name, MemUnit::NAME_LEN);
    image->file[currentFile].offset = currentPos;
    image->file[currentFile].size = size;

    memcpy(dataVec + currentPos, data, size);
    currentPos += size;
}


void MemImageUtil::write(const char* name)
{
    FILE* fp = fopen(name, "wb");
    fwrite(dataVec, currentPos, 1, fp);
    fclose(fp);
}


void MemImageUtil::dumpConsole()
{
    char buf[9] = { 0 };
    uint32_t totalUncompressed = 0, totalSize = 0;
    const MemImage* image = (const MemImage*)dataVec;

    for (int d = 0; d < MemImage::NUM_DIR; ++d) {
        uint32_t dirTotal = 0;

        if (image->dir[d].name[0]) {
            strcpy(buf, image->dir[d].name);
            printf("Dir: %s\n", buf);
             
            for (unsigned f = 0; f < image->dir[d].size; ++f) {
                const MemUnit& fileUnit = image->file[image->dir[d].offset + f];
                std::string sName;
                sName.append(fileUnit.name, fileUnit.name + MemUnit::NAME_LEN);

                const wav12::Wav12Header* header =
                    (const wav12::Wav12Header*)(dataVec + fileUnit.offset);

                printf("   %8s at %8d size=%6d (%3dk) comp=%d ratio=%4.2f shift=%d\n", 
                    sName.c_str(), 
                    fileUnit.offset, fileUnit.size, fileUnit.size / 1024,
                    header->format,
                    float(header->lenInBytes) / (float)(header->nSamples*2),
                    header->shiftBits);

                totalUncompressed += header->nSamples * 2;
                totalSize += header->lenInBytes;
                dirTotal += header->lenInBytes;
            }
        }
        if (dirTotal)
            printf("  Dir total=%dk\n", dirTotal / 1024);
    }
    size_t totalImageSize = sizeof(MemImage) + currentPos;
    printf("Overall ratio=%5.2f\n", (float)totalSize / (float)(totalUncompressed));
    printf("Image size=%d bytes, %d k\n", int(totalImageSize), int(totalImageSize / 1024));
}

