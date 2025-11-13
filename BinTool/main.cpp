#include <stdio.h>
#include <iostream>
#include <locale>
#include <string>
#include <filesystem>
#include "helpers/CommandLineParser.h"
#include "helpers/LoggingHelpers.h"
#include "helpers/Crc32Fast.h"
#include "helpers/sha256.h"
#include <fstream>
//#include <Windows.h>

using namespace std;
using namespace CLP;

bool Overwrite(const std::string& sSourcePath, const std::string& sDestPath, int64_t nSourceOffset, int64_t nDestOffset, int64_t nBytes, bool bVerbose)
{
    if (bVerbose)
        zout << "Overwriting " << nBytes << " from " << sSourcePath << ":" << nSourceOffset << " into " << sDestPath << ":" << nDestOffset << "\n";

    std::ifstream srcFile;
    srcFile.open(sSourcePath, ios::binary);
    if (!srcFile)
    {
        cerr << "Failed to open source file:" << sSourcePath.c_str() << "\n";
        return false;
    }

    int64_t nSourceSize = std::filesystem::file_size(sSourcePath);

    if (nSourceOffset + nBytes >= nSourceSize)
    {
        cerr << "Source range outside of source file bytes.\n";
        return false;
    }

    srcFile.seekg(nSourceOffset, std::ios::beg);


    std::fstream dstFile;
    dstFile.open(sDestPath, ios::in | ios::out | ios::binary);
    if (!dstFile)
    {
        cerr << "Failed to open dest file:" << sDestPath.c_str() << "\n";
        return false;
    }

    dstFile.seekp(nDestOffset, std::ios::beg);


    vector<char> bytesToCopy(nBytes);
    char* pBytesToCopy = &bytesToCopy[0];
    if (!pBytesToCopy)
    {
        cerr << "Failed to allocate:" << nBytes << "\n";
        return false;
    }

    srcFile.read(pBytesToCopy, nBytes);
    if (srcFile.fail())
    {
        cerr << "Failed read from source\n";
        return false;
    }

    dstFile.write(pBytesToCopy, nBytes);
    {
        cerr << "Failed write to dest\n";
        return false;
    }

    dstFile.seekp(0, std::ios::end);

    return true;
}



bool CopyBytes(std::ifstream& inFile, std::ofstream& outFile, int64_t nBytes)
{
    int nBufSize = 256 * 1024;
    char* pBuf = new char[256 * 1024];

    while (nBytes > 0)
    {
        int64_t nBytesToRead = nBufSize;
        if (nBytes < nBytesToRead)
            nBytesToRead = nBytes;

        if (!inFile.read(pBuf, nBytesToRead))
        {
            cerr << "Failed to read " << nBytesToRead << " bytes from input file.\n";
            return false;
        }

        if (!outFile.write(pBuf, nBytesToRead))
        {
            cerr << "Failed to write " << nBytesToRead << " bytes to output file.\n";
            return false;
        }

        nBytes -= nBytesToRead;
    }

    delete[] pBuf;

    return true;
}


bool Extract(const std::string& sSourcePath, const std::string& sDestPath, int64_t nSourceOffset, int64_t nBytes, bool bVerbose)
{
    if (bVerbose)
        zout << "Extracting " << nBytes << " from " << sSourcePath << " to " << sDestPath << "\n";

    std::ifstream srcFile;
    srcFile.open(sSourcePath, ios::binary);
    if (!srcFile)
    {
        cerr << "Failed to open source file:" << sSourcePath.c_str() << "\n";
        return false;
    }

    int64_t nSourceSize = std::filesystem::file_size(sSourcePath);

    if (nSourceOffset + nBytes > nSourceSize)
    {
        cerr << "Source range outside of source file bytes.\n";
        return false;
    }

    srcFile.seekg(nSourceOffset, std::ios::beg);


    std::ofstream dstFile;
    dstFile.open(sDestPath, ios::binary);
    if (!dstFile)
    {
        cerr << "Failed to open dest file:" << sDestPath.c_str() << "\n";
        return false;
    }

    if (!CopyBytes(srcFile, dstFile, nBytes))
    {
        cerr << "Failed to extract " << nBytes << " from " << sSourcePath << "\n";
        return false;
    }

    srcFile.close();
    dstFile.close();

    return true;
}

