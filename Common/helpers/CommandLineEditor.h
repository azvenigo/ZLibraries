#pragma once
#include <string>
#include "CommandLineParser.h"
#include "StringHelpers.h"
#include <Windows.h>
#include <list>

typedef std::list<std::string> tStringList;
typedef std::vector<WORD> tAttribArray;
typedef std::vector<CHAR_INFO> tConsoleBuffer;

#define FOREGROUND_WHITE  (FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED)

namespace CLP
{
    class ConsoleBuffer
    {
    public:
        bool Init(size_t w, size_t h);

        void Clear(WORD attrib = 0);

        void DrawCharClipped(char c, size_t x, size_t y, WORD attrib = FOREGROUND_WHITE);
        void DrawCharClipped(char c, int64_t offset, WORD attrib = FOREGROUND_WHITE);

        void DrawClippedText(size_t x, size_t y, std::string text, WORD attributes = FOREGROUND_WHITE, bool bWrap = true);
        void DrawFixedColumnStrings(size_t x, size_t y, tStringArray& strings, std::vector<size_t>& colWidths, tAttribArray attribs);

        void PaintToWindowsConsole(HANDLE hOut);


        tConsoleBuffer mBuffer;
        size_t mWidth = 0;
        size_t mHeight = 0;

        size_t mX = 0;
        size_t mY = 0;
    };


    class CommandLineEditor
    {
    public:
        CommandLineEditor();

        std::string Edit(int argc, char* argv[]);
        std::string Edit(const std::string& sCommandLine);

        void SetConfiguredCLP(CommandLineParser* pCLP) { mpCLP = pCLP; }


    private:

        // for processing entered parameters
        struct EnteredParams
        {
            int64_t positionalindex = -1;            // if not a named param, must have a position
            std::string sParamText;                 // raw parameter text
            ParamDesc*  pRelatedDesc = nullptr;     // associated param descriptor if available

            WORD drawAttributes = FOREGROUND_WHITE;
            std::string sStatusMessage;
        };
        typedef std::vector<CommandLineEditor::EnteredParams> tEnteredParams;

        tEnteredParams GetPositionalEntries();
        tEnteredParams GetNamedEntries();


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
        tConsoleBuffer originalConsoleBuf;


        ConsoleBuffer   rawCommandBuf;
//        size_t          rawCommandBufTopRow;

        ConsoleBuffer   paramListBuf;
//        size_t          paramListBufTopRow;



        tEnteredParams    mParams;
        std::string EnteredParamsToText();
        tEnteredParams ParamsFromText(const std::string& sText);

        bool ParseParam(const std::string sParamText, std::string& outName, std::string& outValue );



        // Processing of registered CLP
        bool GetParameterUnderCursor(size_t& outStart, size_t& outEnd, std::string& outParam);
        CommandLineParser* mpCLP;
        tStringList GetCLPModes();
        CLP::ParamDesc* GetParamDesc(std::string& paramName);
        CLP::ParamDesc* GetParamDesc(int64_t position);
    };


};  // namespace CLP
