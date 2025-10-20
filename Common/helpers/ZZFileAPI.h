// MIT License
// Copyright 2025 Alex Zvenigorodsky
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include <string>
#include <fstream>
#include <atomic>
#include <mutex>
#include <vector>
#include <iostream>
#include <memory>
#include <filesystem>
#include <queue>
#include <functional>
#include <thread>

#ifdef ENABLE_HTTP
#define USE_HTTP_CACHE
#include "HTTPCache.h"
#endif

#ifdef _WIN64
#include <Windows.h>
#endif



const static int64_t ZZFILE_NO_SEEK = -1;
const static int64_t ZZFILE_SEEK_END = -2;

const static int64_t kZZFileError_None          = 0;
const static int64_t kZZFileError_IllegalSeek   = -5029;
const static int64_t kZZFileError_OutOfBounds   = -5022;
const static int64_t kZZFileError_Unsupported   = -5003;

const static int64_t kZZFileError_Unknown       = -5099;

#ifdef ENABLE_CURL
#include "curl/curl.h"
#endif

#ifdef ENABLE_MMIO
#include "mio/mmap.hpp"
#endif


namespace ZFile
{
    typedef std::shared_ptr<class ZFileBase> tZFilePtr;

#ifdef ENABLE_HTTP
    // Forward declarations
    class ZFileHTTP;
    class ZFileHTTPSession;

    // Shared pointer type for session
    typedef std::shared_ptr<ZFileHTTPSession> tZFileHTTPSessionPtr;

    extern bool gbSkipCertCheck;

#endif



    //////////////////////////////////////////////////////////////////////////////////////////
    // ZFileBase Interface 
    class ZFileBase
    {
    public:
        enum eOpenFlags : uint32_t
        {
            kRead           = 0,
            kWrite          = 1 << 0,   // 1
            kTrunc          = 1 << 1,   // 2
            kUnbuffered     = 1 << 2,   // 4
        };

        // Factory Construction
        // returns either a ZFileLocal, ZFileHTTP or a  depending on the url needs
        static bool Open(const std::string& sURL, tZFilePtr& pFile, uint32_t flags = eOpenFlags::kRead, bool bVerbose = false);

#ifdef ENABLE_HTTP
        static bool IsHTTP(const std::string& sURL);
#endif
        static bool Exists(const std::string& sURL, bool bVerbose = false);
        static bool IsDirectory(const std::string& sURL);
        static bool EnsureFolderForPathExists(std::filesystem::path outputPath);

        virtual                 ~ZFileBase() {};

        virtual bool            Close() = 0;
        virtual bool            Read(int64_t nOffset, int64_t nBytes, uint8_t* pDestination, int64_t& nBytesRead) = 0;
        virtual bool            Write(int64_t nOffset, int64_t nBytes, uint8_t* pSource, int64_t& nBytesWritten) = 0;

        virtual size_t          Read(uint8_t* pDestination, int64_t nBytes) = 0;   // wrapper to support stream like interface
        virtual size_t          Write(uint8_t* pSource, int64_t nBytes) = 0;       // wrapper to support stream like interface
        virtual void            SeekRead(int64_t offset) = 0;
        virtual void            SeekWrite(int64_t offset) = 0;


        virtual bool            FreeSpace(const std::string& sPath, int64_t& nOutBytes, bool bVerbose = false) { return false; }

        virtual uint64_t        GetFileSize() { return mnFileSize; }
        virtual int64_t         GetLastError() { return mnLastError; }

    protected:
        ZFileBase();

        virtual bool            OpenInternal(std::string sURL, uint32_t flags, bool bVerbose) = 0;
        bool                    IsSet(uint32_t flag) { return (mOpenFlags & flag) != 0; }

        std::filesystem::path   mPath;
        uint32_t                mOpenFlags;
        bool                    mbVerbose;
        int64_t		            mnFileSize;
        int64_t                 mnLastError;

        int64_t                 mnReadOffset;
        int64_t                 mnWriteOffset;
    };




    //////////////////////////////////////////////////////////////////////////////////////////
    class ZFileLocal : public ZFileBase
    {
        friend class ZFileBase;
    public:
        ZFileLocal();
        ~ZFileLocal();

        virtual bool    Close();
        virtual bool    Read(int64_t nOffset, int64_t nBytes, uint8_t* pDestination, int64_t& nBytesRead);
        virtual bool    Write(int64_t nOffset, int64_t nBytes, uint8_t* pSource, int64_t& nBytesWritten);

        virtual size_t  Read(uint8_t* pDestination, int64_t nBytes);
        virtual size_t  Write(uint8_t* pSource, int64_t nBytes);
        virtual void    SeekRead(int64_t offset);
        virtual void    SeekWrite(int64_t offset);

        virtual bool    OpenInternal(std::string sURL, uint32_t flags, bool bVerbose);

        virtual bool    FreeSpace(const std::string& sPath, int64_t& nOutBytes, bool bVerbose = false);

        static std::string Canonical(std::string sPath);    // return a weakly canonical path. If on windows, also turn into UNC path

    protected:
        std::mutex      mMutex;
        HANDLE          mhFile;
#ifdef _WIN64
        DWORD           mSectorSize;
#endif
    };






    //////////////////////////////////////////////////////////////////////////////////////////
    // utility class for ZFileRAM 
    class ZMemory
    {
    public:
        static ZMemory* Create(size_t nSize);


