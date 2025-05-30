// MIT License
// Copyright 2019 Alex Zvenigorodsky
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

/*#define _CRTDBG_MAP_ALLOC  
#include <stdlib.h>  
#include <crtdbg.h>  */

//#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <iostream>
//#include <tchar.h>
#include <locale>
#include <string>
#include "ZipHeaders.h"
#include "zlibAPI.h"
#include <filesystem>
#include "ZipJob.h"
#include "helpers/CommandLineParser.h"

using namespace std;

// App Globals for reading command line
ZipJob::eJobType    gCommand        = ZipJob::eJobType::kNone;
string             gsPackageURL;	                            // source package URL
string             gsAuthName;
string             gsAuthPassword;
string             gsBaseFolder;                               // base folder. (default is the folder of ZZip.exe)
string             gsPattern("*");                              // wildcard pattern to match (example "*base*/*.exe"  matches all directories that have the string "base" in them and in those directories all files that end in .exe)
bool                gbSkipCRC		= false;                    // Whether to bypass CRC checks when doing sync
//bool                gbKill			= false;                    // TBD
int64_t            gNumThreads		= std::thread::hardware_concurrency();;	                    // Multithreaded sync/extraction
string              gsOutputFormat;
eToStringFormat     gOutputFormat	= kTabs;                    // For lists or diff operations, output in various formats
extern bool         gbSkipCertCheck;


using namespace CLP;

