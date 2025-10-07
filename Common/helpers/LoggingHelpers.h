#pragma once
#include <stdint.h>
#include <string>
#include <cstring>
#include <sstream>
#include <assert.h>
#include <iostream>
#include <ostream>
#include <inttypes.h>
#include <list>
#include <deque>
#include <vector>
#include <map>
#include <optional>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <mutex>

#include "StringHelpers.h"

#define ENABLE_ANSI_OUT
#ifdef ENABLE_ANSI_OUT

#define COL_CUSTOM_STYLE "[CUSTOMSTYLE]"
#define COL_RESET   "\033[0m"
#define COL_BLACK   "\033[30m"
#define COL_BLACK   "\033[30m"
#define COL_RED     "\033[31m"
#define COL_GREEN   "\033[32m"
#define COL_BLUE    "\033[34m"
#define COL_YELLOW  "\033[33m"
#define COL_PURPLE  "\033[35m"
#define COL_CYAN    "\033[36m"
#define COL_WHITE   "\033[37m"
#define COL_ORANGE  "\033[38;2;255;165;0m"
#define COL_GRAY    "\033[38;2;128;128;128m"

#define COL_BG_BLACK   "\033[40m"
#define COL_BG_RED     "\033[41m"
#define COL_BG_GREEN   "\033[42m"
#define COL_BG_BLUE    "\033[44m"
#define COL_BG_YELLOW  "\033[43m"
#define COL_BG_PURPLE  "\033[45m"
#define COL_BG_CYAN    "\033[46m"
#define COL_BG_WHITE   "\033[47m"
#define COL_BG_ORANGE  "\033[48;2;255;165;0m"
#define COL_BG_GRAY    "\033[48;2;128;128;128m"

#define DEC_LINE_START  "\033(0"
#define DEC_LINE_END    "\033(B"

#else

#define COL_CUSTOM_STYLE    ""
#define COL_RESET           ""
#define COL_BLACK           ""
#define COL_RED             ""
#define COL_GREEN           ""
#define COL_BLUE            ""
#define COL_YELLOW          ""
#define COL_PURPLE          ""
#define COL_CYAN            ""
#define COL_WHITE           ""
#define COL_ORANGE          ""
#define COL_GRAY            ""


#define COL_BG_BLACK        ""
#define COL_BG_RED          ""
#define COL_BG_GREEN        ""
#define COL_BG_BLUE         ""
#define COL_BG_YELLOW       ""
#define COL_BG_PURPLE       ""
#define COL_BG_CYAN         ""
#define COL_BG_WHITE        ""
#define COL_BG_ORANGE       ""
#define COL_BG_GRAY         ""

#define DEC_LINE_START      ""
#define DEC_LINE_END        ""

#endif

bool validateAnsiSequences(const std::string& input);

namespace LOG
{
    // Logging verbosity levels
    // 0 - Silent
    // 1 - Default. Errors, Warnings, Results
    // 2 - Basic diagnostics
    // 3 - Full diagnostics

#define LVL_SILENT 0
#define LVL_DEFAULT 1
#define LVL_DIAG_BASIC 2
#define LVL_DIAG_FULL 3
    extern int64_t gnVerbosityLevel;

#define OUT_ALL(statement)      { if (LOG::gnVerbosityLevel > LVL_SILENT) statement;}
#define OUT_DEFAULT(statement)  { if (LOG::gnVerbosityLevel >= LVL_DEFAULT) statement; }
#define OUT_DIAG(statement)     { if (LOG::gnVerbosityLevel >= LVL_DIAG_BASIC) {statement;} }
#define OUT_DIAG_FULL(statement){ if (LOG::gnVerbosityLevel >= LVL_DIAG_FULL) {statement;} }
#define OUT_ERR(statement)      { statement; cerr << std::flush; assert(false); }


#define OUT_HEX(statement)          std::hex << statement << std::dec 

    struct LogEntry
    {
        LogEntry(const std::string& _text, uint64_t _time = -1);

        uint64_t        time;
        uint64_t        counter;
        std::thread::id threadID;
        std::string     text;
    };

    typedef std::deque<LogEntry> tLogEntries;





