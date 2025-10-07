#include "BlockScanner.h"
#include <list>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <assert.h>
#include <chrono>
#include "helpers/InlineFormatter.h"
#include "helpers/ThreadPool.h"
#include "helpers/LoggingHelpers.h"
#include "helpers/sha256.h"
#include <filesystem>

using namespace std;

InlineFormatter gFormatter;
const int64_t kQPrime = 961748941;  // prime number
//const int64_t kQPrime = 18446744073709551557;
//const int64_t kQPrime = 101;


inline int64_t GetUSSinceEpoch()
{
    return std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1);
}


BlockDescription::BlockDescription() : mRollingChecksum(0),mnOffset(0),  mnSize(0)
{
}



BlockScanner::BlockScanner()
{
    mnStatus = kNone;
    mbCancel = false;
    mbSelfScan = false;

    mTotalSHAHashesChecked = 0;
    mTotalRollingHashesChecked = 0;
    mTotalBlocksMatched = 0;
    mpSharedMemPool = nullptr;
    mnTotalFiles = 0;



//#define VERIFY_ROLLING_HASH_ALG
#ifdef VERIFY_ROLLING_HASH_ALG


    const int kSize = 64;
    const int kWindow = 3;

    // Compute initial rolling hash for window size
    mInitialRollingHashMult = 1;
    for (int i = 1; i < kWindow; i++)
        mInitialRollingHashMult = (mInitialRollingHashMult * 256) % kQPrime;


    uint8_t* pTest = new uint8_t(kSize);

/*    for (int i = 0; i < kSize; i++)
    {
        pTest[i] = i;
    }*/
    memcpy(pTest, "abracadabra\0", 12);


    uint64_t nRollingHash = GetRollingChecksum(pTest, kWindow);
    for (int nTestIndex = 0; nTestIndex < kSize - kWindow; nTestIndex++)
    {
        uint64_t nTestHash = GetRollingChecksum(pTest+nTestIndex, kWindow);

        assert (nTestHash == nRollingHash);

        uint8_t nPrevByte = pTest[nTestIndex];
        uint8_t nNewByte = pTest[nTestIndex +kWindow];
        nRollingHash = UpdateRollingChecksum(nRollingHash, nPrevByte, nNewByte, kWindow);
    }





    delete[] pTest;





#endif

}

BlockScanner::~BlockScanner()
{
}



