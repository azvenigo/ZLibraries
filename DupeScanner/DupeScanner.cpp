// DupeScanner.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <iostream>
#include <filesystem>
#include <tchar.h>
#include <locale>
#include <string>
#include "helpers/LoggingHelpers.h"
#include "BlockScanner.h"
#include "helpers\CommandLineParser.h"
using namespace std;
using namespace CLP;

std::string sSourcePath;
std::string sScanPath;
bool gbVerbose = false;
uint32_t kDefaultBlockSize = 32*1024;
int64_t nThreads = std::thread::hardware_concurrency();
int64_t nBlockSize = kDefaultBlockSize;





int _tmain(int argc, char* argv[])
{
    CommandLineParser parser;

    parser.RegisterParam(ParamDesc("SOURCE_PATH", &sSourcePath, CLP::kPositional | CLP::kRequired, "File/folder to index by blocks."));
    parser.RegisterParam(ParamDesc("SCAN_PATH", &sScanPath, CLP::kPositional | CLP::kRequired, "File/folder to scan at byte granularity."));
    parser.RegisterParam(ParamDesc("threads", &nThreads, CLP::kNamed | CLP::kOptional | CLP::kRangeRestricted, "Number of threads to spawn.", 1, 256));
    parser.RegisterParam(ParamDesc("blocksize", &nBlockSize, CLP::kNamed | CLP::kOptional | CLP::kRangeRestricted, "Granularity of blocks to use for scanning.", 16, /*1024 * 1024 * 1024*/32 * 1024 * 1024));
    parser.RegisterParam(ParamDesc("verbose", &gbVerbose, CLP::kNamed | CLP::kOptional, "Detailed output."));


    parser.RegisterAppDescription("Indexes source file(s) by blocks and scans for those blocks in scan file.\nIf either PATH ends in a '/' or '\\' it is assumed to be a folder and all files are scanned at that path recursively.\nIf blocksize is >= the size of the source file, the entirety of the source file is searched for. ");
    if (!parser.Parse(argc, argv))
        return false;

    if (sSourcePath.empty() || sScanPath.empty())
    {
        parser.OutputUsage();
        return -1;
    }

    std::replace(sSourcePath.begin(), sSourcePath.end(), '\\', '/');
    std::replace(sScanPath.begin(), sScanPath.end(), '\\', '/');

    if (!std::filesystem::exists(sSourcePath))
    {
        cerr << "Source Path:" << sSourcePath << " doesn't exist.\n";
        return -1;
    }

    if (!std::filesystem::exists(sScanPath))
    {
        cerr << "Scan Path:" << sSourcePath << " doesn't exist.\n";
        return -1;
    }

    BlockScanner* pScanner = new BlockScanner();

    if (!pScanner->Scan(sSourcePath, sScanPath, nBlockSize, nThreads, gbVerbose))
        return -1;

    delete pScanner;
	return 0;
}

