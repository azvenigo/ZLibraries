// DupeScanner.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <iostream>
#include <filesystem>
//#include <tchar.h>
#include <locale>
#include <string>
#include "helpers/LoggingHelpers.h"
#include "BlockScanner.h"
#include "helpers/CommandLineParser.h"
using namespace std;
using namespace CLP;

std::string sSourcePath;
std::string sScanPath;
bool gbVerbose = false;
uint32_t kDefaultBlockSize = 32*1024;
int64_t nThreads = std::thread::hardware_concurrency();
int64_t nBlockSize = kDefaultBlockSize;





int main(int argc, char* argv[])
{
    CommandLineParser parser;

    parser.RegisterMode("diff", "Performs a binary diff, looking for blocks of matching data at one byte offsets in the search path.");
    parser.RegisterParam("diff", ParamDesc("SOURCE_PATH", &sSourcePath, CLP::kPositional | CLP::kRequired, "File/folder to index by blocks."));
    parser.RegisterParam("diff", ParamDesc("SEARCH_PATH", &sScanPath, CLP::kPositional | CLP::kRequired, "File/folder to scan at byte granularity."));
    parser.RegisterParam("diff", ParamDesc("threads", &nThreads, CLP::kNamed, "Number of threads to spawn.", 1, 256));
    parser.RegisterParam("diff", ParamDesc("blocksize", &nBlockSize, CLP::kNamed, "Granularity of blocks to use for scanning.", 16, /*1024 * 1024 * 1024*/32 * 1024 * 1024));
    parser.RegisterParam("diff", ParamDesc("verbose", &gbVerbose, CLP::kNamed , "Detailed output."));

    parser.RegisterMode("find_dupes", "Performs a self-search for blocks of data that exist multiple times in the data.");
    parser.RegisterParam("find_dupes", ParamDesc("PATH", &sSourcePath, CLP::kPositional | CLP::kRequired, "File/folder to index by blocks."));
    parser.RegisterParam("find_dupes", ParamDesc("threads", &nThreads, CLP::kNamed , "Number of threads to spawn.", 1, 256));
    parser.RegisterParam("find_dupes", ParamDesc("blocksize", &nBlockSize, CLP::kNamed , "Granularity of blocks to use for scanning.", 16, /*1024 * 1024 * 1024*/32 * 1024 * 1024));
    parser.RegisterParam("find_dupes", ParamDesc("verbose", &gbVerbose, CLP::kNamed , "Detailed output."));

    parser.RegisterAppDescription("Searches for blocks of data.\nPaths can be individual files or folders where all files are scanned at that path recursively.\nIf blocksize is >= the size of the source file, the entirety of the source file is searched for. ");
    if (!parser.Parse(argc, argv))
        return 1;

    if (sSourcePath.empty())
    {
        cout << parser.GetModesString();
        return -1;
    }

    std::replace(sSourcePath.begin(), sSourcePath.end(), '\\', '/');
    std::replace(sScanPath.begin(), sScanPath.end(), '\\', '/');

    if (!std::filesystem::exists(sSourcePath))
    {
        cerr << "Source Path:" << sSourcePath << " doesn't exist.\n";
        return -1;
    }


    if (!sScanPath.empty())
    {
        if (!std::filesystem::exists(sScanPath))
        {
            cerr << "Scan Path:" << sScanPath << " doesn't exist.\n";
            return -1;
        }
    }

    BlockScanner* pScanner = new BlockScanner();

    if (!pScanner->Scan(sSourcePath, sScanPath, nBlockSize, nThreads, gbVerbose))
        return -1;

    delete pScanner;
	return 0;
}