    const size_t kQueueSize = 16*1024;
    class Logger
    {
    public:
        Logger() : logTotalCounter(0)
        {
            gLogStartTime = std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1);
        }

        void addEntry(std::string text);

        size_t getEntryCount() const
        {
            if (logFilter.empty())
                return logEntries.size();

            return logFilteredEntries.size();
        }

/*        const std::deque<LogEntry>& getEntries() const
        {
            if (logFilter.empty())
                return logEntries;

            return logFilteredEntries;
        }*/

        void setFilter(const std::string& sFilter); // if empty string, clears the filter

        bool getEntries(uint64_t startingLogCounter, size_t count, std::deque<LogEntry>& outEntries) const;
        bool tail(size_t count, std::deque<LogEntry>& outEntries) const;


        void clear()
        {
            std::lock_guard<std::mutex> lock(logEntriesMutex);
            logEntries.clear();
            logFilteredEntries.clear();
        }

        friend std::ostream& operator<<(std::ostream& os, const Logger& logger)
        {
            std::lock_guard<std::mutex> lock(logger.logEntriesMutex);

            for (const auto& entry : logger.logEntries)
            {
                os << "[" << entry.time << "][Thread:" << entry.threadID << "] " << entry.text << std::endl;
            }
            return os;
        }

        bool        gOutputToFallback = true;
        std::mutex  gFallbackMutex;
        int64_t     gLogStartTime;

    private:
        tLogEntries logEntries;
        mutable std::mutex logEntriesMutex;
        size_t logTotalCounter;    // total count of flushes, or entries submitted. For tracking changes even if we max out the logEntries queue

        // 
        tLogEntries logFilteredEntries;
        std::string logFilter;
    };

    extern Logger gLogger;


    class LogStreamBuf : public std::streambuf
    {
    public:
        LogStreamBuf()
        {
            setp(m_buffer, m_buffer + BUFFER_SIZE - 1);
        }
    protected:

        virtual int sync() override
        {
            if (pbase() != pptr())
            {
                char* pWalker = pbase();
                char* pStart = pWalker;
                while (pWalker < pptr())
                {
                    if (*pWalker == '\n')
                    {
                        std::string content(pStart, pWalker - pStart);
                        if (!content.empty() && isCompleteAnsiContent(content))
                        {
                            // Send it to the logger
                            assert(content[0] != 0);
                            gLogger.addEntry(content);  // add to global logger
                            pWalker++;  // skip newline
                            pStart = pWalker;
                        }
                    }
                    pWalker++;
                }

                if (pStart < pptr())   // move over any remaining chars
                {
                    size_t remaining = pptr() - pStart;
                    memcpy(m_buffer, pStart, remaining);
                    setp(m_buffer + remaining, m_buffer + BUFFER_SIZE - 1 - remaining);
                }
                else
                {
                    setp(m_buffer, m_buffer + BUFFER_SIZE - 1);
                }
            }
            return 0;
        }


        virtual int overflow(int c) override
        {
            if (c != EOF)
            {
                // Add the character to the buffer first
                *pptr() = static_cast<char>(c);
                pbump(1);

                // If buffer is full or it's a newline, consider syncing
                if (pptr() >= epptr() || c == '\n')
                {
                    sync();
                }
            }
            return c;
        }

    private:
        // Determine if content has complete ANSI sequences
        bool isCompleteAnsiContent(const std::string& content);

        static const int BUFFER_SIZE = 1024; // Adjust size as needed
        char m_buffer[BUFFER_SIZE];
    };

    class LogStream : public std::ostream
    {
    public:
        LogStream(std::ostream& fallback) :
            std::ostream(&m_streamBuf),
            m_fallback(fallback) {}

        ~LogStream()
        {
            flush();
        }

        // Remove virtual - std::ostream::flush() is not virtual
        std::ostream& flush()
        {
            std::ostream::flush(); // This calls sync() on the streambuf
            if (gLogger.gOutputToFallback)
            {
                std::lock_guard<std::mutex> lock(gLogger.gFallbackMutex);
                m_fallback.flush();
            }
            return *this;
        }

        template<typename T>
        LogStream& operator<<(const T& data)
        {
            // Just write to our stream - let LogStreamBuf handle buffering and newline detection
            static_cast<std::ostream&>(*this) << data;

            // Handle fallback output
            if (gLogger.gOutputToFallback)
            {
                std::lock_guard<std::mutex> lock(gLogger.gFallbackMutex);
                m_fallback << data;
            }
            return *this;
        }

        // Handle the non-const reference version
        template<typename T>
        LogStream& operator<<(T& data)
        {
            return operator<<(static_cast<const T&>(data));
        }

        LogStream& operator<<(std::ostream& (*manip)(std::ostream&))
        {
            // Apply to our stream - this will trigger sync() for endl/flush
            manip(static_cast<std::ostream&>(*this));

            if (gLogger.gOutputToFallback)
            {
                std::lock_guard<std::mutex> lock(gLogger.gFallbackMutex);
                manip(m_fallback);
            }

            return *this;
        }

    private:
        LogStreamBuf m_streamBuf;
        std::ostream& m_fallback;
    };

    void usToDateTime(int64_t us, std::string& date, std::string& time, int64_t precision = 2);
    std::string usToElapsed(int64_t us, int64_t precision = 2);

    extern thread_local LogStream gLogOut;
    extern thread_local LogStream gLogErr;
};


