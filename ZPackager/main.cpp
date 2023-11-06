// MIT License
// Copyright 2023 Alex Zvenigorodsky
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
#include <locale>
#include <string>
#include "ZipHeaders.h"
#include "ZZipAPI.h"
#include "zlibAPI.h"
#include <filesystem>
#include "ZipJob.h"
#include "helpers/CommandLineParser.h"
#include "helpers/ThreadPool.h"

#ifdef _WIN64
#include <Windows.h>
#include <shlobj.h>
#include <aclapi.h>
#endif


using namespace std;
using namespace CLP;


filesystem::path exeFilename;

bool ContainsEmbeddedArchive(filesystem::path exeFilename)
{
    ZZipAPI zipAPI;
    if (!zipAPI.Init(exeFilename.string(), ZZipAPI::kZipOpen))
    {
        return false;
    }

    return true;
}


int Create(filesystem::path baseFolder, filesystem::path outputFilename)
{

    filesystem::path outputFolder = outputFilename.parent_path();


    ifstream exeFile(exeFilename, ios::binary);
    size_t nEXESize = filesystem::file_size(exeFilename);

    if (!exeFile)
    {
        cerr << "Failed to open myself for cloning\n";
        return -1;
    }

    char* pBuf = new char[nEXESize];

    exeFile.read(pBuf, nEXESize);

    ofstream outFile(outputFilename, ios::binary | ios::trunc);
    outFile.write(pBuf, nEXESize);

    exeFile.close();
    outFile.close();



    delete[] pBuf;


    ZZipAPI zipAPI;
    if (!zipAPI.Init(outputFilename.string(), ZZipAPI::kZipAppend))
    {
        cerr << "Failed to create output file\n";
        return -1;
    }



    uint64_t nTotalFilesSkipped = 0;

    tCDFileHeaderList filesToCompress;

    // Compute files to add (so that we have the total size of the job)
    uint64_t nTotalBytes = 0;
    std::filesystem::path compressFolder(baseFolder);

    for (auto it : std::filesystem::recursive_directory_iterator(compressFolder))
    {
        if (LOG::gnVerbosityLevel > LVL_DEFAULT)
            cout << "Found:" << it;

        cCDFileHeader fileHeader;
        fileHeader.mFileName = it.path().generic_string();
        if (is_regular_file(it.path()))
        {
            fileHeader.mUncompressedSize = file_size(it.path());
            nTotalBytes += fileHeader.mUncompressedSize;
        }

        filesToCompress.push_back(fileHeader);
    }

    cout << "Found " << filesToCompress.size() << " files.  Total size: " << FormatFriendlyBytes(nTotalBytes, SH::kMiB) << " (" << nTotalBytes << " bytes)\n";

    // Add files one at a time
    for (auto fileHeader : filesToCompress)
    {
        if (LOG::gnVerbosityLevel > LVL_DEFAULT)
            cout << "Adding to Zip File: " << fileHeader.mFileName << "\n";
        zipAPI.AddToZipFile(fileHeader.mFileName, baseFolder.string());
    }

    cout << "Finished\n";

    return 0;
}

bool ExtractInstallFile(const string& sArchiveFilename, string& sResult)
{
    ZZipAPI zipAPI;
    if (!zipAPI.Init(exeFilename.string()))
    {
        cerr << "ERROR: Couldn't open self archive:\"" << exeFilename << "\" for Decompression Job!";
        return false;
    }

    cZipCD& zipCD = zipAPI.GetZipCD();

    if (LOG::gnVerbosityLevel >= LVL_DIAG_BASIC)
        zipCD.DumpCD(cout, "*", true, SH::eToStringFormat::kTabs);


    cCDFileHeader fileHeader;
    if (!zipCD.GetFileHeader(sArchiveFilename, fileHeader))
    {
        cerr << "Couldn't extract file:" << sArchiveFilename << "\n";
        return false;
    }

    bool bSuccess = true;
    uint8_t* pBuf = new uint8_t[fileHeader.mUncompressedSize];

    bSuccess = zipAPI.DecompressToBuffer(sArchiveFilename, pBuf);
    if (bSuccess)
        sResult.assign((const char*)pBuf, fileHeader.mUncompressedSize);
    delete[] pBuf;
    return bSuccess;
}

