#include <stdio.h>
#include <iostream>
#include <locale>
#include <string>
#include <filesystem>
#include "helpers/CommandLineParser.h"
#include <fstream>

using namespace std;
using namespace CLP;

bool Overwrite(const std::string& sSourcePath, const std::string& sDestPath, int64_t nSourceOffset, int64_t nDestOffset, int64_t nBytes, bool /*bVerbose*/)
{
    std::ifstream srcFile;
    srcFile.open(sSourcePath, ios::binary);
    if (!srcFile)
    {
        wcerr << "Failed to open source file:" << sSourcePath.c_str() << "\n";
        return false;
    }

    srcFile.seekg(0, std::ios::end);
    int64_t nSourceSize = srcFile.tellg();
    srcFile.seekg(nSourceOffset, std::ios::beg);

    if (nSourceOffset + nBytes >= nSourceSize)
    {
        wcerr << "Source range outside of source file bytes.\n";
        return false;
    }


    std::fstream dstFile;
    dstFile.open(sDestPath, ios::in | ios::out | ios::binary);
    if (!dstFile)
    {
        wcerr << "Failed to open dest file:" << sDestPath.c_str() << "\n";
        return false;
    }

    dstFile.seekp(nDestOffset, std::ios::beg);


    char* pBytesToCopy = new char[(uint32_t)nBytes];
    if (!pBytesToCopy)
    {
        wcerr << "Failed to allocate:" << nBytes << "\n";
        return false;
    }

    srcFile.read(pBytesToCopy, nBytes);
    if (srcFile.fail())
    {
        wcerr << "Failed read from source\n";
        return false;
    }

    dstFile.write(pBytesToCopy, nBytes);
    if (srcFile.fail())
    {
        wcerr << "Failed write to dest\n";
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

bool Insert(const std::string& sSourcePath, const std::string& sDestPath, int64_t nSourceOffset, int64_t nDestOffset, int64_t nBytes, bool bVerbose)
{
    std::ifstream srcFile;
    srcFile.open(sSourcePath, ios::binary);
    if (!srcFile)
    {
        wcerr << "Failed to open source file:" << sSourcePath.c_str() << "\n";
        return false;
    }

    srcFile.seekg(0, std::ios::end);
    int64_t nSourceSize = srcFile.tellg();
    srcFile.seekg(nSourceOffset, std::ios::beg);

    if (nSourceOffset + nBytes >= nSourceSize)
    {
        wcerr << "Source range outside of source file bytes.\n";
        return false;
    }


    std::ifstream dstFileInput;
    dstFileInput.open(sDestPath, ios::in | ios::out | ios::binary);
    if (!dstFileInput)
    {
        wcerr << "Failed to open dest file:" << sDestPath.c_str() << "\n";
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
        wcerr << "Failed to open temporary dest file:" << tempFilename.c_str() << "\n";
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



int main(int argc, char* argv[])
{
    std::string sSourcePath;
    std::string sDestPath;

    int64_t nSourceOffset = 0;
    int64_t nDestOffset = 0;
    int64_t nBytes = 0;
    bool bVerbose = false;

    CommandLineParser parser;



    parser.RegisterMode("overwrite", "Overwrites bytes in the destination file with those from the source file.");

    parser.RegisterMode("insert", "Inserts bytes into the destination file, shifting the remaining bytes.");

    parser.RegisterParam(ParamDesc("SOURCE_FILE", &sSourcePath, CLP::kPositional | CLP::kRequired, "File to read from."));
    parser.RegisterParam(ParamDesc("DEST_FILE", &sDestPath, CLP::kPositional | CLP::kRequired, "File to write to."));
    parser.RegisterParam(ParamDesc("SOURCE_OFFSET", &nSourceOffset, CLP::kPositional | CLP::kRequired, "Source offset"));
    parser.RegisterParam(ParamDesc("DEST_OFFSET", &nDestOffset, CLP::kPositional | CLP::kRequired, "Destination offset"));
    parser.RegisterParam(ParamDesc("BYTES", &nBytes, CLP::kPositional | CLP::kRequired, "Number of bytes to copy."));

    parser.RegisterParam(ParamDesc("verbose", &bVerbose, CLP::kNamed | CLP::kOptional, "details"));

    parser.RegisterAppDescription("Copies bytes from a source file/offset into a destination file/offset.");

    if (!parser.Parse(argc, argv, bVerbose))
    {
        return -1;
    }

    if (parser.GetAppMode() == "overwrite")
    {
        if (!Overwrite(sSourcePath, sDestPath, nSourceOffset, nDestOffset, nBytes, bVerbose))
            return -1;
    }
    else if (parser.GetAppMode() == "insert")
    {
        if (!Insert(sSourcePath, sDestPath, nSourceOffset, nDestOffset, nBytes, bVerbose))
            return -1;
    }



    wcout << "Done.\n";

    return 0;
}

