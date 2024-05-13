#include <iostream>
#include <fstream>
#include "helpers/CommandLineParser.h"
#include "helpers/InlineFormatter.h"
#include "helpers/RandHelpers.h"
#include "helpers/StringHelpers.h"
#include <filesystem>

InlineFormatter gFormatter;

using namespace CLP;
using namespace std;


bool bVerbose = false;
// 64k increments
void CreateCyclFile(string sPath, int64_t nTotalSize)
{
    if (bVerbose)
        cout << "Creating Cycl file size:" << nTotalSize << " path:" << sPath.c_str() << "...";
    const int kBufferElements = 64 * 1024;
    uint32_t* bufcycl = new uint32_t[kBufferElements];

    // cyclical data
    std::fstream outFile;
    outFile.open(sPath.c_str(), ios_base::out| ios::binary);
    if (outFile.fail())
    {
        cout << "Error creating file:" << errno << "\n";
        return;
    }

    for (int64_t i = 0; i < nTotalSize; i += kBufferElements * sizeof(uint32_t))
    {
        for (int j = 0; j < kBufferElements; j++)
        {
            uint32_t nVal = (uint32_t) (i + (j * sizeof(uint32_t))  );
            *(bufcycl + j) = nVal;
        }

        int32_t nBytesToWrite = kBufferElements * sizeof(uint32_t);
        if (i + nBytesToWrite > nTotalSize)
            nBytesToWrite = (int32_t) (nTotalSize - i);

        outFile.write((char*) bufcycl, nBytesToWrite);
    }

    if (bVerbose)
        cout << "done\n";
    outFile.close();

    delete[] bufcycl;
}

// 64k increments
void CreateRandFile(string sPath, int64_t nTotalSize)
{
    if (bVerbose)
        cout << "Creating Rand file size:" << nTotalSize << " path:" << sPath.c_str() << "...";
    const int kBufferElements = 256 * 1024;
    uint32_t* bufcycl = new uint32_t[kBufferElements];

    // cyclical data
    std::fstream outFile;
    outFile.open(sPath.c_str(), ios_base::out|ios_base::binary);
    if (outFile.fail())
    {
        cout << "Error creating file:" << errno << "\n";
        return;
    }

    for (int64_t i = 0; i < nTotalSize; i += kBufferElements * sizeof(uint32_t))
    {
        for (int j = 0; j < kBufferElements; j++)
            bufcycl[j] = RANDU64(0,0xffffffff);

        // experiment
        


        int32_t nBytesToWrite = kBufferElements * sizeof(uint32_t);
        if (i + nBytesToWrite > nTotalSize)
            nBytesToWrite = (int32_t) (nTotalSize - i);

        outFile.write((char*)&bufcycl[0], nBytesToWrite);
    }

    if (bVerbose)
        cout << "done\n";
    outFile.close();

    delete[] bufcycl;
}


void CreateValueFile(string sPath, int64_t nTotalSize, int64_t nFillValue)
{
    if (bVerbose)
        cout << "Creating Value file size:" << nTotalSize << " path:" << sPath.c_str() << "...";
    const int kBufferElements = 256 * 1024;
    int64_t* buf = new int64_t[kBufferElements];
    for (int j = 0; j < kBufferElements; j++)
        buf[j] = nFillValue;


    // cyclical data
    std::fstream outFile;
    outFile.open(sPath.c_str(), ios_base::out | ios_base::binary);
    if (outFile.fail())
    {
        cout << "Error creating file:" << errno << "\n";
        return;
    }

    for (int64_t i = 0; i < nTotalSize; i += kBufferElements * sizeof(int64_t))
    {

        int32_t nBytesToWrite = kBufferElements * sizeof(int64_t);
        if (i + nBytesToWrite > nTotalSize)
            nBytesToWrite = (int32_t)(nTotalSize - i);

        outFile.write((char*)&buf[0], nBytesToWrite);
    }

    if (bVerbose)
        cout << "done\n";
    outFile.close();

    delete[] buf;
}

