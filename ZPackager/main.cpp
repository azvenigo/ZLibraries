// MIT License
// Copyright 2024 Alex Zvenigorodsky
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
#include "helpers/FileHelpers.h"
#include "helpers/CommandLineParser.h"
#include "helpers/ThreadPool.h"
#include "helpers/Registry.h"
#include <assert.h>

#ifdef _WIN64
#include <Windows.h>
#include <shlobj.h>
#include <aclapi.h>
#endif


using namespace std;
using namespace CLP;


bool gDebug = false;


filesystem::path exeFilename;


void ShowError(const string& sMessage)
{
    cerr << "ERROR: " << sMessage << "\n";
    MessageBox(nullptr, sMessage.c_str(), "ERROR", MB_OK | MB_ICONERROR);
}


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
        ShowError("Failed to open myself for cloning");
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
        ShowError("Failed to create output file");
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
        ShowError("ERROR: Couldn't open self archive:\"" + exeFilename.string() + "\" for Decompression Job!");
        return false;
    }

    cZipCD& zipCD = zipAPI.GetZipCD();

    if (LOG::gnVerbosityLevel >= LVL_DIAG_BASIC)
        zipCD.DumpCD(cout, "*", true, SH::eToStringFormat::kTabs);


    cCDFileHeader fileHeader;
    if (!zipCD.GetFileHeader(sArchiveFilename, fileHeader))
    {
        ShowError("Couldn't extract file:" + sArchiveFilename + "\n");
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
        ShowError("ERROR: Couldn't open self archive:\"" + exeFilename.string() + "\" for Decompression Job!");
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
bool ShowSelectFolderDialog(const std::string& sTitle, std::string& sFilenameResult, std::string sDefaultFolder)
{
    bool bSuccess = false;

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr))
    {
        IFileOpenDialog* pFileOpen;

        // Create the FileOpenDialog object.
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

        if (SUCCEEDED(hr))
        {
            COMDLG_FILTERSPEC rgSpec[] =
            {
                      { L"All Files", L"*.*" }
            };

            pFileOpen->SetFileTypes(1, rgSpec);
            pFileOpen->SetTitle(SH::string2wstring(sTitle).c_str());

            DWORD dwOptions;
            hr = pFileOpen->GetOptions(&dwOptions);
            if (SUCCEEDED(hr))
            {
                hr = pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS);
            }

            if (!sDefaultFolder.empty())
            {
                wstring sDefaultFolderW(SH::string2wstring(sDefaultFolder));
                IShellItem* pShellItem = nullptr;
                SHCreateItemFromParsingName(sDefaultFolderW.c_str(), nullptr, IID_IShellItem, (void**)(&pShellItem));

                pFileOpen->SetFolder(pShellItem);
                pShellItem->Release();
            }

            // Show the Open dialog box.
            hr = pFileOpen->Show(NULL);

            // Get the file name from the dialog box.
            if (SUCCEEDED(hr))
            {
                IShellItem* pItem;
                hr = pFileOpen->GetResult(&pItem);
                if (SUCCEEDED(hr))
                {
                    PWSTR pszFilePath;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

                    if (SUCCEEDED(hr) && pszFilePath)
                    {
                        wstring wideFilename(pszFilePath);
                        sFilenameResult = SH::wstring2string(wideFilename);
                    }

                    pItem->Release();
                    bSuccess = true;
                }
            }
            pFileOpen->Release();
        }
        CoUninitialize();
    }

    return bSuccess;
}

#endif


string ParseValue(const string& sKey, const string& sDoc, bool bRequired = false)
{
    size_t nStartPos = sDoc.find(sKey);
    if (nStartPos != string::npos)
    {
        nStartPos += sKey.length() + 1;

        size_t nEndPos = sDoc.find('\r', nStartPos);
        string sReturn;
        if (nEndPos == string::npos)
        {
            sReturn = sDoc.substr(nStartPos);
        }
        else
        {
            sReturn = sDoc.substr(nStartPos, nEndPos - nStartPos);
        }

        return sReturn;
    }

    if (bRequired)
        ShowError("Failed to extract needed value from installation configuration. key:" + sKey);

    return "";
}

bool SetRegistryString(HKEY key, const string& sPath, const string& sKey, const string& sValue)
{
    int result = REG::SetWindowsRegistryString(key, sPath, sKey, sValue);

    string section;
    if (key == HKEY_CURRENT_USER)
        section = "HKEY_CURRENT_USER";
    else if (key == HKEY_LOCAL_MACHINE)
        section = "HKEY_LOCAL_MACHINE";
    else if (key == HKEY_CLASSES_ROOT)
        section = "HKEY_CLASSES_ROOT";
    else
        section = "unknown section:" + SH::FromInt((int64_t)key);


    if (result != 0)
    {
        ShowError("Failed to set registry path:" + section + "\\" + sPath + " key:" + sKey + " value:" + sValue + " error code:" + SH::FromInt(result));
        return result;
    }

    if (gDebug)
    {
        MessageBox(0, string("Set registry path:" + section + "\\" + sPath + " key:" + sKey + " value:" + sValue + " return code:" + SH::FromInt(result)).c_str(), "DEBUG", MB_OK | MB_ICONINFORMATION);
    }

    return true;
}

