#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <chrono>
#include <stdio.h>
#include <iostream>

extern "C" {
#include "wave_reader.h"
}

#include "tinyxml2.h"
#include "memimage.h"

#include "./wav12/compress.h"
#include "./wav12/bits.h"

using namespace wav12;
using namespace tinyxml2;

bool runTest(wave_reader* wr, int shift);
int parseXML(const char* filename, bool textFile);

int main(int argc, const char* argv[])
{
    if (argc < 2) {
        printf("Usage:\n");
        printf("    wav12 filename                         Runs tests on 'filename'\n");
        printf("    wav12 filename  <outfile> <options>    Compresses 'filename'\n");
        printf("    wav12 xmlFile                          Creates memory image.\n");
        printf("Options:\n");
        printf("    -1 to -4, bit loss.\n");
        printf("    -t, write text file.\n");
        return 1;
    }
    if (strstr(argv[1], ".xml")) {
        bool textFile = false;
        if (argc == 3 && strcmp(argv[2], "-t") == 0) {
            textFile = true;
        }
        int rc = parseXML(argv[1], textFile);
        printf("Image generation return code=%d\n", rc);
        return rc;
    }

    wave_reader_error error;
    wave_reader* wr = wave_reader_open(argv[1], &error);

    int format = wave_reader_get_format(wr);
    int nChannels = wave_reader_get_num_channels(wr);
    int rate = wave_reader_get_sample_rate(wr);
    int nSamples = wave_reader_get_num_samples(wr);

    printf("Format=%d channels=%d rate=%d\n", format, nChannels, rate);

    if (format != 1
        || nChannels != 1
        || rate != 22050)
    {
        printf("Input must be 22050 Hz Mono\n");
        return 1;
    }
    FILE* outputFP = 0;
    int shift = 0;

    for (int i = 2; i < argc; ++i) {
        if (argv[i][0] == '-') {
            char option = argv[i][1];
            if (option >= '1' && option <= '4') {
                shift = option - '0';
            }
        }
        else {
            assert(!outputFP);
            outputFP = fopen(argv[i], "wb");
            if (!outputFP) {
                printf("Could not open '%s'\n", argv[2]);
                return 2;
            }
        }
    }

    if (!outputFP) {
        bool okay = runTest(wr, shift);
        return okay ? 0 : 1;
    }

    int16_t* data = new int16_t[nSamples];
    wave_reader_get_samples(wr, nSamples, data);

    CompressStat stat;
    uint8_t* compressed = 0;
    int nCompressed = 0;

    wav12::linearCompress(data, nSamples, &compressed, &nCompressed, shift, &stat);

    int bytesOriginal = nSamples * 2;
    printf("bytes orig=%d bytes comp=%d ratio=%.2f\n", bytesOriginal, nCompressed,
        float(nCompressed) / float(bytesOriginal));

    if (nCompressed >= bytesOriginal) {
        printf("Didn't compress.\n");
    }

    if (outputFP) {
        uint8_t compressed8 = (nCompressed < bytesOriginal) ? 0 : 1;
        uint8_t shift8 = uint8_t(shift);
        uint32_t samples32 = uint32_t(nSamples);

        fwrite(&compressed8, 1, 1, outputFP);
        fwrite(&shift8, 1, 1, outputFP);
        fwrite(&samples32, 4, 1, outputFP);

        if (!compressed8)
            fwrite(data, 2, nSamples, outputFP);
        else
            fwrite(compressed, 1, nCompressed, outputFP);

        fclose(outputFP);
    }

    delete[] compressed;
    delete[] data;

    wave_reader_close(wr);
    return 0;
}


bool runTest(wave_reader* wr, int shift)
{
    bool testOkay = BitAccum::Test();
    assert(testOkay);
    testOkay = BitReader::TestReaderAndWriter();
    assert(testOkay);

    int format = wave_reader_get_format(wr);
    int nChannels = wave_reader_get_num_channels(wr);
    int rate = wave_reader_get_sample_rate(wr);
    int nSamples = wave_reader_get_num_samples(wr);
    int error = 1 << shift;

    int16_t* data = new int16_t[nSamples];
    wave_reader_get_samples(wr, nSamples, data);

    CompressStat stat;
    uint8_t* compressed = 0;
    int nCompressed = 0;
    std::chrono::time_point<std::chrono::steady_clock> startCompress, endCompress, startExpand, endExpand, startChunk, endChunk;

    startCompress = std::chrono::high_resolution_clock::now();
    wav12::linearCompress(data, nSamples, &compressed, &nCompressed, shift, &stat);
    endCompress = std::chrono::high_resolution_clock::now();

    int bytesOriginal = nSamples * 2;
    printf("bytes orig=%d bytes comp=%d ratio=%.2f\n", 
        bytesOriginal, nCompressed,
        float(nCompressed) / float(bytesOriginal));

    if (nCompressed >= bytesOriginal) {
        printf("Didn't compress.\n");
    }

    stat.consolePrint();

    int16_t* newData = new int16_t[nSamples];
    startExpand = std::chrono::high_resolution_clock::now();
    wav12::linearExpand(compressed, nCompressed, newData, nSamples, shift);
    endExpand = std::chrono::high_resolution_clock::now();

    bool okay = true;
    for (int i = 0; i < nSamples; i++) {
        if (abs(data[i] - newData[i]) >= error) {
            assert(false);
            okay = false;
        }
    }
    printf("Test full stream: %s\n", okay ? "PASS" : "FAIL");
    if (!okay) return false;

    int16_t buffer[17];
    MemStream memStream(compressed, nCompressed);
    Expander expander(&memStream, nSamples, 1, shift);

    okay = true;
    startChunk = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < nSamples; i += 17) {
        int n = wMin(17, nSamples - i);
        expander.expand(buffer, n);
        for (int j = i; j < i + n; ++j) {
            if (abs(data[i] - newData[i]) >= error) { // FIXME doesn't look correct...
                assert(false);
                okay = false;
            }
        }
    }
    endChunk = std::chrono::high_resolution_clock::now();
    printf("Test chunk stream: %s\n", okay ? "PASS" : "FAIL");
    delete[] newData;

    std::cout << "compress: " << std::chrono::duration_cast<std::chrono::microseconds>(endCompress - startCompress).count() << "\n";
    std::cout << "expand: " << std::chrono::duration_cast<std::chrono::microseconds>(endExpand - startExpand).count() << "\n";
    std::cout << "chunk + verify: " << std::chrono::duration_cast<std::chrono::microseconds>(endChunk - startChunk).count() << "\n";

    return okay;
}


