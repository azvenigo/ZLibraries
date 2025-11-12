#include <iostream>
#include <fstream>
#include "helpers/CommandLineParser.h"
#include "helpers/CommandLineMonitor.h"
#include "helpers/CommandLineCommon.h"
#include "helpers/InlineFormatter.h"
#include "helpers/RandHelpers.h"
#include "helpers/StringHelpers.h"
#include "helpers/ZZFileAPI.h"
#include <filesystem>
#include <fcntl.h>
#include <windows.h>
#include <io.h>

InlineFormatter gFormatter;

using namespace CLP;
using namespace std;
using namespace ZFile;
using namespace LOG;

bool bVerbose = false;


class ReaderWin : public InfoWin
{
public:
    ReaderWin() : InfoWin() {}

    bool Execute();

    void SetVisible(bool bVisible = true);

    void UpdateCaptions();
    bool OnKey(int keycode, char c);
    bool OnMouse(MOUSE_EVENT_RECORD event);
    void Paint(tConsoleBuffer& backBuf);

    bool LoadFile(std::string filename);
    void SetFilter(std::string filter) { sFilter = filter; invalid = true; }
    bool ReadPipe();

protected:
    bool        bCTRL_S_Hooked = false;
    void        Update();
    bool        UpdateFromConsoleSize(bool bForce = false);
    tStringList GetLines(const string& rawText) const;
    int64_t     CalculateWordsThatFitInWidth(int64_t nLineWidth, const uint8_t* pChars, int64_t nNumChars) const;

    void        DrawToScreen();

    void        UpdateFiltered();
    int64_t     GetFilteredCount();


    bool bQuit = false;
    bool bLastVisibleState = false;
    int64_t     viewTopLine = -1;
    bool        invalid = true;
    bool        highlight = true;
    void        HookCTRL_S(bool bHook = true);

    bool bPiped = false;
    std::string sFilename;
    std::string sFilter;
    tStringList rows;
    tStringList filteredRows;
};

inline bool IsWhitespace(char c)
{
    return  c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == '\v';
}

inline bool IsBreakingChar(char c)
{
    return c == ';' || c == '.' || IsWhitespace(c);
}


int64_t ReaderWin::CalculateWordsThatFitInWidth(int64_t nLineWidth, const uint8_t* pChars, int64_t nNumChars) const
{
    const uint8_t* pEnd = pChars + nLineWidth;
    if (pEnd > pChars + nNumChars)
        pEnd = pChars + nNumChars;

    // Find first nextline if one exists between start and linewidth
    for (const uint8_t* pFind = pChars; pFind < pChars + nLineWidth && pFind < pEnd; pFind++)
    {
        if (*pFind == '\r' || *pFind == '\n')
        {
            pFind++;    // skip

            // If nextchar is combo \r \n skip it too
            if (pFind < pEnd+1 && (*(pFind+1) == '\r' || *(pFind + 1) == '\n'))
                pFind++;

            return pFind - pChars;
        }
    }

    // no nextlines..... if string is larger than nLineWidth, return whole thing
    if (nNumChars < nLineWidth)
        return nNumChars;

    // No nextlines......find the last breaking char
    const uint8_t* pFind = pChars + nLineWidth;
    while (pFind > pChars && !IsBreakingChar(*pFind))
        pFind--;

    if (pFind == pChars)        // no breaking chars found......return a whole line
        return nLineWidth;

    return pFind - pChars+1;    // include breaking char
}


