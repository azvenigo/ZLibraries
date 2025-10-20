#include "ZZFile_PC.h"
#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <mutex>
#include <regex>
#include <vector>
#include <future>
#include <assert.h>
#include "helpers/StringHelpers.h"
#include "helpers/LoggingHelpers.h"
#include <filesystem>
#include "helpers/aligned_vector.h"
#include <cstdlib>

#ifndef _WIN64
#include <fcntl.h>   // For open, O_RDONLY etc.
#include <errno.h>   // For error handling
#include <sys/stat.h> // For file status
#include <unistd.h>  // For close, read, write, lseek
#else
#endif

using namespace std;
namespace fs = std::filesystem;

namespace ZFile
{

#ifdef ENABLE_HTTP
    bool gbSkipCertCheck = true;


    ZFileHTTPSession::ZFileHTTPSession(const std::string& baseURL, bool verbose)
        : msBaseURL(baseURL), mbVerbose(verbose), mbActive(false), mpCurlShare(nullptr),
        mnBatchSize(10), mnMaxConcurrent(4), mnTimeoutSeconds(60), mbShutdownRequested(false)
    {
        memset(&mStats, 0, sizeof(mStats));

        // Ensure base URL ends with /
        if (!msBaseURL.empty() && msBaseURL.back() != '/') 
        {
            msBaseURL += '/';
        }
    }

    ZFileHTTPSession::~ZFileHTTPSession()
    {
        Shutdown();
    }

    bool ZFileHTTPSession::Initialize()
    {
        if (mbActive) 
        {
            return true;
        }

        // Initialize CURL share
        mpCurlShare = curl_share_init();
        if (!mpCurlShare) 
        {
            std::cerr << "Failed to initialize CURL share\n";
            return false;
        }

        // Configure sharing
        CURLSHcode sharecode;
        sharecode = curl_share_setopt(mpCurlShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
        if (sharecode != CURLSHE_OK) 
        {
            std::cerr << "CURL share setopt COOKIE failed\n";
            curl_share_cleanup(mpCurlShare);
            return false;
        }

        sharecode = curl_share_setopt(mpCurlShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
        if (sharecode != CURLSHE_OK) 
        {
            std::cerr << "CURL share setopt SSL_SESSION failed\n";
            curl_share_cleanup(mpCurlShare);
            return false;
        }

        sharecode = curl_share_setopt(mpCurlShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
        if (sharecode != CURLSHE_OK) 
        {
            std::cerr << "CURL share setopt CONNECT failed\n";
            curl_share_cleanup(mpCurlShare);
            return false;
        }

        // Set locking functions
        curl_share_setopt(mpCurlShare, CURLSHOPT_LOCKFUNC, CurlLockCallback);
        curl_share_setopt(mpCurlShare, CURLSHOPT_UNLOCKFUNC, CurlUnlockCallback);
        curl_share_setopt(mpCurlShare, CURLSHOPT_USERDATA, &mCurlMutex);

        // Pre-create CURL handles for connection pooling
        for (int i = 0; i < mnMaxConcurrent; ++i) 
        {
            CURL* curl = curl_easy_init();
            if (curl) 
            {
                InitializeCurlHandle(curl);
                mAvailableHandles.push_back(curl);
            }
        }

        // Start upload processing thread
        mbShutdownRequested = false;
        mRequestThread = std::thread(&ZFileHTTPSession::ProcessRequestQueue, this);

        mbActive = true;

        if (mbVerbose) 
        {
            std::cout << "ZFileHTTPSession initialized for: " << msBaseURL << std::endl;
        }

        return true;
    }

    void ZFileHTTPSession::Shutdown()
    {
        if (!mbActive) 
        {
            return;
        }

        // Signal shutdown and wake up processing thread
        {
            std::lock_guard<std::mutex> lock(mRequestQueueMutex);
            mbShutdownRequested = true;
        }
        mQueueCondition.notify_all();

        // Wait for upload thread to finish
        if (mRequestThread.joinable()) 
        {
            mRequestThread.join();
        }

        // Clean up CURL handles
        {
            std::lock_guard<std::recursive_mutex> lock(mCurlMutex);
            for (CURL* curl : mAvailableHandles) 
            {
                curl_easy_cleanup(curl);
            }
            for (CURL* curl : mActiveHandles) 
            {
                curl_easy_cleanup(curl);
            }
            mAvailableHandles.clear();
            mActiveHandles.clear();
        }

        // Clean up CURL share
        if (mpCurlShare) {
            curl_share_cleanup(mpCurlShare);
            mpCurlShare = nullptr;
        }

        mbActive = false;

        if (mbVerbose) {
            std::cout << "ZFileHTTPSession shutdown. Stats - Uploads: " << mStats.totalUploads
                << ", Success: " << mStats.successfulUploads
                << ", Failed: " << mStats.failedUploads << std::endl;
        }
    }

    void ZFileHTTPSession::QueueUpload(const std::string& relativePath, const std::vector<uint8_t>& data,
        std::function<void(bool, long, const std::string&)> callback)
    {
        HTTPRequestTask task;
        task.url = msBaseURL + relativePath;
        task.method = "POST";
        task.data = data;
        task.uploadCallback = callback;

        // Use the generic queue
        QueueRequest(task);
    }

/*    void ZFileHTTPSession::UploadImmediate(const std::string& relativePath, const std::vector<uint8_t>& data,
        std::function<void(bool, long, const std::string&)> callback)
    {
        if (!mbActive) 
        {
            if (callback) callback(false, 0, "Session not active");
            return;
        }

        HTTPUploadTask task;
        task.url = msBaseURL + relativePath;
        task.data = data;
        task.callback = callback;

        // Process immediately in current thread
        ProcessSingleUpload(task);
    }*/

    void ZFileHTTPSession::SetAuthentication(const std::string& username, const std::string& password)
    {
        msUsername = username;
        msPassword = password;
    }

    void ZFileHTTPSession::PerformHeadRequest(const std::string& url, std::function<void(bool success, long responseCode, const std::string& response, const std::map<std::string, std::string>& headers)> callback)
    {
        HTTPRequestTask task;
        task.url = url;
        task.method = "HEAD";
        task.headCallback = callback;

        // Queue or process immediately
        QueueRequest(task);
    }

    void ZFileHTTPSession::PerformRangeRequest(const std::string& url, int64_t offset, int64_t length, std::function<void(bool success, long responseCode, const std::vector<uint8_t>& data)> callback)
    {
        HTTPRequestTask task;
        task.url = url;
        task.method = "GET";
        task.rangeStart = offset;
        task.rangeEnd = offset + length - 1;
        task.rangeCallback = callback;

        QueueRequest(task);
    }

/*    void ZFileHTTPSession::QueueUpload(const std::string& relativePath, const std::vector<uint8_t>& data, std::function<void(bool, long, const std::string&)> callback)
    {
        HTTPRequestTask task;
        task.url = msBaseURL + relativePath;
        task.method = "POST";
        task.data = data;
        task.uploadCallback = callback;

        QueueRequest(task);
    }*/

    void ZFileHTTPSession::QueueRequest(const HTTPRequestTask& task)
    {
        if (!mbActive) 
        {
            // Call appropriate callback with error
            if (task.uploadCallback) task.uploadCallback(false, 0, "Session not active");
            if (task.headCallback) task.headCallback(false, 0, "Session not active", {});
            if (task.rangeCallback) task.rangeCallback(false, 0, {});
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mRequestQueueMutex);
            mRequestQueue.push(task);
        }

        mQueueCondition.notify_one();

        if (mbVerbose) 
        {
            std::cout << "Queued " << task.method << " request: " << task.url << std::endl;
        }
    }


    void ZFileHTTPSession::ProcessRequestQueue()  // Renamed from ProcessUploadQueue
    {
        while (!mbShutdownRequested) 
        {
            std::vector<HTTPRequestTask> tasksToProcess;

            // Get batch of tasks to process
            {
                std::unique_lock<std::mutex> lock(mRequestQueueMutex);
                mQueueCondition.wait(lock, [this] 
                    {
                    return mbShutdownRequested || !mRequestQueue.empty();
                    });

                if (mbShutdownRequested && mRequestQueue.empty()) 
                {
                    break;
                }

                // Extract up to mnBatchSize tasks
                int batchCount = 0;
                while (!mRequestQueue.empty() && batchCount < mnBatchSize) 
                {
                    tasksToProcess.push_back(mRequestQueue.front());
                    mRequestQueue.pop();
                    batchCount++;
                }
            }

            // Process tasks concurrently
            if (!tasksToProcess.empty()) 
            {
                std::vector<std::thread> workers;

                for (const auto& task : tasksToProcess) 
                {
                    workers.emplace_back([this, task]() 
                        {
                        ProcessSingleRequest(task);
                        });

                    // Limit concurrent requests
                    if (workers.size() >= mnMaxConcurrent) 
                    {
                        for (auto& worker : workers) 
                        {
                            worker.join();
                        }
                        workers.clear();
                    }
                }

                // Wait for remaining workers
                for (auto& worker : workers) 
                {
                    worker.join();
                }
            }
        }

        if (mbVerbose) 
        {
            std::cout << "Request processing thread shutting down" << std::endl;
        }
    }


    void ZFileHTTPSession::ProcessSingleRequest(const HTTPRequestTask& task)
    {
        auto startTime = std::chrono::high_resolution_clock::now();

        CURL* curl = GetAvailableCurlHandle();
        if (!curl) 
        {
            // Call appropriate error callback
            if (task.uploadCallback) task.uploadCallback(false, 0, "No available CURL handle");
            if (task.headCallback) task.headCallback(false, 0, "No available CURL handle", {});
            if (task.rangeCallback) task.rangeCallback(false, 0, {});
            return;
        }

        std::string response;
        std::vector<uint8_t> binaryData;
        std::map<std::string, std::string> headers;
        long responseCode = 0;
        bool success = ExecuteCurlRequest(curl, task, responseCode, response, binaryData, headers);

        ReleaseCurlHandle(curl);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(mStatsMutex);
            mStats.totalUploads++;  // Should rename to totalRequests
            if (task.method == "POST") 
            {
                mStats.totalBytes += task.data.size();
            }
            mStats.totalTime += duration.count();

            if (success) 
            {
                mStats.successfulUploads++;
            }
            else 
            {
                mStats.failedUploads++;
            }
        }

        if (mbVerbose) 
        {
            std::cout << task.method << " request " << (success ? "succeeded" : "failed") << ": " << task.url << " (" << duration.count() << "ms)" << std::endl;
        }

        // Execute appropriate callback
/*        if (success) 
        {
            // Get response code (we'll store this in ExecuteCurlRequest)
            responseCode = 200; // We'll get the real code from ExecuteCurlRequest
        }*/

        if (task.uploadCallback) 
        {
            task.uploadCallback(success, responseCode, response);
        }
        else if (task.headCallback) 
        {
            task.headCallback(success, responseCode, response, headers);
        }
        else if (task.rangeCallback) 
        {
            task.rangeCallback(success, responseCode, binaryData);
        }
    }

/*    bool ZFileHTTPSession::ExecuteCurlUpload(CURL* curl, const HTTPUploadTask& task, std::string& response)
    {
        // Set URL
        curl_easy_setopt(curl, CURLOPT_URL, task.url.c_str());

        // Set POST data
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, task.data.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)task.data.size());

        // Set content type
        struct curl_slist* headers = nullptr;
        std::string contentTypeHeader = "Content-Type: " + task.contentType;
        headers = curl_slist_append(headers, contentTypeHeader.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Set response capture
        response.clear();
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // Perform the upload
        CURLcode res = curl_easy_perform(curl);

        // Clean up headers
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            response = "CURL error: " + std::string(curl_easy_strerror(res));
            return false;
        }

        // Check HTTP response code
        long responseCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        return (responseCode >= 200 && responseCode < 300);
    }*/

    bool ZFileHTTPSession::ExecuteCurlRequest(CURL* curl, const HTTPRequestTask& task, long& responseCode, std::string& response, std::vector<uint8_t>& binaryData, std::map<std::string, std::string>& headers)
    {
        // retry logic for CURLE_UNSUPPORTED_PROTOCOL
        const int MAX_RETRIES = 2;

        for (int attempt = 0; attempt < MAX_RETRIES; ++attempt)
        {
            if (attempt > 0)
            {
                // Reset the handle completely before retry
                curl_easy_reset(curl);
                InitializeCurlHandle(curl);

                if (mbVerbose) {
                    std::cout << "Retrying request (attempt " << (attempt + 1) << "): " << task.url << std::endl;
                }
            }

            // Set URL
            curl_easy_setopt(curl, CURLOPT_URL, task.url.c_str());

            // Set method
            if (task.method == "POST")
            {
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, task.data.data());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)task.data.size());
            }
            else if (task.method == "HEAD")
            {
                curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
            }
            else if (task.method == "GET")
            {
                curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

                // Set range if specified
                if (task.rangeStart >= 0 && task.rangeEnd >= 0)
                {
                    std::stringstream rangeHeader;
                    rangeHeader << task.rangeStart << "-" << task.rangeEnd;
                    curl_easy_setopt(curl, CURLOPT_RANGE, rangeHeader.str().c_str());
                }
            }

//#define ENABLE_PROXY_DEBUGGING

#ifdef ENABLE_PROXY_DEBUGGING
            curl_easy_setopt(curl, CURLOPT_PROXY, "127.0.0.1:8888");
            curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
#endif

