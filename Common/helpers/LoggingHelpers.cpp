#include "LoggingHelpers.h"
int64_t LOG::gnVerbosityLevel = 1;

using namespace std;

#define PAD(n) string(n, ' ')

Table::Table()
{
}

// Formatting

void Table::SetBorders(const std::string& _L, const std::string& _T, const std::string& _R, const std::string& _B, const std::string& _C)
{
    borders[LEFT] = _L;
    borders[TOP] = _T;
    borders[RIGHT] = _R;
    borders[BOTTOM] = _B;
    borders[CENTER] = _C;
}

bool Table::SetColStyle(size_t col_count, size_t col_num, const Style& style)
{
    if (col_num > col_count)
    {
        assert(false);
        return false;
    }

    // lazy alloc
    if (colCountToColStyles[col_count].empty())
    {
        colCountToColStyles[col_count].resize(col_count);
    }
    
    colCountToColStyles[col_count][col_num] = style;
    return true;
}

Table::Style Table::GetStyle(size_t col, size_t row)
{
    // bounds checks
    if (row >= mRows.size())
        return defaultStyle;

    if (col >= mRows[row].size())
        return defaultStyle;

    // Style is prioritized as follows
    // 1) cell style
    // 2) row style
    // 3) col style
    // 4) default style

    // cell
    if (mRows[row][col].style != std::nullopt)
        return mRows[row][col].style.value();

    // row
    tRowToStyleMap::iterator it = rowStyles.find(row);
    if (it != rowStyles.end())
        return (*it).second.value();

    // col
    size_t col_count = mRows[row].size();
    if (colCountToColStyles[col_count][col] != nullopt)
        return colCountToColStyles[col_count][col].value();

    return defaultStyle;
}



// Table manimpulation
void Table::Clear()
{ 
    mRows.clear();
}



void Table::AddRow(tStringArray& row_strings)
{
    tCellArray a;
    for (const auto& s : row_strings)
        a.push_back(s);

    mRows.push_back(a);
}

void Table::AddRow(tCellArray& row)
{
    mRows.push_back(row);
}


void Table::AddMultilineRow(string& sMultiLine)
{
    stringstream ss;
    ss << sMultiLine;
    string s;
    while (getline(ss, s, '\n'))
        AddRow(s);
}

Table::Cell Table::GetCell(size_t col, size_t row)
{
    if (row > mRows.size())
        return {};

    if (mRows[row].size() < col)
        return {};

    return mRows[row][col];
}


size_t Table::GetRowCount() const
{
    return mRows.size();
}

size_t Table::Cell::Width() const
{
    size_t w = s.length();

    if (style.has_value())
        w += style.value().padding*2;

    return w;
}

std::string Substring(const std::string& str, size_t len) 
{
    return str.substr(0, std::min(len, str.size()));
}

string Table::Cell::StyledOut(size_t width)
{
    if (style == nullopt)
    {
        if (s.length() < width)
            return Substring(s, width); // however many will fit
        
        return s + PAD(width - s.length()); // pad out however many remaining spaces
    }

    uint8_t alignment = style.value().alignment;
    uint8_t padding = style.value().padding;
    size_t remaining_width = width - padding * 2;

    string sOut = PAD(padding) + Substring(s, remaining_width) + PAD(padding);

    if (alignment == Table::CENTER)
    {
        size_t left_pad = (width - sOut.length()) / 2;
        size_t right_pad = (width - left_pad - sOut.length());
        sOut = PAD(left_pad) + sOut + PAD(right_pad);
    }
    else if (alignment == Table::RIGHT)
    {
        sOut = PAD(width - sOut.length()) + sOut;
    }
    else // default left
        sOut += PAD(width - sOut.length());

    return style.value().color + sOut + COL_RESET;
}


// GetTableWidth() returns minimum width in characters to draw table (excluding mMinimumOutputWidth setting for output)
void Table::ComputeColumns()
{
    if (!bLayoutNeedsUpdating)
        return;

    colCountToColWidths.clear();

    for (const auto& row : mRows)
    {
        size_t cols = row.size();
        size_t col_num = 0;

        for (const auto& cell : row)
        {
            if (colCountToColWidths[cols].size() < cols)    // make sure there are enough entries for each column
                colCountToColWidths[cols].resize(cols);

            colCountToColWidths[cols][col_num] = std::max<size_t>(colCountToColWidths[cols][col_num], cell.Width());

            col_num++;
        }
    }

    bLayoutNeedsUpdating = false;
}


