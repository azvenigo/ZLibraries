#pragma once

#include <string>
#include "LoggingHelpers.h"
#include "StringHelpers.h"
#include <list>
#include <assert.h>
#include <algorithm>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <cctype>
#include <optional>


#ifdef _WIN64
#include <Windows.h>
#endif


const uint64_t WHITE_ON_BLACK   = 0xFF000000FFffffff;
const uint32_t TRANS            = 0x00000000;
const uint32_t BLACK            = 0xFF000000;
const uint32_t WHITE            = 0xFFFFFFFF;
const uint32_t RED              = 0xFFFF0000;
const uint32_t GREEN            = 0xFF00FF00;
const uint32_t BLUE             = 0xFF0000FF;
const uint32_t YELLOW           = 0xFFFFFF00;
const uint32_t CYAN             = 0xFF00FFFF;
const uint32_t MAGENTA          = 0xFFFF00FF;
const uint32_t SILVER           = 0xFFC0C0C0;
const uint32_t GRAY             = 0xFF808080;
const uint32_t MAROON           = 0xFF800000;
const uint32_t OLIVE            = 0xFF808000;
const uint32_t PURPLE           = 0xFF800080;
const uint32_t TEAL             = 0xFF008080;
const uint32_t NAVY             = 0xFF000080;
const uint32_t ORANGE           = 0xFFFFA500;
const uint32_t PINK             = 0xFFFFC0CB;
const uint32_t LIME             = 0xFF00FF00;
const uint32_t BROWN            = 0xFFA52A2A;
const uint32_t GOLD             = 0xFFFFD700;
const uint32_t INDIGO           = 0xFF4B0082;
const uint32_t VIOLET           = 0xFFEE82EE;
const uint32_t LIGHTGRAY        = 0xFFD3D3D3;
const uint32_t DARKGRAY         = 0xFFA9A9A9;
const uint32_t LIGHTBLUE        = 0xFFADD8E6;
const uint32_t DARKBLUE         = 0xFF00008B;
const uint32_t LIGHTGREEN       = 0xFF90EE90;
const uint32_t DARKGREEN        = 0xFF006400;
const uint32_t LIGHTCYAN        = 0xFFE0FFFF;
const uint32_t DARKCYAN         = 0xFF008B8B;
const uint32_t LIGHTMAGENTA     = 0xFFFF77FF;
const uint32_t DARKMAGENTA      = 0xFF8B008B;

const uint64_t WHITE_ON_GRAY = ((uint64_t)WHITE << 32) | GRAY;

#define MAKE_BG(x) ((uint64_t)x << 32)

#define GET_A(x) ((x &  0x00000000ff000000) >> 24)
#define GET_R(x) ((x &  0x0000000000ff0000) >> 16)
#define GET_G(x) ((x &  0x000000000000ff00) >> 8)
#define GET_B(x) (x &   0x00000000000000ff)
#define GET_BA(x) ((x & 0xff00000000000000) >> 56)
#define GET_BR(x) ((x & 0x00ff000000000000) >> 48)
#define GET_BG(x) ((x & 0x0000ff0000000000) >> 40)
#define GET_BB(x) ((x & 0x000000ff00000000) >> 32)



#ifdef ENABLE_ANSI_OUT
// DEC_LINE characters 
#define SCROLLBAR_TRACK_TOP '\x77'
#define SCROLLBAR_TRACK_CENTER '\x78'
#define SCROLLBAR_TRACK_BOTTOM '\x76'
#define SCROLLBAR_THUMB '\xb1'
#else
#define SCROLLBAR_TRACK_TOP '|'
#define SCROLLBAR_TRACK_CENTER '|'
#define SCROLLBAR_TRACK_BOTTOM '|'
#define SCROLLBAR_THUMB '#'
#endif

typedef std::list<std::string> tStringList;

