#ifdef ENABLE_CLE

#include "CommandLineEditor.h"
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

// Styles
const ZAttrib kAttribCaption(BLACK | MAKE_BG(GOLD));
const ZAttrib kAttribSection(CYAN);

const ZAttrib kAttribFolderListBG(MAKE_BG(0xff000044));

const ZAttrib kAttribListBoxBG(MAKE_BG(ORANGE));
const ZAttrib kAttribListBoxEntry(WHITE);
const ZAttrib kAttribListBoxSelectedEntry(BLACK | MAKE_BG(YELLOW));

const ZAttrib kAttribParamListBG(MAKE_BG(0xff4444aa));
const ZAttrib kAttribTopInfoBG(BLACK|MAKE_BG(0xFF333399));

const ZAttrib kAttribHelpBG(0xFF444444FF000000);

const ZAttrib kRawText(WHITE);
const ZAttrib kSelectedText(WHITE | MAKE_BG(0xFF999999));
const ZAttrib kGoodParam(GREEN);
const ZAttrib kBadParam(RED);
const ZAttrib kUnknownParam(YELLOW);



namespace CLP
{
    CommandLineParser* pCLP = nullptr;
    CommandLineEditor* pCLE = nullptr;

    RawEntryWin     rawCommandBuf;  // raw editing buffer 
    ParamListWin    paramListBuf;   // parsed parameter list with additional info
    AnsiColorWin    topInfoBuf;
    AnsiColorWin    usageBuf;       // simple one line drawing of usage
    InfoWin         helpWin;        // popup help window
    ListboxWin      popupListWin;
    HistoryWin      historyWin;
    FolderList      popupFolderListWin;

    const size_t    kCommandHistoryLimit = 10; // 10 for now while developing
    tStringList     commandHistory;

    tEnteredParams  enteredParams;
    tStringList     availableModes;
    tStringList     availableNamedParams;

    const string    kEmptyFolderCaption("[EMPTY]");

    CONSOLE_SCREEN_BUFFER_INFO screenInfo;
    inline SHORT ScreenW() { return screenInfo.srWindow.Right - screenInfo.srWindow.Left+1; }
    inline SHORT ScreenH() { return screenInfo.srWindow.Bottom - screenInfo.srWindow.Top+1; }
    bool bScreenChanged = false;


    string GetMode()
    {
        if (enteredParams.size() > 0)
            return enteredParams[0].sParamText;
        return "";
    }

    tEnteredParams GetPositionalEntries()
    {
        tEnteredParams posparams;
        for (auto& param : enteredParams)
            if (param.positionalindex >= 0)
                posparams.push_back(param);

        return posparams;
    }

