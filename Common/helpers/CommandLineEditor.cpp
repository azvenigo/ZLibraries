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

namespace CLP
{
    CommandLineParser* pCLP = nullptr;
    CommandLineEditor* pCLE = nullptr;

    RawEntryWin     rawCommandBuf;  // raw editing buffer 
    ParamListWin    paramListBuf;   // parsed parameter list with additional info
    AnsiColorWin    topInfoBuf;
    UsageWin        usageBuf;       // simple one line drawing of usage
    ListboxWin      popupListWin;
    HistoryWin      historyWin;
    FolderList      popupFolderListWin;

    tEnteredParams  enteredParams;
    tStringList     availableModes;
    tStringList     availableNamedParams;

    const string    kEmptyFolderCaption("[Empty Folder]");

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



    CommandLineEditor::CommandLineEditor()
    {
        pCLP = nullptr;
        pCLE = this;
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

                int64_t general_position = position - pCLP->mModeToCommandLineParser[sMode].GetNumPositionalParamsRegistered();   // adjust index for general position (since they're registered independently of mode params)
                if (pCLP->mGeneralCommandLineParser.GetDescriptor(general_position, &pDesc))
                    return pDesc;

                return nullptr;
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

    COORD RawEntryWin::TextIndexToCursor(int64_t i)
    {
        if (i > (int64_t)(mText.size()))
            i = (int64_t)(mText.size());

        i += CLP::appName.length() + 1; // adjust for visible appname plus space

        if (mWidth > 0)
        {
            COORD c;
            c.X = (SHORT)(i) % mWidth;
            c.Y = (SHORT)((i / mWidth) - firstVisibleRow);
            return c;
        }

        return COORD(0, 0);
    }

    int64_t RawEntryWin::CursorToTextIndex(COORD coord)
    {
        int64_t i = (coord.Y + firstVisibleRow) * mWidth + coord.X - CLP::appName.length() - 1; // adjust for visible app name plus space
        if (i < 0)
            return 0;
        if (i > (int64_t)mText.length())
            return mText.length();
        return i;
    }


    bool RawEntryWin::GetParameterUnderIndex(int64_t index, size_t& outStart, size_t& outEnd, string& outParam, ParamDesc** ppPD)
    {
        for (auto& entry : enteredParams)
        {
            if (index >= entry.rawCommandLineStartIndex && index <= entry.rawCommandLineStartIndex + (int64_t)entry.sParamText.length())
            {
                outParam = entry.sParamText;
                outStart = (size_t)entry.rawCommandLineStartIndex;
                outEnd = outStart + outParam.length();
                if (ppPD)
                    *ppPD = entry.pRelatedDesc;
                return true;
            }
        }

        return false;
    }

    bool RawEntryWin::OnKey(int keycode, char c)
    {
        bool bCTRLHeld = GetKeyState(VK_CONTROL) & 0x800;
        bool bSHIFTHeld = GetKeyState(VK_SHIFT) & 0x800;
        bool bHandled = false;

        gConsole.Invalidate();

        do
        {
            switch (keycode)
            {
            case VK_TAB:
                HandleParamContext();
                bHandled = true;
                break;
            case VK_UP:
            {
                if (mLocalCursorPos.Y + firstVisibleRow == 0 && !commandHistory.empty())
                {
                    // select everything
                    selectionstart = 0;
                    selectionend = mText.size();

                    // show history window
                    historyWin.SetVisible(true);
                    historyWin.positionCaption[Position::CT] = "History";
                    historyWin.positionCaption[Position::RB] = "[ESC-Cancel] [ENTER-Select] [DEL-Delete Entry]";
                    historyWin.mMinWidth = gConsole.Width();
                    historyWin.SetEntries(commandHistory, mText, selectionstart, mY);
                    bHandled = true;
                }
            }
            break;
            }
        } while (0); // for breaking

        // nothing handled above....regular text entry
        if (!bHandled)
        {
            return TextEditWin::OnKey(keycode, c);
        }

        UpdateFirstVisibleRow();
        return bHandled;
    }

    bool RawEntryWin::OnMouse(MOUSE_EVENT_RECORD event)
    {
        COORD localcoord = event.dwMousePosition;
        localcoord.X -= (SHORT)mX;
        localcoord.Y -= (SHORT)(mY+1);

        if (event.dwEventFlags == 0)
        {
            if (event.dwButtonState == RIGHTMOST_BUTTON_PRESSED)
            {
                mLocalCursorPos = localcoord;
                gConsole.Invalidate();
                return HandleParamContext();
            }
        }
        else if (event.dwEventFlags == DOUBLE_CLICK)
        {
            int64_t cursorIndex = CursorToTextIndex(localcoord);
            size_t start;
            size_t end;
            string sText;
            GetParameterUnderIndex(cursorIndex, start, end, sText);

            selectionstart = start;
            selectionend = end;
            gConsole.Invalidate();
            return true;
        }

        return TextEditWin::OnMouse(event);
    }


    void ListboxWin::Paint(tConsoleBuffer& backBuf)
    {
        if (!mbVisible)
            return;

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
                if (selection == mSelection)
                {
                    DrawClippedText(Rect(drawArea.l, drawrow, drawArea.r, drawrow + 1), ">" + s, kAttribListBoxEntry, false, &drawArea);
                    Fill(Rect(drawArea.l, drawrow, drawArea.r, drawrow + 1), kAttribListBoxSelectedEntry);
                }
                else
                {
                    DrawClippedText(Rect(drawArea.l, drawrow, drawArea.r, drawrow + 1), " " + s, kAttribListBoxEntry, false, &drawArea);
                }
            }

            drawrow++;
            selection++;
        }

