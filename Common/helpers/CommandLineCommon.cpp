#pragma once

#include <string>
#include "CommandLineCommon.h"
#include "StringHelpers.h"
#include <Windows.h>
#include <list>
#include <assert.h>

using namespace std;



size_t CLP::ZAttrib::FromAnsi(const char* pChars)
{
    size_t len = strlen(pChars);
    if (len < 2)
        return false;

    const char* pWalker = pChars;
    const char* pEnd = pChars + len;

    // Check if the sequence starts with the ANSI escape character '\x1B' (27 in decimal)
    if (*pWalker != '\x1B')
    {
        return 0; // Not a valid ANSI sequence
    }
    pWalker++;

    // Check if the next character is '[' which indicates the beginning of an ANSI sequence
    if (*pWalker != '[')
    {
        assert(false);
        return 0; // Not a valid ANSI sequence
    }

    // Find the end of the ANSI sequence
    const char* pSeqEnd = pWalker;
    while (pSeqEnd < pEnd && *pSeqEnd != 'm')
    {
        pSeqEnd++;
    }

    if (pSeqEnd >= pEnd) 
    {
        assert(false);
        return false; // Incomplete ANSI sequence
    }

    // Extract the ANSI sequence
    std::string sequence(pWalker, pSeqEnd-pWalker+1);

    // Check if the sequence contains color information
    if (sequence.find("m") != std::string::npos)
    {
        // Convert ANSI color code to ZAttrib
        size_t pos = sequence.find("[");
        if (pos != std::string::npos)
        {
            std::string colorStr = sequence.substr(pos + 1, sequence.size() - pos - 2); // Extract color code part
            std::vector<std::string> colorCodes;
            size_t start = 0;
            size_t semicolon = colorStr.find(";");
            while (semicolon != std::string::npos)
            {
                colorCodes.push_back(colorStr.substr(start, semicolon - start));
                start = semicolon + 1;
                semicolon = colorStr.find(";", start);
            }
            colorCodes.push_back(colorStr.substr(start, colorStr.size() - start));

            // Process color codes
            for (size_t i = 0; i < colorCodes.size(); i++)
            {
                int colorCode = std::stoi(colorCodes[i]);

                // Handle basic colors (30-37, 40-47)
                if (colorCode >= 30 && colorCode <= 37) 
                {
                    // Foreground colors
                    switch (colorCode)
                    {
                    case 30: SetFG(BLACK);      break;
                    case 31: SetFG(RED);        break;
                    case 32: SetFG(GREEN);      break;
                    case 33: SetFG(YELLOW);     break;
                    case 34: SetFG(BLUE);       break;
                    case 35: SetFG(MAGENTA);    break;
                    case 36: SetFG(CYAN);       break;
                    case 37: SetFG(WHITE);      break;
                    }
                }
                else if (colorCode >= 40 && colorCode <= 47) 
                {
                    // Background colors
                    switch (colorCode)
                    {
                    case 40: SetBG(BLACK);      break;
                    case 41: SetBG(RED);        break;
                    case 42: SetBG(GREEN);      break;
                    case 43: SetBG(YELLOW);     break;
                    case 44: SetBG(BLUE);       break;
                    case 45: SetBG(MAGENTA);    break;
                    case 46: SetBG(CYAN);       break;
                    case 47: SetBG(WHITE);      break;
                    }
                }
                // Handle 256 color mode: 38;5;NNN (foreground)
                else if (colorCode == 38 && i + 2 < colorCodes.size() && colorCodes[i + 1] == "5") 
                {
                    int colorIndex = std::stoi(colorCodes[i + 2]);

                    // Convert 256-color index to RGB
                    // This is a simplification - actual mapping depends on the terminal
                    if (colorIndex < 16) 
                    {
                        // Standard colors (0-15)
                        // Map accordingly to your ZAttrib system
                    }
                    else if (colorIndex < 232) 
                    {
                        // 6x6x6 color cube (16-231)
                        int indexAdjusted = colorIndex - 16;
                        a = 0xFF;  // Full alpha
                        r = (indexAdjusted / 36) * 51;
                        g = ((indexAdjusted % 36) / 6) * 51;
                        b = (indexAdjusted % 6) * 51;
                    }
                    else 
                    {
                        // Grayscale (232-255)
                        uint8_t gray = (colorIndex - 232) * 10 + 8;
                        a = 0xFF;
                        r = gray;
                        g = gray;
                        b = gray;
                    }

                    i += 2; // Skip the next two parameters
                }
                // Handle 256 color mode: 48;5;NNN (background)
                else if (colorCode == 48 && i + 2 < colorCodes.size() && colorCodes[i + 1] == "5") 
                {
                    int colorIndex = std::stoi(colorCodes[i + 2]);

                    // Similar conversion for background
                    if (colorIndex < 16) 
                    {
                        // Standard colors (0-15)
                        // Map accordingly
                    }
                    else if (colorIndex < 232) 
                    {
                        // 6x6x6 color cube (16-231)
                        int indexAdjusted = colorIndex - 16;
                        ba = 0xFF;
                        br = (indexAdjusted / 36) * 51;
                        bg = ((indexAdjusted % 36) / 6) * 51;
                        bb = (indexAdjusted % 6) * 51;
                    }
                    else 
                    {
                        // Grayscale (232-255)
                        uint8_t gray = (colorIndex - 232) * 10 + 8;
                        ba = 0xFF;
                        br = gray;
                        bg = gray;
                        bb = gray;
                    }

                    i += 2; // Skip the next two parameters
                }
                // Handle RGB mode: 38;2;R;G;B (foreground)
                else if (colorCode == 38 && i + 4 < colorCodes.size() && colorCodes[i + 1] == "2") 
                {
                    // Set RGB foreground
                    a = 0xFF;
                    r = static_cast<uint8_t>(std::stoi(colorCodes[i + 2]));
                    g = static_cast<uint8_t>(std::stoi(colorCodes[i + 3]));
                    b = static_cast<uint8_t>(std::stoi(colorCodes[i + 4]));

                    i += 4; // Skip the next four parameters
                }
                // Handle RGB mode: 48;2;R;G;B (background)
                else if (colorCode == 48 && i + 4 < colorCodes.size() && colorCodes[i + 1] == "2") 
                {
                    // Set RGB background
                    ba = 0xFF;
                    br = static_cast<uint8_t>(std::stoi(colorCodes[i + 2]));
                    bg = static_cast<uint8_t>(std::stoi(colorCodes[i + 3]));
                    bb = static_cast<uint8_t>(std::stoi(colorCodes[i + 4]));

                    i += 4; // Skip the next four parameters
                }
                // Reset
                else if (colorCode == 0) 
                {
                    Set(WHITE_ON_BLACK);
                }
            }

            return pSeqEnd- pChars+1;
        }
    }
    return 0; // Not a valid ANSI color sequence
}

