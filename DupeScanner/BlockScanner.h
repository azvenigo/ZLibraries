//////////////////////////////////////////////////////////////////////////////////////////////////
// BlockScanner
// Written by Alex Zvenigorodsky
//
#include <string>
#include <stdint.h>
#include <Windows.h>
#include <set>
#include <map>
#include <unordered_map>
#include <mutex>
#include <list>

using namespace std;

typedef set<string> tStringSet;

class BlockDescription
{
public:
    BlockDescription();

    int64_t        mRollingChecksum;   // extra, can be removed since this should be a mapping of rolling checksum to set of blocks matching it
    uint8_t         mSHA256[32];    
    const char*     mpPath;
    uint64_t        mnOffset;           // offset within the file where the block is found

    bool operator < (const BlockDescription& rhs) const
    {
        return mRollingChecksum < rhs.mRollingChecksum;
    }
};

typedef std::list<BlockDescription> tBlockSet;
typedef std::unordered_map<int64_t, tBlockSet>   tChecksumToBlockMap;

struct sMatchResult
{
    uint64_t        nSourceOffset;
    uint64_t        nDestinationOffset;
    uint64_t        nMatchingBytes;
    int64_t         nChecksum;
    uint8_t         mSHA256[32];

    bool operator==(const sMatchResult& rhs)
    {
        return nSourceOffset == rhs.nSourceOffset &&
               nDestinationOffset == rhs.nDestinationOffset &&
               nMatchingBytes == rhs.nMatchingBytes &&
               nChecksum == rhs.nChecksum &&
               memcmp(mSHA256, rhs.mSHA256, 32) == 0;
    }

    bool IsAdjacent(const sMatchResult& rhs)
    {
        return (nSourceOffset + nMatchingBytes == rhs.nSourceOffset &&
            nDestinationOffset + nMatchingBytes == rhs.nDestinationOffset);
    }

};

struct compareMatchResult
{
    bool operator() (const sMatchResult& lhs, const sMatchResult& rhs) const
    {
        return lhs.nDestinationOffset < rhs.nDestinationOffset;
    }
};


typedef std::set<sMatchResult, compareMatchResult> tMatchResultList;




class BlockScanner;
class cJob
{
public:

    enum cJobState
    {
        kNone = 0,
        kRunning = 1,
        kDone = 2,
        kError = 3
    };

    cJob() : mState(kNone), pScanner(nullptr), nBlockSize(0), pDataToScan(nullptr), nDataLength(0), nStartOffset(0), nEndOffset(0), nSHAHashesChecked(0), nRollingHashesChecked(0), nOffsetProgress(0) {}

    cJobState               mState;

    BlockScanner*           pScanner;

    uint64_t                nBlockSize;

    uint8_t*                pDataToScan;
    uint64_t                nDataLength;

    uint64_t                nStartOffset;
    uint64_t                nEndOffset;

    tMatchResultList        matchResultList;

    // stats
    uint64_t                nSHAHashesChecked;
    uint64_t                nRollingHashesChecked;

    uint64_t                nOffsetProgress;
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

    bool			        Scan(string sourcePath, string scanPath, uint64_t nBlockSize, int64_t nThreads=16);
    void			        Cancel();								// Signals the thread to terminate and returns when thread has terminated

    const char*             UniquePath(const string& sPath);
    int32_t                 NumUniquePaths() { return (int32_t) mAllPaths.size(); }

    void                    DumpReport();

    int32_t			        mnStatus;				// Set by Scanner
    std::string	            msError;				// Set by Scanner

    uint64_t                mnNumBlocks;
    uint64_t                mnTotalFiles;
    uint64_t                mnTotalFolders;
    uint64_t                mnTotalBytesProcessed;

private:
    tChecksumToBlockMap     mChecksumToBlockMap[256];        
    tStringSet              mAllPaths;

    void                    ComputeMetadata(bool bVerbose = false);

    static DWORD WINAPI		ScanProc(LPVOID lpParameter);
    static void				FillError(BlockScanner* pScanner);


    // Results
    tMatchResultList        mResults;
    uint64_t                mTimeTakenUS;
    uint64_t                mTotalSHAHashesChecked;
    uint64_t                mTotalRollingHashesChecked;
    uint64_t                mTotalBlocksMatched;



    // checksum calc
    int64_t                 GetRollingChecksum(const uint8_t* pData, size_t dataLength);
    inline int64_t          UpdateRollingChecksum(int64_t prevHash, uint8_t prevBlockFirstByte, uint8_t nextBlockLastByte, size_t dataLength);

    int64_t                 mInitialRollingHashMult; // computed based on window length


    std::string             mSourcePath;
    std::string             mScanPath;
    uint64_t                mnBlockSize;
    int64_t                 mThreads;
    uint64_t                mnSourcePathSize;

    uint8_t*                mpScanFileData;     // read into ram before setting the scanning threads going
    uint64_t                mnScanFileSize;

    bool		            mbCancel;
};