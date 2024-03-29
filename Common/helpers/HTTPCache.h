// MIT License
// Copyright 2019 Alex Zvenigorodsky
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once
#include <stdint.h>
#include <map>
#include <mutex>
#include <memory>

const uint32_t kHTTPCacheLineSize = 4 * 1024;
const uint32_t kMaxCacheLines = 64;
typedef std::pair<int64_t, int64_t> tIntPair;
typedef std::chrono::time_point<std::chrono::system_clock> tSysClock;

class HTTPCacheLine
{
public:
    HTTPCacheLine();
    ~HTTPCacheLine();

    bool Get(int64_t nOffset, int32_t nBytes, uint8_t* pDestination);   // retrieves the byte range requested. may block until data is fullfilled
    bool Commit(int32_t nBytes);                                        // Commits nBytes and releases the reservation for this cache line

    bool        mbCommitted;
    int64_t     mnBaseOffset;
    int32_t     mnBufferData;
    uint8_t     mData[kHTTPCacheLineSize];
    tSysClock   mRequestTime;
    tSysClock   mFullfilledTime;
    tIntPair    mUnfullfilledInterval;  // lower and upper bounds of data that needs to be fullfilled
};

typedef std::map< uint64_t, std::shared_ptr<HTTPCacheLine> > tOffsetToHTTPCacheLineMap;



class HTTPCache
{
public:
    HTTPCache();
    ~HTTPCache();

    bool CheckOrReserve(int64_t nOffset, int32_t nBytes, std::shared_ptr<HTTPCacheLine>& pCacheLine);      // atomically checks if a byte range can be satisfied, if not reserves a new range for being filled. Returns true if it's a new reservation that requires fullfillment

protected:
    std::shared_ptr<HTTPCacheLine> Reserve(int64_t nOffset);

    tOffsetToHTTPCacheLineMap mOffsetToHTTPCacheLineMap;

    std::mutex mMutex;

    // metrics
//    uint64_t mnTotalBytesReserved;
};
