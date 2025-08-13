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
    }

    void LogWin::Update()
    {
        if (!mbVisible)
            return;

        if (lastReportedLogCount != LOG::gLogger.getEntryCount())
        {
            lastReportedLogCount = LOG::gLogger.getEntryCount();
            invalid = true;
        }

        if (textEntryWin.GetText() != sFilter)
        {
            sFilter = textEntryWin.GetText();
            LOG::gLogger.setFilter(sFilter);
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
                
            int64_t firstRowCounter = -1;
            int64_t lastRowCounter = -1;

            if (viewAtEnd)
            {
                mTopVisibleRow = LOG::gLogger.getEntryCount() - drawHeight;
                LOG::gLogger.tail(drawHeight, entries);
            }
            else
            {
                LOG::gLogger.getEntries(mTopVisibleRow, drawHeight, entries);
            }

            if (!entries.empty())
            {
                firstRowCounter = entries[0].counter;
                lastRowCounter = entries[entries.size() - 1].counter;
            }


            positionCaption[ConsoleWin::Position::LT] = "Info Window";

            Table logtail;

            if (firstRowCounter > 0)
                logtail.borders[Table::TOP].clear();    // if there are lines above the top of the window, no top table border
            if (!viewAtEnd)
                logtail.borders[Table::BOTTOM].clear(); // if there are lines below the bottom of the window, no bottom table border



            for (const auto& e : entries)
            {
                Table::tCellArray row;
                Table::Style cellStyle;
                if (viewCountEnabled)
                    row.push_back(SH::FromInt(e.counter));
                if (viewTimestamp)
                    row.push_back(LOG::usToDateTime(e.time));

                string sBGStyle;
                if (viewColoredThreads)
                    sBGStyle = GetColorForThreadID(e.threadID);

                if (ContainsAnsiSequences(e.text))
                {
                    cellStyle = Table::Style(COL_CUSTOM_STYLE);
                }
                else if (viewColorWarningsAndErrors)
                {
                    if (SH::Contains(e.text, "error", false))
                    {
                        cellStyle = Table::Style(COL_RED+sBGStyle);
                    }
                    else if (SH::Contains(e.text, "warning", false))
                    {
                        cellStyle = Table::Style(COL_ORANGE+ sBGStyle);
                    }
                    else
                        cellStyle = Table::Style(sBGStyle);

                }
#ifdef _DEBUG
                validateAnsiSequences(Table::Cell(e.text, cellStyle).StyledOut(e.text.length()));
#endif
                row.push_back(Table::Cell(e.text, cellStyle));

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

        sFeatures += "[F2:Env Vars] [F3:Launch Params]";

        if (sFilter.empty())
        {
            if (textEntryWin.mbVisible)
                positionCaption[ConsoleWin::Position::LB] = "Set Filter:";
            else
                positionCaption[ConsoleWin::Position::LB] = "[CTRL-F:Filter]";
        }
        else
        {
            positionCaption[ConsoleWin::Position::LB] = "Filtering: \"" + sFilter + "\"";
        }

        positionCaption[ConsoleWin::Position::LT] = sFeatures;



        positionCaption[ConsoleWin::Position::RT] = "Log lines (" + SH::FromInt(mTopVisibleRow + 1) + "/" + SH::FromInt(LOG::gLogger.getEntryCount()) + ")";
        positionCaption[ConsoleWin::Position::RB] = "[Wheel][Up/Down][PgUp/PgDown][Home/End]";
    }

    std::string LogWin::GetColorForThreadID(std::thread::id id)
    {
        size_t numeric_id = std::hash<std::thread::id>{}(id);
        size_t index = numeric_id % kThreadCols.size();
        return kThreadCols[index];
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

        int64_t logTotalCounter = (int64_t)LOG::gLogger.getEntryCount();
        if (logTotalCounter > h)
        {
//            ZAttrib bg(MAKE_BG(0xff555555));
//            ZAttrib thumb(MAKE_BG(0xffbbbbbb));
            Rect sb(drawArea.r - 1, drawArea.t, drawArea.r, drawArea.b);
            DrawScrollbar(sb, 0, LOG::gLogger.getEntryCount()-h, mTopVisibleRow, kAttribScrollbarBG, kAttribScrollbarThumb);
        }

        ConsoleWin::RenderToBackBuf(backBuf);
    }

    bool LogWin::OnKey(int keycode, char c)
    {
        bool bHandled = false;
        Rect drawArea;
        GetInnerArea(drawArea);

        bool bCTRLHeld = GetKeyState(VK_CONTROL) & 0x800;


        int64_t entryCount = LOG::gLogger.getEntryCount();

        if (viewAtEnd)
        {
            mTopVisibleRow = entryCount - mHeight;
        }


        if (keycode == VK_UP)
        {
            mTopVisibleRow--;
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
            invalid = true;
            bHandled = true;
        }
        else if (keycode == VK_PRIOR)
        {
            mTopVisibleRow -= mHeight;
            invalid = true;
            bHandled = true;
        }
        else if (keycode == VK_NEXT)
        {
            mTopVisibleRow += mHeight;
            invalid = true;
            bHandled = true;
        }
        else if (keycode == VK_END)
        {
            mTopVisibleRow = entryCount - mHeight;
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
                    textEntryWin.SetArea(Rect(mX+11, mY + mHeight-1, mX + mWidth, mY + mHeight));
                    textEntryWin.SetVisible();
                    invalid = true;
                }
                bHandled = true;
            }
        }

        if (mTopVisibleRow > entryCount - mHeight)
            mTopVisibleRow = entryCount - mHeight;
        if (mTopVisibleRow < 0)
            mTopVisibleRow = 0;

        if (mTopVisibleRow + mHeight >= entryCount)
            viewAtEnd = true;
        else
            viewAtEnd = false;

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


            Rect viewRect(0, 1, w, h);
            Rect logWinRect(viewRect);
            if (textEntryWin.mbVisible)
            {
                logWinRect.b--;
                textEntryWin.SetArea(Rect(logWinRect.l, logWinRect.b, logWinRect.r, logWinRect.b+1));
            }

            logWin.Clear(kAttribHelpBG, true);
            logWin.SetArea(logWinRect);
            logWin.SetEnableFrame();

            helpWin.Clear(kAttribHelpBG, true);
            helpWin.SetArea(viewRect);
            helpWin.SetEnableFrame();
            helpWin.bAutoScrollbar = true;

            UpdateDisplay();
        }
    }

    void CommandLineMonitor::UpdateVisibility()
    {
        if (mbVisible && mbLastVisibleState == false)
        {
            SaveConsoleState();
            if (!textEntryWin.mbVisible)
                cout << "\033[?25l";    // hide cursor

            bScreenInvalid = true;
        }

        if (!mbVisible && mbLastVisibleState == true)
        {
            logWin.SetVisible(false);
            cout << "\033[?25h" << COL_WHITE << COL_BG_BLACK;    // show cursor, reset colors
            RestoreConsoleState();

            LOG::tLogEntries entries;
            LOG::gLogger.tail(ScreenH(), entries);
            for (const auto& entry : entries)
            {
                cout << entry.text << std::endl;
            }
            bScreenInvalid = true;
        }

        //LOG::gLogOut.m_outputToFallback = !mbVisible;
        //LOG::gLogErr.m_outputToFallback = !mbVisible;
        LOG::gLogger.gOutputToFallback = !mbVisible;
        mbLastVisibleState = mbVisible;
    }

    bool CommandLineMonitor::OnKey(int keycode, char c)
    {
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
                bScreenInvalid = true;
                return true;
            }
        }

        if (!bHandled)
        {
            if (keycode == VK_ESCAPE)
            {
                mbDone = true;
                mbVisible = false;
                bScreenInvalid = true;
                return true;
            }
            else if (keycode == VK_F1)
            {
                logWin.mbVisible = !logWin.mbVisible;
                return true;
            }
            else if (keycode == VK_F2)
            {
                ShowEnvVars();
                return true;
            }
            else if (keycode == VK_F3)
            {
                ShowLaunchParams();
                return true;
            }
        }

        return false;
    }

    bool CommandLineMonitor::OnMouse(MOUSE_EVENT_RECORD event)
    {
        COORD coord = event.dwMousePosition;

        if (helpWin.IsOver(coord.X, coord.Y))
        {
            return helpWin.OnMouse(event);
        }

        if (logWin.IsOver(coord.X, coord.Y))
        {
            return logWin.OnMouse(event);
        }

        return false;
        return false;
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
            pCLM->mbVisible = helpWin.mbVisible || logWin.mbVisible;

            pCLM->UpdateVisibility();

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

            if (pCLM->mbVisible)
            {
                pCLM->UpdateFromConsoleSize(bScreenInvalid);  // force update if the screen changed
                pCLM->UpdateDisplay();
                logWin.Update();
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
                        MOUSE_EVENT_RECORD event = inputRecord[i].Event.MouseEvent;
                        event.dwMousePosition.Y--;  // adjust for our coords

                        pCLM->OnMouse(event);
                    }
                    else if (inputRecord[i].EventType == KEY_EVENT && inputRecord[i].Event.KeyEvent.bKeyDown)
                    {
                        pCLM->OnKey(inputRecord[i].Event.KeyEvent.wVirtualKeyCode, inputRecord[i].Event.KeyEvent.uChar.AsciiChar);
                    }
                }
            }
        }

        pCLM->mbVisible = false;
        pCLM->UpdateVisibility();
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
        mode |= ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT;
        mode &= ~(ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE);

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
