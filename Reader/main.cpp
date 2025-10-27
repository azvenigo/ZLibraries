#include <iostream>
#include <fstream>
#include "helpers/CommandLineParser.h"
#include "helpers/CommandLineMonitor.h"
#include "helpers/InlineFormatter.h"
#include "helpers/RandHelpers.h"
#include "helpers/StringHelpers.h"
#include "helpers/ZZFileAPI.h"
#include <filesystem>

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
    int64_t     GetFilteredCount();


    bool bQuit = false;
    bool bLastVisibleState = false;
    std::string viewFilename;
    int64_t     viewTopLine = -1;
    bool        invalid = true;
    void        HookCTRL_S(bool bHook = true);

    std::string sFilename;
    std::string sFilter;
    tStringList rows;

    tConsoleBuffer backBuffer;      // for double buffering
    tConsoleBuffer drawStateBuffer; // for rendering only delta

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
        } while (end < rawText.length());
    }

    return rows;
}

int64_t ReaderWin::GetFilteredCount()
{
    if (sFilter.empty())
        return rows.size();

    int64_t count = 0;
    for (const auto& s : rows)
    {
        if (SH::Contains(s, sFilter, false))
            count++;
    }

    return count;
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
    std::stringstream buf;
    buf << cin.rdbuf();
    mText = buf.str();

    return true;
}


void ReaderWin::DrawToScreen()
{
    // clear back buffer
    Paint(backBuffer);

    if (filterTextEntryWin.mbVisible)
        filterTextEntryWin.Paint(backBuffer);
    if (helpTableWin.mbVisible)
        helpTableWin.Paint(backBuffer);


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
    if (filterTextEntryWin.mbVisible)
    {
        cout << "\033[?25h";    // visible cursor
    }
}


bool ReaderWin::Execute()
{
    mhInput = GetStdHandle(STD_INPUT_HANDLE);
    mhOutput = GetStdHandle(STD_OUTPUT_HANDLE);

    SaveConsoleState();


    // Main loop to read input events
    INPUT_RECORD inputRecord[128];
    DWORD numEventsRead;


    InitScreenInfo();
    // Set console mode to allow reading mouse and key events
    DWORD mode;
    if (!GetConsoleMode(mhInput, &mode))
    {
        // Piped input...... reopen for interactive input
        CloseHandle(mhInput);
        mhInput = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0,NULL);

        if (mhInput == INVALID_HANDLE_VALUE)
        {
            cerr << "Failed to get console mode." << endl;
            return false;
        }

        SetStdHandle(STD_INPUT_HANDLE, mhInput);
        if (!GetConsoleMode(mhInput, &mode))
        {
            cerr << "Failed to get console mode after creating new stdin" << endl;
            return false;
        }
    }
    


    mode |= ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT;
    mode &= ~(ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE);

    if (!SetConsoleMode(mhInput, mode))
    {
        cerr << "Failed to set console mode." << endl;
        return false;
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

    backBuffer.resize(w * h);
    drawStateBuffer.resize(w * h);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Clear the raw command buffer and param buffers
    //UpdateFromConsoleSize(true);

    filterTextEntryWin.SetText(sFilter);

    UpdateFromConsoleSize(true);  // force update if the screen changed


    bScreenInvalid = true;
    Init(Rect(0, 1, w, h));

    ZAttrib kReaderBG(0xFF000000FFff00ff);

    Clear(kReaderBG, false);
    SetEnableFrame();
    bAutoScrollbar = true;

//    mbVisible = true;

    rows = GetLines(mText);

    while (!mbDone)
    {
        bool bForeground = ConsoleHasFocus();
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

        if (UpdateFromConsoleSize(bScreenInvalid))
        {
            rows = GetLines(mText); // recompute rows
            UpdateCaptions();
        }

        DrawToScreen();
        Update();

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
                    return false;
                }

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
    }

    cout << "\033[?25h" << COL_WHITE << COL_BG_BLACK << DEC_LINE_END;    // show cursor, reset colors, ensure dec line mode is off
    RestoreConsoleState();

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
    if (filterTextEntryWin.GetText() != sFilter)
    {
        sFilter = filterTextEntryWin.GetText();
        invalid = true;
    }

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
    if (sFilename.empty())
        positionCaption[ConsoleWin::Position::LT] = "Reader";
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

    positionCaption[ConsoleWin::Position::LB] = "[CTRL-F:Filter]";

    if (sFilter.empty())
    {
        if (filterTextEntryWin.mbVisible)
            positionCaption[ConsoleWin::Position::LB] = "Set Filter:";
        positionCaption[ConsoleWin::Position::RT] = "Lines (" + SH::FromInt(viewing) + "-" + SH::FromInt(bottom) + "/" + SH::FromInt(entryCount) + ")";
    }
    else
    {
        positionCaption[ConsoleWin::Position::LB] = "Filtering: " + sFilter + "";
        positionCaption[ConsoleWin::Position::RT] = "Filtered Lines (" + SH::FromInt(viewing) + "-" + SH::FromInt(bottom) + "/" + SH::FromInt(entryCount) + ")";
    }




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
            invalid = true;
        }
        else
        {
            mTopVisibleRow -= mHeight / 4;
            invalid = true;
        }

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
    int64_t entryCount = rows.size();

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
            bScreenInvalid = true;
        }
    }

    if (helpTableWin.mbCanceled || helpTableWin.mbDone)
    {
        helpTableWin.SetVisible(false);
        helpTableWin.mbCanceled = false;
        helpTableWin.mbDone = false;
        bScreenInvalid = true;
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
                    filterTextEntryWin.SetArea(Rect(mX + 11, mY + mHeight - 1, mX + mWidth, mY + mHeight));
                    filterTextEntryWin.SetVisible();
                    invalid = true;
                }
                bHandled = true;
            }
        }
        else if (keycode == VK_ESCAPE)
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

    Update();
    return bHandled;
}