bool Insert(const std::string& sSourcePath, const std::string& sDestPath, int64_t nSourceOffset, int64_t nDestOffset, int64_t nBytes, bool bVerbose)
{
    if (bVerbose)
        zout << "Inserting " << nBytes << " from " << sSourcePath << ":" << nSourceOffset << " to " << sDestPath << ":" << nDestOffset << "\n";


    std::ifstream srcFile;
    srcFile.open(sSourcePath, ios::binary);
    if (!srcFile)
    {
        cerr << "Failed to open source file:" << sSourcePath.c_str() << "\n";
        return false;
    }

    int64_t nSourceSize = std::filesystem::file_size(sSourcePath);

    if (nSourceOffset + nBytes >= nSourceSize)
    {
        cerr << "Source range outside of source file bytes.\n";
        return false;
    }

    srcFile.seekg(nSourceOffset, std::ios::beg);


    std::ifstream dstFileInput;
    dstFileInput.open(sDestPath, ios::in | ios::out | ios::binary);
    if (!dstFileInput)
    {
        cerr << "Failed to open dest file:" << sDestPath.c_str() << "\n";
        return false;
    }

    int64_t nDestFileSizeBeforeInsert = std::filesystem::file_size(sDestPath);



    std::filesystem::path tempFilename(sDestPath + "_tmp");

    if (bVerbose)
        zout << "Creating temporary file " << tempFilename << "\n";

    std::ofstream tmpDstFile;
    tmpDstFile.open(tempFilename, ios::binary);
    if (!tmpDstFile)
    {
        cerr << "Failed to open temporary dest file:" << tempFilename.string() << "\n";
        return false;
    }

    // copy the data from the destination file into the temporary file up until the insertion point

    if (bVerbose)
        zout << "Copying " << nDestOffset << " bytes from begining of original destination file.\n";

    dstFileInput.seekg(0, std::ios::beg);
    tmpDstFile.seekp(0, std::ios::beg);

    if (!CopyBytes(dstFileInput, tmpDstFile, nDestOffset))
        return false;


    if (bVerbose)
        zout << "Copying " << nBytes << " bytes from source file at offset " << nSourceOffset << "\n";
    srcFile.seekg(nSourceOffset, std::ios::beg);
    if (!CopyBytes(srcFile, tmpDstFile, nBytes))
        return false;


    int64_t nRemainingDestBytes = nDestFileSizeBeforeInsert - nDestOffset;

    if (bVerbose)
        zout << "Copying remaining " << nRemainingDestBytes << " from original destination file.\n";

    if (!CopyBytes(dstFileInput, tmpDstFile, nRemainingDestBytes))
        return false;

    tmpDstFile.close();
    dstFileInput.close();
    srcFile.close();

    std::filesystem::path renamedDestFilename(sDestPath + "_old");

    if (bVerbose)
        zout << "renaming " << sDestPath << " to " << renamedDestFilename << "\n";
    std::filesystem::rename(sDestPath, renamedDestFilename);

    if (bVerbose)
        zout << "renaming " << tempFilename << " to " << sDestPath << "\n";
    std::filesystem::rename(tempFilename, sDestPath);

    if (bVerbose)
        zout << "removing " << renamedDestFilename << "\n";
    std::filesystem::remove(renamedDestFilename);
    

    return true;
}

