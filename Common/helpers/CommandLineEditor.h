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


    class RawEntryWin : public TextEditWin
    {
    public:
        RawEntryWin() 
        {
            bMultiline = true;
        }
        bool OnKey(int keycode, char c);
        virtual bool OnMouse(MOUSE_EVENT_RECORD event);
        void Paint(tConsoleBuffer& backBuf);
        friend class CommandLineEditor;
        bool GetParameterUnderIndex(int64_t index, size_t& outStart, size_t& outEnd, std::string& outParam, ParamDesc** ppPD = nullptr);
        bool HandleParamContext();
    protected:
        virtual COORD TextIndexToCursor(int64_t i);
        virtual int64_t CursorToTextIndex(COORD coord);

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
        virtual bool OnKey(int keycode, char c);
    };

    class FolderList : public ListboxWin
    {
    public:
        FolderList();
        bool            Scan(std::string sPath, int64_t origin_l, int64_t origin_b);  // bottom left corner to auto size from
        std::string     FindClosestParentPath(std::string sPath);    // given some path with possibly non-existant elements, walk up the chain until finding an existing parent
        virtual bool    OnKey(int keycode, char c);

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

        eResponse Edit(int argc, char* argv[], std::string& outEditedCommandLine);
        eResponse Edit(const std::string& sCommandLine, std::string& outEditedCommandLine);

        void SetConfiguredCLP(CommandLineParser* pCLP);


    private:
        bool OnKey(int keycode, char c);
        bool OnMouse(MOUSE_EVENT_RECORD event);


        tEnteredParams  GetPositionalEntries();
        tEnteredParams  GetNamedEntries();

        std::string     GetMode();      // first positional entry

        bool UpdateFromConsoleSize(bool bForce = false);
        void DrawToScreen();
        void ShowHelp();

        bool OutputCommandToConsole(const std::string& command);


        void UpdateParams();        // parse mText and break into parameter fields
        std::string     mLastParsedText;

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
