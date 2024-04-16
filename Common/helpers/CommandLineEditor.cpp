#include "CommandLineEditor.h"
#include "LoggingHelpers.h"
#include <Windows.h>
#include <iostream>
#include <assert.h>

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

    void CommandLineEditor::DrawText(size_t x, size_t y, std::string text, WORD attributes, bool bWrap)
    {
        SHORT screenw = screenBufferInfo.dwSize.X;

        COORD start((SHORT)x, (SHORT)y);
        size_t bufferindex = y * screenw + x;

        size_t remainingchars;
        if (bWrap)
            remainingchars = consoleBuf.size() - bufferindex;   // remaining chars in buffer
        else
            remainingchars = screenw - x;     // remaining chars on row

        for (size_t textindex = 0; textindex < text.size(); textindex++)
        {
            consoleBuf[bufferindex].Char.AsciiChar = text[textindex];
            consoleBuf[bufferindex].Attributes = attributes;
            bufferindex++;
        }
    }


    void CommandLineEditor::UpdateDisplay()
    {
        memset(&consoleBuf[0], 0, consoleBuf.size() * sizeof(CHAR_INFO));


        // top half will be list of params
        size_t rawCommandLineDisplay = consoleBuf.size() / 2;
        size_t paramList = rawCommandLineDisplay;
        

        string displayText(mText);
        if (displayText.length() > rawCommandLineDisplay)
            displayText = mText.substr(mText.length() - rawCommandLineDisplay);

        WORD col = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED;
        for (size_t i = 0; i < displayText.length(); i++)
        {
            consoleBuf[i].Char.AsciiChar = displayText[i];
            consoleBuf[i].Attributes = col;
            if (IsIndexInSelection(i))
                consoleBuf[i].Attributes |= BACKGROUND_BLUE;
        }

        for (size_t i = 0; i < displayText.length(); i++)
        {
            size_t matching = SH::FindMatching(displayText, i);
            if (matching != string::npos)
            {
                while (i <= matching)
                {
                    consoleBuf[i].Attributes |= BACKGROUND_INTENSITY;
                    i++;
                }
            }
        }


        size_t lastCommandLineRow = TextIndexToCursor(mText.length() - 1).Y;
        size_t lastRow = screenBufferInfo.dwSize.Y;

        size_t row = lastCommandLineRow + 1;
        for (int paramindex = 0; paramindex < mParams.size(); paramindex++)
        {
            DrawText(0, row, mParams[paramindex].sParamText, FOREGROUND_GREEN | FOREGROUND_BLUE, false);
           
            if (row++ > lastRow)
                break;
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


    void CommandLineEditor::ClearScreenBuffer()
    {
        consoleBuf.resize(screenBufferInfo.dwSize.X * screenBufferInfo.dwSize.Y);
        for (int i = 0; i < screenBufferInfo.dwSize.X * screenBufferInfo.dwSize.Y; ++i)
        {
            consoleBuf[i].Char.UnicodeChar = L' ';
            consoleBuf[i].Attributes = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED;
        }
    }

    void CommandLineEditor::DrawToScreen()
    {
        COORD bufferCoord = { 0, 0 };
        SMALL_RECT writeRegion = { 0, 0, screenBufferInfo.dwSize.X - 1, screenBufferInfo.dwSize.Y - 1 };
        WriteConsoleOutput(mhOutput, &consoleBuf[0], screenBufferInfo.dwSize, bufferCoord, &writeRegion);
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
        size_t i = coord.Y * screenBufferInfo.dwSize.X + coord.X;
        return std::min<size_t>(i, mText.size());
    }

    COORD CommandLineEditor::TextIndexToCursor(size_t i)
    {
        if (i > mText.length())
            i = mText.length();

        int w = screenBufferInfo.dwSize.X;

        COORD c;
        c.X = (SHORT)(i) % w;
        c.Y = (SHORT)(i) / w;
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

    std::string CommandLineEditor::ParamNameFromText(std::string sParamText)
    {
        // named parameters always start with -
        if (!sParamText.empty() && sParamText[0] == '-')
        {
            size_t nIndexOfColon = sParamText.find(':');
            if (nIndexOfColon != string::npos)
            {
                return sParamText.substr(1, nIndexOfColon - 1).c_str();    // everything from first char to colon
            }
            else
            {
                // flag with no value is the same as -flag:true
                return sParamText.substr(1, nIndexOfColon).c_str();
            }
        }

        return "";
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

        int positionalindex = 0;
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
                if (match != string::npos) // if enclosure, skip to end
                    endofparam = match;
                else
                    endofparam++;
            }

            EnteredParams param;
            param.sParamText = sText.substr(i, endofparam - i);
            string sParamName = ParamNameFromText(param.sParamText);
            if (sParamName.empty())
            {
                param.positionalindex = positionalindex;
                param.pRelatedDesc = GetParamDesc(positionalindex);
                positionalindex++;
            }
            else
            {
                param.pRelatedDesc = GetParamDesc(sParamName);
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
        ClearScreenBuffer();


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
                        if (mCursorPos.Y > 0)
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