int Extract(filesystem::path outputFolder)
{
    ZZipAPI zipAPI;
    if (!zipAPI.Init(exeFilename.string()))
    {
        cerr << "ERROR: Couldn't open self archive:\"" << exeFilename << "\" for Decompression Job!";
        return -1;
    }

    cZipCD& zipCD = zipAPI.GetZipCD();

    tCDFileHeaderList filesToDecompress;
    uint64_t nTotalFilesSkipped = 0;
    uint64_t nTotalTimeOnFileVerification = 0;
    uint64_t nTotalBytesVerified = 0;

    // Create folder structure and build list of files that match pattern
    cout << "Creating Folders.\n";
    for (tCDFileHeaderList::iterator it = zipCD.mCDFileHeaderList.begin(); it != zipCD.mCDFileHeaderList.end(); it++)
    {
        cCDFileHeader& cdFileHeader = *it;

        std::filesystem::path fullPath(outputFolder);
        fullPath += cdFileHeader.mFileName;

        // If the path ends in '/' it's a folder and shouldn't be processed for decompression
        if (cdFileHeader.mFileName[cdFileHeader.mFileName.length() - 1] != '/')
            filesToDecompress.push_back(cdFileHeader);
    }

//    ThreadPool pool(std::thread::hardware_concurrency());
    ThreadPool pool(1);
    vector<shared_future<DecompressTaskResult> > decompResults;

    for (auto cdHeader : filesToDecompress)
    {
        decompResults.emplace_back(pool.enqueue([=, &zipCD, &zipAPI, &nTotalTimeOnFileVerification, &nTotalBytesVerified]
            {
                if (cdHeader.mFileName.length() == 0)
                    return DecompressTaskResult(DecompressTaskResult::kAlreadyUpToDate, 0, 0, 0, 0, "", "empty filename.");

                std::filesystem::path fullPath(outputFolder);
                fullPath += cdHeader.mFileName;


                // decompress
                if (!filesystem::is_directory(fullPath.parent_path()))
                {
                    cout << "Creating Path: \"" << fullPath.parent_path().c_str() << "\"\n";
                    std::filesystem::create_directories(fullPath.parent_path());
                }

                // If the path ends in '/' it's a folder and shouldn't be processed for decompression
                if (cdHeader.mFileName[cdHeader.mFileName.length() - 1] != '/')
                {
                    if (zipAPI.DecompressToFile(cdHeader.mFileName, fullPath.generic_string()))
                    {
                        return DecompressTaskResult(DecompressTaskResult::kExtracted, 0, cdHeader.mCompressedSize, cdHeader.mUncompressedSize, 0, cdHeader.mFileName, "Extracted File");
                    }
                    else
                    {
                        return DecompressTaskResult(DecompressTaskResult::kError, 0, 0, 0, 0, cdHeader.mFileName, "Error Decompressing to File");
                    }
                }

                return DecompressTaskResult(DecompressTaskResult::kFolderCreated, 0, 0, 0, 0, cdHeader.mFileName, "Created Folder");
            }));
    }

    uint64_t nTotalBytesDownloaded = 0;
    uint64_t nTotalWrittenToDisk = 0;
    uint64_t nTotalFoldersCreated = 0;
    uint64_t nTotalErrors = 0;
    uint64_t nTotalFilesUpToDate = 0;
    uint64_t nTotalFilesUpdated = 0;
    for (auto& result : decompResults)
    {
        DecompressTaskResult taskResult = result.get();
        if (taskResult.mDecompressTaskStatus == DecompressTaskResult::kError)
            nTotalErrors++;
        else if (taskResult.mDecompressTaskStatus == DecompressTaskResult::kAlreadyUpToDate)
            nTotalFilesUpToDate++;
        else if (taskResult.mDecompressTaskStatus == DecompressTaskResult::kExtracted)
            nTotalFilesUpdated++;
        else if (taskResult.mDecompressTaskStatus == DecompressTaskResult::kFolderCreated)
            nTotalFoldersCreated++;

        nTotalBytesDownloaded += taskResult.mBytesDownloaded;
        nTotalWrittenToDisk += taskResult.mBytesWrittenToDisk;

        //		cout << taskResult << "\n";
    }

    cout << "done\n";
    return 0;
}

#ifdef _WIN64
string SelectFolder()
{
    char path[MAX_PATH];

    BROWSEINFO bi = { 0 };

    ITEMIDLIST* pidl = nullptr;
//    SHGetFolderLocation(NULL, CSIDL_PROGRAM_FILES, NULL, 0, &pidl);

    bi.lpszTitle = ("Select Installation folder");
//    bi.pidlRoot = pidl;
    bi.ulFlags = /*BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE*/0;
    bi.lpfn = nullptr;
    bi.lParam = (LPARAM)"c:\\temp\\";

    LPITEMIDLIST pidl2 = SHBrowseForFolderA(&bi);

    if (pidl2 != 0)
    {
        //get the name of the folder and put it in path
        SHGetPathFromIDList(pidl2, path);

        //free memory used
        IMalloc* imalloc = 0;
        if (SUCCEEDED(SHGetMalloc(&imalloc)))
        {
            imalloc->Free(pidl2);
            imalloc->Release();
        }

        return string(path);
    }

    return "";
}
#endif