bool Dump(const std::string& sSourcePath, int64_t nSourceOffset, int64_t nBytes, int64_t nColumns, bool bVerbose)
{
    std::ifstream srcFile;
    srcFile.open(sSourcePath, ios::binary);
    if (!srcFile)
    {
        cerr << "Failed to open source file:" << sSourcePath.c_str() << "\n";
        return false;
    }

    int64_t nSourceSize = std::filesystem::file_size(sSourcePath);
    srcFile.seekg(nSourceOffset, std::ios::beg);

    if (nSourceOffset > nSourceSize)
    {
        cerr << "Source offset:" << nSourceOffset << " > file size:" << nSourceSize << "\n";
        return false;
    }

    if (nBytes == 0)
        nBytes = nSourceSize;

    if (nSourceOffset + nBytes > nSourceSize)
    {
        nBytes = nSourceSize - nSourceOffset;
        if (bVerbose)
            zout << "Returning " << nBytes << " only due to reaching end of file.\n";
    }

    uint8_t* pBuf = new uint8_t[nBytes];

    srcFile.read((char*)pBuf, nBytes);

    DumpMemoryToCout(pBuf, (uint32_t) nBytes, (uint64_t) nSourceOffset, (uint32_t) nColumns);

    delete[] pBuf;
    return true;
}

bool Hash(const std::string sSourcePath)
{
    SHA256Hash sha256Hash;
    uint32_t crc32 = 0;


    std::ifstream srcFile;
    srcFile.open(sSourcePath, ios::binary);
    if (!srcFile)
    {
        cerr << "Failed to open source file:" << sSourcePath.c_str() << "\n";
        return false;
    }

    int64_t nBytesLeft = std::filesystem::file_size(sSourcePath);

    size_t nBufferSize = 256 * 1024;
    uint8_t* pBuf = new uint8_t[nBufferSize];

    while (nBytesLeft > 0)
    {
        int64_t nBytesToRead = nBufferSize;
        if (nBytesLeft < nBytesToRead)
            nBytesToRead = nBytesLeft;

        if (!srcFile.read((char*) pBuf, nBytesToRead))
        {
            cerr << "Failed to read " << nBytesToRead << " bytes from input file.\n";
            return false;
        }

        sha256Hash.Compute(pBuf, nBytesToRead);
        crc32 = crc32_16bytes(pBuf, nBytesToRead, crc32);

        nBytesLeft -= nBytesToRead;
    }

    sha256Hash.Final();

    zout << "Hashes of \"" << sSourcePath << "\":\n";
    zout << "SHA256:0x" << sha256Hash.ToString() << "\n";
    zout << "CRC32: 0x" << std::uppercase << std::hex << crc32 << std::dec << "\n";

    delete[] pBuf;
    return true;
}

bool Diff(const std::string& sFile1, const std::string& sFile2, bool bVerbose)
{
    if (bVerbose)
        zout << "Diffing FILE1:" << sFile1 << " vs FILE2:" << sFile2 << "\n";

    std::ifstream file1;
    file1.open(sFile1, ios::binary);
    if (!file1)
    {
        cerr << "ERROR: FILE1:" << sFile1 << " not found.\n";
        return false;
    }


    std::ifstream file2;
    file2.open(sFile2, ios::binary);
    if (!file2)
    {
        cerr << "ERROR: FILE2:" << sFile2 << " not found.\n";
        return false;
    }

    size_t nFile1Size = std::filesystem::file_size(sFile1);
    size_t nFile2Size = std::filesystem::file_size(sFile2);

    if (nFile1Size != nFile2Size)
    {
        zout << "FILE1 Size: " << nFile1Size << "\n";
        zout << "FILE2 Size: " << nFile2Size << "\n";
        zout << "Result: DIFFERENT\n";

        return true;
    }

    size_t nBufferSize = 1024 * 1024;
    uint8_t* pBuf1 = new uint8_t[nBufferSize];
    uint8_t* pBuf2 = new uint8_t[nBufferSize];

    size_t nBytesLeft = nFile1Size;
    bool bSame = true;

    while (nBytesLeft > 0)
    {
        size_t nBytesToRead = nBufferSize;
        if (nBytesLeft < nBytesToRead)
            nBytesToRead = nBytesLeft;

        if (!file1.read((char*)pBuf1, nBytesToRead))
        {
            cerr << "Failed to read " << nBytesToRead << " bytes from FILE1.\n";
            bSame = false;
            break;
        }

        if (!file2.read((char*)pBuf2, nBytesToRead))
        {
            cerr << "Failed to read " << nBytesToRead << " bytes from FILE2.\n";
            bSame = false;
            break;
        }

        if (bVerbose)
            zout << "Comparing offset: " << nFile1Size - nBytesLeft << "\n";

        if (memcmp(pBuf1, pBuf2, nBytesToRead) != 0)
        {
            bSame = false;
            break;
        }

        nBytesLeft -= nBytesToRead;
    }

    if (bSame)
    {
        zout << "Result: SAME\n";
        if (bVerbose)
            zout << "All " << nFile1Size << " bytes match.\n";
    }
    else
    {
        zout << "Result: DIFFERENT\n";
    }

    delete[] pBuf1;
    delete[] pBuf2;

    return true;
}











