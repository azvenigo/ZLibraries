#include <stdio.h>
#include <iostream>
#include <tchar.h>
#include <locale>
#include <string>
#include "helpers/CommandLineParser.h"
#include <fstream>

using namespace std;
using namespace CLP;

int main(int argc, char* argv[])
{
    std::string sSourcePath;
    std::string sDestPath;

    int64_t nSourceOffset = 0;
    int64_t nDestOffset = 0;
    int64_t nBytes = 0;
    bool bVerbose = false;

    CommandLineParser parser;
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



    std::ifstream srcFile;
    srcFile.open(sSourcePath, ios::binary);
    if (!srcFile)
    {
        wcerr << "Failed to open source file:" << sSourcePath.c_str() << "\n";
        return 1;
    }

    srcFile.seekg(0, std::ios::end);
    int64_t nSourceSize = srcFile.tellg();
    srcFile.seekg(nSourceOffset, std::ios::beg);

    if (nSourceOffset + nBytes >= nSourceSize)
    {
        wcerr << "Source range outside of source file bytes.\n";
        return 1;
    }


    std::fstream dstFile;
    dstFile.open(sDestPath, ios::in | ios::out | ios::binary);
    if (!dstFile)
    {
        wcerr << "Failed to open dest file:" << sDestPath.c_str() << "\n";
        return 1;
    }

    dstFile.seekp(nDestOffset, std::ios::beg);


    char* pBytesToCopy = new char[(uint32_t) nBytes];
    if (!pBytesToCopy)
    {
        wcerr << "Failed to allocate:" << nBytes << "\n";
        return 1;
    }

    srcFile.read(pBytesToCopy, nBytes);
    if (srcFile.fail())
    {
        wcerr << "Failed read from source\n";
        return 1;
    }

    dstFile.write(pBytesToCopy, nBytes);
    if (srcFile.fail())
    {
        wcerr << "Failed write to dest\n";
        return 1;
    }

    dstFile.seekp(0, std::ios::end);

    delete[] pBytesToCopy;

    wcout << "Done.\n";

    return 0;
}