#ifdef ENABLE_CLM
#define zout LOG::gLogOut
#define zerr LOG::gLogErr
#else
#define zout std::cout
#define zerr std::cerr
#endif



#define RATE_LIMITED_PROGRESS(cadence_seconds, completed, total, unit_per_second, message)	static uint64_t report_ts_=(std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1)); static uint64_t start_=(std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1));\
{ uint64_t cur_time = (std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1));\
  if ( cur_time - report_ts_ > (cadence_seconds * 1000000))\
  {\
     uint64_t elapsed_us = cur_time - start_;\
     double fCompletions_per_second = (double) completed * 1000000.0 / elapsed_us;\
     double fLeftToDo = (double) (total - completed);\
     double fETA = fLeftToDo / fCompletions_per_second;\
     zout << message \
     << " - Completed:" << completed << "/" << total << " (" << (int)((double) 100.0 * completed / (double) total) << "%) " \
     << " - Rate:" << (completed/(elapsed_us / 1000000)) << unit_per_second \
     << " - Elapsed:" << LOG::usToElapsed(elapsed_us) \
     << " -  ETA:" << (int)fETA << "s                          \r" << std::flush;\
     report_ts_ = cur_time;\
  }\
 }

inline std::string HexValueOut(uint32_t nVal, bool bIncludeDecimal = true)
{
    char buf[64];
    if (bIncludeDecimal)
        sprintf(buf, "0x%" PRIx32 " (%" PRIu32 ")", nVal, nVal);
    else
        sprintf(buf, "0x%" PRIx32, nVal);
    return std::string(buf);
}

inline std::string HexValueOut(uint64_t nVal, bool bIncludeDecimal = true)
{
    char buf[64];
    if (bIncludeDecimal)
        sprintf(buf, "0x%016" PRIX64 " (%" PRIu64 ")", nVal, nVal);
    else
        sprintf(buf, "0x%016" PRIX64, nVal);
    return std::string(buf);
}