int main(int argc, char* argv[])
{
/*
    Table table;
    table.defaultStyle = Table::Style(AnsiCol(40, 40, 255, 0, 0, 0), Table::RIGHT, Table::EVEN, 0);
    table.SetBorders( AnsiCol(40, 40, 255, 0, 0, 0) + string("["), "-", "]", "-" );
    Table::Style sectionStyle;
    sectionStyle.alignment = Table::CENTER;
    sectionStyle.color = AnsiCol(125, 255, 80, 10, 10, 10);
    sectionStyle.padchar = '%';

    Table::Style rightStyle;
    rightStyle.alignment = Table::RIGHT;
    rightStyle.spacing = Table::EVEN;
    rightStyle.padding = 0;
    
    table.AddRow(sectionStyle, " This is a section ");
    table.AddRow(Table::Style(AnsiCol(0, 0, 0, 20, 255, 80), Table::CENTER, Table::EVEN, 1), "123", "12345678901234567890", "123", "1", "1");
    table.AddRow(Table::Style(AnsiCol(0, 0, 0, 20, 255, 20), Table::CENTER, Table::EVEN, 1), "12345", "1234567890", "12345", "1", "1234");

    table.AddRow("blah", 123, "blah blah blah");
    table.renderWidth = 16;
    zout << table;
    
    */

    
    




    std::string sSourcePath;
    std::string sDestPath;

    int64_t nSourceOffset = 0;
    int64_t nDestOffset = 0;
    int64_t nBytes = 0;
    int64_t nColumns = 32;
    bool bOverwrite = false;
    bool bDumpAfterExtract = false;
    bool bVerbose = false;

    CommandLineParser parser;



    parser.RegisterMode("copy", "Copies bytes from a source file into the destination file");

    parser.RegisterParam("copy", ParamDesc("SOURCE_FILE", &sSourcePath, CLP::kPositional | CLP::kRequired | CLP::kPath | CLP::kExistingPath, "File to read from."));
    parser.RegisterParam("copy", ParamDesc("DEST_FILE", &sDestPath, CLP::kPositional | CLP::kRequired | CLP::kPath, "File to write to."));
    parser.RegisterParam("copy", ParamDesc("SOURCE_OFFSET", &nSourceOffset, CLP::kPositional | CLP::kRequired, "Source offset"));
    parser.RegisterParam("copy", ParamDesc("DEST_OFFSET", &nDestOffset, CLP::kPositional | CLP::kRequired, "Destination offset"));
    parser.RegisterParam("copy", ParamDesc("BYTES", &nBytes, CLP::kPositional | CLP::kRequired, "Number of bytes to copy."));
    parser.RegisterParam("copy", ParamDesc("overwrite", &bOverwrite, CLP::kNamed , "Overwrites bytes in the destination file (rather than inserting)"));

    parser.RegisterMode("dump", "Dump byte range to zout");
    parser.RegisterParam("dump", ParamDesc("FILE", &sSourcePath, CLP::kPositional | CLP::kRequired | CLP::kPath | CLP::kExistingPath, "File to read from"));
    parser.RegisterParam("dump", ParamDesc("OFFSET", &nSourceOffset, CLP::kPositional, "Starting offset"));
    parser.RegisterParam("dump", ParamDesc("BYTES", &nBytes, CLP::kPositional, "Number of bytes to dump."));
    parser.RegisterParam("dump", ParamDesc("COLUMNS", &nColumns, CLP::kNamed, "Number of columns"));


    parser.RegisterMode("extract", "Extracts bytes from a file into a new file");
    parser.RegisterParam("extract", ParamDesc("SOURCE_FILE", &sSourcePath, CLP::kPositional | CLP::kRequired | CLP::kPath | CLP::kExistingPath, "File to read from"));
    parser.RegisterParam("extract", ParamDesc("SOURCE_OFFSET", &nSourceOffset, CLP::kPositional | CLP::kRequired, "Source offset"));
    parser.RegisterParam("extract", ParamDesc("DEST_FILE", &sDestPath, CLP::kPositional | CLP::kRequired | CLP::kPath, "File to create"));
    parser.RegisterParam("extract", ParamDesc("BYTES", &nBytes, CLP::kPositional | CLP::kRequired, "Number of bytes to extract"));
    parser.RegisterParam("extract", ParamDesc("dump", &bDumpAfterExtract, CLP::kNamed, "Dump extracted bytes to zout after extraction."));


    parser.RegisterMode("hash", "Compute SHA256 & CRC32 hashes of file.");
    parser.RegisterParam("hash", ParamDesc("FILE", &sSourcePath, CLP::kPositional | CLP::kRequired | CLP::kPath | CLP::kExistingPath, "File to hash"));

    parser.RegisterMode("diff", "Simple binary diff of two files.");
    parser.RegisterParam("diff", ParamDesc("FILE1", &sSourcePath, CLP::kPositional | CLP::kRequired | CLP::kPath | CLP::kExistingPath, "A file to compare"));
    parser.RegisterParam("diff", ParamDesc("FILE2", &sDestPath, CLP::kPositional | CLP::kRequired | CLP::kPath | CLP::kExistingPath, "A second file to compare against"));


    parser.RegisterAppDescription("Various binary operations on files");

    if (!parser.Parse(argc, argv))
    {
        return -1;
    }

    if (parser.IsCurrentMode("copy"))
    {
        if (bOverwrite)
        {
            if (!Overwrite(sSourcePath, sDestPath, nSourceOffset, nDestOffset, nBytes, bVerbose))
                return -1;
        }
        else
        {
            if (!Insert(sSourcePath, sDestPath, nSourceOffset, nDestOffset, nBytes, bVerbose))
                return -1;
        }
    }
    else if (parser.IsCurrentMode("extract"))
    {
        if (!Extract(sSourcePath, sDestPath, nSourceOffset, nBytes, bVerbose))
            return -1;

        if (bDumpAfterExtract)
            Dump(sDestPath, 0, nBytes, nColumns, bVerbose);

    }
    else if (parser.IsCurrentMode("dump"))
    {
        if (!Dump(sSourcePath, nSourceOffset, nBytes, nColumns, bVerbose))
            return -1;
    }
    else if (parser.IsCurrentMode("hash"))
    {
        if (!Hash(sSourcePath))
            return -1;
    }
    else if (parser.IsCurrentMode("diff"))
    {
        if (!Diff(sSourcePath, sDestPath, bVerbose))
            return -1;
    }


    zout << "Done.\n";

    return 0;
}