namespace CLP
{
    const int CTRL_V_HOTKEY = 1;
    const int SHIFT_INSERT_HOTKEY = 2;
    const int CTRL_S_HOTKEY = 3;

    // decorations
    static Table::Style ResetStyle = Table::Style();
    static Table::Style AppStyle = Table::Style(COL_YELLOW);
    static Table::Style SectionStyle = Table::Style(COL_CYAN, Table::CENTER, 0.0f, Table::CHAR_WRAP, 0, '=');
    static Table::Style SubSectionStyle = Table::Style(COL_CYAN);
    static Table::Style ParamStyle = Table::Style(COL_YELLOW);
    static Table::Style WarningStyle = Table::Style(COL_ORANGE);
    static Table::Style ErrorStyle = Table::Style(COL_RED);


    enum eResponse : uint32_t
    {
        kSuccess = 0,
        kCanceled = 1,
        kShowHelp = 2,
        kShowCommandLineHelp = 3,
        kErrorShowEdit = 4,
        kErrorAbort = 5,
#ifdef ENABLE_COMMAND_HISTORY
        kShowHistory = 6
#endif
    };

    class ZAttrib
    {
    public:
        ZAttrib() : ZAttrib(WHITE_ON_BLACK)
        {
        }

        ZAttrib(uint64_t col)
        {
            Set(col);
        }

        ZAttrib(const std::string& sAnsi)
        {
            FromAnsi((uint8_t*)sAnsi.c_str());
        }

        operator uint64_t() const
        {
            return ((uint64_t)ba << 56) | ((uint64_t)br << 48) | ((uint64_t)bg << 40) | ((uint64_t)bb << 32) | ((uint64_t)a << 24) | ((uint64_t)r << 16) | ((uint64_t)g << 8) | b;
        }

        operator std::string() const
        {
            return ToAnsi();
        }


        bool operator ==(const ZAttrib& rhs) const
        {
            return (uint64_t)*this == (uint64_t)rhs && dec_line == rhs.dec_line;
        }

        bool operator !=(const ZAttrib& rhs) const
        {
            return (uint64_t)*this != (uint64_t)rhs || dec_line != rhs.dec_line;
        }

        ZAttrib& operator |=(uint64_t rhs)
        {
            a |= GET_A(rhs);
            r |= GET_R(rhs);
            g |= GET_G(rhs);
            b |= GET_B(rhs);

            ba |= GET_BA(rhs);
            br |= GET_BR(rhs);
            bg |= GET_BG(rhs);
            bb |= GET_BB(rhs);

            return *this;
        }

        void operator =(const ZAttrib& rhs)
        {
            Set((uint64_t)rhs, rhs.dec_line);
        }


        inline void Set(uint64_t col, bool _dec_line = false)
        {
            ba = GET_BA(col);
            br = GET_BR(col);
            bg = GET_BG(col);
            bb = GET_BB(col);

            a = GET_A(col);
            r = GET_R(col);
            g = GET_G(col);
            b = GET_B(col);

            dec_line = _dec_line;
        }

        inline void SetFG(uint32_t col)
        {
            a = (col & 0xff000000) >> 24;
            r = (col & 0x00ff0000) >> 16;
            g = (col & 0x0000ff00) >> 8;
            b = (col & 0x000000ff);
        }

        inline void SetBG(uint32_t col)
        {
            ba = (col & 0xff000000) >> 24;
            br = (col & 0x00ff0000) >> 16;
            bg = (col & 0x0000ff00) >> 8;
            bb = (col & 0x000000ff);
        }

        inline void SetFG(uint8_t _a, uint8_t _r, uint8_t _g, uint8_t _b)
        {
            a = _a;
            r = _r;
            g = _g;
            b = _b;
        }

        inline void SetBG(uint8_t _a, uint8_t _r, uint8_t _g, uint8_t _b)
        {
            ba = _a;
            br = _r;
            bg = _g;
            bb = _b;
        }

