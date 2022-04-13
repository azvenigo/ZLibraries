#include <iostream>
#include <fstream>
#include "helpers/CommandLineParser.h"
#include "helpers/InlineFormatter.h"
#include "helpers/RandHelpers.h"
#include <filesystem>

InlineFormatter gFormatter;

namespace fs = std::filesystem;


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



int main(int argc, char* argv[])
{
    string sDestPath;
    string sFilename("data");
    string sExtension("bin");
    int64_t nFolders = 1;
    int64_t nFileSize = 0;
    int64_t nFilesPerFolder = 1;
    bool bRandomFill = false;
    bool bFillSpecificValue = false;
    int64_t nFillValue = 0;
    bool bSkipExistingFiles = false;

    string sDescription = "Creates one or more files in one or more folders.\n"
        "note: default fill is cyclical data. Each 32bit offset is equal to the 32bit value. This is highly compressible and easy to verify partially read data.\n"\
        "note: NUM can also be specified with tags such as KB, KiB, MB, MiB, etc. (example: -filesize:1,024MiB)\n";


    CommandLineParser parser;
    parser.RegisterAppDescription(sDescription);
    parser.RegisterParam(ParamDesc(ParamDesc::kPositional,  ParamDesc::kRequired, "FILE_SIZE",    &nFileSize,  false, 0, 0, "Size of file(s) to create."));
    parser.RegisterParam(ParamDesc(ParamDesc::kNamed,       ParamDesc::kOptional, "dest",         &sDestPath, "base path to where to create data files. defaults to working directory"));
    parser.RegisterParam(ParamDesc(ParamDesc::kNamed,       ParamDesc::kOptional, "filename",     &sFilename, "Name of file to create. Multiple files in a folder will have numbers included."));
    parser.RegisterParam(ParamDesc(ParamDesc::kNamed,       ParamDesc::kOptional, "folders",      &nFolders, false, 0, 0, "number of folders to generate. default is 0"));
    parser.RegisterParam(ParamDesc(ParamDesc::kNamed,       ParamDesc::kOptional, "files",        &nFilesPerFolder, false, 0, 0, "number of files in each folder. default is 1"));
    parser.RegisterParam(ParamDesc(ParamDesc::kNamed,       ParamDesc::kOptional, "skipexisting", &bSkipExistingFiles, "skips overwriting destination file (even with different values or size) if it already exists."));
    parser.RegisterParam(ParamDesc(ParamDesc::kNamed,       ParamDesc::kOptional, "rand",         &bRandomFill, "fill the file with randomized (incompressible) data."));
    parser.RegisterParam(ParamDesc(ParamDesc::kNamed,       ParamDesc::kOptional, "fillvalue",    &nFillValue, false, 0, 0, "fill the file with specific value."));
    parser.RegisterParam(ParamDesc(ParamDesc::kNamed,       ParamDesc::kOptional, "verbose",      &bVerbose, "hear all the gritty details about everthing that's happening. (Can slow down operation due to command line output.)"));

    if (!parser.Parse(argc, argv, true))
    {
        cerr << "Aborting.\n";
        return -1;
    }

    size_t nLastDot = sFilename.find_last_of('.');
    if (nLastDot != string::npos)
    {
        sExtension = sFilename.substr(nLastDot+1);    // everything after the .
        sFilename = sFilename.substr(0, nLastDot);    // everything before the .
    }


    if (!sDestPath.empty())
    {
        char lastChar = sDestPath[sDestPath.length()-1];
        if (lastChar != '/' && lastChar != '\\')
            sDestPath += "/";

        cout << "Creating package path.\n";
        fs::create_directories(sDestPath);
    }
    else
        sDestPath="./";

    for (int64_t i = 0; i < nFolders; i++)
    {
        string sPath;
        
        if (nFolders > 1)
            sPath = gFormatter.Format("%s/%d/", sDestPath.c_str(), i);
        else
            sPath = sDestPath;

        if (!sPath.empty())
            fs::create_directories(sPath);

        for (int64_t i = 0; i < nFilesPerFolder; i++)
        {
            string sPaddingFileName;
            if (nFilesPerFolder > 1)
            {
                char buf[512];
                sprintf_s(buf, "%s%d.%s", sFilename.c_str(), (int) i, sExtension.c_str());
                sPaddingFileName = sPath + string(buf);
            }
            else
                sPaddingFileName = sPath + sFilename + "." + sExtension;

            if (bSkipExistingFiles && fs::exists(sPaddingFileName))
                continue;

            if (bRandomFill)
                CreateRandFile(sPaddingFileName, nFileSize);
            else if (bFillSpecificValue)
                CreateValueFile(sPaddingFileName, nFileSize, nFillValue);
            else
                CreateCyclFile(sPaddingFileName, nFileSize);
        }
 
    }

    cout << "Done.";

    return 0;
}

