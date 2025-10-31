#pragma once

#ifdef ENABLE_CLM

#include <string>
#include "CommandLineParser.h"
#include "CommandLineCommon.h"
#include "StringHelpers.h"
//#include <Windows.h>
#include <list>
#include <assert.h>

namespace CLP
{


    const std::vector<std::string> kThreadCols =
    {
        "\033[48;2;123;15;15m",
        "\033[48;2;123;125;15m",
        "\033[48;2;123;15;125m",
        "\033[48;2;1;15;115m",
        "\033[48;2;113;113;113m",
        "\033[48;2;13;115;115m",
        "\033[48;2;80;80;85m",
    };

    class LogWin : public InfoWin
    {
    public:

        enum eDateTimeDisplay : uint8_t
        {
            kNone = 0,
            kTime = 1,
            kDateTime = 2,
            kTimeElapsed = 3,
            kTimeFromPrevious = 4,
            kTimeSince = 5
        };



        LogWin() : lastReportedLogCount(0) {}

        void SetVisible(bool bVisible = true);

        void Update();
        void UpdateCaptions();
        bool OnKey(int keycode, char c);
        void Paint(tConsoleBuffer& backBuf);

        void ShowSaveFilenamePrompt();

        bool        viewAtEnd = true;
        size_t      lastReportedLogCount;

        bool        invalid = true;
        bool        viewCountEnabled = true;
        uint8_t     viewDateTime = kTimeElapsed;
        bool        viewColoredThreads = false;
        bool        viewColorWarningsAndErrors = true;

        std::string GetColorForThreadID(std::thread::id);
        std::string TimeLabel();
        std::string TimeValue(int64_t entryTime, int64_t prevEntryTime);
        void        HookCTRL_S(bool bHook = true);

        std::string sFilter;
        std::string sLogFilename;

    protected:
        bool        bCTRL_S_Hooked = false;
        void        OffsetEntry(int64_t offset);        // next/prev or up/down some number based on current filter
        void        SetEntryFromBeginning(int64_t offset_from_beginning);
        void        SetEntryFromEnd(int64_t offset_from_end);
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
        bool OnKey(int keycode, char c);
        bool OnMouse(MOUSE_EVENT_RECORD event);

        static void ThreadProc(CommandLineMonitor* pCLM);

        bool UpdateFromConsoleSize(bool bForce = false);
        void UpdateDisplay();
        void DrawToScreen();
        void UpdateVisibility();

        std::thread monitorthread;

        bool mbVisible;
        bool mbLastVisibleState;
        bool mbDone;
        bool mbCanceled;
    };

    extern  LogWin      logWin;
    extern  TextEditWin filterTextEntryWin;
    extern  TextEditWin saveLogFilenameEntryWin;
};  // namespace CLP

#endif // ENABLE_CLM
