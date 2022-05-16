#include "BlockScanner.h"
#include <list>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include "sha256.h"
#include <assert.h>
#include <chrono>
#include "helpers\InlineFormatter.h"
#include "helpers\ThreadPool.h"
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

InlineFormatter gFormatter;
const int64_t kQPrime = 961748941;  // prime number
//const int64_t kQPrime = 18446744073709551557;
//const int64_t kQPrime = 101;


inline int64_t GetUSSinceEpoch()
{
    return std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1);
}


BlockDescription::BlockDescription() : mRollingChecksum(0)
{
    memset(&mSHA256[0], 0, 32*sizeof(uint8_t));
}



BlockScanner::BlockScanner()
{
    mnStatus = kNone;
    mbCancel = false;
    mbVerbose = false;

    mTimeTakenUS = 0;
    mTotalSHAHashesChecked = 0;
    mTotalRollingHashesChecked = 0;
    mTotalBlocksMatched = 0;



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





// temp
/*


void SearchForPattern(string sPath)
{
    std::ifstream scanFile;
    scanFile.open(sPath, ios::binary);
    if (!scanFile)
    {
        cerr << "Failed to open scan file:" << sPath.c_str() << "\n";
    }

    scanFile.seekg(0, std::ios::end);
    size_t nSize = (uint64_t)scanFile.tellg();
    scanFile.seekg(0, std::ios::beg);



    uint8_t* pScanFileData = new uint8_t[nSize];
    //mpScanFileData = (uint8_t*)malloc(mnScanFileSize);

    scanFile.read((char*)pScanFileData, (streamsize)nSize);


    for (int i = 0; i < nSize - 8; i++)
    {
        if (
            *(pScanFileData + i + 0) == 0x75 &&
            *(pScanFileData + i + 2) == 0xa1 &&
            *(pScanFileData + i + 4) == 0xf3 &&
            *(pScanFileData + i + 6) == 0x49 &&
            *(pScanFileData + i + 8) == 0x27)
        {
            int stophere = 8;
            assert(false);
        }
    }

    cout << "nope";
    delete[] pScanFileData;
}


*/

// temp








bool BlockScanner::Scan(string sourcePath, string scanPath, uint64_t nBlockSize, int64_t nThreads, bool bVerbose)
{
    mSourcePath = sourcePath;
    mScanPath = scanPath;
    mnStatus = BlockScanner::kScanning;
    mnBlockSize = nBlockSize;
    mThreads = nThreads;
    mbVerbose = bVerbose;

    // Compute initial rolling hash for window size
    mInitialRollingHashMult = 1;
    for (int i = 1; i < (int) mnBlockSize; i++)
        mInitialRollingHashMult = (mInitialRollingHashMult * 256) % kQPrime;

    cout << "\n";
    cout << "**************************************************************\n";
    cout << "* Indexing Source:" << mSourcePath << "\n";
    cout << "**************************************************************\n";


    ComputeMetadata();


    uint64_t nStarttime = GetUSSinceEpoch();

    bool bFolderScan = false;
    char trailChar = scanPath[scanPath.length() - 1];
    if (trailChar == '/' || trailChar == '\\')
        bFolderScan = true;

    std::list<string> pathList;

    if (bFolderScan)
    {
        for (auto filePath : std::filesystem::recursive_directory_iterator(mScanPath))
        {
            if (filePath.is_regular_file())
                pathList.push_back(filePath.path().string());
        }
    }
    else
        pathList.push_back(scanPath);


    cout << "\n";
    cout << "**************************************************************\n";
    cout << "* Searching Dest:" << mScanPath << "\n";
    cout << "**************************************************************\n";



    for (auto scanPath : pathList)
    {
        std::ifstream scanFile;
        scanFile.open(scanPath, ios::binary);
        if (!scanFile)
        {
            cerr << "Failed to open scan file:" << scanPath.c_str() << "\n";
            return false;
        }

        scanFile.seekg(0, std::ios::end);
        uint64_t nScanFileSize = (uint64_t)scanFile.tellg();
        scanFile.seekg(0, std::ios::beg);



        mpScanFileData = new uint8_t[nScanFileSize];

        if (!mpScanFileData)
        {
            cerr << "Could not read scanfile. allocate failed for :" << nScanFileSize << "bytes.\n";
            return false;
        }

        cout << "Scanning file: " << scanPath << "\n";

        scanFile.read((char*)mpScanFileData, (streamsize)nScanFileSize);

        if (!scanFile)
        {
            cerr << "ONLY READ:" << scanFile.gcount() << "\n";
            return false;
        }

        scanFile.close();

        ThreadPool pool(mThreads);

        vector<shared_future<SearchJobResult> > jobResults;

/*        if (nScanFileSize <= mnBlockSize)
        {
            jobResults.emplace_back(pool.enqueue(&BlockScanner::SearchProc, scanPath, mpScanFileData, nScanFileSize, nScanFileSize, 0, nScanFileSize, this));
        }
        else*/
        {
            for (uint64_t nScanOffset = 0; nScanOffset < nScanFileSize; nScanOffset += mnBlockSize)
            {
                uint64_t nEndOffset = nScanOffset + mnBlockSize;;

                if (nEndOffset > nScanFileSize)  // for last job, make sure the last byte range is at the end of the file
                    nEndOffset = nScanFileSize;

                jobResults.emplace_back(pool.enqueue(&BlockScanner::SearchProc, scanPath, mpScanFileData, nScanFileSize, mnBlockSize, nScanOffset, nEndOffset, this));
            }
        }

        int64_t nLastReportTime = 0;
        const int64_t kTimeBetweenReports = 10000000;  // 1 second

/*        bool bDone = false;
        while (!bDone)
        {
            bDone = true;
            uint64_t nTotalBytesDone = 0;
            for (uint32_t i = 0; i < mThreads; i++)
            {
                // if any job is still running we're not done
                if (jobResults[i].get().mState == cSearchJob::cJobState::kRunning)
                    bDone = false;
                else
                    break;

                nTotalBytesDone += jobResults[i].get().nOffsetProgress;
            }

            if (GetUSSinceEpoch() - nLastReportTime > kTimeBetweenReports)
            {
                nLastReportTime = GetUSSinceEpoch();
                cout << "Scanning... " << nTotalBytesDone / (1024 * 1024) << "MiB out of " << mnScanFileSize / (1024 * 1024) << "MiB\n" << std::flush;
            }*

            Sleep(10);
        }*/


        // Join all threads
        for (auto result : jobResults)
        {
//            WaitForSingleObject(hThread[i], INFINITE);

            mTotalSHAHashesChecked += result.get().mnSHAHashesChecked;
            mTotalRollingHashesChecked += result.get().mnRollingHashesChecked;
            mTotalBlocksMatched += result.get().matchResultList.size();

            // merge results
            for (auto matchResult : result.get().matchResultList)
            {
                mResults.insert(matchResult);
            }
        }
    }

    uint64_t nEndTime = GetUSSinceEpoch();
    mTimeTakenUS = nEndTime-nStarttime;

    DumpReport();
//    mResults.clear();


    mnStatus = BlockScanner::kFinished;

    return true;
}


void BlockScanner::FillError(BlockScanner* pScanner)
{
    char* pError   = NULL;
    DWORD  dwResult = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (char*)&pError, 0, NULL);
    if(dwResult)
    {
        pScanner->msError  = pError;
        LocalFree(pError);
    }
    pScanner->mnStatus = BlockScanner::kError;
}

