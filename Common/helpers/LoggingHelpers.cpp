#include "LoggingHelpers.h"
#include "StringHelpers.h"
#include <filesystem>
#include <chrono>

using namespace std;

#define PAD(n, c) string(n, c)

namespace LOG
{
    int64_t gnVerbosityLevel = 1;
    Logger gLogger;
    LogStream gLogOut(gLogger, cout);
    LogStream gLogErr(gLogger, std::cerr);
    thread_local char LogStreamBuf::m_buffer[LogStreamBuf::BUFFER_SIZE];



    std::string usToDateTime(uint64_t us)
    {
        time_t seconds = us / 1000000;
        uint64_t remainingus = us % 1000000;
        std::tm* timeInfo = std::localtime(&seconds);
        std::ostringstream oss;
        oss << std::put_time(timeInfo, "%Y/%m/%d %H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(6) << remainingus;
        return oss.str();
    }

    std::string usToElapsed(uint64_t us)
    {
        uint64_t hours = us / 3600000000ULL;
        uint64_t minutes = us / 60000000ULL;
        us %= 60000000ULL;

        uint64_t seconds = us / 1000000ULL;
        us %= 1000000ULL;


        std::ostringstream oss;
        oss << hours << ":"
            << std::setfill('0') << std::setw(2) << minutes << ":"
            << std::setfill('0') << std::setw(2) << seconds << ":"
            << std::setfill('0') << std::setw(6) << us;

        return oss.str();
    }


    LogEntry::LogEntry(const std::string& _text, uint64_t _time)
    {
        text = _text;

        if (_time == -1)
            time = std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1);
        else
            time = _time;
    };

    bool Logger::getEntries(uint64_t startingIndex, size_t count, std::deque<LogEntry>& outEntries, const std::string& sFilter) const
    {
        std::lock_guard<std::mutex> lock(logEntriesMutex);
        outEntries.clear();

        // If we have a filter, we need to count matching entries
        if (!sFilter.empty())
        {
            auto entry = logEntries.begin();
            uint64_t matchingEntriesSkipped = 0;

            // Skip startingIndex matching entries
            while (entry != logEntries.end() && matchingEntriesSkipped < startingIndex)
            {
                // Assuming LogEntry has a toString() or similar method to get its content
                if (SH::Contains((*entry).text, sFilter, false))
                {
                    matchingEntriesSkipped++;
                }
                entry++;
            }

            // Collect count matching entries
            while (entry != logEntries.end() && outEntries.size() < count)
            {
                if (SH::Contains((*entry).text, sFilter, false))
                {
                    outEntries.push_back(*entry);
                }
                entry++;
            }
        }
        else
        {
            // Original logic for when no filter is applied
            auto entry = logEntries.begin();
            while (startingIndex > 0 && entry != logEntries.end())
            {
                entry++;
                startingIndex--;
            }
            while (entry != logEntries.end() && outEntries.size() < count)
            {
                outEntries.push_back(*entry);
                entry++;
            }
        }

        return true;
    }

    std::deque<LogEntry> Logger::tail(size_t n, const std::string& sFilter) const
    {
        std::deque<LogEntry> result;
        std::lock_guard<std::mutex> lock(logEntriesMutex);

        if (sFilter.empty())
        {
            // Original logic for when no filter is applied
            if (n >= logEntries.size())
            {
                // If requesting more entries than exist, return the whole log
                return logEntries;
            }
            // Calculate starting index for the tail portion
            size_t startIndex = logEntries.size() - n;
            // Copy the last n elements to the result deque
            result.insert(result.begin(), logEntries.begin() + startIndex, logEntries.end());
        }
        else
        {
            // When filter is applied, collect the last n matching entries
            for (auto it = logEntries.rbegin(); it != logEntries.rend() && result.size() < n; ++it)
            {
                if (SH::Contains((*it).text, sFilter, false))
                {
                    result.push_front(*it);
                }
            }
        }

        return result;
    }

} // namespace LOG


ostream& operator <<(std::ostream& os, Table::Style& style)
{
    os << style.color;
    if (style.alignment == Table::LEFT)
        os << std::left;
    else if (style.alignment == Table::RIGHT)
        os << std::right;

    return os;
}