string CLP::ZAttrib::ToAnsi() const
{
    std::ostringstream ansi;

    // Start the ANSI sequence
    ansi << "\033[";

    bool needSemicolon = false;

    // Check if we need to handle foreground color
    if (a > 0) 
    {
        // Use RGB mode for foreground
        ansi << "38;2;" << static_cast<int>(r) << ";"
            << static_cast<int>(g) << ";"
            << static_cast<int>(b);
        needSemicolon = true;
    }

    // Check if we need to handle background color
    if (ba > 0) 
    {
        if (needSemicolon) 
        {
            ansi << ";";
        }
        // Use RGB mode for background
        ansi << "48;2;" << static_cast<int>(br) << ";"
            << static_cast<int>(bg) << ";"
            << static_cast<int>(bb);
    }

    // End the ANSI sequence
    ansi << "m";

    return ansi.str();
}


namespace CLP
{
    // Styles
    ZAttrib kAttribAppName(GOLD);
    ZAttrib kAttribCaption(BLACK | MAKE_BG(GOLD));
    ZAttrib kAttribSection(CYAN);

    ZAttrib kAttribFolderListBG(MAKE_BG(0xff000044));

    ZAttrib kAttribListBoxBG(MAKE_BG(ORANGE));
    ZAttrib kAttribListBoxEntry(WHITE);
    ZAttrib kAttribListBoxSelectedEntry(BLACK | MAKE_BG(YELLOW));

    ZAttrib kAttribParamListBG(MAKE_BG(0xff4444aa));
    ZAttrib kAttribTopInfoBG(BLACK | MAKE_BG(0xFF333399));

    ZAttrib kAttribHelpBG(0xFF444444FF000000);

    ZAttrib kRawText(WHITE);
    ZAttrib kSelectedText(WHITE | MAKE_BG(0xFF999999));
    ZAttrib kGoodParam(GREEN);
    ZAttrib kBadParam(RED);
    ZAttrib kUnknownParam(YELLOW);

    ZAttrib kAttribError(RED);
    ZAttrib kAttribWarning(ORANGE);



    HANDLE mhInput;
    HANDLE mhOutput;
    std::vector<CHAR_INFO> originalConsoleBuf;
    CONSOLE_SCREEN_BUFFER_INFO originalScreenInfo;
    CONSOLE_SCREEN_BUFFER_INFO screenInfo;
    bool bScreenChanged = false;


    void SaveConsoleState()
    {
        originalScreenInfo = screenInfo;
        originalConsoleBuf.resize(originalScreenInfo.dwSize.X * originalScreenInfo.dwSize.Y);
        SMALL_RECT readRegion = { 0, 0, originalScreenInfo.dwSize.X - 1, originalScreenInfo.dwSize.Y - 1 };
        ReadConsoleOutput(mhOutput, &originalConsoleBuf[0], screenInfo.dwSize, { 0, 0 }, &readRegion);
    }

    void RestoreConsoleState()
    {
        SMALL_RECT writeRegion = { 0, 0, originalScreenInfo.dwSize.X - 1, originalScreenInfo.dwSize.Y - 1 };
        WriteConsoleOutput(mhOutput, &originalConsoleBuf[0], originalScreenInfo.dwSize, { 0, 0 }, &writeRegion);
        SetConsoleCursorPosition(mhOutput, originalScreenInfo.dwCursorPosition);
        SetConsoleTextAttribute(mhOutput, originalScreenInfo.wAttributes);
    }