tStringList ReaderWin::GetLines(const string& rawText) const
{
    Rect drawArea;
    GetInnerArea(drawArea);

    assert(drawArea.w() > 0);

    Table::Style style = Table::kDefaultStyle;
    style.wrapping = Table::WORD_WRAP;

    // if text fits in width or there's no wrapping just return a single entry
    if (rawText.empty() || style.wrapping == Table::NO_WRAP)
        return tStringList{ rawText };

    tStringList rows;

    if (style.wrapping == Table::CHAR_WRAP)
    {
        size_t i = 0;
        string sLine;
        do
        {
            if (rawText[i] == '\n' || rawText[i] == '\r')
            {
                rows.push_back(sLine);
                sLine.clear();

                if (i + 1 < rawText.size() && rawText[i + 1] == '\n')   
                    i++;
            }
            else if ((int64_t)sLine.length() >= drawArea.w())
            {
                rows.push_back(sLine);
                sLine.clear();
                sLine += rawText[i];
            }
            else
            {
                sLine += rawText[i];
            }
            i++;
        } while (i < rawText.length());

        if (!sLine.empty())
            rows.push_back(sLine);
    }
    else
    {
        assert(style.wrapping == Table::WORD_WRAP);
        size_t start = 0;
        size_t end = 0;
        do
        {
            end = start + CalculateWordsThatFitInWidth(drawArea.w(), (uint8_t*)&rawText[start], rawText.length() - start);
            rows.push_back(rawText.substr(start, end - start));
            start = end;
            if (start < rawText.length() && rawText[start] == '\r' || rawText[start] == '\n')
                start++;

        } while (end < rawText.length());
    }

    return rows;
}


void ReaderWin::UpdateFiltered()
{
    if (filterTextEntryWin.GetText() == sFilter)
        return;

    sFilter = filterTextEntryWin.GetText();
    invalid = true;

    filteredRows.clear();
    if (sFilter.empty())
        filteredRows = rows;

    int64_t count = 0;
    for (const auto& s : rows)
    {
        if (SH::Contains(s, sFilter, false))
            filteredRows.push_back(s);
    }
}

int64_t ReaderWin::GetFilteredCount()
{
    if (invalid)
        UpdateFiltered();

    return filteredRows.size();
}


bool ReaderWin::LoadFile(std::string filename)
{
    tZFilePtr pFile;
    if (!ZFileBase::Open(filename, pFile))
    {
        if (pFile)
            OUT_ERR("Failed to open file: " << filename << " error:" << pFile->GetLastError() << "\n")
        else
            OUT_ERR("Failed to open file: " << filename << "\n")

        return false;
    }

    vector<uint8_t> buf(pFile->GetFileSize());
    if (!pFile->Read(&buf[0], buf.size()))
    {
        OUT_ERR("Failed to read from file: " << filename << " error:" << pFile->GetLastError() << "\n")
        return false;
    }

    mText.assign((char*)&buf[0], buf.size());
    sFilename = filename;

    return true;
}

bool ReaderWin::ReadPipe()
{
    if (_isatty(_fileno(stdin)))
    {
        return false;
    }

    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD type = GetFileType(h);

    std::stringstream buf;
    char tmp[4096];

    if (type == FILE_TYPE_PIPE)
    {
        DWORD bytesAvail = 0;

        int waits = 10;
        int waitMS = 10;

        while (waits > 0)
        {
            if (PeekNamedPipe(h, nullptr, 0, nullptr, &bytesAvail, nullptr) && bytesAvail > 0)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(waitMS));
        }

        if (bytesAvail == 0)
            return false;

        // read until EOF
        while (std::cin.read(tmp, sizeof(tmp)) || std::cin.gcount() > 0)
            buf.write(tmp, std::cin.gcount());
    }
    else if (type == FILE_TYPE_DISK)
    {
        // redirected file input, just read it
        while (std::cin.read(tmp, sizeof(tmp)) || std::cin.gcount() > 0)
            buf.write(tmp, std::cin.gcount());
    }
    else
    {
        return false;
    }

    bPiped = true;
    mText = buf.str();
    return !mText.empty();
}


void ReaderWin::DrawToScreen()
{
    // clear back buffer
    Paint(gConsole.BackBuffer());

    if (filterTextEntryWin.mbVisible)
        filterTextEntryWin.Paint(gConsole.BackBuffer());
    if (helpTableWin.mbVisible)
        helpTableWin.Paint(gConsole.BackBuffer());

    gConsole.Render();
}


