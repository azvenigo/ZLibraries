// MIT License
// Copyright 2019 Alex Zvenigorodsky
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "ZipJob.h"
#include "ZZipAPI.h"
#include <iostream>
#include <iomanip>
#include <filesystem>
#include "helpers/Crc32Fast.h"
#include "helpers/FNMatch.h"
#include "helpers/ThreadPool.h"
#include "helpers/ZZFileAPI.h"
#include "helpers/LoggingHelpers.h"

using namespace std;

inline int64_t GetUSSinceEpoch()
{
    return std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1);
}


ZipJob::~ZipJob()
{
    // wait for all workers to complete
    for (tThreadList::iterator it = mWorkers.begin(); it != mWorkers.end(); it++)
    {
        std::thread* pWorker = *it;
        pWorker->join();
        delete  pWorker;
    }
}

void ZipJob::SetBaseFolder(const string& sBaseFolder)
{
    if (!sBaseFolder.empty())
    {
        msBaseFolder = sBaseFolder;
        std::replace(msBaseFolder.begin(), msBaseFolder.end(), '\\', '/');    // Only forward slashes

        if (msBaseFolder[msBaseFolder.length() - 1] != '/')
            msBaseFolder.append("/");
    }
}


bool ZipJob::Run()
{
    mJobStatus.mStatus = JobStatus::kRunning;
    std::thread* pThread;
    switch (mJobType)
    {
    case kExtract:
        pThread = new std::thread(ZipJob::RunDecompressionJob, (void*)this);
        break;
    case kCompress:
        pThread = new std::thread(ZipJob::RunCompressionJob, (void*)this);
        break;
    case kDiff:
        pThread = new std::thread(ZipJob::RunDiffJob, (void*)this);
        break;
    case kList:
        pThread = new std::thread(ZipJob::RunListJob, (void*)this);
        break;
        default:
        return false;
    }
    mWorkers.push_back(pThread);


    return true;
}

