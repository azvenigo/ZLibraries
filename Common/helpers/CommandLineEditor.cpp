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
    CommandLineEditor::CommandLineEditor()
    {
        mpCLP = nullptr;
    }

    void CommandLineEditor::UpdateCursorPos()
    {
        int index = (int)CursorToTextIndex(mCursorPos);
        if (index > (int)mText.length())
        {
            index = (int)mText.length();
            mCursorPos = TextIndexToCursor(index);
        }
        
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


    void CommandLineEditor::FindNextBreak(int nDir)
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

        mCursorPos = TextIndexToCursor(index);
    }

    bool CommandLineEditor::IsIndexInSelection(int64_t i)
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


    string CommandLineEditor::GetSelectedText()
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
        outStart = CursorToTextIndex(mCursorPos);
        if (outStart == string::npos)
            return false;

        outEnd = outStart;
        while (outStart > 0 && !isblank((int)mText[outStart]))
            outStart--;
        while (outEnd < mText.size() && !isblank((int)mText[outEnd]))
            outEnd++;

        outParam = mText.substr(outStart, outEnd - outStart);
        return true;
    }


    bool ConsoleBuffer::Init(size_t w, size_t h)
    {
        mBuffer.resize(w * h);
        mWidth = w;
        mHeight = h;
        Clear();
        return true;
    }

    void ConsoleBuffer::Clear(WORD attrib)
    {
        for (size_t i = 0; i < mBuffer.size(); i++)
        {
            mBuffer[i].Char.AsciiChar = 0;
            mBuffer[i].Attributes = attrib;
        }

    }

    void ConsoleBuffer::DrawCharClipped(char c, size_t x, size_t y, WORD attrib)
    {
        size_t offset = y * mWidth + x;
        DrawCharClipped(c, (int64_t)offset, attrib);
    }

    void ConsoleBuffer::DrawCharClipped(char c, int64_t offset, WORD attrib)
    {
        if (offset >= 0 && offset < (int64_t)mBuffer.size())    // clip
        {
            mBuffer[offset].Char.AsciiChar = c;
            mBuffer[offset].Attributes |= attrib;
        }
    }



    void ConsoleBuffer::DrawFixedColumnStrings(size_t x, size_t y, tStringArray& strings, vector<size_t>& colWidths, tAttribArray attribs)
    {
        assert(strings.size() == colWidths.size() && colWidths.size() == attribs.size());

        for (int i = 0; i < strings.size(); i++)
        {
            string sDraw(strings[i].substr(0, colWidths[i]));
            DrawClippedText(x, y, sDraw, attribs[i]);
            x += colWidths[i];
        }
    }

    void ConsoleBuffer::DrawClippedText(size_t x, size_t y, std::string text, WORD attributes, bool bWrap)
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

    void ConsoleBuffer::PaintToWindowsConsole(HANDLE hOut)
    {
        COORD origin(0, 0);
        SMALL_RECT writeRegion((SHORT)mX, (SHORT)mY, (SHORT)(mX + mWidth), (SHORT)(mY + mHeight));
        COORD bufsize((SHORT)mWidth, (SHORT)mHeight);
        WriteConsoleOutput(hOut, &mBuffer[0], bufsize, origin, &writeRegion);
    }



    void CommandLineEditor::UpdateDisplay()
    {
        //paramListBufTopRow = 0;

        size_t rows = std::max<size_t>(1, (mText.size() + screenBufferInfo.dwSize.X - 1) / screenBufferInfo.dwSize.X);
//        rawCommandBufTopRow = screenBufferInfo.dwSize.Y - rows;

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Clear the raw command buffer and param buffers
        rawCommandBuf.Clear();
        paramListBuf.Clear(BACKGROUND_BLUE);
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // if the command line is bigger than the screen, show the last n rows that fit

        int64_t rawCommandRows = (mText.size() + rawCommandBuf.mWidth - 1) / rawCommandBuf.mWidth;
        int64_t firstRenderRow = 0;

        if (rawCommandRows > (int64_t)rawCommandBuf.mHeight)
            firstRenderRow = (int64_t)rawCommandBuf.mHeight - rawCommandRows;    // could be negative as we're clipping into the raw command console

//        rawCommandBuf.DrawClippedText(0, 0, mText);
        int64_t consoleBufIndex = firstRenderRow * (int64_t)(rawCommandBuf.mWidth);
        for (size_t i = 0; i < mText.length(); i++)
        {
            WORD attrib = FOREGROUND_WHITE;
            if (IsIndexInSelection(i))
                attrib |= BACKGROUND_INTENSITY;
            rawCommandBuf.DrawCharClipped(mText[i], consoleBufIndex, attrib);
            consoleBufIndex++;
        }

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
            colWidths[kColUsage] = screenBufferInfo.dwSize.X - (colWidths[kColName] + colWidths[kColEntry]);
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


            // first param is mode
            size_t row = 0;
            string sMode(mParams[0].sParamText);

            strings[kColName] = "MODE";
            attribs[kColName] = FOREGROUND_WHITE;

            strings[kColEntry] = sMode;
            attribs[kColUsage] = FOREGROUND_WHITE;

            tStringList modes = GetCLPModes();
            bool bModePermitted = modes.empty() || std::find(modes.begin(), modes.end(), sMode) != modes.end(); // if no modes registered or (if there are) if the first param matches one
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
                strings[kColUsage] = "Unknown mode";
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


    string CommandLineEditor::GetTextFromClipboard() 
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

    bool CommandLineEditor::CopyTextToClipboard(const std::string& text)
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
        blank.resize(screenBufferInfo.dwSize.X * screenBufferInfo.dwSize.Y);

/*        COORD origin(0, 0);
        SHORT paramListTop = (SHORT)rawCommandBufTopRow - paramListRows - 1;
        SMALL_RECT clearRegion = { 0, 0, screenBufferInfo.dwSize.X - 1, paramListTop-1};
        WriteConsoleOutput(mhOutput, &blank[0], screenBufferInfo.dwSize, origin, &clearRegion);*/

        paramListBuf.PaintToWindowsConsole(mhOutput);
        rawCommandBuf.PaintToWindowsConsole(mhOutput);
    }

    void CommandLineEditor::SaveConsoleState()
    {
        if (!GetConsoleScreenBufferInfo(mhOutput, &screenBufferInfo))
        {
            cerr << "Failed to get console info." << endl;
            return;
        }

        originalConsoleBuf.resize(screenBufferInfo.dwSize.X * screenBufferInfo.dwSize.Y);
        SMALL_RECT readRegion = { 0, 0, screenBufferInfo.dwSize.X - 1, screenBufferInfo.dwSize.Y - 1 };
        ReadConsoleOutput(mhOutput, &originalConsoleBuf[0], screenBufferInfo.dwSize, { 0, 0 }, &readRegion);
    }

    void CommandLineEditor::RestoreConsoleState()
    {
        SMALL_RECT writeRegion = { 0, 0, screenBufferInfo.dwSize.X - 1, screenBufferInfo.dwSize.Y - 1 };
        WriteConsoleOutput(mhOutput, &originalConsoleBuf[0], screenBufferInfo.dwSize, { 0, 0 }, &writeRegion);
        SetConsoleCursorPosition(mhOutput, screenBufferInfo.dwCursorPosition);
    }


    size_t CommandLineEditor::CursorToTextIndex(COORD coord)
    {
        size_t i = (coord.Y-rawCommandBuf.mY) * screenBufferInfo.dwSize.X + coord.X;
        return std::min<size_t>(i, mText.size());
    }

    COORD CommandLineEditor::TextIndexToCursor(size_t i)
    {
        if (i > mText.length())
            i = mText.length();

        int w = screenBufferInfo.dwSize.X;

        int64_t rawCommandRows = (mText.size() + rawCommandBuf.mWidth - 1) / rawCommandBuf.mWidth;
        int64_t firstVisibleRow = 0;

        if (rawCommandRows > (int64_t)rawCommandBuf.mHeight)
            firstVisibleRow = rawCommandRows - (int64_t)rawCommandBuf.mHeight;    // could be negative as we're clipping into the raw command console

        int64_t hiddenChars = firstVisibleRow * w;

        i -= (size_t)hiddenChars;

        COORD c;
        c.X = (SHORT)(i) % w;
        c.Y = ((SHORT)rawCommandBuf.mY + (SHORT)(i) / w);
        return c;
    }

    void CommandLineEditor::HandlePaste(string text)
    {
        DeleteSelection();  // delete any selection if needed
        int curindex = (int)CursorToTextIndex(mCursorPos);
        mText.insert(curindex, text);
        curindex += (int)text.length();
        mCursorPos = TextIndexToCursor(curindex);

        static int count = 1;
        char buf[64];
        sprintf(buf, "paste:%d\n", count++);
        OutputDebugString(buf);
    }

    void CommandLineEditor::DeleteSelection()
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
        mCursorPos = TextIndexToCursor(curindex);

        ClearSelection();
    }

    void CommandLineEditor::ClearSelection()
    {
        selectionstart = -1;
        selectionend = -1;
    }

    void CommandLineEditor::UpdateSelection()
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
        if (mLastParsedText == mText)
            return;

        mParams = ParamsFromText(mText);
        mLastParsedText = mText;
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


    string CommandLineEditor::Edit(const string& sCommandLine)
    {
        mText  = sCommandLine;

        assert(mText == sCommandLine);  // should bew the same, right?

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


        SaveConsoleState();

        rawCommandBuf.Init(screenBufferInfo.dwSize.X, 4);
        rawCommandBuf.mY = screenBufferInfo.dwSize.Y - 4;

        paramListBuf.Init(screenBufferInfo.dwSize.X, screenBufferInfo.dwSize.Y-4);




        // Set console mode to allow reading mouse and key events
        DWORD mode;
        if (!GetConsoleMode(mhInput, &mode)) 
        {
            cerr << "Failed to get console mode." << endl;
            return mText;
        }
        mode |= ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT;
        mode &= ~ENABLE_PROCESSED_INPUT ;
        //mode |= ENABLE_PROCESSED_INPUT;
        if (!SetConsoleMode(mhInput, mode  ))
        {
            cerr << "Failed to set console mode." << endl;
            return mText;
        }


        mCursorPos = TextIndexToCursor(mText.size());

        // Display the text with cursor position
        //cout << text;

        // Main loop to read input events
        INPUT_RECORD inputRecord;
        DWORD numEventsRead;

        while (true)
        {
            UpdateParams();

            // Check for hotkey events
            MSG msg;
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_HOTKEY && msg.wParam == MY_HOTKEY_ID)
                {
                    HandlePaste(GetTextFromClipboard());
                }
                else
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
                UpdateDisplay();
                UpdateCursorPos();

            }

            if (PeekConsoleInput(mhInput, &inputRecord, 1, &numEventsRead) && numEventsRead > 0)
            {

                if (!ReadConsoleInput(mhInput, &inputRecord, 1, &numEventsRead))
                {
                    cerr << "Failed to read console input." << endl;
                    return "";
                }

                // Check if the input event is a keyboard event
                if (inputRecord.EventType == KEY_EVENT && inputRecord.Event.KeyEvent.bKeyDown)
                {
                    // Get the virtual key code from the key event
                    int vkCode = inputRecord.Event.KeyEvent.wVirtualKeyCode;
                    char c = inputRecord.Event.KeyEvent.uChar.AsciiChar;

                    bool bCTRLHeld = GetKeyState(VK_CONTROL) & 0x800;
                    bool bSHIFTHeld = GetKeyState(VK_SHIFT) & 0x800;

                    // Check for arrow keys
                    if (vkCode == VK_ESCAPE)
                    {
                        if (IsTextSelected())
                        {
                            ClearSelection();
                        }
                        else
                        {
                            mText = "";
                            break;
                        }
                    }
                    else if (vkCode == VK_RETURN)
                    {
                        break;
                    }
                    else if (vkCode == VK_HOME)
                    {
                        UpdateSelection();
                        mCursorPos = TextIndexToCursor(0);
                        UpdateSelection();
                    }
                    else if (vkCode == VK_END)
                    {
                        UpdateSelection();
                        mCursorPos = TextIndexToCursor(mText.size());
                        UpdateSelection();
                    }
                    else if (vkCode == VK_UP)
                    {
                        UpdateSelection();
                        if (mCursorPos.Y > rawCommandBuf.mY)
                            mCursorPos.Y--;
                        UpdateSelection();
                    }
                    else if (vkCode == VK_DOWN)
                    {
                        UpdateSelection();
                        int newindex = (int)CursorToTextIndex(mCursorPos) + screenBufferInfo.dwSize.X;
                        if (newindex < mText.size())
                            mCursorPos.Y++;
                        UpdateSelection();
                    }
                    else if (vkCode == VK_LEFT)
                    {
                        UpdateSelection();
                        // Move cursor left
                        int index = (int)CursorToTextIndex(mCursorPos);
                        if (index > 0)
                        {
                            if (bCTRLHeld)
                                FindNextBreak(-1);
                            else
                                mCursorPos = TextIndexToCursor(index - 1);
                        }
                        UpdateSelection();
                    }
                    else if (vkCode == VK_RIGHT)
                    {
                        UpdateSelection();
                        if (bCTRLHeld)
                        {
                            FindNextBreak(1);
                        }
                        else
                        {
                            // Move cursor right
                            int index = (int)CursorToTextIndex(mCursorPos);
                            if (index < mText.size())
                                mCursorPos = TextIndexToCursor(index + 1);

                        }
                        UpdateSelection();
                    }
                    else if (vkCode == VK_BACK)
                    {
                        if (IsTextSelected())
                        {
                            DeleteSelection();
                        }
                        else
                        {
                            // Delete character before cursor
                            int index = (int)CursorToTextIndex(mCursorPos);
                            if (index > 0)
                            {
                                mText.erase(index - 1, 1);
                                mCursorPos = TextIndexToCursor(index - 1);
                            }
                            UpdateSelection();
                        }
                    }
                    else if (vkCode == VK_DELETE)
                    {
                        if (IsTextSelected())
                        {
                            DeleteSelection();
                        }
                        else
                        {
                            // Delete character at cursor
                            int index = (int)CursorToTextIndex(mCursorPos);
                            if (index < (int64_t)(mText.size()))
                            {
                                mText.erase(index, 1);
                            }
                        }
                        UpdateSelection();
                    }
                    else if (vkCode == 0x41 && bCTRLHeld)
                    {
                        selectionstart = 0;
                        selectionend = mText.length();
                    }
                    else if (vkCode == 0x43 && bCTRLHeld) // CTRL-C
                    {
                        // handle copy
                        CopyTextToClipboard(GetSelectedText());
                    }
                    else if (c >= 32)
                    {
                        if (IsTextSelected())
                            DeleteSelection();

                        // ASCII key pressed (printable character)
                        char ch = inputRecord.Event.KeyEvent.uChar.AsciiChar;

                        // Insert character at cursor position
                        int index = (int)CursorToTextIndex(mCursorPos);
                        mText.insert(index, 1, ch);
                        mCursorPos = TextIndexToCursor(index + 1);
                        UpdateSelection();
                    }
                }

                UpdateDisplay();
                UpdateCursorPos();
            }
        }


        RestoreConsoleState();
        cout << "restoring\n";

        return mText;
    }
};
