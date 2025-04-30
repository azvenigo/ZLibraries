#pragma once
#include <string>
#include <list>
#include <fstream>
#include <mutex>

namespace LOG
{

    class FileLogger
    {
    public:
        FileLogger(int64_t _maxLogfileBytes = 4 * 1024 * 1024);  // whenever the file goes over this size, it will be trimmed

        ~FileLogger();

        void                    Log(const std::string& sLine);
        void                    Flush();

        std::string             msLogFilename;
        int64_t                 mMaxLogfileBytes;

    protected:
        void                    WriteToFile(const std::string& sLine);
        void                    Trim();
        std::list<std::string>  mQueue;
        int64_t                 mCurrentLogfileBytes;
        std::fstream            mFile;

        std::recursive_mutex    mMutex;

    };
};
extern LOG::FileLogger gLogger;

