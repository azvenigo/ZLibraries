//////////////////////////////////////////////////////////////////////////////////////////////////
// BlockScanner
// Written by Alex Zvenigorodsky
//
#include <string>
#include <stdint.h>
#ifdef WIN32
#define NOMINMAX
#include <Windows.h>
#endif
#include <set>
#include <map>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <list>
#include <future>
#include <thread>
#include "helpers/sha256.h"

using namespace std;

typedef set<string> tStringSet;

class BlockDescription
{
public:
    BlockDescription();

    int64_t         mRollingChecksum;   // extra, can be removed since this should be a mapping of rolling checksum to set of blocks matching it
    SHA256Hash     mSHA256;
    
    const char* mpPath;
    uint64_t    mnOffset;           // offset within the file where the block is found
    uint64_t    mnSize;

    bool operator < (const BlockDescription& rhs) const
    {
        return mRollingChecksum < rhs.mRollingChecksum;
    }
};

typedef std::list<BlockDescription> tBlockSet;
typedef std::unordered_map<int64_t, tBlockSet>   tChecksumToBlockMap;

struct sMatchResult
{
    string          sourceFile;
    uint64_t        nSourceOffset;

    string          destFile;
    uint64_t        nDestinationOffset;

    uint64_t        nMatchingBytes;
    int64_t         nChecksum;
    SHA256Hash    mSHA256;

    bool operator==(const sMatchResult& rhs)
    {
        return nSourceOffset == rhs.nSourceOffset &&
               nDestinationOffset == rhs.nDestinationOffset &&
               nMatchingBytes == rhs.nMatchingBytes &&
               nChecksum == rhs.nChecksum &&
               mSHA256 == rhs.mSHA256;
    }

    bool IsAdjacent(const sMatchResult& rhs)
    {
        return (nSourceOffset + nMatchingBytes == rhs.nSourceOffset &&
            nDestinationOffset + nMatchingBytes == rhs.nDestinationOffset &&
            (sourceFile == rhs.sourceFile && destFile == rhs.destFile) );
    }

};

struct compareMatchResult
{
    bool operator() (const sMatchResult& lhs, const sMatchResult& rhs) const
    {
        if (lhs.destFile == rhs.destFile)
            return lhs.nDestinationOffset < rhs.nDestinationOffset;

        return lhs.destFile < rhs.destFile;
    }
};


typedef std::set<sMatchResult, compareMatchResult> tMatchResultList;



class SharedMemPage
{
public:
    SharedMemPage(size_t allocSize)
    {
        mpBuffer = new uint8_t[allocSize];
        mnBufferBytesReady = 0;
        mbBufferFree = true;
    }

    ~SharedMemPage()
    {
        while (!mbBufferFree)
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        delete[] mpBuffer;
    }

    uint8_t*            mpBuffer;
    std::atomic<size_t> mnBufferBytesReady;
    std::atomic<bool>   mbBufferFree;


};

class SharedMemPool
{
public:
    SharedMemPool(size_t buffers, size_t allocSize)
    {
        mPages.resize(buffers);
        for (size_t i = 0; i < buffers; i++)
            mPages[i] = new SharedMemPage(allocSize);
    }
    
    ~SharedMemPool()
    {
        for (size_t i = 0; i < mPages.size(); i++)
            delete mPages[i];
    }

    SharedMemPage* GetFreePage()
    {
        while (1)
        {
            for (size_t i = 0; i < mPages.size(); i++)
            {
                if (mPages[i]->mbBufferFree)
                {
                    mPages[i]->mbBufferFree = false;
                    return mPages[i];
                }
            }
            // none found. Need to wait for a free one
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    }

    std::vector<SharedMemPage*> mPages;
};



class BlockScanner;

class ComputeJobResult
{
public:
    ComputeJobResult(uint64_t nTotalBytesIndexed = 0, bool bError = false) : mnTotalBytesIndexed(nTotalBytesIndexed), mbError(bError) {}

    uint64_t        mnTotalBytesIndexed;
    bool            mbError;
};


class SearchJobResult
{
public:

    SearchJobResult(uint64_t nSHAHashesChecked = 0, uint64_t nRollingHashesChecked = 0, uint64_t nBytesSearched = 0, bool bError = false) :  mnSHAHashesChecked(nSHAHashesChecked), mnRollingHashesChecked(nRollingHashesChecked), mnBytesSearched(nBytesSearched), mbError(bError) {}

    tMatchResultList        matchResultList;

    // stats
    uint64_t                mnSHAHashesChecked;
    uint64_t                mnRollingHashesChecked;
    uint64_t                mnBytesSearched;

    bool                    mbError;
};



class BlockScanner
{
public:

    enum eState
    {
        kNone       = 0,
        kScanning   = 1,
        kMatching   = 2,
        kFinished   = 3,
        kCancelled  = 4,
        kError      = 5
    };

    BlockScanner();
    ~BlockScanner();

    bool			        Scan(string sourcePath, string searchPath, uint64_t nBlockSize, int64_t nThreads=16, bool bVerbose = false);
    void			        Cancel();								// Signals the thread to terminate and returns when thread has terminated

    const char*             UniquePath(const string& sPath);
    int32_t                 NumUniquePaths() { return (int32_t) mAllPaths.size(); }

    void                    DumpReport();

    int32_t			        mnStatus;				// Set by Scanner
    std::string	            msError;				// Set by Scanner

    uint64_t                mnTotalFiles;
    uint64_t                mnTotalFolders;
    uint64_t                mnTotalBytesProcessed;

private:
    tChecksumToBlockMap     mChecksumToBlockMap[256];       
    std::mutex              mChecksumToBlockMapMutex;


    tStringSet              mAllPaths;
    std::mutex              mAllPathsMutex;

    void                    ComputeMetadata();

    static SearchJobResult  SearchProc(const string& sSearchFilename, uint8_t* pDataToScan, uint64_t nDataLength, uint64_t nBlockSize, uint64_t nStartOffset, uint64_t nEndOffset, BlockScanner* pScanner);
//    static ComputeJobResult ComputeMetadataProc(const string& sFilename, BlockScanner* pScanner);
    static bool             ComputeHashesProc(BlockDescription& block, SharedMemPage* pPage, BlockScanner* pScanner);

    static void				FillError(BlockScanner* pScanner);


    // Results
    tMatchResultList        mResults;
    uint64_t                mTotalSHAHashesChecked;
    uint64_t                mTotalRollingHashesChecked;
    uint64_t                mTotalBlocksMatched;



    // checksum calc
    int64_t                 GetRollingChecksum(const uint8_t* pData, size_t dataLength);
    inline int64_t          UpdateRollingChecksum(int64_t prevHash, uint8_t prevBlockFirstByte, uint8_t nextBlockLastByte);

    int64_t                 mInitialRollingHashMult; // computed based on window length


    std::string             mSourcePath;
    std::string             mSearchPath;
    bool                    mbSelfScan;
    uint64_t                mnBlockSize;
    int64_t                 mThreads;

    std::atomic<uint64_t>   mnSourceDataSize;
    std::atomic<uint64_t>   mnSearchDataSize;

    //uint64_t                mnScanFileSize;

    SharedMemPool*          mpSharedMemPool;

    bool                    mbVerbose;
    bool		            mbCancel;
};