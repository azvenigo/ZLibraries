#include <stdio.h>
#include <iostream>
#include <Windows.h>
#include <tchar.h>
#include <locale>
#include <string>
#include "helpers/CommandLineParser.h"
#include <fstream>

using namespace std;

std::string sSourcePath;
std::string sDestPath;

int64_t nSourceOffset;
int64_t nDestOffset;
int64_t nBytes;


bool ParseCommands(int argc, char* argv[])        // returns false if usage needs to be shown
{
    CommandLineParser parser;
    parser.SetRequiredNumberOfUnnamedParameters(5);
    parser.RegisterUnnamedString("SOURCE_FILE", &sSourcePath);
    parser.RegisterUnnamedString("DEST_FILE", &sDestPath);
    parser.RegisterUnnamedInt64("SOURCE_OFFSET", &nSourceOffset);
    parser.RegisterUnnamedInt64("DEST_OFFSET", &nDestOffset);
    parser.RegisterUnnamedInt64("BYTES", &nBytes);
    parser.RegisterDescription("Copies bytes from a source file/offset into a destination file/offset.");

    return parser.Parse(argc, argv, true);
}

int main(int argc, char* argv[])
{
    if (!ParseCommands(argc, argv))
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
