#include "LoggingHelpers.h"
#include "StringHelpers.h"
#include <filesystem>
#include <chrono>

using namespace std;

#define PAD(n, c) string(n, c)
size_t nextWhitespace(const std::string& s, size_t offset) { return s.find_first_of(" \t\n\r\f\v", offset); }
size_t nextNonWhitespace(const std::string& s, size_t offset) { return s.find_first_not_of(" \t\n\r\f\v", offset); }

namespace LOG
{
    int64_t gnVerbosityLevel = 1;
    Logger gLogger; // singleton global logger
    thread_local LogStream gLogOut(cout);       // per thread stream
    thread_local LogStream gLogErr(std::cerr);  // per thread stream
//    thread_local char LogStreamBuf::m_buffer[LogStreamBuf::BUFFER_SIZE];


    bool LogStreamBuf::isCompleteAnsiContent(const std::string& content)
    {
        enum State {
            Normal,
            EscapeStart,
            CSI,
            OSC,
            Charset
        };

        State state = Normal;

        for (size_t i = 0; i < content.size(); ++i)
        {
            char c = content[i];

            switch (state)
            {
            case Normal:
                if (c == '\x1B') {
                    state = EscapeStart;
                }
                break;

            case EscapeStart:
                if (c == '[') {
                    state = CSI;   // CSI sequence
                }
                else if (c == ']') {
                    state = OSC;   // OSC sequence
                }
                else if (c == '(' || c == ')' || c == '*' || c == '+') {
                    state = Charset; // 3-byte charset selector
                }
                else {
                    // Single-char escapes like ESCc, ESC7, etc.
                    state = Normal;
                }
                break;

            case CSI:
                // CSI runs until a final byte 0x40–0x7E
                if (c >= 0x40 && c <= 0x7E) {
                    state = Normal;
                }
                break;

            case OSC:
                // OSC ends with BEL or ST (ESC \)
                if (c == '\x07') {
                    state = Normal;
                }
                else if (c == '\x1B' && i + 1 < content.size() && content[i + 1] == '\\') {
                    i++;
                    state = Normal;
                }
                break;

            case Charset:
                // Charset designators are only 2 more bytes after ESC
                state = Normal;
                break;
            }
        }

        // Incomplete if we're not back in Normal
        return state == Normal;
    }

    void usToDateTime(int64_t us, string& date, string& time, int64_t precision)
    {
        time_t seconds = us / 1000000;
        int64_t remainingus = us % 1000000;
        std::tm* timeInfo = std::localtime(&seconds);
        std::ostringstream ossdate;
        ossdate << std::put_time(timeInfo, "%Y/%m/%d");
        date = ossdate.str();

        std::ostringstream osstime;
        osstime << std::put_time(timeInfo, "%H:%M:%S");
        // Apply precision to microseconds
        if (precision > 6) precision = 6;  // Max precision is 6 digits for microseconds
        if (precision <= 0) precision = 1; // Minimum precision is 1 digit

        // Calculate the divisor to truncate microseconds to desired precision
        int64_t divisor = 1;
        for (int i = 0; i < (6 - precision); i++) {
            divisor *= 10;
        }
        int64_t truncated_us = remainingus / divisor;

        osstime << "." << std::setfill('0') << std::setw(precision) << truncated_us;
        time = osstime.str();
    }

