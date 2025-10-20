// MIT License
// Copyright 2025 Alex Zvenigorodsky
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once
#include "ZZFileAPI.h"

namespace ZFile
{
    //////////////////////////////////////////////////////////////////////////////////////////
    // utility class for ZFileRAM to be 

    class ZMemory_PC : public ZMemory
    {
    public:
        ZMemory_PC(size_t nSize);

        virtual ~ZMemory_PC();

        virtual uint8_t*    Data() { return mpAllocation; }
        virtual size_t		Size() const { return mnSize; }

        virtual void        Resize(size_t newsize);

    private:
        uint8_t*            mpAllocation;
        size_t		        mnSize;
        size_t              mnAllocatedSize;
    };

#ifdef ENABLE_HTTP
    struct HTTPRequestTask
    {
        std::string url;
        std::string method;  // "GET", "POST", "HEAD", etc.
        std::vector<uint8_t> data;  // For POST data
        std::string contentType;

        // Range request support
        int64_t rangeStart = -1;
        int64_t rangeEnd = -1;

        // Different callback types for different request types
        std::function<void(bool success, long responseCode, const std::string& response)> uploadCallback;
        std::function<void(bool success, long responseCode, const std::string& response, const std::map<std::string, std::string>& headers)> headCallback;
        std::function<void(bool success, long responseCode, const std::vector<uint8_t>& data)> rangeCallback;

        int retryCount;

        HTTPRequestTask() : retryCount(0), contentType("application/octet-stream") {}
    };


    class ZFileHTTPSession
    {
    public:
        ZFileHTTPSession(const std::string& baseURL, bool verbose = false);
        ~ZFileHTTPSession();

        // Session management
        bool Initialize();
        void Shutdown();
        bool IsActive() const { return mbActive; }

        // File upload methods
        void QueueRequest(const HTTPRequestTask& task);
        void QueueUpload(const std::string& relativePath, const std::vector<uint8_t>& data, std::function<void(bool, long, const std::string&)> callback = nullptr);
        //void UploadImmediate(const std::string& relativePath, const std::vector<uint8_t>& data, std::function<void(bool, long, const std::string&)> callback = nullptr);

        // Batch operations
        //void FlushUploads();  // Process all queued uploads
        void SetBatchSize(int size) { mnBatchSize = size; }
        void SetMaxConcurrent(int concurrent) { mnMaxConcurrent = concurrent; }

        // Connection management
        void SetAuthentication(const std::string& username, const std::string& password);
        void SetTimeout(long timeoutSeconds) { mnTimeoutSeconds = timeoutSeconds; }

        // Add HEAD request support
        void PerformHeadRequest(const std::string& url, std::function<void(bool success, long responseCode, const std::string& response, const std::map<std::string, std::string>& headers)> callback);

        // Add range request support for reads
        void PerformRangeRequest(const std::string& url, int64_t offset, int64_t length, std::function<void(bool success, long responseCode, const std::vector<uint8_t>& data)> callback);

        // Statistics
        struct Stats
        {
            uint64_t totalUploads;
            uint64_t totalBytes;
            uint64_t successfulUploads;
            uint64_t failedUploads;
            uint64_t totalTime; // milliseconds
        };
        const Stats& GetStats() const { return mStats; }
        std::string GetBaseURL() const { return msBaseURL; }

    private:
        // Internal upload processing
        //void ProcessUploadQueue();
        //void ProcessSingleUpload(const HTTPUploadTask& task);
        //bool ExecuteCurlUpload(CURL* curl, const HTTPUploadTask& task, std::string& response);

        void ProcessRequestQueue();  // Rename from ProcessUploadQueue
        void ProcessSingleRequest(const HTTPRequestTask& task);  // Rename and update
        bool ExecuteCurlRequest(CURL* curl, const HTTPRequestTask& task, long& responseCode, std::string& response, std::vector<uint8_t>& binaryData, std::map<std::string, std::string>& headers);




        // CURL management
        CURL* GetAvailableCurlHandle();
        void ReleaseCurlHandle(CURL* curl);
        void InitializeCurlHandle(CURL* curl);
        bool ShouldRecreateHandle(CURL* curl);

        // Static CURL callbacks
        static size_t WriteResponseCallback(char* buffer, size_t size, size_t nitems, void* userp);
        static size_t WriteBinaryDataCallback(char* buffer, size_t size, size_t nitems, void* userp);
        static void CurlLockCallback(CURL* handle, curl_lock_data data, curl_lock_access access, void* userp);
        static void CurlUnlockCallback(CURL* handle, curl_lock_data data, void* userp);

    private:
        std::string msBaseURL;
        std::string msUsername;
        std::string msPassword;
        bool mbVerbose;
        bool mbActive;

        // Connection management
        CURLSH* mpCurlShare;
        std::vector<CURL*> mAvailableHandles;
        std::vector<CURL*> mActiveHandles;
        std::recursive_mutex  mCurlMutex;

        // Upload queue management
//        std::queue<HTTPUploadTask> mUploadQueue;
//        std::mutex mQueueMutex;
        std::thread mRequestThread;
        std::condition_variable mQueueCondition;
        bool mbShutdownRequested;

        std::queue<HTTPRequestTask> mRequestQueue;  // Replace the old upload queue
        std::mutex mRequestQueueMutex;

        // Configuration
        int mnBatchSize;
        int mnMaxConcurrent;
        long mnTimeoutSeconds;

        // Statistics
        Stats mStats;
        std::mutex mStatsMutex;
    };

    class ZFileHTTPSessionManager
    {
    public:
        static ZFileHTTPSessionManager& Instance();

        // Session management functions
        static void CloseHTTPSession(const std::string& baseURL);
        static void CloseAllHTTPSessions();
        static void PrintHTTPSessionStats();

        // Extract base URL from full URL
        static std::string ExtractBaseURL(const std::string& fullURL);


        // Get or create session for base URL
        tZFileHTTPSessionPtr GetOrCreateSession(const std::string& url, bool verbose = false);
        void DecrementSessionRef(const std::string& baseURL);

        // Manual session management
        void CloseSession(const std::string& baseURL);
        void CloseAllSessions();
        void SetSessionTimeout(int timeoutSeconds) { mnSessionTimeoutSeconds = timeoutSeconds; }

        // Statistics
        size_t GetActiveSessionCount() const;
        void PrintSessionStats() const;

    private:
        ZFileHTTPSessionManager();
        ~ZFileHTTPSessionManager();

        // Cleanup expired sessions
        void CleanupExpiredSessions();

        struct SessionInfo
        {
            tZFileHTTPSessionPtr session;
            std::atomic<int> refCount;
            std::chrono::time_point<std::chrono::steady_clock> lastUsed;

            SessionInfo(tZFileHTTPSessionPtr s) : session(s), refCount(1), lastUsed(std::chrono::steady_clock::now()) {}
        };

        mutable std::recursive_mutex mSessionsMutex;
        std::map<std::string, std::shared_ptr<SessionInfo>> mSessions;
        int mnSessionTimeoutSeconds;

        // Cleanup thread
        std::thread mCleanupThread;
        std::mutex mCleanupMutex;
        std::condition_variable mCleanupCondition;
        std::atomic<bool> mbShutdown;
        void CleanupThreadFunction();
    };
#endif
};