const Table::Style Table::kLeftAlignedStyle = Table::Style(COL_RESET, LEFT, EVEN);
const Table::Style Table::kRightAlignedStyle = Table::Style(COL_RESET, RIGHT, EVEN);
const Table::Style Table::kCenteredStyle = Table::Style(COL_RESET, CENTER, EVEN);


Table::Table()
{
}

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

std::string Substring(const std::string& input, size_t maxLength)
{
    if (maxLength == 0 || input.empty()) 
    {
        return "";
    }

    std::string result;
    size_t visibleChars = 0;

    enum class State 
    {
        TEXT,  // Normal text
        ESC,   // Escape character
        CSI,   // Control Sequence Introducer (ESC [)
        OSC,   // Operating System Command (ESC ])
        PARAM  // Parameter in sequence
    };

    State state = State::TEXT;
    size_t i = 0;

    while (i < input.length() && (visibleChars < maxLength || state != State::TEXT)) 
    {
        char c = input[i];
        result.push_back(c);

        switch (state) 
        {
        case State::TEXT:
            if (c == '\x1B') 
            {  // ESC character
                state = State::ESC;
            }
            else 
            {
                // Count visible characters only when in TEXT state
                visibleChars++;
            }
            break;

        case State::ESC:
            if (c == '[') 
            {
                state = State::CSI;
            }
            else if (c == ']') 
            {
                state = State::OSC;
            }
            else 
            {
                // Other escape sequences or malformed - assume short
                state = State::TEXT;
            }
            break;

        case State::CSI:
        case State::PARAM:
            if ((c >= '0' && c <= '9') || c == ';' || c == '?' || c == ':') 
            {
                // These are parameter characters
                state = State::PARAM;
            }
            else if (c >= 0x40 && c <= 0x7E) 
            {
                // Final byte - end of CSI sequence
                state = State::TEXT;
            }
            break;

        case State::OSC:
            if (c == '\x07') 
            {  // BEL character - ends OSC
                state = State::TEXT;
            }
            else if (c == '\x1B') 
            {
                // Could be ESC \ which ends OSC
                // We'll leave it in ESC state and handle on next char
                state = State::ESC;
                // Don't advance i here since we need to process this ESC
                i--;
            }
            break;
        }

        i++;
    }

    // If we've reached the max length and are in the middle of an ANSI sequence,
    // we need to add the rest of the sequence to maintain proper styling
    while (state != State::TEXT && i < input.length()) 
    {
        char c = input[i];
        result.push_back(c);

        switch (state) 
        {
        case State::ESC:
            if (c == '[') 
            {
                state = State::CSI;
            }
            else if (c == ']') 
            {
                state = State::OSC;
            }
            else 
            {
                state = State::TEXT;
            }
            break;

        case State::CSI:
        case State::PARAM:
            if ((c >= '0' && c <= '9') || c == ';' || c == '?' || c == ':') 
            {
                state = State::PARAM;
            }
            else if (c >= 0x40 && c <= 0x7E) 
            {
                state = State::TEXT;
            }
            break;

        case State::OSC:
            if (c == '\x07' || (c == '\\' && i > 0 && input[i - 1] == '\x1B')) 
            {
                state = State::TEXT;
            }
            break;

        case State::TEXT:
            // Won't reach here, but to avoid compiler warnings
            break;
        }

        i++;
    }

    // Make sure we end with a reset sequence if we truncated in the middle of styled text
/*    if (visibleChars >= maxLength && i < input.length()) 
    {
        result += "\x1B[0m";
    }*/

    return result;
}

