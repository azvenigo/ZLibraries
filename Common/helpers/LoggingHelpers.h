#pragma once
#include <stdint.h>
#include <string>
#include <cstring>
#include <sstream>
//#include <any>
#include <iostream>
#include <inttypes.h>
#include <list>
#include <vector>
#include <iomanip>
#include <algorithm>

#define ENABLE_ANSI_OUT
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
#define OUT_ERR(statement)      { statement; assert(false); }


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
class TableOutput
{
public:
    TableOutput() : mColumns(0), mT(0), mB(0), mL(0), mR(0), mSeparator(0), mColumnPadding(0), mMinimumOutputWidth(0) {}

    // Formatting

    void SetMinimumOutputWidth(size_t minWidth)
    {
        mMinimumOutputWidth = minWidth;
    }

    void SetSeparator(char sep = 0, size_t padding=0) 
    { 
        mSeparator = sep;  
        mColumnPadding = padding;
    }
    void SetBorders(char top = 0, char bottom = 0, char left = 0, char right = 0) // optional formatting
    {
        mT = top;
        mB = bottom;
        mL = left;
        mR = right;
    }

    void SetAlignment(std::initializer_list<bool> columnAlignments)
    {
        for(auto bRight : columnAlignments)
            mRightAlignedColumns.push_back(bRight);
    }


    // Table manimpulation
    void Clear() { mRows.clear(); }

    template <typename T, typename...Types>
    inline void AddRow(T arg, Types...more)
    {
        tStringArray columns;
        ToStringList(columns, arg, more...);

        // Check if the row being added has more columns than any row before.
        // If so, resize all existing rows to match
        if (columns.size() > mColumns)
        {
            mColumns = columns.size();
            for (auto& row : mRows)
                row.resize(mColumns);
        }
        else if (columns.size() < mColumns)
            columns.resize(mColumns);

        mRows.push_back(columns);
    }

    inline void AddRow(tStringArray& columns)
    {
        // Check if the row being added has more columns than any row before.
        // If so, resize all existing rows to match
        if (columns.size() > mColumns)
        {
            mColumns = columns.size();
            for (auto& row : mRows)
                row.resize(mColumns);
        }
        else if (columns.size() < mColumns)
            columns.resize(mColumns);
        mRows.push_back(columns);
    }

    void AddMultilineRow(std::string& sMultiLine)
    {
        std::stringstream ss;
        ss << sMultiLine;
        std::string s;
        while (std::getline(ss, s, '\n'))
            AddRow(s);
    }

    std::string ElementAt(size_t row, size_t col)
    {
        if (row > mRows.size() || col > mColumns)
            return "";
        
        auto rowIterator = mRows.begin();
        size_t r = 0;
        for (r = 0; r < row; r++)
        {
            rowIterator++;
        }

        tStringArray& rowArray = *rowIterator;
        if (col > rowArray.size())
            return "";

        return rowArray[col];
    }

    // Output

    size_t GetRowCount() const
    {
        return mRows.size();
    }

    static size_t VisLength(const std::string& s)
    {
        const size_t kEscapeLength = strlen(COL_RESET);

        size_t nCount = 0;
        size_t nEscapeChar = s.find("\x1b", 0);
        do
        {
            if (nEscapeChar != std::string::npos)
                nCount++;
            nEscapeChar = s.find("\x1b", nEscapeChar + kEscapeLength);
        } while (nEscapeChar != std::string::npos);

        return s.length() -nCount * kEscapeLength;
    }

    // GetTableWidth() returns minimum width in characters to draw table (excluding mMinimumOutputWidth setting for output)
    size_t GetTableWidth() const
    {
        // Compute max width of each column
        std::vector<size_t> columnWidths;
        columnWidths.resize(mColumns);

        for (auto row : mRows)
        {
            size_t nCol = 0;
            for (auto s : row)
            {
                columnWidths[nCol] = columnWidths[nCol] < VisLength(s) ? VisLength(s) : columnWidths[nCol];
                nCol++;
            }
        }

        size_t tableWidth = 0;
        for (auto w : columnWidths)     // width of all columns
            tableWidth += w;

        // separator between columns is 1 char plus any padding.
        if (columnWidths.size() > 1)
            tableWidth += ((1 + mColumnPadding) * (columnWidths.size() - 1));

        // If left border
        if (mL)
            tableWidth += 1 + mColumnPadding;

        // If right border
        if (mR)
            tableWidth += 1 + mColumnPadding;

        return tableWidth;
    }


    operator std::string()
    {
        std::ostringstream ss;
        ss << *this;
        std::string sTable(ss.str());
        for (size_t i = 0; i < sTable.length(); i++)
            if (sTable[i] == 0)
                sTable[i] = '\n';

        return sTable;
    }

#define PAD(n) std::string(n, ' ')

