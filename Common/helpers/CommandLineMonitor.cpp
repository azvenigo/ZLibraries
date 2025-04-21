#ifdef ENABLE_CLE

#include "CommandLineMonitor.h"
#include "LoggingHelpers.h"
#include "FileHelpers.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <assert.h>
#include <format>
#include <fstream>
#include <filesystem>
#include "MathHelpers.h"

#include <stdio.h>

using namespace std;
namespace fs = std::filesystem;


namespace CLP
{

    void LogWin::SetVisible(bool bVisible)
    {
        mbVisible = bVisible;
        gLogOut.outputToFallback = !bVisible;
        gLogErr.outputToFallback = !bVisible;

        if (bVisible)
        {
            cout << "\033[?25l";    // hide cursor
        }
        else
        {
            cout << "\033[?25h" << COL_WHITE << COL_BG_BLACK;    // show cursor, reset colors
            RestoreConsoleState();
        }
    }

    void LogWin::Update()
    {
        if (!mbVisible)
            return;

        if (lastReportedLogCount != gLogger.getCount())
        {
            lastReportedLogCount = gLogger.getCount();
            invalid = true;
        }


        if (invalid)
        {
            Rect drawArea;
            GetInnerArea(drawArea);
            int64_t drawWidth = drawArea.r - drawArea.l - 2;
            int64_t drawHeight = drawArea.b - drawArea.t-1;

            LOG::tLogEntries entries;
                
            if (viewAtEnd)
            {
                entries = gLogger.tail(drawHeight);
                topLogEntryTimestamp = 0;
                if (!entries.empty())
                {
                    topLogEntryTimestamp = entries[0].time;
                    mTopVisibleRow = gLogger.getCount() - drawHeight;
                }
            }
            else
            {
                gLogger.getEntries(mTopVisibleRow, drawHeight, entries);
            }

            positionCaption[ConsoleWin::Position::LT] = "Info Window";

            Table logtail;
            if (mTopVisibleRow > 0)
                logtail.borders[Table::TOP].clear();    // if there are lines above the top of the window, no top table border
            if (mTopVisibleRow + drawHeight < (int64_t)gLogger.getCount())
                logtail.borders[Table::BOTTOM].clear(); // if there are lines below the bottom of the window, no bottom table border

            for (const auto& e : entries)
            {
                Table::Style cellStyle;
                Table::tCellArray row;
                if (viewCountEnabled)
                    row.push_back(SH::FromInt(e.counter));
                if (viewTimestamp)
                    row.push_back(LOG::usToDateTime(e.time));
                if (viewColorWarningsAndErrors)
                {
                    if (SH::Contains(e.text, "error", false))
                        cellStyle = Table::Style(COL_RED);
                    else if (SH::Contains(e.text, "warning", false))
                        cellStyle = Table::Style(COL_ORANGE);
                }
                row.push_back(Table::Cell(e.text, cellStyle));

                logtail.AddRow(row);
            }

            logtail.AlignWidth(drawWidth, logtail);
            mText = (string)logtail;

            UpdateCaptions();
        }

        invalid = false;
    }

    void LogWin::UpdateCaptions()
    {
        string sFeatures;
        if (viewCountEnabled)
            sFeatures += "[1:COUNT] ";
        else
            sFeatures += "[1:count] ";

        if (viewTimestamp)
            sFeatures += "[2:TIME] ";
        else
            sFeatures += "[2:time] ";

        if (viewColoredThreads)
            sFeatures += "[3:THREADS] ";
        else
            sFeatures += "[3:threads] ";

        if (viewColorWarningsAndErrors)
            sFeatures += "[4:WARN/ERROR] ";
        else
            sFeatures += "[4:warn/error] ";


        positionCaption[ConsoleWin::Position::LT] = sFeatures;
        positionCaption[ConsoleWin::Position::RT] = "Log lines (" + SH::FromInt(mTopVisibleRow + 1) + "/" + SH::FromInt(gLogger.getCount()) + ")";
        positionCaption[ConsoleWin::Position::RB] = "[UP/DOWN][PAGE Up/Down][HOME/END]";
    }

    void LogWin::Paint(tConsoleBuffer& backBuf)
    {
        if (!mbVisible)
            return;

        ConsoleWin::BasePaint();

        Rect drawArea;
        GetInnerArea(drawArea);

        DrawClippedAnsiText(drawArea.l, drawArea.t, mText, true, &drawArea);

        ConsoleWin::RenderToBackBuf(backBuf);
    }

