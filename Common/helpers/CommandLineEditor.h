#pragma once
#include <string>
#include "CommandLineParser.h"
#include "StringHelpers.h"
#include <Windows.h>
#include <list>

typedef std::list<std::string> tStringList;


namespace CLP
{
    class CommandLineEditor
    {
    public:
        CommandLineEditor();

        std::string Edit(const std::string& sCommandLine);

        void SetConfiguredCLP(CommandLineParser* pCLP) { mpCLP = pCLP; }


    private:

        // for processing entered parameters
        struct EnteredParams
        {
            size_t positionalindex; // if not a named parameter
            std::string sParamText;
            ParamDesc*  pRelatedDesc;
        };
        typedef std::vector<CommandLineEditor::EnteredParams> tEnteredParams;


        void FindNextBreak(int nDir);
        void UpdateCursorPos();
        void UpdateDisplay();
        void DrawText(size_t x, size_t y, std::string text, WORD attributes = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED, bool bWrap = true);
        void ClearScreenBuffer();
        void DrawToScreen();
        void SaveConsoleState();
        void RestoreConsoleState();
        void UpdateSelection();
        void DeleteSelection();
        void ClearSelection();
        bool HandleParamContext();
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

        void UpdateParams();        // parse mText and break into parameter fields
        std::string     mLastParsedText;


        COORD mCursorPos;

        std::string     mText;

        HANDLE mhInput;
        HANDLE mhOutput;
        CONSOLE_SCREEN_BUFFER_INFO screenBufferInfo;
        std::vector<CHAR_INFO> originalConsoleBuf;
        std::vector<CHAR_INFO> consoleBuf;


        tEnteredParams    mParams;
        std::string EnteredParamsToText();
        tEnteredParams ParamsFromText(const std::string& sText);

        std::string ParamNameFromText(std::string sParamText);



        // Processing of registered CLP
        bool GetParameterUnderCursor(size_t& outStart, size_t& outEnd, std::string& outParam);
        CommandLineParser* mpCLP;
        tStringList GetCLPModes();
        CLP::ParamDesc* GetParamDesc(std::string& paramName);
        CLP::ParamDesc* GetParamDesc(int64_t position);
    };

};  // namespace CLP
