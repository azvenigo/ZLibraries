#include <stdio.h>
#include <iostream>
#include <locale>
#include <string>
#include <filesystem>
#include "helpers/CommandLineParser.h"
#include "helpers/LoggingHelpers.h"
#include <filesystem>
#include <fstream>

using namespace std;
using namespace CLP;

bool Overwrite(const std::string& sSourcePath, const std::string& sDestPath, int64_t nSourceOffset, int64_t nDestOffset, int64_t nBytes, bool bVerbose)
{
    if (bVerbose)
        cout << "Overwriting " << nBytes << " from " << sSourcePath << ":" << nSourceOffset << " into " << sDestPath << ":" << nDestOffset << "\n";

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


    char* pBytesToCopy = new char[(uint32_t)nBytes];
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
    if (srcFile.fail())
    {
        cerr << "Failed write to dest\n";
        return false;
    }

    dstFile.seekp(0, std::ios::end);

    delete[] pBytesToCopy;

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
        cout << "Extracting " << nBytes << " from " << sSourcePath << " to " << sDestPath << "\n";

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
        cout << "Inserting " << nBytes << " from " << sSourcePath << ":" << nSourceOffset << " to " << sDestPath << ":" << nDestOffset << "\n";


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
        cout << "Creating temporary file " << tempFilename << "\n";

    std::ofstream tmpDstFile;
    tmpDstFile.open(tempFilename, ios::binary);
    if (!tmpDstFile)
    {
        cerr << "Failed to open temporary dest file:" << tempFilename.c_str() << "\n";
        return false;
    }

    // copy the data from the destination file into the temporary file up until the insertion point

    if (bVerbose)
        cout << "Copying " << nDestOffset << " bytes from begining of original destination file.\n";

    dstFileInput.seekg(0, std::ios::beg);
    tmpDstFile.seekp(0, std::ios::beg);

    if (!CopyBytes(dstFileInput, tmpDstFile, nDestOffset))
        return false;


    if (bVerbose)
        cout << "Copying " << nBytes << " bytes from source file at offset " << nSourceOffset << "\n";
    srcFile.seekg(nSourceOffset, std::ios::beg);
    if (!CopyBytes(srcFile, tmpDstFile, nBytes))
        return false;


    int64_t nRemainingDestBytes = nDestFileSizeBeforeInsert - nDestOffset;

    if (bVerbose)
        cout << "Copying remaining " << nRemainingDestBytes << " from original destination file.\n";

    if (!CopyBytes(dstFileInput, tmpDstFile, nRemainingDestBytes))
        return false;

    tmpDstFile.close();
    dstFileInput.close();
    srcFile.close();

    std::filesystem::path renamedDestFilename(sDestPath + "_old");

    if (bVerbose)
        cout << "renaming " << sDestPath << " to " << renamedDestFilename << "\n";
    std::filesystem::rename(sDestPath, renamedDestFilename);

    if (bVerbose)
        cout << "renaming " << tempFilename << " to " << sDestPath << "\n";
    std::filesystem::rename(tempFilename, sDestPath);

    if (bVerbose)
        cout << "removing " << renamedDestFilename << "\n";
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

    srcFile.seekg(0, std::ios::end);
    int64_t nSourceSize = srcFile.tellg();
    srcFile.seekg(nSourceOffset, std::ios::beg);

    if (nSourceOffset > nSourceSize)
    {
        cerr << "Source offset:" << nSourceOffset << " > file size:" << nSourceSize << "\n";
        return false;
    }


    if (nSourceOffset + nBytes > nSourceSize)
    {
        nBytes = nSourceSize - nSourceOffset;
        if (bVerbose)
            cout << "Returning " << nBytes << " only due to reaching end of file.\n";
    }

    uint8_t* pBuf = new uint8_t[nBytes];

    srcFile.read((char*)pBuf, nBytes);

    DumpMemoryToCout(pBuf, (uint32_t) nBytes, (uint64_t) nSourceOffset, (uint32_t) nColumns);

    delete[] pBuf;
    return true;
}



int main(int argc, char* argv[])
{
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

    parser.RegisterParam("copy", ParamDesc("SOURCE_FILE", &sSourcePath, CLP::kPositional | CLP::kRequired, "File to read from."));
    parser.RegisterParam("copy", ParamDesc("DEST_FILE", &sDestPath, CLP::kPositional | CLP::kRequired, "File to write to."));
    parser.RegisterParam("copy", ParamDesc("SOURCE_OFFSET", &nSourceOffset, CLP::kPositional | CLP::kRequired, "Source offset"));
    parser.RegisterParam("copy", ParamDesc("DEST_OFFSET", &nDestOffset, CLP::kPositional | CLP::kRequired, "Destination offset"));
    parser.RegisterParam("copy", ParamDesc("BYTES", &nBytes, CLP::kPositional | CLP::kRequired, "Number of bytes to copy."));
    parser.RegisterParam("copy", ParamDesc("overwrite", &bOverwrite, CLP::kNamed , "Overwrites bytes in the destination file (rather than inserting)"));

    parser.RegisterMode("dump", "Dump byte range to cout");
    parser.RegisterParam("dump", ParamDesc("FILE", &sSourcePath, CLP::kPositional | CLP::kRequired, "File to read from."));
    parser.RegisterParam("dump", ParamDesc("OFFSET", &nSourceOffset, CLP::kNamed , "Starting offset"));
    parser.RegisterParam("dump", ParamDesc("BYTES", &nBytes, CLP::kNamed, "Number of bytes to dump."));


    parser.RegisterMode("extract", "Extracts bytes from a file into a new file");
    parser.RegisterParam("extract", ParamDesc("SOURCE_FILE", &sSourcePath, CLP::kPositional | CLP::kRequired, "File to read from."));
    parser.RegisterParam("extract", ParamDesc("DEST_FILE", &sDestPath, CLP::kPositional | CLP::kRequired, "File to create."));
    parser.RegisterParam("extract", ParamDesc("SOURCE_OFFSET", &nSourceOffset, CLP::kPositional | CLP::kRequired, "Source offset"));
    parser.RegisterParam("extract", ParamDesc("BYTES", &nBytes, CLP::kPositional | CLP::kRequired, "Number of bytes to extract."));
    parser.RegisterParam("extract", ParamDesc("dump", &bDumpAfterExtract, CLP::kNamed, "Dump extracted bytes to cout after extraction."));

    parser.RegisterParam(ParamDesc("verbose", &bVerbose, CLP::kNamed , "details"));
    parser.RegisterParam(ParamDesc("COLUMNS", &nColumns, CLP::kNamed, "Number of columns"));

    parser.RegisterAppDescription("Various binary operations on files");

    if (!parser.Parse(argc, argv, bVerbose))
    {
        return -1;
    }

    string sMode = parser.GetAppMode();

    if (CLP::StringCompare(sMode, "copy", false))
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
    else if (CLP::StringCompare(sMode, "extract", false))
    {
        if (!Extract(sSourcePath, sDestPath, nSourceOffset, nBytes, bVerbose))
            return -1;

        if (bDumpAfterExtract)
            Dump(sDestPath, 0, nBytes, nColumns, bVerbose);

    }
    else if (CLP::StringCompare(sMode, "dump", false))
    {
        if (!Dump(sSourcePath, nSourceOffset, nBytes, nColumns, bVerbose))
            return -1;
    }


    std::cout << "Done.\n";

    return 0;
}