void ZipJob::RunCompressionJob(void* pContext)
{
    ZipJob* pZipJob = (ZipJob*)pContext;
    if (!pZipJob)
    {
        cerr << "No job context passed in to thread!\n";
        return;
    }

    zout << "Running Compression Job.\n";
    zout << "Package: " << pZipJob->msPackageURL << "\n";
    zout << "Path:    " << pZipJob->msBaseFolder << "\n";
    zout << "Pattern: " << pZipJob->msPattern << "\n";


    if (!std::filesystem::exists(pZipJob->msBaseFolder))
    {
        pZipJob->mJobStatus.SetError(JobStatus::kError_NotFound, "Folder \"" + pZipJob->msBaseFolder + "\" not found!");
        std::cerr << "Folder \"" << pZipJob->msBaseFolder << "\" not found!\n";
        return;
    }

    ZZipAPI zipAPI;
    if (!zipAPI.Init(pZipJob->msPackageURL, ZZipAPI::kZipCreate))
    {
        pZipJob->mJobStatus.SetError(JobStatus::kError_OpenFailed, "Couldn't create package:\"" + pZipJob->msPackageURL + "\" for compression Job!");
        //cerr << "Couldn't Open " << pZipJob->msPackageURL << " for Decompression Job!" << std::endl;
        return;
    }


    uint64_t nTotalFilesSkipped = 0;

    tCDFileHeaderList filesToCompress;

    // Compute files to add (so that we have the total size of the job)
    uint64_t nTotalBytes = 0;
    std::filesystem::path compressFolder(pZipJob->msBaseFolder);

    for (auto it : std::filesystem::recursive_directory_iterator(compressFolder))
    {
        if (pZipJob->mbVerbose)
            zout << "Found:" << it;

        if (FNMatch(pZipJob->msPattern, it.path().generic_string().c_str()))
        {
            if (pZipJob->mbVerbose)
                zout << "...matches.\n";

            cCDFileHeader fileHeader;
            fileHeader.mFileName = it.path().generic_string();
            if (is_regular_file(it.path()))
            {
                fileHeader.mUncompressedSize = file_size(it.path());
                nTotalBytes += fileHeader.mUncompressedSize;
            }

            filesToCompress.push_back(fileHeader);
        }
        else
        {
            nTotalFilesSkipped++;
            if (pZipJob->mbVerbose)
                zout << "...no match. Skipping.\n";
        }
    }

    if (filesToCompress.size() == 0)
    {
        zout << "Found no matching files to compress. Aborting.\n";
        pZipJob->mJobStatus.mStatus = JobStatus::kFinished;
        return;
    }

    zout << "Found " << filesToCompress.size() << " files.  Total size: " << FormatFriendlyBytes(nTotalBytes, SH::kMiB) << " (" << nTotalBytes << " bytes)\n";
    pZipJob->mJobProgress.Reset();
    pZipJob->mJobProgress.AddBytesToProcess(nTotalBytes);
    
    // Add files one at a time
    for (auto fileHeader : filesToCompress)
    {
        if (pZipJob->mbVerbose)
            zout << "Adding to Zip File: " << fileHeader.mFileName << "\n";
        zipAPI.AddToZipFile(fileHeader.mFileName, pZipJob->msBaseFolder, &pZipJob->mJobProgress);
    }

    zout << "Finished\n";


    cZipCD& zipCD = zipAPI.GetZipCD();

    zout << "[==============================================================]\n";
    zout << "Total Files Skipped:               " << nTotalFilesSkipped << "\n";
    zout << "Total Files Added:                 " << zipCD.GetNumTotalFiles() << "\n";
    zout << "Total Folders Added:               " << zipCD.GetNumTotalFolders() << "\n";


    uint64_t nTotalUncompressed = zipCD.GetTotalUncompressedBytes();
    uint64_t nTotalCompressed = zipCD.GetTotalCompressedBytes();

    zout << "Total Bytes Read from Disk:        " << nTotalUncompressed << "\n";
    zout << "Total Compressed Bytes:            " << zipCD.GetTotalCompressedBytes() << "\n";


    if (zipCD.GetTotalUncompressedBytes() > 0)
    {
        float fCompressionRatio = ((float) (nTotalCompressed/1024) / (float) (nTotalUncompressed/1024));

//        std::ios cout_state(nullptr);   // store precision
//        cout_state.copyfmt(zout);
        zout << "Compression Ratio:                 " << setprecision(2) << fCompressionRatio << "\n";

//        zout.copyfmt(cout_state);  // restore precision
    }

    zout << "[--------------------------------------------------------------]\n";

    zout << "Total Time Taken:                  " << pZipJob->mJobProgress.GetElapsedTimeMS()/1000 << "s\n";
    zout << "Compression Speed:                 " << FormatFriendlyBytes(pZipJob->mJobProgress.GetBytesPerSecond(), SH::kMiB) << "/s \n";

    zout << "[==============================================================]\n";



    pZipJob->mJobStatus.mStatus = JobStatus::kFinished;
}



