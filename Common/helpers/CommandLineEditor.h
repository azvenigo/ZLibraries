#pragma once

#ifdef ENABLE_CLE

#include <string>
#include "CommandLineParser.h"
#include "CommandLineCommon.h"
#include "StringHelpers.h"
#include <Windows.h>
#include <list>
#include <assert.h>

namespace CLP
{
    // for processing entered parameters
    struct EnteredParams
    {
        int64_t positionalindex = -1;            // if not a named param, must have a position
        std::string sParamText;                 // raw parameter text
        ParamDesc* pRelatedDesc = nullptr;     // associated param descriptor if available

        ZAttrib drawAttributes = WHITE;
        std::string sStatusMessage;

        int64_t rawCommandLineStartIndex = -1;

    };
    typedef std::vector<EnteredParams> tEnteredParams;


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

        void DrawClippedText(int64_t x, int64_t y, std::string text, ZAttrib attributes = WHITE_ON_BLACK, bool bWrap = true, bool bHighlightSelection = true, Rect* pClip = nullptr);

        void Paint(tConsoleBuffer& backBuf);
        bool GetParameterUnderIndex(int64_t index, size_t& outStart, size_t& outEnd, std::string& outParam, ParamDesc** ppPD = nullptr);
        bool HandleParamContext();

        std::string GetText() { return mText; }
//        COORD GetCursorPos() { return mCursorPos; }
//        int64_t GetCursorIndex() { return CursorToTextIndex(mCursorPos); }

        void FindNextBreak(int nDir);
        void UpdateCursorPos(COORD localPos);
        void UpdateFirstVisibleRow();


        bool IsIndexInSelection(int64_t i);
        bool IsTextSelected() { return selectionstart >= 0 && selectionend >= 0; }

        void SetArea(const Rect& r);

        virtual void OnKey(int keycode, char c);

        void HandlePaste(std::string text);


        void UpdateSelection();
        void DeleteSelection();
        void ClearSelection();
        std::string GetSelectedText();

        void AddUndoEntry();
        void Undo();


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



    class ParamListWin : public AnsiColorWin
    {
    public:
        void Paint(tConsoleBuffer& backBuf);
    };

    class UsageWin : public AnsiColorWin
    {
    public:
        void Paint(tConsoleBuffer& backBuf);
        std::string sHighlightParam;
        ZAttrib highlightAttrib;
    };

    class HistoryWin : public ListboxWin
    {
    public:
        HistoryWin();
        virtual void OnKey(int keycode, char c);
    };

    class FolderList : public ListboxWin
    {
    public:
        FolderList();
        bool            Scan(std::string sPath, int64_t origin_l, int64_t origin_b);  // bottom left corner to auto size from
        std::string     FindClosestParentPath(std::string sPath);    // given some path with possibly non-existant elements, walk up the chain until finding an existing parent
        virtual void    OnKey(int keycode, char c);

        void            UpdateCaptions();
        tStringList     mEntries;
        bool            IsRootFolder(std::string sPath);
    protected:
        std::string     mPath;
        int64_t         mSelection;
    };

    class CommandLineEditor
    {
    public:
        friend class ParamListWin;
        CommandLineEditor();

        std::string Edit(int argc, char* argv[]);
        std::string Edit(const std::string& sCommandLine);

        void SetConfiguredCLP(CommandLineParser* pCLP);


    private:

        tEnteredParams  GetPositionalEntries();
        tEnteredParams  GetNamedEntries();

        std::string     GetMode();      // first positional entry

        std::string HistoryPath();
        bool LoadHistory();
        bool SaveHistory();
        bool AddToHistory(const std::string& sCommandLine);     // removes if previously seen and appends to end

        void UpdateFromConsoleSize(bool bForce = false);
        void UpdateDisplay();
        void DrawToScreen();
        void SaveConsoleState();
        void RestoreConsoleState();
        void ShowHelp();
        bool OutputCommandToConsole(const std::string& command);


        void UpdateParams();        // parse mText and break into parameter fields
        std::string     mLastParsedText;

//        std::string     msMode;       


        HANDLE mhInput;
        HANDLE mhOutput;
        std::vector<CHAR_INFO> originalConsoleBuf;
        CONSOLE_SCREEN_BUFFER_INFO originalScreenInfo;


        tConsoleBuffer backBuffer;      // for double buffering
        tConsoleBuffer drawStateBuffer; // for rendering only delta

//        tEnteredParams    mParams;
        std::string EnteredParamsToText();
        tEnteredParams ParamsFromText(const std::string& sText);

        bool ParseParam(const std::string sParamText, std::string& outName, std::string& outValue );



        // Processing of registered CLP
        tStringList GetCLPModes();
        tStringList GetCLPNamedParamsForMode(const std::string& sMode);
        void UpdateUsageWin();
        CLP::ParamDesc* GetParamDesc(const std::string& sMode, std::string& paramName);
        CLP::ParamDesc* GetParamDesc(const std::string& sMode, int64_t position);
    };


};  // namespace CLP

#endif // ENABLE_CLE
