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

        LogWin() : lastReportedLogCount(0) {}

        void SetVisible(bool bVisible = true);

        void Update();
        void UpdateCaptions();
        bool OnKey(int keycode, char c);
        void Paint(tConsoleBuffer& backBuf);


        bool        viewAtEnd = true;
        size_t      lastReportedLogCount;

        bool        invalid = true;
        bool        viewCountEnabled = false;
        bool        viewTimestamp = false;
        bool        viewColoredThreads = false;
        bool        viewColorWarningsAndErrors = true;

        std::string GetColorForThreadID(std::thread::id);

        std::string sFilter;

    protected:
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
