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

    LogWin      logWin;
    TextEditWin textEntryWin;
//    InfoWin     helpWin;        // popup help window


    void LogWin::SetVisible(bool bVisible)
    {
        if (mbVisible != bVisible)
            invalid = true;

        mbVisible = bVisible;
        LOG::gLogOut.m_outputToFallback = !bVisible;
        LOG::gLogErr.m_outputToFallback = !bVisible;

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

        if (lastReportedLogCount != LOG::gLogger.getCount())
        {
            lastReportedLogCount = LOG::gLogger.getCount();
            invalid = true;
        }

        if (textEntryWin.GetText() != sFilter)
        {
            sFilter = textEntryWin.GetText();
            invalid = true;
        }


        if (invalid)
        {
            Rect drawArea;
            GetInnerArea(drawArea);
            int64_t drawWidth = drawArea.r - drawArea.l - 1;
            int64_t drawHeight = drawArea.b - drawArea.t-1;
            if (textEntryWin.mbVisible)
                drawHeight--;


            LOG::tLogEntries entries;
                
            if (viewAtEnd)
            {
                entries = LOG::gLogger.tail(drawHeight, sFilter);
                topLogEntryTimestamp = 0;
                if (!entries.empty())
                {
                    topLogEntryTimestamp = entries[0].time;
                    mTopVisibleRow = LOG::gLogger.getCount() - drawHeight;
                }
            }
            else
            {
                LOG::gLogger.getEntries(mTopVisibleRow, drawHeight, entries, sFilter);
            }

            positionCaption[ConsoleWin::Position::LT] = "Info Window";

            Table logtail;
            if (mTopVisibleRow > 0)
                logtail.borders[Table::TOP].clear();    // if there are lines above the top of the window, no top table border
            if (mTopVisibleRow + drawHeight < (int64_t)LOG::gLogger.getCount())
                logtail.borders[Table::BOTTOM].clear(); // if there are lines below the bottom of the window, no bottom table border

            for (const auto& e : entries)
            {
                Table::tCellArray row;
                Table::Style cellStyle;
                if (viewCountEnabled)
                    row.push_back(SH::FromInt(e.counter));
                if (viewTimestamp)
                    row.push_back(LOG::usToDateTime(e.time));

                if (ContainsAnsiSequences(e.text))
                {
                    cellStyle = Table::Style(COL_CUSTOM_STYLE);
                }
                else if (viewColorWarningsAndErrors)
                {
                    if (SH::Contains(e.text, "error", false))
                    {
                        cellStyle = Table::Style(COL_RED);
                    }
                    else if (SH::Contains(e.text, "warning", false))
                    {
                        cellStyle = Table::Style(COL_ORANGE);
                    }

                }
#ifdef _DEBUG
                validateAnsiSequences(e.text);
#endif
                row.push_back(Table::Cell(e.text, cellStyle));
#ifdef _DEBUG
                validateAnsiSequences(Table::Cell(e.text, cellStyle).StyledOut(e.text.length()));
#endif

                logtail.AddRow(row);
            }

            logtail.AlignWidth(drawWidth, logtail);
            mText = (string)logtail;
#ifdef _DEBUG
            validateAnsiSequences(mText);
#endif

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

        if (sFilter.empty())
            positionCaption[ConsoleWin::Position::LB] = "Filter:(none)";
        else
            positionCaption[ConsoleWin::Position::LB] = "Filter:\"" + sFilter + "\"";

        positionCaption[ConsoleWin::Position::RT] = "Log lines (" + SH::FromInt(mTopVisibleRow + 1) + "/" + SH::FromInt(LOG::gLogger.getCount()) + ")";
        positionCaption[ConsoleWin::Position::RB] = "[UP/DOWN][PAGE Up/Down][HOME/END]";
    }

    void LogWin::Paint(tConsoleBuffer& backBuf)
    {
        if (!mbVisible)
            return;

        ConsoleWin::BasePaint();

        Rect drawArea;
        GetInnerArea(drawArea);


        DrawClippedAnsiText(drawArea, mText, true, &drawArea);
        int64_t h = drawArea.b - drawArea.t;

        int64_t logCount = (int64_t)LOG::gLogger.getCount();
        if (logCount > h)
        {
//            ZAttrib bg(MAKE_BG(0xff555555));
//            ZAttrib thumb(MAKE_BG(0xffbbbbbb));
            Rect sb(drawArea.r - 1, drawArea.t, drawArea.r, drawArea.b);
            DrawScrollbar(sb, 0, LOG::gLogger.getCount()-h, mTopVisibleRow, kAttribScrollbarBG, kAttribScrollbarThumb);
        }

        ConsoleWin::RenderToBackBuf(backBuf);
    }

    bool LogWin::OnKey(int keycode, char c)
    {
        bool bHandled = false;
        Rect drawArea;
        GetInnerArea(drawArea);
        int64_t drawHeight = drawArea.b - drawArea.t;

        bool bCTRLHeld = GetKeyState(VK_CONTROL) & 0x800;


        int64_t entryCount = LOG::gLogger.getCount();

        if (keycode == VK_UP)
        {
            mTopVisibleRow--;
            viewAtEnd = false;
            invalid = true;
            bHandled = true;
        }
        else if (keycode == VK_DOWN)
        {
            mTopVisibleRow++;
            invalid = true;
            bHandled = true;
        }
        else if (keycode == VK_HOME)
        {
            mTopVisibleRow = 0;
            viewAtEnd = false;
            invalid = true;
            bHandled = true;
        }
        else if (keycode == VK_PRIOR)
        {
            mTopVisibleRow -= drawHeight;
            viewAtEnd = false;
            invalid = true;
            bHandled = true;
        }
        else if (keycode == VK_NEXT)
        {
            mTopVisibleRow += drawHeight;
            invalid = true;
            bHandled = true;
        }
        else if (keycode == VK_END)
        {
            mTopVisibleRow = entryCount - drawHeight;
            viewAtEnd = true;
            invalid = true;
            bHandled = true;
        }
        else if (keycode == '1')
        {
            viewCountEnabled = !viewCountEnabled;
            invalid = true;
            bHandled = true;
        }
        else if (keycode == '2')
        {
            viewTimestamp = !viewTimestamp;
            invalid = true;
            bHandled = true;
        }
        else if (keycode == '3')
        {
            viewColoredThreads = !viewColoredThreads;
            invalid = true;
            bHandled = true;
        }
        else if (keycode == '4')
        {
            viewColorWarningsAndErrors = !viewColorWarningsAndErrors;
            invalid = true;
            bHandled = true;
        }
        else if (keycode == 'f' || keycode == 'F')
        {
            if (bCTRLHeld)
            {
                if (!textEntryWin.mbVisible)
                {
                    textEntryWin.Clear(ZAttrib(0xff666699ffffffff));
                    textEntryWin.SetArea(Rect(mX, mY + mHeight, mX + mWidth - 1, mY + mHeight+1));
                    textEntryWin.SetVisible();
                    invalid = true;
                }
                bHandled = true;
            }
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
        return bHandled;
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

        if (logWin.mbVisible)
            logWin.Paint(backBuffer);
        if (textEntryWin.mbVisible)
            textEntryWin.Paint(backBuffer);
        if (helpWin.mbVisible)
            helpWin.Paint(backBuffer);

        cout << "\033[?25l";
        COORD savePos = gLastCursorPos;

        for (int64_t y = 0; y < ScreenH(); y++)
        {
            for (int64_t x = 0; x < ScreenW(); x++)
            {
                int64_t i = (y * ScreenW()) + x;
                if (bScreenInvalid || backBuffer[i] != drawStateBuffer[i])
                {
                    DrawAnsiChar(x, y, backBuffer[i].c, backBuffer[i].attrib);
                }
            }
        }

        SetCursorPosition(savePos);

        drawStateBuffer = backBuffer;
        bScreenInvalid = false;


        //        if (bCursorHidden && !bCursorShouldBeHidden)
        if (textEntryWin.mbVisible)
        {
            cout << "\033[?25h";    // visible cursor
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
            bScreenInvalid = true;

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


            logWin.Clear(kAttribHelpBG, true);

            Rect logWinRect(1, 1, w - 1, h - 1);
            if (textEntryWin.mbVisible)
                logWinRect.b--;
            logWin.SetArea(logWinRect);

            textEntryWin.SetArea(Rect(1, h-1, w-1, h));

            helpWin.Clear(kAttribHelpBG, true);
            helpWin.SetArea(Rect(0, 1, w, h));
            helpWin.SetEnableFrame();
            helpWin.bAutoScrollbar = true;

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
            logWin.Init(Rect(0, 1, ScreenW(), ScreenH()));
            logWin.SetEnableFrame();
            logWin.Clear(kAttribHelpBG, true);

            logWin.Update();
            logWin.SetVisible(true);
        }
        else
        {
            logWin.SetVisible(false);
            RestoreConsoleState();

            LOG::tLogEntries entries = LOG::gLogger.tail(ScreenH());
            for (const auto& entry : entries)
            {
                cout << entry.text << std::endl;
            }
        }
        bScreenInvalid = true;
    }


    void CommandLineMonitor::ThreadProc(CommandLineMonitor* pCLM)
    {
        // Main loop to read input events
        INPUT_RECORD inputRecord[128];
        DWORD numEventsRead;

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


        pCLM->backBuffer.resize(w*h);
        pCLM->drawStateBuffer.resize(w * h);

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Clear the raw command buffer and param buffers
        //UpdateFromConsoleSize(true);

        while (!pCLM->mbDone && !pCLM->mbCanceled)
        {
            logWin.Update();

            if (textEntryWin.mbVisible)
            {
                if (GetConsoleWindow() == GetForegroundWindow())
                {
                    textEntryWin.HookHotkeys();
                }
                else
                {
                    textEntryWin.UnhookHotkeys();
                }
            }

            if (pCLM->mbVisible || helpWin.mbVisible)
            {
                pCLM->UpdateFromConsoleSize(bScreenInvalid);  // force update if the screen changed
                pCLM->UpdateDisplay();
            }

            // Check for hotkey events
            MSG msg;
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_HOTKEY && (msg.wParam == CTRL_V_HOTKEY || msg.wParam == SHIFT_INSERT_HOTKEY))
                {
                    if (textEntryWin.mbVisible)
                    {
                        textEntryWin.AddUndoEntry();
                        textEntryWin.HandlePaste(GetTextFromClipboard());
                    }
                }
                else
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
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

                        bool bHandled = false;
                        if (helpWin.mbVisible)
                            bHandled = helpWin.OnKey(keycode, c);
                        if (textEntryWin.mbVisible && !bHandled)
                            bHandled = textEntryWin.OnKey(keycode, c);
                        if (logWin.mbVisible && !bHandled)
                            bHandled = logWin.OnKey(keycode, c);

                        if (textEntryWin.mbVisible)
                        {
                            if (textEntryWin.mbCanceled)
                                textEntryWin.SetText("");

                            if (textEntryWin.mbCanceled || textEntryWin.mbDone)
                            {
                                textEntryWin.SetVisible(false);
                                textEntryWin.mbCanceled = false;
                                textEntryWin.mbDone = false;
                                bHandled = true;
                                bScreenInvalid = true;
                            }
                        }
                        if (logWin.mbVisible && logWin.mbCanceled)
                        {
                            pCLM->SetMonitorVisible(!pCLM->mbVisible);
                            bHandled = true;
                            bScreenInvalid = true;
                        }

                        if (keycode == VK_ESCAPE && !bHandled)
                        {
                            pCLM->mbDone = true;
                            bScreenInvalid = true;
                        }
                        else if (keycode == VK_F1)
                        {
                            pCLM->SetMonitorVisible(!pCLM->mbVisible);
                        }
                        else if (keycode == VK_F2)
                        {
                            ShowEnvVars();
                        }
                    }
                }
            }
        }

        if (pCLM->mbVisible)
            pCLM->SetMonitorVisible(false);
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

        if (!GetConsoleScreenBufferInfo(mhOutput, &screenInfo))
        {
            cerr << "Failed to get console info." << endl;
            return;
        }



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

        monitorthread = thread(ThreadProc, this);
    }

    void CommandLineMonitor::End()
    {
        mbDone = true;
        monitorthread.join();
    }

};

#endif // ENABLE_CLM
