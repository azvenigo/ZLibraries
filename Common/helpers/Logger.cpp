#include "Logger.h"
#include <assert.h>
#include <iostream>
#include <ctime>
//#include <iomanip>


using namespace std;

namespace LOG
{
    const int kMaxQueueSize = 10;

    Logger::Logger(int64_t _maxLogfileBytes)
    {
        mMaxLogfileBytes = _maxLogfileBytes;
        mCurrentLogfileBytes = 0;
    }

    Logger::~Logger()
    {
        Flush();
        mFile.close();
    }

    void Logger::Log(const std::string& sLine)
    {
        const std::time_t now = std::time(nullptr);
        char buf[32];
        std::strftime(buf, 32, "[%F %X] ", std::localtime(&now));

        string stampedLine(buf);
        stampedLine.append(sLine);

        const std::lock_guard<std::recursive_mutex> lock(mMutex);
        mQueue.emplace_back(std::move(stampedLine));

       if (mQueue.size() > kMaxQueueSize)
           Flush();
    }



    void Logger::Flush()
    {
        const std::lock_guard<std::recursive_mutex> lock(mMutex);

        for (auto s : mQueue)
        {
            WriteToFile(s);
        }
        mQueue.clear();

        Trim();
    }

    void Logger::Trim()
    {
        if (mCurrentLogfileBytes < mMaxLogfileBytes)
            return;

        const std::lock_guard<std::recursive_mutex> lock(mMutex);

        int64_t nSizeAfterTrim = (mMaxLogfileBytes * 8) / 10;   // 80%

        char* pTemp = new char[nSizeAfterTrim];

        mFile.seekg(mMaxLogfileBytes - nSizeAfterTrim);

        char t;
        while (mFile.peek() != '\r' && mFile.peek() != '\n')
        {
            mFile >> t;
            nSizeAfterTrim--;
        }
        
        mFile.read(pTemp, nSizeAfterTrim);
        mFile.close();

        mFile.open(msLogFilename, ios::in | ios::out | ios::trunc);
        mFile.write(pTemp, nSizeAfterTrim);

        mCurrentLogfileBytes = nSizeAfterTrim;

        delete[] pTemp;
    }


    void Logger::WriteToFile(const std::string& sLine)
    {
        const std::lock_guard<std::recursive_mutex> lock(mMutex);

        assert(!msLogFilename.empty());

        if (!mFile.is_open())
        {
            mFile.open(msLogFilename, ios::in | ios::out|ios::ate);
            if (mFile.fail())
            {
                string s = strerror(errno);
                cerr << "Error opening log: " << strerror(errno) << "\n";
            }

            mFile.seekp(0, ios::end);
            mCurrentLogfileBytes = mFile.tellp();
        }

        //assert(mFile);

        mFile.write(sLine.data(), sLine.length());
        mCurrentLogfileBytes += sLine.length();           
    }


    std::string     msLogFilename;
    int64_t         mMaxLogfileBytes;
};

