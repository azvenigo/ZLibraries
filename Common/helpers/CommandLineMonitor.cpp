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
    TextEditWin filterTextEntryWin;
    TextEditWin saveLogFilenameEntryWin;
//    InfoWin     helpWin;        // popup help window


    void LogWin::SetVisible(bool bVisible)
    {
        if (mbVisible != bVisible)
        {
            mbWindowInvalid = true;

            // if log window is visible, hook CTRL-S for saving (otherwise it is a suspend command to a console app)
            HookCTRL_S(bVisible);
        }

        mbVisible = bVisible;
    }

    void LogWin::HookCTRL_S(bool bHook)
    {
        if (bHook == bCTRL_S_Hooked)
            return;

        if (bHook)
        {
            if (!RegisterHotKey(nullptr, CTRL_S_HOTKEY, MOD_CONTROL, 'S'))
            {
                std::cerr << "Error registering hotkey:" << GetLastError() << std::endl;
                return;
            }
            bCTRL_S_Hooked = true;
        }
        else
        {
            UnregisterHotKey(nullptr, CTRL_S_HOTKEY);
            bCTRL_S_Hooked = false;
        }
    }


    void LogWin::Update()
    {
        if (!mbVisible)
            return;

        if (lastReportedLogCount != LOG::gLogger.getEntryCount())
        {
            lastReportedLogCount = LOG::gLogger.getEntryCount();
            mbWindowInvalid = true;
        }

        if (filterTextEntryWin.GetText() != sFilter)
        {
            sFilter = filterTextEntryWin.GetText();
            LOG::gLogger.setFilter(sFilter);
            mbWindowInvalid = true;
        }

        if (saveLogFilenameEntryWin.GetText() != sLogFilename)
        {
            sLogFilename = saveLogFilenameEntryWin.GetText();
            mbWindowInvalid = true;
        }

        if (mbWindowInvalid)
        {
            Rect drawArea;
            GetInnerArea(drawArea);
            int64_t drawWidth = drawArea.r - drawArea.l;
            int64_t drawHeight = drawArea.b - drawArea.t;
            if (filterTextEntryWin.mbVisible || saveLogFilenameEntryWin.mbVisible)
                drawHeight--;


            LOG::tLogEntries entries;
                
            int64_t firstRowCounter = -1;
            int64_t lastRowCounter = -1;

            if (viewAtEnd)
            {
                mTopVisibleRow = LOG::gLogger.getEntryCount() - drawHeight;
                if (mTopVisibleRow < 0)
                    mTopVisibleRow = 0;
                LOG::gLogger.tail(drawHeight-2, entries); // -2 for the header and bottom line indicating end
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


            ZAttrib bg(MAKE_BG(0xFF222222));
            ZAttrib tableBorder(bg);
            tableBorder.dec_line = true;
            Table logtail;
            logtail.borders[Table::LEFT].clear();
            logtail.borders[Table::RIGHT].clear();
            logtail.borders[Table::CENTER] = bg.ToAnsi() + string(" ");


            if (firstRowCounter == 0)
                logtail.borders[Table::TOP] = tableBorder.ToAnsi() + "\xb1";
            else
                logtail.borders[Table::TOP].clear();    // if there are lines above the top of the window, no top table border

            if (viewAtEnd)
            {
                logtail.borders[Table::BOTTOM] = tableBorder.ToAnsi() + "\xb1";
            }
            else
                logtail.borders[Table::BOTTOM].clear(); // if there are lines below the bottom of the window, no bottom table border


            // Table header
            Table::Style headerStyle(COL_ORANGE + bg.ToAnsi(), false, Table::CENTER, Table::NO_WRAP, 0, '-');
//            Table::Style headerStyleRight(COL_ORANGE, Table::RIGHT,0, '-');
            Table::tCellArray header;
            if (viewCountEnabled)
                header.push_back(Table::Cell("#", headerStyle));
            if (viewDateTime != 0)
                header.push_back(Table::Cell(TimeLabel(), headerStyle));
            header.push_back(Table::Cell("ENTRY", headerStyle));

            logtail.AddRow(header);




            Table::Style counterStyle(ZAttrib(MAKE_BG(0xFF222222)), false, Table::RIGHT, Table::NO_WRAP);

            Table::Style timeStyle(ZAttrib(MAKE_BG(0xFF222222)), false, Table::RIGHT, Table::NO_WRAP);

            int64_t prevEntryTime = LOG::gLogger.gLogStartTime;
            for (const auto& e : entries)
            {
                Table::tCellArray row;
                Table::Style cellStyle;
                if (viewCountEnabled)
                    row.push_back(Table::Cell(SH::FromInt(e.counter), counterStyle));

                if (viewDateTime != 0)
                {
                    row.push_back(Table::Cell(TimeValue(e.time, prevEntryTime), timeStyle));
                }

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
                row.push_back(Table::Cell(e.text, cellStyle));

                if (e.text == "#")
                    int stophere = 5;

                logtail.AddRow(row);
                prevEntryTime = e.time;
            }

            logtail.AlignWidth(drawWidth, logtail);
            mText = (string)logtail;
#ifdef _DEBUG
            validateAnsiSequences(mText);
#endif

            UpdateCaptions();
            gConsole.Invalidate();
        }

        mbWindowInvalid = false;
    }

    void LogWin::UpdateCaptions()
    {
        string sFeatures;
        if (viewCountEnabled)
            sFeatures += "[1:Count  ON] ";
        else
            sFeatures += "[1:Count OFF] ";

        sFeatures += "[2:" + TimeLabel() + "] ";

        if (viewColoredThreads)
            sFeatures += "[3:Threads  ON] ";
        else
            sFeatures += "[3:Threads OFF] ";

        if (viewColorWarningsAndErrors)
            sFeatures += "[4:Warn/Err  ON] ";
        else
            sFeatures += "[4:Warn/Err OFF] ";

        sFeatures += "[F2:Environment]";

        positionCaption[ConsoleWin::Position::LT] = sFeatures;

        Rect drawArea;
        GetInnerArea(drawArea);

        int64_t entryCount = LOG::gLogger.getEntryCount();
        int64_t viewing = mTopVisibleRow + 1;
        if (viewing > entryCount)
            viewing = entryCount;
        int64_t bottom = mTopVisibleRow + drawArea.h();
        if (entryCount < drawArea.h())
            bottom = entryCount;


        string sHighlightCaption;
        if (sFilter.empty())
        {
            positionCaption[ConsoleWin::Position::RT] = "Total Lines (" + SH::FromInt(viewing) + "-" + SH::FromInt(bottom) + "/" + SH::FromInt(entryCount) + ")";
        }
        else
        {
            positionCaption[ConsoleWin::Position::RT] = "Filtered: " + sFilter + " (" + SH::FromInt(viewing) + "-" + SH::FromInt(bottom) + "/" + SH::FromInt(entryCount) + ")";
        }

        positionCaption[ConsoleWin::Position::LB] = "[CTRL-F:Filter]";
        if (filterTextEntryWin.mbVisible)
        {
            filterTextEntryWin.positionCaption[ConsoleWin::Position::LT] = "Filter:";
            filterTextEntryWin.positionCaption[ConsoleWin::Position::RB] = "[ESC-Clear Filter][ENTER-Set Filter]";
            positionCaption[ConsoleWin::Position::LB].clear();
            positionCaption[ConsoleWin::Position::RB].clear();
        }


        if (!filterTextEntryWin.mbVisible)
            positionCaption[ConsoleWin::Position::RB] = "[Wheel][Up/Down][PgUp/PgDown][Home/End]";
    }

    string LogWin::GetColorForThreadID(std::thread::id id)
    {
        size_t numeric_id = std::hash<std::thread::id>{}(id);
        size_t index = numeric_id % kThreadCols.size();
        return kThreadCols[index];
    }

    string LogWin::TimeLabel()
    {
        switch (viewDateTime)
        {
        case kNone: return "Time Options";
        case kTime: return "TIME";
        case kDateTime: return "DATE/TIME";
        case kTimeElapsed: return "ELAPSED (s)";
        case kTimeFromPrevious: return "DELTA (us)";
        case kTimeSince: return "SINCE (s)";
        }

        return "unknown";
    }

    string LogWin::TimeValue(int64_t entryTime, int64_t prevEntryTime)
    {
        switch (viewDateTime)
        {
        case kTime:
        {
            string date;
            string time;
            LOG::usToDateTime(entryTime, date, time);
            return time;
        }
        case kDateTime:
        {
            string date;
            string time;
            LOG::usToDateTime(entryTime, date, time);
            return date + " " + time;
        }
        case kTimeElapsed:
        {
            return LOG::usToElapsed(entryTime - LOG::gLogger.gLogStartTime);
        }
        case kTimeFromPrevious:
        {
            return "+"+SH::FromInt(entryTime - prevEntryTime);
        }
        case kTimeSince:
        {
            int64_t curTime = std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1);
            return "-"+LOG::usToElapsed(curTime - entryTime);
        }
        }

        return "";
    }



    void LogWin::Paint(tConsoleBuffer& backBuf)
    {
        if (!mbVisible)
            return;

        ConsoleWin::BasePaint();





        int64_t firstDrawRow = mTopVisibleRow - 1;

        Rect drawArea;
        GetInnerArea(drawArea);
        Rect textArea = GetTextOuputRect(mText);
        textArea.offset(drawArea.l, drawArea.t);
        int64_t logTotalCounter = (int64_t)LOG::gLogger.getEntryCount();
        if (logTotalCounter > drawArea.h())
        {
//            ZAttrib bg(MAKE_BG(0xff555555));
//            ZAttrib thumb(MAKE_BG(0xffbbbbbb));
            Rect sb(drawArea.r, drawArea.t, drawArea.r+1, drawArea.b);
            DrawScrollbar(sb, 0, LOG::gLogger.getEntryCount()- drawArea.h(), mTopVisibleRow, kAttribScrollbarBG, kAttribScrollbarThumb);
            drawArea.r--;
        }

        DrawClippedAnsiText(textArea, mText, true, &drawArea);
        

/*        ZAttrib test(0xff005500ff55ffff);
        test.dec_line = true;


        int x = 0;
        int y = 0;
        for (int i = 96; i < 256; i++)
        {
            DrawAnsiChar(x, y, (char)i, test);
            x++;
            if (x > mWidth)
            {
                x = 0;
                y++;
            }
        }
*/











        ConsoleWin::RenderToBackBuf(backBuf);
    }

    void LogWin::ShowSaveFilenamePrompt()
    {
        if (!saveLogFilenameEntryWin.mbVisible)
        {
            saveLogFilenameEntryWin.SetVisible(true);
            saveLogFilenameEntryWin.Clear(ZAttrib(0xff666699ffffffff));
            saveLogFilenameEntryWin.SetArea(Rect(logWin.mX + 15, mY + mHeight - 1, mX + mWidth, mY + mHeight));
            saveLogFilenameEntryWin.SetVisible();
            mbWindowInvalid = true;
        }
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
            mbWindowInvalid = true;
            bHandled = true;
        }
        else if (keycode == VK_DOWN)
        {
            mTopVisibleRow++;
            mbWindowInvalid = true;
            bHandled = true;
        }
        else if (keycode == VK_HOME)
        {
            mTopVisibleRow = 0;
            mbWindowInvalid = true;
            bHandled = true;
        }
        else if (keycode == VK_PRIOR)
        {
            mTopVisibleRow -= mHeight;
            mbWindowInvalid = true;
            bHandled = true;
        }
        else if (keycode == VK_NEXT)
        {
            mTopVisibleRow += mHeight;
            mbWindowInvalid = true;
            bHandled = true;
        }
        else if (keycode == VK_END)
        {
            mTopVisibleRow = entryCount - mHeight;
            mbWindowInvalid = true;
            bHandled = true;
        }
        else if (keycode == '1')
        {
            viewCountEnabled = !viewCountEnabled;
            mbWindowInvalid = true;
            bHandled = true;
        }
        else if (keycode == '2')
        {
            viewDateTime = (viewDateTime + 1) % 6;  // rotate over possible options
            mbWindowInvalid = true;
            bHandled = true;
        }
        else if (keycode == '3')
        {
            viewColoredThreads = !viewColoredThreads;
            mbWindowInvalid = true;
            bHandled = true;
        }
        else if (keycode == '4')
        {
            viewColorWarningsAndErrors = !viewColorWarningsAndErrors;
            mbWindowInvalid = true;
            bHandled = true;
        }
        else if (keycode == 'f' || keycode == 'F')
        {
            if (bCTRLHeld)
            {
                if (!filterTextEntryWin.mbVisible)
                {
                    filterTextEntryWin.Clear(ZAttrib(0xff666699ffffffff));
                    filterTextEntryWin.SetArea(Rect(mX + 11, mY + mHeight - 3, mX + mWidth - 11, mY + mHeight));
                    filterTextEntryWin.SetEnableFrame();
                    filterTextEntryWin.SetVisible();
                    mbWindowInvalid = true;
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
        if (logWin.mbVisible)
            logWin.Paint(gConsole.BackBuffer());
        if (filterTextEntryWin.mbVisible)
            filterTextEntryWin.Paint(gConsole.BackBuffer());
        if (saveLogFilenameEntryWin.mbVisible)
            saveLogFilenameEntryWin.Paint(gConsole.BackBuffer());
        if (helpTableWin.mbVisible)
            helpTableWin.Paint(gConsole.BackBuffer());

        gConsole.Render();
    }

    bool CommandLineMonitor::UpdateFromConsoleSize(bool bForce)
    {
        if (bForce || gConsole.UpdateNativeConsole())
        {
            int64_t w = gConsole.Width();
            int64_t h = gConsole.Height();

            if (w < 1)
                w = 1;
            if (h < 8)
                h = 8;

            Rect viewRect(0, 0, w, h);


            Rect logWinRect(viewRect);

            if (filterTextEntryWin.mbVisible)
                filterTextEntryWin.SetArea(Rect(11, h - 3, w - 11, h));

            if (filterTextEntryWin.mbVisible)
            {
                logWinRect.b--;
                filterTextEntryWin.SetArea(Rect(logWinRect.l, logWinRect.b, logWinRect.r, logWinRect.b+1));
            }
            else if (saveLogFilenameEntryWin.mbVisible)
            {
                logWinRect.b--;
                saveLogFilenameEntryWin.SetArea(Rect(logWinRect.l, logWinRect.b, logWinRect.r, logWinRect.b + 1));
            }

            logWin.Clear(kAttribHelpBG, true);
            logWin.SetArea(logWinRect);
            logWin.SetEnableFrame();

            helpTableWin.Clear(kAttribHelpBG, true);
            helpTableWin.SetArea(viewRect);
            helpTableWin.SetEnableFrame();
            helpTableWin.bAutoScrollbar = true;

            return true;
           
        }

        return false;
    }

    void CommandLineMonitor::UpdateVisibility()
    {
        if (mbVisible && mbLastVisibleState == false)
        {
            gConsole.Init();
        }

        if (!mbVisible && mbLastVisibleState == true)
        {
            logWin.SetVisible(false);
            cout << "\033[?25h" << COL_WHITE << COL_BG_BLACK << DEC_LINE_END;    // show cursor, reset colors, ensure dec line mode is off
            int64_t height = gConsole.Height();
            gConsole.Shutdown();

            LOG::tLogEntries entries;
            LOG::gLogger.tail(height, entries);
            for (const auto& entry : entries)
            {
                cout << entry.text << std::endl;
            }
        }

        //LOG::gLogOut.m_outputToFallback = !mbVisible;
        //LOG::gLogErr.m_outputToFallback = !mbVisible;
        LOG::gLogger.gOutputToFallback = !mbVisible;
        mbLastVisibleState = mbVisible;
    }

    bool CommandLineMonitor::OnKey(int keycode, char c)
    {
        gConsole.Invalidate();

        bool bHandled = false;
//        if (helpTextWin.mbVisible)
//            bHandled = helpTextWin.OnKey(keycode, c);
        if (helpTableWin.mbVisible)
            bHandled = helpTableWin.OnKey(keycode, c);
        if (filterTextEntryWin.mbVisible && !bHandled)
            bHandled = filterTextEntryWin.OnKey(keycode, c);
        if (saveLogFilenameEntryWin.mbVisible && !bHandled)
            bHandled = saveLogFilenameEntryWin.OnKey(keycode, c);
        if (logWin.mbVisible && !bHandled)
            bHandled = logWin.OnKey(keycode, c);

        if (filterTextEntryWin.mbVisible)
        {
            if (filterTextEntryWin.mbCanceled)
                filterTextEntryWin.SetText("");

            if (filterTextEntryWin.mbCanceled || filterTextEntryWin.mbDone)
            {
                filterTextEntryWin.SetVisible(false);
                filterTextEntryWin.mbCanceled = false;
                filterTextEntryWin.mbDone = false;
                return true;
            }
        }
        if (saveLogFilenameEntryWin.mbVisible)
        {
            if (saveLogFilenameEntryWin.mbCanceled || saveLogFilenameEntryWin.mbDone)
            {
                saveLogFilenameEntryWin.SetVisible(false);
                saveLogFilenameEntryWin.mbCanceled = false;
                saveLogFilenameEntryWin.mbDone = false;
                return true;
            }
        }

        if (!bHandled)
        {
            if (keycode == VK_ESCAPE)
            {
                mbDone = true;
                mbVisible = false;
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
        }

        return false;
    }

    bool CommandLineMonitor::OnMouse(MOUSE_EVENT_RECORD event)
    {
        COORD coord = event.dwMousePosition;

        if (helpTableWin.IsOver(coord.X, coord.Y))
        {
            gConsole.Invalidate();
            return helpTableWin.OnMouse(event);
        }

        if (logWin.IsOver(coord.X, coord.Y))
        {
            gConsole.Invalidate();
            return logWin.OnMouse(event);
        }

        return false;
    }


    void CommandLineMonitor::ThreadProc(CommandLineMonitor* pCLM)
    {
        // Main loop to read input events
        INPUT_RECORD inputRecord[128];
        DWORD numEventsRead;

        int64_t w = gConsole.Width();
        int64_t h = gConsole.Height();


        std::vector<CHAR_INFO> blank(w * h);
        for (int i = 0; i < blank.size(); i++)
        {
            blank[i].Char.AsciiChar = ' ';
            blank[i].Attributes = 0;
        }
        //SMALL_RECT smallrect(0, 0, w, h);

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Clear the raw command buffer and param buffers
        pCLM->UpdateFromConsoleSize(true);

        while (!pCLM->mbDone && !pCLM->mbCanceled)
        {
            pCLM->mbVisible = helpTableWin.mbVisible || /*helpTextWin.mbVisible ||*/ logWin.mbVisible;

            pCLM->UpdateVisibility();

            gConsole.SetCursorVisible(filterTextEntryWin.mbVisible);


            if (gConsole.ConsoleHasFocus())
            {
                if (filterTextEntryWin.mbVisible || saveLogFilenameEntryWin.mbVisible)
                {
                    filterTextEntryWin.HookHotkeys();
                }
                else
                {
                    filterTextEntryWin.UnhookHotkeys();
                }
            }

            logWin.HookCTRL_S(pCLM->mbVisible);



            if (pCLM->mbVisible)
            {
                pCLM->UpdateFromConsoleSize();  // force update if the screen changed
                gConsole.UpdateNativeConsole();
                pCLM->UpdateDisplay();
                logWin.Update();
            }

            // Check for hotkey events
            MSG msg;
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_HOTKEY)
                {
                    if (msg.wParam == CTRL_V_HOTKEY || msg.wParam == SHIFT_INSERT_HOTKEY)
                    {
                        if (filterTextEntryWin.mbVisible)
                        {
                            filterTextEntryWin.AddUndoEntry();
                            filterTextEntryWin.HandlePaste(GetTextFromClipboard());
                        }
                        else if (saveLogFilenameEntryWin.mbVisible)
                        {
                            saveLogFilenameEntryWin.AddUndoEntry();
                            saveLogFilenameEntryWin.HandlePaste(GetTextFromClipboard());
                        }
                    }
                    else if (msg.wParam == CTRL_S_HOTKEY)
                    {
                        logWin.ShowSaveFilenamePrompt();
                    }
                }
                else
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }

            if (PeekConsoleInput(gConsole.InputHandle(), inputRecord, 128, &numEventsRead) && numEventsRead > 0)
            {
                for (DWORD i = 0; i < numEventsRead; i++)
                {
                    if (!ReadConsoleInput(gConsole.InputHandle(), inputRecord, 1, &numEventsRead))
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

        gConsole.Init();

        monitorthread = thread(ThreadProc, this);
    }

    void CommandLineMonitor::End()
    {
        mbDone = true;
        monitorthread.join();
    }

};

#endif // ENABLE_CLM
