#pragma once
#include <string>
#include <list>
#include <fstream>
#include <mutex>

namespace LOG
{
    typedef std::list<std::string>  tStringList;


    class Logger
    {
    public:
        Logger(int64_t _maxLogfileBytes = 4 * 1024 * 1024);  // whenever the file goes over this size, it will be trimmed

        ~Logger();

        void            Log(const std::string& sLine);
        void            Flush();

        std::string     msLogFilename;
        int64_t         mMaxLogfileBytes;

    protected:
        void            WriteToFile(const std::string& sLine);
        void            Trim();
        tStringList     mQueue;
        int64_t         mCurrentLogfileBytes;
        std::fstream    mFile;

        std::recursive_mutex    mMutex;

    };
};
extern LOG::Logger gLogger;