int main(int argc, char* argv[])
{
//	_CrtMemState s1;
//	_CrtMemCheckpoint(&s1);

    CommandLineParser parser;

    parser.RegisterAppDescription("Performs various operations on ZIP archives whether local or remote.");

    parser.RegisterMode("list", "Lists the content of a zip file.");
    parser.AddInfo("list", "    - ZIPFILE can be a fully qualified web URL or a local path.\n"\
                               "    - For example the following are all ok:\n"\
                               "    - http://www.example.com/sample.zip\n"\
                               "    - https://www.example.com/sample.zip\n"\
                               "    - c:/example/sample.zip\n");

    parser.RegisterParam("list", ParamDesc("ZIPFILE", &gsPackageURL, CLP::kPositional | CLP::kRequired, "Path or URL to a ZIP archive"));
    parser.RegisterParam("list", ParamDesc("output_format", &gsOutputFormat, CLP::kNamed | CLP::kOptional, "Report format. { plain, html, tabs, commas }"));

    parser.RegisterMode("create", "Creates a ZIP archive from a given folder or file.");
    parser.RegisterParam("create", ParamDesc("ZIPFILE", &gsPackageURL, CLP::kPositional | CLP::kRequired, "Path of the ZIP archive to create."));
    parser.RegisterParam("create", ParamDesc("FOLDER", &gsBaseFolder, CLP::kPositional | CLP::kRequired | CLP::kExistingPath, "Base folder of files add to the archive"));

    parser.RegisterMode("diff", "Compares the contents of a ZIP archive with a local folder and reports the differences." );
    parser.RegisterParam("diff", ParamDesc("ZIPFILE", &gsPackageURL, CLP::kPositional | CLP::kRequired, "Path or URL to a ZIP archive"));
    parser.RegisterParam("diff", ParamDesc("FOLDER", &gsBaseFolder, CLP::kPositional | CLP::kRequired | CLP::kExistingPath, "Base folder to diff against"));

    parser.RegisterMode("update", "Compares the contents of a ZIP archive with a local folder and extracts all files that are new or different.");
    parser.RegisterParam("update", ParamDesc("ZIPFILE", &gsPackageURL, CLP::kPositional | CLP::kRequired, "Path or URL to a ZIP archive"));
    parser.RegisterParam("update", ParamDesc("FOLDER", &gsBaseFolder, CLP::kPositional | CLP::kRequired | CLP::kExistingPath, "Base folder to update"));
    parser.RegisterParam("update", ParamDesc("skipcrc", &gbSkipCRC, CLP::kNamed | CLP::kOptional, "Skip CRC checks for matching files and overwrite everything when doing an update. (Same behavior as extract.)"));
    parser.RegisterParam("update", ParamDesc("pattern", &gsPattern, CLP::kPositional | CLP::kOptional, "Wildcard pattern to use when filtering filenames"));

    parser.RegisterMode("extract", "Extracts files from a ZIP archive.");
    parser.RegisterParam("extract", ParamDesc("ZIPFILE", &gsPackageURL, CLP::kPositional | CLP::kRequired, "Path or URL to a ZIP archive"));
    parser.RegisterParam("extract", ParamDesc("FOLDER", &gsBaseFolder, CLP::kPositional | CLP::kRequired | CLP::kPath, "Base folder to extract to"));
    parser.RegisterParam("extract", ParamDesc("pattern", &gsPattern, CLP::kPositional | CLP::kOptional, "Wildcard pattern to use when filtering filenames"));

    parser.RegisterParam(ParamDesc("pattern", &gsPattern, CLP::kNamed | CLP::kOptional, "Wildcard pattern to use when filtering filenames"));

    parser.RegisterParam(ParamDesc("name", &gsAuthName, CLP::kNamed | CLP::kOptional, "Auth name"));
    parser.RegisterParam(ParamDesc("password", &gsAuthPassword, CLP::kNamed | CLP::kOptional, "Auth password"));

    parser.RegisterParam(ParamDesc("threads", &gNumThreads, CLP::kNamed | CLP::kOptional, "Number of threads to use when updating or extracting. Defaults to number of CPU cores.", 1, 256));
    parser.RegisterParam(ParamDesc("skip_cert_check", &gbSkipCertCheck, CLP::kNamed | CLP::kOptional, "If true, bypasses certificate verification on secure connetion. (Careful!)"));

    if (!parser.Parse(argc, argv))
        return -1;


    if (gsPackageURL.find("*") != std::string::npos)
    {
        zout << "ERROR: URL \"" << gsPackageURL << "\" includes illegal wildcards.\n";
        return -1;
    }

    if (gsBaseFolder.find("?") != std::string::npos ||
        gsBaseFolder.find("*") != std::string::npos)
    {
        zout << "ERROR: Folder \"" << gsBaseFolder << "\" includes illegal wildcards.\n";
        return -1;
    }

    if (gsOutputFormat == "html")
        gOutputFormat = kHTML;
    else if (gsOutputFormat == "tabs")
        gOutputFormat = kTabs;
    else if (gsOutputFormat == "commas")
        gOutputFormat = kCommas;
    else
        gOutputFormat = kUnknown;


    if (parser.GetAppMode() == "list")
        gCommand = ZipJob::kList;
    else if (parser.GetAppMode() == "diff")
        gCommand = ZipJob::kDiff;
    else if (parser.GetAppMode() == "create")
        gCommand = ZipJob::kCompress;
    else if (parser.GetAppMode() == "update")
    {
        gCommand = ZipJob::kExtract;
        gbSkipCRC = false;
    }
    else if (parser.GetAppMode() == "extract")
    {
        gCommand = ZipJob::kExtract;
        gbSkipCRC = true;
    }
    else
    {
        cerr << "ERROR: Unknown operation mode:" << parser.GetAppMode() << "\n";
        return -1;
    }



    ZipJob newJob(gCommand);
    newJob.SetBaseFolder(gsBaseFolder);
    newJob.SetURL(gsPackageURL);
    newJob.SetNamePassword(gsAuthName, gsAuthPassword);
    newJob.SetSkipCRC(gbSkipCRC);
    newJob.SetNumThreads((uint32_t) gNumThreads);
    newJob.SetOutputFormat(gOutputFormat);
    newJob.SetPattern(gsPattern);
    newJob.SetVerbose(LOG::gnVerbosityLevel > LVL_DEFAULT);

    newJob.Run();
    newJob.Join();  // will output progress to zout until completed

    zout << std::flush;
    wcout << std::flush;

//	_CrtMemDumpStatistics(&s1);  
//	_CrtDumpMemoryLeaks();

	return 0;

}

