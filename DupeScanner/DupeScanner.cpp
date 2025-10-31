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
namespace fs = std::filesystem;

std::string sSourcePath;
std::string sScanPath;
uint32_t kDefaultBlockSize = 32*1024;
int64_t nThreads = std::thread::hardware_concurrency();
int64_t nBlockSize = kDefaultBlockSize;


void DiffFolders(fs::path source, fs::path dest)
{

    Table diff;
    diff.SetBorders("", "*", "", "*");
    diff.SetRenderWidth(gConsole.Width());

    diff.AddRow("SRC", source);
    diff.AddRow("DST", dest);


    diff.AddRow(SectionStyle, " Files in SRC but not in DST ");

    size_t filesSrc = 0;
    size_t missing = 0;
    for (auto filePath : std::filesystem::recursive_directory_iterator(source))
    {
        if (filePath.is_regular_file())
        {
            filesSrc++;
            fs::path destRel = fs::relative(filePath.path(), source);
            fs::path destFull = dest;
            destFull.append(destRel.string());

            if (!fs::exists(destFull))
            {
                missing++;
                diff.AddRow(filePath.path().string());
            }
        }
    }

    diff.AddRow("Files checked from Src:", filesSrc);
    diff.AddRow("Missing from DST:", missing);

    cout << diff;
}


int main(int argc, char* argv[])
{
    CommandLineParser parser;

    parser.RegisterMode("diff", "Binary diff. Looks for blocks of matching data at one byte offsets.");
    parser.RegisterParam("diff", ParamDesc("SOURCE_PATH", &sSourcePath, CLP::kPositional | CLP::kRequired| CLP::kExistingPath, "File/folder to index by blocks."));
    parser.RegisterParam("diff", ParamDesc("SEARCH_PATH", &sScanPath, CLP::kPositional | CLP::kRequired | CLP::kExistingPath, "File/folder to scan at byte granularity."));
    parser.RegisterParam("diff", ParamDesc("threads", &nThreads, CLP::kNamed, "Number of threads to spawn.", 1, 256));
    parser.RegisterParam("diff", ParamDesc("blocksize", &nBlockSize, CLP::kNamed, "Granularity of blocks to use for scanning.", 16, /*1024 * 1024 * 1024*/32 * 1024 * 1024));

    parser.RegisterMode("filename_diff", "Looks only at filenames in SOURCE that are not in DEST");
    parser.RegisterParam("filename_diff", ParamDesc("SOURCE", &sSourcePath, CLP::kPositional | CLP::kRequired | CLP::kExistingPath, "folder to index"));
    parser.RegisterParam("filename_diff", ParamDesc("DEST", &sScanPath, CLP::kPositional | CLP::kRequired | CLP::kExistingPath, "Folder to compare against."));

    parser.RegisterMode("find_dupes", "Self-search for blocks of data that exist multiple times in the data.");
    parser.RegisterParam("find_dupes", ParamDesc("PATH", &sSourcePath, CLP::kPositional | CLP::kRequired | CLP::kExistingPath, "File/folder to index by blocks."));
    parser.RegisterParam("find_dupes", ParamDesc("threads", &nThreads, CLP::kNamed , "Number of threads to spawn.", 1, 256));
    parser.RegisterParam("find_dupes", ParamDesc("blocksize", &nBlockSize, CLP::kNamed , "Granularity of blocks to use for scanning.", 16, /*1024 * 1024 * 1024*/32 * 1024 * 1024));

    parser.RegisterAppDescription("Searches for blocks of data.\nPaths can be individual files or folders where all files are scanned at that path recursively.\nIf blocksize is >= the size of the source file, the entirety of the source file is searched for. ");
    if (!parser.Parse(argc, argv))
        return 1;

    std::replace(sSourcePath.begin(), sSourcePath.end(), '\\', '/');
    std::replace(sScanPath.begin(), sScanPath.end(), '\\', '/');

    if (!std::filesystem::exists(sSourcePath))
    {
        cerr << "Source Path:" << sSourcePath << " doesn't exist.\n";
        return -1;
    }

    if (parser.IsCurrentMode("filename_diff"))
    {
        DiffFolders(sSourcePath, sScanPath);
        return 0;
    }

    if (parser.IsCurrentMode("diff") || parser.IsCurrentMode("find_dupes"))
    {
        if (!sScanPath.empty())
        {
            if (!std::filesystem::exists(sScanPath))
            {
                cerr << "Scan Path:" << sScanPath << " doesn't exist.\n";
                return -1;
            }
        }

        BlockScanner* pScanner = new BlockScanner();

        if (!pScanner->Scan(sSourcePath, sScanPath, nBlockSize, nThreads))
            return -1;

        delete pScanner;
        return 0;
    }

    cout << "Error: unknown command\n";
    return -1;
}