bool SetFileAssociation(const string& sExecutablePath, const string& sProgIDSubkey, const string& sExtension)
{
    // Specify the file extension and executable path
    //LPCSTR fileExtension = sExtension.c_str();
    //LPCSTR executablePath = L"your_executable_path.exe";

    // Register the application to handle the file type under HKEY_CURRENT_USER
    HKEY hKeyExt;
    string sClass("Software\\Classes\\." + sExtension);
    if (RegCreateKeyExA(HKEY_CURRENT_USER, sClass.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKeyExt, NULL) == ERROR_SUCCESS) 
    {
        // Set the command to execute when opening the file under HKEY_CURRENT_USER
        string sOpenCommand("\"" + sExecutablePath + "\"" + " \"%1\"");
        if (RegCreateKeyExA(hKeyExt, "shell\\open\\command", 0, NULL, 0, KEY_WRITE, NULL, &hKeyExt, NULL) == ERROR_SUCCESS) 
        {
            RegSetValueExA(hKeyExt, NULL, 0, REG_SZ, (BYTE*)sOpenCommand.data(), (DWORD)sOpenCommand.length());
            RegCloseKey(hKeyExt);

            return true;
        }
    }

    assert(false);
    return false;
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

bool ElevatedLaunch(const string& sOutputFolder, bool bForceInstall)
{
    string sParameters("\"" + sOutputFolder + "\"");
    if (bForceInstall)
        sParameters += " -force";

    if (gDebug)
        sParameters += " -debug";


    ShellExecuteA(NULL, "runas", exeFilename.string().c_str(), sParameters.c_str(), NULL, SW_SHOWDEFAULT);
    return true;
}

bool DoesFolderRequireElevation(const string& folder) 
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

bool ElevateIfNeeded(const string& sOutputFolder, bool bForceInstall)
{
    if (DoesFolderRequireElevation(sOutputFolder) && !IsProcessElevated())
    {
        string sMessage("Folder \"" + sOutputFolder + "\" requires elevated permissions. Restarting installer to request permission.");
        if (MessageBox(nullptr, sMessage.c_str(), "Elevated permissions required", MB_ICONINFORMATION | MB_OK) == MB_OK)
            return ElevatedLaunch(sOutputFolder, bForceInstall);
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


//    sInstallDoc = "appname=ZImageViewer\nappver=1.0.0.81\nprogId=ZImageViewer.exe\nprogDesc=Fast image viewer and workflow for photographers\npost_install_template=/install/associate.reg.template\nbuildurl=https://www.azvenigo.com/zimageviewerbuild/index.html\nfile_type_assoc_list=png\n";






    string progName = ParseValue("appname", sInstallDoc, true);
    string sPackagedVersion = ParseValue("appver", sInstallDoc, true);
    string progID = ParseValue("progId", sInstallDoc, true);
    string sProgDesc = ParseValue("progDesc", sInstallDoc);

    string sPackedAssociationList = ParseValue("file_type_assoc_list", sInstallDoc);

    // convert the packed list separated by semicolons into an array
    std::vector<std::string> associationList;
    SH::ToVector(sPackedAssociationList, associationList, ';');

    // 1) Is registered?
    //      yes continue to 2
    //      no
    //       \_>    elevated?
    //                 \_     re-run elevated
    //
    //       yes (elevated but not registered
    //              \_>    register and continue to 2

    //



    string sInstallerCaption(progName + " (v" + sPackagedVersion + ") Installer");



    string sEXEName(progName + ".exe"); // no path, ex: "MYAPP.exe"

    // my (seemingly working)
    // for each extension
    //  HKLM\SOFTWARE\Classes\<appName>.<extension>\shell\Open\command\"app.exe" "%1"
    // 
    //  and maybe need for each extension:
    //      HKCR\<extension>\default = <appName>.<extension>


    // 1) HKLM/SOFTWARE/Microsoft/Windows/CurrentVersion/App Paths/<progname>: "" = <appPath>

    string sKey;
    string sLaunchEXEPath;

    string sInstalledVersion("unknown");


    // If no folder provided on command line, look in registry for existing install
    if (sOutputFolder.empty())
    {
        // Previously installed?
        sKey = /*HKEY_LOCAL_MACHINE\\*/ "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + sEXEName;
        if (REG::GetWindowsRegistryString(HKEY_LOCAL_MACHINE, sKey, "", sLaunchEXEPath) == 0 && std::filesystem::exists(sLaunchEXEPath))
        {
            // already installed to a location.... ask whether to install to same

            sInstalledVersion = GetFileVersion(sLaunchEXEPath);

            char buf[2048];

            if (sInstalledVersion == sPackagedVersion)
                sprintf_s(buf, "%s (ver %s)\nis already installed in location:\n%s.\n\nReinstall to same location?", progName.c_str(), sInstalledVersion.c_str(), sLaunchEXEPath.c_str());
            else
                sprintf_s(buf, "%s (ver %s)\nis installed in location:\n%s.\n\nInstall ver %s to same location?", progName.c_str(), sInstalledVersion.c_str(), sLaunchEXEPath.c_str(), sPackagedVersion.c_str());

            // if bForce then don't ask
            if (bForce || MessageBox(0, buf, sInstallerCaption.c_str(), MB_ICONQUESTION | MB_YESNO) == IDYES)
                sOutputFolder = std::filesystem::path(sLaunchEXEPath).parent_path().string();       // strip off the exe path
            else
                sOutputFolder.clear();
        }
    }

    // Prompt for location if still don't know where
    if (sOutputFolder.empty())
    {
        string sTitle = "Select folder to install " + progName + " ver:" + sPackagedVersion;

        if (ShowSelectFolderDialog(sTitle, sOutputFolder, getenv("ProgramFiles")))
        {
            filesystem::path outputFolder(sOutputFolder);
            outputFolder.append(progName);

            char buf[2048];
            sprintf_s(buf, "OK to install %s version %s to:\n\"%s\" ?", progName.c_str(), sPackagedVersion.c_str(), outputFolder.string().c_str());

            if (MessageBox(0, buf, sInstallerCaption.c_str(), MB_ICONQUESTION | MB_YESNO) != IDYES)
            {
                return -1;
            }

            sOutputFolder = outputFolder.string();  // letting filesystem do any folder shenanigans and returning back to string
        }
        else
            return 0;
    }


    if (sOutputFolder.empty())
    {
        ShowError("Something went wrong.... no output folder specified.");
        return -1;
    }


    // either application hasn't been registered or is no longer at the location being pointed at
    if (!IsProcessElevated())
    {
        ElevatedLaunch(sOutputFolder, bForce);      // restart elevated to perform registration
        return 0;
    }




/*    string sMessage = "pretend install to:" + sOutputFolder;
    MessageBox(0, sMessage.c_str(), "", MB_OK);
    return 0;*/


    filesystem::path appPath(sOutputFolder);    
    appPath.append(progName + ".exe");  // full path, ex: "c:/stuff/MYAPP.exe"
    string sOpenValue("\"" + appPath.string() + "\" \"%1\"");


    // Register 1)
    sKey = /*HKEY_LOCAL_MACHINE\\*/ "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + sEXEName;
    if (!SetRegistryString(HKEY_LOCAL_MACHINE, sKey, "", appPath.string()))
        return -1;

    // 2) HKCR\Applications\progName.exe\shell\Open\command\"app.exe" "%1"
    sKey = /*HKEY_CLASSES_ROOT*/ "Applications\\" + progName + ".exe\\shell\\Open\\command";
    if (!SetRegistryString(HKEY_CLASSES_ROOT, sKey, "", sOpenValue))
        return -1;

    for (auto fileExt : associationList)
    {
        cout << "Setting association:" << fileExt << "\n";

        if (fileExt[0] != '.')
            fileExt = "." + fileExt;

        // 3) HKLM\SOFTWARE\Classes\<appName>.<extension>\shell\Open\command\"app.exe" "%1"
        sKey = /*HKEY_LOCAL_MACHINE*/ "SOFTWARE\\Classes\\" + progName + fileExt + "\\shell\\Open\\command";
        if (!SetRegistryString(HKEY_LOCAL_MACHINE, sKey, "", sOpenValue))
            return -1;

        // 4) HKCR\Applications\progName.exe\SupportedTypes\<ext>
        sKey = /*HKEY_CLASSES_ROOT*/ "Applications\\" + progName + ".exe\\SupportedTypes";
        if (!SetRegistryString(HKEY_CLASSES_ROOT, sKey, fileExt, ""))
            return -1;
    }


    if (Extract(sOutputFolder) != 0)
    {
        ShowError("Failed to install to" + sOutputFolder);
        return -1;
    }


    if (!bForce)
    {
        string sInstalledMessage = "Version " + sPackagedVersion + " installed to\nPath \"" + sOutputFolder + "\n\nLaunching...";
        MessageBox(0, sInstalledMessage.c_str(), sInstallerCaption.c_str(), MB_OK | MB_ICONINFORMATION);
    }

    ShellExecuteA(NULL, "open", appPath.string().c_str(), nullptr, NULL, SW_SHOWDEFAULT);
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
        parser.RegisterParam(ParamDesc("OUTPUTFOLDER", &sOutputFolder, CLP::kPositional | CLP::kOptional, "Folder into which to extract all files."));
        parser.RegisterParam(ParamDesc("force", &bForce, CLP::kNamed | CLP::kOptional, "Forces overwrite even if installed version matches package."));

        parser.RegisterParam(ParamDesc("debug", &gDebug, CLP::kNamed | CLP::kOptional, "debug installer output"));
        
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

// for windows linking
int WinMain(HINSTANCE hInst, HINSTANCE hPrev, PSTR cmdline, int cmdshow)
{
    return main(__argc, __argv);
}