bool BlockScanner::Scan(string sourcePath, string scanPath, uint64_t nBlockSize, int64_t nThreads)
{
    mSourcePath = sourcePath;
    mSearchPath = scanPath;
    mnStatus = BlockScanner::kScanning;
    mnBlockSize = nBlockSize;
    mThreads = nThreads;

    mbSelfScan = scanPath.empty();
    if (mbSelfScan)
    {
        zout << "Performing self scan for dupes.\n";
        mSearchPath = sourcePath;
    }



    // Compute initial rolling hash for window size
    mInitialRollingHashMult = 1;
    for (int i = 1; i < (int) mnBlockSize; i++)
        mInitialRollingHashMult = (mInitialRollingHashMult * 256) % kQPrime;


    

    mpSharedMemPool = new SharedMemPool(nThreads, nBlockSize);  // TBD, make scoped ptr

    zout << "\n";
    zout << "* Indexing Source:" << mSourcePath << "\n";

    uint64_t nStartCompute = GetUSSinceEpoch();
    ComputeMetadata();
    uint64_t nEndCompute = GetUSSinceEpoch();
    

    uint64_t nStartSearch = GetUSSinceEpoch();

    bool bFolderScan = std::filesystem::is_directory(mSearchPath);
    char trailChar = mSearchPath[mSearchPath.length() - 1];
    if (bFolderScan && (trailChar != '/' || trailChar != '\\'))
        mSearchPath += "/";

    std::list<string> pathList;

    if (bFolderScan)
    {
        for (auto filePath : std::filesystem::recursive_directory_iterator(mSearchPath))
        {
            if (filePath.is_regular_file())
            {
                pathList.push_back(filePath.path().string());
                mnSearchDataSize += filePath.file_size();
            }
        }
    }
    else
    {
        pathList.push_back(mSearchPath);
        mnSearchDataSize = std::filesystem::file_size(mSearchPath);
    }
    

    zout << "\n";
    zout << "* Searching Dest:" << mSearchPath << "\n";


    uint64_t nTotalDataSearched = 0;
    for (auto scanPath : pathList)
    {
        uint64_t nScanFileSize = std::filesystem::file_size(scanPath);

        // nothing to search for 0 byte files
        if (nScanFileSize == 0)
            continue;
#ifdef WIN32

#define MEMORY_MAPPING

#endif


#ifdef MEMORY_MAPPING
        HANDLE hFile = CreateFile(scanPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, 0);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            cerr << "Could not open scanfile:" << scanPath << ".\n";
            return false;
        }

        HANDLE hFileMapping = CreateFileMapping(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);

        if (hFileMapping == 0)
        {
            cerr << "Couldn't create file mapping for scanfile:" << scanPath << " error:" << GetLastError() << ".\n";
            return false;
        }

        uint8_t* pScanFileData = (uint8_t*)MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
        if (!pScanFileData)
        {
            cerr << "Could not read scanfile. allocate failed for :" << scanPath << "bytes.\n";
            return false;
        }

#else
        std::ifstream scanFile;
        scanFile.open(scanPath, ios::binary);
        if (!scanFile)
        {
            cerr << "Failed to open scan file:" << scanPath.c_str() << "\n";
            return false;
        }


        uint8_t* pScanFileData = new uint8_t[nScanFileSize];

        if (!pScanFileData)
        {
            cerr << "Could not read scanfile. allocate failed for :" << nScanFileSize << "bytes.\n";
            return false;
        }




        zout << "Scanning file: " << scanPath << "\n";

        scanFile.read((char*)pScanFileData, (streamsize)nScanFileSize);

        if (!scanFile)
        {
            cerr << "ONLY READ:" << scanFile.gcount() << "\n";
            delete[] pScanFileData;
            return false;
        }

        scanFile.close();
#endif

        ThreadPool pool(mThreads);

        vector<shared_future<SearchJobResult> > jobResults;

        // for reporting status
        int64_t nReportTime = GetUSSinceEpoch();
        const int64_t kReportCadence = 1000000;

        for (uint64_t nSearchOffset = 0; nSearchOffset < nScanFileSize; nSearchOffset += mnBlockSize)
        {
            uint64_t nEndOffset = nSearchOffset + mnBlockSize;;

            if (nEndOffset > nScanFileSize)  // for last job, make sure the last byte range is at the end of the file
                nEndOffset = nScanFileSize;

            jobResults.emplace_back(pool.enqueue(&BlockScanner::SearchProc, scanPath, pScanFileData, nScanFileSize, mnBlockSize, nSearchOffset, nEndOffset, this));
        }

        // Compile all results
        for (auto result : jobResults)
        {
            mTotalSHAHashesChecked += result.get().mnSHAHashesChecked;
            mTotalRollingHashesChecked += result.get().mnRollingHashesChecked;
            mTotalBlocksMatched += result.get().matchResultList.size();

            nTotalDataSearched += result.get().mnBytesSearched;

            // merge results
            for (auto matchResult : result.get().matchResultList)
            {
                mResults.insert(matchResult);
            }

            int64_t nTime = GetUSSinceEpoch();
            if (LOG::gnVerbosityLevel > LVL_DEFAULT && nTime - nReportTime > kReportCadence)
            {
                zout << "Searching: " << nTotalDataSearched / (1024 * 1024) << "/" << mnSearchDataSize / (1024 * 1024) << "MiB (" << std::fixed << std::setprecision(2) << (double)nTotalDataSearched * 100.0 / (double)mnSearchDataSize << "%)\n";
                nReportTime = nTime;
            }

        }


#ifdef MEMORY_MAPPING
        CloseHandle(hFileMapping);
        CloseHandle(hFile);
#else
        delete[] pScanFileData;
#endif
    }

    uint64_t nEndSearch = GetUSSinceEpoch();

    DumpReport();

    uint64_t nIndexMBPerSec = mnSourceDataSize / (nEndCompute - nStartCompute);
    uint64_t nSearchMBPerSec = mnSearchDataSize / (nEndSearch - nStartSearch);
    zout << "Time to Index:  " << (nEndCompute - nStartCompute) / 1000 << "ms. \t" << nIndexMBPerSec << " MiB/s\n";
    zout << "Time to Search: " << (nEndSearch - nStartSearch) / 1000 << "ms. \t" << nSearchMBPerSec << " MiB/s\n";


    delete mpSharedMemPool;
    mpSharedMemPool = nullptr;

    mnStatus = BlockScanner::kFinished;

    return true;
}