void CreateCompressibleFile(string sPath, int64_t nTotalSize, int64_t nCompressFactor)
{
    if (bVerbose)
        cout << "Creating Compressible file size:" << nTotalSize << "Compress factor:" << nCompressFactor << " path:" << sPath.c_str() << "...";
    const int kBufferElements = 64 * 1024;
    uint32_t* bufcycl = new uint32_t[kBufferElements];

    // cyclical data
    std::fstream outFile;
    outFile.open(sPath.c_str(), ios_base::out | ios::binary);
    if (outFile.fail())
    {
        cout << "Error creating file:" << errno << "\n";
        return;
    }

    for (int64_t i = 0; i < nTotalSize; i += kBufferElements * sizeof(uint32_t))
    {
        for (int j = 0; j < kBufferElements; j++)
        {
            if (j % nCompressFactor == 0)
                *(bufcycl + j) = RANDU64(0, 0xffffffff);
            else
                *(bufcycl + j) = 0;
        }

        int32_t nBytesToWrite = kBufferElements * sizeof(uint32_t);
        if (i + nBytesToWrite > nTotalSize)
            nBytesToWrite = (int32_t)(nTotalSize - i);

        outFile.write((char*)bufcycl, nBytesToWrite);
    }

    if (bVerbose)
        cout << "done\n";
    outFile.close();

    delete[] bufcycl;
}

void CreateVariableFile(string sPath, int64_t nTotalSize, int64_t nCompressFactor)
{
    if (bVerbose)
        cout << "Creating Variable Compressible file size:" << nTotalSize << "Compress factor:" << nCompressFactor << " path:" << sPath.c_str() << "...";
    const int kBufferElements = 64 * 1024;
    uint32_t* bufcycl = new uint32_t[kBufferElements];

    // cyclical data
    std::fstream outFile;
    outFile.open(sPath.c_str(), ios_base::out | ios::binary);
    if (outFile.fail())
    {
        cout << "Error creating file:" << errno << "\n";
        return;
    }

    for (int64_t i = 0; i < nTotalSize; i += kBufferElements * sizeof(uint32_t))
    {
        for (int j = 0; j < kBufferElements; j++)
        {
            if (RANDI64(0,900) < (750 / nCompressFactor) )
                *(bufcycl + j) = RANDU64(0, 0xffffffff);
            else
                *(bufcycl + j) = (uint32_t)i;
        }

        int32_t nBytesToWrite = kBufferElements * sizeof(uint32_t);
        if (i + nBytesToWrite > nTotalSize)
            nBytesToWrite = (int32_t)(nTotalSize - i);

        outFile.write((char*)bufcycl, nBytesToWrite);
    }

    if (bVerbose)
        cout << "done\n";
    outFile.close();

    delete[] bufcycl;
}