size_t Table::GetTableMinWidth() 
{
    ComputeColumns();

    size_t minWidth = 0;
    for (const auto& cols : colCountToColWidths)
    {
        size_t colCountWidth = 0;
        for (const auto& col : cols.second)
        {
            colCountWidth += col;
        }
        minWidth = std::max<size_t>(minWidth, colCountWidth);
    }

    return minWidth;
}


Table::operator string()
{
    ostringstream ss;
    ss << *this;
    string sTable(ss.str());
    for (size_t i = 0; i < sTable.length(); i++)
        if (sTable[i] == 0)
            sTable[i] = '\n';

    return sTable;
}

size_t VisLength(const std::string& s)
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

    return s.length() - nCount * kEscapeLength;
}

string RepeatString(const string& s, int64_t w)
{
    string sOut;
    size_t visLen = VisLength(s);
    while (w > 0)
    {
        sOut += s;
        w -= visLen;
    }

    return sOut;
};

ostream& operator <<(ostream& os, Table& tableOut)
{
    tableOut.ComputeColumns();

    if (tableOut.mRows.empty())
        return os;

    size_t renderWidth = tableOut.renderWidth;
    if (renderWidth == 0)
        renderWidth = tableOut.GetTableMinWidth();

    // Draw top border
    if (!tableOut.borders[Table::TOP].empty())
    {
        os << RepeatString(tableOut.borders[Table::TOP], renderWidth) << COL_RESET << "\n";
    }



    // Now print each row based on column widths
    size_t row_num = 0;
    for (const auto& row : tableOut.mRows)
    {
        size_t cursor = 0;
        size_t cols = row.size();

        string separator = tableOut.borders[Table::CENTER];

        // Draw left border
        os << tableOut.borders[Table::LEFT] << COL_RESET;
        cursor += VisLength(tableOut.borders[Table::LEFT]);


        for (size_t col_num = 0; col_num < cols; col_num++)
        {
            bool bLastColumnInRow = (col_num == cols - 1);


            size_t nColWidth = tableOut.colCountToColWidths[cols][col_num];
            size_t nDrawWidth = nColWidth;

            Table::Style style = tableOut.GetStyle(col_num, row_num);

            if (bLastColumnInRow)
            {
                // last column is drawn to end
                nDrawWidth = renderWidth - cursor - VisLength(tableOut.borders[Table::RIGHT]);
            }
            else
            {
                if (style.spacing == Table::TIGHT)
                    nDrawWidth = nColWidth;
                else if (style.spacing == Table::EVEN)
                    nDrawWidth = renderWidth / cols;
            }

            os << tableOut.GetCell(col_num, row_num).StyledOut(nDrawWidth);

            cursor += nDrawWidth;

            // Output a separator for all but last column
            if (!bLastColumnInRow)
            {
                os << separator << COL_RESET;
                cursor += VisLength(separator);
            }
        }

        // Draw right border
        os << tableOut.borders[Table::RIGHT] << COL_RESET << "\n";

        row_num++;
    }

    // bottom border
    if (!tableOut.borders[Table::BOTTOM].empty())
    {
        os << RepeatString(tableOut.borders[Table::BOTTOM], renderWidth) << COL_RESET << "\n";
    }

    return os;
}
/*
TableOutput TableOutput::Transpose()
{
    TableOutput transposedTable;

    size_t maxCols = 0;
    for (const auto& row : mRows)
    {
        maxCols = max<size_t>(maxCols, row.size());
    }

    for (size_t colCounter = 0; colCounter < maxCols; colCounter++)
    {
        tStringArray newRow;
        for (size_t rowCounter = 0; rowCounter < mRows.size(); rowCounter++)
            newRow.push_back(ElementAt(rowCounter, colCounter));

        transposedTable.mRows.push_back(newRow);
    }
    //        transposedTable.mMaxColumns = mRows.size();
    transposedTable.mT = mT;
    transposedTable.mL = mL;
    transposedTable.mR = mR;
    transposedTable.mB = mB;
    transposedTable.mSeparator = mSeparator;
    transposedTable.mColumnPadding = mColumnPadding;
    transposedTable.mMinimumOutputWidth = mMinimumOutputWidth;

    return transposedTable;
}
*/