bool BlockScanner::ComputeHashesProc(BlockDescription& block, SharedMemPage* pPage, BlockScanner* pScanner)
{
    block.mRollingChecksum = pScanner->GetRollingChecksum(pPage->mpBuffer, pPage->mnBufferBytesReady);
    block.mSHA256.Init();
    block.mSHA256.Compute(pPage->mpBuffer, pPage->mnBufferBytesReady);
    block.mSHA256.Final();

    uint8_t c = *pPage->mpBuffer;
    pScanner->mChecksumToBlockMapMutex.lock();
    pScanner->mChecksumToBlockMap[c][block.mRollingChecksum].emplace_back(block);
    pScanner->mChecksumToBlockMapMutex.unlock();


    pPage->mnBufferBytesReady = 0;
    pPage->mbBufferFree = true;

    return true;
}

void BlockScanner::ComputeMetadata()
{
    bool bFolderScan = std::filesystem::is_directory(mSourcePath);
    char trailChar = mSourcePath[mSourcePath.length() - 1];
    if (bFolderScan && (trailChar != '/' && trailChar != '\\'))
        mSourcePath += "/";

    std::list<string> pathList;

    mnSourceDataSize = 0;
    if (bFolderScan)
    {
        for (auto filePath : std::filesystem::recursive_directory_iterator(mSourcePath))
        {
            if (filePath.is_regular_file())
            {
                pathList.push_back(filePath.path().string());
                mnSourceDataSize += filePath.file_size();
            }
        }
    }
    else
    {
        pathList.push_back(mSourcePath);
        mnSourceDataSize = std::filesystem::file_size(mSourcePath);
    }

    if (LOG::gnVerbosityLevel > LVL_DEFAULT)
    {
        zout << "Source file count:" << pathList.size() << "\n";
        zout << "Source data size:" << mnSourceDataSize << "\n";
    }



    ThreadPool pool(mThreads);
    vector<shared_future<bool> > jobResults;

    // for reporting status
    int64_t nReportTime = GetUSSinceEpoch();
    const int64_t kReportCadence = 1000000;

    uint64_t nTotalScanned = 0;
    for (auto path: pathList)
    {
        std::ifstream sourceFile;
        sourceFile.open(path, ios::binary);
        if (!sourceFile)
        {
            cerr << "Failed to open source file:" << path.c_str() << "\n";
            return;
        }

        sourceFile.seekg(0, std::ios::end);
        uint64_t nScanFileSize = (uint64_t)sourceFile.tellg();
        sourceFile.seekg(0, std::ios::beg);

        // Nothing to scan for 0 byte files
        if (nScanFileSize == 0)
            continue;

        uint64_t nBlockSize = mnBlockSize;
        if (nScanFileSize < nBlockSize)
            nBlockSize = nScanFileSize;

        bool bDone = false;
        uint64_t nOffset = 0;
        do
        {
            SharedMemPage* pPage = mpSharedMemPool->GetFreePage();

            sourceFile.read((char*)pPage->mpBuffer, nBlockSize);

            if (sourceFile.bad())
            {
                cerr << "Couldn't read from file: " << path.c_str() << " offset:" << nOffset  << "!\n";
                bDone = true;
            }
            else
            {
                size_t nNumRead = sourceFile.gcount();
                if (nNumRead > 0)
                {
                    BlockDescription block;
                    block.mpPath = UniquePath(path);
                    block.mnOffset = nOffset;
                    block.mnSize = nNumRead;

                    pPage->mnBufferBytesReady = nNumRead;
                    jobResults.emplace_back(pool.enqueue(&BlockScanner::ComputeHashesProc, block, pPage, this));

                    nOffset += nNumRead;
                    nTotalScanned += nNumRead;
                }
                else
                {
                    pPage->mbBufferFree = true;
                    bDone = true;
                }

                int64_t nTime = GetUSSinceEpoch();
                if (LOG::gnVerbosityLevel > LVL_DEFAULT && nTime - nReportTime > kReportCadence)
                {
                    zout << "Indexing: " << nTotalScanned / (1024 * 1024) << "/" << mnSourceDataSize / (1024 * 1024) << "MiB (" << std::fixed << std::setprecision(2) << (double)nTotalScanned * 100.0 / (double)mnSourceDataSize << "%)\n";
                    nReportTime = nTime;
                }
            }
        } while (!bDone);

        sourceFile.close();
    }

    for (auto jobResult : jobResults)
    {
        if (!jobResult.get())
        {
            cerr << "jobResult is in Error\n";
            return;
        }
    }



//#define DEBUG_HASHING
#ifdef DEBUG_HASHING
    if (LOG::gnVerbosityLevel > LVL_DEFAULT)
    {
        for (int i = 0; i < 256; i++)
        {
            zout << "rolling hashes:[" << i << "]:" << mChecksumToBlockMap[i].size() << "\n";


            typedef std::pair<int64_t, int64_t> tHashesAndCountPair;
            std::list< tHashesAndCountPair > hashesAndCounts;

            for (tChecksumToBlockMap::iterator it = mChecksumToBlockMap[i].begin(); it != mChecksumToBlockMap[i].end(); it++)
            {
                tBlockSet& blockSet = (*it).second;

                hashesAndCounts.push_back(tHashesAndCountPair((*it).first, (*it).second.size()));

            }
            hashesAndCounts.sort([](auto const& a, auto const& b) { return a.second > b.second; });
            for (auto h : hashesAndCounts)
            {
                if (h.second > 1)
                    zout << "hash:" << h.first << "count:" << h.second << "\n";
            }
            zout << "done.";
        }
    }
#endif
}