void BlockScanner::Cancel()
{
    mbCancel = true;
    while (mnStatus == BlockScanner::kScanning)
    {
        Sleep(50);
    }
    mnStatus = kCancelled;
}

ComputeJobResult BlockScanner::ComputeMetadataProc(const string& sFilename, BlockScanner* pScanner)
{
    uint64_t nBlockSize = pScanner->mnBlockSize;

    ComputeJobResult result;

    SHA256_CTX context;

    std::ifstream sourceFile;
    sourceFile.open(sFilename, ios::binary);
    if (!sourceFile)
    {
        cerr << "Failed to open source file:" << sFilename.c_str() << "\n";
        result.mbError = true;
        return result;
    }

    sourceFile.seekg(0, std::ios::end);
    uint64_t nScanFileSize = (uint64_t)sourceFile.tellg();
    sourceFile.seekg(0, std::ios::beg);

    if ((size_t)nBlockSize > nScanFileSize)
    {
        nBlockSize = (uint32_t)nScanFileSize;
        if (pScanner->mbVerbose)
            cout << "Blocksize set to the full source file size:" << nScanFileSize << "\n";
    }


    uint8_t* pBuff = new uint8_t[nBlockSize];

    bool bDone = false;
    uint64_t nOffset = 0;
    do
    {
        sourceFile.read((char*)pBuff, nBlockSize);
        if (sourceFile.bad())
        {
            cerr << "Couldn't read from file: " << sFilename.c_str() << "!\n";
            bDone = true;
        }
        else
        {
            size_t nNumRead = sourceFile.gcount();
            if (nNumRead > 0)
            {
                BlockDescription block;

                result.mnTotalBytesScanned += nNumRead;
                block.mRollingChecksum = pScanner->GetRollingChecksum(pBuff, nNumRead);
                block.mpPath = pScanner->UniquePath(sFilename);
                block.mnOffset = nOffset;

                sha256_init(&context);
                sha256_update(&context, pBuff, nNumRead);
                sha256_final(&context, block.mSHA256);

                uint8_t c = *pBuff;
                pScanner->mChecksumToBlockMapMutex.lock();
                pScanner->mChecksumToBlockMap[c][block.mRollingChecksum].push_back(block);
                pScanner->mChecksumToBlockMapMutex.unlock();

                nOffset += nNumRead;
            }
            else
                bDone = true;
        }
    } while (!bDone);

    delete[] pBuff;
    result.mbError = false;
    return result;
}