    void LogWin::OnKey(int keycode, char c)
    {
        Rect drawArea;
        GetInnerArea(drawArea);
        int64_t drawHeight = drawArea.b - drawArea.t;


        int64_t entryCount = gLogger.getCount();
        if (keycode == VK_F1 || keycode == VK_ESCAPE)
        {
            mText.clear();
            SetVisible(false);
            mbDone = true;
        }


        if (keycode == VK_UP)
        {
            mTopVisibleRow--;
            viewAtEnd = false;
            invalid = true;
        }
        else if (keycode == VK_DOWN)
        {
            mTopVisibleRow++;
            invalid = true;
        }
        else if (keycode == VK_HOME)
        {
            mTopVisibleRow = 0;
            viewAtEnd = false;
            invalid = true;
        }
        else if (keycode == VK_PRIOR)
        {
            mTopVisibleRow -= drawHeight;
            viewAtEnd = false;
            invalid = true;
        }
        else if (keycode == VK_NEXT)
        {
            mTopVisibleRow += drawHeight;
            invalid = true;
        }
        else if (keycode == VK_END)
        {
            mTopVisibleRow = entryCount - drawHeight;
            viewAtEnd = true;
            invalid = true;
        }
        else if (keycode == '1')
        {
            viewCountEnabled = !viewCountEnabled;
            invalid = true;
        }
        else if (keycode == '2')
        {
            viewTimestamp = !viewTimestamp;
            invalid = true;
        }
        else if (keycode == '3')
        {
            viewColoredThreads = !viewColoredThreads;
            invalid = true;
        }
        else if (keycode == '4')
        {
            viewColorWarningsAndErrors = !viewColorWarningsAndErrors;
            invalid = true;
        }

        if (mTopVisibleRow < 0)
        {
            mTopVisibleRow = 0;
            viewAtEnd = false;
            invalid = true;
        }
        if (mTopVisibleRow > (entryCount - drawHeight))
        {
            mTopVisibleRow = entryCount;
            viewAtEnd = true;
            invalid = true;
        }

        Update();
    }



    CommandLineMonitor::CommandLineMonitor()
    {
    }

    void CommandLineMonitor::UpdateDisplay()
    {
        DrawToScreen();

/*        static int count = 1;
        char buf[64];
        sprintf(buf, "draw:%d\n", count++);
        OutputDebugString(buf);*/
   
    }

    void CommandLineMonitor::DrawToScreen()
    {
        // clear back buffer
        memset(&backBuffer[0], 0, backBuffer.size() * sizeof(ZChar));
        bool bCursorShouldBeHidden = false;
        bool bCursorHidden = false;

        if (infoWin.mbVisible)
            infoWin.Paint(backBuffer);


        if (!bCursorHidden)
        {
            bCursorHidden = true;
            cout << "\033[?25l";
        }

        for (int64_t y = 0; y < ScreenH(); y++)
        {
            for (int64_t x = 0; x < ScreenW(); x++)
            {
                int64_t i = (y * ScreenW()) + x;
                if (bScreenChanged || backBuffer[i] != drawStateBuffer[i])
                {
                    DrawAnsiChar(x, y, backBuffer[i].c, backBuffer[i].attrib);
                }
            }
        }

        drawStateBuffer = backBuffer;
        bScreenChanged = false;


        if (bCursorHidden && !bCursorShouldBeHidden)
        {
            cout << "\033[?25h";
        }
    }

    void CommandLineMonitor::UpdateFromConsoleSize(bool bForce)
    {
        CONSOLE_SCREEN_BUFFER_INFO newScreenInfo;
        if (!GetConsoleScreenBufferInfo(mhOutput, &newScreenInfo))
        {
            cerr << "Failed to get console info." << endl;
            return;
        }

        if ((newScreenInfo.dwSize.X != screenInfo.dwSize.X || newScreenInfo.dwSize.Y != screenInfo.dwSize.Y) || bForce)
        {
            screenInfo = newScreenInfo;
            bScreenChanged = true;

            SHORT w = ScreenW();
            SHORT h = ScreenH();

            if (w < 1)
                w = 1;
            if (h < 8)
                h = 8;


            backBuffer.clear();
            backBuffer.resize(w*h);


            drawStateBuffer.clear();
            drawStateBuffer.resize(w * h);

            infoWin.Clear(kAttribHelpBG, true);
            infoWin.SetArea(Rect(1, 1, w-1, h-1));

            UpdateDisplay();
        }
    }

