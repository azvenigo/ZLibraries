#include "CommandLineEditor.h"
#include "LoggingHelpers.h"
#include <Windows.h>
#include <iostream>
#include <assert.h>
#include <format>

#include <stdio.h>

using namespace std;

namespace CLP
{

    bool CopyTextToClipboard(const std::string& text);

    CommandLineEditor::CommandLineEditor()
    {
        mpCLP = nullptr;
    }

    void ConsoleWin::UpdateCursorPos(COORD newPos)
    {
        int index = (int)CursorToTextIndex(newPos);
        if (index > (int)mText.length())
            index = (int)mText.length();

        mCursorPos = TextIndexToCursor(index);
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), mCursorPos);
    }


    CommandLineEditor::tEnteredParams CommandLineEditor::GetPositionalEntries()
    {
        CommandLineEditor::tEnteredParams posparams;
        for (auto& param : mParams)
            if (param.positionalindex >= 0)
                posparams.push_back(param);

        return posparams;
    }

    CommandLineEditor::tEnteredParams CommandLineEditor::GetNamedEntries()
    {
        CommandLineEditor::tEnteredParams namedparams;
        bool bFirstParam = true;
        for (auto& param : mParams)
        {
            if (param.positionalindex < 0)
            {
                if (bFirstParam)
                {
                    bFirstParam = false;
                }
                else
                    namedparams.push_back(param);
            }
        }

        return namedparams;
    }


    void ConsoleWin::FindNextBreak(int nDir)
    {
        int index = (int)CursorToTextIndex(mCursorPos);

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

    bool ConsoleWin::IsIndexInSelection(int64_t i)
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


    tStringList CommandLineEditor::GetCLPModes()
    {
        tStringList modes;
        if (mpCLP)
        {
            for (auto& mode : mpCLP->mModeToCommandLineParser)
                modes.push_back(mode.first);
        }

        return modes;
    }

    CLP::ParamDesc* CommandLineEditor::GetParamDesc(int64_t position)
    {
        if (mpCLP)
        {
            ParamDesc* pDesc = nullptr;
            // if CLP is configured with a specific mode go through parameters for that mode
            if (!mpCLP->msMode.empty())
            {
                CLP::CLModeParser& parser = mpCLP->mModeToCommandLineParser[mpCLP->msMode];

                if (parser.GetDescriptor(position, &pDesc))
                    return pDesc;
            }

            // no mode specific param, search general params
            if (mpCLP->mGeneralCommandLineParser.GetDescriptor(position, &pDesc))
                return pDesc;
        }

        return nullptr;
    }

    CLP::ParamDesc* CommandLineEditor::GetParamDesc(std::string& paramName)
    {
        if (mpCLP)
        {
            ParamDesc* pDesc = nullptr;
            // if CLP is configured with a specific mode go through parameters for that mode
            if (!mpCLP->msMode.empty())
            {
                CLP::CLModeParser& parser = mpCLP->mModeToCommandLineParser[mpCLP->msMode];

                if (parser.GetDescriptor(paramName, &pDesc))
                    return pDesc;
            }

            // no mode specific param, search general params
            if (mpCLP->mGeneralCommandLineParser.GetDescriptor(paramName, &pDesc))
                return pDesc;
        }

        return nullptr;
    }


    string ConsoleWin::GetSelectedText()
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

    bool CommandLineEditor::GetParameterUnderCursor(size_t& outStart, size_t& outEnd, string& outParam)
    {
        outStart = rawCommandBuf.GetCursorIndex();
        if (outStart == string::npos)
            return false;

        string sText(rawCommandBuf.GetText());

        outEnd = outStart;
        while (outStart > 0 && !isblank((int)sText[outStart]))
            outStart--;
        while (outEnd < sText.size() && !isblank((int)sText[outEnd]))
            outEnd++;

        outParam = sText.substr(outStart, outEnd - outStart);
        return true;
    }


    bool ConsoleWin::Init(int64_t l, int64_t t, int64_t r, int64_t b, string sText)
    {
        SetArea(l, t, r, b);
        mText = sText;
        mCursorPos = TextIndexToCursor((int64_t)mText.size());
        mbVisible = true;
        mbDone = false;
        return true;
    }

    void ConsoleWin::SetArea(int64_t l, int64_t t, int64_t r, int64_t b)
    {
        mX = l;
        mY = t;

        int64_t newW = r - l;
        int64_t newH = b - t;
        if (mWidth != newW || mHeight != newH)
        {
            mBuffer.resize(newW * newH);
            mWidth = newW;
            mHeight = newH;
            Clear();
        }
    }

    void ConsoleWin::GetArea(int64_t& l, int64_t& t, int64_t& r, int64_t& b)
    {
        l = mX;
        t = mY;
        r = l + mWidth;
        b = t + mHeight;
    }



    void ConsoleWin::Clear(WORD attrib)
    {
        for (size_t i = 0; i < mBuffer.size(); i++)
        {
            mBuffer[i].Char.AsciiChar = 0;
            mBuffer[i].Attributes = attrib;
        }

    }

    void ConsoleWin::DrawCharClipped(char c, int64_t x, int64_t y, WORD attrib)
    {
        WORD foregroundColor = attrib & 0x0F;
        WORD backgroundColor = (attrib >> 4) & 0x0F;
        if (foregroundColor == backgroundColor)
            attrib = FOREGROUND_WHITE;

        size_t offset = y * mWidth + x;
        DrawCharClipped(c, (int64_t)offset, attrib);
    }

    void ConsoleWin::DrawCharClipped(char c, int64_t offset, WORD attrib)
    {
        if (offset >= 0 && offset < (int64_t)mBuffer.size())    // clip
        {
            mBuffer[offset].Char.AsciiChar = c;
            mBuffer[offset].Attributes |= attrib;
        }
    }



    void ConsoleWin::DrawFixedColumnStrings(int64_t x, int64_t y, tStringArray& strings, vector<size_t>& colWidths, tAttribArray attribs)
    {
        assert(strings.size() == colWidths.size() && colWidths.size() == attribs.size());

        for (int i = 0; i < strings.size(); i++)
        {
            string sDraw(strings[i].substr(0, colWidths[i]));
            DrawClippedText(x, y, sDraw, attribs[i]);
            x += colWidths[i];
        }
    }

    void ConsoleWin::DrawClippedText(int64_t x, int64_t y, std::string text, WORD attributes, bool bWrap)
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
                DrawCharClipped(c, cursor.X, cursor.Y, attributes);
            }

            cursor.X++;
            if (cursor.X >= mWidth && !bWrap)
                break;
        }
    }

    bool getANSIColorAttribute(const std::string& str, size_t offset, WORD& attribute, size_t& length)
    {
        // Check if there are enough characters after the offset to form an ANSI sequence
        if (offset + 2 >= str.size())
        {
            return false; // Not a valid ANSI sequence
        }

        // Check if the sequence starts with the ANSI escape character '\x1B' (27 in decimal)
        if (str[offset] != '\x1B')
        {
            return false; // Not a valid ANSI sequence
        }

        // Check if the next character is '[' which indicates the beginning of an ANSI sequence
        if (str[offset + 1] != '[')
        {
            return false; // Not a valid ANSI sequence
        }

        // Find the end of the ANSI sequence
        size_t endIndex = offset + 2;
        while (endIndex < str.size() && str[endIndex] != 'm')
        {
            endIndex++;
        }

        // Extract the ANSI sequence
        std::string sequence = str.substr(offset, endIndex - offset + 1);

        // Check if the sequence contains color information
        if (sequence.find("m") != std::string::npos)
        {
            // Convert ANSI color code to Windows console attribute
            attribute = 0;
            size_t pos = sequence.find("[");
            if (pos != std::string::npos)
            {
                std::string colorStr = sequence.substr(pos + 1, sequence.size() - 2); // Extract color code part
                std::vector<std::string> colorCodes;
                size_t start = 0;
                size_t comma = colorStr.find(",");
                while (comma != std::string::npos)
                {
                    colorCodes.push_back(colorStr.substr(start, comma - start));
                    start = comma + 1;
                    comma = colorStr.find(",", start);
                }
                colorCodes.push_back(colorStr.substr(start, colorStr.size() - start));

                for (const auto& code : colorCodes)
                {
                    int colorCode = std::stoi(code);
                    if (colorCode >= 30 && colorCode <= 37)
                    { // Foreground colors
                        attribute |= FOREGROUND_INTENSITY;
                        switch (colorCode)
                        {
                        case 30: attribute |= FOREGROUND_BLUE; break;
                        case 31: attribute |= FOREGROUND_RED; break;
                        case 32: attribute |= FOREGROUND_GREEN; break;
                        case 33: attribute |= FOREGROUND_RED | FOREGROUND_GREEN; break; // Yellow
                        case 34: attribute |= FOREGROUND_BLUE | FOREGROUND_GREEN; break; // Cyan
                        case 35: attribute |= FOREGROUND_RED | FOREGROUND_BLUE; break; // Magenta
                        case 36: attribute |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; break; // White
                        case 37: break; // Default color
                        }
                    }
                    else if (colorCode >= 40 && colorCode <= 47)
                    { // Background colors
                        attribute |= BACKGROUND_INTENSITY;
                        attribute |= (colorCode - 40) << 4; // Shift left by 4 bits to set the background color
                    }
                }
            }
            length = endIndex - offset + 1;
            return true;
        }

        return false; // Not a valid ANSI color sequence
    }

    void ConsoleWin::GetTextOuputRect(std::string text, int64_t& w, int64_t& h)
    {
        w = 0;
        h = 0;
        int64_t x = 0;
        int64_t y = 0;
        WORD attrib = FOREGROUND_WHITE;
        for (size_t i = 0; i < text.length(); i++)
        {
            size_t skiplength = 0;
            if (getANSIColorAttribute(text, i, attrib, skiplength))
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


    void ConsoleWin::DrawClippedAnsiText(int64_t x, int64_t y, std::string ansitext, bool bWrap)
    {
        COORD cursor((SHORT)x, (SHORT)y);

        WORD attrib = FOREGROUND_WHITE;

        for (size_t i = 0; i < ansitext.length(); i++)
        {
            size_t skiplength = 0;
            if (getANSIColorAttribute(ansitext, i, attrib, skiplength))
            {
                i += skiplength - 1;    // -1 so that the i++ above will be correct
            }
            else
            {
                char c = ansitext[i];
                if (c == '\n' && bWrap)
                {
                    cursor.X = 0;
                    cursor.Y++;
                }
                else
                {
                    DrawCharClipped(c, cursor.X, cursor.Y, attrib);
                }

                cursor.X++;
                if (cursor.X >= mWidth && !bWrap)
                    break;
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

    void ConsoleWin::PaintToWindowsConsole(HANDLE hOut)
    {
        // Update display
        if (!mbVisible)
            return;
/*
        int64_t rawCommandRows = (mText.size() + mWidth - 1) / mWidth;
        int64_t firstRenderRow = 0;

        if (rawCommandRows > mHeight)
            firstRenderRow = mHeight - rawCommandRows;    // could be negative as we're clipping into the raw command console

        int64_t consoleBufIndex = firstRenderRow * mWidth;
        for (size_t i = 0; i < mText.length(); i++)
        {
            WORD attrib = FOREGROUND_WHITE;
            if (IsIndexInSelection(i))
                attrib |= BACKGROUND_INTENSITY;
            DrawCharClipped(mText[i], consoleBufIndex, attrib);
            consoleBufIndex++;
        }*/

        DrawClippedAnsiText(0, 0, mText);





        COORD origin(0, 0);
        SMALL_RECT writeRegion((SHORT)mX, (SHORT)mY, (SHORT)(mX + mWidth), (SHORT)(mY + mHeight));
        COORD bufsize((SHORT)mWidth, (SHORT)mHeight);
        WriteConsoleOutput(hOut, &mBuffer[0], bufsize, origin, &writeRegion);
    }

    void ConsoleWin::OnKey(int keycode, char c)
    {
        static int count = 0;

        char buf[64];
        sprintf(buf, "count:%d key:%c\n", count++, c);
        OutputDebugString(buf);




        bool bCTRLHeld = GetKeyState(VK_CONTROL) & 0x800;
        bool bSHIFTHeld = GetKeyState(VK_SHIFT) & 0x800;

        switch (keycode)
        {
        case VK_RETURN:
            mbDone = true;
            return;
        case VK_ESCAPE:
        {
            if (IsTextSelected())
            {
                ClearSelection();
            }
            else
            {
                mText = "";
                mbDone = true;
            }
        }
        return;
        case VK_HOME:
        {
            UpdateSelection();
            UpdateCursorPos(TextIndexToCursor(0));
            UpdateSelection();
        }
        return;
        case VK_END:
        {
            UpdateSelection();
            UpdateCursorPos(TextIndexToCursor((int64_t)mText.size()));
            UpdateSelection();
        }
        return;
        case VK_UP:
        {
            UpdateSelection();
            COORD newPos = mCursorPos;

            if (newPos.Y > mY)
            {
                newPos.Y--;
                UpdateCursorPos(newPos);
            }

            UpdateSelection();
        }     
        return;
        case VK_DOWN:
        {
            UpdateSelection();
            int64_t newindex = CursorToTextIndex(mCursorPos) + mWidth;
            if (newindex < (int64_t)mText.size())
            {
                COORD newPos = mCursorPos;
                newPos.Y++;
                UpdateCursorPos(newPos);
            }
            UpdateSelection();
        }
        return;
        case VK_LEFT:
        {
            UpdateSelection();
            // Move cursor left
            int64_t index = CursorToTextIndex(mCursorPos);
            if (index > 0)
            {
                if (bCTRLHeld)
                    FindNextBreak(-1);
                else
                    UpdateCursorPos(TextIndexToCursor(index - 1));
            }
            UpdateSelection();
        }
        return;
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
                int64_t index = CursorToTextIndex(mCursorPos);
                if (index < (int64_t)mText.size())
                {
                    UpdateCursorPos(TextIndexToCursor(index + 1));
                }

            }
            UpdateSelection();
        }
        return;
        case VK_BACK:
        {
            if (IsTextSelected())
            {
                DeleteSelection();
            }
            else
            {
                // Delete character before cursor
                int64_t index = CursorToTextIndex(mCursorPos);
                if (index > 0)
                {
                    mText.erase(index - 1, 1);
                    UpdateCursorPos(TextIndexToCursor(index - 1));
                }
                UpdateSelection();
            }
        }
        return;
        case VK_DELETE:
        {
            if (IsTextSelected())
            {
                DeleteSelection();
            }
            else
            {
                // Delete character at cursor
                int64_t index = CursorToTextIndex(mCursorPos);
                if (index < (int64_t)(mText.size()))
                {
                    mText.erase(index, 1);
                }
            }
            UpdateSelection();
        }
        return;
        case 0x41:  
        {
            if (bCTRLHeld)  // CTRL-A
            {
                selectionstart = 0;
                selectionend = mText.length();
                return;
            }
        }
        break;
        case 0x43:
        {
            if (bCTRLHeld)  // CTRL-C
            {
                // handle copy
                CopyTextToClipboard(GetSelectedText());
                return;
            }
        }
        break;
        }

        // nothing handled above....regular text entry
        if (keycode >= 32)
        {
            if (IsTextSelected())
                DeleteSelection();

            // Insert character at cursor position
            int index = (int)CursorToTextIndex(mCursorPos);
            mText.insert(index, 1, c);
            UpdateCursorPos(TextIndexToCursor(index + 1));
            UpdateSelection();
        }
    }




    void CommandLineEditor::UpdateDisplay()
    {

        string sRaw(rawCommandBuf.GetText());
       
        size_t rows = std::max<size_t>(1, (sRaw.size() + mScreenInfo.dwSize.X - 1) / mScreenInfo.dwSize.X);
//        rawCommandBufTopRow = screenBufferInfo.dwSize.Y - rows;

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Clear the raw command buffer and param buffers
        rawCommandBuf.Clear();
        paramListBuf.Clear(BACKGROUND_BLUE);
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // if the command line is bigger than the screen, show the last n rows that fit


        if (!mParams.empty())   // if there are params entered
        {
            // compute column widths

            const int kColName = 0;
            const int kColEntry = 1;
            const int kColUsage = 2;

            vector<size_t> colWidths;
            colWidths.resize(3);
            colWidths[kColName] = 12;
            colWidths[kColEntry] = 12;
            colWidths[kColUsage] = mScreenInfo.dwSize.X - (colWidths[kColName] + colWidths[kColEntry]);
            for (int paramindex = 1; paramindex < mParams.size(); paramindex++)
            {
                string sText(mParams[paramindex].sParamText);
                colWidths[kColEntry] = std::max<size_t>(sText.length(), colWidths[0]);

                ParamDesc* pPD = mParams[paramindex].pRelatedDesc;
                if (pPD)
                {
                    colWidths[kColName] = std::max<size_t>(pPD->msName.length(), colWidths[1]);
                    colWidths[kColUsage] = std::max<size_t>(pPD->msUsage.length(), colWidths[2]);
                }
            }

            for (auto& i : colWidths)   // pad all cols
                i+=2;

            tStringArray strings(3);
            tAttribArray attribs(3);


            // first param is command
            size_t row = 0;
            msMode = mParams[0].sParamText;

            strings[kColName] = "COMMAND";
            attribs[kColName] = FOREGROUND_WHITE;

            strings[kColEntry] = msMode;
            attribs[kColUsage] = FOREGROUND_WHITE;

            tStringList modes = GetCLPModes();
            bool bModePermitted = modes.empty() || std::find(modes.begin(), modes.end(), msMode) != modes.end(); // if no modes registered or (if there are) if the first param matches one
            if (bModePermitted)
            {
                attribs[kColEntry] = FOREGROUND_GREEN;

                if (mpCLP)
                {
                    mpCLP->GetCommandLineExample(strings[kColUsage]);
                }
            }
            else
            {
                attribs[kColEntry] = FOREGROUND_RED;
                strings[kColUsage] = "Unknown Command";
            }

            paramListBuf.DrawFixedColumnStrings(0, row, strings, colWidths, attribs);




            // next list positional params
            row+=2;
            paramListBuf.DrawClippedText(0, row++, "*Positional Parameters*");


            tEnteredParams posParams = GetPositionalEntries();

            for (auto& param : posParams)
            {
                strings[kColName] = "[" + SH::FromInt(param.positionalindex) + "]";

                if (param.pRelatedDesc)
                {
                    strings[kColName] += " " + param.pRelatedDesc->msName;
                    strings[kColUsage] = param.pRelatedDesc->msUsage;
                }
                else
                {
                    strings[kColUsage] = "Unexpected parameter";
                    attribs[kColUsage] = FOREGROUND_RED;
                }

                strings[kColEntry] = param.sParamText;

                paramListBuf.DrawFixedColumnStrings(0, row++, strings, colWidths, attribs);
            }


            row++;
            paramListBuf.DrawClippedText(0, row++, "*Named Parameters*");

            tEnteredParams namedParams = GetNamedEntries();

            for (auto& param : namedParams)
            {
                strings[kColName] = "-";
                if (param.pRelatedDesc)
                {
                    strings[kColName] += param.pRelatedDesc->msName;
                    attribs[kColName] = FOREGROUND_WHITE;

                    attribs[kColEntry] = FOREGROUND_GREEN;

                    strings[kColUsage] = param.pRelatedDesc->msUsage;
                    attribs[kColUsage] = FOREGROUND_WHITE;
                }
                else
                {
                    attribs[kColEntry] = FOREGROUND_RED;
                    strings[kColUsage] = "Unknown parameter";
                    attribs[kColUsage] = FOREGROUND_RED;
                }


                strings[kColEntry] = param.sParamText;

                paramListBuf.DrawFixedColumnStrings(0, row++, strings, colWidths, attribs);
            }
        }

        DrawToScreen();

        static int count = 1;
        char buf[64];
        sprintf(buf, "draw:%d\n", count++);
        OutputDebugString(buf);
   
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

    bool CommandLineEditor::HandleParamContext()
    {
        size_t start = string::npos;
        size_t end = string::npos;
        string sText;
        if (!GetParameterUnderCursor(start, end, sText))
            return false;

        // Find set of auto complete for param


        return true;
    }

    void CommandLineEditor::DrawToScreen()
    {
        SHORT paramListRows = (SHORT)mParams.size();


        tConsoleBuffer blank;
        blank.resize(mScreenInfo.dwSize.X * mScreenInfo.dwSize.Y);

/*        COORD origin(0, 0);
        SHORT paramListTop = (SHORT)rawCommandBufTopRow - paramListRows - 1;
        SMALL_RECT clearRegion = { 0, 0, screenBufferInfo.dwSize.X - 1, paramListTop-1};
        WriteConsoleOutput(mhOutput, &blank[0], screenBufferInfo.dwSize, origin, &clearRegion);*/

        if (helpBuf.mbVisible)
        {
            helpBuf.PaintToWindowsConsole(mhOutput);
        }
        else
        {
            paramListBuf.PaintToWindowsConsole(mhOutput);
            rawCommandBuf.PaintToWindowsConsole(mhOutput);
            popupBuf.PaintToWindowsConsole(mhOutput);
        }
    }

    void CommandLineEditor::SaveConsoleState()
    {
        originalConsoleBuf.resize(mScreenInfo.dwSize.X * mScreenInfo.dwSize.Y);
        SMALL_RECT readRegion = { 0, 0, mScreenInfo.dwSize.X - 1, mScreenInfo.dwSize.Y - 1 };
        ReadConsoleOutput(mhOutput, &originalConsoleBuf[0], mScreenInfo.dwSize, { 0, 0 }, &readRegion);
    }

    void CommandLineEditor::RestoreConsoleState()
    {
        SMALL_RECT writeRegion = { 0, 0, mScreenInfo.dwSize.X - 1, mScreenInfo.dwSize.Y - 1 };
        WriteConsoleOutput(mhOutput, &originalConsoleBuf[0], mScreenInfo.dwSize, { 0, 0 }, &writeRegion);
        SetConsoleCursorPosition(mhOutput, mScreenInfo.dwCursorPosition);
    }


    int64_t ConsoleWin::CursorToTextIndex(COORD coord)
    {
        int64_t i = (coord.Y-mY) * mWidth + coord.X;
        return std::min<size_t>(i, mText.size());
    }

    COORD ConsoleWin::TextIndexToCursor(int64_t i)
    {
        if (i > (int64_t)mText.size())
            i = (int64_t)mText.size();

        int64_t rawCommandRows = ((int64_t)mText.size() + mWidth - 1) / mWidth;
        int64_t firstVisibleRow = 0;

        if (rawCommandRows > mHeight)
            firstVisibleRow = rawCommandRows - mHeight;    // could be negative as we're clipping into the raw command console

        int64_t hiddenChars = firstVisibleRow * mWidth;

        i -= (size_t)hiddenChars;

        COORD c;
        c.X = (SHORT)(i) % mWidth;
        c.Y = ((SHORT)mY + (SHORT)(i / mWidth));
        return c;
    }

    void ConsoleWin::HandlePaste(string text)
    {
        DeleteSelection();  // delete any selection if needed
        int64_t curindex = CursorToTextIndex(mCursorPos);
        mText.insert(curindex, text);
        curindex += (int)text.length();
        UpdateCursorPos(TextIndexToCursor(curindex));

        static int count = 1;
        char buf[64];
        sprintf(buf, "paste:%d\n", count++);
        OutputDebugString(buf);
    }

    void ConsoleWin::DeleteSelection()
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

        int curindex = (int)CursorToTextIndex(mCursorPos);
        if (curindex > normalizedStart)
            curindex -= (int)(curindex- normalizedStart);
        UpdateCursorPos(TextIndexToCursor(curindex));

        ClearSelection();
    }

    void ConsoleWin::ClearSelection()
    {
        selectionstart = -1;
        selectionend = -1;
    }

    void ConsoleWin::UpdateSelection()
    {
        if (!(GetKeyState(VK_SHIFT) & 0x800))
        {
            ClearSelection();
        }
        else
        {
            if (selectionstart == -1)
            {
                selectionstart = CursorToTextIndex(mCursorPos);
            }
            selectionend = CursorToTextIndex(mCursorPos);
        }
    }

    bool CommandLineEditor::ParseParam(const std::string sParamText, std::string& outName, std::string& outValue)
    {
        // named parameters always start with -
        if (!sParamText.empty() && sParamText[0] == '-')
        {
            size_t nIndexOfColon = sParamText.find(':');
            if (nIndexOfColon != string::npos)
            {
                outName = sParamText.substr(1, nIndexOfColon - 1).c_str();    // everything from first char to colon
                outValue = sParamText.substr(nIndexOfColon + 1);    // everything after colon
            }
            else
            {
                // flag with no value is the same as -flag:true
                outName = sParamText.substr(1, nIndexOfColon).c_str();
            }
            return true;
        }

        return false;   // not a named param
    }

    std::string CommandLineEditor::EnteredParamsToText()
    {
        std::string sText;
        for (int i = 0; i < mParams.size(); i++)
        {
            sText += mParams[i].sParamText + " ";
        }

        if (!sText.empty())
            sText = sText.substr(0, sText.length() - 1);    // strip last space

        return sText;
    }

    CommandLineEditor::tEnteredParams CommandLineEditor::ParamsFromText(const std::string& sText)
    {
        CommandLineEditor::tEnteredParams params;

        int positionalindex = -1;
        size_t length = sText.length();
        for (size_t i = 0; i < sText.length(); i++)
        {   
            // find start of param
            while (isblank(sText[i]) && i < length) // skip whitespace
                i++;

            size_t endofparam = i;
            // find end of param
            while (!isblank(sText[endofparam]) && endofparam < length)
            {
                // if this is an enclosing
                size_t match = SH::FindMatching(sText, endofparam);
                if (match != string::npos) // if enclosure, skip to endYour location
                    endofparam = match;
                else
                    endofparam++;
            }

            EnteredParams param;
            param.sParamText = sText.substr(i, endofparam - i);

            string sParamName;
            string sParamValue;

            if (positionalindex == -1)   // mode position
            {
                positionalindex++;
            }
            else if (ParseParam(param.sParamText, sParamName, sParamValue)) // is it a named parameter
            {
                param.pRelatedDesc = GetParamDesc(sParamName);
            }
            else
            {
                param.positionalindex = positionalindex;
                param.pRelatedDesc = GetParamDesc(positionalindex);

                if (param.pRelatedDesc)
                {
                    if (!param.pRelatedDesc->Satisfied())
                    {
                        param.drawAttributes = FOREGROUND_RED;
                    }
                }

                positionalindex++;
            }

            params.push_back(param);

            i = endofparam;
        }

        return params;
    }

    void CommandLineEditor::UpdateParams()
    {
        string sText = rawCommandBuf.GetText();
        if (mLastParsedText == sText)
            return;

        mLastParsedText = sText;

        mParams = ParamsFromText(mLastParsedText);
    }

    std::string CommandLineEditor::Edit(int argc, char* argv[])
    {
        string sCommandLine;
        for (int i = 1; i < argc; i++)  // skip app in argv[0]
        {
            sCommandLine += string(argv[i]) + " ";
        }
        if (!sCommandLine.empty())
            sCommandLine = sCommandLine.substr(0, sCommandLine.length() - 1);   // strip last ' '

        return Edit(sCommandLine);
    }

    void CommandLineEditor::UpdateFromConsoleSize()
    {
        CONSOLE_SCREEN_BUFFER_INFO screenInfo;
        if (!GetConsoleScreenBufferInfo(mhOutput, &screenInfo))
        {
            cerr << "Failed to get console info." << endl;
            return;
        }

        if (memcmp(&screenInfo, &mScreenInfo, sizeof(screenInfo) != 0))
        {
            mScreenInfo = screenInfo;
            rawCommandBuf.SetArea(0, mScreenInfo.dwSize.Y - 4, mScreenInfo.dwSize.X, mScreenInfo.dwSize.Y);
            paramListBuf.SetArea(0, 0, mScreenInfo.dwSize.X, mScreenInfo.dwSize.Y - 4);
            helpBuf.SetArea(0, 0, mScreenInfo.dwSize.X, mScreenInfo.dwSize.Y);
        }



    }

    void CommandLineEditor::ShowHelp()
    {
        if (mpCLP)
        {
            string help;
            if (mpCLP->IsRegisteredMode(msMode))
                help = mpCLP->GetHelpString(msMode, true);
            else
                help = mpCLP->GetModesString();

//            std::string help = "\x1B[32;41mHello\x1B[0m";
            helpBuf.Init(0, 0, mScreenInfo.dwSize.X, mScreenInfo.dwSize.Y, help);
        }
    }


    string CommandLineEditor::Edit(const string& sCommandLine)
    {
        // Get the handle to the standard input
        mhInput = GetStdHandle(STD_INPUT_HANDLE);
        mhOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        if (mhInput == INVALID_HANDLE_VALUE || mhOutput == INVALID_HANDLE_VALUE)
        {
            cerr << "Failed to get standard input/output handle." << endl;
            return "";
        }

        int MY_HOTKEY_ID = 1;

        if (!RegisterHotKey(NULL, MY_HOTKEY_ID, MOD_CONTROL, 'V'))
        {
            std::cerr << "Error registering hotkey" << std::endl;
            return "";
        }

        if (!GetConsoleScreenBufferInfo(mhOutput, &mScreenInfo))
        {
            cerr << "Failed to get console info." << endl;
            return sCommandLine;
        }

        rawCommandBuf.Init(0, mScreenInfo.dwSize.Y - 4, mScreenInfo.dwSize.X, mScreenInfo.dwSize.Y, sCommandLine);
        paramListBuf.Init(0, 0, mScreenInfo.dwSize.X, mScreenInfo.dwSize.Y - 4, "");

        SaveConsoleState();


        // Set console mode to allow reading mouse and key events
        DWORD mode;
        if (!GetConsoleMode(mhInput, &mode)) 
        {
            cerr << "Failed to get console mode." << endl;
            return sCommandLine;
        }
        mode |= ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT;
        mode &= ~ENABLE_PROCESSED_INPUT ;
        //mode |= ENABLE_PROCESSED_INPUT;
        if (!SetConsoleMode(mhInput, mode  ))
        {
            cerr << "Failed to set console mode." << endl;
            return sCommandLine;
        }

        // Main loop to read input events
        INPUT_RECORD inputRecord;
        DWORD numEventsRead;

        while (!rawCommandBuf.mbDone)
        {
            UpdateFromConsoleSize();
            UpdateParams();

            // Check for hotkey events
            MSG msg;
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_HOTKEY && msg.wParam == MY_HOTKEY_ID)
                {
                    rawCommandBuf.HandlePaste(GetTextFromClipboard());
                }
                else
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }

            if (PeekConsoleInput(mhInput, &inputRecord, 1, &numEventsRead) && numEventsRead > 0)
            {
                UpdateDisplay();

                if (!ReadConsoleInput(mhInput, &inputRecord, 1, &numEventsRead))
                {
                    cerr << "Failed to read console input." << endl;
                    return "";
                }

                if (inputRecord.EventType == KEY_EVENT && inputRecord.Event.KeyEvent.bKeyDown)
                {
                    int keycode = inputRecord.Event.KeyEvent.wVirtualKeyCode;
                    char c = inputRecord.Event.KeyEvent.uChar.AsciiChar;

                    if (popupBuf.mbVisible)
                    {
                        popupBuf.OnKey(keycode, c);
                    }
                    else if (helpBuf.mbVisible)
                    {
                        helpBuf.OnKey(keycode, c);
                    }
                    else
                    {
                        if (keycode == VK_F1)
                            ShowHelp();
                        else
                            rawCommandBuf.OnKey(keycode, c);
                    }
                }

                UpdateDisplay();
            }
        }


        RestoreConsoleState();
        cout << "restoring\n";

        return rawCommandBuf.GetText();
    }

    void InfoWin::PaintToWindowsConsole(HANDLE hOut)
    {
        if (!mbVisible)
            return;

        Clear(BACKGROUND_INTENSITY);
        DrawClippedAnsiText(0, -firstVisibleRow, mText);

        COORD origin(0, 0);
        SMALL_RECT writeRegion((SHORT)mX, (SHORT)mY, (SHORT)(mX + mWidth), (SHORT)(mY + mHeight));
        COORD bufsize((SHORT)mWidth, (SHORT)mHeight);
        WriteConsoleOutput(hOut, &mBuffer[0], bufsize, origin, &writeRegion);
    }

    void InfoWin::OnKey(int keycode, char c)
    {
        if (keycode == VK_F1 || keycode == VK_ESCAPE)
        {
            mText.clear();
            mbVisible = false;
            mbDone = true;
        }
        else if (keycode == VK_UP)
        {
            if (firstVisibleRow > 0)
                firstVisibleRow--;
        }
        else if (keycode == VK_DOWN)
        {
            int64_t w = 0;
            int64_t h = 0;
            GetTextOuputRect(mText, w, h);

            if (firstVisibleRow < (h - mHeight))
                firstVisibleRow++;
        }
    }

};
