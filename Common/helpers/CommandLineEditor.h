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
    class ConsoleWin
    {
    public:
        bool Init(int64_t l, int64_t t, int64_t r, int64_t b, std::string sText);

        void Clear(WORD attrib = 0);

        void DrawCharClipped(char c, int64_t x, int64_t y, WORD attrib = FOREGROUND_WHITE);
        void DrawCharClipped(char c, int64_t offset, WORD attrib = FOREGROUND_WHITE);

        void DrawClippedText(int64_t x, int64_t y, std::string text, WORD attributes = FOREGROUND_WHITE, bool bWrap = true);
        void DrawClippedAnsiText(int64_t x, int64_t y, std::string ansitext, bool bWrap = true);
        void DrawFixedColumnStrings(int64_t x, int64_t y, tStringArray& strings, std::vector<size_t>& colWidths, tAttribArray attribs);

        void GetTextOuputRect(std::string text, int64_t& w, int64_t& h);        

        virtual void PaintToWindowsConsole(HANDLE hOut);

        virtual void OnKey(int keycode, char c);

        void SetArea(int64_t l, int64_t t, int64_t r, int64_t b);
        void GetArea(int64_t& l, int64_t& t, int64_t& r, int64_t& b);

        std::string GetText() { return mText; }
        COORD GetCursorPos() { return mCursorPos; }
        int64_t GetCursorIndex() { return CursorToTextIndex(mCursorPos); }

        void FindNextBreak(int nDir);
        void UpdateCursorPos(COORD newPos);

        void HandlePaste(std::string text);
        void ClearScreenBuffer();
        void UpdateSelection();
        void DeleteSelection();
        void ClearSelection();
        bool IsIndexInSelection(int64_t i);
        bool IsTextSelected() { return selectionstart >= 0 && selectionend >= 0; }
        std::string GetSelectedText();



        bool mbDone = false;
        bool mbVisible = false;
        tConsoleBuffer  mBuffer;

    protected:
        int64_t CursorToTextIndex(COORD coord);
        COORD TextIndexToCursor(int64_t i);


        std::string     mText;

        COORD mCursorPos;

        int64_t selectionstart = -1;
        int64_t selectionend = -1;

        int64_t mWidth = 0;
        int64_t mHeight = 0;

        int64_t mX = 0;
        int64_t mY = 0;
    };


    // InfoWin - read only window (maybe add scrolling around?) that closes on esc
    class InfoWin : public ConsoleWin
    {
    public:
        void PaintToWindowsConsole(HANDLE hOut);
        void OnKey(int keycode, char c);

        int64_t firstVisibleRow = 0;
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


        void UpdateFromConsoleSize();
        void UpdateDisplay();
        void DrawToScreen();
        void SaveConsoleState();
        void RestoreConsoleState();
        bool HandleParamContext();
        void ShowHelp();

        void UpdateParams();        // parse mText and break into parameter fields
        std::string     mLastParsedText;

        std::string     msMode;       


        HANDLE mhInput;
        HANDLE mhOutput;
        CONSOLE_SCREEN_BUFFER_INFO mScreenInfo;
        tConsoleBuffer originalConsoleBuf;


        ConsoleWin      rawCommandBuf;
        ConsoleWin      paramListBuf;
        InfoWin         helpBuf;
        InfoWin         popupBuf;

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