void ReaderWin::Paint(tConsoleBuffer& backBuf)
{
    if (!mbVisible || !invalid)
        return;

    ConsoleWin::BasePaint();


    int64_t firstDrawRow = mTopVisibleRow - 1;

    Rect drawArea;
    GetInnerArea(drawArea);

    tStringList::iterator it = rows.begin();
    int64_t startrow = 0;
    while (startrow < mTopVisibleRow && it != rows.end())
    {
        bool bInclude = sFilter.empty() || SH::Contains(*it, sFilter, false);  // either always include if filter is empty, or if s contains the filter text
        if (bInclude)
            startrow++;
        it++;
    }


    int64_t drawrow = drawArea.t;
    int64_t endrow = drawArea.b;
    int64_t filteredCount = GetFilteredCount();
    while (drawrow < endrow && it != rows.end())
    {
        if (drawrow < mHeight - 1 && it != rows.end())
        {
            string s = *it;
            bool bInclude = sFilter.empty() || SH::Contains(s, sFilter, false);  // either always include if filter is empty, or if s contains the filter text
            if (bInclude)
            {
                DrawClippedText(Rect(drawArea.l, drawrow, drawArea.r, drawrow + 1), s, WHITE_ON_BLACK, false, &drawArea);
                drawrow++;
            }
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
    invalid = false;
}


bool ReaderWin::UpdateFromConsoleSize(bool bForce)
{
    CONSOLE_SCREEN_BUFFER_INFO newScreenInfo;
    if (!GetConsoleScreenBufferInfo(mhOutput, &newScreenInfo))
    {
        cerr << "Failed to get console info." << endl;
        return false;
    }

    bool bChanges = false;
    if ((newScreenInfo.dwSize.X != screenInfo.dwSize.X || newScreenInfo.dwSize.Y != screenInfo.dwSize.Y) || bForce)
    {
        screenInfo = newScreenInfo;
        bScreenInvalid = true;
        invalid = true;

        SHORT w = ScreenW();
        SHORT h = ScreenH();


        if (w < 1)
            w = 1;
        if (h < 8)
            h = 8;


        Rect viewRect(0, 0, w, h);
        SetArea(viewRect);

        backBuffer.clear();
        backBuffer.resize(w * h);

        drawStateBuffer.clear();
        drawStateBuffer.resize(w * h);

        helpTableWin.Clear(kAttribHelpBG, true);
        helpTableWin.SetArea(viewRect);
        helpTableWin.SetEnableFrame();
        helpTableWin.bAutoScrollbar = true;


        if (filterTextEntryWin.mbVisible)
        {
            viewRect.b--;
            filterTextEntryWin.SetArea(Rect(viewRect.l, viewRect.b, viewRect.r, viewRect.b + 1));
        }

        Update();
        bChanges = true;
    }

    return bChanges;
}



int main(int argc, char* argv[])
{
    ReaderWin readerWin;

    string sFilename;
    string sFilter;
    int64_t nStartLineNumber = 0;

    CommandLineParser parser;
    parser.RegisterAppDescription("Reader application for scrolling through or searching text files or piped input.");

    parser.RegisterParam(ParamDesc("PATH", &sFilename,  CLP::kOptional|CLP::kExistingPath, "Optional path to read from if no piped input."));

    parser.RegisterParam(ParamDesc("FILTER", &sFilter, CLP::kNamed|CLP::kOptional, "Filter lines including this text."));

    parser.RegisterParam(ParamDesc("start", &nStartLineNumber, CLP::kNamed | CLP::kOptional, "Starting line number"));


    bool bParseSuccess = parser.Parse(argc, argv);
    if (!bParseSuccess)
    {
        return -1;
    }                

    if (!sFilename.empty() && sFilename[0] != '<') // temporarily detect pipe in via commandline param for vs debugging
    {
        readerWin.LoadFile(sFilename);
    }
    else
    {
        readerWin.ReadPipe();
    }

    if (!sFilter.empty())
        readerWin.SetFilter(sFilter);

    readerWin.Execute();

    return 0;
}