    SHORT ScreenW() { return screenInfo.srWindow.Right - screenInfo.srWindow.Left + 1; }
    SHORT ScreenH() { return screenInfo.srWindow.Bottom - screenInfo.srWindow.Top + 1; }

    void DrawAnsiChar(int64_t x, int64_t y, char c, ZAttrib ca)
    {
        static ZAttrib lastC;
        static int64_t lastX = -1;
        static int64_t lastY = -1;

        string s;
        if (x != lastX || y != lastY)
        {
            s += "\033[" + SH::FromInt(x + 1) + "G\033[" + SH::FromInt(y) + "d";
            lastX = x;
            lastY = y;
        }

        if (lastC.a != ca.a || lastC.r != ca.r || lastC.g != ca.g || lastC.b != ca.b || lastC.ba != ca.ba || lastC.br != ca.br || lastC.bg != ca.bg || lastC.bb != ca.bb)
        {
            s += "\033[38;2;" + SH::FromInt(ca.r) + ";" + SH::FromInt(ca.g) + ";" + SH::FromInt(ca.b) + "m"; //forground color
            s += "\033[48;2;" + SH::FromInt(ca.br) + ";" + SH::FromInt(ca.bg) + ";" + SH::FromInt(ca.bb) + "m"; //background color

            lastC = ca;
        }

        if (c < 32 || lastC.a == 0)
            s += ' ';
        else
            s += c;
        cout << s;
        lastX++;    // cursor advances after drawing
    };



    bool ConsoleWin::Init(const Rect& r)
    {
        SetArea(r);
        mbVisible = true;
        mbDone = false;
        mbCanceled = false;
        return true;
    }

    void ConsoleWin::DrawClippedAnsiText(int64_t x, int64_t y, std::string ansitext, bool bWrap, Rect* pClip)
    {
        int64_t cursorX = x;
        int64_t cursorY = y;

        ZAttrib attrib(WHITE);

        CLP::Rect drawArea;
        GetInnerArea(drawArea);

        for (size_t i = 0; i < ansitext.length(); i++)
        {
            size_t skiplength = attrib.FromAnsi(&ansitext[i]);
            if (skiplength > 0)
            {
                i += skiplength - 1;    // -1 so that the i++ above will be correct
            }
            else
            {
                char c = ansitext[i];
                if ((c == '\n' || cursorX > drawArea.r) && bWrap)
                {
                    cursorX = drawArea.l;
                    cursorY++;
                }
                else
                {
                    DrawCharClipped(c, cursorX, cursorY, attrib, pClip);
                    cursorX++;
                    if (cursorX >= drawArea.r && !bWrap)
                        break;
                }
            }
        }
    }

    std::string removeANSISequences(const std::string& str)
    {
        std::string result;
        bool inEscapeSequence = false;

        for (char ch : str)
        {
            if (inEscapeSequence)
            {
                if (ch == 'm')
                {
                    inEscapeSequence = false;
                }
                continue;
            }
            if (ch == '\x1B')
            {
                inEscapeSequence = true;
                continue;
            }
            result.push_back(ch);
        }

        return result;
    }


    void ConsoleWin::GetTextOuputRect(std::string text, int64_t& w, int64_t& h)
    {
        w = 0;
        h = 0;
        int64_t x = 0;
        int64_t y = 0;
        ZAttrib attrib;
        for (size_t i = 0; i < text.length(); i++)
        {
            size_t skiplength = attrib.FromAnsi(&text[i]);
            if (skiplength > 0)
            {
                i += skiplength - 1;    // -1 so that the i++ above will be correct
            }
            else
            {
                char c = text[i];
                if (c == '\n' || x >= mWidth)
                {
                    x = 0;
                    y++;
                    h = std::max<int64_t>(h, y);
                }
                else
                {
                    x++;
                    w = std::max<int64_t>(w, x);
                }
            }
        }
    }



    void ConsoleWin::GetCaptionPosition(string& caption, ConsoleWin::Position pos, int64_t& x, int64_t& y)
    {
        if (pos == Position::LT || pos == Position::LB) // Left
            x = 0;
        else if (pos == Position::CT || pos == Position::CB) // Center
            x = (mWidth - caption.length()) / 2;
        else
            x = mWidth - caption.length(); // Right

        if (pos == Position::LT || pos == Position::CT || pos == Position::RT)
            y = 0;
        else
            y = mHeight - 1;
    }

    void ConsoleWin::BasePaint()
    {
        // Update display
        if (!mbVisible)
            return;

        Clear(mClearAttrib, mbGradient);

        // Fill 
        Fill(Rect(0, 0, mWidth, mHeight), mClearAttrib, mbGradient);

        if (enableFrame[Side::T])
            Fill(Rect(0, 0, mWidth, 1), kAttribCaption, mbGradient);   // top

        if (enableFrame[Side::L])
            Fill(Rect(0, 0, 1, mHeight), kAttribCaption, mbGradient);   // left

        if (enableFrame[Side::B])
            Fill(Rect(0, mHeight - 1, mWidth, mHeight), kAttribCaption, mbGradient);    // bottom

        if (enableFrame[Side::R])
            Fill(Rect(mWidth - 1, 0, mWidth, mHeight), kAttribCaption, mbGradient);    // right  

        // draw all captions
        for (uint8_t pos = 0; pos < Position::MAX_POSITIONS; pos++)
        {
            string caption = positionCaption[pos];
            if (!caption.empty())
            {
                int64_t x = 0;
                int64_t y = 0;
                GetCaptionPosition(caption, Position(pos), x, y);
                DrawClippedText(x, y, caption, kAttribCaption, false);
            }
        }
    }


