﻿#include "CommandLineEditor.h"
#include "LoggingHelpers.h"
#include <Windows.h>
#include <iostream>
#include <assert.h>
#include <format>
#include <fstream>
#include <filesystem>

#include <stdio.h>

using namespace std;
namespace fs = std::filesystem;

namespace CLP
{
//    string          appEXE;         // first argument
    RawEntryWin     rawCommandBuf;  // raw editing buffer 
    AnsiColorWin    paramListBuf;   // parsed parameter list with additional info
    AnsiColorWin    topInfoBuf;
    AnsiColorWin    usageBuf;       // simple one line drawing of usage
    InfoWin         helpBuf;        // popup help window
    ListboxWin      popupListWin;
    HistoryWin      historyWin;
    FolderList      popupFolderListWin;

    const size_t    kCommandHistoryLimit = 10; // 10 for now while developing
    tStringList     commandHistory;

    CONSOLE_SCREEN_BUFFER_INFO screenInfo;


    bool CopyTextToClipboard(const std::string& text);
    
    CommandLineEditor::CommandLineEditor()
    {
        mpCLP = nullptr;
    }

    void RawEntryWin::UpdateCursorPos(COORD newPos)
    {
        if (!mbVisible)
            return;

        int index = (int)CursorToTextIndex(newPos);
        if (index > (int)mText.length())
            index = (int)mText.length();

        mCursorPos = TextIndexToCursor(index);
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), mCursorPos);
    }


    tEnteredParams CommandLineEditor::GetPositionalEntries()
    {
        tEnteredParams posparams;
        for (auto& param : mParams)
            if (param.positionalindex >= 0)
                posparams.push_back(param);

        return posparams;
    }

    tEnteredParams CommandLineEditor::GetNamedEntries()
    {
        tEnteredParams namedparams;
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


    void RawEntryWin::FindNextBreak(int nDir)
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

    bool RawEntryWin::IsIndexInSelection(int64_t i)
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

    tStringList CommandLineEditor::GetCLPNamedParamsForMode(const std::string& sMode)
    {
        tStringList params;
        if (mpCLP)
        {
            // go over all general params
            for (auto& pd : mpCLP->mGeneralCommandLineParser.mParameterDescriptors)
            {
                if (pd.IsNamed())
                    params.push_back("-" + pd.msName + ":");
            }

            // now add any mode specific
            if (mpCLP->IsRegisteredMode(sMode))
            {
                for (auto& pd : mpCLP->mModeToCommandLineParser[sMode].mParameterDescriptors)
                {
                    if (pd.IsNamed())
                        params.push_back("-" + pd.msName + ":");
                }
            }

        }

        return params;
    }


    CLP::ParamDesc* CommandLineEditor::GetParamDesc(const std::string& sMode, int64_t position)
    {
        if (mpCLP)
        {
            ParamDesc* pDesc = nullptr;
            // if CLP is configured with a specific mode go through parameters for that mode
            if (!sMode.empty() && mpCLP->IsRegisteredMode(sMode))
            {
                CLP::CLModeParser& parser = mpCLP->mModeToCommandLineParser[sMode];

                if (parser.GetDescriptor(position, &pDesc))
                    return pDesc;
            }

            // no mode specific param, search general params
            if (mpCLP->mGeneralCommandLineParser.GetDescriptor(position, &pDesc))
                return pDesc;
        }

        return nullptr;
    }

    CLP::ParamDesc* CommandLineEditor::GetParamDesc(const std::string& sMode, std::string& paramName)
    {
        if (mpCLP)
        {
            ParamDesc* pDesc = nullptr;
            // if CLP is configured with a specific mode go through parameters for that mode
            if (!sMode.empty() && mpCLP->IsRegisteredMode(sMode))
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


    string RawEntryWin::GetSelectedText()
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

/*    bool RawEntryWin::GetParameterUnderIndex(int64_t index, size_t& outStart, size_t& outEnd, string& outParam)
    {
        if (index == string::npos || index >= (int64_t)mText.size())
            return false;




        while (index > 0 && !isblank((int)mText[index-1])) 
            index--;

        outStart = index;
        outEnd = index;

        while (outEnd < mText.size() && !isblank((int)mText[outEnd]))
            outEnd++;

        outParam = mText.substr(outStart, outEnd - outStart);
        return true;
    }*/

    bool RawEntryWin::GetParameterUnderIndex(int64_t index, size_t& outStart, size_t& outEnd, string& outParam)
    {
        for (auto& entry : mEnteredParams)
        {
            if (index >= entry.rawCommandLineStartIndex && index <= entry.rawCommandLineStartIndex + (int64_t)entry.sParamText.length())
            {
                outParam = entry.sParamText;
                outStart = (size_t)entry.rawCommandLineStartIndex;
                outEnd = outStart + outParam.length();
                return true;
            }
        }

        return false;
    }


    bool ConsoleWin::Init(int64_t l, int64_t t, int64_t r, int64_t b)
    {
        assert((r - l) > 0 && (b - t) > 0);
        SetArea(l, t, r, b);
        mbVisible = true;
        mbDone = false;
        mbCanceled = false;
        return true;
    }

    void RawEntryWin::SetText(const std::string& text)
    {
        mText = text;
        mCursorPos = TextIndexToCursor((int64_t)text.size());
    }

    void ConsoleWin::SetArea(int64_t l, int64_t t, int64_t r, int64_t b)
    {
        assert(r > l && b > t);
        mX = l;
        mY = t;

        int64_t newW = r - l;
        int64_t newH = b - t;
        if (mWidth != newW || mHeight != newH)
        {
            if (newW > 0 && newH > 0)
            {
                mBuffer.resize(newW * newH);
                mWidth = newW;
                mHeight = newH;
                Clear(mClearAttrib);
            }

        }
    }

    void RawEntryWin::SetArea(int64_t l, int64_t t, int64_t r, int64_t b)
    {
        ConsoleWin::SetArea(l, t, r, b);
        UpdateCursorPos(mCursorPos);
    }

    void ConsoleWin::GetArea(int64_t& l, int64_t& t, int64_t& r, int64_t& b)
    {
        l = mX;
        t = mY;
        r = l + mWidth;
        b = t + mHeight;
    }

void ListboxWin::Paint(tConsoleBuffer& backBuf)
{
    if (!mbVisible)
        return;

    int64_t topVisibleRow = -1;
    int64_t visibleRows = mHeight - 2;
    if (mSelection > visibleRows)
        topVisibleRow -= (visibleRows- mSelection-1);

    if (!mCaption.empty())
    {
        string sCaption(mCaption + " [" + SH::FromInt(mSelection + 1) + "/" + SH::FromInt(mEntries.size()) + "]");
        Fill(0, 0, mWidth, 1, 0);
        Fill(0, 0, 1, mHeight, 0);
        Fill(0, mHeight-1, mWidth, mHeight, 0);
        Fill(mWidth-1, 0, mWidth, mHeight, 0);
        DrawClippedAnsiText(1, 0, sCaption, false);
    }


    int64_t drawrow = -topVisibleRow;
    int64_t selection = 0;
    for (auto& entry : mEntries)
    {
        if (drawrow > 0 && drawrow < mHeight-1)
        {
            string s = entry;
            if (selection == mSelection)
                s = COL_BG_GREEN + s;
            DrawClippedAnsiText(1, drawrow, s, false);
        }

        drawrow++;
        selection++;
    }

    ConsoleWin::Paint(backBuf);
}

string ListboxWin::GetSelection()
{
    if (mSelection < 0 || mSelection >= (int64_t)mEntries.size())
        return "";

    tStringList::iterator it = mEntries.begin();
    for (int i = 0; i < mSelection; i++)
        it++;

    return *it;
}


void ListboxWin::OnKey(int keycode, char c)
{
    bool bCTRLHeld = GetKeyState(VK_CONTROL) & 0x800;
    bool bSHIFTHeld = GetKeyState(VK_SHIFT) & 0x800;

    switch (keycode)
    {
    case VK_TAB:
        if (bSHIFTHeld)
        {
            mSelection--;
            if (mSelection < 0)
                mSelection = (int64_t)mEntries.size() - 1;
        }
        else
        {
            mSelection++;
            if (mSelection >= (int64_t)mEntries.size())
                mSelection = 0;
        }
        break;
    case VK_UP:
        {
            mSelection--;
            if (mSelection < 0)
                mSelection = (int64_t)mEntries.size() - 1;
        }
        break;
    case VK_DOWN:
    {
        mSelection++;
        if (mSelection >= (int64_t)mEntries.size())
            mSelection = 0;
    }
    break;
    case VK_HOME:
        mSelection = 0;
        break;
    case VK_END:
        mSelection = (int64_t)mEntries.size() - 1;
        break;
    case VK_PRIOR:
        mSelection -= mHeight;
        if (mSelection < 0)
            mSelection = 0;
        break;
    case VK_NEXT:
        mSelection+=mHeight;
        if (mSelection >= (int64_t)mEntries.size()-1)
            mSelection = (int64_t)mEntries.size() - 1;
        break;
    case VK_RETURN:
        {
        rawCommandBuf.HandlePaste(GetSelection());
        mEntries.clear();
        mbVisible = false;
        }
        break;
    case VK_ESCAPE:
        mEntries.clear();
        mbVisible = false;
        rawCommandBuf.ClearSelection();
        break;
    }
    Clear(mClearAttrib);
}

void HistoryWin::OnKey(int keycode, char c)
{
    bool bCTRLHeld = GetKeyState(VK_CONTROL) & 0x800;
    bool bSHIFTHeld = GetKeyState(VK_SHIFT) & 0x800;

    switch (keycode)
    {
        case VK_DELETE:
        {
            if (mSelection < (int64_t)commandHistory.size())
            {
                tStringList::iterator it = commandHistory.begin();
                for (int64_t count = 0; count < mSelection; count++)
                    it++;
                commandHistory.erase(it);
                if (mSelection >= (int64_t)commandHistory.size())
                    mSelection = (int64_t)commandHistory.size()-1;
                mEntries = commandHistory;
                if (mEntries.empty())
                {
                    mbVisible = false;
                    rawCommandBuf.ClearSelection();
                }
            }
            return;
        }
    }

    ListboxWin::OnKey(keycode, c);
}


void ListboxWin::SetEntries(tStringList entries, string selectionSearch, int64_t anchor_l, int64_t anchor_b)
{ 
    mEntries = entries; 
    mAnchorL = anchor_l;
    mAnchorB = anchor_b;

        
    // find nearest selection from the entries

    size_t longestMatch = selectionSearch.length();
    mSelection = 0;
    bool bFound = false;
    while (selectionSearch.length() && !bFound)
    {
        size_t i = 0;
        for (auto& entry : mEntries)
        {
            size_t cmpLength = std::min<size_t>(entry.length(), selectionSearch.length());
            string sEntrySub(entry.substr(0, cmpLength));
            string sSearchSub(selectionSearch.substr(0, cmpLength));
            if (SH::Compare(sEntrySub, sSearchSub, false))   // exact match
            {
//                cout << "nearest match:" << entry << "\n";
                mSelection = i;
                bFound = true;
                break;      // found match
            }
            i++;
        }

        // didn't find exact..... look for one shorter
        selectionSearch = selectionSearch.substr(0, selectionSearch.length() - 1);
    }

    // find widest entry
    int64_t width = 0;
    int64_t height = mEntries.size();

    if (!mCaption.empty())
        width = mCaption.length() + 8;  // add 10 for [###/###]

    for (auto& entry : mEntries)
    {
        if (width < (int64_t)  entry.length())
            width = entry.length();
    }
    width += 2;
    height += 2;


    int64_t l = mAnchorL;
    int64_t t = mAnchorB - height;
    int64_t r = l + width;
    int64_t b = t + height;

    if (height > mAnchorB)
    {
        t = 0;
        b = mAnchorB;
    }

    if (width > screenInfo.dwSize.X)
    {
        l = 0;
        r = screenInfo.dwSize.X;
    }


    // move window to fit on screen
    if (r > screenInfo.dwSize.X)
    {
        int64_t shiftleft = r - screenInfo.dwSize.X;
        l -= shiftleft;
        r -= shiftleft;
    }
    if (l < 0)
    {
        int64_t shiftright = -l;
        l += shiftright;
        r += shiftright;
    }
    if (b > screenInfo.dwSize.Y)
    {
        int64_t shiftdown = b - screenInfo.dwSize.Y;
        t += shiftdown;
        b += shiftdown;
    }
    if (t < 0)
    {
        int64_t shiftup = -t;
        t -= shiftup;
        b -= shiftup;
    }

    SetArea(l, t, r, b);
}



void FolderList::Paint(tConsoleBuffer& backBuf)
{
    return ListboxWin::Paint(backBuf);
}

void FolderList::OnKey(int keycode, char c)
{
    bool bCTRLHeld = GetKeyState(VK_CONTROL) & 0x800;
    bool bSHIFTHeld = GetKeyState(VK_SHIFT) & 0x800;

    switch (keycode)
    {
        case VK_BACK:
        {
            fs::path navigate(mPath);
            if (navigate.has_parent_path())
                Scan(navigate.parent_path().string(), mX, mY + mHeight);
            return;
        }
        case VK_TAB:
        {
            string selection(GetSelection());
            if (fs::is_directory(selection))
                Scan(selection, mX, mY + mHeight);
            return;
        }
        case VK_RETURN:
        {
            string selection(GetSelection());
            if (SH::ContainsWhitespace(selection))
                selection = "\"" + selection + "\"";
            rawCommandBuf.HandlePaste(selection);
            mEntries.clear();
            mbVisible = false;
            return;
        }
    }

    return ListboxWin::OnKey(keycode, c);
}




    void ConsoleWin::Clear(WORD attrib)
    {
        mClearAttrib = attrib;
        for (size_t i = 0; i < mBuffer.size(); i++)
        {
            mBuffer[i].Char.AsciiChar = 0;
            mBuffer[i].Attributes = mClearAttrib;
        }
    }

    void ConsoleWin::Fill(int64_t l, int64_t t, int64_t r, int64_t b, WORD attrib)
    {
        for (int64_t y = t; y < b; y++)
        {
            for (int64_t x = l; x < r; x++)
            {
                size_t offset = y * mWidth + x;
                mBuffer[offset].Attributes = attrib;
            }
        }
    }


    void ConsoleWin::DrawCharClipped(char c, int64_t x, int64_t y, WORD attrib)
    {
        WORD foregroundColor = attrib & 0x0F;
        WORD backgroundColor = (attrib >> 4) & 0x0F;
        if (foregroundColor == backgroundColor && backgroundColor == mClearAttrib)  // would be invisible
        {
            if (backgroundColor == FOREGROUND_WHITE)
                attrib = 0;
            else
                attrib = FOREGROUND_WHITE;
        }

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
            DrawClippedText(x, y, sDraw, attribs[i], false);
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

    void RawEntryWin::DrawClippedText(int64_t x, int64_t y, std::string text, WORD attributes, bool bWrap, bool bHeightlightSelection)
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
                if (bHeightlightSelection && IsIndexInSelection(textindex))
                    DrawCharClipped(c, cursor.X, cursor.Y, attributes | BACKGROUND_INTENSITY);
                else
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
//                        attribute |= FOREGROUND_INTENSITY;
                        switch (colorCode)
                        {
                        case 30: break; // Black
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


    void ConsoleWin::Paint(tConsoleBuffer& backBuf)
    {
        // Update display
        if (!mbVisible)
            return;

        int64_t dr = screenInfo.dwSize.X;
        int64_t db = screenInfo.dwSize.Y;

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

    void RawEntryWin::Paint(tConsoleBuffer& backBuf)
    {
        // Update display
        if (!mbVisible)
            return;

        COORD cursor((SHORT)0, (SHORT)0);

        std::vector<WORD> attribs;
        attribs.resize(mText.size());


        for (size_t textindex = 0; textindex < mText.length(); textindex++)
        {
            attribs[textindex] = FOREGROUND_WHITE;
        }

        for (size_t textindex = 0; textindex < mText.length(); textindex++)
        {
            // find error params
            size_t startIndex = string::npos;
            size_t endIndex = string::npos;
            string sParamUnderCursor;
            if (GetParameterUnderIndex(textindex, startIndex, endIndex, sParamUnderCursor))
            {
#ifdef _DEBUG
                if (startIndex < 0 || endIndex > mText.length())
                {
                    int stophere = 5;
                }
#endif

                for (auto& param : mEnteredParams)
                {
                    if (param.sParamText == sParamUnderCursor)
                    {
                        for (size_t colorindex = startIndex; colorindex < endIndex; colorindex++)
                        {
                            attribs[colorindex] = param.drawAttributes;
                        }

                        textindex = endIndex;   // skip to end of param
                        break;
                    }
                }
            }
        }


        for (size_t textindex = 0; textindex < mText.length(); textindex++)
        {
            if (IsIndexInSelection(textindex))
                attribs[textindex] |= BACKGROUND_INTENSITY;
        }




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



        ConsoleWin::Paint(backBuf);
    }

    void AnsiColorWin::Paint(tConsoleBuffer& backBuf)
    {
        // Update display
        if (!mbVisible)
            return;

        DrawClippedAnsiText(0, 0, mText);
        ConsoleWin::Paint(backBuf);
    }


/*    void ConsoleWin::PaintToWindowsConsole(HANDLE hOut)
    {
        // Update display
        if (!mbVisible)
            return;

        DrawClippedAnsiText(0, 0, mText);

        COORD origin((SHORT)mX, (SHORT)mY);
        SMALL_RECT writeRegion((SHORT)mX, (SHORT)mY, (SHORT)(mX + mWidth), (SHORT)(mY + mHeight));
        COORD bufsize((SHORT)mWidth, (SHORT)mHeight);
        WriteConsoleOutput(hOut, &mBuffer[0], bufsize, origin, &writeRegion);
    }*/

    void RawEntryWin::OnKey(int keycode, char c)
    {
        bool bCTRLHeld = GetKeyState(VK_CONTROL) & 0x800;
        bool bSHIFTHeld = GetKeyState(VK_SHIFT) & 0x800;

        switch (keycode)
        {
        case VK_TAB:
            HandleParamContext();
            break;
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
                mbCanceled = true;;
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
            if (mCursorPos.Y == mY && !commandHistory.empty())
            {
                // select everything
                selectionstart = 0;
                selectionend = mText.size();

                // show history window
                historyWin.mbVisible = true;
                historyWin.mCaption = "History [DEL - Delete Entry]";
                //            popupListWin.SetArea(selectionstart, mCursorPos.Y - mAvailableModes.size()-2, selectionstart +1, mCursorPos.Y- mAvailableModes.size()-1);
                historyWin.SetEntries(commandHistory, mText, selectionstart, mCursorPos.Y);
            }
            else
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
       
        size_t rows = std::max<size_t>(1, (sRaw.size() + screenInfo.dwSize.X - 1) / screenInfo.dwSize.X);
//        rawCommandBufTopRow = screenBufferInfo.dwSize.Y - rows;

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Clear the raw command buffer and param buffers
        rawCommandBuf.Clear();
        popupListWin.Clear(BACKGROUND_INTENSITY);
        historyWin.Clear(BACKGROUND_INTENSITY);
        popupFolderListWin.Clear(BACKGROUND_INTENSITY);
        paramListBuf.Clear(BACKGROUND_BLUE);
        usageBuf.Clear(BACKGROUND_GREEN | BACKGROUND_RED);
        topInfoBuf.Clear(BACKGROUND_GREEN | BACKGROUND_RED);


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
            colWidths[kColName] = 16;
            colWidths[kColEntry] = 12;
            colWidths[kColUsage] = screenInfo.dwSize.X - (colWidths[kColName] + colWidths[kColEntry]);
            for (int paramindex = 1; paramindex < mParams.size(); paramindex++)
            {
                string sText(mParams[paramindex].sParamText);
                colWidths[kColEntry] = std::max<size_t>(sText.length(), colWidths[kColEntry]);

                ParamDesc* pPD = mParams[paramindex].pRelatedDesc;
                if (pPD)
                {
                    colWidths[kColName] = std::max<size_t>(pPD->msName.length(), colWidths[kColName]);
                    colWidths[kColUsage] = std::max<size_t>(pPD->msUsage.length(), colWidths[kColUsage]);
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
                    mpCLP->GetCommandLineExample(msMode, strings[kColUsage]);
                }
            }
            else
            {
                attribs[kColName] = FOREGROUND_RED;
                strings[kColName] = "UNKNOWN COMMAND ";
            }

            paramListBuf.DrawFixedColumnStrings(0, row, strings, colWidths, attribs);




            // next list positional params
            row+=2;
            string sSection = "-positional params-" + string(screenInfo.dwSize.X, '-');
            paramListBuf.DrawClippedText(0, row++, sSection, BACKGROUND_INTENSITY, false);


            tEnteredParams posParams = GetPositionalEntries();

            for (auto& param : posParams)
            {
                strings[kColName] = "[" + SH::FromInt(param.positionalindex) + "]";

                if (param.pRelatedDesc)
                {
                    strings[kColName] += " " + param.pRelatedDesc->msName;

                    if (param.pRelatedDesc->DoesValueSatifsy(param.sParamText))
                    {
                        strings[kColUsage] = param.pRelatedDesc->msUsage;
                        attribs[kColUsage] = FOREGROUND_WHITE;
                    }
                    else
                    {
                        strings[kColUsage] = "Parameter out of range";
                        attribs[kColEntry] = FOREGROUND_RED;
                    }
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
            sSection = "-named params-" + string(screenInfo.dwSize.X, '-');
            paramListBuf.DrawClippedText(0, row++, sSection, BACKGROUND_INTENSITY, false);

            tEnteredParams namedParams = GetNamedEntries();

            for (auto& param : namedParams)
            {
                strings[kColName] = "-";
                if (param.pRelatedDesc)
                {
                    string sName;
                    string sValue;
                    ParseParam(param.sParamText, sName, sValue);

                    if (param.pRelatedDesc->DoesValueSatifsy(sValue))
                    {
                        strings[kColName] += param.pRelatedDesc->msName;
                        attribs[kColName] = FOREGROUND_WHITE;

                        attribs[kColEntry] = FOREGROUND_GREEN;

                        strings[kColUsage] = param.pRelatedDesc->msUsage;
                        attribs[kColUsage] = FOREGROUND_WHITE;
                    }
                    else
                    {
                        strings[kColName] += param.pRelatedDesc->msName;
                        attribs[kColName] = FOREGROUND_WHITE;

                        attribs[kColEntry] = FOREGROUND_RED;

                        strings[kColUsage] = "Parameter out of range";
                        attribs[kColUsage] = FOREGROUND_RED;
                    }
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

/*        static int count = 1;
        char buf[64];
        sprintf(buf, "draw:%d\n", count++);
        OutputDebugString(buf);*/
   
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



    bool FolderList::Scan(std::string sPath, int64_t anchor_l, int64_t anchor_b)
    {
        string sCurSelection = GetSelection();
        mEntries.clear();
        if (sPath.length() >= 2)
        {
            if (sPath[0] == '\"' && sPath[sPath.length() - 1] == '\"')      // strip enclosures
                sPath = sPath.substr(1, sPath.length() - 2);

            if (sPath[0] == '\'' && sPath[sPath.length() - 1] == '\'')      // strip enclosures
                sPath = sPath.substr(1, sPath.length() - 2);
        }


        if (fs::is_directory(sPath))
            mPath = sPath;
        else
            mPath = fs::path(sPath).parent_path().string();
        mCaption = sPath;

        fs::path enteredPath(sPath);

        while (enteredPath.string().size() > 2)
        {
            try
            {
                fs::path searchPath(enteredPath.parent_path());
                if (fs::exists(searchPath))
                {
                    for (const auto& entry : fs::directory_iterator(searchPath))
                    {
                        string sEntry(entry.path().string());

                        if (entry.is_directory())
                            sEntry += "\\";

                        mEntries.push_back(sEntry);
                    }

                    break;
                }
            }
            catch (const std::filesystem::filesystem_error& e)
            {
                std::cerr << "Filesystem error: " << e.what() << std::endl;
                return false;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Error: " << e.what() << std::endl;
                return false;
            }

            enteredPath = enteredPath.parent_path();        // move up a folder and search
        }

        if (!mEntries.empty())
        {
            SetEntries(mEntries, sCurSelection, anchor_l, anchor_b);
            mbVisible = true;
        }
        else
            mbVisible = false;

        return true;
    }

    bool RawEntryWin::HandleParamContext()
    {
        size_t start = string::npos;
        size_t end = string::npos;
        string sText;

        int64_t cursorIndex = CursorToTextIndex(mCursorPos);
        if (!GetParameterUnderIndex(cursorIndex, start, end, sText))
            return false;

        selectionstart = start;
        selectionend = end;

        // Find set of auto complete for param


        // If mode..... popup modes list
        if (sText == mEnteredParams[0].sParamText)
        {
            popupListWin.mbVisible = true;
//            popupListWin.SetArea(selectionstart, mCursorPos.Y - mAvailableModes.size()-2, selectionstart +1, mCursorPos.Y- mAvailableModes.size()-1);
            popupListWin.mCaption = "Commands";
            popupListWin.SetEntries(mAvailableModes, sText, selectionstart, mCursorPos.Y);
        }
        else if (sText[0] == '-')
        {
            popupListWin.mbVisible = true;
//            popupListWin.SetArea(selectionstart, mCursorPos.Y - mAvailableNamedParams.size() - 2, selectionstart+1, mCursorPos.Y - mAvailableNamedParams.size() - 1);
            popupListWin.SetEntries(mAvailableNamedParams, sText, selectionstart, mCursorPos.Y);
        }
        else
        {
            // find param desc
            for (int i = 0; i < mEnteredParams.size(); i++)
            {
                if (sText == mEnteredParams[i].sParamText)
                {
                    if (mEnteredParams[i].pRelatedDesc && (mEnteredParams[i].pRelatedDesc->IsAPath()|| mEnteredParams[i].pRelatedDesc->MustHaveAnExistingPath()))
                    {
                        popupFolderListWin.Scan(sText, selectionstart, mCursorPos.Y);

                        if (popupFolderListWin.mEntries.size() == 1)
                        {
                            HandlePaste(*popupFolderListWin.mEntries.begin());    // only one option, fill it in
                            popupFolderListWin.mbVisible = false;
                        }
                    }
                }
            }
        }

        return true;
    }

    void CommandLineEditor::DrawToScreen()
    {
        SHORT paramListRows = (SHORT)mParams.size();


        // clear back buffer
        memset(&backBuffer[0], 0, backBuffer.size() * sizeof(CHAR_INFO));

        int64_t stride = screenInfo.dwSize.X;
        if (helpBuf.mbVisible)
        {
            //helpBuf.PaintToWindowsConsole(mhOutput);
            helpBuf.Paint(backBuffer);
        }
        else
        {
//            paramListBuf.PaintToWindowsConsole(mhOutput);
//            rawCommandBuf.PaintToWindowsConsole(mhOutput);
//            popupBuf.PaintToWindowsConsole(mhOutput);
            paramListBuf.Paint(backBuffer);
            rawCommandBuf.Paint(backBuffer);
            usageBuf.Paint(backBuffer);
            topInfoBuf.Paint(backBuffer);
            popupListWin.Paint(backBuffer);
            historyWin.Paint(backBuffer);
            popupFolderListWin.Paint(backBuffer);

            if (mpCLP)
            {
//                if (mpCLP->IsRegisteredMode(msMode))
                {
                    string sExample;
                    mpCLP->GetCommandLineExample(msMode, sExample);
                    usageBuf.SetText(string(COL_BLACK) + "usage: " + sExample);
                }
            }

            topInfoBuf.SetText(string(COL_BLACK) + "[F1 - HELP] - [ESC - CANCEL] - [TAB - Auto-Complete]");
        }

        // Finally draw to screen
        COORD origin(0, 0);
        SMALL_RECT region = { 0, 0, screenInfo.dwSize.X - 1, screenInfo.dwSize.Y - 1 };
        WriteConsoleOutput(mhOutput, &backBuffer[0], screenInfo.dwSize, origin, &region);
    }

    void CommandLineEditor::SaveConsoleState()
    {
        originalScreenInfo = screenInfo;
        originalConsoleBuf.resize(screenInfo.dwSize.X * screenInfo.dwSize.Y);
        SMALL_RECT readRegion = { 0, 0, screenInfo.dwSize.X - 1, screenInfo.dwSize.Y - 1 };
        ReadConsoleOutput(mhOutput, &originalConsoleBuf[0], screenInfo.dwSize, { 0, 0 }, &readRegion);
    }

    void CommandLineEditor::RestoreConsoleState()
    {
        SMALL_RECT writeRegion = { 0, 0, originalScreenInfo.dwSize.X - 1, originalScreenInfo.dwSize.Y - 1 };
        WriteConsoleOutput(mhOutput, &originalConsoleBuf[0], originalScreenInfo.dwSize, { 0, 0 }, &writeRegion);
        SetConsoleCursorPosition(mhOutput, originalScreenInfo.dwCursorPosition);
    }


    int64_t RawEntryWin::CursorToTextIndex(COORD coord)
    {
        int64_t i = (coord.Y-mY) * mWidth + coord.X;
        return std::min<size_t>(i, mText.size());
    }

    COORD RawEntryWin::TextIndexToCursor(int64_t i)
    {
        if (i > (int64_t)mText.size())
            i = (int64_t)mText.size();

        if (mWidth > 0)
        {
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

        return COORD(0, 0);
    }

    void RawEntryWin::HandlePaste(string text)
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

    void RawEntryWin::DeleteSelection()
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

    void RawEntryWin::ClearSelection()
    {
        selectionstart = -1;
        selectionend = -1;
    }

    void RawEntryWin::UpdateSelection()
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

    tEnteredParams CommandLineEditor::ParamsFromText(const std::string& sText)
    {
        tEnteredParams params;
        string sModeWhileParsing;

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
                {
                    endofparam = match+1;
                    break;
                }
                else
                    endofparam++;
            }

            EnteredParams param;
            param.sParamText = sText.substr(i, endofparam - i);
            param.rawCommandLineStartIndex = i;

            string sParamName;
            string sParamValue;

            if (positionalindex == -1)   // mode position
            {
                tStringList modes = GetCLPModes();
                sModeWhileParsing = param.sParamText;
                bool bModePermitted = modes.empty() || std::find(modes.begin(), modes.end(), sModeWhileParsing) != modes.end(); // if no modes registered or (if there are) if the first param matches one
                if (bModePermitted)
                {
                    param.drawAttributes = FOREGROUND_GREEN;
                }
                else
                {
                    param.drawAttributes = FOREGROUND_RED;
                }
                positionalindex++;
            }
            else if (ParseParam(param.sParamText, sParamName, sParamValue)) // is it a named parameter
            {
                param.pRelatedDesc = GetParamDesc(sModeWhileParsing, sParamName);
                if (!param.pRelatedDesc)
                    param.drawAttributes = FOREGROUND_RED;      // unknown named parameter
                else if (!param.pRelatedDesc->DoesValueSatifsy(sParamValue))
                    param.drawAttributes = FOREGROUND_RED;      // known named parameter but not in required range
            }
            else
            {
                param.positionalindex = positionalindex;
                param.pRelatedDesc = GetParamDesc(sModeWhileParsing, positionalindex);

                if (!param.pRelatedDesc)       // unsatisfied positional parameter
                    param.drawAttributes = FOREGROUND_RED;
                else if (!param.pRelatedDesc->DoesValueSatifsy(param.sParamText))     // positional param has descriptor but not in required range
                    param.drawAttributes = FOREGROUND_RED;

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
        rawCommandBuf.mEnteredParams = mParams;
        rawCommandBuf.mAvailableModes = GetCLPModes();
        rawCommandBuf.mAvailableNamedParams = GetCLPNamedParamsForMode(msMode);
    }

    std::string CommandLineEditor::Edit(int argc, char* argv[])
    {
//        appEXE = argv[0];
        tStringArray params(CommandLineParser::ToArray(argc-1, argv));    // first convert to param array then to a string which will enclose parameters with whitespaces
        return Edit(CommandLineParser::ToString(params));
    }

    void CommandLineEditor::UpdateFromConsoleSize()
    {
        CONSOLE_SCREEN_BUFFER_INFO newScreenInfo;
        if (!GetConsoleScreenBufferInfo(mhOutput, &newScreenInfo))
        {
            cerr << "Failed to get console info." << endl;
            return;
        }

        if (memcmp(&newScreenInfo, &screenInfo, sizeof(screenInfo) != 0))
        {
            screenInfo = newScreenInfo;

            SHORT w = screenInfo.dwSize.X;
            SHORT h = screenInfo.dwSize.Y;


            backBuffer.resize(w*h);
            rawCommandBuf.SetArea(0, h - 4, w, h);
            paramListBuf.SetArea(0, 1, w, h - 6);
            topInfoBuf.SetArea(0, 0, w, 1);
            usageBuf.SetArea(0, h - 5, w, h - 4);
            helpBuf.SetArea(0, 0, w, h);
//            popupListWin.SetArea(w / 4, h / 4, w * 3 / 4, h * 3 / 4);
//            popupFolderListWin.SetArea(w / 4, h / 4, w * 3 / 4, h * 3 / 4);

            UpdateDisplay();
        }
    }

    void CommandLineEditor::ShowHelp()
    {
        if (mpCLP)
        {
            helpBuf.Init(0, 0, screenInfo.dwSize.X, screenInfo.dwSize.Y);
            helpBuf.Clear(0);
            if (mpCLP->IsRegisteredMode(msMode))
                helpBuf.SetText(mpCLP->GetHelpString(msMode, false));
            else
                helpBuf.SetText(mpCLP->GetModesString());
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

        if (!GetConsoleScreenBufferInfo(mhOutput, &screenInfo))
        {
            cerr << "Failed to get console info." << endl;
            return sCommandLine;
        }

        SHORT w = screenInfo.dwSize.X;
        SHORT h = screenInfo.dwSize.Y;


        popupListWin.Init(w / 4, h / 4, w * 3 / 4, h * 3 / 4);
        popupListWin.mbVisible = false;

        historyWin.Init(w / 4, h / 4, w * 3 / 4, h * 3 / 4);
        historyWin.mbVisible = false;


        popupFolderListWin.Init(w / 4, h / 4, w * 3 / 4, h * 3 / 4);
        popupFolderListWin.mbVisible = false;



        backBuffer.resize(w*h);
        rawCommandBuf.Init(0, h - 4, w, h);
        rawCommandBuf.SetText(sCommandLine);

        paramListBuf.Init(0, 1, w, h - 6);
        rawCommandBuf.UpdateCursorPos(COORD((SHORT)sCommandLine.length(), 0));

        usageBuf.Init(0, h - 6, w, h - 5);
        topInfoBuf.Init(0, 0, w, 1);


        SaveConsoleState();
        std::vector<CHAR_INFO> blank(w*h);
        for (int i = 0; i < blank.size(); i++)
        {
            blank[i].Char.AsciiChar = ' ';
            blank[i].Attributes = 0;
        }
        SMALL_RECT smallrect(0, 0, w, h);
        WriteConsoleOutput(mhOutput, &blank[0], screenInfo.dwSize, { 0, 0 }, &smallrect);


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
        INPUT_RECORD inputRecord[128];
        DWORD numEventsRead;

        LoadHistory();

        while (!rawCommandBuf.mbDone && !rawCommandBuf.mbCanceled)
        {
            UpdateFromConsoleSize();
            UpdateParams();
            UpdateDisplay();

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

            if (PeekConsoleInput(mhInput, inputRecord, 128, &numEventsRead) && numEventsRead > 0)
            {
//                UpdateDisplay();

                for (DWORD i = 0; i < numEventsRead; i++)
                {
                    if (!ReadConsoleInput(mhInput, inputRecord, 1, &numEventsRead))
                    {
                        cerr << "Failed to read console input." << endl;
                        return "";
                    }

                    if (inputRecord[i].EventType == MOUSE_EVENT)
                    {
                        MOUSE_EVENT_RECORD mer = inputRecord[i].Event.MouseEvent;
                        if (mer.dwEventFlags == 0 && mer.dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED)
                        {
                            int64_t l;
                            int64_t t;
                            int64_t r;
                            int64_t b;
                            rawCommandBuf.GetArea(l, t, r, b);

                            COORD coord(mer.dwMousePosition);
                            coord.Y -= (SHORT)t;       // to raw buffer coordinates
                            rawCommandBuf.UpdateCursorPos(coord);
                        }
                    }
                    else if (inputRecord[i].EventType == KEY_EVENT && inputRecord[i].Event.KeyEvent.bKeyDown)
                    {
                        int keycode = inputRecord[i].Event.KeyEvent.wVirtualKeyCode;
                        char c = inputRecord[i].Event.KeyEvent.uChar.AsciiChar;

                        if (popupListWin.mbVisible)
                        {
                            popupListWin.OnKey(keycode, c);
                        }
                        else if (historyWin.mbVisible)
                        {
                            historyWin.OnKey(keycode, c);
                        }
                        else if (popupFolderListWin.mbVisible)
                        {
                            popupFolderListWin.OnKey(keycode, c);
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
                }
            }
        }


        RestoreConsoleState();
        if (rawCommandBuf.mbCanceled)
        {
            cout << "Canceled editing.\n";
            if (!rawCommandBuf.GetText().empty())
            {
                cout << string(screenInfo.dwSize.X, '*');
                cout << "Last Edit: \"" << COL_YELLOW << CLP::appName << " " << rawCommandBuf.GetText() << COL_RESET << "\"\n";
                cout << string(screenInfo.dwSize.X, '*');
                cout << "\n\n";
            }
            return "";
        }

        AddToHistory(rawCommandBuf.GetText());
        SaveHistory();

        return rawCommandBuf.GetText();
    }

    void InfoWin::Paint(tConsoleBuffer& backBuf)
    {
        if (!mbVisible)
            return;

        Clear(mClearAttrib);
        DrawClippedAnsiText(0, -firstVisibleRow, mText);

        for (int64_t y = 0; y < mHeight; y++)
        {
            int64_t readOffset = (y * mWidth);
            int64_t writeOffset = ((y + mY) * mWidth + mX);

            memcpy(&backBuf[writeOffset], &mBuffer[readOffset], mWidth * sizeof(CHAR_INFO));
        }
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

    string CommandLineEditor::HistoryPath()
    {
        string sPath = getenv("LOCALAPPDATA");
        sPath += "/" + CLP::appName + "_history";
        return sPath;
    }

    bool CommandLineEditor::LoadHistory()
    {
        if (CLP::appName.empty())
            return false;

        string sPath = HistoryPath();
        ifstream inFile(sPath);
        if (inFile)
        {
            stringstream ss;
            ss << inFile.rdbuf();
            commandHistory.clear();
            string sEncoded(ss.str());
            if (!sEncoded.empty())
            {
                SH::ToList(sEncoded, commandHistory);
                while (commandHistory.size() > kCommandHistoryLimit)
                    commandHistory.pop_front();
            }

            return true;
        }

        return false;
    }

    bool CommandLineEditor::SaveHistory()
    {
        if (CLP::appName.empty() || commandHistory.empty())
            return false;

        while (commandHistory.size() > kCommandHistoryLimit)
            commandHistory.pop_front();

        string sPath = HistoryPath();
        ofstream outFile(sPath, ios_base::trunc);
        if (outFile)
        {
            string sEncoded = SH::FromList(commandHistory);
            outFile << sEncoded;
            return true;
        }
        return false;
    }

    bool CommandLineEditor::AddToHistory(const std::string& sCommandLine)
    {
        for (tStringList::iterator it = commandHistory.begin(); it != commandHistory.end(); it++)
        {
            if (SH::Compare(*it, sCommandLine, false))
            {
                commandHistory.erase(it);
                break;
            }
        }

        commandHistory.emplace_back(sCommandLine);
        return true;
    }
};