    tEnteredParams GetNamedEntries()
    {
        tEnteredParams namedparams;
        for (auto& param : enteredParams)
        {
            if (param.sParamText[0] == '-')
            {
                namedparams.push_back(param);
            }
        }

        return namedparams;
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

    CommandLineEditor::CommandLineEditor()
    {
        pCLP = nullptr;
        pCLE = this;
    }

    void RawEntryWin::UpdateFirstVisibleRow()
    {
        if (!mbVisible)
            return;

        int64_t rowCount = ((int64_t)mText.size() + mWidth - 1) / mWidth;

        if (mLocalCursorPos.Y < 0)
        {
            firstVisibleRow = firstVisibleRow+mLocalCursorPos.Y;
            UpdateCursorPos(COORD(mLocalCursorPos.X, 0));
        }
        else if (mLocalCursorPos.Y >= mHeight)
        {
            firstVisibleRow = firstVisibleRow + mLocalCursorPos.Y - mHeight + 1;
            UpdateCursorPos(COORD(mLocalCursorPos.X, (SHORT)mHeight-1));
        }
    }


    void RawEntryWin::UpdateCursorPos(COORD localPos)
    {
        if (!mbVisible)
            return;

        int index = (int)CursorToTextIndex(localPos);
        if (index > (int)mText.length())
            index = (int)mText.length();
        mLocalCursorPos = TextIndexToCursor(index);

//        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), LocalCursorToGlobal(mLocalCursorPos));

        COORD global = LocalCursorToGlobal(mLocalCursorPos);
        cout << "\033[" << global.X+1 << "G\033[" << global.Y << "d" << std::flush;
    }


    COORD RawEntryWin::LocalCursorToGlobal(COORD cursor)
    {
        return COORD(cursor.X + (SHORT)mX, cursor.Y + (SHORT)mY);
    }



    void RawEntryWin::FindNextBreak(int nDir)
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

    void CommandLineEditor::SetConfiguredCLP(CommandLineParser* _pCLP)
    { 
        pCLP = _pCLP; 
        availableModes.clear();
        if (pCLP)
        {
            for (auto& mode : pCLP->mModeToCommandLineParser)
                availableModes.push_back(mode.first);
        }
    }

    tStringList CommandLineEditor::GetCLPNamedParamsForMode(const std::string& sMode)
    {
        tStringList params;
        if (pCLP)
        {
            // go over all general params
            for (auto& pd : pCLP->mGeneralCommandLineParser.mParameterDescriptors)
            {
                if (pd.IsNamed())
                    params.push_back("-" + pd.msName + ":");
            }

            // now add any mode specific
            if (pCLP->IsRegisteredMode(sMode))
            {
                for (auto& pd : pCLP->mModeToCommandLineParser[sMode].mParameterDescriptors)
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
        if (pCLP)
        {
            ParamDesc* pDesc = nullptr;
            // if CLP is configured with a specific mode go through parameters for that mode
            if (!sMode.empty() && pCLP->IsRegisteredMode(sMode))
            {
                CLP::CLModeParser& parser = pCLP->mModeToCommandLineParser[sMode];

                if (parser.GetDescriptor(position, &pDesc))
                    return pDesc;
            }

            // no mode specific param, search general params
            if (pCLP->mGeneralCommandLineParser.GetDescriptor(position, &pDesc))
                return pDesc;
        }

        return nullptr;
    }

    CLP::ParamDesc* CommandLineEditor::GetParamDesc(const std::string& sMode, std::string& paramName)
    {
        if (pCLP)
        {
            ParamDesc* pDesc = nullptr;
            // if CLP is configured with a specific mode go through parameters for that mode
            if (!sMode.empty() && pCLP->IsRegisteredMode(sMode))
            {
                CLP::CLModeParser& parser = pCLP->mModeToCommandLineParser[sMode];

                if (parser.GetDescriptor(paramName, &pDesc))
                    return pDesc;
            }

            // no mode specific param, search general params
            if (pCLP->mGeneralCommandLineParser.GetDescriptor(paramName, &pDesc))
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

    void RawEntryWin::AddUndoEntry()
    {
        undoEntry entry(mText, CursorToTextIndex(mLocalCursorPos), selectionstart, selectionend);
        mUndoEntryList.emplace_back(std::move(entry));
    }

    void RawEntryWin::Undo()
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
        for (auto& entry : enteredParams)
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


    bool ConsoleWin::Init(const Rect& r)
    {
        SetArea(r);
        mbVisible = true;
        mbDone = false;
        mbCanceled = false;
        return true;
    }

    void RawEntryWin::SetText(const std::string& text)
    {
        mText = text;
        UpdateCursorPos(TextIndexToCursor((int64_t)text.size()));
    }

    void ConsoleWin::SetArea(const Rect& r)
    {
        assert(r.r > r.l && r.b > r.t);
        mX = r.l;
        mY = r.t;

        int64_t newW = r.r - r.l;
        int64_t newH = r.b - r.t;
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

    void RawEntryWin::SetArea(const Rect& r)
    {
        ConsoleWin::SetArea(r);
        UpdateCursorPos(mLocalCursorPos);
    }

    void ConsoleWin::GetArea(Rect& r)
    {
        r.l = mX;
        r.t = mY;
        r.r = r.l + mWidth;
        r.b = r.t + mHeight;
    }

    void ConsoleWin::GetInnerArea(Rect& r)
    {
        r.l = 0;
        r.t = 0;
        r.r = mWidth;
        r.b = mHeight;

        if (enableFrame[Side::L])
            r.l++;
        if (enableFrame[Side::T])
            r.t++;
        if (enableFrame[Side::R])
            r.r--;
        if (enableFrame[Side::B])
            r.b--;
    }


    void ListboxWin::Paint(tConsoleBuffer& backBuf)
    {
        if (!mbVisible)
            return;

    //    Clear(mClearAttrib);
        ConsoleWin::BasePaint();


        Rect drawArea;
        GetInnerArea(drawArea);
    
        int64_t visibleRows = mHeight - 2;

        // scroll if selection is not on screen
        if (mSelection < mTopVisibleRow+1)
            mTopVisibleRow = mSelection-1;
        else if (mSelection > mTopVisibleRow+visibleRows)
            mTopVisibleRow = (mSelection-visibleRows);

        int64_t drawrow = -mTopVisibleRow;
        int64_t selection = 0;

    


        for (auto& entry : mEntries)
        {
            if (drawrow > 0 && drawrow < mHeight-1)
            {
                string s = entry;
                DrawClippedText(drawArea.l, drawrow, s, kAttribListBoxEntry, false, &drawArea);
                if (selection == mSelection)
                    Fill(Rect(drawArea.l, drawrow, drawArea.r, drawrow + 1), kAttribListBoxSelectedEntry);
    //                DrawClippedText(1, drawrow, s, kAttribListBoxSelectedEntry);
            }

            drawrow++;
            selection++;
        }

    //    ConsoleWin::Paint(backBuf);
        ConsoleWin::RenderToBackBuf(backBuf);

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
            rawCommandBuf.AddUndoEntry();
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
        UpdateCaptions();
    }


    HistoryWin::HistoryWin()
    {
        mMinWidth = 64;
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
        mWidth = 0;
        mHeight = 0;
        
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
    //            string sSearchSub(selectionSearch.substr(0, cmpLength));
                string sSearchSub(selectionSearch);
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

        UpdateCaptions();
        SizeWindowToEntries();
    }

    void ListboxWin::UpdateCaptions()
    {
        positionCaption[ConsoleWin::Position::RT] = "(" + SH::FromInt(mSelection+1) + "/" + SH::FromInt(mEntries.size()) + ")";
    }


    void ListboxWin::SizeWindowToEntries()
    {

        // find widest entry
        int64_t width = MH::Max(mMinWidth, mWidth); 
        int64_t height = mEntries.size()+2;

        int64_t topCaptionsWidth = positionCaption[ConsoleWin::Position::LT].length() + positionCaption[ConsoleWin::Position::CT].length() + positionCaption[ConsoleWin::Position::RT].length();
        int64_t bottomCaptionsWidth = positionCaption[ConsoleWin::Position::LB].length() + positionCaption[ConsoleWin::Position::CB].length() + positionCaption[ConsoleWin::Position::RB].length();

        width = MH::Max(topCaptionsWidth, width);
        width = MH::Max(bottomCaptionsWidth, width);

        for (auto& entry : mEntries)
        {
            if (width < (int64_t)entry.length()+2)
                width = entry.length()+2;
        }

        Rect r(mAnchorL, mAnchorB - height, mAnchorL + width, mAnchorB);

        if (height > mAnchorB)
        {
            r.t = 1;
            r.b = mAnchorB - 1;
        }

        if (width > ScreenW())
        {
            r.l = 1;
            r.r = ScreenW() - 1;
        }


        // move window to fit on screen
        if (r.r > ScreenW())
        {
            int64_t shiftleft = r.r - ScreenW();
            r.l -= shiftleft;
            r.r -= shiftleft;
        }
        if (r.l < 0)
        {
            int64_t shiftright = -r.l;
            r.l += shiftright;
            r.r += shiftright;
        }
        if (r.b > ScreenH())
        {
            int64_t shiftdown = r.b - ScreenH();
            r.t += shiftdown;
            r.b += shiftdown;
        }
        if (r.t < 0)
        {
            int64_t shiftup = -r.t;
            r.t -= shiftup;
            r.b -= shiftup;
        }

        SetArea(r);
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
                if (SH::EndsWith(mPath, "\\"))
                    navigate = mPath.substr(0, mPath.length() - 1); // strip last directory indicator so that parent_path works
                if (navigate.has_parent_path())
                    Scan(navigate.parent_path().string(), mX, mY + mHeight);
                return;
            }
            case VK_TAB:
            {
                string selection(mPath + CommandLineParser::StripEnclosure(GetSelection()));
                if (fs::is_directory(selection))
                    Scan(selection, mX, mY + mHeight);
                return;
            }
            case VK_RETURN:
            {
                string selection(CommandLineParser::StripEnclosure(GetSelection()));
                if (selection == kEmptyFolderCaption)
                    selection = mPath;

                rawCommandBuf.AddUndoEntry();
                rawCommandBuf.HandlePaste(CommandLineParser::EncloseWhitespaces(mPath + selection));
                mEntries.clear();
                mbVisible = false;
                return;
            }
        }

        ListboxWin::OnKey(keycode, c);
        UpdateCaptions();
    }

    void FolderList::UpdateCaptions()
    {
        if (SH::EndsWith(GetSelection(), "\\"))
            positionCaption[Position::LB] = "[TAB-To Subfolder] [BACKSPACE-Parent]";
        else
            positionCaption[Position::LB] = "[BACKSPACE-Parent]";
        positionCaption[Position::RB] = " [ENTER-Select]";

        ListboxWin::UpdateCaptions();
        SizeWindowToEntries();
    }





    void ConsoleWin::Clear(ZAttrib attrib)
    {
        mClearAttrib = attrib;
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


    void ConsoleWin::Fill(const Rect& r, ZAttrib attrib)
    {
        for (int64_t y = r.t; y < r.b; y++)
        {
            for (int64_t x = r.l; x < r.r; x++)
            {
                size_t offset = y * mWidth + x;
                mBuffer[offset].attrib = attrib;
            }
        }
    }

    void ConsoleWin::Fill(ZAttrib attrib)
    {
        return Fill(Rect(0, 0, mWidth, mHeight), attrib);
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
        for (int i = 0; i < strings.size()-1; i++)
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

/*    void RawEntryWin::DrawClippedText(int64_t x, int64_t y, std::string text, ZAttrib attributes, bool bWrap, bool bHeightlightSelection)
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
                    DrawCharClipped(c, cursor.X, cursor.Y, kAttribRawSelectedText);
                else
                    DrawCharClipped(c, cursor.X, cursor.Y, kAttribRawText);
            }

            cursor.X++;
            if (cursor.X >= mWidth && !bWrap)
                break;
        }
    }*/


    bool getANSIColorAttribute(const std::string& str, size_t offset, ZAttrib& attribute, size_t& length)
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
                    switch (colorCode)
                    {
                    case 30: attribute |= BLACK;  break; // Black
                    case 31: attribute |= RED; break;
                    case 32: attribute |= GREEN; break;
                    case 33: attribute |= YELLOW; break; // Yellow
                    case 34: attribute |= CYAN; break; // Cyan
                    case 35: attribute |= MAGENTA; break; // Magenta
                    case 36: attribute |= WHITE; break; // White
                    case 37: break; // Default color
                    case 40: attribute |= MAKE_BG(BLACK);  break; // Background Black
                    case 41: attribute |= MAKE_BG(RED); break;
                    case 42: attribute |= MAKE_BG(GREEN); break;
                    case 43: attribute |= MAKE_BG(YELLOW); break; // Yellow
                    case 44: attribute |= MAKE_BG(CYAN); break; // Cyan
                    case 45: attribute |= MAKE_BG(MAGENTA); break; // Magenta
                    case 46: attribute |= MAKE_BG(WHITE); break; // White
                    case 47: break; // Default color
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
        ZAttrib attrib;
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


    void ConsoleWin::DrawClippedAnsiText(int64_t x, int64_t y, std::string ansitext, bool bWrap, Rect* pClip)
    {
        int64_t cursorX = x;
        int64_t cursorY = y;

        ZAttrib attrib(WHITE);

        CLP::Rect drawArea;
        GetInnerArea(drawArea);

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
            y = mHeight-1;
    }

    void ConsoleWin::BasePaint()
    {
        // Update display
        if (!mbVisible)
            return;

        Clear(mClearAttrib);

        // Fill 
        Fill(Rect(0, 0, mWidth, mHeight), mClearAttrib);

        if (enableFrame[Side::T])
            Fill(Rect(0, 0, mWidth, 1), kAttribCaption);   // top

        if (enableFrame[Side::L])
            Fill(Rect(0, 0, 1, mHeight), kAttribCaption);   // left

        if (enableFrame[Side::B])
            Fill(Rect(0, mHeight - 1, mWidth, mHeight), kAttribCaption);    // bottom

        if (enableFrame[Side::R])
            Fill(Rect(mWidth - 1, 0, mWidth, mHeight), kAttribCaption);    // right  

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

    void RawEntryWin::Paint(tConsoleBuffer& backBuf)
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
            // find error params
            size_t startIndex = string::npos;
            size_t endIndex = string::npos;
            string sParamUnderCursor;
            if (GetParameterUnderIndex(textindex, startIndex, endIndex, sParamUnderCursor))
            {
                for (auto& param : enteredParams)
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
                attribs[textindex] = kSelectedText;
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


        
        ConsoleWin::RenderToBackBuf(backBuf);
    }

    void AnsiColorWin::Paint(tConsoleBuffer& backBuf)
    {
        // Update display
        if (!mbVisible)
            return;

        ConsoleWin::BasePaint();

        Rect r;
        GetInnerArea(r);

        DrawClippedAnsiText(r.l, r.t, mText, true, &r);

        ConsoleWin::RenderToBackBuf(backBuf);
    }


    void RawEntryWin::OnKey(int keycode, char c)
    {
        bool bCTRLHeld = GetKeyState(VK_CONTROL) & 0x800;
        bool bSHIFTHeld = GetKeyState(VK_SHIFT) & 0x800;

        bool bHandled = false;

        do
        {

            switch (keycode)
            {
            case VK_TAB:
                HandleParamContext();
                bHandled = true;
                break;
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
                if (mLocalCursorPos.Y+firstVisibleRow > 0)
                {
                    mLocalCursorPos.Y--;
                    UpdateFirstVisibleRow();
                    UpdateCursorPos(mLocalCursorPos);
                }
                else if (mLocalCursorPos.Y + firstVisibleRow == 0 && !commandHistory.empty())
                {
                    // select everything
                    selectionstart = 0;
                    selectionend = mText.size();

                    // show history window
                    historyWin.mbVisible = true;
                    historyWin.positionCaption[Position::CT] = "History";
                    historyWin.positionCaption[Position::RB] = "[ESC-Cancel] [ENTER-Select] [DEL-Delete Entry]";
                    historyWin.mMinWidth = ScreenW();
                    historyWin.SetEntries(commandHistory, mText, selectionstart, mY);
                }
                else
                {
                    UpdateSelection();
                    UpdateCursorPos(COORD(mLocalCursorPos.X, mLocalCursorPos.Y-1));
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
                    rawCommandBuf.Undo();
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

    void ParamListWin::Paint(tConsoleBuffer& backBuf)
    {
        if (!pCLP)
            return;

        ConsoleWin::BasePaint();

        Rect drawArea;
        GetInnerArea(drawArea);

        string sRaw(rawCommandBuf.GetText());

        size_t rows = std::max<size_t>(1, (sRaw.size() + ScreenW() - 1) / ScreenW());
        //        rawCommandBufTopRow = screenBufferInfo.dwSize.Y - rows;


                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

                // if the command line is bigger than the screen, show the last n rows that fit


        if (!enteredParams.empty())   // if there are params entered
        {
            // compute column widths

            const int kColName = 0;
            const int kColEntry = 1;
            const int kColUsage = 2;

            vector<size_t> colWidths;
            colWidths.resize(3);
            colWidths[kColName] = 16;
            colWidths[kColEntry] = 12;
            colWidths[kColUsage] = ScreenW() - (colWidths[kColName] + colWidths[kColEntry]);
            for (int paramindex = 0; paramindex < enteredParams.size(); paramindex++)
            {
                string sText(enteredParams[paramindex].sParamText);
                colWidths[kColEntry] = std::max<size_t>(sText.length(), colWidths[kColEntry]);

                ParamDesc* pPD = enteredParams[paramindex].pRelatedDesc;
                if (pPD)
                {
                    colWidths[kColName] = std::max<size_t>(pPD->msName.length(), colWidths[kColName]);
                    colWidths[kColUsage] = std::max<size_t>(pPD->msUsage.length(), colWidths[kColUsage]);
                }
            }

            for (auto& i : colWidths)   // pad all cols
                i += 2;

            tStringArray strings(3);
            tAttribArray attribs(3);

            size_t row = drawArea.t;
           

            if (availableModes.empty())
            {
            }
            else
            {
                // first param is command
                string sMode = CLP::GetMode();

                strings[kColName] = "COMMAND";
                attribs[kColName] = kRawText;

                strings[kColEntry] = sMode;
                attribs[kColUsage] = kRawText;

                bool bModePermitted = availableModes.empty() || std::find(availableModes.begin(), availableModes.end(), sMode) != availableModes.end(); // if no modes registered or (if there are) if the first param matches one
                if (bModePermitted)
                {
                    attribs[kColEntry] = kGoodParam;

                    if (pCLP)
                    {
                        strings[kColUsage] = pCLP->GetModeDescription(sMode);
                        //                    mpCLP->GetCommandLineExample(msMode, strings[kColUsage]);
                    }
                }
                else
                {
                    attribs[kColName] = kUnknownParam;
                    strings[kColName] = "UNKNOWN COMMAND ";
                }

                row += DrawFixedColumnStrings(drawArea.l, row, strings, colWidths, attribs, &drawArea);
            }



            // next list positional params
            row++;
            string sSection = "-positional params-" + string(ScreenW(), '-');
            DrawClippedText(drawArea.l, row++, sSection, kAttribSection, false, &drawArea);


            tEnteredParams posParams = GetPositionalEntries();

            for (auto& param : posParams)
            {
                attribs[kColName] = kRawText;
                attribs[kColEntry] = kUnknownParam;
                attribs[kColUsage] = kRawText;

                //                strings[kColName] = "[" + SH::FromInt(param.positionalindex) + "]";

                string sFailMessage;
                if (param.pRelatedDesc)
                {
                    strings[kColName] = param.pRelatedDesc->msName;

                    if (param.pRelatedDesc->DoesValueSatisfy(param.sParamText, sFailMessage))
                    {
                        strings[kColUsage] = param.pRelatedDesc->msUsage;
                        attribs[kColEntry] = kGoodParam;
                    }
                    else
                    {
                        strings[kColUsage] = sFailMessage;
                        attribs[kColEntry] = kBadParam;
                        attribs[kColUsage] = kBadParam;
                    }
                }
                else
                {
                    strings[kColName] = "";
                    attribs[kColEntry] = kUnknownParam;
                    strings[kColUsage] = "Unexpected parameter";
                    attribs[kColUsage] = kUnknownParam;
                }

                strings[kColEntry] = param.sParamText;

                row += DrawFixedColumnStrings(drawArea.l, row, strings, colWidths, attribs, &drawArea);
            }


            tEnteredParams namedParams = GetNamedEntries();

            if (!namedParams.empty())
            {
                row++;
                sSection = "-named params-" + string(ScreenW(), '-');
                DrawClippedText(drawArea.l, row++, sSection, kAttribSection, false, &drawArea);

                for (auto& param : namedParams)
                {
                    attribs[kColName] = kRawText;
                    attribs[kColEntry] = kUnknownParam;
                    attribs[kColUsage] = kRawText;

                    strings[kColName] = "-";
                    string sFailMessage;
                    if (param.pRelatedDesc)
                    {
                        string sName;
                        string sValue;
                        pCLE->ParseParam(param.sParamText, sName, sValue);

                        if (param.pRelatedDesc->DoesValueSatisfy(sValue, sFailMessage))
                        {
                            strings[kColName] += param.pRelatedDesc->msName;
                            attribs[kColEntry] = kGoodParam;
                            strings[kColUsage] = param.pRelatedDesc->msUsage;
                        }
                        else
                        {
                            strings[kColName] += param.pRelatedDesc->msName;
                            attribs[kColEntry] = kBadParam;
                            strings[kColUsage] = sFailMessage;
                            attribs[kColUsage] = kBadParam;
                        }
                    }
                    else
                    {
                        attribs[kColEntry] = kUnknownParam;
                        strings[kColUsage] = "Unknown parameter";
                        attribs[kColUsage] = kUnknownParam;
                    }


                    strings[kColEntry] = param.sParamText;

                    row += DrawFixedColumnStrings(drawArea.l, row, strings, colWidths, attribs, &drawArea);
                }
            }
        }
        else
        {
            // no commands entered
            TableOutput commandsTable = pCLP->GetCommandsTable();
            commandsTable.AlignWidth(ScreenW());
            string sCommands = commandsTable;
            DrawClippedAnsiText(drawArea.l, drawArea.t, sCommands, true, &drawArea);
        }

        ConsoleWin::RenderToBackBuf(backBuf);

//        AnsiColorWin::Paint(backBuf);

        return;
    }

    void CommandLineEditor::UpdateDisplay()
    {
        DrawToScreen();

/*        static int count = 1;
        char buf[64];
        sprintf(buf, "draw:%d\n", count++);
        OutputDebugString(buf);*/
   
    }



    FolderList::FolderList()
    {
        mMinWidth = 64;
    }


    bool FolderList::Scan(std::string sSelectedPath, int64_t anchor_l, int64_t anchor_b)
    {
        string sPath = FindClosestParentPath(sSelectedPath);
        if (!fs::exists(sPath))
            return false;

        sPath = FH::Canonicalize(sPath);
        if (!fs::is_directory(sPath))
            sPath = fs::path(sPath).parent_path().string();

        tStringList folders;
        tStringList files;

        for (const auto& entry : fs::directory_iterator(sPath))
        {
            try
            {
                string sEntry(FH::Canonicalize(entry.path().string()));
                if (FH::HasPermission(sEntry))
                {
                    filesystem::path relPath(filesystem::relative(entry, sPath));
                    if (entry.is_directory())
                        folders.push_back(relPath.string() + "\\");
                    else
                        files.push_back(relPath.string());
                }
            }
            catch (const std::filesystem::filesystem_error& /*e*/)
            {
//                std::cerr << "Filesystem error: " << e.what() << std::endl;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Error: " << e.what() << std::endl;
                return false;
            }
        }

        if (folders.empty() && files.empty())
        {
            mEntries.clear();
            mEntries.push_back(kEmptyFolderCaption);
        }
        else
        {
            mEntries = folders;
            mEntries.splice(mEntries.end(), files);
        }

        SetEntries(mEntries, sSelectedPath, anchor_l, anchor_b);

        mPath = sPath;

        string sSelection = GetSelection();
        positionCaption[Position::LT] = sPath;
        UpdateCaptions();
        SizeWindowToEntries();

        return true;
    }

    string FolderList::FindClosestParentPath(string sPath)
    {
        while (sPath.length() > 2) // for example C:
        {
            fs::path path(sPath);
            if (fs::exists(path) && fs::is_directory(path))
                return sPath;

            sPath = path.parent_path().string();
        }

        return sPath;
    }

    bool RawEntryWin::HandleParamContext()
    {
        size_t start = string::npos;
        size_t end = string::npos;
        string sText;

        int64_t cursorIndex = CursorToTextIndex(mLocalCursorPos);
        GetParameterUnderIndex(cursorIndex, start, end, sText);

        selectionstart = start;
        selectionend = end;

        // Find set of auto complete for param

        COORD anchor = TextIndexToCursor(selectionstart);
        anchor.Y += (SHORT)mY;



        // If mode..... popup modes list
        if (enteredParams.empty())
        {
            popupListWin.mbVisible = true;
            popupListWin.positionCaption[Position::CT] = "Commands";
            popupListWin.SetEntries(availableModes, sText, anchor.X, anchor.Y);
        }
        else if (sText == enteredParams[0].sParamText && !availableModes.empty())
        {
            popupListWin.mbVisible = true;
            popupListWin.positionCaption[Position::CT] = "Commands";
            popupListWin.SetEntries(availableModes, sText, anchor.X, anchor.Y);
        }
        else if (sText[0] == '-')
        {
            popupListWin.positionCaption[Position::CT] = "Options";
            popupListWin.mbVisible = true;
            popupListWin.SetEntries(availableNamedParams, sText, anchor.X, anchor.Y);
        }
        else
        {
            // find param desc
            for (int i = 0; i < enteredParams.size(); i++)
            {
                if (sText == enteredParams[i].sParamText)
                {
                    if (enteredParams[i].pRelatedDesc && (enteredParams[i].pRelatedDesc->IsAPath()|| enteredParams[i].pRelatedDesc->MustHaveAnExistingPath()))
                    {
                        if (popupFolderListWin.Scan(CommandLineParser::StripEnclosure(sText), anchor.X, anchor.Y+1))
                        {
                            popupFolderListWin.mbVisible = true;
                        }
                    }
                }
            }
        }

        return true;
    }

    void DrawAnsiChar(int64_t x, int64_t y, char c, ZAttrib ca)
    {
        static ZAttrib lastC;
        static int64_t lastX = -1;
        static int64_t lastY = -1;

        string s;
        if (x != lastX || y != lastY)
        {
            s += "\033[" + SH::FromInt(x+1) + "G\033[" + SH::FromInt(y) + "d";
            lastX = x;
            lastY = y;
        }

        if (lastC.a != ca.a || lastC.r != ca.r || lastC.g != ca.g || lastC.b != ca.b || lastC.ba != ca.ba || lastC.br != ca.br || lastC.bg != ca.bg || lastC.bb != ca.bb)
        {
            s += "\033[38;2;" + SH::FromInt(ca.r) + ";" + SH::FromInt(ca.g) + ";" + SH::FromInt(ca.b) + "m"; //forground color
            s += "\033[48;2;" + SH::FromInt(ca.br) + ";" + SH::FromInt(ca.bg) + ";" + SH::FromInt(ca.bb) + "m"; //background color

            lastC = ca;
        }

        if (c < 32)
            s += ' ';
        else
            s += c;
        cout << s;
        lastX++;    // cursor advances after drawing
    };


    void CommandLineEditor::DrawToScreen()
    {
        SHORT paramListRows = (SHORT)enteredParams.size();


        // clear back buffer
        memset(&backBuffer[0], 0, backBuffer.size() * sizeof(ZChar));
        bool bCursorShouldBeHidden = false;

        if (helpWin.mbVisible || popupListWin.mbVisible || popupFolderListWin.mbVisible || historyWin.mbVisible)
            bCursorShouldBeHidden = true;






        if (helpWin.mbVisible)
        {
            //helpBuf.PaintToWindowsConsole(mhOutput);
            helpWin.Paint(backBuffer);
        }
        else
        {
            if (pCLP)
            {
//                if (mpCLP->IsRegisteredMode(msMode))
                {
                    string sExample;
                    pCLP->GetCommandLineExample(CLP::GetMode(), sExample);
                    usageBuf.SetText(string(COL_BLACK) + "usage: " + sExample);
                }
            }

            string sTop = "[F1-Help] [ESC-Cancel] [TAB-Contextual] [CTRL+Z-Undo]";
            if (!commandHistory.empty())
                sTop += " [UP-History (" + SH::FromInt(commandHistory.size()) + ")]";
            topInfoBuf.SetText(string(COL_BLACK) + sTop);

            paramListBuf.Paint(backBuffer);
            rawCommandBuf.Paint(backBuffer);
            usageBuf.Paint(backBuffer);
            topInfoBuf.Paint(backBuffer);
            popupListWin.Paint(backBuffer);
            historyWin.Paint(backBuffer);
            popupFolderListWin.Paint(backBuffer);
        }

        bool bCursorHidden = false;
        for (int64_t y = 0; y < ScreenH(); y++)
        {
            for (int64_t x = 0; x < ScreenW(); x++)
            {
                int64_t i = (y * ScreenW()) + x;
                if (bScreenChanged || backBuffer[i] != drawStateBuffer[i])
                {
                    if (!bCursorHidden)
                    {
                        bCursorHidden = true;
                        cout << "\033[?25l";
                    }

                    bScreenChanged = true;
                    DrawAnsiChar(x, y, backBuffer[i].c, backBuffer[i].attrib);
                }
            }
        }

        if (bScreenChanged)
        {
            bScreenChanged = false;
            drawStateBuffer = backBuffer;
            rawCommandBuf.UpdateCursorPos(rawCommandBuf.mLocalCursorPos);
        }

//        DrawAnsiChar(0, 0, '*', 0xffff00ffff00ff00);

        if (bCursorHidden && !bCursorShouldBeHidden)
        {
            cout << "\033[?25h";
        }
    }

/*    void CommandLineEditor::SaveConsoleState()
    {
        originalScreenInfo = screenInfo;
        originalConsoleBuf.resize(originalScreenInfo.dwSize.X * originalScreenInfo.dwSize.Y);
        SMALL_RECT readRegion = { 0, 0, originalScreenInfo.dwSize.X - 1, originalScreenInfo.dwSize.Y - 1 };
        ReadConsoleOutput(mhOutput, &originalConsoleBuf[0], screenInfo.dwSize, { 0, 0 }, &readRegion);
    }

    void CommandLineEditor::RestoreConsoleState()
    {
        SMALL_RECT writeRegion = { 0, 0, originalScreenInfo.dwSize.X - 1, originalScreenInfo.dwSize.Y - 1 };
        WriteConsoleOutput(mhOutput, &originalConsoleBuf[0], originalScreenInfo.dwSize, { 0, 0 }, &writeRegion);
        SetConsoleCursorPosition(mhOutput, originalScreenInfo.dwCursorPosition);
    }*/


    int64_t RawEntryWin::CursorToTextIndex(COORD coord)
    {
        int64_t i = (coord.Y+firstVisibleRow) * mWidth + coord.X;
        return std::min<size_t>(i, mText.size());
    }

    COORD RawEntryWin::TextIndexToCursor(int64_t i)
    {
        if (i > (int64_t)mText.size())
            i = (int64_t)mText.size();

        if (mWidth > 0)
        {
            COORD c;
            c.X = (SHORT)(i) % mWidth;
            c.Y = (SHORT)((i/mWidth)-firstVisibleRow);
            return c;
        }

        return COORD(0, 0);
    }

    void RawEntryWin::HandlePaste(string text)
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

        int curindex = (int)CursorToTextIndex(mLocalCursorPos);
        if (curindex > normalizedStart)
            curindex -= (int)(curindex- normalizedStart);
        UpdateCursorPos(TextIndexToCursor(curindex));

        ClearSelection();
    }

    void RawEntryWin::ClearSelection()
    {
        selectionstart = -1;
        selectionend = -1;
        UpdateCursorPos(mLocalCursorPos);
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
                selectionstart = CursorToTextIndex(mLocalCursorPos);
            }
            selectionend = CursorToTextIndex(mLocalCursorPos);
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
        for (int i = 0; i < enteredParams.size(); i++)
        {
            sText += enteredParams[i].sParamText + " ";
        }

        if (!sText.empty())
            sText = sText.substr(0, sText.length() - 1);    // strip last space

        return sText;
    }

    tEnteredParams CommandLineEditor::ParamsFromText(const std::string& sText)
    {
        tEnteredParams params;
        string sModeWhileParsing;

        const int64_t kModePosition = -1;
        int positionalindex = 0;
        if (!availableModes.empty())         
            positionalindex = kModePosition;

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
            string sFailMessage;

            if (positionalindex == kModePosition) 
            {
                sModeWhileParsing = param.sParamText;
                bool bModePermitted = availableModes.empty() || std::find(availableModes.begin(), availableModes.end(), sModeWhileParsing) != availableModes.end(); // if no modes registered or (if there are) if the first param matches one
                if (bModePermitted)
                {
                    param.drawAttributes = kGoodParam;
                }
                else
                {
                    param.drawAttributes = kUnknownParam;
                }
                positionalindex++;
            }
            else if (ParseParam(param.sParamText, sParamName, sParamValue)) // is it a named parameter
            {
                param.pRelatedDesc = GetParamDesc(sModeWhileParsing, sParamName);
                if (!param.pRelatedDesc)
                    param.drawAttributes = kUnknownParam;      // unknown named parameter
                else if (!param.pRelatedDesc->DoesValueSatisfy(sParamValue, sFailMessage))
                    param.drawAttributes = kBadParam;      // known named parameter but not in required range
            }
            else
            {
                param.positionalindex = positionalindex;
                param.pRelatedDesc = GetParamDesc(sModeWhileParsing, positionalindex);

                if (!param.pRelatedDesc)       // unsatisfied positional parameter
                    param.drawAttributes = kBadParam;
                else if (!param.pRelatedDesc->DoesValueSatisfy(param.sParamText, sFailMessage))     // positional param has descriptor but not in required range
                    param.drawAttributes = kBadParam;

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

        enteredParams = ParamsFromText(mLastParsedText);
//        rawCommandBuf.mEnteredParams = mParams;
        availableNamedParams = GetCLPNamedParamsForMode(CLP::GetMode());
    }

    std::string CommandLineEditor::Edit(int argc, char* argv[])
    {
//        appEXE = argv[0];
        tStringArray params(CommandLineParser::ToArray(argc-1, argv));    // first convert to param array then to a string which will enclose parameters with whitespaces
        return Edit(CommandLineParser::ToString(params));
    }

    void CommandLineEditor::UpdateFromConsoleSize(bool bForce)
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

            topInfoBuf.Clear(kAttribTopInfoBG);
            topInfoBuf.SetArea(Rect(0, 1, w, 2));

            paramListBuf.Clear(kAttribParamListBG);
            paramListBuf.SetArea(Rect(0, 2, w, h - 5));

            usageBuf.Clear(kAttribTopInfoBG);
            usageBuf.SetArea(Rect(0, h - 5, w, h - 4));

            rawCommandBuf.Clear(0xff444444);
            rawCommandBuf.SetArea(Rect(0, h - 4, w, h));

            helpWin.Clear(kAttribHelpBG);
            helpWin.SetArea(Rect(0, 1, w, h));
            helpWin.SetEnableFrame();

            popupFolderListWin.Clear(kAttribFolderListBG);
            popupFolderListWin.SetEnableFrame();

            historyWin.Clear(0xFF888800FF000000);
            historyWin.SetEnableFrame();

            popupListWin.Clear(kAttribListBoxBG);
            popupListWin.SetEnableFrame();
            popupListWin.mMinWidth = 32;

            UpdateDisplay();
        }
    }

    void CommandLineEditor::ShowHelp()
    {
        string sText;
        helpWin.Init(Rect(0, 1, ScreenW(), ScreenH()));
        helpWin.Clear(kAttribHelpBG);

        Rect drawArea;
        helpWin.GetInnerArea(drawArea);
        int64_t drawWidth = drawArea.r - drawArea.l-1;


        if (pCLP)
        {
            string sMode = CLP::GetMode();

            if (pCLP->IsRegisteredMode(sMode))
            {
                sText = pCLP->GetModeHelpString(sMode, false);
                helpWin.positionCaption[ConsoleWin::Position::LT] = "Help for \"" + sMode + "\"";
            }
            else
            {
                sText = pCLP->GetGeneralHelpString();
                helpWin.positionCaption[ConsoleWin::Position::LT] = "General Help";
            }
        }

        TableOutput additionalHelp;
        additionalHelp.SetBorders('+', '+', '+', '+');
        additionalHelp.SetSeparator(' ', 1);
        additionalHelp.SetMinimumOutputWidth(drawWidth);

        additionalHelp.AddRow(cols[kSECTION] + "--Key Combo--", "--Action--");

        additionalHelp.AddRow(cols[kPARAM] + "[F1]", "General help or contextual help (if first parameter is recognized command.)" + cols[kRESET]);

        additionalHelp.AddRow(cols[kPARAM] + "[TAB]", "Context specific popup" + cols[kRESET]);

        additionalHelp.AddRow(cols[kPARAM] + "[SHIFT-LEFT/RIGHT]", "Select characters" + cols[kRESET]);
        additionalHelp.AddRow(cols[kPARAM] + "[SHIFT+CTRL-LEFT/RIGHT]", "Select words" + cols[kRESET]);

        additionalHelp.AddRow(cols[kPARAM] + "[CTRL-A]", "Select All" + cols[kRESET]);
        additionalHelp.AddRow(cols[kPARAM] + "[CTRL-C/V]", "Copy/Paste" + cols[kRESET]);
        additionalHelp.AddRow(cols[kPARAM] + "[CTRL-Z]", "Undo" + cols[kRESET]);

        additionalHelp.AddRow(cols[kPARAM] + "[UP]", "Command Line History (When cursor at top)" + cols[kRESET]);


        additionalHelp.AlignWidth(drawWidth);


        sText += "\n\n------Help for Interactive Command Line Editor---------\n\n";
        sText += "Rich editing of command line passed in with contextual auto-complete popups.\n";

        sText += "Command Line example usage is visible just above the command line entry.\n";
        sText += "Parameters (positional and named) along with desciptions are enumerated above.\n";

        sText += "Path parameters bring up folder listing at that path. (TAB to go into the folder, BACKSPACE to go up a folder.)\n";
        sText += (string)additionalHelp;

        helpWin.mText = sText;
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

        if (!RegisterHotKey(NULL, MY_HOTKEY_ID, MOD_SHIFT, VK_INSERT))
        {
            std::cerr << "Error registering hotkey" << std::endl;
            return "";
        }

        if (!GetConsoleScreenBufferInfo(mhOutput, &screenInfo))
        {
            cerr << "Failed to get console info." << endl;
            return sCommandLine;
        }


        SHORT w = ScreenW();
        SHORT h = ScreenH();


        //SaveConsoleState();
        std::vector<CHAR_INFO> blank(w * h);
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
        mode |= ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        mode &= ~ENABLE_PROCESSED_INPUT;

        //mode |= ENABLE_PROCESSED_INPUT;
        if (!SetConsoleMode(mhInput, mode))
        {
            cerr << "Failed to set console mode." << endl;
            return sCommandLine;
        }

        // Main loop to read input events
        INPUT_RECORD inputRecord[128];
        DWORD numEventsRead;








        popupListWin.Init(Rect(w / 4, h / 4, w * 3 / 4, h * 3 / 4));
        popupListWin.mbVisible = false;

        historyWin.Init(Rect(w / 4, h / 4, w * 3 / 4, h * 3 / 4));
        historyWin.mbVisible = false;


        popupFolderListWin.Init(Rect(w / 4, h / 4, w * 3 / 4, h * 3 / 4));
        popupFolderListWin.mbVisible = false;


        LoadHistory();

        backBuffer.resize(w*h);
        drawStateBuffer.resize(w * h);
        rawCommandBuf.Init(Rect(0, h - 4, w, h));


        if (!sCommandLine.empty())
            rawCommandBuf.SetText(sCommandLine);
        else if (sCommandLine.empty() && !commandHistory.empty())    // if there is history and no command line was passed in, 
            rawCommandBuf.SetText(*(commandHistory.rbegin()));




        paramListBuf.Init(Rect(0, 1, w, h - 6));
        usageBuf.Init(Rect(0, h - 6, w, h - 5));
        topInfoBuf.Init(Rect(0, 0, w, 1));


        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Clear the raw command buffer and param buffers
        UpdateFromConsoleSize(true);





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
                    rawCommandBuf.AddUndoEntry();
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
                            COORD coord(mer.dwMousePosition);
                            coord.Y -= (SHORT)rawCommandBuf.mY;       // to raw buffer coordinates
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
                        else if (helpWin.mbVisible)
                        {
                            helpWin.OnKey(keycode, c);
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


        //RestoreConsoleState();
        if (rawCommandBuf.mbCanceled)
        {
            cout << "Canceled editing.\n";
            if (!rawCommandBuf.GetText().empty())
            {
                cout << string(ScreenW(), '*');
                cout << "Last Edit: \"" << COL_YELLOW << CLP::appName << " " << rawCommandBuf.GetText() << COL_RESET << "\"\n";
                cout << string(ScreenW(), '*');
                cout << "\n\n";
            }
            return "";
        }
        else
        {
            if (!rawCommandBuf.GetText().empty())
            {
                cout << "Command Entered:\n\"" << COL_YELLOW << CLP::appName << " " << rawCommandBuf.GetText() << COL_RESET << "\"\n";
                cout << "\n\n";
            }
        }

        AddToHistory(rawCommandBuf.GetText());
        SaveHistory();

        return rawCommandBuf.GetText();
    }

    void InfoWin::Paint(tConsoleBuffer& backBuf)
    {
        if (!mbVisible)
            return;

//        Clear(mClearAttrib);
        ConsoleWin::BasePaint();

        Rect drawArea;
        GetInnerArea(drawArea);

        int64_t firstDrawRow = mTopVisibleRow-1;
        DrawClippedAnsiText(drawArea.l, -firstDrawRow, mText, true, &drawArea);

        ConsoleWin::RenderToBackBuf(backBuf);

/*        for (int64_t y = 0; y < mHeight; y++)
        {
            int64_t readOffset = (y * mWidth);
            int64_t writeOffset = ((y + mY) * mWidth + mX);

            memcpy(&backBuf[writeOffset], &mBuffer[readOffset], mWidth * sizeof(ZChar));
        }
        */
    }

    void InfoWin::OnKey(int keycode, char c)
    {
        Rect drawArea;
        GetInnerArea(drawArea);
        int64_t drawHeight = drawArea.b - drawArea.t;

        if (keycode == VK_F1 || keycode == VK_ESCAPE)
        {
            mText.clear();
            mbVisible = false;
            mbDone = true;
        }
        else if (keycode == VK_UP)
        {
            if (mTopVisibleRow > 0)
                mTopVisibleRow--;
        }
        else if (keycode == VK_DOWN)
        {
            int64_t w = 0;
            int64_t h = 0;
            GetTextOuputRect(mText, w, h);

            if (mTopVisibleRow < (h - drawHeight + 1))
                mTopVisibleRow++;
        }
        else if (keycode == VK_HOME)
        {
            mTopVisibleRow = 0;
        }
        else if (keycode == VK_END)
        {
            int64_t w = 0;
            int64_t h = 0;
            GetTextOuputRect(mText, w, h);
            if (h > drawHeight)
                mTopVisibleRow = h - drawHeight;
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

#endif // ENABLE_CLE