    void ConsoleWin::RenderToBackBuf(tConsoleBuffer& backBuf)
    {
        int64_t dr = ScreenW();
        int64_t db = ScreenH();

        for (int64_t sy = 0; sy < mHeight; sy++)
        {
            int64_t dy = sy + mY;
            if (dy >= 0 && dy < db) // clip top and bottom
            {
                for (int64_t sx = 0; sx < mWidth; sx++)
                {
                    int64_t dx = sx + mX;
                    if (dx >= 0 && dx < dr)
                    {
                        int64_t sindex = (sy * mWidth) + sx;
                        int64_t dindex = (dy * dr) + dx;
                        backBuf[dindex] = mBuffer[sindex];
                    }
                }
            }

        }
    }


    void ConsoleWin::Clear(ZAttrib attrib, bool bGradient)
    {
        mClearAttrib = attrib;
        mbGradient = bGradient;
        for (size_t i = 0; i < mBuffer.size(); i++)
        {
            mBuffer[i].c = 0;
            mBuffer[i].attrib = mClearAttrib;
        }
    }

    void ConsoleWin::SetEnableFrame(bool _l, bool _t, bool _r, bool _b)
    {
        enableFrame[Side::L] = _l;
        enableFrame[Side::T] = _t;
        enableFrame[Side::R] = _r;
        enableFrame[Side::B] = _b;
    }


    void ConsoleWin::ClearCaptions()
    {
        for (uint8_t pos = 0; pos < ConsoleWin::Position::MAX_POSITIONS; pos++)
            positionCaption[pos].clear();
    }


    void ConsoleWin::Fill(const Rect& r, ZAttrib attrib, bool bGradient)
    {
        for (int64_t y = r.t; y < r.b; y++)
        {
            // playing with gradient fill
            if (bGradient)
            {
                attrib.br = (uint64_t)(attrib.br * 0.96);
                attrib.bg = (uint64_t)(attrib.bg * 0.96);
                attrib.bb = (uint64_t)(attrib.bb * 0.96);
            }
            for (int64_t x = r.l; x < r.r; x++)
            {
                size_t offset = y * mWidth + x;
                mBuffer[offset].attrib = attrib;
            }
        }
    }

    void ConsoleWin::Fill(ZAttrib attrib, bool bGradient)
    {
        return Fill(Rect(0, 0, mWidth, mHeight), attrib, bGradient);
    }

    void ConsoleWin::DrawCharClipped(char c, int64_t x, int64_t y, ZAttrib attrib, Rect* pClip)
    {
        if (pClip)
        {
            if (x < pClip->l || y < pClip->t || x >= pClip->r || y >= pClip->b)
                return;
        }

        if (attrib.FG() == attrib.BG())  // would be invisible
        {
            // invert fg
            attrib.r = ~attrib.r;
            attrib.g = ~attrib.g;
            attrib.b = ~attrib.b;
        }

        size_t offset = y * mWidth + x;
        DrawCharClipped(c, (int64_t)offset, attrib);
    }

    void ConsoleWin::DrawCharClipped(char c, int64_t offset, ZAttrib attrib)
    {
        if (offset >= 0 && offset < (int64_t)mBuffer.size())    // clip
        {
            mBuffer[offset].c = c;
            if (GET_BA(attrib) > 0)
                mBuffer[offset].attrib = (uint64_t)attrib;
            else
                mBuffer[offset].attrib.SetFG(attrib.FG());  // if background is transparent, only set FG color
        }
    }



    int64_t ConsoleWin::DrawFixedColumnStrings(int64_t x, int64_t y, tStringArray& strings, vector<size_t>& colWidths, tAttribArray attribs, Rect* pClip)
    {
        assert(strings.size() == colWidths.size() && colWidths.size() == attribs.size());

        int64_t rowsDrawn = 0;
        for (int i = 0; i < strings.size() - 1; i++)
        {
            DrawClippedText(x, y, strings[i], attribs[i], false, pClip);
            x += colWidths[i];
        }

        // draw final column
        int64_t finalCol = strings.size() - 1;
        string sDraw = strings[finalCol];

        // draw as many rows as needed to
        int64_t remainingColumnWidth = screenInfo.srWindow.Right - x;
        while (remainingColumnWidth > 0 && sDraw.length())
        {
            DrawClippedText(x, y, sDraw.substr(0, remainingColumnWidth), attribs[finalCol], false, pClip);
            y++;
            sDraw = sDraw.substr(std::min<int64_t>(remainingColumnWidth, (int64_t)sDraw.length()));
            rowsDrawn++;
        }

        return rowsDrawn;
    }

