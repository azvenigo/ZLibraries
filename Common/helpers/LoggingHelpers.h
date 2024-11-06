#pragma once
#include <stdint.h>
#include <string>
#include <cstring>
#include <sstream>
#include <assert.h>
#include <iostream>
#include <inttypes.h>
#include <list>
#include <vector>
#include <map>
#include <optional>
#include <iomanip>
#include <algorithm>

#ifndef __PROSPERO__
#define ENABLE_ANSI_OUT
#endif
#ifdef ENABLE_ANSI_OUT

#define COL_RESET   "\033[00m"
#define COL_BLACK   "\033[30m"
#define COL_RED     "\033[31m"
#define COL_GREEN   "\033[32m"
#define COL_BLUE    "\033[34m"
#define COL_YELLOW  "\033[33m"
#define COL_PURPLE  "\033[35m"
#define COL_CYAN    "\033[36m"
#define COL_WHITE   "\033[37m"


#define COL_BG_BLACK   "\033[40m"
#define COL_BG_RED     "\033[41m"
#define COL_BG_GREEN   "\033[42m"
#define COL_BG_BLUE    "\033[44m"
#define COL_BG_YELLOW  "\033[43m"
#define COL_BG_PURPLE  "\033[45m"
#define COL_BG_CYAN    "\033[46m"
#define COL_BG_WHITE   "\033[47m"

#else

#define COL_RESET           ""
#define COL_BLACK           ""
#define COL_RED             ""
#define COL_GREEN           ""
#define COL_BLUE            ""
#define COL_YELLOW          ""
#define COL_PURPLE          ""
#define COL_CYAN            ""
#define COL_WHITE           ""


#define COL_BG_BLACK        ""
#define COL_BG_RED          ""
#define COL_BG_GREEN        ""
#define COL_BG_BLUE         ""
#define COL_BG_YELLOW       ""
#define COL_BG_PURPLE       ""
#define COL_BG_CYAN         ""
#define COL_BG_WHITE        ""

#endif

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
};

#define RATE_LIMITED_PROGRESS(cadence_seconds, completed, total, unit_per_second, message)	static uint64_t report_ts_=(std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1)); static uint64_t start_=(std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1));\
{ uint64_t cur_time = (std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1));\
  if ( cur_time - report_ts_ > (cadence_seconds * 1000000))\
  {\
     uint64_t elapsed_us = cur_time - start_;\
     double fCompletions_per_second = (double) completed * 1000000.0 / elapsed_us;\
     double fLeftToDo = (double) (total - completed);\
     double fETA = fLeftToDo / fCompletions_per_second;\
     cout << message << " - Elapsed:" << (elapsed_us) / 1000000 << "s. Completed:" << completed << "/" << total << " (" << (int)((double) 100.0 * completed / (double) total) << "%) " \
     "  Rate:" << (completed/(elapsed_us / 1000000)) << unit_per_second \
     "  ETA:" << (int)fETA << "s                          \r";\
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
                std::cout << " |" << sAscii << "|";

            nBytesOnLine = 0;
            std::cout << "\n" << HexValueOut((uint64_t)(nBaseMemoryOffset + (pWalker - pBuf)), false) << ": ";
            sAscii.clear();
        }
        else if (nBytesOnLine % 4 == 0)  // extra space every 4 bytes
        {
            std::cout << " ";
        }

        uint8_t c = *pWalker;
        std::cout << byteToAscii[c >> 4] << byteToAscii[c & 0x0F] << " ";

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
            std::cout << "   ";
            if (nBytesOnLine % 4 == 0 && nBytesOnLine < nColumns)
                std::cout << " ";
            nBytesOnLine++;
        }
        std::cout << " |" << sAscii << "|\n";
    }
}

typedef std::vector<std::string> tStringArray;


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TableOutput - Prints a table with columns automatically sizes. Rows can have any number of elements
// usage:
// TableOutput t(',');
// t.AddRow(1, 2.0, "three");
// t.AddRow('4', 5, 6.0, 7.0, 8, "nine", "ten");
// cout << t;
class Table
{
public:
    Table();