void ZipJob::RunDiffJob(void* pContext)
{
    ZipJob* pZipJob = (ZipJob*) pContext;

    eToStringFormat stringFormat = pZipJob->mOutputFormat;

    shared_ptr<cZZFile> pZZFile;
    if (!cZZFile::Open(pZipJob->msPackageURL, cZZFile::ZZFILE_READ, pZZFile, pZipJob->mbVerbose))
    {
        pZipJob->mJobStatus.SetError(JobStatus::kError_OpenFailed, "Failed to open package: \"" + pZipJob->msPackageURL + "\"");
        return;
    }

    cZipCD zipCD;
    if (!zipCD.Init(*pZZFile))
    {
        pZipJob->mJobStatus.SetError(JobStatus::kError_ReadFailed, "Failed to read Zip Central Directory from package: \"" + pZipJob->msPackageURL + "\"");
        return;
    }

    string sExtractPath = pZipJob->msBaseFolder + "/";

    ThreadPool pool(pZipJob->mnThreads);
    vector<shared_future<DiffTaskResult> > diffResults;

    // Step 1) See what files in the zip archive do not exist locally
    for (auto cdHeader : zipCD.mCDFileHeaderList)
    {
        diffResults.emplace_back(pool.enqueue([=]
        {
            if (cdHeader.mFileName.length() == 0)
                return DiffTaskResult(DiffTaskResult::kError, 0, "empty filename.");

            std::filesystem::path fullPath(pZipJob->msBaseFolder);
            fullPath.append(cdHeader.mFileName);

            // If the entry is a folder (ending in '/') see if that folder already exists
            if (cdHeader.mFileName[cdHeader.mFileName.length() - 1] == '/')
            {
                // Directory Diff Check
                if (std::filesystem::exists(fullPath) && std::filesystem::is_directory(fullPath))
                {
                    return DiffTaskResult(DiffTaskResult::kDirMatch, 0, cdHeader.mFileName);
                }
                else
                {
                    return DiffTaskResult(DiffTaskResult::kDirPackageOnly, 0, cdHeader.mFileName);
                }
            }
            else
            {
                // File Diff Check
                if (!std::filesystem::exists(fullPath))
                {
                    return DiffTaskResult(DiffTaskResult::kFilePackageOnly, cdHeader.mUncompressedSize, cdHeader.mFileName);
                }

                if (pZipJob->FileNeedsUpdate(fullPath.string(), cdHeader.mUncompressedSize, cdHeader.mCRC32))
                {
                    return DiffTaskResult(DiffTaskResult::kFileDifferent, cdHeader.mUncompressedSize, cdHeader.mFileName);
                }
            }

            return DiffTaskResult(DiffTaskResult::kFileMatch, cdHeader.mUncompressedSize, cdHeader.mFileName);
        }));
    }

    // Step 2) See what target files exist (local) that are not in the source (zip)
    tCDFileHeaderList localFiles;
    filesystem::path fullPath(pZipJob->msBaseFolder);

    for (auto it : filesystem::recursive_directory_iterator(fullPath))
    {
        string sRelativePath = it.path().generic_string().substr(pZipJob->msBaseFolder.length());  // get the relative path from the iterator's found path
        bool bIsDirectory = is_directory(it.path());

        if (bIsDirectory && sRelativePath[sRelativePath.length() - 1] != '/')    // if this is a directory ensure it ends in an ending '/'
            sRelativePath.append("/");


        cCDFileHeader fileHeader;
        if (!zipCD.GetFileHeader(sRelativePath, fileHeader))    // no entry found?
        {
            if (bIsDirectory)
                diffResults.emplace_back(pool.enqueue([=] { return DiffTaskResult(DiffTaskResult::kDirPathOnly, 0, sRelativePath); }));
            else
                diffResults.emplace_back(pool.enqueue([=] { return DiffTaskResult(DiffTaskResult::kFilePathOnly, std::filesystem::file_size(it.path()), sRelativePath); }));
        }
    }

    uint64_t nMatchDirs = 0;
    uint64_t nMatchFiles = 0;
    uint64_t nMatchFileBytes = 0;
    uint64_t nDifferentFiles = 0;
    uint64_t nDifferentFilesSourceBytes = 0;
    uint64_t nSourceOnlyDirs = 0;
    uint64_t nSourceOnlyFiles = 0;
    uint64_t nTargetOnlyDirs = 0;
    uint64_t nTargetOnlyFiles = 0;

    uint64_t nTargetOnlyFileBytes = 0;
    uint64_t nSourceOnlyFileBytes = 0;

    for (auto &resultIt : diffResults)
    {
        DiffTaskResult diffResult = resultIt.get();
        switch (diffResult.mDiffTaskStatus)
        {
        case DiffTaskResult::kDirPackageOnly: nSourceOnlyDirs++; break;
        case DiffTaskResult::kDirPathOnly: nTargetOnlyDirs++; break;
        case DiffTaskResult::kFilePackageOnly: nSourceOnlyFiles++; nSourceOnlyFileBytes += diffResult.mnSize; break;
        case DiffTaskResult::kFilePathOnly: nTargetOnlyFiles++; nTargetOnlyFileBytes += diffResult.mnSize; break;
        case DiffTaskResult::kDirMatch: nMatchDirs++; break;
        case DiffTaskResult::kFileMatch: nMatchFiles++; nMatchFileBytes += diffResult.mnSize; break;
        case DiffTaskResult::kFileDifferent: nDifferentFiles++; nDifferentFilesSourceBytes += diffResult.mnSize; break;
        default: break;
        }
    }


    bool bAllMatch = (nDifferentFiles == 0 && nSourceOnlyFiles == 0 && nTargetOnlyFiles == 0);

    // Write formatted header
    zout << StartPageHeader(stringFormat);
    zout << StartSection(stringFormat);
    zout << FormatStrings(stringFormat, "ZiP Package", "Path");
    zout << FormatStrings(stringFormat, pZipJob->msPackageURL, pZipJob->msBaseFolder);
    zout << EndSection(stringFormat);

    // If any files are different then show a breakdown of differences
    if (!bAllMatch)
    {
        zout << StartSection(stringFormat);
        zout << FormatStrings(stringFormat, "Package Only (Missing from destination folder)", "Path Only (Not in package)");

        // Source Only
        for (auto &resultIt : diffResults)
        {
            DiffTaskResult diffResult = resultIt.get();
            switch (diffResult.mDiffTaskStatus)
            {
            case DiffTaskResult::kDirPackageOnly:
            case DiffTaskResult::kFilePackageOnly:
                zout << FormatStrings(stringFormat, diffResult.mFilename, ""); // second column is blank
                break;
            default: break;
            }
        }

        // Destination only
        for (auto &resultIt : diffResults)
        {
            DiffTaskResult diffResult = resultIt.get();
            switch (diffResult.mDiffTaskStatus)
            {
            case DiffTaskResult::kDirPathOnly:
            case DiffTaskResult::kFilePathOnly:
                zout << FormatStrings(stringFormat, "", diffResult.mFilename, ""); // first column is blank
                break;
            default: break;
            }
        }

        zout << EndSection(stringFormat);
    }

    zout << StartSection(stringFormat);
    zout << StartDelimiter(stringFormat, 5) << "Diff Summary" << EndDelimiter(stringFormat);

    zout << FormatStrings(stringFormat, "Matching Dirs: ", to_string(nMatchDirs));
    zout << FormatStrings(stringFormat, "Matching Files: ", to_string(nMatchFiles), " Size:", to_string(nMatchFileBytes));

    if (bAllMatch)
    {
        zout << FormatStrings(stringFormat, "** ALL MATCH **");
    }
    else
    {
        zout << FormatStrings(stringFormat, "Different Files: ", to_string(nDifferentFiles), " Size:", to_string(nDifferentFilesSourceBytes));

        zout << FormatStrings(stringFormat, "Package Only Dirs: ", to_string(nSourceOnlyDirs));
        zout << FormatStrings(stringFormat, "Path Only Dirs: ", to_string(nTargetOnlyDirs));

        zout << FormatStrings(stringFormat, "Package Only Files: ", to_string(nSourceOnlyFiles), " Size:", to_string(nSourceOnlyFileBytes));
        zout << FormatStrings(stringFormat, "Path Only Files: ", to_string(nTargetOnlyFiles), " Size:", to_string(nTargetOnlyFileBytes));
    }

    zout << EndSection(stringFormat);
    zout << EndPageFooter(stringFormat);

    pZipJob->mJobStatus.mStatus = JobStatus::kFinished;
}