string ParseValue(const string& sKey, const string& sDoc)
{
    size_t nStartPos = sDoc.find(sKey);
    if (nStartPos != string::npos)
    {
        nStartPos += sKey.length() + 1;

        size_t nEndPos = sDoc.find('\r', nStartPos);
        if (nEndPos != string::npos)
        {
            return sDoc.substr(nStartPos, nEndPos - nStartPos);
        }
    }

    return false;
}

bool GetAppRegistryValue(const string& sAppName, const string& sKey, string& sValue)
{
    string sAppRegistryNode = "SOFTWARE\\" + sAppName;

    HKEY hKey;
    LRESULT result = RegOpenKeyEx(HKEY_CURRENT_USER, sAppRegistryNode.c_str(), 0, KEY_READ, &hKey);

    if (result == ERROR_SUCCESS)
    {

        DWORD nDataSize = 1024;
        char buffer[1024];
        DWORD dataType;

        result = RegQueryValueEx(hKey, sKey.c_str(), nullptr, &dataType, (LPBYTE)buffer, &nDataSize);
        RegCloseKey(hKey);

        if (result == ERROR_SUCCESS)
        {
            sValue.assign(buffer);
            return true;
        }
    }

    return false;
}

bool SetAppRegistryValue(const string& sAppName, const string& sKey, const string& sValue)
{
    string sAppRegistryNode = "SOFTWARE\\" + sAppName;

    HKEY hKey;
    LRESULT result = RegCreateKeyEx(HKEY_CURRENT_USER, sAppRegistryNode.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (result == ERROR_SUCCESS)
    {
        result = RegSetValueEx(hKey, sKey.c_str(), 0, REG_SZ, (uint8_t*)sValue.c_str(), (DWORD)sValue.length());
        RegCloseKey(hKey);
    }

    return result == ERROR_SUCCESS;
}

bool IsProcessElevated()
{
    BOOL fIsElevated = FALSE;
    HANDLE hToken = NULL;
    TOKEN_ELEVATION elevation;
    DWORD dwSize;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        printf("\n Failed to get Process Token :%d.", GetLastError());
        goto Cleanup;  // if Failed, we treat as False
    }


    if (!GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize))
    {
        printf("\nFailed to get Token Information :%d.", GetLastError());
        goto Cleanup;// if Failed, we treat as False
    }

    fIsElevated = elevation.TokenIsElevated;

Cleanup:
    if (hToken)
    {
        CloseHandle(hToken);
        hToken = NULL;
    }
    return fIsElevated;
}