        static const size_t kAllocUnit = 16 * 1024;

        ZMemory(size_t nSize = 0) {}
        virtual ~ZMemory() {}

        virtual void        Resize(size_t newsize) = 0;
        virtual uint8_t*    Data() = 0;
        virtual size_t      Size() const = 0;

        static size_t       RoundToPage(size_t size) { return ((size + kAllocUnit - 1) / kAllocUnit) * kAllocUnit; }
    };

    typedef std::shared_ptr<class ZMemory> tZMemoryPtr;





    //////////////////////////////////////////////////////////////////////////////////////////
    class ZFileRAM : public ZFileBase
    {
        friend class ZFileBase;
    public:
        ZFileRAM(size_t initialSize = 0);
        virtual ~ZFileRAM() {}

        virtual bool        From(const tZFilePtr fromFile);

        virtual bool        Close();
        virtual bool        Read(int64_t nOffset, int64_t nBytes, uint8_t* pDestination, int64_t& nBytesRead);
        virtual bool        Write(int64_t nOffset, int64_t nBytes, uint8_t* pSource, int64_t& nBytesWritten);

        virtual size_t      Read(uint8_t* pDestination, int64_t nBytes);
        virtual size_t      Write(uint8_t* pSource, int64_t nBytes);

        virtual void        SeekRead(int64_t offset);
        virtual void        SeekWrite(int64_t offset);

        virtual uint64_t    GetFileSize();

        virtual uint8_t*    GetBuffer();

    protected:

        virtual bool        OpenInternal(std::string sURL, uint32_t flags, bool bVerbose) { return true; }

        tZMemoryPtr         mpBuffer;
        std::mutex          mMutex;
    };


#ifdef ENABLE_MMIO
    //////////////////////////////////////////////////////////////////////////////////////////
    class ZFileMMIO : public ZFileBase
    {
        friend class ZFileBase;
    public:
        ZFileMMIO();
        virtual ~ZFileMMIO();

        virtual bool        Close();
        virtual bool        Read(int64_t nOffset, int64_t nBytes, uint8_t* pDestination, int64_t& nBytesRead);
        virtual bool        Write(int64_t nOffset, int64_t nBytes, uint8_t* pSource, int64_t& nBytesWritten);

        virtual size_t      Read(uint8_t* pDestination, int64_t nBytes);
        virtual size_t      Write(uint8_t* pSource, int64_t nBytes);

        virtual void        SeekRead(int64_t offset);
        virtual void        SeekWrite(int64_t offset);

        virtual uint8_t*    GetBuffer();

        virtual bool        OpenInternal(std::string sURL, uint32_t flags, bool bVerbose);

    protected:

        mio::mmap_source    ro_mmap;
        uint8_t*            mpBuffer;
        std::mutex          mMutex;
    };
#endif



#ifdef ENABLE_HTTP
    //////////////////////////////////////////////////////////////////////////////////////////
    class ZFileHTTP : public ZFileBase
    {
        friend class ZFileBase;
    public:
        ~ZFileHTTP();

        virtual bool            Close();
        virtual bool            Read(int64_t nOffset, int64_t nBytes, uint8_t* pDestination, int64_t& nBytesRead);
        virtual size_t          Read(uint8_t* pDestination, int64_t nBytesRead);

        virtual bool            Write(int64_t, int64_t, uint8_t*, int64_t&);    
        virtual size_t          Write(uint8_t*, int64_t);                       

        virtual void            SeekRead(int64_t offset);
        virtual void            SeekWrite(int64_t offset);


        static std::string      ToURLPath(const std::string& sPath); // converts slashes if necessary 
        static bool             GetURLAuth(std::string sURL, std::string& sName, std::string& sPassword);

        std::string             GetServerResponse() const { return msResponse; }

    protected:
        ZFileHTTP();

        virtual bool	        OpenInternal(std::string sURL, uint32_t flags, bool bVerbose);
        bool                    ReadFromCurlRange(int64_t nOffset, int64_t nBytes, uint8_t* pDestination, int64_t& nBytesRead);

        std::string             msURL;
        std::string             msHost;
        std::string             msSessionBaseURL;

        // Auth
        std::string             msName;
        std::string             msPassword;

        // write to HTTP server via post support
        ZFileRAM*               mpFileRAM;
        std::string             msResponse;

#ifdef ENABLE_CURL
        static size_t           write_data(char* buffer, size_t size, size_t nitems, void* userp);
        static size_t           write_response_to_string(char* buffer, size_t size, size_t nitems, void* userp);

        static void             lock_cb(CURL* handle, curl_lock_data data, curl_lock_access access, void* userp);
        static void             unlock_cb(CURL* handle, curl_lock_data data, void* userp);

        CURLSH*                 mpCurlShare;
        std::recursive_mutex    mCurlMutex;
        tZFileHTTPSessionPtr    mpSession;
        bool                    mbUseAsyncPostOnClose;

        bool                    HandleHTTPError(int error, const std::string& sFunction, bool& bRetriable);    // returns true if execution can proceed
        bool                    HandleHTTPPost();   // should only be called internally on close of writable
#endif

        bool                    GetFileSizeViaSession();
        bool                    ReadFromSessionRange(int64_t nOffset, int64_t nBytes, uint8_t* pDestination, int64_t& nBytesRead);



#ifdef USE_HTTP_CACHE
        HTTPCache               mCache;
#endif
    };
#endif // ENABLE_HTTP
};