//#define SIMPLE_SUM
//#define ORIGINAL
#define RABINKARP

int64_t BlockScanner::UpdateRollingChecksum(int64_t prevHash, uint8_t prevBlockFirstByte, uint8_t nextBlockLastByte)
{

#ifdef RABINKARP
    prevHash += kQPrime;
    prevHash -= (mInitialRollingHashMult * prevBlockFirstByte)%kQPrime;
    prevHash *= 256;
    prevHash += nextBlockLastByte;
    prevHash = prevHash%kQPrime;


    return prevHash;
#endif
}

int64_t BlockScanner::GetRollingChecksum(const uint8_t* pData, size_t dataLength)
{
#ifdef ORIGINAL
    uint16_t low16  = 0;
    uint16_t high16 = 0;

    while(dataLength)
    {
        const uint8_t c = *pData++;
        low16  += (uint16_t)(c);
        high16 += (uint16_t)(c * dataLength);
        dataLength--;
    }

    return (uint32_t)((high16 << 16) | low16);
#endif

#ifdef SIMPLE_SUM
    int64_t nSum = 0;
    while (dataLength--)
        nSum += *pData++;

    return nSum;
#endif

#ifdef RABINKARP
    int64_t nHash = 0;
    for (size_t i = 0; i < dataLength; i++)
        nHash = (nHash*256 + pData[i])%kQPrime;

    return nHash;
#endif
}