    friend std::ostream& operator <<(std::ostream& os, const TableOutput& tableOut)
    {
        if (tableOut.mRows.empty())
            return os;

        size_t tableWidth = tableOut.GetTableWidth();

        // Compute max width of each column
        std::vector<size_t> columnWidths;
        columnWidths.resize(tableOut.mColumns);

        for (auto row : tableOut.mRows)
        {
            size_t nCol = 0;
            for (auto s : row)
            {
                if (columnWidths[nCol] < VisLength(s))
                    columnWidths[nCol] = VisLength(s);
                nCol++;
            }
        }

        size_t nTotalColumnWidths = 0;
        for (auto c : columnWidths)
            nTotalColumnWidths += (c+tableOut.mColumnPadding);
        // If left border
        if (tableOut.mL)
            nTotalColumnWidths += (1 + tableOut.mColumnPadding);
        // If right border
        if (tableOut.mR)
            nTotalColumnWidths += (1 + tableOut.mColumnPadding);

        // If the computed widths don't add up to the minimum, pad the last column extra
        if (nTotalColumnWidths < tableOut.mMinimumOutputWidth)
        {
            tableWidth = tableOut.mMinimumOutputWidth;
        }


        // Draw top border
        if (tableOut.mT)
            os << std::string(tableWidth, tableOut.mT) << "\n"; // repeat out



        // Now print each row based on column widths
        for (auto row : tableOut.mRows)
        {
            size_t nCharsOnRow = 0;
            // Draw left border
            if (tableOut.mL)
            {
                os << tableOut.mL << PAD(tableOut.mColumnPadding);
                nCharsOnRow += (tableOut.mL != 0) + tableOut.mColumnPadding;
            }

            for (size_t nCol = 0; nCol < tableOut.mColumns; nCol++)
            {
                size_t nColWidth = columnWidths[nCol];
                std::string s(row[nCol]);

                if (nCol < row.size() - 1)
                {
                    s += tableOut.mSeparator;
                    nColWidth++;
                }

                if (tableOut.RightAligned(nCol))
                    os << PAD(nColWidth - VisLength(s)) << s;
                else
                    os << s << PAD(nColWidth - VisLength(s));

                nCharsOnRow += nColWidth;


                // Output a separator for all but last column
                if (nCol < row.size() - 1)
                {
                    os << PAD(tableOut.mColumnPadding);
                    nCharsOnRow += tableOut.mColumnPadding;
                }
            }

            // Draw right border
            if (tableOut.mR)
            {
                size_t nFinalColumnPadding = tableWidth - nCharsOnRow-1;
                os << PAD(nFinalColumnPadding) << tableOut.mR;
            }

            os << "\n";
        }

        // bottom border
        if (tableOut.mB)
            os << std::string(tableWidth, tableOut.mB) << "\n";

        return os;
    }

    TableOutput Transpose()
    {
        TableOutput transposedTable;


        for (size_t colCounter = 0; colCounter < mColumns; colCounter++)
        {
            tStringArray newRow;
            for (size_t rowCounter = 0; rowCounter < mRows.size(); rowCounter++)
                newRow.push_back(ElementAt(rowCounter, colCounter));

            transposedTable.mRows.push_back(newRow);
        }
        transposedTable.mColumns = mRows.size();
        transposedTable.mT = mT;
        transposedTable.mL = mL;
        transposedTable.mR = mR;
        transposedTable.mB = mB;
        transposedTable.mSeparator = mSeparator;
        transposedTable.mColumnPadding = mColumnPadding;
        transposedTable.mMinimumOutputWidth = mMinimumOutputWidth;

        return transposedTable;
    }

    // Variadic template function to find the maximum table width
    template <typename... Tables>
    size_t GetMaxTableWidth(size_t minW, const Tables&... tables) 
    {
        return std::max({ minW, tables.GetTableWidth()... });
    }

    template <typename... Tables>
    void AlignWidth(size_t minW, Tables&... tables) 
    {
        size_t nMinTableWidth = GetMaxTableWidth(minW, tables...);
        (tables.SetMinimumOutputWidth(nMinTableWidth), ...);
    }





protected:

    template <typename S, typename...SMore>
    inline void ToStringList(tStringArray& columns, S arg, SMore...moreargs)
    {
        std::stringstream ss;
        ss << arg;
        columns.push_back(ss.str());
        return ToStringList(columns, moreargs...);
    }

    inline void ToStringList(tStringArray&) {}   // needed for the variadic with no args

    inline bool RightAligned(size_t nCol) const
    {
        if (mRightAlignedColumns.size() <= nCol)
            return false;

        return mRightAlignedColumns[nCol];
    }

    std::list<tStringArray> mRows;
    size_t mColumns;

    std::vector<bool> mRightAlignedColumns;  // true if right aligned

    // Formatting options
    char mT;
    char mB;
    char mL;
    char mR;
    char mSeparator; // between columns
    size_t mColumnPadding;
    size_t mMinimumOutputWidth;
};