        size_t FromAnsi(const uint8_t* pChars);    // returns number of chars consumed (or 0 if not a valid ansi sequence)
        std::string ToAnsi() const;

        uint32_t FG() const { return (uint32_t)a << 24 | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b; }
        uint32_t BG() const { return (uint32_t)ba << 24 | (uint32_t)br << 16 | (uint32_t)bg << 8 | (uint32_t)bb; }


        uint64_t a : 8;
        uint64_t r : 8;
        uint64_t g : 8;
        uint64_t b : 8;

        uint64_t ba : 8;
        uint64_t br : 8; // background
        uint64_t bg : 8; // background
        uint64_t bb : 8; // background

        bool dec_line = false;
    };

    int16_t ScreenW();
    int16_t ScreenH();
    void InitScreenInfo();

    bool ConsoleHasFocus();

#ifdef ENABLE_CLE

    struct ZChar
    {
        bool operator ==(const ZChar& rhs) const { return c == rhs.c && attrib == rhs.attrib; }
        bool operator !=(const ZChar& rhs) const { return c != rhs.c || attrib != rhs.attrib; }
        void operator =(const ZChar& rhs) { c = rhs.c; attrib = rhs.attrib; }

        uint8_t c;
        ZAttrib attrib;
    };

    typedef std::list<std::string> tStringList;
    typedef std::vector<ZAttrib> tAttribArray;
    typedef std::vector<ZChar> tConsoleBuffer;

    // Styles
    extern ZAttrib kAttribAppName;
    extern ZAttrib kAttribCaption;
    extern ZAttrib kAttribSection;
    extern ZAttrib kAttribFolderListBG;
    extern ZAttrib kAttribListBoxBG;
    extern ZAttrib kAttribListBoxEntry;
    extern ZAttrib kAttribListBoxSelectedEntry;
    extern ZAttrib kAttribParamListBG;
    extern ZAttrib kAttribTopInfoBG;
    extern ZAttrib kAttribHelpBG;
    extern ZAttrib kRawText;
    extern ZAttrib kSelectedText;
    extern ZAttrib kGoodParam;
    extern ZAttrib kBadParam;
    extern ZAttrib kUnknownParam;
    extern ZAttrib kAttribError;
    extern ZAttrib kAttribWarning;
    extern ZAttrib kAttribScrollbarBG;
    extern ZAttrib kAttribScrollbarThumb;

    struct Rect
    {
        Rect(int64_t _l = 0, int64_t _t = 0, int64_t _r = 0, int64_t _b = 0)
        {
            l = _l;
            t = _t;
            r = _r;
            b = _b;
        };

        int64_t w() const { return r - l; }
        int64_t h() const { return b - t; }
        void offset(int64_t dx, int64_t dy)
        {
            l += dx;
            r += dx;
            t += dy;
            b += dy;
        }

        int64_t l;
        int64_t t;
        int64_t r;
        int64_t b;
    };

    typedef std::vector<ZAttrib> tAttribArray;
    typedef std::vector<ZChar> tConsoleBuffer;


    class ConsoleWin
    {
    public:
        enum Side : uint8_t
        {
            L    = 0,
            T    = 1,
            R    = 2, 
            B    = 3,

            MAX_SIDES = 4
        };

        enum Position : uint8_t
        {
            LT = 0,
            CT = 1,
            RT = 2,

            LB = 3,
            CB = 4,
            RB = 5,

            MAX_POSITIONS = 6
        };


        virtual bool Init(const Rect& r);
        void SetEnableFrame(bool _l = 1, bool _t = 1, bool _r = 1, bool _b = 1);
        void Clear(ZAttrib attrib = 0, bool bGradient = false);


        void Fill(uint8_t c, const Rect& r, ZAttrib attrib);
        void Fill(const Rect& r, ZAttrib attrib, bool bGradient = false);
        void Fill(ZAttrib attrib, bool bGradient = false);