bool ReaderWin::Execute()
{
    if (!gConsole.Init())
    {
        cout << "Failed to initialize.....aborting\n";
        return false;
    }


    // Main loop to read input events
    INPUT_RECORD inputRecord[128];
    DWORD numEventsRead;


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Clear the raw command buffer and param buffers
    //UpdateFromConsoleSize(true);

    filterTextEntryWin.SetText(sFilter);


    Init(Rect(0, 1, gConsole.Width(), gConsole.Height()));

    ZAttrib kReaderBG(0xFF000000FFff00ff);

    Clear(kReaderBG, false);
    SetEnableFrame();
    bAutoScrollbar = true;

//    mbVisible = true;

    rows = GetLines(mText);
    if (sFilter.empty())
        filteredRows = rows;
    UpdateFiltered();
    Update();

    // for longer and longer idles when the application has no activity
    const uint64_t kIdleInc = 5000;
    const uint64_t kIdleMin = 100;
    const uint64_t kIdleMax = 250000; // 250ms

    uint64_t idleSleep = kIdleMin;

    while (!mbDone)
    {
        gConsole.SetCursorVisible(filterTextEntryWin.mbVisible);

        bool bForeground = gConsole.ConsoleHasFocus();
        if (bForeground)
        {
            if (filterTextEntryWin.mbVisible)
            {
                filterTextEntryWin.HookHotkeys();
            }
            else
            {
                filterTextEntryWin.UnhookHotkeys();
            }
        }

        if (UpdateFromConsoleSize())
        {
            rows = GetLines(mText); // recompute rows
            UpdateFiltered();
            UpdateCaptions();
        }


        if (gConsole.Invalid())
        {
            DrawToScreen();
            Update();
            idleSleep = kIdleMin;
        }
        else
        {
            idleSleep += kIdleInc;
        }

        // Check for hotkey events
        MSG msg;
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            idleSleep = kIdleMin;
            if (msg.message == WM_HOTKEY)
            {
                if (msg.wParam == CTRL_V_HOTKEY || msg.wParam == SHIFT_INSERT_HOTKEY)
                {
                    if (filterTextEntryWin.mbVisible)
                    {
                        filterTextEntryWin.AddUndoEntry();
                        filterTextEntryWin.HandlePaste(GetTextFromClipboard());
                    }
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
            idleSleep = kIdleMin;

            for (DWORD i = 0; i < numEventsRead; i++)
            {
                if (!ReadConsoleInput(gConsole.InputHandle(), inputRecord, 1, &numEventsRead))
                {
                    cerr << "Failed to read console input." << endl;
                    return false;
                }

                // Work on getting sizing stride fix
                /*
                if (inputRecord[i].EventType == WINDOW_BUFFER_SIZE_EVENT)
                {
                    gConsole.UpdateScreenInfo();
                    invalid = true;
                }*/

                if (inputRecord[i].EventType == MOUSE_EVENT)
                {
                    MOUSE_EVENT_RECORD event = inputRecord[i].Event.MouseEvent;
                    event.dwMousePosition.Y--;  // adjust for our coords

                    OnMouse(event);
                }
                else if (inputRecord[i].EventType == KEY_EVENT && inputRecord[i].Event.KeyEvent.bKeyDown)
                {
                    int keycode = inputRecord[i].Event.KeyEvent.wVirtualKeyCode;
                    char c = inputRecord[i].Event.KeyEvent.uChar.AsciiChar;
                    OnKey(keycode, c);
                }
            }
        }

        if (idleSleep < kIdleMin)
            idleSleep = kIdleMin;
        if (idleSleep > kIdleMax)
            idleSleep = kIdleMax;

        std::this_thread::sleep_for(std::chrono::microseconds(idleSleep));

    }

    //RestoreConsoleState();
    gConsole.Shutdown();

    if (bPiped)
        cout << mText;
    return true;
}



void ReaderWin::SetVisible(bool bVisible)
{
    if (mbVisible != bVisible)
    {
        invalid = true;

        // if log window is visible, hook CTRL-S for saving (otherwise it is a suspend command to a console app)
        HookCTRL_S(bVisible);
    }

    mbVisible = bVisible;
}

void ReaderWin::HookCTRL_S(bool bHook)
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


void ReaderWin::Update()
{
    UpdateFiltered();

    Rect drawArea;
    GetInnerArea(drawArea);
    int64_t filteredCount = GetFilteredCount();
    if (mTopVisibleRow > filteredCount - drawArea.h())
        mTopVisibleRow = filteredCount - drawArea.h();
    if (mTopVisibleRow < 0)
        mTopVisibleRow = 0;

    
    if (viewTopLine != mTopVisibleRow)
    {
        viewTopLine = mTopVisibleRow;
        invalid = true;
    }

    if (!mbVisible || !invalid)
        return;

//    positionCaption[ConsoleWin::Position::LT] = "Reader";
    if (bPiped)
        positionCaption[ConsoleWin::Position::LT] = "Piped Text";
    else
        positionCaption[ConsoleWin::Position::LT] = "File: " + sFilename;

    UpdateCaptions();
}

void ReaderWin::UpdateCaptions()
{
    Rect drawArea;
    GetInnerArea(drawArea);

    int64_t entryCount = GetFilteredCount();
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
        if (highlight)
            sHighlightCaption = "[CTRL-H:Highlight ON]";
        else
            sHighlightCaption = "[CTRL-H:Highlight OFF]";
        positionCaption[ConsoleWin::Position::RT] = "Filtered: " + sFilter + " (" + SH::FromInt(viewing) + "-" + SH::FromInt(bottom) + "/" + SH::FromInt(entryCount) + ")";
    }

    positionCaption[ConsoleWin::Position::LB] = "[CTRL-F:Filter]" + sHighlightCaption;
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

bool ReaderWin::OnMouse(MOUSE_EVENT_RECORD event)
{
    COORD localcoord = event.dwMousePosition;
    localcoord.X -= (SHORT)mX;
    localcoord.Y -= (SHORT)mY;

    if (helpTableWin.IsOver(localcoord.X, localcoord.Y))
    {
        return helpTableWin.OnMouse(event);
    }

    if (event.dwEventFlags == MOUSE_WHEELED)
    {
        SHORT wheelDelta = HIWORD(event.dwButtonState);
        if (wheelDelta < 0)
        {
            mTopVisibleRow += mHeight / 4;
        }
        else
        {
            mTopVisibleRow -= mHeight / 4;
        }
        invalid = true;
        gConsole.Invalidate();

        Update();
    }

    return ConsoleWin::OnMouse(event);
}

bool ReaderWin::OnKey(int keycode, char c)
{
    bool bHandled = false;
    Rect drawArea;
    GetInnerArea(drawArea);

    bool bCTRLHeld = GetKeyState(VK_CONTROL) & 0x800;
    int64_t entryCount = GetFilteredCount();

    gConsole.Invalidate();

    if (filterTextEntryWin.mbVisible && !bHandled)
        bHandled = filterTextEntryWin.OnKey(keycode, c);
    if (helpTableWin.mbVisible && !bHandled)
        bHandled = helpTableWin.OnKey(keycode, c);

    if (filterTextEntryWin.mbVisible)
    {
        if (filterTextEntryWin.mbCanceled)
            filterTextEntryWin.SetText("");

        if (filterTextEntryWin.mbCanceled || filterTextEntryWin.mbDone)
        {
            filterTextEntryWin.SetVisible(false);
            filterTextEntryWin.mbCanceled = false;
            filterTextEntryWin.mbDone = false;
            invalid = true;
        }
    }

    if (helpTableWin.mbCanceled || helpTableWin.mbDone)
    {
        helpTableWin.SetVisible(false);
        helpTableWin.mbCanceled = false;
        helpTableWin.mbDone = false;
        invalid = true;
    }

    if (!bHandled)
    {
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
            mTopVisibleRow -= drawArea.h();
            invalid = true;
            bHandled = true;
        }
        else if (keycode == VK_NEXT)
        {
            mTopVisibleRow += drawArea.h();
            invalid = true;
            bHandled = true;
        }
        else if (keycode == VK_END)
        {
            mTopVisibleRow = entryCount - drawArea.h();
            invalid = true;
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
                    invalid = true;
                }
                bHandled = true;
            }
        }
        else if (keycode == 'h' || keycode == 'H')
        {
            if (bCTRLHeld)
            {
                highlight = !highlight;
                invalid = true;
            }
        }
        else if (keycode == VK_ESCAPE)
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

    Update();
    return bHandled;
}