void BlockScanner::ComputeMetadata()
{
    bool bFolderScan = false;
    char trailChar = mSourcePath[mSourcePath.length() - 1];
    if (trailChar == '/' || trailChar == '\\')
        bFolderScan = true;

    std::list<string> pathList;

    if (bFolderScan)
    {
        for (auto filePath : std::filesystem::recursive_directory_iterator(mSourcePath))
        {
            if (filePath.is_regular_file())
                pathList.push_back(filePath.path().string());
        }
    }
    else
        pathList.push_back(mSourcePath);


    ThreadPool pool(mThreads);
    vector<shared_future<ComputeJobResult> > jobResults;


    for (auto path: pathList)
    {
        if (mbVerbose)
            cout << "Indexing file:" << path << "\n";
        jobResults.emplace_back(pool.enqueue(&BlockScanner::ComputeMetadataProc, path, this));
    }

    for (auto jobResult : jobResults)
    {
        if (jobResult.get().mbError)
        {
            cerr << "jobResult is in Error\n";
            return;
        }
        mnSourceDataSize += jobResult.get().mnTotalBytesScanned;
    }



    if (mbVerbose)
    {
        cout << "blocksize:" << mnBlockSize << "\n";
        cout << "Source file count:" << pathList.size() << "\n";
        cout << "Source data size:" << mnSourceDataSize << "\n";

        for (int i = 0; i < 256; i++)
        {
            cout << "rolling hashes:[" << i << "]:" << mChecksumToBlockMap[i].size() << "\n";

            //#define VERY_VERBOSE_OUTPUT
#ifdef VERY_VERBOSE_OUTPUT

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
                    cout << "hash:" << h.first << "count:" << h.second << "\n";
    }
            cout << "done.";
#endif
}
    }
}