            // Set headers
            struct curl_slist* headerList = nullptr;
            if (task.method == "POST")
            {
                std::string contentTypeHeader = "Content-Type: " + task.contentType;
                headerList = curl_slist_append(headerList, contentTypeHeader.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
            }

            // Set up response capture
            if (task.rangeCallback)
            {
                // Capture binary data
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteBinaryDataCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &binaryData);
            }
            else
            {
                // Capture text response
                response.clear();
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteResponseCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            }

            // Perform the request
            CURLcode res = curl_easy_perform(curl);

            // Clean up headers
            if (headerList)
            {
                curl_slist_free_all(headerList);
                headerList = nullptr;
            }

            if (res != CURLE_OK)
            {
            }

            // Check HTTP response code
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

            if (res == CURLE_OK)
            {

                // For HEAD requests, we might want to capture headers
                if (task.method == "HEAD" && task.headCallback)
                {
                    // Parse Content-Length
                    curl_header* pHeader = nullptr;
                    CURLHcode hRes = curl_easy_header(curl, "Content-Length", 0, CURLH_HEADER, -1, &pHeader);
                    if (hRes == CURLHE_OK && pHeader)
                    {
                        headers["content-length"] = std::string(pHeader->value);
                    }

                    // Parse other useful headers
                    hRes = curl_easy_header(curl, "Content-Type", 0, CURLH_HEADER, -1, &pHeader);
                    if (hRes == CURLHE_OK && pHeader)
                    {
                        headers["content-type"] = std::string(pHeader->value);
                    }

                    hRes = curl_easy_header(curl, "Last-Modified", 0, CURLH_HEADER, -1, &pHeader);
                    if (hRes == CURLHE_OK && pHeader)
                    {
                        headers["last-modified"] = std::string(pHeader->value);
                    }
                }

                return (responseCode >= 200 && responseCode < 300) || (task.method == "GET" && task.rangeStart >= 0 && responseCode == 206); // Accept 206 for range requests
            }
            else if (res == CURLE_UNSUPPORTED_PROTOCOL && attempt < MAX_RETRIES - 1)
            {
                // Retry on CURLE_UNSUPPORTED_PROTOCOL
                if (mbVerbose)
                {
                    std::cout << "CURLE_UNSUPPORTED_PROTOCOL error, will retry: " << task.url << std::endl;
                }
                continue; // Try again
            }
            else
            {
                // Other errors or final attempt failed
                cout << "CURL error:" << std::string(curl_easy_strerror(res)) << " method:" << task.method << "\n";
                response = "CURL error: " + std::string(curl_easy_strerror(res));
                return false;
            }
        }

        response = "CURL error: All retries failed\n";
        cout << "All retries failed\n";
        return false;
    }

    // Add binary data callback
    size_t ZFileHTTPSession::WriteBinaryDataCallback(char* buffer, size_t size, size_t nitems, void* userp)
    {
        std::vector<uint8_t>* data = static_cast<std::vector<uint8_t>*>(userp);
        size_t totalSize = size * nitems;

        size_t oldSize = data->size();
        data->resize(oldSize + totalSize);
        memcpy(data->data() + oldSize, buffer, totalSize);

        return totalSize;
    }

    CURL* ZFileHTTPSession::GetAvailableCurlHandle()
    {
        std::lock_guard<std::recursive_mutex> lock(mCurlMutex);

        if (mAvailableHandles.empty()) {
            // Create new handle if we haven't hit the limit
            if (mActiveHandles.size() + mAvailableHandles.size() < mnMaxConcurrent * 2) {
                CURL* curl = curl_easy_init();
                if (curl) {
                    InitializeCurlHandle(curl);
                    return curl;
                }
            }
            return nullptr; // No handles available
        }

        CURL* curl = mAvailableHandles.back();
        mAvailableHandles.pop_back();
        mActiveHandles.push_back(curl);

        return curl;
    }

    void ZFileHTTPSession::ReleaseCurlHandle(CURL* curl)
    {
        std::lock_guard<std::recursive_mutex> lock(mCurlMutex);

        // Remove from active handles
        auto it = std::find(mActiveHandles.begin(), mActiveHandles.end(), curl);
        if (it != mActiveHandles.end()) {
            mActiveHandles.erase(it);
        }

        // This helps with proxy connection issues after 404s
        if (ShouldRecreateHandle(curl)) 
        {
            curl_easy_cleanup(curl);
            curl = curl_easy_init();
            if (!curl) return; // Handle creation failed
        }
        else 
        {
            // Reset handle for reuse
            curl_easy_reset(curl);
        }
        InitializeCurlHandle(curl);

        // Return to available pool
        mAvailableHandles.push_back(curl);
    }

    bool ZFileHTTPSession::ShouldRecreateHandle(CURL* curl)
    {
        // For debugging proxy issues, recreate handles more aggressively
#ifdef ENABLE_PROXY_DEBUGGING
        return true; // Always recreate when debugging with proxy
#endif

        // In production, you might want more sophisticated logic here
        // For now, recreate occasionally to prevent stale connections
        static thread_local int handleUseCount = 0;
        return (++handleUseCount % 10 == 0); // Recreate every 10th use
    }

    void ZFileHTTPSession::InitializeCurlHandle(CURL* curl)
    {
        // Basic configuration
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
//      curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, mnTimeoutSeconds);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, mbVerbose ? 1L : 0L);

        curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);           // Disable Nagle's algorithm
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 512 * 1024);    // 512KB receive buffer


        // Share configuration
        curl_easy_setopt(curl, CURLOPT_SHARE, mpCurlShare);

        // Response handling
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteResponseCallback);

        // Authentication
        if (!msUsername.empty()) {
            curl_easy_setopt(curl, CURLOPT_USERNAME, msUsername.c_str());
        }
        if (!msPassword.empty()) {
            curl_easy_setopt(curl, CURLOPT_PASSWORD, msPassword.c_str());
        }

        // SSL settings (using your existing pattern)
        if (gbSkipCertCheck) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        }
        else {
            curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
        }
    }

    // Static callback functions
    size_t ZFileHTTPSession::WriteResponseCallback(char* buffer, size_t size, size_t nitems, void* userp)
    {
        std::string* response = static_cast<std::string*>(userp);
        response->append(buffer, size * nitems);
        return size * nitems;
    }

    void ZFileHTTPSession::CurlLockCallback(CURL* handle, curl_lock_data data, curl_lock_access access, void* userp)
    {
        std::mutex* mutex = static_cast<std::mutex*>(userp);
        mutex->lock();
    }

    void ZFileHTTPSession::CurlUnlockCallback(CURL* handle, curl_lock_data data, void* userp)
    {
        std::mutex* mutex = static_cast<std::mutex*>(userp);
        mutex->unlock();
    }


    ZFileHTTPSessionManager& ZFileHTTPSessionManager::Instance()
    {
        static ZFileHTTPSessionManager instance;
        return instance;
    }

    ZFileHTTPSessionManager::ZFileHTTPSessionManager()
        : mnSessionTimeoutSeconds(300), mbShutdown(false) // 5 minute default timeout
    {
        // Start cleanup thread
        mCleanupThread = std::thread(&ZFileHTTPSessionManager::CleanupThreadFunction, this);
    }

    ZFileHTTPSessionManager::~ZFileHTTPSessionManager()
    {
        mbShutdown = true;
        mCleanupCondition.notify_all();
        if (mCleanupThread.joinable()) {
            mCleanupThread.join();
        }
        CloseAllSessions();
    }

    std::string ZFileHTTPSessionManager::ExtractBaseURL(const std::string& fullURL)
    {
        // Extract protocol + host + port from URL
        // Example: "https://server:8080/path/to/file" -> "https://server:8080/"

        std::regex url_regex(R"(^(https?://[^/]+)(?:/.*)?$)");
        std::smatch matches;

        if (std::regex_search(fullURL, matches, url_regex) && matches.size() > 1) 
        {
            std::string baseURL = matches[1];
            if (baseURL.back() != '/') 
            {
                baseURL += '/';
            }
            return baseURL;
        }

        // Fallback - just return the URL up to the third slash
        size_t protocolEnd = fullURL.find("://");
        if (protocolEnd != std::string::npos) {
            size_t hostStart = protocolEnd + 3;
            size_t pathStart = fullURL.find('/', hostStart);
            if (pathStart != std::string::npos) 
            {
                std::string baseURL = fullURL.substr(0, pathStart + 1);
                return baseURL;
            }
        }

        return fullURL; // Fallback to full URL
    }

    // Implementation in ZZFile_PC.cpp
    void ZFileHTTPSessionManager::CloseHTTPSession(const std::string& baseURL)
    {
        ZFileHTTPSessionManager::Instance().CloseSession(baseURL);
    }

    void ZFileHTTPSessionManager::CloseAllHTTPSessions()
    {
        ZFileHTTPSessionManager::Instance().CloseAllSessions();
    }

    void ZFileHTTPSessionManager::PrintHTTPSessionStats()
    {
        ZFileHTTPSessionManager::Instance().PrintSessionStats();
    }

    tZFileHTTPSessionPtr ZFileHTTPSessionManager::GetOrCreateSession(const std::string& url, bool verbose)
    {
        std::string baseURL = ExtractBaseURL(url);

        std::lock_guard<std::recursive_mutex> lock(mSessionsMutex);

        auto it = mSessions.find(baseURL);
        if (it != mSessions.end()) 
        {
            // Session exists, increment ref count and update last used time
            it->second->refCount++;
            it->second->lastUsed = std::chrono::steady_clock::now();

            if (verbose) 
            {
                std::cout << "Reusing session for: " << baseURL << " (refs: " << it->second->refCount.load() << ")" << std::endl;
            }

            return it->second->session;
        }

        // Create new session
        auto session = std::make_shared<ZFileHTTPSession>(baseURL, verbose);
        if (!session->Initialize()) 
        {
            std::cerr << "Failed to initialize session for: " << baseURL << std::endl;
            return nullptr;
        }

        auto sessionInfo = std::make_shared<SessionInfo>(session);
        mSessions[baseURL] = sessionInfo;

        if (verbose) 
        {
            std::cout << "Created new session for: " << baseURL << std::endl;
        }

        return session;
    }

    void ZFileHTTPSessionManager::DecrementSessionRef(const std::string& baseURL)
    {
        std::lock_guard<std::recursive_mutex> lock(mSessionsMutex);

        auto it = mSessions.find(baseURL);
        if (it != mSessions.end())
        {
            int currentRefs = --it->second->refCount;
            it->second->lastUsed = std::chrono::steady_clock::now();

//            std::cout << "Decremented session refs for: " << baseURL << " (now: " << currentRefs << ")" << std::endl;

            // Note: Don't immediately close sessions with 0 refs
            // Let the cleanup thread handle timeout-based removal
        }
    }

    void ZFileHTTPSessionManager::CloseSession(const std::string& baseURL)
    {
        std::lock_guard<std::recursive_mutex> lock(mSessionsMutex);

        auto it = mSessions.find(baseURL);
        if (it != mSessions.end()) 
        {
            it->second->session->Shutdown();
            mSessions.erase(it);
        }
    }

    void ZFileHTTPSessionManager::CloseAllSessions()
    {
        std::lock_guard<std::recursive_mutex> lock(mSessionsMutex);

        for (auto& pair : mSessions) 
        {
            pair.second->session->Shutdown();
        }
        mSessions.clear();
    }

    void ZFileHTTPSessionManager::CleanupExpiredSessions()
    {
        auto now = std::chrono::steady_clock::now();
        std::vector<std::string> sessionsToRemove;

        {
            std::lock_guard<std::recursive_mutex> lock(mSessionsMutex);

            for (auto& pair : mSessions) 
            {
                const std::string& baseURL = pair.first;
                auto& sessionInfo = pair.second;

                // Check if session has expired (no references and timeout passed)
                auto timeSinceLastUse = std::chrono::duration_cast<std::chrono::seconds>(now - sessionInfo->lastUsed);

                if (sessionInfo->refCount == 0 && timeSinceLastUse.count() > mnSessionTimeoutSeconds) 
                {
                    sessionsToRemove.push_back(baseURL);
                }
            }
        }

        // Remove expired sessions (outside the lock to avoid deadlock)
        for (const auto& baseURL : sessionsToRemove) 
        {
            CloseSession(baseURL);
        }

        if (!sessionsToRemove.empty()) 
        {
            std::cout << "Cleaned up " << sessionsToRemove.size() << " expired HTTP sessions" << std::endl;
        }
    }

    void ZFileHTTPSessionManager::CleanupThreadFunction()
    {
        while (!mbShutdown)
        {
            // Wait for 60 seconds OR until shutdown is signaled
            std::unique_lock<std::mutex> lock(mCleanupMutex);
            mCleanupCondition.wait_for(lock, std::chrono::seconds(60), [this] 
                {
                return mbShutdown.load();
                });

            if (!mbShutdown)
            {
                CleanupExpiredSessions();
            }
        }
    }

    size_t ZFileHTTPSessionManager::GetActiveSessionCount() const
    {
        std::lock_guard<std::recursive_mutex> lock(mSessionsMutex);
        return mSessions.size();
    }

    void ZFileHTTPSessionManager::PrintSessionStats() const
    {
        std::lock_guard<std::recursive_mutex> lock(mSessionsMutex);

        std::cout << "Active HTTP Sessions: " << mSessions.size() << std::endl;
        for (const auto& pair : mSessions) 
        {
            const auto& sessionInfo = pair.second;
            std::cout << "  " << pair.first << " - refs: " << sessionInfo->refCount.load() << ", stats: " << sessionInfo->session->GetStats().successfulUploads << " uploads" << std::endl;
        }
    }