    void ConsoleWin::DrawClippedText(int64_t x, int64_t y, std::string text, ZAttrib attributes, bool bWrap, Rect* pClip)
    {
        COORD cursor((SHORT)x, (SHORT)y);

        for (size_t textindex = 0; textindex < text.size(); textindex++)
        {
            char c = text[textindex];
            if (c == '\n' && bWrap)
            {
                cursor.X = 0;
                cursor.Y++;
            }
            else
            {
                DrawCharClipped(c, cursor.X, cursor.Y, attributes, pClip);
            }

            cursor.X++;
            if (cursor.X >= mWidth && !bWrap)
                break;
        }
    }


    void InfoWin::Paint(tConsoleBuffer& backBuf)
    {
        if (!mbVisible)
            return;

        ConsoleWin::BasePaint();

        Rect drawArea;
        GetInnerArea(drawArea);

        int64_t firstDrawRow = mTopVisibleRow - 1;
        DrawClippedAnsiText(drawArea.l, -firstDrawRow, mText, true, &drawArea);

        ConsoleWin::RenderToBackBuf(backBuf);
    }



    void InfoWin::DrawScrollbar(const Rect& r, int64_t min, int64_t max, int64_t cur, ZAttrib bg, ZAttrib thumb)
    {
        if (!mbVisible)
            return;

        bool bHorizontal = true;

        int64_t sbSize = r.r - r.l;
        if (r.b - r.t > sbSize)
        {
            sbSize = r.b - r.t;
            bHorizontal = false;
        }

        assert(sbSize > 0);

        float fThumbRatio = (float)sbSize/(float)(max - min);

        int64_t thumbSize = std::max<int64_t>(1, (int64_t) ((float)(sbSize) * fThumbRatio)); // minimum 1 
        thumbSize = std::min<int64_t>(sbSize, thumbSize);                                       // maximum the whole scrollbar

        Fill(r, bg, false);


        int64_t nUnscaledValRange = max - min;
        double fNormalizedValue = (double)(cur - min) / nUnscaledValRange;
        if (fNormalizedValue < 0.0)
            fNormalizedValue = 0.0;
        else if (fNormalizedValue > 1.0)
            fNormalizedValue = 1.0;

        Rect rThumb;
        if (bHorizontal)
        {
            rThumb.l = r.l + (int64_t) (fNormalizedValue * (float)((r.r - r.l) - thumbSize));
            rThumb.r = rThumb.l + thumbSize;
            rThumb.t = r.t;
            rThumb.b = r.b;
        }
        else
        {
            rThumb.t = r.t + (int64_t) (fNormalizedValue * (float)((r.b - r.t) - thumbSize));
            rThumb.b = rThumb.t + thumbSize;
            rThumb.l = r.l;
            rThumb.r = r.r;
        }

        Fill(rThumb, thumb, false);
    }


    void InfoWin::UpdateCaptions()
    {
        Rect drawArea;
        GetInnerArea(drawArea);
        int64_t drawHeight = drawArea.b - drawArea.t;

        int64_t docWidth = 0;
        int64_t docHeight = 0;
        GetTextOuputRect(mText, docWidth, docHeight);

        if (docHeight > drawHeight)
        {
            positionCaption[ConsoleWin::Position::RT] = "Lines (" + SH::FromInt(mTopVisibleRow + 1) + "-" + SH::FromInt(mTopVisibleRow + drawHeight) + "/" + SH::FromInt(docHeight) + ")";
            positionCaption[ConsoleWin::Position::RB] = "[UP/DOWN][PAGE Up/Down][HOME/END]";
        }
        else
        {
            positionCaption[ConsoleWin::Position::RT].clear();
        }
    }

    void InfoWin::OnKey(int keycode, char c)
    {
        Rect drawArea;
        GetInnerArea(drawArea);
        int64_t drawHeight = drawArea.b - drawArea.t;

        int64_t docWidth = 0;
        int64_t docHeight = 0;
        GetTextOuputRect(mText, docWidth, docHeight);

        if (keycode == VK_F1 || keycode == VK_ESCAPE)
        {
            mText.clear();
            mbVisible = false;
            mbDone = true;
        }

        if (docHeight > drawHeight)
        {
            if (keycode == VK_UP)
            {
                mTopVisibleRow--;
            }
            else if (keycode == VK_DOWN)
            {
                mTopVisibleRow++;
            }
            else if (keycode == VK_HOME)
            {
                mTopVisibleRow = 0;
            }
            else if (keycode == VK_PRIOR)
            {
                mTopVisibleRow -= drawHeight;
            }
            else if (keycode == VK_NEXT)
            {
                mTopVisibleRow += drawHeight;
            }
            else if (keycode == VK_END)
            {
                mTopVisibleRow = docHeight - drawHeight;
            }

            if (mTopVisibleRow < 0)
                mTopVisibleRow = 0;
            if (mTopVisibleRow > (docHeight - drawHeight))
                mTopVisibleRow = (docHeight - drawHeight);
        }

        UpdateCaptions();

    }