void ReaderWin::Paint(tConsoleBuffer& backBuf)
{
    if (!mbVisible || !invalid)
        return;

    ConsoleWin::BasePaint();


    int64_t filteredCount = GetFilteredCount();
    int64_t firstDrawRow = mTopVisibleRow - 1;

    Rect drawArea;
    GetInnerArea(drawArea);

    tStringList::iterator it = filteredRows.begin();
    int64_t startrow = 0;
    while (startrow < mTopVisibleRow && it != filteredRows.end())
    {
        startrow++;
        it++;
    }


    int64_t drawrow = drawArea.t;
    int64_t endrow = drawArea.b;
    while (drawrow < endrow && it != filteredRows.end())
    {
        if (drawrow < mHeight - 1 && it != filteredRows.end())
        {
            string s = *it;

            if (highlight && !sFilter.empty())
            {
                size_t i = 0;
                size_t filtLen = sFilter.length();
                size_t colLen = string(COL_YELLOW).length();
                while (i < s.length())
                {
                    if (SH::Compare(s.substr(i, filtLen), sFilter, false))
                    {
                        s.insert(i + filtLen, COL_RESET);
                        s.insert(i, COL_YELLOW);
                        i += filtLen + colLen * 2;
                    }
                    i++;
                }
            }

            DrawClippedAnsiText(Rect(drawArea.l, drawrow, drawArea.r, drawrow + 1), s, false, &drawArea);
            drawrow++;
            it++;
        }
    }

    if (filteredCount > drawArea.h())
    {
        Rect sb(drawArea.r, drawArea.t, drawArea.r+1, drawArea.b);    // drawArea is reduced by 1 for scrollbar
        DrawScrollbar(sb, 0, filteredCount - drawArea.h(), mTopVisibleRow, kAttribScrollbarBG, kAttribScrollbarThumb);
        drawArea.r--;
    }

    ConsoleWin::RenderToBackBuf(backBuf);
    gConsole.Invalidate();
    invalid = false;
}