#endif


    inline int64_t GetUSSinceEpoch()
    {
        return std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1);
    }


    string Extension(const std::string& sURL)
    {
        size_t nPos = sURL.find_last_of('.');
        if (nPos != string::npos)
            return sURL.substr(nPos);

        return "";
    }

    string Filename(const std::string& sURL)
    {
        size_t nPos = sURL.find_last_of("/\\");
        if (nPos != string::npos)
            return sURL.substr(nPos);

        return "";
    }

    string ParentPath(const std::string& sURL)
    {
        size_t nPos = sURL.find_last_of("/\\");
        if (nPos != string::npos)
            return sURL.substr(0, nPos);

        return "";
    }


#ifdef ENABLE_HTTP
    bool ZFileBase::IsHTTP(const std::string& sURL)
    {
        return SH::StartsWith(sURL, "http", false);
    }
#endif

    // Factory
    bool ZFileBase::Open(const string& sURL, tZFilePtr& pFile, uint32_t flags, bool bVerbose)
    {
#ifdef ENABLE_HTTP
        if (SH::StartsWith(sURL, "http") || SH::StartsWith(sURL, "sftp"))
        {
            pFile.reset(new ZFileHTTP());
        }
        else
#endif
        {
            pFile.reset(new ZFileLocal());
        }

        return pFile->OpenInternal(sURL, flags, bVerbose);   // call protected virtualized Open
    }


    string ZFileLocal::Canonical(string sPath)
    {
#ifdef _WIN64
        // Don't mix slashes
        for (size_t pos = 0; (pos = sPath.find('/', pos)) != std::string::npos; ++pos)
            sPath.replace(pos, 1, "\\");


        if (sPath.length() > 2 && sPath.substr(0, 2) == "\\\\")
            return sPath;

        std::filesystem::path p(sPath);
        sPath = std::filesystem::absolute(p).string();

        if (sPath.substr(0, 4) != "\\\\?\\")
        {
            return "\\\\?\\" + sPath;
        }

        return sPath;
#else
        // forward slashes for everyone else
        for (size_t pos = 0; (pos = sPath.find('\\', pos)) != std::string::npos; ++pos)
            sPath.replace(pos, 1, "/");

        return std::filesystem::weakly_canonical(sPath).string();
#endif
    }


    bool ZFileBase::Exists(const std::string& sURL, bool bVerbose)
    {
#ifdef ENABLE_HTTP
        if (sURL.substr(0, 4) == "http" || sURL.substr(0, 4) == "sftp")
        {
            ZFileHTTP httpFile;
            bool bOpened = httpFile.OpenInternal(sURL, kRead, bVerbose);    // returns true if it is able to connect and retrieve headers via HEAD request
            if (!bOpened && httpFile.GetLastError() != 404)   // not found errors should be silent
            {
                std::cout << "HTTP error code: " << httpFile.GetLastError() << "\n";
            }
            return bOpened;
        }
#endif
        // local file
        string sCanonical = ZFileLocal::Canonical(sURL);
        return fs::exists(sCanonical);
    }

    bool ZFileBase::IsDirectory(const std::string& sURL)
    {
#ifdef ENABLE_HTTP
        if (sURL.substr(0, 4) == "http" || sURL.substr(0, 4) == "sftp")
        {
            return sURL[sURL.length()-1] == '/';    // assuming for http paths that folders and in trailing slash. (There is no URL standard)
        }
#endif
        // local file
        string sCanonical = ZFileLocal::Canonical(sURL);
        return fs::is_directory(sCanonical);
    }

    bool ZFileBase::EnsureFolderForPathExists(std::filesystem::path outputPath)
    {
        if (outputPath.empty())
            return true;

        return std::filesystem::create_directories(ZFile::ZFileLocal::Canonical(outputPath.string()));
    }



    ZFileBase::ZFileBase() : mOpenFlags(kRead), mbVerbose(false), mnFileSize(0), mnLastError(0), mnReadOffset(0), mnWriteOffset(0)
    {
    }


    ZFileLocal::ZFileLocal() : ZFileBase()
    {
        mhFile = INVALID_HANDLE_VALUE;
#ifdef _WIN64
        mSectorSize = 4 * 1024;
#endif
    }

    ZFileLocal::~ZFileLocal()
    {
        ZFileLocal::Close();
    }

    bool ZFileLocal::OpenInternal(string sURL, uint32_t flags, bool bVerbose)
    {
        mnLastError = kZZFileError_None;
        mOpenFlags = flags;
        mbVerbose = bVerbose;
        mPath = ZFileLocal::Canonical(sURL);


        bool bFileExists = Exists(mPath.string(), bVerbose);
        if (!bFileExists && !IsSet(kWrite))    // trying to open a file to read that doesn't exist
            return false;

#ifdef _WIN64
        DWORD desiredAccess = GENERIC_READ;
        DWORD creationDisposition = OPEN_EXISTING;
        DWORD shareMode = FILE_SHARE_READ;
        DWORD createFlags = FILE_ATTRIBUTE_NORMAL/*| FILE_FLAG_SEQUENTIAL_SCAN*/;

        if (IsSet(kWrite))
        {
            desiredAccess |= GENERIC_WRITE;

            if (bFileExists)
            {
                if (IsSet(kTrunc))
                    creationDisposition = TRUNCATE_EXISTING;
                else
                    creationDisposition = OPEN_EXISTING;
            }
            else
                creationDisposition = CREATE_NEW;
        }
        else if (IsSet(kUnbuffered))
        {
            createFlags = FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN;

            std::filesystem::path rootPath = mPath.root_path();

            DWORD sectorSize = 0;
            if (GetDiskFreeSpace(rootPath.string().c_str(), NULL, &sectorSize, NULL, NULL))
            {
                mSectorSize = sectorSize;
            }
            else
            {
                if (bVerbose)
                    cout << "NOTE: Could not get sector size for:" << mPath << "\n";
            }
        }

        mhFile = CreateFile(mPath.string().c_str(), desiredAccess, shareMode, nullptr, creationDisposition, createFlags, 0);
        if (mhFile == INVALID_HANDLE_VALUE)
        {
            cerr << "ERROR: Could not open file:" << mPath << "\n";
            cout << "CreateFile() called with desiredAccess:" << std::hex << desiredAccess << " shareMode:" << shareMode << " creationDisposition:" << creationDisposition << " createFlags: " << createFlags << " returned with:" << GetLastError() << std::dec << "\n";
            return false;
        }
#else

        if (IsSet(kWrite))
        {
            if (!bFileExists || IsSet(kTrunc))   // file exists, open truncate
            {
                mhFile = open(mPath.c_str(), O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                mnWriteOffset = 0;
            }
            else
            {
                mhFile = open(mPath.c_str(), O_RDWR);
                mnWriteOffset = fs::file_size(mPath);
            }
        }
        else
        {
            mhFile = open(mPath.c_str(), O_RDONLY);
        }

        if (mhFile == DEFAULT_HANDLE)
        {
            mnLastError = errno;
            cerr << "Failed to open file:" << mPath.c_str() << "! Reason: " << strerror(errno) << "\n";
            return false;
        }

#endif

        mnFileSize = fs::file_size(mPath);

        if (IsSet(kWrite))
            mnWriteOffset = mnFileSize;

        return true;
    }


    bool ZFileLocal::FreeSpace(const std::string& sPath, int64_t& nOutBytes, bool bVerbose)
    {
        fs::path checkPath(sPath);
        checkPath = checkPath.parent_path();

        fs::space_info info = fs::space(checkPath);
        nOutBytes = (int64_t)info.free;

        return true;
    }


    bool ZFileLocal::Close()
    {
#ifdef _WIN64
        CloseHandle(mhFile);
#else
        close(mhFile);
#endif
        mhFile = INVALID_HANDLE_VALUE;
        mnLastError = kZZFileError_None;

        return true;
    }


    bool ZFileLocal::Read(int64_t nOffset, int64_t nBytes, uint8_t* pDestination, int64_t& nBytesRead)
    {
        std::unique_lock<mutex> lock(mMutex);

        mnLastError = kZZFileError_None;
        nBytesRead = 0;
        SeekRead(nOffset);

#ifdef _WIN64
        DWORD nRead;

        if (IsSet(kUnbuffered) && mSectorSize > 0)
        {
            if ((((uint64_t)pDestination) % mSectorSize) != 0 ||
                (nOffset % mSectorSize) != 0)
            {
                assert(false);
                cerr << "Unbuffered reads have strict requirements. Dest buffer, offset and bytes read must be aligned to source drive's sector size. Currently:" << mSectorSize << "\n";
                return false;
            }

            if (nBytes < mSectorSize)
                nBytes = ((nBytes + mSectorSize - 1) / mSectorSize) * mSectorSize;
        }

        if (!ReadFile(mhFile, pDestination, (DWORD)nBytes, &nRead, nullptr))
        {
            mnLastError = GetLastError();
            cerr << "Failed to read " << nBytes << " bytes reason:" << mnLastError << "\n";
            return false;
        }
        nBytesRead = nRead;
#else

        int64_t nRead = read(mhFile, pDestination, nBytes);
        if (nRead < 0)
        {
            mnLastError = errno;
            cerr << "Failed to read " << nBytes << " bytes reason:" << strerror(errno) << "\n";
            return false;
        }
        nBytesRead = nRead;
#endif
        mnReadOffset = nOffset + nRead;
        return true;
    }

    size_t ZFileLocal::Read(uint8_t* pDestination, int64_t nBytes)
    {
        int64_t nBytesRead = 0;
        if (!Read(mnReadOffset, nBytes, pDestination, nBytesRead))
            return 0;

        return nBytesRead;
    }

    size_t ZFileLocal::Write(uint8_t* pSource, int64_t nBytes)
    {
        int64_t nBytesWritten = 0;
        if (!Write(mnWriteOffset, nBytes, pSource, nBytesWritten))
            return 0;

        return nBytesWritten;
    }


    bool ZFileLocal::Write(int64_t nOffset, int64_t nBytes, uint8_t* pSource, int64_t& nBytesWritten)
    {
        std::unique_lock<mutex> lock(mMutex);
        mnLastError = kZZFileError_None;

        if (nOffset == ZZFILE_SEEK_END)
        {
            SeekWrite(mnFileSize);
        }
        else
            SeekWrite(nOffset);

#ifdef _WIN64

        const size_t kWriteSize = 16 * 1024 * 1024;;
        int64_t nBytesLeft = nBytes;
        while (nBytesLeft > 0)
        {
            DWORD dwordWriteSize = kWriteSize;
            if (nBytesLeft < kWriteSize)
                dwordWriteSize = (DWORD)nBytesLeft;

            DWORD nWritten;
            BOOL bRet = WriteFile(mhFile, pSource, dwordWriteSize, &nWritten, nullptr);
            assert(nWritten == dwordWriteSize);
            if (bRet == FALSE || nWritten != dwordWriteSize)
            {
                mnLastError = GetLastError();
                cerr << "Failed to write:" << nBytes << " bytes to offset:" << nOffset << " Reason: " << mnLastError << "\n";
                return false;
            }
            mnWriteOffset = nOffset + nWritten;
            nBytesLeft -= nWritten;
            pSource += nWritten;
        }
#else

        //mFileStream.write((char*)pSource, nBytes);
        ssize_t written = write(mhFile, pSource, nBytes);
        if (written < 0)
        {
            mnLastError = errno;
            cerr << "Failed to write:" << nBytes << " bytes! Reason: " << strerror(errno) << "\n";
            return false;
        }
        mnWriteOffset = nOffset + written;
#endif
        nBytesWritten = nBytes;

        // If we're writing past the end of the current file size we need to know what the new file size is
        if (mnWriteOffset > mnFileSize)
        {
            mnFileSize = mnWriteOffset;
        }

        return true;
    }

    void ZFileLocal::SeekRead(int64_t offset)
    {
        if (offset < 0 || offset > mnFileSize)
        {
            assert(false);
            mnLastError = kZZFileError_IllegalSeek;
            return;
        }

#ifdef _WIN64
        LARGE_INTEGER bigInt;
        bigInt.QuadPart = offset;
        if (!SetFilePointerEx(mhFile, bigInt, nullptr, FILE_BEGIN))
        {
            mnLastError = GetLastError();
            cerr << "Failed to seek to:" << offset << " error:" << mnLastError << "\n";
            return;
        }
#else
        //mFileStream.seekg(offset);

        if (lseek(mhFile, offset, SEEK_SET) == -1)
        {
            mnLastError = errno;
            cerr << "Failed to seek to " << offset << "reason:" << strerror(errno) << "\n";
            return;
        }
#endif
        mnReadOffset = offset;
    }

    void ZFileLocal::SeekWrite(int64_t offset)
    {
#ifdef _WIN64
        LARGE_INTEGER bigInt;
        bigInt.QuadPart = offset;
        if (!SetFilePointerEx(mhFile, bigInt, nullptr, FILE_BEGIN))
        {
            mnLastError = GetLastError();
            cerr << "Failed to seek to:" << offset << " error:" << mnLastError << "\n";
            return;
        }
#else
        if (lseek(mhFile, offset, SEEK_SET) == -1)
        {
            mnLastError = errno;
            cerr << "Failed to seek to " << offset << "reason:" << strerror(errno) << "\n";
            return;
        }
#endif
        mnWriteOffset = offset;
    }


    ZMemory* ZMemory::Create(size_t nSize)
    {
        return new ZMemory_PC(nSize);
    }

    size_t kAllocUnit = 16 * 1024;
    ZMemory_PC::ZMemory_PC(size_t nSize)
    {
        mnSize = nSize;
        mnAllocatedSize = RoundToPage(nSize);
        mpAllocation = new uint8_t[mnAllocatedSize];

    }

    ZMemory_PC::~ZMemory_PC()
    {
        delete[] mpAllocation;
    }

    void ZMemory_PC::Resize(size_t newsize)
    {
        size_t newAllocSize = RoundToPage(newsize);
        if (mnAllocatedSize != newAllocSize)
        {
            assert(newAllocSize > mnAllocatedSize);
            uint8_t* newAlloc = new uint8_t[newAllocSize];

            memcpy(newAlloc, mpAllocation, mnAllocatedSize);
            delete[] mpAllocation;
            mpAllocation = newAlloc;
            mnAllocatedSize = newAllocSize;
        }

        mnSize = newsize;
    }




    uint64_t ZFileRAM::GetFileSize()
    {
        if (!mpBuffer)
            return 0;

        return mpBuffer->Size();
    }


    ZFileRAM::ZFileRAM(size_t initialSize)
    {
        mnReadOffset = 0;
        mnWriteOffset = 0;
        if (initialSize == 0)
            mpBuffer.reset(ZMemory::Create(1));
        else
            mpBuffer.reset(ZMemory::Create(initialSize));
    }

    bool ZFileRAM::From(const tZFilePtr fromFile)
    {
        assert(fromFile);

        mnReadOffset = 0;
        mnWriteOffset = 0;
        mpBuffer.reset(ZMemory::Create(fromFile->GetFileSize()));

        int64_t nBytesRead = 0;
        bool bSuccess = fromFile->Read((int64_t)0, (int64_t)fromFile->GetFileSize(), mpBuffer->Data(), nBytesRead);
        return (bSuccess && nBytesRead == fromFile->GetFileSize());
    }


    bool ZFileRAM::Close()
    {
        std::unique_lock<mutex> lock(mMutex);
        mpBuffer = nullptr;
        return true;
    }

    bool ZFileRAM::Read(int64_t nOffset, int64_t nBytes, uint8_t* pDestination, int64_t& nBytesRead)
    {
        if (nOffset < 0 || nOffset + nBytes >(int64_t)mpBuffer->Size())
        {
            cerr << "Attempting to read offset:" << nOffset << " bytes:" << nBytes << " outside of buffer size:" << mpBuffer->Size() << "\n";
            return false;
        }

        memcpy(pDestination, mpBuffer->Data() + nOffset, nBytes);
        nBytesRead = nBytes;
        return true;
    }

    bool ZFileRAM::Write(int64_t nOffset, int64_t nBytes, uint8_t* pSource, int64_t& nBytesWritten)
    {
        if (nOffset + nBytes > (int64_t)mpBuffer->Size())
        {
            mpBuffer->Resize(nOffset + nBytes);
        }

        memcpy(mpBuffer->Data() + nOffset, pSource, nBytes);
        nBytesWritten = nBytes;
        mnWriteOffset = nOffset+nBytes;
        return true;
    }

    size_t ZFileRAM::Read(uint8_t* pDestination, int64_t nBytes)
    {
        if (mnReadOffset + nBytes > (int64_t)mpBuffer->Size())
        {
            cerr << "Attempting to read offset:" << mnReadOffset << " bytes:" << nBytes << " outside of buffer size:" << mpBuffer->Size() << "\n";
            return false;
        }

        memcpy(pDestination, mpBuffer->Data() + mnReadOffset, nBytes);
        mnReadOffset += nBytes;

        return nBytes;
    }

    size_t ZFileRAM::Write(uint8_t* pSource, int64_t nBytes)
    {
        if (mnWriteOffset + nBytes > (int64_t)mpBuffer->Size())
        {
            mpBuffer->Resize(mnWriteOffset + nBytes);
        }

        memcpy(mpBuffer->Data() + mnWriteOffset, pSource, nBytes);
        mnWriteOffset += nBytes;

        return nBytes;
    }

    void ZFileRAM::SeekRead(int64_t offset)
    {
        if (offset < 0 || offset > mnFileSize)
        {
            assert(false);
            mnLastError = kZZFileError_IllegalSeek;
            return;
        }

        mnReadOffset = offset;
    }

    void ZFileRAM::SeekWrite(int64_t offset)
    {
        mnWriteOffset = offset;
    }


    uint8_t* ZFileRAM::GetBuffer()
    {
        assert(mpBuffer);
        if (!mpBuffer)
            return nullptr;

        return mpBuffer->Data();
    }

#ifdef ENABLE_MMIO

    ZFileMMIO::ZFileMMIO() : mpBuffer(nullptr)
    {
    }

    ZFileMMIO::~ZFileMMIO()
    {
        Close();
    }


    bool ZFileMMIO::Close()
    {
        ro_mmap.unmap();
        return true;
    }

    bool ZFileMMIO::Read(int64_t nOffset, int64_t nBytes, uint8_t* pDestination, int64_t& nBytesRead)
    {
        if (nOffset < 0 || nOffset > mnFileSize)
        {
            assert(false);
            mnLastError = kZZFileError_IllegalSeek;
            return false;
        }

        if (nOffset + nBytes > mnFileSize)
        {
            mnLastError = kZZFileError_OutOfBounds;
            return false;
        }

        memcpy(pDestination, mpBuffer + nOffset, nBytes);
        mnReadOffset = nOffset + nBytes;
        nBytesRead = nBytes;

        return true;
    }

    bool ZFileMMIO::Write(int64_t nOffset, int64_t nBytes, uint8_t* pSource, int64_t& nBytesWritten)
    {
        // TBD
        assert(false);
        return false;
    }

    size_t ZFileMMIO::Read(uint8_t* pDestination, int64_t nBytes)
    {
        int64_t nBytesRead = 0;
        if (!Read(mnReadOffset, nBytes, pDestination, nBytesRead))
            return 0;

        return nBytesRead;
    }

    size_t ZFileMMIO::Write(uint8_t* pSource, int64_t nBytes)
    {
        // TBD
        assert(false);
        return false;
    }

    void ZFileMMIO::SeekRead(int64_t offset)
    {
        if (offset < 0 || offset > mnFileSize)
        {
            assert(false);
            mnLastError = kZZFileError_IllegalSeek;
            return;
        }

        mnReadOffset = offset;
    }

    void ZFileMMIO::SeekWrite(int64_t offset)
    {
        assert(false);
    }

    uint8_t* ZFileMMIO::GetBuffer()
    {
        assert(mpBuffer);
        return mpBuffer;
    }

    bool ZFileMMIO::OpenInternal(std::string sURL, uint32_t flags, bool bVerbose)
    {
        std::error_code error;
        mOpenFlags = flags;
        mPath = ZFileLocal::Canonical(sURL);
        assert(IsSet(kWrite));  // writing not supported

        ro_mmap.map(mPath.string(), error);
        if (error)
        {
            cerr << "ERROR: Could not open source:" << mPath << "\n";
            return false;
        }

        mnFileSize = std::filesystem::file_size(mPath);
        mpBuffer = (uint8_t*)ro_mmap.data();

        return true;
    }

#endif // ENABLE_MMIO





#ifdef ENABLE_HTTP

    // Tracking stats
    atomic<int64_t> gnTotalHTTPBytesRequested = 0;
    atomic<int64_t> gnTotalRequestsIssued = 0;


    struct HTTPFileResponse
    {
        HTTPFileResponse() : pDest(nullptr), nBytesWritten(0) {}

        uint8_t* pDest;
        size_t nBytesWritten;
    };



    ZFileHTTP::ZFileHTTP() : ZFileBase()
    {
        mpCurlShare = nullptr;
        mpFileRAM = nullptr;
        mpSession = nullptr;
        mbUseAsyncPostOnClose = false;
    }

    ZFileHTTP::~ZFileHTTP()
    {
        ZFileHTTP::Close();
        delete mpFileRAM;

        // Decrement session reference count
        if (mpSession && !msSessionBaseURL.empty()) 
        {
            auto& manager = ZFileHTTPSessionManager::Instance();
            manager.DecrementSessionRef(msSessionBaseURL);  // <-- Need to implement this method        
        }
    }

#ifdef ENABLE_CURL
    void ZFileHTTP::lock_cb(CURL* /*handle*/, curl_lock_data /*data*/, curl_lock_access /*access*/, void* userp)
    {
        ZFileHTTP* pFile = (ZFileHTTP*)userp;
        pFile->mCurlMutex.lock();

    }

    void ZFileHTTP::unlock_cb(CURL* /*handle*/, curl_lock_data /*data*/, void* userp)
    {
        ZFileHTTP* pFile = (ZFileHTTP*)userp;
        pFile->mCurlMutex.unlock();
    }

    bool ZFileHTTP::GetURLAuth(string sURL, string& sName, string& sPassword)
    {
        std::regex url_regex(R"((http|https)://([^:@]+):([^:@]+)@([^/:]+))");
        std::smatch matches;

        if (std::regex_search(sURL, matches, url_regex) && matches.size() > 3)
        {
            sName = matches[2];  // username
            sPassword = matches[3];  // password

            return true;
        }

        return false;
    }

    bool ZFileHTTP::OpenInternal(string sURL, uint32_t flags, bool bVerbose)       // todo maybe someday use real URI class
    {
        mnLastError = kZZFileError_None;
        mOpenFlags = flags;
        mbVerbose = bVerbose;
        msURL = sURL;

        if (GetURLAuth(msURL, msName, msPassword))
        {
            if (bVerbose)
                std::cout << "Credentials parsed from url\n";
        }

        auto& manager = ZFileHTTPSessionManager::Instance();
        mpSession = manager.GetOrCreateSession(msURL, bVerbose);

        if (mpSession)
        {
            msSessionBaseURL = ZFileHTTPSessionManager::ExtractBaseURL(msURL); // Store for cleanup
        }


        if (IsSet(kWrite)) 
        {
            // For write operations, automatically get/create session

            if (!mpFileRAM) 
            {
                mpFileRAM = new ZFileRAM();
            }
            return true;
        }

        if (mbVerbose)
            cout << "Opening HTTP file for read: " << sURL << "\n";

        // Get file size via HEAD request using session
        if (!GetFileSizeViaSession()) 
        {
            //std::cerr << "Failed to get file size for: " << sURL << std::endl;
            return false;
        }

        return true;

/*        mpCurlShare = curl_share_init();

        CURLSHcode sharecode = curl_share_setopt(mpCurlShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
        if (sharecode != 0)
        {
            std::cerr << "CURL Fail: curl_share_setopt(pShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE)\n";
            return false;
        }
        sharecode = curl_share_setopt(mpCurlShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
        if (sharecode != 0)
        {
            std::cerr << "CURL Fail: curl_share_setopt(pShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION )\n";
            return false;
        }
        sharecode = curl_share_setopt(mpCurlShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
        if (sharecode != 0)
        {
            std::cerr << "CURL Fail: curl_share_setopt(pShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT  )\n";
            return false;
        }

        sharecode = curl_share_setopt(mpCurlShare, CURLSHOPT_LOCKFUNC, lock_cb);
        if (sharecode != 0)
        {
            std::cerr << "CURL Fail: curl_share_setopt(pShare, CURLSHOPT_SHARE, CURLSHOPT_LOCKFUNC  )\n";
            return false;
        }

        sharecode = curl_share_setopt(mpCurlShare, CURLSHOPT_UNLOCKFUNC, unlock_cb);
        if (sharecode != 0)
        {
            std::cerr << "CURL Fail: curl_share_setopt(pShare, CURLSHOPT_SHARE, CURLSHOPT_UNLOCKFUNC  )\n";
            return false;
        }

        sharecode = curl_share_setopt(mpCurlShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
        if (sharecode != 0)
        {
            std::cerr << "CURL Fail: curl_share_setopt(pShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT  )\n";
            return false;
        }
        sharecode = curl_share_setopt(mpCurlShare, CURLSHOPT_USERDATA, this);
        if (sharecode != 0)
        {
            std::cerr << "CURL Fail: curl_share_setopt(pShare, CURLSHOPT_USERDATA, this  )\n";
            return false;
        }

        CURL* pCurl = nullptr;
        curl_header* pHeader = nullptr;

        bool bRetry = true;
        while (bRetry)
        {

            pCurl = curl_easy_init();
            if (!pCurl)
            {
                std::cerr << "Failed to create curl instance!\n";
                return false;
            }

            CURLcode res;

            curl_easy_setopt(pCurl, CURLOPT_URL, msURL.c_str());
            curl_easy_setopt(pCurl, CURLOPT_FOLLOWLOCATION, 1);
            curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, write_data);
            curl_easy_setopt(pCurl, CURLOPT_SHARE, mpCurlShare);
            curl_easy_setopt(pCurl, CURLOPT_VERBOSE, (int)mbVerbose);
            //    curl_easy_setopt(pCurl, CURLOPT_FAILONERROR, 1);
            curl_easy_setopt(pCurl, CURLOPT_NOBODY, 1);     // "HEAD" request only
            curl_easy_setopt(pCurl, CURLOPT_TCP_KEEPALIVE, 1L);
            if (!msName.empty())
                curl_easy_setopt(pCurl, CURLOPT_USERNAME, msName.c_str());
            if (!msPassword.empty())
                curl_easy_setopt(pCurl, CURLOPT_PASSWORD, msPassword.c_str());

            if (gbSkipCertCheck)
            {
                curl_easy_setopt(pCurl, CURLOPT_SSL_VERIFYPEER, 0);
            }
            else
            {
                curl_easy_setopt(pCurl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
            }


            res = curl_easy_perform(pCurl);

            if (res != CURLE_OK)
            {
                if (!HandleHTTPError(res, "curl GET", bRetry))
                {
                    std::cerr << "curl error: failed for url:" << msURL << " response: " << curl_easy_strerror(res) << "\n";
                    curl_easy_cleanup(pCurl);
                    return false;
                }

                std::cout << "ZFileHTTP::Read() retryable error:" << res << "\n";
                continue;
            }

            long code = 0;
            curl_easy_getinfo(pCurl, CURLINFO_RESPONSE_CODE, &code);
            if (code >= 400)
            {
                mnLastError = code;

                if (bVerbose)
                    std::cout << "server error reply for url:" << msURL << " http code:" << code << "\n";

                curl_easy_cleanup(pCurl);
                return false;
            }

            bRetry = false;
        }

        CURLHcode hRes = curl_easy_header(pCurl, "Content-Length", 0, CURLH_HEADER, -1, &pHeader);
        if (hRes != CURLHE_OK)
        {
            std::cerr << "curl to Content-Length for url:" << msURL << " response: " << hRes << "\n";
            curl_easy_cleanup(pCurl);
            return false;
        }

        mnFileSize = (uint64_t)strtoull(pHeader->value, nullptr, 10);


        if (bVerbose)
            cout << "Opened HTTP server_host:\"" << msHost << "\"\n server_path:\"" << mPath << "\"\n";

        curl_easy_cleanup(pCurl);
        return true;*/
    }

    // Tracking stats
    //atomic<int64_t> gnTotalHTTPBytesRequested = 0;
    //atomic<int64_t> gnTotalRequestsIssued = 0;

    bool ZFileHTTP::Close()
    {
        bool success = true;

        // If we were writing, decide how to upload
        if (mOpenFlags == eOpenFlags::kWrite && mpFileRAM)
        {
            int64_t dataSize = mpFileRAM->GetFileSize();
            if (dataSize > 0)
            {

                if (mpSession && mbUseAsyncPostOnClose)
                {
                    // Use session for upload
                    std::vector<uint8_t> data(dataSize);
                    int64_t bytesRead = 0;

                    if (mpFileRAM->Read(0, dataSize, data.data(), bytesRead) && bytesRead == dataSize)
                    {
                        // Extract relative path from URL
                        std::string relativePath = msURL;
                        for (auto& c : relativePath)
                        {
                            if (c == '\\')
                                c = '/';
                        }

                        if (mpSession)
                        {
                            auto& manager = ZFileHTTPSessionManager::Instance();
                            std::string baseURL = manager.ExtractBaseURL(msURL);
                            if (relativePath.length() > baseURL.length() && relativePath.substr(0, baseURL.length()) == baseURL)
                            {
                                relativePath = relativePath.substr(baseURL.length());
                            }

                            // Queue for upload via session
                            mpSession->QueueUpload(relativePath, data,
                                [this](bool uploadSuccess, long responseCode, const std::string& response)
                                {
                                    if (mbVerbose)
                                    {
                                        std::cout << "Session upload " << (uploadSuccess ? "succeeded" : "failed") << " for: " << msURL
                                            << " (code: " << responseCode << ")" << std::endl;
                                    }
                                });

                            success = true;  // Queued successfully
                        }
                        else
                        {
                            std::cerr << "Failed to read data from RAM file for session upload\n";
                            success = false;
                        }
                    }
                }
                else
                {
                    // Use traditional direct upload
                    success = HandleHTTPPost();
                }
            }

            if (mbVerbose && !mpSession)
            {
                cout << "Total HTTP Requests:" << gnTotalRequestsIssued << "\n";
                cout << "Total HTTP bytes requested:" << gnTotalHTTPBytesRequested << "\n";
            }

            if (mpCurlShare)
            {
                // Only cleanup if not using session
                curl_share_cleanup(mpCurlShare);
                mpCurlShare = nullptr;
            }

        }
        return success;
    }
        bool ZFileHTTP::HandleHTTPPost()
        {
            assert(mOpenFlags == eOpenFlags::kWrite);
            if (!mpFileRAM)
            {
                std::cerr << "ZFileHTTP does not have ram file prepared.\n" << std::endl;
                return false;
            }

            // Get the data from the RAM file
            int64_t dataSize = mpFileRAM->GetFileSize();
            if (dataSize == 0)
            {
                if (mbVerbose)
                    std::cout << "No data to POST for: " << msURL << std::endl;
                return true;  // Nothing to post is not an error
            }

            std::vector<uint8_t> data(dataSize);
            int64_t bytesRead = 0;
            if (!mpFileRAM->Read(0, dataSize, data.data(), bytesRead) || bytesRead != dataSize)
            {
                std::cerr << "Failed to read data from RAM file for POST\n";
                return false;
            }

            // Perform the HTTP POST
            CURL* pCurl = curl_easy_init();
            if (!pCurl)
            {
                std::cerr << "Failed to initialize CURL for POST\n";
                return false;
            }

            CURLcode res;
            bool bRetry = false;
            int nRetries = 0;
            const int kMaxRetries = 3;

            string sURL = msURL;

            for (auto& c : sURL)
            {
                if (c == '\\')
                    c = '/';
            }

            do
            {
                // Set up the POST request
                curl_easy_setopt(pCurl, CURLOPT_URL, sURL.c_str());
                curl_easy_setopt(pCurl, CURLOPT_POST, 1L);
                curl_easy_setopt(pCurl, CURLOPT_POSTFIELDS, data.data());
                curl_easy_setopt(pCurl, CURLOPT_POSTFIELDSIZE, (long)dataSize);
                curl_easy_setopt(pCurl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(pCurl, CURLOPT_SHARE, mpCurlShare);
                curl_easy_setopt(pCurl, CURLOPT_VERBOSE, (int)mbVerbose);
                curl_easy_setopt(pCurl, CURLOPT_TCP_KEEPALIVE, 1L);

                // Set authentication if provided
                if (!msName.empty())
                    curl_easy_setopt(pCurl, CURLOPT_USERNAME, msName.c_str());
                if (!msPassword.empty())
                    curl_easy_setopt(pCurl, CURLOPT_PASSWORD, msPassword.c_str());

                // SSL settings
                if (gbSkipCertCheck)
                {
                    curl_easy_setopt(pCurl, CURLOPT_SSL_VERIFYPEER, 0);
                }
                else
                {
                    curl_easy_setopt(pCurl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
                }

                // Set content type header (for binary data)
                struct curl_slist* headers = nullptr;
                headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
                curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, headers);

                // Optional: capture response
                curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, write_response_to_string);
                curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, &msResponse);

                // Perform the request
                res = curl_easy_perform(pCurl);

                // Clean up headers
                curl_slist_free_all(headers);

                if (res != CURLE_OK)
                {
                    if (!HandleHTTPError(res, "curl POST", bRetry))
                    {
                        std::cerr << "curl POST error for url:" << sURL << " response: " << curl_easy_strerror(res) << "\n";
                        curl_easy_cleanup(pCurl);
                        return false;
                    }

                    if (bRetry && nRetries < kMaxRetries)
                    {
                        nRetries++;
                        if (mbVerbose)
                            std::cout << "Retrying POST request (" << nRetries << "/" << kMaxRetries << ")\n";
                        continue;
                    }
                    else
                    {
                        std::cerr << "curl POST error for url:" << sURL << " response: " << curl_easy_strerror(res) << "\n";
                    }
                }
                else
                {
                    // Check HTTP response code
                    long response_code;
                    curl_easy_getinfo(pCurl, CURLINFO_RESPONSE_CODE, &response_code);

                    if (response_code >= 200 && response_code < 300)
                    {
                        if (mbVerbose)
                            std::cout << "POST successful for: " << sURL << " (" << response_code << ")\n";

                        gnTotalHTTPBytesRequested += dataSize;
                        gnTotalRequestsIssued++;
                        break;  // Success
                    }
                    else
                    {
                        std::cerr << "HTTP POST failed with response code: " << response_code << " for URL: " << sURL << "\n";
                        if (!msResponse.empty())
                            std::cerr << "Response: " << msResponse << "\n";

                        curl_easy_cleanup(pCurl);
                        return false;
                    }
                }

                bRetry = false;
                std::cerr << "curl POST error for url:" << sURL << " response: " << curl_easy_strerror(res) << "\n";
            } while (bRetry && nRetries < kMaxRetries);

            curl_easy_cleanup(pCurl);

            // clean up ramfile
            if (mpFileRAM)
            {
                mpFileRAM->Close();
            }

            return res == CURLE_OK;
        }

        bool ZFileHTTP::GetFileSizeViaSession()
        {
            if (!mpSession)
            {
                return false;
            }

            // Use session to perform HEAD request
            std::promise<bool> headPromise;
            auto headFuture = headPromise.get_future();

            // Create a HEAD request task (we need to extend the session for this)
            // For now, use immediate upload with empty data and special callback
            std::vector<uint8_t> emptyData;

            mpSession->PerformHeadRequest(
                msURL,
                [this, &headPromise](bool success, long responseCode, const std::string& response, const std::map<std::string, std::string>& headers)
                {
                    if (success && responseCode == 200)
                    {
                        // Parse Content-Length header
                        auto it = headers.find("content-length");
                        if (it != headers.end())
                        {
                            try
                            {
                                mnFileSize = std::stoull(it->second);
                                headPromise.set_value(true);
                                return;
                            }
                            catch (...)
                            {
                                // Failed to parse content length
                            }
                        }
                    }
                    if (responseCode == 404)
                    {
                        // 404 is not necessarily an error if using this call to check for file existence
                        if (mbVerbose)
                            cout << "FileSize HEAD request failed. response code:" << responseCode << " respnse:" << response << "\n";
                    }
                    else
                    {
                        cout << "FileSize HEAD request failed. response code:" << responseCode << " respnse:" << response << "\n";
                    }
                    headPromise.set_value(false);
                    mnLastError = responseCode;
                });

            return headFuture.get();
        }

        size_t ZFileHTTP::write_data(char* buffer, size_t size, size_t nitems, void* userp)
        {
            HTTPFileResponse* pResponse = (HTTPFileResponse*)userp;
            memcpy(pResponse->pDest + pResponse->nBytesWritten, buffer, size * nitems);
            pResponse->nBytesWritten += size * nitems;

            return size * nitems;
        }

        size_t ZFileHTTP::write_response_to_string(char* buffer, size_t size, size_t nitems, void* userp)
        {
            std::string* pResponse = (std::string*)userp;
            pResponse->append(buffer, size * nitems);
            return size * nitems;
        }

        bool ZFileHTTP::HandleHTTPError(int error, const std::string& sFunction, bool& bRetriable)
        {
            if (error == 0)
                return true;

            static std::vector<int> retriableCodes = {CURLE_COULDNT_CONNECT, CURLE_OPERATION_TIMEDOUT,      CURLE_SEND_ERROR,  CURLE_RECV_ERROR,
                                                      CURLE_AGAIN,           CURLE_NO_CONNECTION_AVAILABLE, CURLE_HTTP2_STREAM};

            for (auto i : retriableCodes)
            {
                if (i == error)
                {
                    bRetriable = true;
                    return true;
                }
            }

            return false;
        }

        bool ZFileHTTP::Read(int64_t nOffset, int64_t nBytes, uint8_t* pDestination, int64_t& nBytesRead)
        {
            if (mpSession)
            {
                return ReadFromSessionRange(nOffset, nBytes, pDestination, nBytesRead);
            }

            // Fallback to original implementation
            return ReadFromCurlRange(nOffset, nBytes, pDestination, nBytesRead);
        }

        bool ZFileHTTP::ReadFromSessionRange(int64_t nOffset, int64_t nBytes, uint8_t* pDestination, int64_t& nBytesRead)
        {
            bool bUseCache = false;
            int64_t nOffsetToRequest = nOffset;
            int64_t nBytesToRequest = nBytes;
            uint8_t* pBufferWrite = pDestination;

#ifdef USE_HTTP_CACHE
            shared_ptr<HTTPCacheLine> cacheLine;

            if (nBytesToRequest < kHTTPCacheLineSize)
                bUseCache = true;

            if (bUseCache)
            {
                bool bNew = mCache.CheckOrReserve(
                    nOffset, (int32_t)nBytes,
                    cacheLine);  // If a cache line that would contain this data hasn't already been requested this will reserve one and return it
                if (!bNew)
                {
                    cacheLine->Get(nOffset, (int32_t)nBytes, pDestination);  // this may block if the line is pending
                    nBytesRead = nBytes;

                    //            cout << "HTTP Data read from cache...Requested:" << nBytes << "b at offset:" << nOffset << "\n";
                    return true;
                }

                int64_t nUnfullfilledBytes = cacheLine->mUnfullfilledInterval.second - cacheLine->mUnfullfilledInterval.first;

                assert(!cacheLine->mbCommitted);

                nOffsetToRequest = cacheLine->mUnfullfilledInterval.first;
                if (nOffsetToRequest + nUnfullfilledBytes >= mnFileSize)
                    nUnfullfilledBytes = mnFileSize - nOffsetToRequest;

                assert(nUnfullfilledBytes >= 0);

                nBytesToRequest = nUnfullfilledBytes;
                int64_t nOffsetIntoCacheLine = nOffsetToRequest - cacheLine->mnBaseOffset;

                pBufferWrite = &cacheLine->mData[nOffsetIntoCacheLine];
            }
#endif

            bool bSuccess = false;

            int64_t nBytesReturned = 0;

            if (nBytesToRequest > 0)
            {
                std::promise<bool> readPromise;
                auto readFuture = readPromise.get_future();

                mpSession->PerformRangeRequest(msURL, nOffsetToRequest, nBytesToRequest,
                    [&readPromise, pBufferWrite, &nBytesReturned, nOffsetToRequest, nBytesToRequest, this](bool success, long responseCode, const std::vector<uint8_t>& data)
                    {
                        if (!success)
                        {
                            if (mbVerbose) cout << "PerformRangeRequest failed\n";
                            nBytesReturned = 0;
                            readPromise.set_value(false);
                            return;
                        }

                        if (responseCode >= 400)
                        {
                            mnLastError = responseCode;
                            readPromise.set_value(false);
                            return;
                        }

                        memcpy(pBufferWrite, data.data(), data.size());
                        nBytesReturned = data.size();
                        readPromise.set_value(true);
                    });

                bSuccess = readFuture.get();
            }
#ifdef USE_HTTP_CACHE
            // if the request can be cached
            if (bUseCache)
            {
                //cout << "committed " << nBytesToRequest << "b to cache offset:" << cacheLine->mnBaseOffset << ". Returning:" << nBytes << "\n";
                // cache the retrieved results

                if (cacheLine->mnBufferData + nBytesReturned < nBytes)
                {
                    // something went wrong and unable to return the requested data
                    assert(false);
                    return false;
                }

                memcpy(pDestination, cacheLine->mData, nBytes);
                cacheLine->Commit((int32_t)nBytesReturned);     // this commits the data to the cache line and frees any waiting requests on it

                nBytesRead = nBytes;
                return true;
            }
#endif

            nBytesRead = std::min<int64_t>(nBytesReturned, nBytes);
            return bSuccess;
        }

        bool ZFileHTTP::ReadFromCurlRange(int64_t nOffset, int64_t nBytes, uint8_t* pDestination, int64_t& nBytesRead)
        {
            if (nOffset < 0 || nOffset > mnFileSize)
            {
                assert(false);
                mnLastError = kZZFileError_IllegalSeek;
                return false;
            }

            mnLastError = kZZFileError_None;

            bool bUseCache = false;
            int64_t nOffsetToRequest = nOffset;
            int64_t nBytesToRequest = nBytes;
            uint8_t* pBufferWrite = pDestination;

#ifdef USE_HTTP_CACHE
            shared_ptr<HTTPCacheLine> cacheLine;

            if (nBytesToRequest < kHTTPCacheLineSize)
                bUseCache = true;

            if (bUseCache)
            {
                bool bNew = mCache.CheckOrReserve(
                    nOffset, (int32_t)nBytes,
                    cacheLine);  // If a cache line that would contain this data hasn't already been requested this will reserve one and return it
                if (!bNew)
                {
                    cacheLine->Get(nOffset, (int32_t)nBytes, pDestination);  // this may block if the line is pending
                    nBytesRead = nBytes;

                    //            cout << "HTTP Data read from cache...Requested:" << nBytes << "b at offset:" << nOffset << "\n";
                    return true;
                }

                int64_t nUnfullfilledBytes = cacheLine->mUnfullfilledInterval.second - cacheLine->mUnfullfilledInterval.first;

                assert(!cacheLine->mbCommitted);

                nOffsetToRequest = cacheLine->mUnfullfilledInterval.first;
                if (nOffsetToRequest + nUnfullfilledBytes >= mnFileSize)
                    nUnfullfilledBytes = mnFileSize - nOffsetToRequest;

                nBytesToRequest = nUnfullfilledBytes;
                int64_t nOffsetIntoCacheLine = nOffsetToRequest - cacheLine->mnBaseOffset;

                pBufferWrite = &cacheLine->mData[nOffsetIntoCacheLine];
            }
#endif

        if (nBytesToRequest > 0)
        {
            CURL* pCurl = curl_easy_init();


            HTTPFileResponse response;
            response.pDest = pBufferWrite;

            curl_easy_setopt(pCurl, CURLOPT_URL, msURL.c_str());
            curl_easy_setopt(pCurl, CURLOPT_FOLLOWLOCATION, 1);
            curl_easy_setopt(pCurl, CURLOPT_VERBOSE, (int)mbVerbose);
            curl_easy_setopt(pCurl, CURLOPT_BUFFERSIZE, 512 * 1024);
            curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, write_data);
            curl_easy_setopt(pCurl, CURLOPT_SHARE, mpCurlShare);
            curl_easy_setopt(pCurl, CURLOPT_NOBODY, 0);
            curl_easy_setopt(pCurl, CURLOPT_FAILONERROR, 1);
            curl_easy_setopt(pCurl, CURLOPT_TIMEOUT, 60L);
            curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, (void*)&response);
            curl_easy_setopt(pCurl, CURLOPT_TCP_KEEPALIVE, 1L);

            if (gbSkipCertCheck)
            {
                curl_easy_setopt(pCurl, CURLOPT_SSL_VERIFYPEER, 0);
            }
            else
            {
                curl_easy_setopt(pCurl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
            }


            stringstream ss;
            ss << nOffsetToRequest << "-" << nOffsetToRequest + nBytesToRequest;
            curl_easy_setopt(pCurl, CURLOPT_RANGE, ss.str().c_str());


            bool bRetry = true;
            while (bRetry)
            {
                //        uint64_t nStartTime = GetUSSinceEpoch();
                //        std::cout << "curl perform: " << ss.str() << "\n";
                CURLcode res = curl_easy_perform(pCurl);
                //        uint64_t nEndTime = GetUSSinceEpoch();

                if (res != CURLE_OK)
                {
                    if (!HandleHTTPError(res, "curl GET", bRetry))
                    {
                        std::cerr << "curl error: failed for url:" << msURL << " response: " << curl_easy_strerror(res) << "\n";
                        curl_easy_cleanup(pCurl);
                        return false;
                    }

                    std::cout << "ZFileHTTP::Read() retryable error:" << res << "\n";
                    continue;
                }

                long code = 0;
                curl_easy_getinfo(pCurl, CURLINFO_HTTP_CONNECTCODE, &code);
                if (code >= 400)
                {
                    mnLastError = code;

                    curl_easy_cleanup(pCurl);
                    return false;
                }

                bRetry = false;
            }

            nBytesRead = nBytes;
            gnTotalHTTPBytesRequested += nBytesToRequest;
            gnTotalRequestsIssued++;

            curl_easy_cleanup(pCurl);
        }