int main(int argc, char* argv[])
{
//    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
//    SetConsoleMode(hConsole, ENABLE_VIRTUAL_TERMINAL_PROCESSING);







    string sDestPath;
    string sFilename("data");
    string sExtension("bin");
    int64_t nFolders = 1;
    int64_t nFileSize = 0;
    int64_t nFilesPerFolder = 1;
    bool bRandomFill = false;
    bool bVariableFill = false;
    bool bFillSpecificValue = false;
    int64_t nFillValue = 0;
    bool bSkipExistingFiles = false;
    bool bCompressFactorFill = false;
    int64_t nCompressFactor = 10;

    CLP::DisableCols();

    CommandLineParser parser;
    parser.RegisterAppDescription("Creates one or more files in one or more folders.");

    parser.RegisterMode("rand", "Fill the file with random (uncompressible) data.");

    parser.RegisterMode("cycl", "Fill the file with cyclical data where each 32bit offset into the file is equal to the 32bit value at that offset.");

    parser.RegisterMode("compressible", "Fill the file with random values every so often to achieve roughly a compressable ratio.");
    parser.RegisterParam("compressible", ParamDesc("FACTOR", &nCompressFactor, CLP::kPositional | CLP::kRequired, "Goal compression factor. (roughly)"));

    parser.RegisterMode("variable", "Fill the file with random (variable compression) data.");
    parser.RegisterParam("variable", ParamDesc("FACTOR", &nCompressFactor, CLP::kPositional | CLP::kRequired, "Goal compression factor. (roughly)"));


    parser.RegisterMode("value", "Fill the file specific value.");
    parser.RegisterParam("value", ParamDesc("FILLVALUE", &nFillValue, CLP::kPositional | CLP::kRequired, "specific value to fill the file with"));



    parser.RegisterParam(ParamDesc("FILENAME",      &sFilename,         CLP::kPositional| CLP::kRequired|CLP::kPath, "Name of file to create. Multiple files in a folder will have numbers included."));
    parser.RegisterParam(ParamDesc("SIZE",          &nFileSize,         CLP::kPositional | CLP::kRequired | CLP::kRangeRestricted,  "Size of file(s) to create.", 100, 500));

    parser.RegisterParam(ParamDesc("dest",         &sDestPath,          CLP::kNamed | CLP::kPath,       "base path to where to create data files. defaults to working directory"));
    parser.RegisterParam(ParamDesc("folders",      &nFolders,           CLP::kNamed,       "number of folders to generate. default is 0"));
    parser.RegisterParam(ParamDesc("files",        &nFilesPerFolder,    CLP::kNamed,       "number of files in each folder. default is 1"));

    parser.RegisterParam(ParamDesc("skipexisting", &bSkipExistingFiles, CLP::kNamed ,       "skips overwriting destination file (even with different values or size) if it already exists."));
//    parser.RegisterParam(ParamDesc("verbose",      &bVerbose,           CLP::kNamed,       "hear all the gritty details about everthing that's happening. (Can slow down operation due to command line output.)"));


    bool bParseSuccess = parser.Parse(argc, argv);
    if (!bParseSuccess)
    {
        cerr << "Aborting.\n";
        return -1;
    }
    
    bFillSpecificValue = SH::Compare(parser.GetAppMode(), "value", false);
    bRandomFill = SH::Compare(parser.GetAppMode(), "rand", false);
    bCompressFactorFill = SH::Compare(parser.GetAppMode(), "compressible", false);
    bVariableFill = SH::Compare(parser.GetAppMode(), "variable", false);

    size_t nLastDot = sFilename.find_last_of('.');
    if (nLastDot != string::npos)
    {
        sExtension = sFilename.substr(nLastDot+1);    // everything after the .
        sFilename = sFilename.substr(0, nLastDot);    // everything before the .
    }

    if (nFilesPerFolder > 1 || nFolders > 1)
    {
        if (!sDestPath.empty())
        {
            char lastChar = sDestPath[sDestPath.length() - 1];
            if (lastChar != '/' && lastChar != '\\')
                sDestPath += "/";
        }
        else
            sDestPath = "./";
    }

    for (int64_t i = 0; i < nFolders; i++)
    {
        string sPath;
        
        if (nFolders > 1)
            sPath = gFormatter.Format("%s/%d/", sDestPath.c_str(), i);
        else
            sPath = sDestPath;

        if (!sPath.empty())
            filesystem::create_directories(sPath);

        for (int64_t i = 0; i < nFilesPerFolder; i++)
        {
            string sGeneratedFilename;
            if (nFilesPerFolder > 1)
            {
                char buf[512];
                sprintf(buf, "%s%d.%s", sFilename.c_str(), (int) i, sExtension.c_str());
                sGeneratedFilename = sPath + string(buf);
            }
            else
                sGeneratedFilename = sPath + sFilename + "." + sExtension;

            if (bSkipExistingFiles && filesystem::exists(sGeneratedFilename))
                continue;

            if (bRandomFill)
                CreateRandFile(sGeneratedFilename, nFileSize);
            else if (bFillSpecificValue)
                CreateValueFile(sGeneratedFilename, nFileSize, nFillValue);
            else if (bCompressFactorFill)
                CreateCompressibleFile(sGeneratedFilename, nFileSize, nCompressFactor);
            else if (bVariableFill)
                CreateVariableFile(sGeneratedFilename, nFileSize, nCompressFactor);
            else
                CreateCyclFile(sGeneratedFilename, nFileSize);
        }
 
    }

    cout << "Done.";

    return 0;
}