        if ((int64_t)mEntries.size() > mHeight)
        {
            Rect sb(drawArea);
            sb.l = drawArea.r;
            sb.r = sb.l + 1;
            DrawScrollbar(sb, 0, mEntries.size() - sb.h() - 1, mTopVisibleRow, kAttribScrollbarBG, kAttribScrollbarThumb);
        }

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

    bool ListboxWin::OnMouse(MOUSE_EVENT_RECORD event)
    {
        COORD localcoord = event.dwMousePosition;
        localcoord.X -= (SHORT)mX;
        localcoord.Y -= (SHORT)(mY+1);


        // because frame at the top and bottom, only handle mouse events when within the list rows
        if (localcoord.Y >= 1 && localcoord.Y <= mHeight - 2)
        {

            if (event.dwEventFlags == 0)
            {
                if (event.dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED)
                {
                    mSelection = mTopVisibleRow + localcoord.Y;
                    gConsole.Invalidate();
                }
            }
            else if (event.dwEventFlags == DOUBLE_CLICK)
            {
                mSelection = mTopVisibleRow + localcoord.Y;
                return OnKey(VK_RETURN, 0);
            }
            else if (event.dwEventFlags == MOUSE_WHEELED)
            {
                SHORT wheelDelta = HIWORD(event.dwButtonState);
                if (wheelDelta < 0)
                {
                    return OnKey(VK_DOWN, 0);
                }
                else
                {
                    return OnKey(VK_UP, 0);
                }
            }
        }

        return ConsoleWin::OnMouse(event);
    }

    bool ListboxWin::OnKey(int keycode, char c)
    {
        bool bHandled = false;
        bool bCTRLHeld = GetKeyState(VK_CONTROL) & 0x800;
        bool bSHIFTHeld = GetKeyState(VK_SHIFT) & 0x800;
        gConsole.Invalidate();

        switch (keycode)
        {
        case VK_TAB:
            if (bSHIFTHeld)
            {
                mSelection--;
                if (mSelection < 0)
                    mSelection = (int64_t)mEntries.size() - 1;
                bHandled = true;
            }
            else
            {
                mSelection++;
                if (mSelection >= (int64_t)mEntries.size())
                    mSelection = 0;
                bHandled = true;
            }
            break;
        case VK_UP:
            {
                mSelection--;
                if (mSelection < 0)
                    mSelection = (int64_t)mEntries.size() - 1;
                bHandled = true;
        }
            break;
        case VK_DOWN:
        {
            mSelection++;
            if (mSelection >= (int64_t)mEntries.size())
                mSelection = 0;
            bHandled = true;
        }
        break;
        case VK_HOME:
            mSelection = 0;
            bHandled = true;
            break;
        case VK_END:
            mSelection = (int64_t)mEntries.size() - 1;
            bHandled = true;
            break;
        case VK_PRIOR:
            mSelection -= mHeight;
            if (mSelection < 0)
                mSelection = 0;
            bHandled = true;
            break;
        case VK_NEXT:
            mSelection+=mHeight;
            if (mSelection >= (int64_t)mEntries.size()-1)
                mSelection = (int64_t)mEntries.size() - 1;
            bHandled = true;
            break;
        case VK_RETURN:
            {
            rawCommandBuf.AddUndoEntry();
            rawCommandBuf.HandlePaste(GetSelection());
            mEntries.clear();
            SetVisible(false);
            bHandled = true;
        }
            break;
        case VK_ESCAPE:
            mEntries.clear();
            SetVisible(false);
            rawCommandBuf.ClearSelection();
            bHandled = true;
            break;
        }
        Clear(mClearAttrib, mbGradient);
        UpdateCaptions();

        return bHandled;
    }


    HistoryWin::HistoryWin()
    {
        mMinWidth = 64;
    }

