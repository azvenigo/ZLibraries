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
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

const uint32_t kMaxThreads = 128;
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

    mnNumBlocks = 0;

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








bool BlockScanner::Scan(string sourcePath, string scanPath, uint64_t nBlockSize, int64_t nThreads)
{

    mSourcePath = sourcePath;
    mScanPath = scanPath;
    mnStatus = BlockScanner::kScanning;
    mnBlockSize = nBlockSize;
    mThreads = nThreads;

    // Compute initial rolling hash for window size
    mInitialRollingHashMult = 1;
    for (int i = 1; i < (int) mnBlockSize; i++)
        mInitialRollingHashMult = (mInitialRollingHashMult * 256) % kQPrime;


    ComputeMetadata(false);


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
    cout << "************************************\n";
    cout << "**             Scanning           **\n";
    cout << "************************************\n";



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
        mnScanFileSize = (uint64_t)scanFile.tellg();
        scanFile.seekg(0, std::ios::beg);



        mpScanFileData = new uint8_t[mnScanFileSize];

        if (!mpScanFileData)
        {
            cerr << "Could not read scanfile. allocate failed for :" << mnScanFileSize << "bytes.\n";
            return false;
        }

        cout << "Scanning file: " << scanPath << "\n";

        scanFile.read((char*)mpScanFileData, (streamsize)mnScanFileSize);

        if (!scanFile)
        {
            cerr << "ONLY READ:" << scanFile.gcount() << "\n";
            return false;
        }

        scanFile.close();



        HANDLE hThread[kMaxThreads];
        DWORD ThreadID[kMaxThreads];
        cJob jobs[kMaxThreads];

        uint64_t nBytesPerThread = (mnScanFileSize - mnBlockSize) / mThreads;

        for (uint32_t i = 0; i < mThreads; i++)
        {
            jobs[i].mState = cJob::cJobState::kRunning;
            jobs[i].pScanner = this;
            jobs[i].pDataToScan = mpScanFileData;
            jobs[i].nDataLength = mnScanFileSize;
            jobs[i].nBlockSize = mnBlockSize;       // can be variable if need be

            jobs[i].nStartOffset = i * nBytesPerThread;
            jobs[i].nEndOffset = (i + 1) * nBytesPerThread;

            if (i == mThreads - 1)  // for last job, make sure the last byte range is at the end of the file
                jobs[i].nEndOffset = mnScanFileSize - mnBlockSize+1;

            hThread[i] = CreateThread(NULL, 0, BlockScanner::ScanProc, (void*)&(jobs[i]), 0, &ThreadID[i]);
        }


        int64_t nLastReportTime = 0;
        const int64_t kTimeBetweenReports = 10000000;  // 1 second

        bool bDone = false;
        while (!bDone)
        {
            bDone = true;
            uint64_t nTotalBytesDone = 0;
            for (uint32_t i = 0; i < mThreads; i++)
            {
                // if any job is still running we're not done
                if (jobs[i].mState == cJob::cJobState::kRunning)
                    bDone = false;
                else
                    break;

                nTotalBytesDone += jobs[i].nOffsetProgress;
            }

/*            if (GetUSSinceEpoch() - nLastReportTime > kTimeBetweenReports)
            {
                nLastReportTime = GetUSSinceEpoch();
                cout << "Scanning... " << nTotalBytesDone / (1024 * 1024) << "MiB out of " << mnScanFileSize / (1024 * 1024) << "MiB\n" << std::flush;
            }*/ 

            Sleep(10);
        }


        // Join all threads
        for (uint32_t i = 0; i < mThreads; i++)
        {
            WaitForSingleObject(hThread[i], INFINITE);

            mTotalSHAHashesChecked += jobs[i].nSHAHashesChecked;
            mTotalRollingHashesChecked += jobs[i].nRollingHashesChecked;
            mTotalBlocksMatched += jobs[i].matchResultList.size();

            for (tMatchResultList::iterator resultIt = jobs[i].matchResultList.begin(); resultIt != jobs[i].matchResultList.end(); resultIt++)
            {
                sMatchResult result = *resultIt;
                //            cout << "Match. Source:" << result.nSourceOffset << " Dest:" << result.nDestinationOffset << " bytes:" << result.nMatchingBytes << " checksum:0x" << std::hex << result.nChecksum << std::dec << "\n";

                mResults.insert(result);

                //            nTotalBytesMatched += result.nMatchingBytes;
            }
        }

        DumpReport();
        mResults.clear();
    }

    uint64_t nEndTime = GetUSSinceEpoch();
    mTimeTakenUS = nEndTime-nStarttime;

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