    void CommandLineMonitor::SetMonitorVisible(bool bVisible)
    {
        mbVisible = bVisible;
        if (mbVisible)
        {
            SaveConsoleState();

            string sText;
            infoWin.Init(Rect(0, 1, ScreenW(), ScreenH()));
            infoWin.SetEnableFrame();
            infoWin.Clear(kAttribHelpBG, true);

            infoWin.Update();
            infoWin.SetVisible(true);
        }
        else
        {
            infoWin.SetVisible(false);
            RestoreConsoleState();

            LOG::tLogEntries entries = gLogger.tail(ScreenH());
            for (const auto& entry : entries)
            {
                cout << entry.text << std::endl;
            }
        }
        bScreenChanged = true;
    }



    void CommandLineMonitor::Start()
    {
        mbVisible = false;
        mbDone = false;
        mbCanceled = false;

        // Get the handle to the standard input
        mhInput = GetStdHandle(STD_INPUT_HANDLE);
        mhOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        if (mhInput == INVALID_HANDLE_VALUE || mhOutput == INVALID_HANDLE_VALUE)
        {
            cerr << "Failed to get standard input/output handle." << endl;
            return;
        }

        int MY_HOTKEY_ID = 1;

        if (!RegisterHotKey(NULL, MY_HOTKEY_ID, MOD_CONTROL, 'V'))
        {
            std::cerr << "Error registering hotkey" << std::endl;
            return;
        }

        if (!RegisterHotKey(NULL, MY_HOTKEY_ID, MOD_SHIFT, VK_INSERT))
        {
            std::cerr << "Error registering hotkey" << std::endl;
            return;
        }

        if (!GetConsoleScreenBufferInfo(mhOutput, &screenInfo))
        {
            cerr << "Failed to get console info." << endl;
            return;
        }


        SHORT w = ScreenW();
        SHORT h = ScreenH();


        std::vector<CHAR_INFO> blank(w * h);
        for (int i = 0; i < blank.size(); i++)
        {
            blank[i].Char.AsciiChar = ' ';
            blank[i].Attributes = 0;
        }
        SMALL_RECT smallrect(0, 0, w, h);
        //WriteConsoleOutput(mhOutput, &blank[0], screenInfo.dwSize, { 0, 0 }, &smallrect);


        // Set console mode to allow reading mouse and key events
        DWORD mode;
        if (!GetConsoleMode(mhInput, &mode))
        {
            cerr << "Failed to get console mode." << endl;
            return;
        }
        mode |= ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        mode &= ~ENABLE_PROCESSED_INPUT;

        //mode |= ENABLE_PROCESSED_INPUT;
        if (!SetConsoleMode(mhInput, mode))
        {
            cerr << "Failed to set console mode." << endl;
            return;
        }

        // Main loop to read input events
        INPUT_RECORD inputRecord[128];
        DWORD numEventsRead;

        backBuffer.resize(w*h);
        drawStateBuffer.resize(w * h);

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Clear the raw command buffer and param buffers
        //UpdateFromConsoleSize(true);

        while (!mbDone && !mbCanceled)
        {
            infoWin.Update();

            if (mbVisible)
            {
                UpdateFromConsoleSize();
                UpdateDisplay();
            }

            // Check for hotkey events
            MSG msg;
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            if (PeekConsoleInput(mhInput, inputRecord, 128, &numEventsRead) && numEventsRead > 0)
            {
                for (DWORD i = 0; i < numEventsRead; i++)
                {
                    if (!ReadConsoleInput(mhInput, inputRecord, 1, &numEventsRead))
                    {
                        cerr << "Failed to read console input." << endl;
                        return;
                    }

                    if (inputRecord[i].EventType == MOUSE_EVENT)
                    {
                        MOUSE_EVENT_RECORD mer = inputRecord[i].Event.MouseEvent;
                    }
                    else if (inputRecord[i].EventType == KEY_EVENT && inputRecord[i].Event.KeyEvent.bKeyDown)
                    {
                        int keycode = inputRecord[i].Event.KeyEvent.wVirtualKeyCode;
                        char c = inputRecord[i].Event.KeyEvent.uChar.AsciiChar;


                        if (keycode == VK_ESCAPE)
                        {
                            if (infoWin.mbVisible)
                            {
                                infoWin.SetVisible(false);
                            }
                            else
                                mbDone = true;
                            bScreenChanged = true;
                        }
                        else if (keycode == VK_F1)
                        {
                            SetMonitorVisible(!mbVisible);
                        }
                        else
                        {
                            if (infoWin.mbVisible)
                                infoWin.OnKey(keycode, c);
                        }
                    }
                }
            }
        }

        RestoreConsoleState();
    }

};

#endif // ENABLE_CLM
