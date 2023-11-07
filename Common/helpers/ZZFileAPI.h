//////////////////////////////////////////////////////////////////////////////////////////////////
// ZZFileAPI
// Purpose: An abstraction around local or HTTP files. 
//          ZZFile can open local files for reading or writing
//          cHTTPFile and cHTTPSFile can open remote files served by a web server for reading only.
//
// Usage:  Use the factory function cZZFile::Open to instantiate the appropriate subclass type
// Example: 
//                  shared_ptr<cZZFile> pInFile;
//                  bool bSuccess = cZZFile::Open( filename, ZZFILE_READ, pInFile);
//
// MIT License
// Copyright 2023 Alex Zvenigorodsky
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


typedef std::shared_ptr<class cZZFile> tZZFilePtr;


// Static runtime parameters 
class cZZFile_EnvironmentParameters
{
public:
    cZZFile_EnvironmentParameters(){}

    std::string login_name;
    std::string login_password;
};

extern bool gbSkipCertCheck;

// Interface class
class cZZFile
{
public:
    // helper functions for URLs since filesystem::path doesn't work with URLs
    static bool IsHTTP(const std::string& sURL);
    static std::string Extension(const std::string& sURL);
    static std::string Filename(const std::string& sURL);
    static std::string ParentPath(const std::string& sURL);

    const static int64_t ZZFILE_NO_SEEK = -1;
    const static int64_t ZZFILE_SEEK_END = -2;
    const static bool    ZZFILE_READ = false;
    const static bool    ZZFILE_WRITE = true;

    // Factory Construction
    // returns either a cZZFileLocal, cHTTPFile* or a cHTTPSFile* depending on the url needs
    static bool         Open(const std::string& sURL, bool bWrite, tZZFilePtr& pFile, bool bAppend = false, bool bVerbose = false);
    static bool         Exists(const std::string& sURL, bool bVerbose = false);

    virtual             ~cZZFile() {};

    virtual bool	    Close() = 0;
    virtual bool	    Read(int64_t nOffset, int64_t nBytes, uint8_t* pDestination, int64_t& nBytesRead) = 0;
    virtual bool        Write(int64_t nOffset, int64_t nBytes, uint8_t* pSource, int64_t& nBytesWritten) = 0;

    virtual size_t      Read(uint8_t* pDestination, int64_t nBytes) = 0;   // wrapper to support stream like interface
    virtual size_t      Write(uint8_t* pSource, int64_t nBytes) = 0;       // wrapper to support stream like interface

    virtual uint64_t    GetFileSize() { return mnFileSize; }
    virtual int64_t     GetLastError() { return mnLastError; }

protected:
    cZZFile();          // private constructor.... use cZZFile::Open factory function for construction

    virtual bool	    OpenInternal(std::string sURL, bool bWrite, bool bAppend, bool bVerbose) = 0;
    std::string         msPath;
    bool                mbVerbose;
    int64_t		        mnFileSize;
    int64_t             mnLastError;

    int64_t             mnReadOffset;
    int64_t             mnWriteOffset;
};

extern cZZFile_EnvironmentParameters gZZFileEnv;


//////////////////////////////////////////////////////////////////////////////////////////
class cZZFileLocal : public cZZFile
{
    friend class cZZFile;
public:
    ~cZZFileLocal();

    virtual bool    Close();
    virtual bool    Read(int64_t nOffset, int64_t nBytes, uint8_t* pDestination, int64_t& nBytesRead);
    virtual bool    Write(int64_t nOffset, int64_t nBytes, uint8_t* pSource, int64_t& nBytesWritten);

    virtual size_t  Read(uint8_t* pDestination, int64_t nBytes);
    virtual size_t  Write(uint8_t* pSource, int64_t nBytes);


protected:
    cZZFileLocal(); // private constructor.... use cZZFile::Open factory function for construction

    virtual bool    OpenInternal(std::string sURL, bool bWrite, bool bAppend, bool bVerbose);

protected:
    std::fstream    mFileStream;
    std::mutex      mMutex;
};



#ifdef ENABLE_LIB_CURL

#include "curl/include/curl/curl.h"
#include "helpers/HTTPCache.h"



//////////////////////////////////////////////////////////////////////////////////////////
class cHTTPFile : public cZZFile
{
    friend class cZZFile;
public:
    ~cHTTPFile();

    virtual bool    Close();
    virtual bool    Read(int64_t nOffset, int64_t nBytes, uint8_t* pDestination, int64_t& nBytesRead);
    virtual bool    Write(int64_t, int64_t, uint8_t*, int64_t&);    // not permitted

    virtual size_t  Read(uint8_t* pDestination, int64_t nBytesRead);
    virtual size_t  Write(uint8_t*, int64_t); // not permitted


    static std::string ToURLPath(const std::string& sPath); // converts slashes if necessary 

protected:
    cHTTPFile();    // private constructor.... use cZZFile::Open factory function for construction

    virtual bool	OpenInternal(std::string sURL, bool bWrite, bool bAppend, bool bVerbose);

    static size_t   write_data(char* buffer, size_t size, size_t nitems, void* userp);

    static void     lock_cb(CURL* handle, curl_lock_data data, curl_lock_access access, void* userp);
    static void     unlock_cb(CURL* handle, curl_lock_data data, void* userp);

    std::string     msURL;
    std::string     msHost;

    // Auth
    std::string     msName;
    std::string     msPassword;
    CURLSH* mpCurlShare;
    std::mutex      mCurlMutex;

    HTTPCache       mCache;
};
#endif
