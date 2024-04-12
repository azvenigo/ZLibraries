#pragma once
#include <string>
#include "StringHelpers.h"
#include <Windows.h>

namespace CLP
{
    class CommandLineEditor
    {
    public:
        CommandLineEditor();

        std::string Edit(int argc, char* argv[]);



    private:
        void FindNextBreak(int nDir);
        void UpdateCursorPos();
        void UpdateDisplay();
        void ClearScreenBuffer();
        void DrawToScreen();
        void SaveConsoleState();
        void RestoreConsoleState();
        void UpdateSelection();
        void DeleteSelection();
        void ClearSelection();
        void HandlePaste(std::string text);
        bool CopyTextToClipboard(const std::string& text);
        std::string GetTextFromClipboard();
        int64_t selectionstart = -1;
        int64_t selectionend = -1;

        bool IsIndexInSelection(int64_t i);
        bool IsTextSelected() { return selectionstart >= 0 && selectionend >= 0; }
        std::string GetSelectedText();

        size_t CursorToTextIndex(COORD coord);
        COORD TextIndexToCursor(size_t i);


        COORD mCursorPos;

        std::string     mText;

        std::string mDisplayedtext;
        int64_t displayedCursorPos;
        int64_t displayedSelectionStart = -1;
        int64_t displayedSelectionEnd = -1;

        HANDLE mhInput;
        HANDLE mhOutput;
        CONSOLE_SCREEN_BUFFER_INFO screenBufferInfo;
        std::vector<CHAR_INFO> originalConsoleBuf;
        std::vector<CHAR_INFO> consoleBuf;
    };

};  // namespace CLP
