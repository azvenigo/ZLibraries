#pragma once

#ifdef ENABLE_CLE

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

    // for processing entered parameters
    struct EnteredParams
    {
        int64_t positionalindex = -1;            // if not a named param, must have a position
        std::string sParamText;                 // raw parameter text
        ParamDesc* pRelatedDesc = nullptr;     // associated param descriptor if available

        WORD drawAttributes = FOREGROUND_WHITE;
        std::string sStatusMessage;

        int64_t rawCommandLineStartIndex = -1;

    };


    typedef std::vector<EnteredParams> tEnteredParams;


    class ConsoleWin
    {
    public:
        bool Init(int64_t l, int64_t t, int64_t r, int64_t b);

        void Clear(WORD attrib = 0);
        void Fill(int64_t l, int64_t t, int64_t r, int64_t b, WORD attrib);

        void DrawCharClipped(char c, int64_t x, int64_t y, WORD attrib = FOREGROUND_WHITE);
        void DrawCharClipped(char c, int64_t offset, WORD attrib = FOREGROUND_WHITE);

        void DrawClippedText(int64_t x, int64_t y, std::string text, WORD attributes = FOREGROUND_WHITE, bool bWrap = true);
        void DrawClippedAnsiText(int64_t x, int64_t y, std::string ansitext, bool bWrap = true);
        void DrawFixedColumnStrings(int64_t x, int64_t y, tStringArray& strings, std::vector<size_t>& colWidths, tAttribArray attribs);

        void GetTextOuputRect(std::string text, int64_t& w, int64_t& h);        

        //virtual void PaintToWindowsConsole(HANDLE hOut);
        virtual void Paint(tConsoleBuffer& backBuf);


        virtual void OnKey(int keycode, char c) {}

        virtual void SetArea(int64_t l, int64_t t, int64_t r, int64_t b);
        void GetArea(int64_t& l, int64_t& t, int64_t& r, int64_t& b);

        void ClearScreenBuffer();

        bool mbDone = false;
        bool mbCanceled = false;
        bool mbVisible = false;
        tConsoleBuffer  mBuffer;

    protected:
        SHORT mClearAttrib = 0;

        int64_t mWidth = 0;
        int64_t mHeight = 0;

        int64_t mX = 0;
        int64_t mY = 0;
    };

    class RawEntryWin : public ConsoleWin
    {
        friend class CommandLineEditor;
        struct undoEntry
        {
            undoEntry(const std::string& _text = "", int64_t _cursorindex = -1, int64_t _selectionstart = -1, int64_t _selectionend = -1) :
                text(_text), cursorindex(_cursorindex), selectionstart(_selectionstart), selectionend(_selectionend) {}

            std::string text;
            int64_t     cursorindex     = -1;
            int64_t     selectionstart  = -1;
            int64_t     selectionend    = -1;
        };
        typedef std::list<undoEntry> tUndoEntryList;

    public:
        void SetText(const std::string& text);

        void DrawClippedText(int64_t x, int64_t y, std::string text, WORD attributes = FOREGROUND_WHITE, bool bWrap = true, bool bHeightlightSelection = true);

        void Paint(tConsoleBuffer& backBuf);
        bool GetParameterUnderIndex(int64_t index, size_t& outStart, size_t& outEnd, std::string& outParam);
        bool HandleParamContext();

        std::string GetText() { return mText; }
//        COORD GetCursorPos() { return mCursorPos; }
//        int64_t GetCursorIndex() { return CursorToTextIndex(mCursorPos); }

        void FindNextBreak(int nDir);
        void UpdateCursorPos(COORD localPos);
        void UpdateFirstVisibleRow();


        bool IsIndexInSelection(int64_t i);
        bool IsTextSelected() { return selectionstart >= 0 && selectionend >= 0; }

        void SetArea(int64_t l, int64_t t, int64_t r, int64_t b);

        virtual void OnKey(int keycode, char c);

        void HandlePaste(std::string text);


        void UpdateSelection();
        void DeleteSelection();
        void ClearSelection();
        std::string GetSelectedText();

        void AddUndoEntry();
        void Undo();


        tEnteredParams mEnteredParams;
        tStringList mAvailableModes;
        tStringList mAvailableNamedParams;
        tUndoEntryList mUndoEntryList;
    protected:

        int64_t CursorToTextIndex(COORD coord);
        COORD TextIndexToCursor(int64_t i);
        COORD LocalCursorToGlobal(COORD cursor);


        std::string     mText;

        COORD mLocalCursorPos;
        int64_t firstVisibleRow = 0;
        int64_t selectionstart = -1;
        int64_t selectionend = -1;
    };



    // InfoWin - read only window (maybe add scrolling around?) that closes on esc
    class InfoWin : public ConsoleWin
    {
    public:
        void SetText(const std::string& text) { mText = text; }
        void Paint(tConsoleBuffer& backBuf);
        void OnKey(int keycode, char c);

        int64_t firstVisibleRow = 0;
    protected:
        std::string     mText;
    };


    class AnsiColorWin : public ConsoleWin
    {
    public:
        void SetText(const std::string& text) { mText = text; }
        void Paint(tConsoleBuffer& backBuf);
    protected:
        std::string     mText;
    };


    class ListboxWin : public ConsoleWin
    {
    public:
        ListboxWin() : mMinWidth(0), mSelection(-1), mAnchorL(-1), mAnchorB(-1) {}
        virtual std::string GetSelection();

        virtual void SetEntries(tStringList entries, std::string selectionSearch = "", int64_t anchor_l = -1, int64_t anchor_b = -1);
        virtual void Paint(tConsoleBuffer& backBuf);
        virtual void OnKey(int keycode, char c);

        std::string mTopCaption;
        std::string mBottomCaption;
        int64_t mMinWidth;
    protected:
        tStringList mEntries;
        int64_t     mSelection;
        int64_t     mAnchorL;
        int64_t     mAnchorB;
    };

    class HistoryWin : public ListboxWin
    {
    public:
        virtual void OnKey(int keycode, char c);
    };

    class FolderList : public ListboxWin
    {
    public:
        bool            Scan(std::string sPath, int64_t origin_l, int64_t origin_b);  // bottom left corner to auto size from
        std::string     FindClosestParentPath(std::string sPath);    // given some path with possibly non-existant elements, walk up the chain until finding an existing parent
        virtual void    Paint(tConsoleBuffer& backBuf);
        virtual void    OnKey(int keycode, char c);

        tStringList     mEntries;
    protected:
        std::string     mPath;
        int64_t         mSelection;
    };



    class CommandLineEditor
    {
    public:
        CommandLineEditor();

        std::string Edit(int argc, char* argv[]);
        std::string Edit(const std::string& sCommandLine);

        void SetConfiguredCLP(CommandLineParser* pCLP) { mpCLP = pCLP; }


    private:

        tEnteredParams  GetPositionalEntries();
        tEnteredParams  GetNamedEntries();

        std::string     GetMode();      // first positional entry

        std::string HistoryPath();
        bool LoadHistory();
        bool SaveHistory();
        bool AddToHistory(const std::string& sCommandLine);     // removes if previously seen and appends to end

        void UpdateFromConsoleSize();
        void UpdateDisplay();
        void DrawToScreen();
        void SaveConsoleState();
        void RestoreConsoleState();
        void ShowHelp();

        void UpdateParams();        // parse mText and break into parameter fields
        std::string     mLastParsedText;

//        std::string     msMode;       


        HANDLE mhInput;
        HANDLE mhOutput;
        tConsoleBuffer originalConsoleBuf;
        CONSOLE_SCREEN_BUFFER_INFO originalScreenInfo;


        tConsoleBuffer backBuffer;      // for double buffering

        tEnteredParams    mParams;
        std::string EnteredParamsToText();
        tEnteredParams ParamsFromText(const std::string& sText);

        bool ParseParam(const std::string sParamText, std::string& outName, std::string& outValue );



        // Processing of registered CLP
        CommandLineParser* mpCLP;
        tStringList GetCLPModes();
        tStringList GetCLPNamedParamsForMode(const std::string& sMode);
        CLP::ParamDesc* GetParamDesc(const std::string& sMode, std::string& paramName);
        CLP::ParamDesc* GetParamDesc(const std::string& sMode, int64_t position);
    };


};  // namespace CLP

#endif // ENABLE_CLE