    std::string usToElapsed(int64_t us, int64_t precision)
    {
        int64_t hours = us / 3600000000ULL;
        us %= 3600000000ULL;  // Remove hours from us
        int64_t minutes = us / 60000000ULL;
        us %= 60000000ULL;    // Remove minutes from us
        int64_t seconds = us / 1000000ULL;
        us %= 1000000ULL;     // Remove seconds from us (now us contains only microseconds)

        std::ostringstream oss;
        if (hours > 0)
        {
            oss << hours << ":";
            oss << std::setfill('0') << std::setw(precision) << minutes << ":";
            oss << std::setfill('0') << std::setw(precision) << seconds << ".";
        }
        else if (minutes > 0)
        {
            oss << minutes << ":";
            oss << std::setfill('0') << std::setw(precision) << seconds << ".";
        }
        else
        {
            oss << seconds << ".";
        }

        // Apply precision to microseconds
        if (precision > 6) precision = 6;  // Max precision is 6 digits for microseconds
        if (precision <= 0) precision = 1; // Minimum precision is 1 digit

        // Calculate the divisor to truncate microseconds to desired precision
        int64_t divisor = 1;
        for (int i = 0; i < (6 - precision); i++) {
            divisor *= 10;
        }
        int64_t truncated_us = us / divisor;

        oss << std::setfill('0') << std::setw(precision) << truncated_us;
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

    void Logger::setFilter(const std::string& sFilter)
    {
        std::lock_guard<std::mutex> loggerLock(logEntriesMutex);
        logFilteredEntries.clear();

        if (sFilter == logFilter)
        {
            return;
        }

        for (const auto& entry : logEntries)
        {
            if (SH::Contains(entry.text, sFilter, false))
                logFilteredEntries.push_back(entry);
        }

        logFilter = sFilter;
    }


    void Logger::addEntry(std::string text)
    {
        std::lock_guard<std::mutex> loggerLock(logEntriesMutex);
        while (logEntries.size() > kQueueSize)
            logEntries.pop_front();


        assert(text[0] != 0);
        size_t start = 0;
        while (start < text.length() && (text[start] == '\n' || text[start] == '\r'))
            start++;
        int64_t end = text.length();
        while (end > 0 && (text[end] == '\n' || text[end] == '\r' || text[end] == 0))
            end--;

        text = text.substr(start, end - start + 1);

        if (!text.empty())  // do we add an entry if it's just a newline?
        {
            LogEntry e(text);

#ifdef _DEBUG
            validateAnsiSequences(text);
#endif
            e.threadID = std::this_thread::get_id();
            e.counter = logTotalCounter++;

            if (!logFilter.empty() && SH::Contains(text, logFilter, false))
                logFilteredEntries.push_back(e);

            logEntries.emplace_back(std::move(e));
        }
    }


    bool Logger::getEntries(uint64_t startingEntry, size_t count, std::deque<LogEntry>& outEntries) const
    {
        std::lock_guard<std::mutex> lock(logEntriesMutex);
        outEntries.clear();

        if (logEntries.empty())
            return true;

        tLogEntries::const_iterator it;
        tLogEntries::const_iterator itEnd;


        if (logFilter.empty())
        {
            it = logEntries.begin();
            itEnd = logEntries.end();
        }
        else
        {
            it = logFilteredEntries.begin();
            itEnd = logFilteredEntries.end();
        }


        uint64_t startCount = 0;
        while (it != itEnd && startCount < startingEntry)
        {
            it++;
            startCount++;
        }

        while (it != itEnd && outEntries.size() < count)
        {
            outEntries.push_back(*it);
            it++;
        }

        return true;
    }

   bool Logger::tail(size_t count, std::deque<LogEntry>& outEntries) const
    {
        std::lock_guard<std::mutex> lock(logEntriesMutex);

        outEntries.clear();

        if (logEntries.empty())
            return true;

        tLogEntries::const_reverse_iterator it;
        tLogEntries::const_reverse_iterator itEnd;


        if (logFilter.empty())
        {
            it = logEntries.rbegin();
            itEnd = logEntries.rend();

            while (outEntries.size() < count && it != itEnd)
            {
                if (SH::Contains((*it).text, logFilter, false))
                {
                    outEntries.push_front(*it);
                }
                it++;
            }
        }
        else
        {
            it = logFilteredEntries.rbegin();
            itEnd = logFilteredEntries.rend();

            while (outEntries.size() < count && it != itEnd)
            {
                outEntries.push_front(*it);
                it++;
            }
        }

        return true;
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

Table::Style Table::kLeftAlignedStyle = Table::Style(COL_RESET, LEFT);
Table::Style Table::kRightAlignedStyle = Table::Style(COL_RESET, RIGHT);
Table::Style Table::kCenteredStyle = Table::Style(COL_RESET, CENTER);
Table::Style Table::kDefaultStyle = Table::Style(AnsiCol(0xFFFFFFFF), LEFT);

const size_t kMinCellWidth = 3;


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

Table::tOptionalStyle Table::GetColStyle(size_t col_count, size_t col_num)
{
    if (col_num > col_count)
    {
        return nullopt;
    }

    if (col_count < colCountToColStyles.size() || colCountToColStyles.empty())
    {
        return nullopt;
    }

    if (col_num > colCountToColStyles[col_count].size() || colCountToColStyles[col_count].empty())
    {
        return nullopt;
    }

    return colCountToColStyles[col_count][col_num];
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
        return kDefaultStyle;

    if (col >= mRows[row].size())
        return kDefaultStyle;

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

    return kDefaultStyle;
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
    if (row >= mRows.size())
        return {};

    if (mRows[row].size() < col)
        return {};

    tCellArray get5row = mRows[row];
    Table::Cell cell = mRows[row][col];
    if (!cell.style.has_value())
        cell.style = GetStyle(col, row);

    return cell;
}


size_t Table::GetRowCount() const
{
    return mRows.size();
}

size_t Table::Cell::MinWidth() const
{
    size_t minw = kMinCellWidth;

    if (style.has_value())
    {
        Style st = style.value();

        if (st.wrapping == NO_WRAP)
        {
            return s.length() + st.padding * 2;
        }

        // WORD_WRAP
        size_t start = 0;
        size_t end = 0;
        size_t minw = kMinCellWidth;
        // Find the longest word
        do
        {
            start = nextNonWhitespace(s, start); 
            end = nextWhitespace(s, start);
            if (end == string::npos)
                end = s.length();
            minw = std::max<size_t>(minw, end - start);
            start = end+1;
        } while (end < s.length());

        return minw + st.padding * 2;
    }

    return s.length() + kDefaultStyle.padding*2;
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

std::tuple<std::string, std::string, std::string>
SplitAnsiWrapped(const std::string& input)
{
    enum State {
        Normal,
        EscapeStart,
        CSI,
        OSC,
        Charset
    };

    std::string leading, plain, trailing;
    State state = Normal;
    size_t i = 0;

    // --- Step 1: Parse leading ANSI sequences ---
    for (; i < input.size(); ++i) {
        char c = input[i];

        switch (state) {
        case Normal:
            if (c == '\x1B') {
                leading.push_back(c);
                state = EscapeStart;
            }
            else {
                // First non-ANSI char ? done with leading
                goto plain_loop;
            }
            break;

        case EscapeStart:
            leading.push_back(c);
            if (c == '[') {
                state = CSI;
            }
            else if (c == ']') {
                state = OSC;
            }
            else if (c == '(' || c == ')' || c == '*' || c == '+') {
                state = Charset;
            }
            else {
                state = Normal; // single char escape
            }
            break;

        case CSI:
            leading.push_back(c);
            if (c >= 0x40 && c <= 0x7E) {
                state = Normal;
            }
            break;

        case OSC:
            leading.push_back(c);
            if (c == '\x07') {
                state = Normal;
            }
            else if (c == '\x1B' && i + 1 < input.size() && input[i + 1] == '\\') {
                leading.push_back(input[++i]);
                state = Normal;
            }
            break;

        case Charset:
            leading.push_back(c);
            state = Normal;
            break;
        }
    }

plain_loop:
    // --- Step 2: Capture plain text until first trailing ANSI ---
    for (; i < input.size(); ++i) {
        if (input[i] == '\x1B') {
            // Found start of trailing ANSI
            break;
        }
        plain.push_back(input[i]);
    }

    // --- Step 3: Everything else is trailing ANSI ---
    for (; i < input.size(); ++i) {
        trailing.push_back(input[i]);
    }

    return { leading, plain, trailing };
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

#ifdef _DEBUG
//    assert(!ContainsAnsiSequences(s));  // let's make it so cells can only have one style and not embedded in text

    for (auto c : s)
    {
        assert(c != '\0');
    }
#endif

}

size_t Table::Cell::RowCount(size_t width) const
{
    size_t visLength = VisLength(s);
    assert(visLength == s.length());

    if (visLength <= width || width < kMinCellWidth)
        return 1;

    if (!style.has_value())
        return 1;

    Style use_style = style.value();
    if (use_style.wrapping == NO_WRAP)
        return 1;

    if (use_style.wrapping == CHAR_WRAP)
    {
        size_t rows = (visLength + width - 1) / width;  // ceiling
        return rows;
    }

    tStringList words;

    size_t start = 0;
    size_t end = 0;
    do
    {
        start = nextNonWhitespace(s, start); // start with first non-whitespace
        end = nextWhitespace(s, start);
        words.push_back(s.substr(start, end - start));
        start = end + 1;
    } while (end < s.length());


    return -3;
}

tStringArray Table::Cell::GetLines(size_t width) const
{
    tOptionalStyle use_st = style;
    if (use_st == nullopt)
        use_st = kDefaultStyle;

    // if text fits in width or there's no wrapping just return a single entry
    if (s.empty() || use_st.value().wrapping == NO_WRAP)
        return tStringArray{ s };

    tStringArray rows;

    if (use_st.value().wrapping == CHAR_WRAP)
    {
        size_t start = 0;
        size_t end = 0;
        string sLine;
        do
        {
            if (sLine.length() == width || s[end] == '\n')
            {
                rows.push_back(sLine);
                sLine.clear();
                start = end + 1;
            }
            else
                sLine += s[end];

            end++;
        } while (end < s.length());

        if (!sLine.empty())
            rows.push_back(sLine);
    }
    else
    {
        assert(use_st.value().wrapping == WORD_WRAP);
        size_t start = 0;
        size_t end = 0;
        do
        {
            start = nextNonWhitespace(s, start); // start with first non-whitespace
            end = nextWhitespace(s, start);
            rows.push_back(s.substr(start, end - start));
            start = end;
        } while (end < s.length());
    }

    return rows;
}


string Table::StyledOut(const string& s, size_t width, tOptionalStyle style)
{
    if (style == nullopt)       // no style passed in and no member style
    {
        if (s.length() > width)
            return Substring(s, width); // however many will fit

        return s + PAD(width - s.length(), ' '); // pad out however many remaining spaces
    }


    uint8_t alignment = style.value().alignment;
    uint8_t padding = style.value().padding;
    char padchar = style.value().padchar;

    // if no room to pad, disable
    if (VisLength(s) >= width)
        padding = 0;

    if (width < padding * 2)    // if not enough space to draw anything
        return style.value().color + PAD(width, padchar) + COL_RESET;

    string use_color;
    
    if (style.value().color != COL_CUSTOM_STYLE)
        use_color = style.value().color;




    size_t remaining_width = width - padding * 2;

    string sOut = Substring(s, remaining_width);
    string sStyled = PAD(padding, padchar) + sOut + PAD(padding, padchar);

    size_t visOutLen = VisLength(sStyled);

    //assert(visOutLen <= remaining_width);


    if (alignment == Table::CENTER)
    {
        size_t left_pad = (width - visOutLen) / 2;
        size_t right_pad = (width - left_pad - visOutLen);
        return use_color + PAD(left_pad, padchar) + sStyled + PAD(right_pad, padchar) + COL_RESET;
    }
    else if (alignment == Table::RIGHT)
    {
        return use_color + PAD(width - visOutLen, padchar) + sStyled + COL_RESET;
    }

    return use_color + sStyled + PAD(width - visOutLen, padchar) + COL_RESET;
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
Table::tColCountToColWidth Table::GetMinColWidths()
{
    tColCountToColWidth widths;

    size_t row_num = 0;
    for (const auto& row : mRows)
    {
        size_t cols = row.size();
        size_t col_num = 0;

        for (const auto& cell : row)
        {
            if (widths[cols].size() < cols)    // make sure there are enough entries for each column
                widths[cols].resize(cols);
            widths[cols][col_num] = std::max<size_t>(widths[cols][col_num], cell.MinWidth());

            col_num++;
        }
        row_num++;
    }

    return widths;
}

void Table::SetRenderWidth(size_t w)
{
    if (w != renderWidth)
        bLayoutNeedsUpdating = true;

    renderWidth = w;
}

bool Table::SetColWidth(size_t col_count, size_t col_num, size_t width)
{
    if (colCountToColWidths.size() < col_count)
    {
        assert(false);
        return false;
    }

    if (colCountToColWidths[col_count].size() < col_num)
    {
        assert(false);
        return false;
    }

    colCountToColWidths[col_count][col_num] = width;

    return true;
}

bool Table::AutosizeColumns()
{
    colCountToColWidths = GetMinColWidths();
    if (renderWidth == 0)
        renderWidth = GetTableMinWidth();

    size_t totalWidthAvailable = renderWidth - (VisLength(borders[LEFT]) + VisLength(borders[RIGHT]));

    if (totalWidthAvailable > 0)
    {
        for (auto& colwidths : colCountToColWidths)
        {
            size_t cols = colwidths.first; 
            size_t colWidthAvailable = totalWidthAvailable - (cols - 1) * VisLength(borders[CENTER]);    // subtract space needed for separators
            int64_t remainingWidth = (int64_t)colWidthAvailable;

            size_t avgColWidthForCount = remainingWidth / cols;

            for (size_t col_num = 0; col_num < cols; col_num++)
            {
                tOptionalStyle optStyle = GetColStyle(cols, col_num);
                if (optStyle.has_value() && optStyle.value().space_allocation > 0.0f)
                {
                    size_t w = (size_t) ((float)(colWidthAvailable) * optStyle.value().space_allocation);
                    colCountToColWidths[cols][col_num] = w;
                }

                if (colCountToColWidths[cols][col_num] > (size_t)remainingWidth)
                {
                    size_t cols_remaining = cols - col_num;
                    size_t w = remainingWidth / cols_remaining;
                    colCountToColWidths[cols][col_num] = w;
                }

                remainingWidth -= colCountToColWidths[cols][col_num];
                assert(remainingWidth >= 0);
            }

            assert(remainingWidth >= 0);
            if (remainingWidth > 0)
                colwidths.second[colwidths.second.size() - 1] += remainingWidth;

/*
            usedWidth += (cols - 1) * VisLength(borders[CENTER]);   // separator between each column
            for (auto w : colwidths.second)
                usedWidth += w;

            size_t leftoverWidth = totalWidthAvailable - usedWidth;
            size_t pad = (leftoverWidth) / cols;
            if (pad > 0)
            {
                for (auto& w : colwidths.second)
                {
                    w += pad;
                    leftoverWidth -= pad;
                }

                // any leftover add to last column
                if (leftoverWidth > 0)
                    colwidths.second[colwidths.second.size() - 1] += leftoverWidth;
            }*/
        }
    }

    bLayoutNeedsUpdating = false;

    return true;
}

size_t Table::GetTableMinWidth()
{
    tColCountToColWidth widths = GetMinColWidths();

    size_t sepLen = VisLength(borders[Table::CENTER]);
    size_t leftBorderLen = VisLength(borders[Table::LEFT]);
    size_t rightBorderLen = VisLength(borders[Table::RIGHT]);

    size_t minWidth = 0;
    for (const auto& cols : widths)
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

/*size_t Table::GetTableMinWidthForColCount(size_t col_count)
{
    AutosizeColumns();
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

*/

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
    // simplest case for commonly called single char
    if (s.length() == 1)
        return string(w, s[0]);

    auto [start, text, end] = SplitAnsiWrapped(s);

    size_t visLen = text.length();



    int64_t repeatedW = w;
    string sOut;
    while (repeatedW > 0)
    {
        sOut +=  text;
        repeatedW -= visLen;
    }

    // if the text sequence is multiple characters it may have gone over, so trim it
    sOut = sOut.substr(0, w);

    return start + sOut + end;
};

/*string Table::ToString(size_t width)
{

}*/

void Table::DrawRow(size_t row_num, ostream& os)
{
    if (row_num > mRows.size())
    {
        assert(false);
        return;
    }

    const Table::tCellArray& row = mRows[row_num];
    size_t linecount = 0;
    size_t cols = row.size();
    std::vector< tStringArray > cellColumns;
    std::vector< Style > cellStyles;
    for (size_t col = 0; col < cols; col++)
    {
        const auto& cell = row[col];
        Style cellStyle = GetStyle(col, row_num);
        size_t w = colCountToColWidths[cols][col] - cellStyle.padding * 2;

        tStringArray lines = cell.GetLines(w);
        linecount = std::max<size_t>(lines.size(), linecount);
        cellColumns.push_back(std::move(lines));
        cellStyles.push_back(cellStyle);
    }

    for (size_t line_num = 0; line_num < linecount; line_num++)
    {
        size_t cursor = 0;
        size_t nEndDraw = renderWidth - VisLength(borders[Table::RIGHT]);

        string separator = borders[Table::CENTER];

        // draw left border
        os << Table::kDefaultStyle << borders[Table::LEFT] << COL_RESET;
        cursor += VisLength(borders[Table::LEFT]);
        

        for (size_t col = 0; col < cellColumns.size(); col++)
        {
            bool bLastColumnInRow = (col == cols - 1);
            if (line_num < cellColumns[col].size()) // if this cell has text on this line
            {
                string unstyled = cellColumns[col][line_num];
                string styled = StyledOut(unstyled, colCountToColWidths[cols][col], cellStyles[col]);
                os << cellStyles[col] << styled << kDefaultStyle;
                cursor += VisLength(styled);
            }
            else
            {
                os << PAD(colCountToColWidths[cols][col], kDefaultStyle.padchar);
                cursor += colCountToColWidths[cols][col];
            }

            // Output a separator for all but last column
            if (!bLastColumnInRow /*&& cursor < nEndDraw*/)
            {
                os << separator << kDefaultStyle;
                cursor += VisLength(separator);
            }
            else
            {
                if (cursor < nEndDraw)
                    os << cellStyles[col] << PAD(nEndDraw - cursor, kDefaultStyle.padchar) << kDefaultStyle;
            }
        }

        // Draw right border
        os << borders[Table::RIGHT] << "\n";
    }
};

ostream& operator <<(ostream& os, Table& tableOut)
{
//    tableOut.AutosizeColumns();

    if (tableOut.mRows.empty())
        return os;

    // reset any previous color
    os << COL_RESET;

    if (tableOut.bLayoutNeedsUpdating)
    {
        tableOut.AutosizeColumns();
    }

    size_t tableMinWidth = tableOut.GetTableMinWidth();
    size_t renderWidth = tableOut.renderWidth;

    // Draw top border
    if (!tableOut.borders[Table::TOP].empty())
    {
        os << RepeatString(tableOut.borders[Table::TOP], renderWidth) << COL_RESET << "\n";
    }

    for (size_t row_num = 0; row_num < tableOut.mRows.size(); row_num++)
    {
        tableOut.DrawRow(row_num, os);
    }

    /*

    // Now print each row based on column widths
    size_t row_num = 0;
    for (const auto& row : tableOut.mRows)
    {
        size_t cursor = 0;
        size_t cols = row.size();

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

        bool bDrawRightColumn = true;
        for (size_t col_num = 0; col_num < cols; col_num++)
        {
            bool bLastColumnInRow = (col_num == cols - 1);

            size_t colWidth = tableOut.colCountToColWidths[cols][col_num];
            size_t nDrawWidth = colWidth;

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
                    nDrawWidth = colWidth;
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
            bDrawRightColumn = cursor <= nEndDraw;
        }

        // Draw right border
        if (bDrawRightColumn)
            os << tableOut.borders[Table::RIGHT];
        os << COL_RESET << "\n";

        row_num++;
    }*/

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
            else if (c >= 0x28 && c <= 0x2B) {
                // Charset designation: ESC (, ), * or +
                state = State::INTERMEDIATE;
            }
            else if (c >= 0x40 && c <= 0x5F) {
                // 2-character escape sequence
                state = State::TEXT;
            }
            else {
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
            else 
            {
                std::cerr << "Invalid character in parameter at position " << i << std::endl;

                int64_t starti = (int64_t)i - 64;
                if (starti < 0)
                    starti = 0;

                DumpMemoryToCout((uint8_t*)input.data() + starti, 256, 0, 32);
                return false;
            }
            break;

        case State::INTERMEDIATE:
            if (c >= 0x20 && c <= 0x2F) {
                // Stay in INTERMEDIATE state
            }
            else if (c >= 0x30 && c <= 0x7E) {
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