    void TextEditWin::UpdateFirstVisibleRow()
    {
        if (!mbVisible)
            return;

        int64_t rowCount = ((int64_t)mText.size() + mWidth - 1) / mWidth;

        if (mLocalCursorPos.Y < 0)
        {
            firstVisibleRow = firstVisibleRow + mLocalCursorPos.Y;
            UpdateCursorPos(COORD(mLocalCursorPos.X, 0));
        }
        else if (mLocalCursorPos.Y >= mHeight)
        {
            firstVisibleRow = firstVisibleRow + mLocalCursorPos.Y - mHeight + 1;
            UpdateCursorPos(COORD(mLocalCursorPos.X, (SHORT)mHeight - 1));
        }
    }


    void TextEditWin::UpdateCursorPos(COORD localPos)
    {
        if (!mbVisible)
            return;

        int index = (int)CursorToTextIndex(localPos);
        if (index > (int)mText.length())
            index = (int)mText.length();
        mLocalCursorPos = TextIndexToCursor(index);

        //        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), LocalCursorToGlobal(mLocalCursorPos));

        COORD global = LocalCursorToGlobal(mLocalCursorPos);
        cout << "\033[" << global.X + 1 << "G\033[" << global.Y << "d" << std::flush;
    }


    COORD TextEditWin::LocalCursorToGlobal(COORD cursor)
    {
        return COORD(cursor.X + (SHORT)mX + (SHORT)(CLP::appName.length() + 1), cursor.Y + (SHORT)mY);
    }



    void TextEditWin::FindNextBreak(int nDir)
    {
        int index = (int)CursorToTextIndex(mLocalCursorPos);

        if (nDir > 0)
        {
            index++;
            if (index < (int64_t)mText.length())
            {
                if (isalnum((int)mText[index]))   // index is on an alphanumeric
                {
                    while (index < (int64_t)mText.length() && isalnum((int)mText[index]))   // while an alphanumeric character, skip
                        index++;
                }

                while (index < (int64_t)mText.length() && isblank((int)mText[index]))    // while whitespace, skip
                    index++;
            }
        }
        else
        {
            index--;
            if (index > 0)
            {
                if (isalnum((int)mText[index]))   // index is on an alphanumeric
                {
                    while (index > 0 && isalnum((int)mText[index - 1]))   // while an alphanumeric character, skip
                        index--;
                }
                else
                {
                    while (index > 0 && isblank((int)mText[index - 1]))    // while whitespace, skip
                        index--;

                    while (index > 0 && isalnum((int)mText[index - 1]))   // while an alphanumeric character, skip
                        index--;
                }
            }
        }

        if (index < 0)
            index = 0;
        if (index > (int)mText.length())
            index = (int)mText.length();

        UpdateCursorPos(TextIndexToCursor(index));
    }

    bool TextEditWin::IsIndexInSelection(int64_t i)
    {
        int64_t normalizedStart = selectionstart;
        int64_t normalizedEnd = selectionend;
        if (normalizedEnd < normalizedStart)
        {
            normalizedStart = selectionend;
            normalizedEnd = selectionstart;
        }

        return (i >= normalizedStart && i < normalizedEnd);
    }

    string TextEditWin::GetSelectedText()
    {
        if (!IsTextSelected())
            return "";

        int64_t normalizedStart = selectionstart;
        int64_t normalizedEnd = selectionend;
        if (normalizedEnd < normalizedStart)
        {
            normalizedStart = selectionend;
            normalizedEnd = selectionstart;
        }

        return mText.substr(normalizedStart, normalizedEnd - normalizedStart);
    }

    void TextEditWin::AddUndoEntry()
    {
        undoEntry entry(mText, CursorToTextIndex(mLocalCursorPos), selectionstart, selectionend);
        mUndoEntryList.emplace_back(std::move(entry));
    }

    void TextEditWin::Undo()
    {
        if (mUndoEntryList.empty())
            return;

        undoEntry entry(*mUndoEntryList.rbegin());
        mUndoEntryList.pop_back();

        mText = entry.text;
        selectionstart = entry.selectionstart;
        selectionend = entry.selectionend;
        UpdateCursorPos(TextIndexToCursor(entry.cursorindex));
    }

    void TextEditWin::SetText(const std::string& text)
    {
        mText = text;
        UpdateCursorPos(TextIndexToCursor((int64_t)text.size()));
    }

    void TextEditWin::SetArea(const Rect& r)
    {
        ConsoleWin::SetArea(r);
        UpdateCursorPos(mLocalCursorPos);
    }