#ifdef USE_HTTP_CACHE
        // if the request can be cached
        if (bUseCache)
        {
            //cout << "committed " << nBytesToRequest << "b to cache offset:" << cacheLine->mnBaseOffset << ". Returning:" << nBytes << "\n";
            // cache the retrieved results
            memcpy(pDestination, cacheLine->mData, nBytes);
            cacheLine->Commit((int32_t)nBytesToRequest);     // this commits the data to the cache line and frees any waiting requests on it
        }
#endif
        return true;
    }
#endif // ENABLE_CURL


    bool ZFileHTTP::Write(int64_t nOffset, int64_t nBytes, uint8_t* pSource, int64_t& nBytesWritten)
    {
        assert(mOpenFlags == eOpenFlags::kWrite);
        if (!mpFileRAM)
        {
            std::cerr << "ZFileHTTP does not have ram file prepared.\n" << std::endl;
            return false;
        }

        if (mpFileRAM->Write(nOffset, nBytes, pSource, nBytesWritten))
        {
            mnWriteOffset = nOffset + nBytes;
            return true;
        }

        return false;
    }

    size_t ZFileHTTP::Read(uint8_t* pDestination, int64_t nBytes)
    {
        int64_t nBytesRead = 0;
        if (!Read(mnReadOffset, nBytes, pDestination, nBytesRead))
        {
            std::cerr << "ERROR Reading\n";
            return 0;
        }

        mnReadOffset += nBytesRead;
        return nBytesRead;
    }

    size_t ZFileHTTP::Write(uint8_t* pSource, int64_t nBytes)
    {
        int64_t nBytesWritten = 0;
        if (!Write(mnWriteOffset, nBytes, pSource, nBytesWritten))
            return 0;

        return nBytesWritten;
    }

    void ZFileHTTP::SeekRead(int64_t offset)
    {
        if (offset < 0 || offset > mnFileSize)
        {
            assert(false);
            mnLastError = kZZFileError_IllegalSeek;
        }
        mnReadOffset = offset;
    }

    void ZFileHTTP::SeekWrite(int64_t offset)
    {
        assert(mOpenFlags == eOpenFlags::kWrite);
        if (!mpFileRAM)
        {
            std::cerr << "ZFileHTTP does not have ram file prepared.\n" << std::endl;
            return;
        }

        mpFileRAM->SeekWrite(offset);
    }


    string ZFileHTTP::ToURLPath(const string& sPath)
    {
        string sNewPath(sPath);
        std::replace(sNewPath.begin(), sNewPath.end(), '\\', '/');
        return sNewPath;
    }
#endif

}; // end namespace ZFile