    bool HistoryWin::OnKey(int keycode, char c)
    {
        bool bCTRLHeld = GetKeyState(VK_CONTROL) & 0x800;
        bool bSHIFTHeld = GetKeyState(VK_SHIFT) & 0x800;
        gConsole.Invalidate();

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
                        SetVisible(false);
                        rawCommandBuf.ClearSelection();
                    }
                }
                return true;
            }
        }

        return ListboxWin::OnKey(keycode, c);
    }


    void ListboxWin::SetEntries(tStringList entries, string selectionSearch, int64_t anchor_l, int64_t anchor_b)
    { 
        mEntries = entries; 
        mAnchorL = anchor_l;
        mAnchorB = anchor_b;
        mWidth = 0;
        mHeight = 0;
        gConsole.Invalidate();

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

        if ((int64_t)mEntries.size() > mHeight)
        {
            // scrollbar
            width = MH::Max(mMinWidth+1, mWidth);
        }


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

        if (width > gConsole.Width())
        {
            r.l = 1;
            r.r = gConsole.Width() - 1;
        }


        // move window to fit on screen
        if (r.r > gConsole.Width())
        {
            int64_t shiftleft = r.r - gConsole.Width();
            r.l -= shiftleft;
            r.r -= shiftleft;
        }
        if (r.l < 0)
        {
            int64_t shiftright = -r.l;
            r.l += shiftright;
            r.r += shiftright;
        }
        if (r.b > gConsole.Height())
        {
            int64_t shiftdown = r.b - gConsole.Height();
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


    bool FolderList::OnKey(int keycode, char c)
    {
        bool bHandled = false;
        bool bCTRLHeld = GetKeyState(VK_CONTROL) & 0x800;
        bool bSHIFTHeld = GetKeyState(VK_SHIFT) & 0x800;
        gConsole.Invalidate();

        switch (keycode)
        {
            case VK_BACK:
            {
                fs::path navigate(mPath);
                if (SH::EndsWith(mPath, "\\"))
                    navigate = mPath.substr(0, mPath.length() - 1); // strip last directory indicator so that parent_path works
                if (navigate.has_parent_path())
                    Scan(navigate.parent_path().string(), mAnchorL, mAnchorB);
                return true;
            }
            case VK_TAB:
            {
                string selection(mPath + CommandLineParser::StripEnclosure(GetSelection()));
                if (fs::is_directory(selection))
                    Scan(selection, mAnchorL, mAnchorB);
                else if (IsRootFolder(CommandLineParser::StripEnclosure(GetSelection())))
                    Scan(CommandLineParser::StripEnclosure(GetSelection()), mAnchorL, mAnchorB);

                return true;
            }
            case VK_RETURN:
            {
                string selection(CommandLineParser::StripEnclosure(GetSelection()));
                if (selection == kEmptyFolderCaption)
                    selection = mPath;

                rawCommandBuf.AddUndoEntry();
                rawCommandBuf.HandlePaste(CommandLineParser::EncloseWhitespaces(mPath + selection));
                mEntries.clear();
                SetVisible(false);
                return true;
            }
        }

        bHandled = ListboxWin::OnKey(keycode, c);
        UpdateCaptions();

        return bHandled;
    }

    bool FolderList::OnMouse(MOUSE_EVENT_RECORD event)
    {
        COORD localcoord = event.dwMousePosition;
        localcoord.X -= (SHORT)mX;
        localcoord.Y -= (SHORT)(mY + 1);

        // override base double-click for directories
        if (event.dwEventFlags == DOUBLE_CLICK)
        {
            mSelection = mTopVisibleRow + localcoord.Y;
            string selection(mPath + CommandLineParser::StripEnclosure(GetSelection()));
            if (fs::is_directory(selection) || IsRootFolder(CommandLineParser::StripEnclosure(GetSelection())))
                return OnKey(VK_TAB, 0);
        }

        return ListboxWin::OnMouse(event);
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




    void AnsiColorWin::Paint(tConsoleBuffer& backBuf)
    {
        // Update display
        if (!mbVisible)
            return;

        ConsoleWin::BasePaint();

        Rect r;
        GetInnerArea(r);

        DrawClippedAnsiText(r, mText, true, &r);

        ConsoleWin::RenderToBackBuf(backBuf);
    }


    void UsageWin::Paint(tConsoleBuffer& backBuf)
    {
        if (!pCLP)
            return;

        // Update display
        if (!mbVisible)
            return;

        ConsoleWin::BasePaint();

        Rect r;
        GetInnerArea(r);

        string sDrawString = mText;
        if (!sHighlightParam.empty())
        {
            string sHighlight = highlightAttrib.ToAnsi();
            sDrawString = SH::replaceTokens(sDrawString, sHighlightParam, sHighlight + sHighlightParam + mClearAttrib.ToAnsi());
        }



        DrawClippedAnsiText(r, sDrawString, true, &r);

        ConsoleWin::RenderToBackBuf(backBuf);
    }


void ParamListWin::Paint(tConsoleBuffer& backBuf)
{
    if (!pCLP)
        return;

    ConsoleWin::BasePaint();

    Rect drawArea;
    GetInnerArea(drawArea);

    string sRaw(rawCommandBuf.GetText());

    size_t rows = std::max<size_t>(1, (sRaw.size() + gConsole.Width() - 1) / gConsole.Width());

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // if the command line is bigger than the screen, show the last n rows that fit


    if (!enteredParams.empty())   // if there are params entered
    {
        // compute column widths

        const int kColName = 0;
        const int kColEntry = 1;
        const int kColUsage = 2;


        const size_t kMinColWidth = 12;
        const size_t kMinUsageCol = 24;
        vector<size_t> colWidths;
        colWidths.resize(3);
        colWidths[kColName] = kMinColWidth;
        colWidths[kColEntry] = kMinColWidth;
        for (int paramindex = 0; paramindex < enteredParams.size(); paramindex++)
        {
            string sText(ExpandEnvVars(enteredParams[paramindex].sParamText));
            colWidths[kColEntry] = std::max<size_t>(sText.length(), colWidths[kColEntry]);

            ParamDesc* pPD = enteredParams[paramindex].pRelatedDesc;
            if (pPD)
            {
                size_t len = pPD->msName.length();
                if (pPD->IsNamed())
                    len += 2;   // add 2 for leading '-' and ':'

                colWidths[kColName] = std::max<size_t>(len, colWidths[kColName]);
            }
        }

        int64_t paddedScreenW = gConsole.Width() - 2;  // 1 char between each of the three columns

        // if there's enough room for the final column
        if (colWidths[kColName] + colWidths[kColEntry] < (paddedScreenW-kMinUsageCol))
        {
            colWidths[kColUsage] = paddedScreenW - (colWidths[kColName] + colWidths[kColEntry]);
        }
        else
        {
            colWidths[kColUsage] = kMinUsageCol;
            colWidths[kColEntry] = paddedScreenW - (colWidths[kColName]+colWidths[kColUsage]);

        }


        tStringArray strings(3);
        tAttribArray attribs(3);

        size_t row = drawArea.t;



        bool bAlternateBGCol = false;
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
                }
            }
            else
            {
                attribs[kColEntry] = kBadParam;
                strings[kColUsage] = "UNKNOWN COMMAND";
                attribs[kColUsage] = kBadParam;
            }

            row += DrawFixedColumnStrings(drawArea.l, row, strings, colWidths, 1, attribs, &drawArea);
        }



        // next list positional params
        row++;
        string sSection = "-positional params-" + string(gConsole.Width(), '-');
        DrawClippedText(Rect(drawArea.l, row, drawArea.r, row+1), sSection, kAttribSection, false, &drawArea);
        row++;

        tEnteredParams posParams = GetPositionalEntries();

        for (auto& param : posParams)
        {


            attribs[kColName] = kRawText;
            attribs[kColEntry] = kUnknownParam;
            attribs[kColUsage] = kRawText;

            string sFailMessage;
            if (param.pRelatedDesc)
            {
                strings[kColName] = param.pRelatedDesc->msName;
                string sValue = ExpandEnvVars(param.sParamText);
                if (param.pRelatedDesc->DoesValueSatisfy(sValue, sFailMessage))
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

            strings[kColEntry] = ExpandEnvVars(param.sParamText);

            if (bAlternateBGCol)
            {
                attribs[kColName].SetBG(255, (uint8_t)((mClearAttrib.br * 200) / 255), (uint8_t)((mClearAttrib.bg * 200) / 255), (uint8_t)((mClearAttrib.bb * 200) / 255));
                attribs[kColEntry].SetBG(255, (uint8_t)((mClearAttrib.br * 200) / 255), (uint8_t)((mClearAttrib.bg * 200) / 255), (uint8_t)((mClearAttrib.bb * 200) / 255));
                attribs[kColUsage].SetBG(255, (uint8_t)((mClearAttrib.br * 200) / 255), (uint8_t)((mClearAttrib.bg * 200) / 255), (uint8_t)((mClearAttrib.bb * 200) / 255));
            }
            bAlternateBGCol = !bAlternateBGCol;

            row += DrawFixedColumnStrings(drawArea.l, row, strings, colWidths, 1, attribs, &drawArea);
        }


        tEnteredParams namedParams = GetNamedEntries();

        if (!namedParams.empty())
        {


            row++;
            sSection = "-named params-" + string(gConsole.Width(), '-');
            DrawClippedText(Rect(drawArea.l, row, drawArea.r, row+1), sSection, kAttribSection, false, &drawArea);
            row++;

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

                    strings[kColEntry] = sValue;
                }
                else
                {
                    attribs[kColEntry] = kUnknownParam;
                    strings[kColEntry] = ExpandEnvVars(param.sParamText);

                    strings[kColUsage] = "Unknown parameter";
                    attribs[kColUsage] = kUnknownParam;
                }

                if (bAlternateBGCol)
                {
                    attribs[kColName].SetBG(255, (uint8_t)((mClearAttrib.br * 200) / 255), (uint8_t)((mClearAttrib.bg * 200) / 255), (uint8_t)((mClearAttrib.bb * 200) / 255));
                    attribs[kColEntry].SetBG(255, (uint8_t)((mClearAttrib.br * 200) / 255), (uint8_t)((mClearAttrib.bg * 200) / 255), (uint8_t)((mClearAttrib.bb * 200) / 255));
                    attribs[kColUsage].SetBG(255, (uint8_t)((mClearAttrib.br * 200) / 255), (uint8_t)((mClearAttrib.bg * 200) / 255), (uint8_t)((mClearAttrib.bb * 200) / 255));
                }
                bAlternateBGCol = !bAlternateBGCol;

                row += DrawFixedColumnStrings(drawArea.l, row, strings, colWidths, 1, attribs, &drawArea);


            }
        }
    }
    else
    {
        // no commands entered
        Table commandsTable;
        pCLP->GetCommandsTable(commandsTable);
        commandsTable.AlignWidth(gConsole.Width());
        string sCommands = commandsTable;
        DrawClippedAnsiText(drawArea, sCommands, true, &drawArea);
    }

    ConsoleWin::RenderToBackBuf(backBuf);
    return;
}



    FolderList::FolderList()
    {
        mMinWidth = 64;
    }


    bool FolderList::Scan(std::string sSelectedPath, int64_t anchor_l, int64_t anchor_b)
    {
        gConsole.Invalidate();

        string sPath = FindClosestParentPath(sSelectedPath);
        if (!fs::exists(sPath))
            return false;

        bool bIsRoot = false;
        sPath = FH::Canonicalize(sPath);
        if (fs::is_directory(sPath))
        {
            bIsRoot = IsRootFolder(sPath);
        }
        else
        {
            sPath = fs::path(sPath).parent_path().string();
        }

        tStringList folders;
        tStringList files;

        try
        {
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
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << "Filesystem error: " << e.what() << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
            return false;
        }

        mEntries.clear();
        if (folders.empty() && files.empty())
        {
            mEntries.push_back(kEmptyFolderCaption);
        }
        else
        {
            if (bIsRoot)
            {

                char buf[1024];
                GetLogicalDriveStringsA(1024, buf);

                char* pBuf = &buf[0];
                while (*pBuf && pBuf < &buf[1024])
                {
                    string sDrive(pBuf);
                    mEntries.push_back(sDrive);
                    pBuf += sDrive.length()+1;
                }

            }
            else
            {
                folders.push_front("..");   
            }


            mEntries.splice(mEntries.end(), folders);
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

    bool FolderList::IsRootFolder(std::string sPath)
    {
        return fs::path(sPath).root_path() == fs::path(sPath);
    }


    bool RawEntryWin::HandleParamContext()
    {
        size_t start = string::npos;
        size_t end = string::npos;
        string sText;
        gConsole.Invalidate();


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
            popupListWin.SetVisible();
            popupListWin.positionCaption[Position::CT] = "Commands";
            popupListWin.SetEntries(availableModes, sText, anchor.X, anchor.Y);
        }
        else if (sText == enteredParams[0].sParamText && !availableModes.empty())
        {
            popupListWin.SetVisible();
            popupListWin.positionCaption[Position::CT] = "Commands";
            popupListWin.SetEntries(availableModes, sText, anchor.X, anchor.Y);
        }
        else if (sText[0] == '-')
        {
            popupListWin.positionCaption[Position::CT] = "Options";
            popupListWin.SetVisible();
            popupListWin.SetEntries(availableNamedParams, sText, anchor.X, anchor.Y);
        }
        else if (sText[0] == '%')
        {
            popupListWin.positionCaption[Position::CT] = "Vars";
            popupListWin.SetVisible();

            tKeyValList keyVals = GetEnvVars();
            tStringList keys;
            for (const auto& kv : keyVals)
                keys.push_back('%' + kv.first + '%');

            popupListWin.SetEntries(keys, sText, anchor.X, anchor.Y);
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
                            popupFolderListWin.SetVisible();
                        }
                    }
                }
            }
        }

        return true;
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


    void CommandLineEditor::UpdateUsageWin()
    {
        string sParamUnderCursor;
        size_t startIndex;
        size_t endIndex;
        ParamDesc* pPD = nullptr;
        usageBuf.sHighlightParam.clear();
        if (rawCommandBuf.GetParameterUnderIndex(rawCommandBuf.CursorToTextIndex(rawCommandBuf.mLocalCursorPos), startIndex, endIndex, sParamUnderCursor, &pPD))
        {
            string sExample;
            pCLP->GetCommandLineExample(CLP::GetMode(), sExample);
            usageBuf.SetText(string(COL_BLACK) + "usage: " + sExample);

            if (pPD)
            {
                usageBuf.sHighlightParam = pPD->msName;
                usageBuf.highlightAttrib = kSelectedText;
            }
            else
            {
                if (pCLP->IsRegisteredMode(sParamUnderCursor))
                {
                    usageBuf.sHighlightParam = sParamUnderCursor;
                    usageBuf.highlightAttrib = kSelectedText;
                }
                else
                {
                    usageBuf.sHighlightParam = "COMMAND";
                    usageBuf.highlightAttrib = kBadParam;
                }
            }
        }
    }

    void CommandLineEditor::DrawToScreen()
    {
        SHORT paramListRows = (SHORT)enteredParams.size();


        gConsole.SetCursorVisible(!helpTableWin.mbVisible && !popupListWin.mbVisible && !popupFolderListWin.mbVisible && !historyWin.mbVisible);

        if (helpTableWin.mbVisible)
        {
            helpTableWin.Paint(gConsole.BackBuffer());
        }
        else
        {
            string sTop = "[F1-Help] [F2-Env Vars] [ESC-Quit] [TAB-Contextual] [CTRL+Z-Undo]";
            if (!commandHistory.empty())
                sTop += " [UP-History (" + SH::FromInt(commandHistory.size()) + ")]";
            topInfoBuf.SetText(string(COL_BLACK) + sTop);

            paramListBuf.Paint(gConsole.BackBuffer());
            rawCommandBuf.Paint(gConsole.BackBuffer());
            usageBuf.Paint(gConsole.BackBuffer());
            topInfoBuf.Paint(gConsole.BackBuffer());
            popupListWin.Paint(gConsole.BackBuffer());
            historyWin.Paint(gConsole.BackBuffer());
            popupFolderListWin.Paint(gConsole.BackBuffer());
        }

        bool bChanges = gConsole.Render();

        if (bChanges)
        {
            rawCommandBuf.UpdateCursorPos(rawCommandBuf.mLocalCursorPos);
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
                outValue = ExpandEnvVars(sParamText.substr(nIndexOfColon + 1));    // everything after colon
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
            while (isblank((uint8_t)sText[i]) && i < length) // skip whitespace
                i++;

            size_t endofparam = i;
            // find end of param
            while (!isblank((uint8_t)sText[endofparam]) && endofparam < length)
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

                string sValue = ExpandEnvVars(param.sParamText);
                if (!param.pRelatedDesc)       // unsatisfied positional parameter
                    param.drawAttributes = kBadParam;
                else if (!param.pRelatedDesc->DoesValueSatisfy(sValue, sFailMessage))     // positional param has descriptor but not in required range
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

        gConsole.Invalidate();

        mLastParsedText = sText;

        enteredParams = ParamsFromText(mLastParsedText);
//        rawCommandBuf.mEnteredParams = mParams;
        availableNamedParams = GetCLPNamedParamsForMode(CLP::GetMode());
    }

    eResponse CommandLineEditor::Edit(int argc, char* argv[], std::string& outEditedCommandLine)
    {
//        appEXE = argv[0];
        tStringArray params(CommandLineParser::ToArray(argc-1, argv));    // first convert to param array then to a string which will enclose parameters with whitespaces
        return Edit(CommandLineParser::ToString(params), outEditedCommandLine);
    }

    bool CommandLineEditor::UpdateFromConsoleSize(bool bForce)
    {
        if (bForce || gConsole.UpdateScreenInfo())
        {
            gConsole.Invalidate();

            SHORT w = gConsole.Width();
            SHORT h = gConsole.Height();

            if (w < 1)
                w = 1;
            if (h < 8)
                h = 8;

            topInfoBuf.Clear(kAttribTopInfoBG);
            topInfoBuf.SetArea(Rect(0, 1, w, 2));

            paramListBuf.Clear(kAttribParamListBG);
            paramListBuf.SetArea(Rect(0, 2, w, h - 5));

            usageBuf.Clear(kAttribTopInfoBG);
            usageBuf.SetArea(Rect(0, h - 5, w, h - 4));

            rawCommandBuf.Clear(0xff000000ffffffff);
            rawCommandBuf.SetArea(Rect(0, h - 4, w, h));

            helpTableWin.Clear(kAttribHelpBG, true);
            helpTableWin.SetArea(Rect(0, 1, w, h));
            helpTableWin.SetEnableFrame();
            helpTableWin.UpdateCaptions();

            popupFolderListWin.Clear(kAttribFolderListBG);
            popupFolderListWin.SetEnableFrame();

            historyWin.Clear(0xFF888800FF000000);
            historyWin.SetEnableFrame();

            popupListWin.Clear(kAttribListBoxBG);
            popupListWin.SetEnableFrame();
            popupListWin.mMinWidth = 32;
            popupListWin.positionCaption[ConsoleWin::Position::RB] = "[UP/DOWN][ENTER-Select][ESC-Cancel]";

            DrawToScreen();
            return true;
        }

        return false; // no changes
    }

    void CommandLineEditor::ShowHelp()
    {
        helpTableWin.Init(Rect(0, 1, gConsole.Width(), gConsole.Height()));
        helpTableWin.Clear(kAttribHelpBG, true);
        gConsole.Invalidate();

        Rect drawArea;
        helpTableWin.GetInnerArea(drawArea);
        int64_t drawWidth = drawArea.r - drawArea.l-2;

        assert(drawWidth > 0);

        Table& t = helpTableWin.mTable;

        t.Clear();

        if (pCLP)
        {
            string sMode = CLP::GetMode();

            pCLP->GetHelpTable(sMode, t);

            if (pCLP->IsRegisteredMode(sMode))
            {
//                pCLP->GetModeHelpTable(sMode, helpTable);
                helpTableWin.positionCaption[ConsoleWin::Position::LT] = "Help for \"" + sMode + "\"";
            }
            else
            {
  //              pCLP->GetAppDescriptionHelpTable(helpTable);
                helpTableWin.positionCaption[ConsoleWin::Position::LT] = "General Help";
            }
        }


        t.AddRow(" ");
        t.AddRow(SectionStyle, " Help for Interactive Command Line Editor ");
        t.AddRow("Rich editing of command line passed in with contextual auto-complete popups.");
        t.AddRow("Command Line example usage is visible just above the command line entry.");
        t.AddRow("Parameters (positional and named) along with desciptions are enumerated above.");
        t.AddRow("Path parameters bring up folder listing at that path. (TAB to go into the folder, BACKSPACE to go up a folder.)");
        t.AddRow(" ");

        t.AddRow(SubSectionStyle, "--Key Combo--", "--Action--");

        t.AddRow(ParamStyle, "[F1]", "General help or contextual help (if first parameter is recognized command.)");
        t.AddRow(ParamStyle, "[F2]", "Show Environment Variables");

        t.AddRow(ParamStyle, "[TAB]", "Context specific popup");

        t.AddRow(ParamStyle, "[SHIFT-LEFT/RIGHT]", "Select characters");
        t.AddRow(ParamStyle, "[SHIFT+CTRL-LEFT/RIGHT]", "Select words");

        t.AddRow(ParamStyle, "[CTRL-A]", "Select All");
        t.AddRow(ParamStyle, "[CTRL-C/V]", "Copy/Paste");
        t.AddRow(ParamStyle, "[CTRL-Z]", "Undo");

        t.AddRow(ParamStyle, "[UP]", "Command Line History (When cursor at top)");
        Table::SetDecLineBorders(t, COL_GRAY);


        helpTableWin.UpdateCaptions();
    }

    void PrintConsoleInputMode(DWORD mode)
    {
        std::cout << "Console Input Mode: 0x" << std::hex << mode << std::dec << std::endl;

        if (mode & ENABLE_PROCESSED_INPUT)
            std::cout << "  ENABLE_PROCESSED_INPUT (0x0001) - Ctrl+C processed by system" << std::endl;

        if (mode & ENABLE_LINE_INPUT)
            std::cout << "  ENABLE_LINE_INPUT (0x0002) - Line input mode" << std::endl;

        if (mode & ENABLE_ECHO_INPUT)
            std::cout << "  ENABLE_ECHO_INPUT (0x0004) - Characters echoed" << std::endl;

        if (mode & ENABLE_WINDOW_INPUT)
            std::cout << "  ENABLE_WINDOW_INPUT (0x0008) - Window size events reported" << std::endl;

        if (mode & ENABLE_MOUSE_INPUT)
            std::cout << "  ENABLE_MOUSE_INPUT (0x0010) - Mouse events reported" << std::endl;

        if (mode & ENABLE_INSERT_MODE)
            std::cout << "  ENABLE_INSERT_MODE (0x0020) - Insert mode for line editing" << std::endl;

        if (mode & ENABLE_QUICK_EDIT_MODE)
            std::cout << "  ENABLE_QUICK_EDIT_MODE (0x0040) - Quick edit mode (may block mouse)" << std::endl;

        if (mode & ENABLE_EXTENDED_FLAGS)
            std::cout << "  ENABLE_EXTENDED_FLAGS (0x0080) - Extended flags enabled" << std::endl;

        if (mode & ENABLE_AUTO_POSITION)
            std::cout << "  ENABLE_AUTO_POSITION (0x0100) - Auto position cursor" << std::endl;

        if (mode & ENABLE_VIRTUAL_TERMINAL_INPUT)
            std::cout << "  ENABLE_VIRTUAL_TERMINAL_INPUT (0x0200) - VT input sequences" << std::endl;

        std::cout << std::endl;
    }




    bool CommandLineEditor::OnKey(int keycode, char c)
    {
        gConsole.Invalidate();

        if (popupListWin.mbVisible)
        {
            return popupListWin.OnKey(keycode, c);
        }
        else if (historyWin.mbVisible)
        {
            return historyWin.OnKey(keycode, c);
        }
        else if (popupFolderListWin.mbVisible)
        {
            return popupFolderListWin.OnKey(keycode, c);
        }
        else if (helpTableWin.mbVisible)
        {
            return helpTableWin.OnKey(keycode, c);
        }
        else
        {
            if (keycode == VK_F1)
            {
                ShowHelp();
                return true;
            }
            else if (keycode == VK_F2)
            {
                ShowEnvVars();
                return true;
            }
            else
            {
                return rawCommandBuf.OnKey(keycode, c);
            }
        }

        return false;
    }

    bool CommandLineEditor::OnMouse(MOUSE_EVENT_RECORD event)
    {
        COORD coord = event.dwMousePosition;

        if (helpTableWin.IsOver(coord.X, coord.Y))
        {
            return helpTableWin.OnMouse(event);
        }

        if (popupListWin.IsOver(coord.X, coord.Y))
        {
            return popupListWin.OnMouse(event);
        }

        if (historyWin.IsOver(coord.X, coord.Y))
        {
            return historyWin.OnMouse(event);
        }

        if (popupFolderListWin.IsOver(coord.X, coord.Y))
        {
            return popupFolderListWin.OnMouse(event);
        }

        if (paramListBuf.IsOver(coord.X, coord.Y))
        {
            return paramListBuf.OnMouse(event);
        }

        if (rawCommandBuf.IsOver(coord.X, coord.Y))
        {
            return rawCommandBuf.OnMouse(event);
        }

        if (topInfoBuf.IsOver(coord.X, coord.Y))
        {
            return topInfoBuf.OnMouse(event);
        }

        return false;
    }


    eResponse CommandLineEditor::Edit(const string& sCommandLine, std::string& outEditedCommandLine)
    {
        // Get the handle to the standard input
        if (!gConsole.Init())
        {
            return kErrorAbort;
        }
        ResetCols();

        int16_t w = gConsole.Width();
        int16_t h = gConsole.Height();


        // Main loop to read input events
        INPUT_RECORD inputRecord[128];
        DWORD numEventsRead;




        popupListWin.Init(Rect(w / 4, h / 4, w * 3 / 4, h * 3 / 4));
        popupListWin.SetVisible(false);

        historyWin.Init(Rect(w / 4, h / 4, w * 3 / 4, h * 3 / 4));
        historyWin.SetVisible(false);


        popupFolderListWin.Init(Rect(w / 4, h / 4, w * 3 / 4, h * 3 / 4));
        popupFolderListWin.SetVisible(false);

         
        LoadHistory();

        rawCommandBuf.Init(Rect(0, h - 4, w, h));
        rawCommandBuf.SetVisible();


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



        // for longer and longer idles when the application has no activity
        const uint64_t kIdleInc = 1000;
        const uint64_t kIdleMin = 100;
        const uint64_t kIdleMax = 250000; // 250ms

        uint64_t idleSleep = kIdleMin;

        while (!rawCommandBuf.mbDone && !rawCommandBuf.mbCanceled)
        {

            UpdateFromConsoleSize();
            UpdateParams();
            if (gConsole.Invalid())
            {
                UpdateUsageWin();
                DrawToScreen();
                gConsole.Render();
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
                if (msg.message == WM_HOTKEY && (msg.wParam == CTRL_V_HOTKEY || msg.wParam == SHIFT_INSERT_HOTKEY))
                {
                    rawCommandBuf.AddUndoEntry();
                    rawCommandBuf.HandlePaste(GetTextFromClipboard());
                    gConsole.Invalidate();
                }
                else
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
                idleSleep = kIdleMin;    // reset
            }

            DWORD numEventsAvailable;
            if (GetNumberOfConsoleInputEvents(gConsole.InputHandle(), &numEventsAvailable) && numEventsAvailable > 0)
            {
                if (ReadConsoleInput(gConsole.InputHandle(), inputRecord, 128, &numEventsRead) && numEventsRead > 0)
                {
                    for (DWORD i = 0; i < numEventsRead; i++)
                    {
                        // Debug output
//                        std::cout << "Event type: " << inputRecord[i].EventType << std::endl;


                        if (inputRecord[i].EventType == MOUSE_EVENT)
                        {
                            MOUSE_EVENT_RECORD event = inputRecord[i].Event.MouseEvent;
                            event.dwMousePosition.Y++;  // adjust for our coords
                            OnMouse(event);
                        }
                        else if (inputRecord[i].EventType == KEY_EVENT && inputRecord[i].Event.KeyEvent.bKeyDown)
                        {
                            OnKey(inputRecord[i].Event.KeyEvent.wVirtualKeyCode, inputRecord[i].Event.KeyEvent.uChar.AsciiChar);
                        }
                    }
                }

                idleSleep = kIdleMin;    // reset
            }

            if (idleSleep < kIdleMin)
                idleSleep = kIdleMin;
            if (idleSleep > kIdleMax)
                idleSleep = kIdleMax;

            std::this_thread::sleep_for(std::chrono::microseconds(idleSleep));
        }

        UnregisterHotKey(nullptr, CTRL_V_HOTKEY);
        UnregisterHotKey(nullptr, SHIFT_INSERT_HOTKEY);

        gConsole.Shutdown();

        string OutCommandLine = fs::path(CLP::appPath + "\\" + CLP::appName).string() + " " + rawCommandBuf.GetText();

/*        if (!rawCommandBuf.mbCanceled)
        {
            OutCommandLine += "\n";
        }*/


        OutputCommandToConsole(OutCommandLine);

        outEditedCommandLine = rawCommandBuf.GetText();

        if (rawCommandBuf.mbCanceled)
            return kCanceled;

        return kSuccess;
    }

    bool CommandLineEditor::OutputCommandToConsole(const std::string& command)
    {
        // Find the window handle for the Command Prompt (assuming it has a unique title or class name)
        HWND hwnd = GetConsoleWindow();
        if (!hwnd)
        {
            cerr << "Failed to get console window!\n";
            return false;
        }

        // Set the Command Prompt window to the foreground
        SetForegroundWindow(hwnd);

        // Wait for the window to be focused
        //Sleep(10);

        // Prepare a list of inputs
        std::vector<INPUT> inputs;

        for (char c : command)
        {
            // Determine if Shift key needs to be held down
            bool shift = isupper(c) || strchr("!@#$%^&*()_+{}|:\"<>?", c);

            // Map character to a virtual key code
            SHORT vk = VkKeyScan(c);
            if (vk == -1) continue;  // Skip if the character doesn't have a virtual key

            // Add Shift key down if necessary
            if (shift) 
            {
                INPUT shiftDown = {};
                shiftDown.type = INPUT_KEYBOARD;
                shiftDown.ki.wVk = VK_SHIFT;
                inputs.push_back(shiftDown);
            }

            // Add key down and key up events for the character
            INPUT keyDown = {};
            keyDown.type = INPUT_KEYBOARD;
            keyDown.ki.wVk = vk & 0xFF;  // Extract the virtual key code
            inputs.push_back(keyDown);

            INPUT keyUp = keyDown;
            keyUp.ki.dwFlags = KEYEVENTF_KEYUP;
            inputs.push_back(keyUp);

            // Add Shift key up if necessary
            if (shift) 
            {
                INPUT shiftUp = {};
                shiftUp.type = INPUT_KEYBOARD;
                shiftUp.ki.wVk = VK_SHIFT;
                shiftUp.ki.dwFlags = KEYEVENTF_KEYUP;
                inputs.push_back(shiftUp);
            }
        }

        // Send the input array to the system
        SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));

        return true;
    }


};

#endif // ENABLE_CLE