    void TextEditWin::Paint(tConsoleBuffer& backBuf)
    {
        // Update display
        if (!mbVisible)
            return;

        ConsoleWin::BasePaint();

        COORD cursor((SHORT)0, (SHORT)-firstVisibleRow);

        std::vector<ZAttrib> attribs;
        attribs.resize(mText.size());


        for (size_t textindex = 0; textindex < mText.length(); textindex++)
        {
            attribs[textindex] = kRawText;
        }

        for (size_t textindex = 0; textindex < mText.length(); textindex++)
        {
            if (IsIndexInSelection(textindex))
                attribs[textindex] = kSelectedText;
        }


        for (size_t textindex = 0; textindex < CLP::appName.size(); textindex++)
        {
            char c = CLP::appName[textindex];
            if (c == '\n')
            {

                cursor.X = 0;
                cursor.Y++;
            }
            else
            {
                DrawCharClipped(c, cursor.X, cursor.Y, kAttribAppName);
            }

            cursor.X++;
        }

        cursor.X++;


        for (size_t textindex = 0; textindex < mText.size(); textindex++)
        {
            char c = mText[textindex];
            if (c == '\n')
            {

                cursor.X = 0;
                cursor.Y++;
            }
            else
            {
                DrawCharClipped(c, cursor.X, cursor.Y, attribs[textindex]);
            }

            cursor.X++;
        }



        ConsoleWin::RenderToBackBuf(backBuf);
    }


    void TextEditWin::OnKey(int keycode, char c)
    {
        bool bCTRLHeld = GetKeyState(VK_CONTROL) & 0x800;
        bool bSHIFTHeld = GetKeyState(VK_SHIFT) & 0x800;

        bool bHandled = false;

        do
        {
            switch (keycode)
            {
            case VK_RETURN:
                mbDone = true;
                bHandled = true;
                return;
            case VK_ESCAPE:
            {
                if (IsTextSelected())
                {
                    ClearSelection();
                }
                else
                {
                    mbCanceled = true;
                }
                bHandled = true;
            }
            break;
            case VK_HOME:
            {
                UpdateSelection();
                UpdateCursorPos(TextIndexToCursor(0));
                UpdateSelection();
                bHandled = true;
            }
            break;
            case VK_END:
            {
                UpdateSelection();
                UpdateCursorPos(TextIndexToCursor((int64_t)mText.size()));
                UpdateSelection();
                bHandled = true;
            }
            break;
            case VK_UP:
            {
                if (mLocalCursorPos.Y + firstVisibleRow > 0)
                {
                    mLocalCursorPos.Y--;
                    UpdateFirstVisibleRow();
                    UpdateCursorPos(mLocalCursorPos);
                }
                else
                {
                    UpdateSelection();
                    UpdateCursorPos(COORD(mLocalCursorPos.X, mLocalCursorPos.Y - 1));
                    UpdateSelection();
                }
                bHandled = true;
            }
            break;
            case VK_DOWN:
            {
                UpdateSelection();
                UpdateCursorPos(COORD(mLocalCursorPos.X, mLocalCursorPos.Y + 1));
                UpdateSelection();
                bHandled = true;
            }
            break;
            case VK_LEFT:
            {
                UpdateSelection();
                // Move cursor left
                int64_t index = CursorToTextIndex(mLocalCursorPos);
                if (index > 0)
                {
                    if (bCTRLHeld)
                        FindNextBreak(-1);
                    else
                        UpdateCursorPos(TextIndexToCursor(index - 1));
                }
                UpdateSelection();
                bHandled = true;
            }
            break;
            case VK_RIGHT:
            {
                UpdateSelection();
                if (bCTRLHeld)
                {
                    FindNextBreak(1);
                }
                else
                {
                    // Move cursor right
                    int64_t index = CursorToTextIndex(mLocalCursorPos);
                    if (index < (int64_t)mText.size())
                    {
                        UpdateCursorPos(TextIndexToCursor(index + 1));
                    }

                }
                UpdateSelection();
                bHandled = true;
            }
            break;
            case VK_BACK:
            {
                if (IsTextSelected())
                {
                    AddUndoEntry();
                    DeleteSelection();
                }
                else
                {
                    // Delete character before cursor
                    int64_t index = CursorToTextIndex(mLocalCursorPos);
                    if (index > 0)
                    {
                        UpdateSelection();
                        mText.erase(index - 1, 1);
                        UpdateCursorPos(TextIndexToCursor(index - 1));
                    }
                }
                bHandled = true;
            }
            break;
            case VK_DELETE:
            {
                if (IsTextSelected())
                {
                    AddUndoEntry();
                    if (bSHIFTHeld) // SHIFT-DELETE is cut
                    {
                        CopyTextToClipboard(GetSelectedText());
                    }

                    DeleteSelection();
                }
                else
                {
                    // Delete character at cursor
                    int64_t index = CursorToTextIndex(mLocalCursorPos);
                    if (index < (int64_t)(mText.size()))
                    {
                        AddUndoEntry();
                        mText.erase(index, 1);
                    }
                }
                UpdateSelection();
                bHandled = true;
            }
            break;
            case 0x58:
            {
                if (bCTRLHeld)  // CTRL-X is cut
                {
                    if (IsTextSelected())
                    {
                        AddUndoEntry();
                        CopyTextToClipboard(GetSelectedText());
                        DeleteSelection();
                    }

                    bHandled = true;
                    break;
                }
            }
            case 0x41:
            {
                if (bCTRLHeld)  // CTRL-A
                {
                    selectionstart = 0;
                    selectionend = mText.length();
                    bHandled = true;
                    break;
                }
            }
            break;
            case VK_INSERT:
            case 0x43:
            {
                if (bCTRLHeld)  // CTRL-C
                {
                    // handle copy
                    CopyTextToClipboard(GetSelectedText());
                    bHandled = true;
                    break;
                }
                if (bSHIFTHeld)  // SHIFT-INSERT is paste
                {
                    AddUndoEntry();
                    HandlePaste(GetTextFromClipboard());
                    bHandled = true;
                    break;
                }
            }
            break;
            case 0x5a:          // CTRL-Z
            {
                if (bCTRLHeld)
                {
                    Undo();
                    bHandled = true;
                    break;
                }
            }
            }
        } while (0); // for breaking

        // nothing handled above....regular text entry
        if (!bHandled && keycode >= 32)
        {
            AddUndoEntry();
            if (IsTextSelected())
                DeleteSelection();

            // Insert character at cursor position
            int index = (int)CursorToTextIndex(mLocalCursorPos);
            mText.insert(index, 1, c);
            UpdateCursorPos(TextIndexToCursor(index + 1));
            UpdateSelection();
        }


        UpdateFirstVisibleRow();
    }

