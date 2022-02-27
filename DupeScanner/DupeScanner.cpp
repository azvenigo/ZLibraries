// DupeScanner.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <iostream>
#include <Windows.h>
#include <tchar.h>
#include <locale>
#include <string>
#include "BlockScanner.h"
#include "helpers\CommandLineParser.h"
using namespace std;

std::string sSourcePath;
std::string sScanPath;
BlockScanner scanner;
uint32_t kDefaultBlockSize = 32*1024;
int64_t nThreads = 16;
int64_t nBlockSize = kDefaultBlockSize;

BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
    // Any event, stop the scanner
    wcout << "Canceling\n";
    scanner.Cancel();

    return TRUE;
}

bool ParseCommands(int argc, char* argv[])
{
    CommandLineParser parser;

    parser.RegisterUnnamedString("SOURCE_PATH", &sSourcePath);
    parser.RegisterUnnamedString("SCAN_PATH", &sScanPath);
    parser.RegisterNamedInt64("threads", &nThreads);
    parser.RegisterNamedInt64("blocksize", &nBlockSize);
    parser.RegisterDescription("Indexes source file by blocks and scans for those blocks in scan file.\nIf SCAN_PATH ends in a '/' or '\\' it is assumed to be a folder and all files are scanned at that path recursively.\nIf blocksize is >= the size of the source file, the entirety of the source file is searched for. ");
    if (!parser.Parse(argc, argv))
        return false;

    return true;
}

int _tmain(int argc, char* argv[])
{
    ParseCommands(argc, argv);

    if (sSourcePath.empty() || sScanPath.empty())
    {
        return -1;
    }

    if (!scanner.Scan(sSourcePath, sScanPath, nBlockSize, nThreads))
        return 1;

    DWORD nReportTime = ::GetTickCount();
    const DWORD kReportPeriod = 1000;

    bool bDone = false;
    while (!bDone)
    {
        DWORD nCurrentTime = ::GetTickCount();

        if (scanner.mnStatus == BlockScanner::kError)
        {
            wcout << L"BlockScanner - Error - \"" << scanner.msError.c_str() << "\"";

            bDone = true;
            return 0;
        }

        if (scanner.mnStatus == BlockScanner::kFinished)
        {
            wcout << L"done.\n";
            return 0;
        }

        if (scanner.mnStatus == BlockScanner::kCancelled)
        {
            wcout << L"cancelled.";
            return 0;
        }

        if (nCurrentTime - nReportTime > kReportPeriod)
        {
            nReportTime = nCurrentTime;
//            cout << "Scanning... " << scanner.GetProgress().c_str() << "\n";
        }



        Sleep(100);
    }

	return 0;
}