//#define DEBUG_SEARCH
SearchJobResult BlockScanner::SearchProc(const string& sSearchFilename, uint8_t* pDataToScan, uint64_t nDataLength, uint64_t nBlockSize, uint64_t nStartOffset, uint64_t nEndOffset, BlockScanner* pScanner)
{
//    zout << "Scanning from:" << job->nStartOffset << " to:" << job->nEndOffset << "\n";

    SearchJobResult result;
    result.mnBytesSearched = nEndOffset-nStartOffset;
    bool bComputeFullChecksum = true;
    bool bLastBlock = false;
    int64_t nRollingHash;

    const char* pSearchFilename = pScanner->UniquePath(sSearchFilename);

#ifdef DEBUG_SEARCH
    uint64_t nUSSpendLookingUpRollingHash = 0;
    uint64_t nLastReportRollingHashTime = 0;

    uint64_t nUSSpentComputingSHA = 0;
    uint64_t nLastReportComputingSHA = 0;
#endif

    for (uint64_t nOffset = nStartOffset; nOffset < nEndOffset;)
    {
        // compute how many bytes we're considering (last block may be smaller than full block size)
        uint64_t nBytesToScan = nBlockSize;
        if (nOffset + nBytesToScan > nDataLength)
        {
            nBytesToScan = nDataLength - nOffset;
            bComputeFullChecksum = true;
            bLastBlock = true;
        }

        if (bComputeFullChecksum)
        {
            nRollingHash = pScanner->GetRollingChecksum(pDataToScan + nOffset, nBytesToScan);
            bComputeFullChecksum = false;  
        }

        result.mnRollingHashesChecked++;

#ifdef DEBUG_SEARCH
        uint64_t nStartFind = GetUSSinceEpoch();
#endif

        uint8_t c = *(pDataToScan+nOffset);
        tChecksumToBlockMap::iterator blockSetIt = pScanner->mChecksumToBlockMap[c].find(nRollingHash);

#ifdef DEBUG_SEARCH
        uint64_t nEndFind = GetUSSinceEpoch();
        nUSSpendLookingUpRollingHash += (nEndFind-nStartFind);
        if (nUSSpendLookingUpRollingHash - nLastReportRollingHashTime > 1000000)
        {
            nLastReportRollingHashTime = nUSSpendLookingUpRollingHash;
            zout << "Time spent searching for rolling hash:" << nUSSpendLookingUpRollingHash << "\n";
        }
#endif

        if (blockSetIt != pScanner->mChecksumToBlockMap[c].end())
        {
            // Found a match for the checksum. Compute the SHA256
#ifdef DEBUG_SEARCH
            nStartFind = GetUSSinceEpoch();
#endif

            SHA256Hash sha256(pDataToScan + nOffset, nBytesToScan);


#ifdef DEBUG_SEARCH
            nEndFind = GetUSSinceEpoch();
            nUSSpentComputingSHA += (nEndFind - nStartFind);
            if (nUSSpentComputingSHA - nLastReportComputingSHA > 1000000)
            {
                nLastReportComputingSHA = nUSSpentComputingSHA;
                zout << "Time spent computing SHA256:" << nUSSpentComputingSHA << "\n";
            }
#endif

            tBlockSet& blockSet = (*blockSetIt).second;

            result.mnSHAHashesChecked++;

            // Try a true MD5 match
            for (auto block : blockSet)
            {
                if (sha256.operator==(block.mSHA256))
                {
//                    zout << "True match found offset: " << nOffset << "  Source:" << block.mpPath << " offset :" << block.mnOffset << "\n";

                    bool bSelfMatch = (pScanner->mbSelfScan && block.mpPath == pSearchFilename && block.mnOffset == nOffset);  // if self scan, ignore matches for the same file at the same offset

                    if (!bSelfMatch)
                    {
                        sMatchResult match;
                        match.nSourceOffset = block.mnOffset;
                        match.nDestinationOffset = nOffset;
                        match.nChecksum = nRollingHash;
                        match.nMatchingBytes = nBytesToScan;
                        match.sourceFile = block.mpPath;
                        match.destFile = sSearchFilename;
                        match.mSHA256 = block.mSHA256;

                        result.matchResultList.insert(match);

                        bComputeFullChecksum = true;
                        nOffset += nBytesToScan;
                    }
                    break;
                }
                else
                {
                    // fast hash collision but sha doesn't match
                    //job->nUnmatchedCollisions++;
                }
            }
        }

        if (bLastBlock)
            break;

        if (!bComputeFullChecksum)
        {
            // no fast hash match. Advance
            uint8_t oldByte = *(pDataToScan + nOffset);
            nOffset++;

            // if we have more to compute update the fast hash
            if (nOffset < nDataLength - nBytesToScan)
            {
                uint8_t newByte = *(pDataToScan + nOffset + nBytesToScan-1);

                nRollingHash = pScanner->UpdateRollingChecksum(nRollingHash, oldByte, newByte);

//#define DEBUG_VERIFY_ROLLING_CHECKSUM
#ifdef DEBUG_VERIFY_ROLLING_CHECKSUM

                int64_t nVerify = pScanner->GetRollingChecksum(pDataToScan+nOffset, nBytesToScan);
                if (nVerify != nRollingHash)
                {
                    assert(false);
                }
#endif


            }
        }
    }

    return result;
}



const char* BlockScanner::UniquePath(const string& sPath)
{
    std::lock_guard<std::mutex> guard(mAllPathsMutex);
    tStringSet::iterator it = mAllPaths.insert(sPath).first;

    return (*it).c_str();
}