        void DrawCharClipped(uint8_t c, int64_t x, int64_t y, ZAttrib attrib = {}, const Rect* pClip = nullptr);
        void DrawCharClipped(uint8_t c, int64_t offset, ZAttrib attrib = {});

        void DrawClippedText(const Rect& r, std::string text, ZAttrib attributes = WHITE_ON_BLACK, bool bWrap = true, const Rect* pClip = nullptr);
        void DrawClippedAnsiText(const Rect& r, std::string ansitext, bool bWrap = true, const Rect* pClip = nullptr);
        int64_t DrawFixedColumnStrings(int64_t x, int64_t y, tStringArray& strings, std::vector<size_t>& colWidths, int64_t padding, tAttribArray attribs, const Rect* pClip = nullptr); // returns rows drawn

        Rect GetTextOuputRect(std::string text);
        int64_t GetTextOutputRows(const std::string& text, int64_t w); // returns required number of rows for a given width

        void GetCaptionPosition(std::string& caption, Position pos, int64_t& x, int64_t& y);

        virtual void BasePaint();
        virtual void RenderToBackBuf(tConsoleBuffer& backBuf);

        virtual bool OnKey(int keycode, char c) { return false; }
        virtual bool OnMouse(MOUSE_EVENT_RECORD event);

        virtual void SetVisible(bool bVisible = true) { mbVisible = bVisible; }
        virtual void SetArea(const Rect& r);
        bool IsOver(int64_t x, int64_t y) { return mbVisible && x >= mX && x <= mX + mWidth && y >= mY && y <= mY + mHeight; }
        void GetArea(Rect& r);
        void GetInnerArea(Rect& r);  // adjusted for frame



        void ClearScreenBuffer();

        bool mbDone = false;
        bool mbCanceled = false;
        bool mbVisible = false;
        tConsoleBuffer  mBuffer;

        void ClearCaptions();

        std::string positionCaption[Position::MAX_POSITIONS];
        bool enableFrame[Side::MAX_SIDES] = { 0 };

    protected:
        void DrawScrollbar(const Rect& r, int64_t min, int64_t max, int64_t cur, ZAttrib bg, ZAttrib thumb);

        ZAttrib mClearAttrib = 0;
        bool mbGradient = false;

        int64_t mWidth = 0;
        int64_t mHeight = 0;

        int64_t mX = 0;
        int64_t mY = 0;
    };




    // InfoWin - read only window that closes on esc
    class InfoWin : public ConsoleWin
    {
    public:
        virtual bool Init(const Rect& r);
        void Paint(tConsoleBuffer& backBuf);
        bool OnKey(int keycode, char c);
        virtual bool OnMouse(MOUSE_EVENT_RECORD event);

        virtual void UpdateCaptions();

        bool bAutoScrollbar = true;
        int64_t mTopVisibleRow = 0;

    protected:
        std::string mText;
    };

    class TableWin : public InfoWin
    {
    public:
        void Paint(tConsoleBuffer& backBuf);
        virtual void SetVisible(bool bVisible = true);
        Table mTable;

    protected:
        int64_t mRenderedW = 0;
        int64_t mRenderedH = 0;
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
        ListboxWin() : mMinWidth(0), mSelection(-1), mTopVisibleRow(0), mAnchorL(-1), mAnchorB(-1) {}
        virtual std::string GetSelection();

        virtual void SetEntries(tStringList entries, std::string selectionSearch = "", int64_t anchor_l = -1, int64_t anchor_b = -1);
        virtual void Paint(tConsoleBuffer& backBuf);
        virtual bool OnKey(int keycode, char c);
        virtual bool OnMouse(MOUSE_EVENT_RECORD event);

        virtual void SizeWindowToEntries();
        virtual void UpdateCaptions();

        int64_t     mMinWidth;
        int64_t     mMinHeight;