void BlockScanner::ComputeMetadata(bool bVerbose)
{
    cout << "Computing Metadata for file: " << mSourcePath.c_str() << ". BlockSize:" << mnBlockSize << "\n";

    uint8_t* pBuff = new uint8_t[mnBlockSize];
    
    SHA256_CTX context;

    std::ifstream sourceFile;
    sourceFile.open(mSourcePath, ios::binary);
    if (!sourceFile)
    {
        cerr << "Failed to open source file:" << mSourcePath.c_str() << "\n";
        return;
    }

    sourceFile.seekg(0, std::ios::end);
    mnScanFileSize = (uint64_t)sourceFile.tellg();
    sourceFile.seekg(0, std::ios::beg);

    if ((size_t)mnBlockSize > mnScanFileSize)
    {
        mnBlockSize = (uint32_t)mnScanFileSize;
        cout << "Blocksize set to the full source file size:" << mnScanFileSize << "\n";
    }



    bool bDone = false;
    uint64_t nOffset = 0;
    do 
    {
        sourceFile.read((char*)pBuff, mnBlockSize);
        if (sourceFile.bad())
        {
            cout << "Couldn't read from file: " << mSourcePath.c_str() << "!\n";
            bDone = true;
        }
        else
        {
            size_t nNumRead = sourceFile.gcount();
            if (nNumRead > 0)
            {
                BlockDescription block;
            
                mnTotalBytesProcessed += nNumRead;
                block.mRollingChecksum = GetRollingChecksum(pBuff, nNumRead);
                block.mpPath = UniquePath(mSourcePath);
                block.mnOffset = nOffset;

                sha256_init(&context);
                sha256_update(&context, pBuff, nNumRead);
                sha256_final(&context, block.mSHA256);

                uint8_t c = *pBuff;
                mChecksumToBlockMap[c][block.mRollingChecksum].push_back(block);

                nOffset += nNumRead;
                mnNumBlocks++;
            }
            else
                bDone = true;
        }
    } while (!bDone);

    mnSourcePathSize = nOffset;


    if (bVerbose)
    {
        cout << "blocksize:" << mnBlockSize << "\n";
        cout << "Source file size:" << mnSourcePathSize << "\n";
        cout << "total blocks in source file:" << mnSourcePathSize / mnBlockSize << "\n";

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
DWORD WINAPI BlockScanner::ScanProc(LPVOID lpParameter)
{
    cJob* job = (cJob*) lpParameter;
    BlockScanner* pScanner = job->pScanner;

//    cout << "Scanning from:" << job->nStartOffset << " to:" << job->nEndOffset << "\n";

    SHA256_CTX context;

    bool bComputeFullChecksum = true;
    int64_t nRollingHash;

#ifdef VERBOSE_SCAN
    uint64_t nUSSpendLookingUpRollingHash = 0;
    uint64_t nLastReportRollingHashTime = 0;

    uint64_t nUSSpentComputingSHA = 0;
    uint64_t nLastReportComputingSHA = 0;
#endif

    for (uint64_t nOffset = job->nStartOffset; nOffset < job->nEndOffset;)
    {
        // compute how many bytes we're considering (last block may be smaller than full block size)
        uint64_t nBytesToScan = job->nBlockSize;
        if (nOffset + nBytesToScan > job->nDataLength)
            nBytesToScan = job->nDataLength - nOffset;

        if (bComputeFullChecksum)
        {
            nRollingHash = job->pScanner->GetRollingChecksum(job->pDataToScan + nOffset, nBytesToScan);
            bComputeFullChecksum = false;  
        }

        job->nRollingHashesChecked++;
        job->nOffsetProgress = nOffset-job->nStartOffset;

#ifdef VERBOSE_SCAN
        uint64_t nStartFind = GetUSSinceEpoch();
#endif

        uint8_t c = *(job->pDataToScan+nOffset);
        tChecksumToBlockMap::iterator blockSetIt = job->pScanner->mChecksumToBlockMap[c].find(nRollingHash);

#ifdef VERBOSE_SCAN
        uint64_t nEndFind = GetUSSinceEpoch();
        nUSSpendLookingUpRollingHash += (nEndFind-nStartFind);
        if (nUSSpendLookingUpRollingHash - nLastReportRollingHashTime > 1000000)
        {
            nLastReportRollingHashTime = nUSSpendLookingUpRollingHash;
            cout << "Time spent searching for rolling hash:" << nUSSpendLookingUpRollingHash << "\n";
        }
#endif

        if (blockSetIt != job->pScanner->mChecksumToBlockMap[c].end())
        {
            // Found a match for the checksum. Compute the SHA256
#ifdef VERBOSE_SCAN
            nStartFind = GetUSSinceEpoch();
#endif

            sha256_init(&context);
            sha256_update(&context, job->pDataToScan + nOffset, nBytesToScan);
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

            job->nSHAHashesChecked++;

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
                    memcpy(match.mSHA256, block.mSHA256, 32);

                    job->matchResultList.insert(match);

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
            uint8_t oldByte = *(job->pDataToScan + nOffset);
            nOffset++;

            // if we have more to compute update the fast hash
            if (nOffset < job->nDataLength - nBytesToScan)
            {
                uint8_t newByte = *(job->pDataToScan + nOffset + nBytesToScan-1);

                nRollingHash = job->pScanner->UpdateRollingChecksum(nRollingHash, oldByte, newByte, nBytesToScan);

//#define VEFIRY_CHECKSUM
#ifdef VEFIRY_CHECKSUM

                int64_t nVerify = job->pScanner->GetRollingChecksum(job->pDataToScan+nOffset, nBytesToScan);
                if (nVerify != nRollingHash)
                {
                    assert(false);
                }
#endif


            }
        }
    }

    job->mState = cJob::cJobState::kDone;
    return 0;
}



const char* BlockScanner::UniquePath(const string& sPath)
{
    tStringSet::iterator it = mAllPaths.insert(sPath).first;
   return (*it).c_str();
}

void BlockScanner::DumpReport()
{
    uint64_t nTotalReusableBytes = 0;
    uint64_t nMergedBlocks = 0;
    if (mResults.size() > 0)
    {
        cout << "\n*Merged Results*" << "\n";
        cout << std::setw(16) << std::right << "src" << std::setw(0) << "," << std::setw(16) << "dst" << std::setw(0) << "," << std::setw(16) << "bytes" << std::setw(0) << "\n";

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

            cout << std::right << std::setw(16) << mergedResult.nSourceOffset << std::setw(0) << "," << std::setw(16) << mergedResult.nDestinationOffset << std::setw(0) << ","  << std::setw(16) << mergedResult.nMatchingBytes << std::setw(0) << "\n";
            nTotalReusableBytes += mergedResult.nMatchingBytes;
            nMergedBlocks++;
            result = nextResult;
        }
    }

    cout << "\n*Summary*\n";
    cout << std::left << std::setw(16) << "Block Size: " << mnBlockSize << "\n";
    cout << std::left << std::setw(16) << "Unmerged blocks:" << mResults.size() << "\n";
    cout << std::left << std::setw(16) << "Merged Blocks:" << nMergedBlocks << "\n";
    cout << std::left << std::setw(16) << "Reusable bytes:" << nTotalReusableBytes << " out of:" << mnScanFileSize << "\n";

    cout << "\n*Debug Metrics*\n";
    cout << std::left << std::setw(24) << "SHA Hashes Checked:" << mTotalSHAHashesChecked << "\n";
    cout << std::left << std::setw(24) << "Rolling Hashes Checked:" << mTotalRollingHashesChecked << "\n";
    cout << std::left << std::setw(24) << "Threads:" << mThreads << "\n";
    cout << std::left << std::setw(24) << "Time Spent:" << mTimeTakenUS / 1000.0 << "ms\n";
}
