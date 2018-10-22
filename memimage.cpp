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

void MemImageUtil::dumpConsole()
{
    uint32_t totalUncompressed = 0, totalSize = 0;
    const MemImage* image = (const MemImage*)dataVec;

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
                    wave_reader_error error = WR_NO_ERROR;
                    wave_reader* wr = wave_reader_open(path.c_str(), &error);
                    assert(error == WR_NO_ERROR);
                    int nSamples = wave_reader_get_num_samples(wr);
                    int16_t* wav = new int16_t[nSamples];
                    wave_reader_get_samples(wr, nSamples, wav);

                    static const int BUFSIZE = 256;
                    int16_t buf[BUFSIZE];

                    assert(fileUnit.size == header->lenInBytes + sizeof(wav12::Wav12Header));

                    if (header->format == 1) {
                        wav12::MemStream memStream(
                            dataVec + fileUnit.offset + sizeof(wav12::Wav12Header),
                            fileUnit.size);

                        wav12::Expander expander(&memStream, header->nSamples, header->shiftBits);
                        int errorRange = 1 << header->shiftBits;

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
                    }
                    else {
                        int cmp = memcmp(wav, dataVec + fileUnit.offset + sizeof(wav12::Wav12Header), nSamples * 2);
                        if (cmp) {
                            assert(false);
                            okay = false;
                        }
                    }

                    delete[] wav;
                    wave_reader_close(wr);
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

