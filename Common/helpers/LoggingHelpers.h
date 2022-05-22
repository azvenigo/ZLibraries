#pragma once
#include <stdint.h>
#include <string>
#include <sstream>
#include <any>
#include <iostream>
#include <list>
#include <vector>
#include <iomanip>

inline std::string HexValueOut(uint32_t nVal, bool bIncludeDecimal = true)
{
    char buf[64];
    if (bIncludeDecimal)
        sprintf_s(buf, "0x%08x (%ld)", nVal, nVal);
    else
        sprintf_s(buf, "0x%08x", nVal);
    return std::string(buf);
}

inline std::string HexValueOut(uint64_t nVal, bool bIncludeDecimal = true)
{
    char buf[64];
    if (bIncludeDecimal)
        sprintf_s(buf, "0x%016llx (%lld)", nVal, nVal);
    else
        sprintf_s(buf, "0x%016llx", nVal);
    return std::string(buf);
}

inline void DumpMemoryToCout(uint8_t* pBuf, uint32_t nBytes, uint32_t nBaseMemoryOffset = 0, uint32_t nColumns = 32)
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
            std::cout << "\n" << HexValueOut((uint32_t)(nBaseMemoryOffset + (pWalker - pBuf)), false) << ": ";
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


typedef std::list<std::string> tStringList;



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
    TableOutput(const std::string& separator = " ") { mSeparator = separator; }
    void SetSeparator(const std::string& separator) { mSeparator = separator; }
    void Clear() { mRows.clear(); }

    template <typename T, typename...Types>
    inline void AddRow(T arg, Types...more)
    {
        tStringList columns;
        ToStringList(columns, arg, more...);
        mRows.push_back(columns);
    }


    friend std::ostream& operator <<(std::ostream& os, const TableOutput& tableOut)
    {
        // compute max # of columns
        size_t nMaxColumns = 0;
        for (auto row : tableOut.mRows)
            nMaxColumns = nMaxColumns < row.size() ? row.size() : nMaxColumns;


        // Compute max width of each column
        std::vector<size_t> columnWidths;
        columnWidths.resize(nMaxColumns);

        for (auto row : tableOut.mRows)
        {
            size_t nCol = 0;
            for (auto s : row)
            {
                columnWidths[nCol] = columnWidths[nCol] < s.length() ? s.length() : columnWidths[nCol];
                nCol++;
            }
        }

        // Now print each row based on column widths
        for (auto row : tableOut.mRows)
        {
            size_t nCol = 0;
            for (auto s : row)
            {
                os << std::setw(columnWidths[nCol]) << s;
                // Output a separator for all but last column
                if (nCol < row.size() - 1)
                    os << tableOut.mSeparator;
                nCol++;
            }
            os << "\n";
        }

        return os;
    }

protected:

    template <typename S, typename...SMore>
    inline void ToStringList(tStringList& columns, S arg, SMore...moreargs)
    {
        std::stringstream ss;
        ss << arg;
        columns.push_back(ss.str());
        return ToStringList(columns, moreargs...);
    }

    inline void ToStringList(tStringList&) {}   // needed for the variadic with no args

    std::list<tStringList> mRows;

    // Formatting options
    std::string mSeparator; // between columns
    char mOutlineChar;      // Surrounding Table
};