    // Formatting
    size_t renderWidth = 0; // table output will be to this width (ideally at least GetTableMinWidth()  )

    enum eSide : uint8_t
    {
        LEFT = 0,
        TOP = 1,
        RIGHT = 2,
        BOTTOM = 3,
        CENTER = 4
    };

    enum eSpacing : uint8_t
    {
        TIGHT = 0,      // minimum width for content
        EVEN  = 1,      // average width for available width
        MAX   = 2       // maximum width available
    };

    struct Style
    {
        Style(std::string _color = COL_RESET, uint8_t _alignment = LEFT, uint8_t _spacing = TIGHT, uint8_t _padding = 1) : color(_color), alignment(_alignment), spacing(_spacing), padding(_padding) {}

        std::string color       = COL_RESET;
        uint8_t     alignment   = LEFT;
        uint8_t     spacing     = TIGHT;
        uint8_t     padding     = 1;
    };

    typedef std::optional<Style> tOptionalStyle;

    struct Cell
    {
        Cell(const std::string& _s = "", const Style& _style = {}) : s(_s), style(_style) {}

        std::string StyledOut(size_t width);    // output aligned, padded, colored as needed into provided width

        size_t Width() const;

        std::string     s;
        tOptionalStyle  style;
    };
    typedef std::vector<Cell> tCellArray;


    std::string borders[5] =
    {
        COL_RESET "*",  // LEFT
        COL_RESET "*",  // TOP
        COL_RESET "*",  // RIGHT
        COL_RESET "*"   // BOTTOM
        COL_RESET " "   // COLUMN SEPARATOR
    };

    typedef std::map<size_t, std::vector<tOptionalStyle>> tColCountToStyles;
    typedef std::map<size_t, std::vector<size_t>> tColCountToColWidth;
    typedef std::map<size_t, tOptionalStyle> tRowToStyleMap;

    void SetBorders(const std::string& _left, const std::string& _top, const std::string& _right, const std::string& _bottom, const std::string& _center = " ");

    // Style is prioritized as follows
    // 1) cell style
    // 2) row style
    // 3) col style
    // 4) default style

    bool SetColStyle(size_t col_count, size_t col_num, const Style& style); 
    bool SetRowStyle(size_t row, const Style& style);
    bool SetCellStyle(size_t col, size_t row, const Style& style);
    bool SetDefaultStyle(const Style& style);

    Style GetStyle(size_t col, size_t row);

    // Accessors
    Cell GetCell(size_t col, size_t row);
    size_t GetRowCount() const;
    size_t GetTableMinWidth();    // minimum width for all cells to be fully visible


    // Table manipulation
    void Clear();

    template <typename T, typename...Types>
    void AddRow(T arg, Types...more)
    {
        tCellArray columns;
        ToCellList(columns, arg, more...);

        mRows.push_back(columns);
    }    
    
    void AddRow(tStringArray& columns);
    void AddRow(tCellArray& columns);

    void AddMultilineRow(std::string& sMultiLine);




    operator std::string();

    friend std::ostream& operator <<(std::ostream& os, Table& table);

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
        size_t nMinTableWidth = GetMinWidthForTables(minW, tables...);
        ((tables.renderWidth = nMinTableWidth), ...);
    }



protected:
    size_t CellWidth(size_t row, size_t col);
    void ComputeColumns();
    bool bLayoutNeedsUpdating = true;


    template <typename S, typename...SMore>
    inline void ToCellList(tCellArray& columns, S arg, SMore...moreargs)
    {
        std::stringstream ss;
        ss << arg;
        columns.push_back(Cell(ss.str()));
        ToCellList(columns, moreargs...);
    }


    template <typename...SMore>
    inline void ToCellList(tCellArray& columns, const Cell& cell, SMore...moreargs)
    {
        columns.push_back(cell);
        ToCellList(columns, moreargs...);
    }


    inline void ToCellList(tCellArray&) {}   // needed for the variadic with no args
    std::vector<tCellArray> mRows;

    tColCountToStyles   colCountToColStyles; // an array of column styles for each column count
    tRowToStyleMap      rowStyles;
    Style               defaultStyle;

    tColCountToColWidth colCountToColWidths;
};