void BlockScanner::DumpReport()
{
    zout << "**************************************************************\n";
    zout << "*                         Report                             *\n";
    zout << "**************************************************************\n";

    uint64_t nTotalReusableBytes = 0;
    uint64_t nMergedBlocks = 0;

    std::string sCommonSource;
    std::string sCommonDest;

    if (std::filesystem::is_directory(mSourcePath))
    {
        sCommonSource = mSourcePath;
        zout << "Source Base: " << sCommonSource << "\n";
    }

    if (!mSearchPath.empty())
    {
        if (std::filesystem::is_directory(mSearchPath))
        {
            sCommonDest = mSearchPath;
            zout << "Search Base: " << sCommonDest << "\n";
        }
    }

    size_t commonSourceChars = sCommonSource.length();
    size_t commonDestChars = sCommonDest.length();


    list<sMatchResult> fullFileMatches;
    list<sMatchResult> partialMatches;
    Table table;
    table.SetBorders("*", "*", "*", "*", ",");
    Table::kDefaultStyle = Table::Style(COL_RESET, Table::LEFT, 0.0, Table::NO_WRAP, 10, ' ');

    if (mResults.size() > 0)
    {
        for (auto result = mResults.begin(); result != mResults.end();)
        {
            sMatchResult mergedResult(*result);
            auto nextResult = result;
            nextResult++;
            while ( nextResult != mResults.end() && mergedResult.IsAdjacent(*nextResult))
            {
                mergedResult.nMatchingBytes += (*nextResult).nMatchingBytes;
                nextResult++;
            }

            size_t nDestFileSize = std::filesystem::file_size(mergedResult.destFile);

            if (nDestFileSize == mergedResult.nMatchingBytes)
                fullFileMatches.push_back(mergedResult);
            else
                partialMatches.push_back(mergedResult);

            nTotalReusableBytes += mergedResult.nMatchingBytes;
            nMergedBlocks++;
            result = nextResult;
        }

        if (LOG::gnVerbosityLevel > LVL_DEFAULT)
        {
            if (fullFileMatches.size() > 0)
            {
                zout << "\n*Full File Matched Results*\n";
                table.AddRow("src_path", "dst_path", "bytes");

                for (auto result : fullFileMatches)
                {
                    table.AddRow(result.sourceFile.substr(commonSourceChars), result.destFile.substr(commonDestChars), result.nMatchingBytes);
                }
                zout << (string)table;
                table.Clear();
            }

            if (partialMatches.size() > 0)
            {
                zout << "\n*Partial File Matched Results*\n";
                table.AddRow("src_path", "src_offset", "dst_path", "dst_offset", "bytes");
                for (auto result : partialMatches)
                {
                    table.AddRow(result.sourceFile.substr(commonSourceChars), result.nSourceOffset, result.destFile.substr(commonDestChars), result.nDestinationOffset, result.nMatchingBytes);
                }
                zout << (string)table;
                table.Clear();
            }
        }
    }


    

    table.SetBorders("*", "*", "*", "*", ":");
    
    zout << "\n*Summary*\n";
    table.AddRow("Source bytes", (uint64_t)mnSourceDataSize);
    table.AddRow("Search bytes", (uint64_t)mnSearchDataSize);

    table.AddRow("Block Size", mnBlockSize);
    table.AddRow("Individual blocks found", mResults.size());
    table.AddRow("Merged referrable ranges", nMergedBlocks);
    if (mbSelfScan)
    {
        table.AddRow("Duplicate bytes", nTotalReusableBytes);
        table.AddRow("Duplicate percent", (double)nTotalReusableBytes * 100.0 / (double)mnSourceDataSize);
    }
    else
    {
        table.AddRow("Reusable bytes", nTotalReusableBytes);
        table.AddRow("Reusable percent", (double)nTotalReusableBytes * 100.0 / (double)mnSourceDataSize);
    }
    table.AddRow("Unfound bytes", mnSearchDataSize - nTotalReusableBytes);

    zout << (string) table;

    if (LOG::gnVerbosityLevel > LVL_DEFAULT)
    {
        zout << "\n*Debug Metrics*\n";
        zout << std::left << std::setw(24) << "SHA Hashes Checked:" << mTotalSHAHashesChecked << "\n";
        zout << std::left << std::setw(24) << "Rolling Hashes Checked:" << mTotalRollingHashesChecked << "\n";
        zout << std::left << std::setw(24) << "Threads:" << mThreads << "\n";
    }
}