bool ZipJob::FileNeedsUpdate(const string& sPath, uint64_t nComparedFileSize, uint32_t nComparedFileCRC)
{
    if (mbVerbose)
        zout << "Verifying file " << sPath;

    shared_ptr<cZZFile> pLocalFile;
    if (!cZZFile::Open(sPath, cZZFile::ZZFILE_READ, pLocalFile, mbVerbose))	// If no local file it clearly needs to be updated
    {
        if (mbVerbose)
            zout << "...missing. NEEDS UPDATE.\n";
        return true;
    }

    uint64_t nFileSize = pLocalFile->GetFileSize();
    if (nFileSize != nComparedFileSize)	// if the file size is different no need to do a CRC calc
    {
        if (mbVerbose)
            zout << "...size on disk:" << nFileSize << " package:" << nComparedFileSize << ". NEEDS UPDATE.\n";
        return true;
    }

    uint32_t nCRC = 0;
    uint32_t kCalcBufferSize = 128 * 1024;	// 128k buffer
    unique_ptr<uint8_t[]> pCalcBuffer(new uint8_t[kCalcBufferSize]);
    int64_t nBytesProcessed = 0;

    while (nBytesProcessed < (int64_t)nFileSize)
    {
        int64_t nBytesRead = 0;
        pLocalFile->Read(cZZFile::ZZFILE_NO_SEEK, kCalcBufferSize, pCalcBuffer.get(), nBytesRead);
        nCRC = crc32_16bytes(pCalcBuffer.get(), nBytesRead, nCRC);
        nBytesProcessed += nBytesRead;
    }

    if (nCRC != nComparedFileCRC)
    {
        if (mbVerbose)
            zout << "...CRC on disk:" << nCRC << " package:" << nComparedFileCRC << ". NEEDS UPDATE.\n";
        return true;
    }

    if (mbVerbose)
        zout << "...matches.\n";

    return false;
}