bool RequiresElevationToWrite(const string& folder) 
{
    filesystem::path testFile(folder);
    testFile.append("testwritetemp");

    HANDLE hFile = CreateFile(testFile.string().c_str(), GENERIC_WRITE | GENERIC_READ, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return true;

    CloseHandle(hFile);
    DeleteFile(testFile.string().c_str());

    return false;
}

bool ElevateIfNeeded(const string& sOutputFolder, bool bForce)
{
    if (RequiresElevationToWrite(sOutputFolder) && !IsProcessElevated())
    {
        string sParameters("\"" + sOutputFolder + "\"");
        if (bForce)
            sParameters += " -force";

        string sMessage("Folder \"" + sOutputFolder + "\" requires elevated permissions. Restarting installer to request permission.");
        MessageBox(nullptr, sMessage.c_str(), "Elevated permissions required", MB_ICONINFORMATION|MB_OK);
        ShellExecuteA(NULL, "runas", exeFilename.string().c_str(), sParameters.c_str(), NULL, SW_SHOWDEFAULT);
        return true;
    }

    return false;
}

int RunInstall(string sOutputFolder, bool bForce = false)
{
    string sInstallDoc;
    if (ExtractInstallFile("/install/install.cfg", sInstallDoc))
    {
        if (LOG::gnVerbosityLevel > LVL_DEFAULT)
            cout << "Using install.cfg from archive\n";
        if (LOG::gnVerbosityLevel > LVL_DIAG_BASIC)
            cout << sInstallDoc << "\n";
    }

    string sAppName = ParseValue("appname", sInstallDoc);
    string sPackagedVersion = ParseValue("appver", sInstallDoc);
    string sPostInstallTemplate = ParseValue("post_install_template", sInstallDoc);


    string sInstalledVersion;
    string sInstalledPath;
    if (GetAppRegistryValue(sAppName, "Version", sInstalledVersion) && GetAppRegistryValue(sAppName, "Path", sInstalledPath))
    {
        filesystem::path installedEXEPath(sInstalledPath);
        installedEXEPath.append(sAppName + ".exe");

        if (filesystem::exists(installedEXEPath))
        {
            cout << "Installed version:" << sInstalledVersion << "\n";
            cout << "Packaged version: " << sPackagedVersion << "\n";

            if (sInstalledVersion == sPackagedVersion && !bForce)
            {

                string sInstalledMessage = "Version " + sInstalledVersion + " already installed to\nPath \"" + sInstalledPath + "\"\n\nRun with \"-force\" command line option to force reinstallation.";
                MessageBox(0, sInstalledMessage.c_str(), "Already Installed", MB_ICONINFORMATION | MB_OK);
                cout << "Version already installed. Run with -force to force reinstallation.\n";

                ShellExecuteA(NULL, "open", sInstalledPath.c_str(), nullptr, NULL, SW_SHOWDEFAULT);

                return 0;
            }
        }
        else
        {
            sInstalledPath.clear();
            sInstalledVersion.clear();
        }
    }



    // If no output folder specified but the app is installed already, use the installed path
    if (sOutputFolder.empty() && !sInstalledPath.empty())
        sOutputFolder = sInstalledPath;
    else if (sOutputFolder.empty())         // if installed path is empty and no output folder specified, show selection dialog
        sOutputFolder = SelectFolder();

    // If use canceled, exit
    if (sOutputFolder.empty())
        return -1;




    // if we're re-running elevated, just exit
    if (ElevateIfNeeded(sOutputFolder, bForce))
        return 0;


    string sRegTemplate;
    if (ExtractInstallFile(sPostInstallTemplate, sRegTemplate))
    {
        if (LOG::gnVerbosityLevel > LVL_DEFAULT)
            cout << "Found registry template\n";
        if (LOG::gnVerbosityLevel > LVL_DIAG_BASIC)
            cout << sRegTemplate << "\n";
    }

    sRegTemplate = SH::replaceTokens(sRegTemplate, "$$INSTALL_LOC$$", sOutputFolder);
    sRegTemplate = SH::replaceTokens(sRegTemplate, "$$VERSION$$", sPackagedVersion);

    Extract(sOutputFolder);

    ofstream outRegFile(sOutputFolder + sPostInstallTemplate + ".reg");
    outRegFile.write(sRegTemplate.c_str(), sRegTemplate.length());

    SetAppRegistryValue(sAppName, "Version", sPackagedVersion);
    SetAppRegistryValue(sAppName, "Path", sOutputFolder);

    string sInstalledMessage = "Version " + sPackagedVersion + " installed to\nPath \"" + sOutputFolder;
    MessageBox(0, sInstalledMessage.c_str(), "Install complete",  MB_OK);

    ShellExecuteA(NULL, "open", sOutputFolder.c_str(), nullptr, NULL, SW_SHOWDEFAULT);

    return 0;
}


int main(int argc, char* argv[])
{
//	_CrtMemState s1;
//	_CrtMemCheckpoint(&s1);

    exeFilename = filesystem::path(argv[0]);

    // App Globals for reading command line
    string  sOutputFilename;
    string  sBaseFolder;            // base folder. (default is the folder of ZZip.exe)
    string  sOutputFolder;
    bool bForce = false;

    CommandLineParser parser;

    if (ContainsEmbeddedArchive(exeFilename))
    {
        parser.RegisterAppDescription("Self extracting installer");
        parser.RegisterParam(ParamDesc("OUTPUTFOLDER", &sOutputFolder, CLP::kPositional | CLP::kRequired, "Folder into which to extract all files."));
        parser.RegisterParam(ParamDesc("force", &bForce, CLP::kNamed | CLP::kOptional, "Forces overwrite even if installed version matches package."));

        parser.Parse(argc, argv);

        RunInstall(sOutputFolder, bForce);
    }
    else
    {
        parser.RegisterAppDescription("Creates a self-installer from an application folder.");

        parser.RegisterParam(ParamDesc("OUTPUTFILENAME", &sOutputFilename, CLP::kPositional | CLP::kRequired, "Path of the ZIP archive to create."));
        parser.RegisterParam(ParamDesc("FOLDER", &sBaseFolder, CLP::kPositional | CLP::kRequired, "Base folder of files add to the archive"));

        if (!parser.Parse(argc, argv))
            return -1;

        filesystem::path outputFilename(sOutputFilename);

        Create(sBaseFolder, outputFilename);
    }

	return 0;

}

