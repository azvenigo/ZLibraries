#include "LoggingHelpers.h"
int64_t LOG::gnVerbosityLevel = 1;

const Table::Style Table::kLeftAlignedStyle = Table::Style(COL_RESET, LEFT, EVEN);
const Table::Style Table::kRightAlignedStyle = Table::Style(COL_RESET, RIGHT, EVEN);
const Table::Style Table::kCenteredStyle = Table::Style(COL_RESET, CENTER, EVEN);



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

bool Table::SetColStyles(tOptionalStyleArray styles)
{
    size_t col_count = styles.size();
    if (colCountToColStyles[col_count].empty())
    {
        colCountToColStyles[col_count].resize(col_count);
    }

    for (size_t col_num = 0; col_num < col_count; col_num++)
    {
        colCountToColStyles[col_count][col_num] = styles[col_num];
    }
    return true;
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

bool Table::SetRowStyle(size_t row, const Style& style)
{
    rowStyles[row] = style;
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
    if (colCountToColStyles[col_count].size() == col_count)
    {
        if (colCountToColStyles[col_count][col] != nullopt)
            return colCountToColStyles[col_count][col].value();
    }

    return defaultStyle;
}

bool Table::SetCellStyle(size_t col, size_t row, const Style& style)
{
    // bounds checks
    if (row >= mRows.size())
        return false;

    if (col >= mRows[row].size())
        return false;

    mRows[row][col].style = style;
    return true;
}


// Table manimpulation
void Table::Clear()
{
    mRows.clear();

    bLayoutNeedsUpdating = true;
}



void Table::AddRow(tStringArray& row_strings)
{
    tCellArray a;
    for (const auto& s : row_strings)
        a.push_back(s);

    mRows.push_back(a);

    bLayoutNeedsUpdating = true;
}

void Table::AddRow(tCellArray& row)
{
    mRows.push_back(row);

    bLayoutNeedsUpdating = true;
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

size_t Table::Cell::Width(tOptionalStyle _style) const
{
    tOptionalStyle use_style = _style;
    if (use_style == nullopt)   // if none passed in, use member style
        use_style = style;

    size_t w = VisLength(s);

    if (use_style.has_value())
        w += use_style.value().padding * 2;

    return w;
}

std::string Substring(const std::string& str, size_t len)
{
    return str.substr(0, std::min(len, str.size()));
}

Table::Cell::Cell(const std::string& _s, tOptionalStyle _style)
{
    style = _style;

    std::string ansi;
    std::string rest;
    if (ExtractStyle(_s, ansi, rest))
    {
        s = rest;

        if (style == nullopt)
            style = Style();

        style.value().color = ansi;
    }
    else
    {
        s = _s;
    }
}

string Table::Cell::StyledOut(size_t width, tOptionalStyle _style)
{
    tOptionalStyle use_style = _style;
    if (use_style == nullopt)   // if none passed in, use member style
        use_style = style;

    if (use_style == nullopt)       // no style passed in and no member style
    {
        if (s.length() > width)
            return Substring(s, width); // however many will fit

        return s + PAD(width - s.length()); // pad out however many remaining spaces
    }

    uint8_t alignment = use_style.value().alignment;
    uint8_t padding = use_style.value().padding;

    if (width < padding * 2)    // if not enough space to draw anything
        return use_style.value().color + PAD(width) + COL_RESET;


    size_t remaining_width = width - padding * 2;

    string sOut = PAD(padding) + Substring(s, remaining_width) + PAD(padding);
    size_t visOutLen = VisLength(sOut);


    if (alignment == Table::CENTER)
    {
        size_t left_pad = (width - visOutLen) / 2;
        size_t right_pad = (width - left_pad - visOutLen);
        sOut = PAD(left_pad) + sOut + PAD(right_pad);
    }
    else if (alignment == Table::RIGHT)
    {
        sOut = PAD(width - visOutLen) + sOut;
    }
    else // default left
        sOut += PAD(width - visOutLen);

    return use_style.value().color + sOut + COL_RESET;
}


bool Table::Cell::ExtractStyle(const std::string& s, std::string& ansi, std::string& rest)
{
    if (s.size() < 4 || s[0] != '\x1b' || s[1] != '[')
        return false;

    size_t pos = 2;
    while (pos < s.size())
    {
        char c = s[pos];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
        {
            ansi = s.substr(0, pos + 1);
            rest = StripAnsiSequences(s.substr(pos + 1));
            return true;
        }
        else if (!(c >= '0' && c <= '9') && c != ';')
        {
            return false;
        }
        pos++;
    }

    return false;
}


// GetTableWidth() returns minimum width in characters to draw table (excluding mMinimumOutputWidth setting for output)
void Table::ComputeColumns()
{
    if (!bLayoutNeedsUpdating)
        return;

    colCountToColWidths.clear();

    size_t row_num = 0;
    for (const auto& row : mRows)
    {
        size_t cols = row.size();
        size_t col_num = 0;

        for (const auto& cell : row)
        {
            if (colCountToColWidths[cols].size() < cols)    // make sure there are enough entries for each column
                colCountToColWidths[cols].resize(cols);

            tOptionalStyle style = GetStyle(col_num, row_num);
            colCountToColWidths[cols][col_num] = std::max<size_t>(colCountToColWidths[cols][col_num], cell.Width(style));

            col_num++;
        }
        row_num++;
    }

    bLayoutNeedsUpdating = false;
}


size_t Table::GetTableMinWidth()
{
    ComputeColumns();
    size_t sepLen = VisLength(borders[Table::CENTER]);
    size_t leftBorderLen = VisLength(borders[Table::LEFT]);
    size_t rightBorderLen = VisLength(borders[Table::RIGHT]);

    size_t minWidth = 0;
    for (const auto& cols : colCountToColWidths)
    {
        size_t colCountWidth = sepLen * (cols.first - 1); // start width computation by taking account separators for all but last column
        for (const auto& col : cols.second)
        {
            colCountWidth += col;
        }
        minWidth = std::max<size_t>(minWidth, colCountWidth);
    }

    minWidth += leftBorderLen + rightBorderLen; // account for left and right borders

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


    size_t tableMinWidth = tableOut.GetTableMinWidth();

    size_t renderWidth = tableOut.renderWidth;
    if (renderWidth < tableMinWidth)
        renderWidth = tableMinWidth;

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

            size_t nEndDraw = renderWidth - VisLength(tableOut.borders[Table::RIGHT]);

            Table::Style style = tableOut.GetStyle(col_num, row_num);

            if (cursor < nEndDraw)   // adding this check in case previously drawn columns were wider than total available
            {
                if (bLastColumnInRow)
                {
                    // last column is drawn to end
                    nDrawWidth = nEndDraw - cursor;
                }
                else
                {
                    if (style.spacing == Table::TIGHT)
                        nDrawWidth = nColWidth;
                    else if (style.spacing == Table::EVEN)
                        nDrawWidth = std::max<size_t>(renderWidth / cols, nColWidth);
                }

                os << tableOut.GetCell(col_num, row_num).StyledOut(nDrawWidth, style);

                cursor += nDrawWidth;

                // Output a separator for all but last column
                if (!bLastColumnInRow && cursor < nEndDraw)
                {
                    os << separator << COL_RESET;
                    cursor += VisLength(separator);
                }
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
Table Table::Transpose()
{
    Table transposedTable;

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