    protected:
        tStringList mEntries;
        int64_t     mSelection;
        int64_t     mTopVisibleRow;
        int64_t     mAnchorL;
        int64_t     mAnchorB;
    };

    class TextEditWin : public ConsoleWin
    {
        struct undoEntry
        {
            undoEntry(const std::string& _text = "", int64_t _cursorindex = -1, int64_t _selectionstart = -1, int64_t _selectionend = -1) :
                text(_text), cursorindex(_cursorindex), selectionstart(_selectionstart), selectionend(_selectionend) {}

            std::string text;
            int64_t     cursorindex = -1;
            int64_t     selectionstart = -1;
            int64_t     selectionend = -1;
        };
        typedef std::list<undoEntry> tUndoEntryList;

    public:
        TextEditWin() : bMultiline(false) {}
        ~TextEditWin();

        void SetText(const std::string& text);

        virtual void SetVisible(bool bVisible = true);

        void DrawClippedText(int64_t x, int64_t y, std::string text, ZAttrib attributes = WHITE_ON_BLACK, bool bWrap = true, bool bHighlightSelection = true, Rect* pClip = nullptr);

        void Paint(tConsoleBuffer& backBuf);

        std::string GetText() { return mText; }

        void FindNextBreak(int nDir);
        void UpdateCursorPos(COORD localPos);
        void UpdateFirstVisibleRow();


        bool IsIndexInSelection(int64_t i);
        bool IsTextSelected() { return selectionstart >= 0 && selectionend >= 0 && selectionstart != selectionend; }

        void SetArea(const Rect& r);

        virtual bool OnKey(int keycode, char c);
        virtual bool OnMouse(MOUSE_EVENT_RECORD event);


        void HandlePaste(std::string text);


        void UpdateSelection();
        void DeleteSelection();
        void ClearSelection();
        std::string GetSelectedText();

        void AddUndoEntry();
        void Undo();
        void HookHotkeys();
        void UnhookHotkeys();

        tUndoEntryList mUndoEntryList;

        bool    bMultiline;
    protected:

        virtual int64_t CursorToTextIndex(COORD coord);
        virtual COORD TextIndexToCursor(int64_t i);
        virtual COORD LocalCursorToGlobal(COORD cursor);


        std::string     mText;

        COORD mLocalCursorPos;
        int64_t firstVisibleRow = 0;
        int64_t selectionstart = -1;
        int64_t selectionend = -1;
        bool bMouseCapturing = false;

#ifdef _WIN64
        bool    bHotkeyHooked = false;
#endif


    };

    std::string GetTextFromClipboard();
    bool CopyTextToClipboard(const std::string& text);

    void SetCursorPosition(COORD coord, bool bForce = false);

    void SaveConsoleState();
    void RestoreConsoleState();
    void DrawAnsiChar(int64_t x, int64_t y, uint8_t c, ZAttrib ca);

    std::string ExpandEnvVars(const std::string& s); // replaces any %var% with environment variables 
    typedef std::pair<std::string, std::string> tKeyVal;
    typedef std::list<tKeyVal> tKeyValList;
    tKeyValList GetEnvVars();
    void ShowEnvVars();
    void ShowLaunchParams();

    extern TableWin helpTableWin;

    extern CONSOLE_SCREEN_BUFFER_INFO screenInfo;
    extern bool bScreenInvalid;
    extern HANDLE mhInput;
    extern HANDLE mhOutput;
    extern COORD gLastCursorPos;
#endif // ENABLE_CLE

    extern std::string appPath;
    extern std::string appName;

#ifdef ENABLE_COMMAND_HISTORY
    const size_t    kCommandHistoryLimit = 25; 

    std::string HistoryPath();
    bool LoadHistory();
    bool SaveHistory();
    bool AddToHistory(const std::string& sCommandLine);     // removes if previously seen and appends to end

    extern tStringList commandHistory;
#endif
};  // namespace CLP