    int64_t TextEditWin::CursorToTextIndex(COORD coord)
    {
        int64_t i = (coord.Y + firstVisibleRow) * mWidth + coord.X;
        return std::min<size_t>(i, mText.size());
    }

    COORD TextEditWin::TextIndexToCursor(int64_t i)
    {
        if (i > (int64_t)mText.size())
            i = (int64_t)mText.size();

        if (mWidth > 0)
        {
            COORD c;
            c.X = (SHORT)(i) % mWidth;
            c.Y = (SHORT)((i / mWidth) - firstVisibleRow);
            return c;
        }

        return COORD(0, 0);
    }

    void TextEditWin::HandlePaste(string text)
    {
        DeleteSelection();  // delete any selection if needed
        int64_t curindex = CursorToTextIndex(mLocalCursorPos);
        mText.insert(curindex, text);
        curindex += (int)text.length();
        UpdateCursorPos(TextIndexToCursor(curindex));

        /*        static int count = 1;
                char buf[64];
                sprintf(buf, "paste:%d\n", count++);
                OutputDebugString(buf);*/
    }

    void TextEditWin::DeleteSelection()
    {
        if (!IsTextSelected())
            return;

        int64_t normalizedStart = selectionstart;
        int64_t normalizedEnd = selectionend;
        if (normalizedEnd < normalizedStart)
        {
            normalizedStart = selectionend;
            normalizedEnd = selectionstart;
        }

        int64_t selectedChars = normalizedEnd - normalizedStart;

        mText.erase(normalizedStart, selectedChars);

        int curindex = (int)CursorToTextIndex(mLocalCursorPos);
        if (curindex > normalizedStart)
            curindex -= (int)(curindex - normalizedStart);
        UpdateCursorPos(TextIndexToCursor(curindex));

        ClearSelection();
    }

    void TextEditWin::ClearSelection()
    {
        selectionstart = -1;
        selectionend = -1;
        UpdateCursorPos(mLocalCursorPos);
    }

    void TextEditWin::UpdateSelection()
    {
        if (!(GetKeyState(VK_SHIFT) & 0x800))
        {
            ClearSelection();
        }
        else
        {
            if (selectionstart == -1)
            {
                selectionstart = CursorToTextIndex(mLocalCursorPos);
            }
            selectionend = CursorToTextIndex(mLocalCursorPos);
        }
    }



    string GetTextFromClipboard()
    {
        if (!OpenClipboard(NULL))
            return "";

        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData == NULL)
        {
            CloseClipboard();
            return "";
        }

        CHAR* pszText = static_cast<CHAR*>(GlobalLock(hData));
        if (pszText == NULL)
        {
            CloseClipboard();
            return "";
        }

        std::string clipboardText = pszText;

        GlobalUnlock(hData);
        CloseClipboard();

        return clipboardText;
    }

    bool CopyTextToClipboard(const std::string& text)
    {
        if (!OpenClipboard(NULL))
        {
            std::cerr << "Error opening clipboard" << std::endl;
            return false;
        }

        if (!EmptyClipboard())
        {
            CloseClipboard();
            std::cerr << "Error emptying clipboard" << std::endl;
            return false;
        }

        HGLOBAL hClipboardData = GlobalAlloc(GMEM_MOVEABLE, (text.length() + 1) * sizeof(char));
        if (hClipboardData == NULL)
        {
            CloseClipboard();
            std::cerr << "Error allocating memory for clipboard" << std::endl;
            return false;
        }

        char* pBuffer = static_cast<char*>(GlobalLock(hClipboardData));
        if (pBuffer == NULL)
        {
            CloseClipboard();
            std::cerr << "Error locking memory for clipboard" << std::endl;
            return false;
        }

        // Copy the text to the buffer
        strcpy_s(pBuffer, text.length() + 1, text.c_str());

        GlobalUnlock(hClipboardData);

        if (!SetClipboardData(CF_TEXT, hClipboardData))
        {
            CloseClipboard();
            std::cerr << "Error setting clipboard data" << std::endl;
            return false;
        }

        CloseClipboard();
        return true;
    }

};  // namespace CLP