std::string stdString(const char* p)
{
    std::string s;
    while (*p) {
        s += tolower(*p);
        p++;
    }
    if (s.size() > MemUnit::NAME_LEN) {
        s.resize(MemUnit::NAME_LEN, ' ');
    }
    return s;
}


int parseXML(const char* filename, bool textFile)
{
    XMLDocument doc;
    doc.LoadFile(filename);
    if (doc.Error()) return 1;

    MemImageUtil image;

    for (const XMLElement* dirElement = doc.RootElement()->FirstChildElement();
        dirElement;
        dirElement = dirElement->NextSiblingElement()) 
    {
        std::string stdDirName = stdString(dirElement->Attribute("path"));
        image.addDir(stdDirName.c_str());

        for (const XMLElement* fileElement = dirElement->FirstChildElement();
            fileElement;
            fileElement = fileElement->NextSiblingElement())
        {
            const char* fname = fileElement->Attribute("path");
            const char* extension = strrchr(fname, '.');
            std::string stdfname;
            stdfname.append(fname, extension);

            std::string fullPath = stdDirName;
            fullPath += '/';
            fullPath += fname;

            wave_reader_error error = WR_NO_ERROR;
            wave_reader* wr = wave_reader_open(fullPath.c_str(), &error);
            if (error != WR_NO_ERROR) return error;

            int format = wave_reader_get_format(wr);
            int nChannels = wave_reader_get_num_channels(wr);
            int rate = wave_reader_get_sample_rate(wr);
            int nSamples = wave_reader_get_num_samples(wr);

            if (format != 1 || nChannels != 1 || rate != 22050) {
                printf("Input '%s' must be 22050 Hz Mono, freq=%d channels=%d\n", fname, rate, nChannels);
                return 100;
            }

            int16_t* data = new int16_t[nSamples];
            wave_reader_get_samples(wr, nSamples, data);

            CompressStat stat;
            uint8_t* compressed = 0;
            int nCompressed = 0;
            int shift = fileElement->IntAttribute("shift", 0);
            wav12::linearCompress(data, nSamples, &compressed, &nCompressed, shift, &stat);
            int bytesOriginal = nSamples * 2;

            //printf("%s/%s bytes orig=%d comp=%d ratio=%.2f\n",
            //    stdDirName.c_str(), stdfname.c_str(),
            //    bytesOriginal, nCompressed,
            //    float(nCompressed) / float(bytesOriginal));

            bool isCompressed = nCompressed < bytesOriginal;
            isCompressed = fileElement->BoolAttribute("compress", isCompressed);

            Wav12Header header;
            header.id[0] = 'w';
            header.id[1] = 'v';
            header.id[2] = '1';
            header.id[3] = '2';
            header.lenInBytes = isCompressed ? nCompressed : bytesOriginal;
            header.nSamples = nSamples;
            header.format = isCompressed ? 1 : 0;
            header.shiftBits = isCompressed ? shift : 0;
            header.unused[0] = header.unused[1] = 0;

            uint8_t* mem = new uint8_t[sizeof(Wav12Header) + header.lenInBytes];
            memcpy(mem, &header, sizeof(Wav12Header));
            if (isCompressed)
                memcpy(mem + sizeof(Wav12Header), compressed, nCompressed);
            else
                memcpy(mem + sizeof(Wav12Header), data, bytesOriginal);

            image.addFile(stdfname.c_str(), mem, header.lenInBytes + sizeof(Wav12Header));

            delete[] mem;
            delete[] data;
            delete[] compressed;
            wave_reader_close(wr);
        }
    }

    image.dumpConsole();
    image.write("memimage.bin");
    if (textFile)
        image.writeText("memimage.txt");
    return 0;
}