//#define SIMPLE_SUM
//#define ORIGINAL
#define RABINKARP

int64_t BlockScanner::UpdateRollingChecksum(int64_t prevHash, uint8_t prevBlockFirstByte, uint8_t nextBlockLastByte, size_t dataLength)
{

#ifdef ORIGINAL
    uint16_t low16  = (uint16_t)prevHash;
    uint16_t high16 = (uint16_t)(prevHash >> 16);

    low16  += (nextBlockLastByte - prevBlockFirstByte);
    high16 += low16 - (prevBlockFirstByte << 15);

    return (uint32_t)((high16 << 16) | low16);

#endif


#ifdef SIMPLE_SUM
    return  prevHash - prevBlockFirstByte + nextBlockLastByte;
#endif



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
    for (int i = 0; i < dataLength; i++)
        nHash = (nHash*256 + pData[i])%kQPrime;

    return nHash;
#endif
}

//#define VERBOSE_SCAN
SearchJobResult BlockScanner::SearchProc(const string& sSearchFilename, uint8_t* pDataToScan, uint64_t nDataLength, uint64_t nBlockSize, uint64_t nStartOffset, uint64_t nEndOffset, BlockScanner* pScanner)
{
//    cout << "Scanning from:" << job->nStartOffset << " to:" << job->nEndOffset << "\n";

    SHA256_CTX context;

    SearchJobResult result;
    result.mnBytesScanned = nEndOffset-nStartOffset;
    bool bComputeFullChecksum = true;
    int64_t nRollingHash;

#ifdef VERBOSE_SCAN
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
            nBytesToScan = nDataLength - nOffset;

        if (bComputeFullChecksum)
        {
            nRollingHash = pScanner->GetRollingChecksum(pDataToScan + nOffset, nBytesToScan);
            bComputeFullChecksum = false;  
        }

        result.mnRollingHashesChecked++;

#ifdef VERBOSE_SCAN
        uint64_t nStartFind = GetUSSinceEpoch();
#endif

        uint8_t c = *(pDataToScan+nOffset);
        tChecksumToBlockMap::iterator blockSetIt = pScanner->mChecksumToBlockMap[c].find(nRollingHash);

#ifdef VERBOSE_SCAN
        uint64_t nEndFind = GetUSSinceEpoch();
        nUSSpendLookingUpRollingHash += (nEndFind-nStartFind);
        if (nUSSpendLookingUpRollingHash - nLastReportRollingHashTime > 1000000)
        {
            nLastReportRollingHashTime = nUSSpendLookingUpRollingHash;
            cout << "Time spent searching for rolling hash:" << nUSSpendLookingUpRollingHash << "\n";
        }
#endif

        if (blockSetIt != pScanner->mChecksumToBlockMap[c].end())
        {
            // Found a match for the checksum. Compute the SHA256
#ifdef VERBOSE_SCAN
            nStartFind = GetUSSinceEpoch();
#endif

            sha256_init(&context);
            sha256_update(&context, pDataToScan + nOffset, nBytesToScan);
            uint8_t sha256[32];
            sha256_final(&context, sha256);

#ifdef VERBOSE_SCAN
            nEndFind = GetUSSinceEpoch();
            nUSSpentComputingSHA += (nEndFind - nStartFind);
            if (nUSSpentComputingSHA - nLastReportComputingSHA > 1000000)
            {
                nLastReportComputingSHA = nUSSpentComputingSHA;
                cout << "Time spent computing SHA256:" << nUSSpentComputingSHA << "\n";
            }
#endif

            tBlockSet& blockSet = (*blockSetIt).second;

            result.mnSHAHashesChecked++;

            // Try a true MD5 match
            for (auto block : blockSet)
            {
                if (memcmp(block.mSHA256, sha256, 32) == 0)
                {
//                    cout << "True match found offset: " << nOffset << "  Source:" << block.mpPath << " offset :" << block.mnOffset << "\n";

                    sMatchResult match;
                    match.nSourceOffset = block.mnOffset;
                    match.nDestinationOffset = nOffset;
                    match.nChecksum = nRollingHash;
                    match.nMatchingBytes = nBytesToScan;
                    match.sourceFile = block.mpPath;
                    match.destFile = sSearchFilename;
                    memcpy(match.mSHA256, block.mSHA256, 32);

                    result.matchResultList.insert(match);

                    bComputeFullChecksum = true;
                    nOffset += nBytesToScan;
                    break;
                }
                else
                {
                    // fast hash collision but sha doesn't match
                    //job->nUnmatchedCollisions++;
                }
            }
        }

        if (!bComputeFullChecksum)
        {
            // no fast hash match. Advance
            uint8_t oldByte = *(pDataToScan + nOffset);
            nOffset++;

            // if we have more to compute update the fast hash
            if (nOffset < nDataLength - nBytesToScan)
            {
                uint8_t newByte = *(pDataToScan + nOffset + nBytesToScan-1);

                nRollingHash = pScanner->UpdateRollingChecksum(nRollingHash, oldByte, newByte, nBytesToScan);

//#define VEFIRY_CHECKSUM
#ifdef VEFIRY_CHECKSUM

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
    uint64_t nTotalReusableBytes = 0;
    uint64_t nMergedBlocks = 0;
    uint64_t nUnmergeableRanges = 0;
    if (mResults.size() > 0)
    {
        cout << "\n*Merged Results*" << "\n";
        cout << std::setw(16) << std::right << "src_path" << std::setw(0) << "," << std::setw(16) << std::right << "src_offset" << std::setw(0) << "," << std::setw(16) << "dst_path" << std::setw(0) << "," << std::setw(16) << std::right << "dst_offset" << std::setw(0) << "," << std::setw(16) << "bytes" << std::setw(0) << "\n";

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

            if (nextResult != mResults.end())
                nUnmergeableRanges++;

            cout << std::right << std::setw(32) << mergedResult.sourceFile << std::setw(16) << mergedResult.nSourceOffset << std::setw(0) << "," << std::setw(32) << mergedResult.destFile << std::setw(16) << mergedResult.nDestinationOffset << std::setw(0) << ","  << std::setw(16) << mergedResult.nMatchingBytes << std::setw(0) << "\n";
            nTotalReusableBytes += mergedResult.nMatchingBytes;
            nMergedBlocks++;
            result = nextResult;
        }
    }

    cout << "\n*Summary*\n";
    cout << std::left << std::setw(32) << "Block Size: " << mnBlockSize << "\n";
    cout << std::left << std::setw(32) << "Total blocks:" << mResults.size() << "\n";
    cout << std::left << std::setw(32) << "Merged Blocks:" << nMergedBlocks << "\n";
    cout << std::left << std::setw(32) << "Unmergeable Ranges:" << nUnmergeableRanges << "\n";
    cout << std::left << std::setw(32) << "Reusable bytes:" << nTotalReusableBytes << "/" << mnSourceDataSize << "\n";
    cout << std::left << std::setw(32) << "Unfound byte:  " << mnSourceDataSize - nTotalReusableBytes << "\n";
    cout << std::left << std::setw(32) << "Time Spent:" << mTimeTakenUS / 1000.0 << "ms\n";

    if (mbVerbose)
    {
        cout << "\n*Debug Metrics*\n";
        cout << std::left << std::setw(24) << "SHA Hashes Checked:" << mTotalSHAHashesChecked << "\n";
        cout << std::left << std::setw(24) << "Rolling Hashes Checked:" << mTotalRollingHashesChecked << "\n";
        cout << std::left << std::setw(24) << "Threads:" << mThreads << "\n";
    }
}