void ZipJob::RunDecompressionJob(void* pContext)
{
    ZipJob* pZipJob = (ZipJob*) pContext;


    if (pZipJob->mbVerbose)
    {
        zout << "Running Deompression Job.\n";
        zout << "Package: " << pZipJob->msPackageURL << "\n";
        zout << "Path:    " << pZipJob->msBaseFolder << "\n";
        zout << "Pattern: " << pZipJob->msPattern << "\n";
        zout << "Threads: " << pZipJob->mnThreads << "\n";
    }


    pZipJob->mJobStatus.mStatus = JobStatus::eJobStatus::kRunning;
    uint64_t startTime = GetUSSinceEpoch();

    ZZipAPI zipAPI;
    if (!zipAPI.Init(pZipJob->msPackageURL))
    {
        pZipJob->mJobStatus.SetError(JobStatus::kError_OpenFailed, "Couldn't Open package:\"" + pZipJob->msPackageURL + "\" for Decompression Job!");
        return;
    }

    string sExtractPath = pZipJob->msBaseFolder + "/";

    cZipCD& zipCD = zipAPI.GetZipCD();

    pZipJob->mJobProgress.Reset();

    tCDFileHeaderList filesToDecompress;
    uint64_t nTotalFilesSkipped = 0;
    uint64_t nTotalTimeOnFileVerification = 0;
    uint64_t nTotalBytesVerified = 0;

    string sPattern = pZipJob->msPattern;

    // Create folder structure and build list of files that match pattern
    zout << "Creating Folders.\n";
    for (tCDFileHeaderList::iterator it = zipCD.mCDFileHeaderList.begin(); it != zipCD.mCDFileHeaderList.end(); it++)
    {
        cCDFileHeader& cdFileHeader = *it;

        if (FNMatch(sPattern, cdFileHeader.mFileName))
        {
            if (pZipJob->mbVerbose)
                zout << "Pattern: \"" << sPattern.c_str() << "\" File: \"" << cdFileHeader.mFileName.c_str() << "\" matches. \n";

            std::filesystem::path fullPath(pZipJob->msBaseFolder);
            fullPath.append(cdFileHeader.mFileName);

            // If the path ends in '/' it's a folder and shouldn't be processed for decompression
            if (cdFileHeader.mFileName[cdFileHeader.mFileName.length() - 1] != '/')
                filesToDecompress.push_back(cdFileHeader);

            pZipJob->mJobProgress.AddBytesToProcess(cdFileHeader.mUncompressedSize);
        }
        else
        {
            if (pZipJob->mbVerbose)
                zout << "File Skipped: \"" << cdFileHeader.mFileName.c_str() << "\"\n";
            nTotalFilesSkipped++;
        }
    }

    ThreadPool pool(pZipJob->mnThreads);
    vector<shared_future<DecompressTaskResult> > decompResults;

    for (auto cdHeader : filesToDecompress)
    {
        decompResults.emplace_back(pool.enqueue([=, &zipAPI, &nTotalTimeOnFileVerification, &nTotalBytesVerified]
        {
            if (cdHeader.mFileName.length() == 0)
                return DecompressTaskResult(DecompressTaskResult::kAlreadyUpToDate, 0, 0, 0, 0, "", "empty filename.");

            std::filesystem::path fullPath(pZipJob->msBaseFolder);
            fullPath.append(cdHeader.mFileName);


            // decompress
            if (!filesystem::is_directory(fullPath.parent_path()))
            {
                if (pZipJob->mbVerbose)
                    zout << "Creating Path: \"" << fullPath.parent_path() << "\"\n";
                std::filesystem::create_directories(fullPath.parent_path());
            }

            // If the path ends in '/' it's a folder and shouldn't be processed for decompression
            if (cdHeader.mFileName[cdHeader.mFileName.length() - 1] != '/')
            {
                if (!pZipJob->mbSkipCRC)	// If doing CRC checking
                {
                    uint64_t verificationStartTime = GetUSSinceEpoch();

                    bool bNeedsUpdate = pZipJob->FileNeedsUpdate(fullPath.string(), cdHeader.mUncompressedSize, cdHeader.mCRC32);

                    uint64_t verificationEndTime = GetUSSinceEpoch();

                    uint64_t verificationDelta = verificationEndTime - verificationStartTime;

                    nTotalTimeOnFileVerification += verificationDelta;
                    nTotalBytesVerified += cdHeader.mUncompressedSize;   // in reality it's the size of the file on the drive but this should be good enough for tracking purposes


                    if (!bNeedsUpdate)
                    {
                        pZipJob->mJobProgress.AddBytesProcessed(cdHeader.mUncompressedSize);
                        return DecompressTaskResult(DecompressTaskResult::kAlreadyUpToDate, 0, 0, 0, 0, cdHeader.mFileName, "already matches target.");
                    }
                }

                    if (zipAPI.DecompressToFile(cdHeader.mFileName, fullPath.generic_string(), &pZipJob->mJobProgress))
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
    for (auto &result : decompResults)
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

        //		zout << taskResult << "\n";
    }



    uint64_t endTime = GetUSSinceEpoch();
    uint64_t diffMS = (endTime - startTime)/1000;

    if (nTotalErrors == 0)
        pZipJob->mJobStatus.mStatus = JobStatus::eJobStatus::kFinished;
    else 
        pZipJob->mJobStatus.mStatus = JobStatus::eJobStatus::kError;

        
    zout << "[==============================================================]\n";
    zout << "Total Files Skipped:               " << nTotalFilesSkipped << "\n";

    if (!pZipJob->mbSkipCRC)
    {
        zout << "Total Files Verified:              " << nTotalFilesUpToDate << "\n";
        zout << "Total Bytes Verified:              " << FormatFriendlyBytes(nTotalBytesVerified);
        if (nTotalTimeOnFileVerification > 0)
            zout << " (Rate:" << (nTotalBytesVerified / 1024) / (nTotalTimeOnFileVerification / 1000) << "MB/s)";
        zout << "\n";
    }

    bool bIsHTTPJob = (pZipJob->msPackageURL.substr(0, 4) == "http");  // if the url starts with "http" then we're downloading 


    if (nTotalBytesDownloaded > 0 || nTotalFilesUpdated > 0)
    {

        zout << "Total Files Extracted:             " << nTotalFilesUpdated << "\n";
        zout << "Total Folders Created:             " << nTotalFoldersCreated << "\n";
        zout << "Total Errors:                      " << nTotalErrors << "\n";

        if (bIsHTTPJob)
            zout << "Total Downloaded:                  ";
        else
            zout << "Total Bytes Extracted:             ";

        zout << FormatFriendlyBytes(nTotalBytesDownloaded);
        
        if (diffMS > 0)
            zout << " (Rate:" << (nTotalBytesDownloaded / 1024) / (diffMS) << "MB/s)";

        zout << "\n";


        zout << "Total Uncompressed Bytes Written:  " << FormatFriendlyBytes(nTotalWrittenToDisk);
        
        if (diffMS > 0)
            zout << " (Rate:" << (nTotalWrittenToDisk / 1024) / (diffMS) << "MB/s)";

        zout << "\n";
    }
    else
    {
        if (bIsHTTPJob)
            zout << "No files needed to be downloaded.\n";
        else
            zout << "No files needed to be extracted.\n";
    }

    if (pZipJob->mbVerbose)
    {
        zout << "[--------------------------------------------------------------]\n";
        zout << "Total Job Time:                    " << diffMS << "\n";
        zout << "Threads:                           " << pZipJob->mnThreads << "\n";
        zout << "[==============================================================]\n";
    }

    pZipJob->mJobStatus.mStatus = JobStatus::kFinished;
}

void ZipJob::RunListJob(void* pContext)
{
    ZipJob* pZipJob = (ZipJob*)pContext;

    string sURL = pZipJob->msPackageURL;
    string sName = pZipJob->msName;
    string sPassword = pZipJob->msPassword;

    zout << "List files in Package: " << sURL << "\n";
    if (!pZipJob->msPattern.empty())
        zout << "Files that match pattern: \"" << pZipJob->msPattern << "\"\n";

    shared_ptr<cZZFile> pZZFile;
    if (!cZZFile::Open(sURL, cZZFile::ZZFILE_READ, pZZFile,  pZipJob->mbVerbose))
    {
        pZipJob->mJobStatus.SetError(JobStatus::kError_OpenFailed, "Failed to open package: \"" + sURL + "\"");
        return;
    }

    cZipCD zipCD;
    if (!zipCD.Init(*pZZFile))
    {
        pZipJob->mJobStatus.SetError(JobStatus::kError_ReadFailed, "Failed to read Zip Central Directory from package: \"" + sURL + "\"");
        return;
    }

    zipCD.DumpCD(zout, pZipJob->msPattern, pZipJob->mbVerbose, pZipJob->mOutputFormat);



    pZipJob->mJobStatus.mStatus = JobStatus::kFinished;
}

bool ZipJob::Join()
{
    const uint64_t kMSBetweenReports = 2000;

    std::chrono::time_point<std::chrono::system_clock> timeOfLastReport = std::chrono::system_clock::now();
    while (!IsDone())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        std::chrono::milliseconds elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::system_clock::now() - timeOfLastReport);
        if ((uint64_t)elapsed_ms.count() > kMSBetweenReports && mJobProgress.GetEstimatedSecondsRemaining() > kMSBetweenReports/1000) // don't report any more if there's less than 2 seconds remaining
        {
            zout << mJobProgress.GetPercentageComplete() << "% Completed. Elapsed:" << mJobProgress.GetElapsedTimeMS() / 1000 << "s Remaining:~" << mJobProgress.GetEstimatedSecondsRemaining() << "s Rate:" << FormatFriendlyBytes(mJobProgress.GetBytesPerSecond(), SH::kMiB) << "/s Completed:" << FormatFriendlyBytes(mJobProgress.GetBytesProcessed(), SH::kMiB) << " of " << FormatFriendlyBytes(mJobProgress.GetBytesToProcess(), SH::kMiB) << " \n";
            timeOfLastReport = std::chrono::system_clock::now();
        }
    }

    for (tThreadList::iterator it = mWorkers.begin(); it != mWorkers.end(); it++)
    {
        std::thread* pThread = *it;
        pThread->join();
        delete pThread;
    }

    if (mbVerbose)
        zout << mJobStatus << "\n";

    mWorkers.clear();

    return true;
}