Table::Cell::Cell(const std::string& _s, tOptionalStyle _style)
{
    style = _style;

    std::string ansi;
    std::string rest;
    if (ExtractStyle(_s, ansi, rest))
    {
        s = rest;

#ifdef _DEBUG
        validateAnsiSequences(s);
#endif

        if (style == nullopt)
            style = Style();

        style.value().color = ansi;
#ifdef _DEBUG
        validateAnsiSequences(ansi);
#endif


    }
    else
    {
        s = _s;
#ifdef _DEBUG
        validateAnsiSequences(s);
#endif
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

        return s + PAD(width - s.length(), ' '); // pad out however many remaining spaces
    }


    uint8_t alignment = use_style.value().alignment;
    uint8_t padding = use_style.value().padding;
    char padchar = use_style.value().padchar;

    // if no room to pad, disable
    if (VisLength(s) >= width)
        padding = 0;

    if (width < padding * 2)    // if not enough space to draw anything
        return use_style.value().color + PAD(width, padchar) + COL_RESET;

    string use_color;
    
    if (use_style.value().color != COL_CUSTOM_STYLE)
        use_color = use_style.value().color;




    size_t remaining_width = width - padding * 2;

    string sOut = Substring(s, remaining_width);
    string sStyled = PAD(padding, padchar) + use_color + sOut + COL_RESET + PAD(padding, padchar);

    size_t visOutLen = VisLength(sStyled);

    //assert(visOutLen <= remaining_width);


    if (alignment == Table::CENTER)
    {
        size_t left_pad = (width - visOutLen) / 2;
        size_t right_pad = (width - left_pad - visOutLen);
        return PAD(left_pad, padchar) + sStyled + PAD(right_pad, padchar);
    }
    else if (alignment == Table::RIGHT)
    {
        return PAD(width - visOutLen, padchar) + sStyled;
    }

    return sStyled + PAD(width - visOutLen, padchar);
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

    colCountToMinColWidths.clear();

    size_t row_num = 0;
    for (const auto& row : mRows)
    {
        size_t cols = row.size();
        size_t col_num = 0;

        for (const auto& cell : row)
        {
            if (colCountToMinColWidths[cols].size() < cols)    // make sure there are enough entries for each column
                colCountToMinColWidths[cols].resize(cols);

            tOptionalStyle style = GetStyle(col_num, row_num);
            colCountToMinColWidths[cols][col_num] = std::max<size_t>(colCountToMinColWidths[cols][col_num], cell.Width(style));

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
    for (const auto& cols : colCountToMinColWidths)
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

size_t Table::GetTableMinWidthForColCount(size_t col_count)
{
    ComputeColumns();
    size_t sepLen = VisLength(borders[Table::CENTER]);
    size_t leftBorderLen = VisLength(borders[Table::LEFT]);
    size_t rightBorderLen = VisLength(borders[Table::RIGHT]);
    size_t minWidth = 0;

    const auto& cols = colCountToMinColWidths[col_count];
    size_t colCountWidth = sepLen * (col_count - 1); // start width computation by taking account separators for all but last column
    for (const auto& col : cols)
    {
        colCountWidth += col;
    }
    minWidth = std::max<size_t>(minWidth, colCountWidth);

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

#ifdef _DEBUG
    validateAnsiSequences(sTable);
#endif

    return sTable;
}


string RepeatString(const string& s, int64_t w)
{
    int64_t repeatedW = w;
    string sOut;
    size_t visLen = VisLength(s);
    while (repeatedW > 0)
    {
        sOut +=  s;
        repeatedW -= visLen;
    }

    if (repeatedW < 0)
    {
        // for multi-char repeated strings, trim back until the visible width doesn't exceed w
        while ((int64_t)StripAnsiSequences(sOut).length() > w)
        {
            sOut = sOut.substr(0, sOut.length() - 1);
        }
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

        size_t tableMinWidthForColCount = tableOut.GetTableMinWidthForColCount(cols);

        string separator = tableOut.borders[Table::CENTER];
#ifdef _DEBUG
        validateAnsiSequences(separator);
        validateAnsiSequences(tableOut.borders[Table::LEFT]);
        validateAnsiSequences(tableOut.borders[Table::TOP]);
        validateAnsiSequences(tableOut.borders[Table::RIGHT]);
        validateAnsiSequences(tableOut.borders[Table::BOTTOM]);
        validateAnsiSequences(COL_RESET);

#endif



        // Draw left border
        os << tableOut.borders[Table::LEFT] << COL_RESET;
        cursor += VisLength(tableOut.borders[Table::LEFT]);


        for (size_t col_num = 0; col_num < cols; col_num++)
        {
            bool bLastColumnInRow = (col_num == cols - 1);

            size_t nMinColWidth = tableOut.colCountToMinColWidths[cols][col_num];
            size_t nDrawWidth = nMinColWidth;

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
                        nDrawWidth = nMinColWidth;
                    else if (style.spacing == Table::EVEN)
                    {
//                        size_t cellWidth = tableOut.GetCell(col_num, row_num).Width();
                        size_t cellWidth = nMinColWidth;
                        nDrawWidth = std::max<size_t>((cellWidth * renderWidth) / tableMinWidthForColCount, nMinColWidth);
                    }
                }

#ifdef _DEBUG
                string sStyleCheck = tableOut.GetCell(col_num, row_num).StyledOut(nDrawWidth, style);
                validateAnsiSequences(sStyleCheck);
#endif

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

bool validateAnsiSequences(const std::string& input)
{
    enum class State
    {
        TEXT,       // Normal text
        ESC,        // Escape character seen
        CSI,        // Control Sequence Introducer (ESC [)
        OSC,        // Operating System Command (ESC ])
        PARAM,      // Parameter in CSI
        PRIVATE,    // Private mode parameter (after ?)
        INTERMEDIATE // Intermediate bytes (between params and final byte)
    };

    State state = State::TEXT;
    size_t i = 0;

    while (i < input.length()) {
        char c = input[i];

        switch (state) {
        case State::TEXT:
            if (c == '\x1B') {  // ESC character
                state = State::ESC;
            }
            break;

        case State::ESC:
            if (c == '[') {
                state = State::CSI;
            }
            else if (c == ']') {
                state = State::OSC;
            }
            else if (c >= 0x40 && c <= 0x5F) {
                // 2-character escape sequence (ESC + function)
                state = State::TEXT;
            }
            else {
                // Invalid escape sequence
                std::cerr << "Invalid escape sequence at position " << i << std::endl;
                return false;
            }
            break;

        case State::CSI:
            if (c == '?') {
                state = State::PRIVATE;
            }
            else if (c >= '0' && c <= '9') {
                state = State::PARAM;
            }
            else if (c >= 0x20 && c <= 0x2F) {
                state = State::INTERMEDIATE;
            }
            else if (c >= 0x40 && c <= 0x7E) {
                // Final byte - end of CSI sequence
                state = State::TEXT;
            }
            else {
                std::cerr << "Invalid character in CSI sequence at position " << i << std::endl;
                return false;
            }
            break;

        case State::PRIVATE:
            if (c >= '0' && c <= '9') {
                // Parameter digits after ?
                // Stay in PRIVATE state
            }
            else if (c == ';') {
                // Parameter separator
                // Stay in PRIVATE state
            }
            else if (c >= 0x20 && c <= 0x2F) {
                state = State::INTERMEDIATE;
            }
            else if (c >= 0x40 && c <= 0x7E) {
                // Final byte
                state = State::TEXT;
            }
            else {
                std::cerr << "Invalid character in private mode sequence at position " << i << std::endl;
                return false;
            }
            break;

        case State::PARAM:
            if (c >= '0' && c <= '9') {
                // Stay in PARAM state for digits
            }
            else if (c == ';') {
                // Parameter separator
                // Stay in PARAM state
            }
            else if (c >= 0x20 && c <= 0x2F) {
                state = State::INTERMEDIATE;
            }
            else if (c >= 0x40 && c <= 0x7E) {
                // Final byte
                state = State::TEXT;
            }
            else {
                std::cerr << "Invalid character in parameter at position " << i << std::endl;
                return false;
            }
            break;

        case State::INTERMEDIATE:
            if (c >= 0x20 && c <= 0x2F) {
                // Stay in INTERMEDIATE state
            }
            else if (c >= 0x40 && c <= 0x7E) {
                // Final byte
                state = State::TEXT;
            }
            else {
                std::cerr << "Invalid intermediate byte at position " << i << std::endl;
                return false;
            }
            break;

        case State::OSC:
            // For Operating System Commands, we look for ST (String Terminator)
            // which is ESC \ or BEL character
            if (c == '\x07') { // BEL character
                state = State::TEXT;
            }
            else if (c == '\x1B') {
                // Check for ESC 
                if (i + 1 < input.length() && input[i + 1] == '\\')
                {
                    state = State::TEXT;
                    i++; // Skip the next character (\)
                }
                else {
                    std::cerr << "Invalid OSC termination at position " << i << std::endl;
                    return false;
                }
            }
            // Otherwise stay in OSC state
            break;
        }

        i++;
    }

    // Check if we ended in the middle of a sequence
    if (state != State::TEXT) {
        std::cerr << "Unterminated ANSI sequence at the end of string" << std::endl;
        return false;
    }

    return true;
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