bool ReaderWin::UpdateFromConsoleSize(bool bForce)
{
    if (bForce || gConsole.ScreenChanged())
    {
        gConsole.UpdateNativeConsole();
        invalid = true;

        int64_t w = gConsole.Width();
        int64_t h = gConsole.Height();


        if (w < 1)
            w = 1;
        if (h < 8)
            h = 8;


        Rect viewRect(0, 0, w, h);
        SetArea(viewRect);

        helpTableWin.Clear(kAttribHelpBG, true);
        helpTableWin.SetArea(viewRect);
        helpTableWin.SetEnableFrame();
        helpTableWin.bAutoScrollbar = true;


        if (filterTextEntryWin.mbVisible)
            filterTextEntryWin.SetArea(Rect(mX + 11, mY + mHeight - 3, mX + mWidth - 11, mY + mHeight));

        Update();
        return true;
    }

    return false;
}



int main(int argc, char* argv[])
{
/*    Table t;
    Table::SetDecLineBorders(t);
    t.AddRow("row 1", "row 2");
    t.AddSeparator();
    t.AddRow("one long row");
    t.AddSeparator();
    t.AddRow("row 1", "row 2", "row 3", "row 4444444444");
    t.AddSeparator();
    t.AddRow("one long row");
    t.AddSeparator();
    t.AddRow("row 1", "row 2");
    t.AddSeparator();
    t.AddRow("row 1", "row 2", "row 3");

    t.SetRenderWidth(100);

    cout << t;


    return 0;
    */


    ReaderWin readerWin;
    CommandLineParser parser;
    string sFilename;
    string sFilter;
    int64_t nStartLineNumber = 0;

    parser.RegisterAppDescription("Reader application for scrolling through or searching text files or piped input.");
    parser.RegisterParam(ParamDesc("PATH", &sFilename, CLP::kOptional | CLP::kExistingPath, "Optional path to read from if no piped input."));
    parser.RegisterParam(ParamDesc("FILTER", &sFilter, CLP::kNamed | CLP::kOptional, "Filter lines including this text."));
    parser.RegisterParam(ParamDesc("start", &nStartLineNumber, CLP::kNamed | CLP::kOptional, "Starting line number"));

    if (readerWin.ReadPipe())
    {
        readerWin.Execute();
    }
    else
    {
        bool bParseSuccess = parser.Parse(argc, argv);
        if (!bParseSuccess)
        {
            return -1;
        }

        if (!sFilename.empty() && sFilename[0] != '<') // temporarily detect pipe in via commandline param for vs debugging
        {
            readerWin.LoadFile(sFilename);

            if (!sFilter.empty())
                readerWin.SetFilter(sFilter);

            readerWin.Execute();
        }
        else
        {
            parser.ShowCommandLineHelp();
        }
    }

    return 0;
}
