#pragma once

#ifdef ENABLE_CLM

#include <string>
#include "CommandLineParser.h"
#include "CommandLineCommon.h"
#include "StringHelpers.h"
#include <Windows.h>
#include <list>
#include <assert.h>

namespace CLP
{
    class LogWin : public InfoWin
    {
    public:
        LogWin() : topLogEntryTimestamp(0), lastReportedLogCount(0), viewAtEnd(true) {}

        void SetVisible(bool bVisible = true);

        void Update();
        void UpdateCaptions();
        bool OnKey(int keycode, char c);
        void Paint(tConsoleBuffer& backBuf);


        bool    viewAtEnd;
        uint64_t topLogEntryTimestamp;
        size_t lastReportedLogCount;

        bool    invalid = true;
        bool    viewCountEnabled = false;
        bool    viewTimestamp = false;
        bool    viewColoredThreads = false;
        bool    viewColorWarningsAndErrors = true;

        std::string sFilter;
    };


    class CommandLineMonitor
    {
    public:
        friend class LogWin;
        CommandLineMonitor();

        void Start();
        void End();
        bool IsDone() const { return mbDone; } 

    private:

        static void ThreadProc(CommandLineMonitor* pCLM);

        void UpdateFromConsoleSize(bool bForce = false);
        void UpdateDisplay();
        void DrawToScreen();
        void UpdateVisibility();

        tConsoleBuffer backBuffer;      // for double buffering
        tConsoleBuffer drawStateBuffer; // for rendering only delta

        std::thread monitorthread;

        bool mbVisible;
        bool mbLastVisibleState;
        bool mbDone;
        bool mbCanceled;
    };

    extern  LogWin      logWin;
    extern  TextEditWin textEntryWin;


};  // namespace CLP

#endif // ENABLE_CLM
