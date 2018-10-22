#include <assert.h>
#include <string.h>

#include "memimage.h"
extern "C" { 
#include "wave_reader.h" 
}
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

void MemImageUtil::writeText(const char* name)
{
    FILE* fp = fopen(name, "w");

    fprintf(fp, "%d\n", currentPos);
    int line = 0;
    for (uint32_t i = 0; i < currentPos; ++i) {
        fprintf(fp, "%02x", dataVec[i]);
        line++;
        if (line == 64) {
            fprintf(fp, "\n");
            line = 0;
        }
    }
    if (line) {
        fprintf(fp, "\n");
    }
    fclose(fp);
}

class MemChunkStream : public wav12::ChunkStream
{
public:
    MemChunkStream(const uint8_t* mem, uint32_t memSize, uint8_t* subBuffer, int subBufferSize) :
        ChunkStream(subBuffer, subBufferSize),
        m_mem(mem),
        m_pos(0),
        m_size(memSize)
    {}

    virtual void fillSubBuffer() {
        assert((m_pos & 1) == 0);
        int toRead = wav12::wMin(int(m_size - m_pos), m_subBufferSize);
        assert(m_pos + toRead <= m_size);
        memcpy(m_subBuffer, m_mem + m_pos, toRead);
        m_subBufferPos = 0;
        m_pos += toRead;
        assert(m_pos <= m_size);
    }

private:
    const uint8_t* m_mem;
    uint32_t m_pos;
    uint32_t m_size;
};

void MemImageUtil::dumpConsole()
{
    uint32_t totalUncompressed = 0, totalSize = 0;
    const MemImage* image = (const MemImage*)dataVec;
    static const int SUB_BUFFER_SIZE = 64;
    uint8_t subBuffer[SUB_BUFFER_SIZE];

    for (int d = 0; d < MemImage::NUM_DIR; ++d) {
        uint32_t dirTotal = 0;

        if (image->dir[d].name[0]) {
            char dirName[9] = { 0 };
            strncpy(dirName, image->dir[d].name, 8);
            printf("Dir: %s\n", dirName);
             
            for (unsigned f = 0; f < image->dir[d].size; ++f) {
                const MemUnit& fileUnit = image->file[image->dir[d].offset + f];
                char fileName[9] = { 0 };
                strncpy(fileName, fileUnit.name, 8);

                const wav12::Wav12Header* header =
                    (const wav12::Wav12Header*)(dataVec + fileUnit.offset);

                // Verify!
                bool okay = true;
                {
                    std::string path = std::string(dirName) 
                        + "/" + std::string(fileName) + std::string(".wav");

                    int16_t* wav = 0;
                    int nSamples = 0;
                    {
                        wave_reader_error error = WR_NO_ERROR;
                        wave_reader* wr = wave_reader_open(path.c_str(), &error);
                        assert(error == WR_NO_ERROR);
                        nSamples = wave_reader_get_num_samples(wr);
                        wav = new int16_t[nSamples];
                        wave_reader_get_samples(wr, nSamples, wav);
                        wave_reader_close(wr);
                    }

                    MemChunkStream fcs(
                        dataVec + fileUnit.offset + sizeof(wav12::Wav12Header), 
                        fileUnit.size,
                        subBuffer, SUB_BUFFER_SIZE);

                    assert(fileUnit.size == header->lenInBytes + sizeof(wav12::Wav12Header));
                    assert(nSamples == header->nSamples);
                    
                    wav12::MemStream memStream(
                        dataVec + fileUnit.offset + sizeof(wav12::Wav12Header),
                        fileUnit.size);
                    
                    wav12::Expander expander(&fcs, header->nSamples, header->format, header->shiftBits);
                    int errorRange = 1 << header->shiftBits;

                    static const int BUFSIZE = 256;
                    int16_t buf[BUFSIZE];

                    for (int i = 0; i < nSamples; i += BUFSIZE) {
                        int n = wav12::wMin(BUFSIZE, nSamples - i);
                        expander.expand(buf, n);

                        for (int j = 0; j < n; ++j) {
                            if (abs(buf[j] - wav[i + j]) >= errorRange) {
                                assert(false);
                                okay = false;
                            }
                        }
                    }

                    delete[] wav;
                }

                printf("   %8s at %8d size=%6d (%3dk) comp=%d ratio=%4.2f shift=%d valid=%s\n", 
                    fileName, 
                    fileUnit.offset, fileUnit.size, fileUnit.size / 1024,
                    header->format,
                    float(header->lenInBytes) / (float)(header->nSamples*2),
                    header->shiftBits,
                    okay ? "true" : "ERROR" );

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