inline void DumpMemoryToCout(uint8_t* pBuf, uint32_t nBytes, uint64_t nBaseMemoryOffset = 0, uint32_t nColumns = 32)
{
    char byteToAscii[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

    uint32_t nBytesOnLine = 0;
    uint8_t* pWalker = pBuf;
    std::string sAscii;
    while (pWalker < pBuf + nBytes)
    {
        if (nBytesOnLine % nColumns == 0)
        {
            // if we've reached a line end and we've accumulated ascii text
            if (!sAscii.empty())
                zout << " |" << sAscii << "|";

            nBytesOnLine = 0;
            zout << "\n" << HexValueOut((uint64_t)(nBaseMemoryOffset + (pWalker - pBuf)), false) << ": ";
            sAscii.clear();
        }
        else if (nBytesOnLine % 4 == 0)  // extra space every 4 bytes
        {
            zout << " ";
        }

        uint8_t c = *pWalker;
        zout << byteToAscii[c >> 4] << byteToAscii[c & 0x0F] << " ";

        // Accumulate visible ascii characters or just substitute a space
        if (c > ' ' && c != 127)
            sAscii += c;
        else
            sAscii += '.';

        nBytesOnLine++;
        pWalker++;
    }

    // remainder of text
    if (!sAscii.empty())
    {
        // add spaces for any missing chars
        while (nBytesOnLine < nColumns)
        {
            sAscii += ' ';
            zout << "   ";
            if (nBytesOnLine % 4 == 0 && nBytesOnLine < nColumns)
                zout << " ";
            nBytesOnLine++;
        }
        zout << " |" << sAscii << "|\n";
    }
}


inline size_t VisLength(const std::string& s)
{
    size_t length = 0;
    size_t pos = 0;
    while (pos < s.size() && s[pos] != '\0')
    {
        if (s[pos] == '\x1b' && pos + 1 < s.size())
        {
            char c = s[pos + 1];
            if (c == '[') // CSI sequence
            {
                pos += 2; // Skip ESC and '['

                // Skip until we find a final character in the range 0x40-0x7E (@-~)
                while (pos < s.size() && !(s[pos] >= 0x40 && s[pos] <= 0x7E))
                {
                    pos++;
                }

                // Skip the final command character if we found one
                if (pos < s.size())
                    pos++;
            }
            else if (c == '(' || c == ')' || c == '*' || c == '+') // Charset designator
            {
                // Sequence form: ESC ( F, ESC ) F, ESC * F, ESC + F
                if (pos + 2 < s.size())
                    pos += 3; // skip ESC, selector, and final
                else
                    pos = s.size(); // truncated sequence at end
            }
            else
            {
                // Not a recognized escape, treat ESC as visible
                length++;
                pos++;
            }
        }
        else
        {
            length++;
            pos++;
        }
    }
    return length;
}

inline bool ContainsAnsiSequences(const std::string& s)
{
    size_t pos = 0;

    while (pos < s.size())
    {
        if (s[pos] == '\x1b')
        {
            if (pos + 1 < s.size())
            {
                char c = s[pos + 1];
                if (c == '[') // CSI
                    return true;
                if (c == '(' || c == ')' || c == '*' || c == '+') // Charset designators
                    return true;
            }
        }
        pos++;
    }

    return false;
}

inline std::string StripAnsiSequences(const std::string& s)
{
    std::string result;
    size_t pos = 0;

    while (pos < s.size())
    {
        if (s[pos] == '\x1b' && pos + 1 < s.size())
        {
            char c = s[pos + 1];
            if (c == '[') // CSI sequence
            {
                pos += 2; // Skip '\x1b['

                // Skip all characters valid inside the ANSI sequence
                while (pos < s.size() && (isdigit(s[pos]) || s[pos] == ';' ||
                    s[pos] == '?' || s[pos] == ':' ||
                    s[pos] == '.' || s[pos] == '='))
                {
                    pos++;
                }

                if (pos < s.size())
                    pos++; // Skip the final letter
            }
            else if (c == '(' || c == ')' || c == '*' || c == '+') // Charset designator
            {
                // Expect ESC + selector + final
                if (pos + 2 < s.size())
                    pos += 3;
                else
                    pos = s.size(); // incomplete at end
            }
            else
            {
                // Not recognized, keep the ESC as literal
                result += s[pos];
                pos++;
            }
        }
        else
        {
            result += s[pos];
            pos++;
        }
    }

    return result;
}

inline std::string AnsiCol(uint8_t r, uint8_t g, uint8_t b, uint8_t bg_r, uint8_t bg_g, uint8_t bg_b)
{
    std::string s;
    s += "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
    s += "\033[48;2;" + std::to_string(bg_r) + ";" + std::to_string(bg_g) + ";" + std::to_string(bg_b) + "m"; //background color
    return s;
};

inline std::string AnsiCol(uint8_t r, uint8_t g, uint8_t b)
{
    std::string s;
    s += "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
    return s;
};

inline std::string AnsiCol(uint32_t col)
{
    return AnsiCol((col & 0x00ff0000) >> 16, (col & 0x0000ff00) >> 8, (col & 0x000000ff));
}

inline std::string AnsiCol(uint64_t col)
{
    return AnsiCol(
        (uint8_t)((col & 0x0000000000ff0000) >> 16), 
        (uint8_t)((col & 0x000000000000ff00) >>  8), 
        (uint8_t)((col & 0x00000000000000ff)      ),
        (uint8_t)((col & 0x00ff000000000000) >> 48), 
        (uint8_t)((col & 0x0000ff0000000000) >> 40), 
        (uint8_t)((col & 0x000000ff00000000) >> 32) );
}

std::tuple<std::string, std::string, std::string>
SplitAnsiWrapped(const std::string& input);

typedef std::vector<std::string> tStringArray;


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Table - Prints a table with columns automatically sizes. Rows can have any number of elements
// usage:
// Table table;
// table.SetBorders("L", COL_RED "T", "R", COL_RED "B", "|");
// table.defaultStyle = Table::Style(COL_GREEN, Table::RIGHT, Table::TIGHT, 1);
// 
// Table::Style sectionStyle;
// sectionStyle.alignment = Table::CENTER;
// sectionStyle.color = COL_BG_CYAN COL_YELLOW;

// Table::Style rightStyle;
// rightStyle.alignment = Table::RIGHT;
// rightStyle.padding = 4;
//
// table.AddRow(sectionStyle, "This is a section");
// table.AddRow("a name", "another cell");
// table.AddRow(rightStyle, 1.345, 0, 543, "wow");
// 
// table.renderWidth = 80;
// zout << table;

class Table
{
public:
    Table();

    enum eSide : uint8_t
    {
        LEFT = 0,
        TOP = 1,
        RIGHT = 2,
        BOTTOM = 3,
        CENTER = 4
    };

/*    enum eSpacing : uint8_t
    {
        TIGHT = 0,      // minimum width for content
        EVEN = 1,      // average width for available width
        MAX = 2       // maximum width available
    };*/

    enum eWrapping : uint8_t
    {
        NO_WRAP = 0,        // single line only
        CHAR_WRAP = 1,      // break up string by char
        WORD_WRAP = 2       // only wrap on whitespace boundaries
    };

    struct Style
    {
        Style(std::string _color = COL_RESET, eSide _alignment = LEFT, float _space_allocation = 0.0, eWrapping _wrapping = CHAR_WRAP, uint8_t _padding = 1, char _padchar = ' ') : color(_color), alignment(_alignment), space_allocation(_space_allocation), wrapping(_wrapping), padding(_padding), padchar(_padchar)
        {
            assert(alignment >= LEFT && alignment <= CENTER);
            assert(wrapping >= NO_WRAP && wrapping <= WORD_WRAP);
        }

        friend std::ostream& operator <<(std::ostream& os, Style& style);

        std::string color = COL_RESET;
        uint8_t     alignment = LEFT;
//        uint8_t     spacing = TIGHT;
        float       space_allocation = 0.0;
        uint8_t     wrapping = CHAR_WRAP;
        uint8_t     padding = 1;
        char        padchar = ' ';
    };

    // Some helpful defaults
    static Style kLeftAlignedStyle;
    static Style kRightAlignedStyle;
    static Style kCenteredStyle;
    static Style kDefaultStyle;


    typedef std::optional<Style>        tOptionalStyle;
    typedef std::vector<tOptionalStyle> tOptionalStyleArray;

    struct Cell
    {
        Cell(const std::string& _s = "", tOptionalStyle _style = std::nullopt);

        tStringArray GetLines(size_t width) const;
        size_t RowCount(size_t width) const;

        size_t MinWidth() const;

        bool ExtractStyle(const std::string& s, std::string& ansi, std::string& rest);  // if a string starts with an ansi sequence split it out


        std::string     s;
        tOptionalStyle  style;
    };
    typedef std::vector<Cell> tCellArray;


    std::string borders[5] =
    {
        "*",  // LEFT
        "*",  // TOP
        "*",  // RIGHT
        "*",  // BOTTOM
        " "   // COLUMN SEPARATOR
    };

    typedef std::map<size_t, tOptionalStyleArray> tColCountToStyles;
    typedef std::map<size_t, std::vector<size_t>> tColCountToColWidth;
    typedef std::map<size_t, tOptionalStyle> tRowToStyleMap;

    void SetBorders(const std::string& _left, const std::string& _top, const std::string& _right, const std::string& _bottom, const std::string& _center = " ");

    // Style is prioritized as follows
    // 1) cell style
    // 2) row style
    // 3) col style
    // 4) default style

    bool SetColStyle(size_t col_count, size_t col_num, const Style& style);
    bool SetColStyles(tOptionalStyleArray styles);  // helper for easy setting styles
    bool SetRowStyle(size_t row, const Style& style);
    bool SetCellStyle(size_t col, size_t row, const Style& style);

    tOptionalStyle GetColStyle(size_t col_count, size_t col_num);
    Style GetStyle(size_t col, size_t row);



    // Accessors
    Cell GetCell(size_t col, size_t row);
    size_t GetRowCount() const;
    size_t GetTableMinWidth();    // minimum width for all cells to be fully visible
    size_t GetTableMinWidthForColCount(size_t col_count);   // minimum width for rendering all rows with this column count


    // Table manipulation
    void Clear();

    template <typename T, typename...Types>
    void AddRow(T arg, Types...more)
    {
        tCellArray columns;
        ToCellList(columns, arg, more...);

        mRows.push_back(columns);
    }

    template <typename T, typename...Types>
    void AddRow(const Style& style, T arg, Types...more)
    {
        tCellArray columns;
        ToCellList(columns, style, arg, more...);

        mRows.push_back(columns);
    }


    void AddRow(tStringArray& columns);
    void AddRow(tCellArray& columns);

    void AddMultilineRow(std::string& sMultiLine);



    // output functions
    operator std::string();
    friend std::ostream& operator <<(std::ostream& os, Table& table);
    //std::string ToString(size_t width = ~0);    


    Table Transpose();

    // Variadic template function to find the minimum table width
    template <typename... Tables>
    size_t GetMinWidthForTables(size_t minW, Tables&... tables)
    {
        return std::max<size_t>({ minW, tables.GetTableMinWidth()... });
    }

    template <typename... Tables>
    void AlignWidth(size_t minW, Tables&... tables)
    {
/*        size_t nMinTableWidth = GetMinWidthForTables(minW, tables...);
        ((tables.renderWidth = nMinTableWidth), ...);
        renderWidth = nMinTableWidth;*/
        renderWidth = minW;
        ((tables.renderWidth = minW), ...);
        assert(renderWidth > 0 && renderWidth < 4 * 1024);
        bLayoutNeedsUpdating = true;
    }

    void SetRenderWidth(size_t w);
    bool SetColWidth(size_t col_count, size_t col_num, size_t width);   
    bool AutosizeColumns();
    tColCountToColWidth GetMinColWidths();


//    static Style           defaultStyle;

protected:
    std::string StyledOut(const std::string& s, size_t width, tOptionalStyle style = std::nullopt);

    void DrawRow(size_t row_num, std::ostream& os);

    size_t CellWidth(size_t row, size_t col);
    bool bLayoutNeedsUpdating = true;


    template <typename S, typename...SMore>
    inline void ToCellList(tCellArray& columns, S arg, SMore...moreargs)
    {
        std::stringstream ss;
        ss << arg;
        columns.push_back(Cell(ss.str()));
        ToCellList(columns, moreargs...);
    }

    template <typename S, typename...SMore>
    inline void ToCellList(tCellArray& columns, const Style& style, S arg, SMore...moreargs)
    {
        std::stringstream ss;
        ss << arg;
        columns.push_back(Cell(ss.str(), style));
        ToCellList(columns, style, moreargs...);
    }


    template <typename...SMore>
    inline void ToCellList(tCellArray& columns, const Cell& cell, SMore...moreargs)
    {
        columns.push_back(cell);
        ToCellList(columns, moreargs...);
    }


    inline void ToCellList(tCellArray&) {}   // needed for the variadic with no args
    inline void ToCellList(tCellArray&, const Style&) {}   // needed for the variadic with no args

    std::vector<tCellArray> mRows;

    tColCountToStyles   colCountToColStyles; // an array of column styles for each column count
    tRowToStyleMap      rowStyles;
    tColCountToColWidth colCountToColWidths;

    // Formatting
    size_t              renderWidth = 0; // if not set, table will output to minimum width for all cells to be visible and padded per style
};
