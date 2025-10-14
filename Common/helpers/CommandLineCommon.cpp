#include <string>
#include "CommandLineCommon.h"
#include "LoggingHelpers.h"
#include "StringHelpers.h"
#include <list>
#include <fstream>
#include <assert.h>
#include <sstream>


#ifdef _WIN64
#include <Windows.h>
#endif

using namespace std;

string CLP::appPath;
string CLP::appName;


size_t CLP::ZAttrib::FromAnsi(const uint8_t* pChars)
{
    if (!pChars || pChars[0] != '\x1b')
        return 0;

    size_t pos = 0;
    bool anyConsumed = false;

    while (pChars[pos] == '\x1b')
    {
        char next = pChars[pos + 1];
        if (next == '(')
        {
            // DEC G0 charset
            if (pChars[pos + 2] == '0')  // line drawing
                dec_line = true;
            else if (pChars[pos + 2] == 'B') // normal ASCII
                dec_line = false;
            else
                return anyConsumed ? pos : 0; // unknown charset

            pos += 3;
            anyConsumed = true;
        }
        else if (next == '[')
        {
            // CSI sequence (SGR)
            pos += 2;
            int codes[20] = { 0 };
            int codeCount = 0;
            int num = 0;
            bool hasNum = false;

            while (pChars[pos])
            {
                char c = pChars[pos];
                if (c >= '0' && c <= '9')
                {
                    num = num * 10 + (c - '0');
                    hasNum = true;
                }
                else if (c == ';')
                {
                    if (hasNum && codeCount < 20)
                        codes[codeCount++] = num;
                    num = 0;
                    hasNum = false;
                }
                else if (c >= '@' && c <= '~') // final character
                {
                    if (hasNum && codeCount < 20)
                        codes[codeCount++] = num;

                    if (c == 'm')  // SGR
                    {
                        int i = 0;
                        while (i < codeCount)
                        {
                            int code = codes[i++];
                            switch (code)
                            {
                            case 0:
                                SetFG(0xFF, 0xFF, 0xFF, 0xFF); // reset FG to white
                                SetBG(0xFF, 0x00, 0x00, 0x00); // reset BG to black
                                break;

                            case 1: // bold, often increase brightness
                                // Optionally handle
                                break;

                            case 38: // Foreground extended color
                                if (i < codeCount)
                                {
                                    int colorType = codes[i++];
                                    if (colorType == 2 && i + 2 < codeCount) // RGB
                                    {
                                        // RGB color: 38;2;r;g;b
                                        int r = codes[i++];
                                        int g = codes[i++];
                                        int b = codes[i++];
                                        SetFG(0xFF, r, g, b);
                                    }
                                    else if (colorType == 5 && i < codeCount) // 256-color
                                    {
                                        // 256-color: 38;5;n
                                        // Handle 256-color palette if needed
                                        i++; // Skip the color index for now
                                    }
                                }
                                break;

                            case 48: // Background extended color
                                if (i < codeCount)
                                {
                                    int colorType = codes[i++];
                                    if (colorType == 2 && i + 2 < codeCount) // RGB
                                    {
                                        // RGB color: 48;2;r;g;b
                                        int r = codes[i++];
                                        int g = codes[i++];
                                        int b = codes[i++];
                                        SetBG(0xFF, r, g, b);
                                    }
                                    else if (colorType == 5 && i < codeCount) // 256-color
                                    {
                                        // 256-color: 48;5;n
                                        // Handle 256-color palette if needed
                                        i++; // Skip the color index for now
                                    }
                                }
                                break;

                            case 30: SetFG(0xFF, 0x00, 0x00, 0x00); break; // Black
                            case 31: SetFG(0xFF, 0xFF, 0x00, 0x00); break; // Red
                            case 32: SetFG(0xFF, 0x00, 0xFF, 0x00); break; // Green
                            case 33: SetFG(0xFF, 0xFF, 0xFF, 0x00); break; // Yellow
                            case 34: SetFG(0xFF, 0x00, 0x00, 0xFF); break; // Blue
                            case 35: SetFG(0xFF, 0xFF, 0x00, 0xFF); break; // Magenta
                            case 36: SetFG(0xFF, 0x00, 0xFF, 0xFF); break; // Cyan
                            case 37: SetFG(0xFF, 0xFF, 0xFF, 0xFF); break; // White
                            case 39: SetFG(0xFF, 0xFF, 0xFF, 0xFF); break; // Default FG
                            case 40: SetBG(0xFF, 0x00, 0x00, 0x00); break; // BG Black
                            case 41: SetBG(0xFF, 0xFF, 0x00, 0x00); break; // BG Red
                            case 42: SetBG(0xFF, 0x00, 0xFF, 0x00); break; // BG Green
                            case 43: SetBG(0xFF, 0xFF, 0xFF, 0x00); break; // BG Yellow
                            case 44: SetBG(0xFF, 0x00, 0x00, 0xFF); break; // BG Blue
                            case 45: SetBG(0xFF, 0xFF, 0x00, 0xFF); break; // BG Magenta
                            case 46: SetBG(0xFF, 0x00, 0xFF, 0xFF); break; // BG Cyan
                            case 47: SetBG(0xFF, 0xFF, 0xFF, 0xFF); break; // BG White
                            case 49: SetBG(0xFF, 0x00, 0x00, 0x00); break; // Default BG
                            default:
                                if (code >= 90 && code <= 97)
                                {
                                    // Bright foreground colors
                                    static const uint32_t brightFg[8] = {
                                        0xFF808080, 0xFFFF6060, 0xFF60FF60, 0xFFFFFF60,
                                        0xFF6060FF, 0xFFFF60FF, 0xFF60FFFF, 0xFFFFFFFF
                                    };
                                    SetFG(brightFg[code - 90]);
                                }
                                else if (code >= 100 && code <= 107)
                                {
                                    // Bright background colors
                                    static const uint32_t brightBg[8] = {
                                        0xFF808080, 0xFFFF6060, 0xFF60FF60, 0xFFFFFF60,
                                        0xFF6060FF, 0xFFFF60FF, 0xFF60FFFF, 0xFFFFFFFF
                                    };
                                    SetBG(brightBg[code - 100]);
                                }
                                break;
                            }
                        }
                    }

                    pos++; // consume final character
                    anyConsumed = true;
                    break;
                }
                else
                {
                    // Invalid inside CSI
                    return anyConsumed ? pos : 0;
                }
                pos++;
            }
        }
        else
        {
            // Not recognized
            return anyConsumed ? pos : 0;
        }
    }

    return anyConsumed ? pos : 0;
}
/*
size_t CLP::ZAttrib::FromAnsi(const char* pChars)
{
    if (!pChars || pChars[0] != '\x1b')
        return 0;

    size_t pos = 2; // skip ESC and [
    int codes[20] = { 0 }; // Increased array size to handle RGB values
    int codeCount = 0;
    int num = 0;
    bool hasNum = false;

    while (pChars[pos])
    {
        char c = pChars[pos];

        if (c >= '0' && c <= '9')
        {
            num = num * 10 + (c - '0');
            hasNum = true;
        }
        else if (c == ';')
        {
            if (hasNum)
            {
                if (codeCount < 20)
                    codes[codeCount++] = num;
                num = 0;
                hasNum = false;
            }
        }
        else if (c >= '@' && c <= '~') // final character of ANSI sequence
        {
            if (hasNum)
            {
                if (codeCount < 20)
                    codes[codeCount++] = num;
            }

            if (c != 'm')  // Only handle SGR sequences (colors, attributes)
                return pos + 1;

            // Now process codes
            int i = 0;
            while (i < codeCount)
            {
                int code = codes[i++];
                switch (code)
                {
                case 0:
                    SetFG(0xFF, 0xFF, 0xFF, 0xFF); // reset FG to white
                    SetBG(0xFF, 0x00, 0x00, 0x00); // reset BG to black
                    break;

                case 1: // bold, often increase brightness
                    // Optionally handle
                    break;

                case 38: // Foreground extended color
                    if (i < codeCount)
                    {
                        int colorType = codes[i++];
                        if (colorType == 2 && i + 2 < codeCount) // RGB
                        {
                            // RGB color: 38;2;r;g;b
                            int r = codes[i++];
                            int g = codes[i++];
                            int b = codes[i++];
                            SetFG(0xFF, r, g, b);
                        }
                        else if (colorType == 5 && i < codeCount) // 256-color
                        {
                            // 256-color: 38;5;n
                            // Handle 256-color palette if needed
                            i++; // Skip the color index for now
                        }
                    }
                    break;

                case 48: // Background extended color
                    if (i < codeCount)
                    {
                        int colorType = codes[i++];
                        if (colorType == 2 && i + 2 < codeCount) // RGB
                        {
                            // RGB color: 48;2;r;g;b
                            int r = codes[i++];
                            int g = codes[i++];
                            int b = codes[i++];
                            SetBG(0xFF, r, g, b);
                        }
                        else if (colorType == 5 && i < codeCount) // 256-color
                        {
                            // 256-color: 48;5;n
                            // Handle 256-color palette if needed
                            i++; // Skip the color index for now
                        }
                    }
                    break;

                case 30: SetFG(0xFF, 0x00, 0x00, 0x00); break; // Black
                case 31: SetFG(0xFF, 0xFF, 0x00, 0x00); break; // Red
                case 32: SetFG(0xFF, 0x00, 0xFF, 0x00); break; // Green
                case 33: SetFG(0xFF, 0xFF, 0xFF, 0x00); break; // Yellow
                case 34: SetFG(0xFF, 0x00, 0x00, 0xFF); break; // Blue
                case 35: SetFG(0xFF, 0xFF, 0x00, 0xFF); break; // Magenta
                case 36: SetFG(0xFF, 0x00, 0xFF, 0xFF); break; // Cyan
                case 37: SetFG(0xFF, 0xFF, 0xFF, 0xFF); break; // White
                case 39: SetFG(0xFF, 0xFF, 0xFF, 0xFF); break; // Default FG
                case 40: SetBG(0xFF, 0x00, 0x00, 0x00); break; // BG Black
                case 41: SetBG(0xFF, 0xFF, 0x00, 0x00); break; // BG Red
                case 42: SetBG(0xFF, 0x00, 0xFF, 0x00); break; // BG Green
                case 43: SetBG(0xFF, 0xFF, 0xFF, 0x00); break; // BG Yellow
                case 44: SetBG(0xFF, 0x00, 0x00, 0xFF); break; // BG Blue
                case 45: SetBG(0xFF, 0xFF, 0x00, 0xFF); break; // BG Magenta
                case 46: SetBG(0xFF, 0x00, 0xFF, 0xFF); break; // BG Cyan
                case 47: SetBG(0xFF, 0xFF, 0xFF, 0xFF); break; // BG White
                case 49: SetBG(0xFF, 0x00, 0x00, 0x00); break; // Default BG
                default:
                    if (code >= 90 && code <= 97)
                    {
                        // Bright foreground colors
                        static const uint32_t brightFg[8] = {
                            0xFF808080, 0xFFFF6060, 0xFF60FF60, 0xFFFFFF60,
                            0xFF6060FF, 0xFFFF60FF, 0xFF60FFFF, 0xFFFFFFFF
                        };
                        SetFG(brightFg[code - 90]);
                    }
                    else if (code >= 100 && code <= 107)
                    {
                        // Bright background colors
                        static const uint32_t brightBg[8] = {
                            0xFF808080, 0xFFFF6060, 0xFF60FF60, 0xFFFFFF60,
                            0xFF6060FF, 0xFFFF60FF, 0xFF60FFFF, 0xFFFFFFFF
                        };
                        SetBG(brightBg[code - 100]);
                    }
                    break;
                }
            }
            return pos + 1;
        }
        else
        {
            // Invalid character inside ANSI?
            return 0;
        }
        pos++;
    }

    return 0;
}*/

string CLP::ZAttrib::ToAnsi() const
{
    std::ostringstream ansi;

    // Handle DEC line drawing first
    if (dec_line)
        ansi << "\033(0";  // G0 → line drawing
    else
        ansi << "\033(B";  // G0 → ASCII

    // Start the ANSI sequence
    ansi << "\033[";

    bool needSemicolon = false;

    // Check if we need to handle foreground color
    if (a > 0) 
    {
        // Use RGB mode for foreground
        ansi << "38;2;" << static_cast<int>(r) << ";"
            << static_cast<int>(g) << ";"
            << static_cast<int>(b);
        needSemicolon = true;
    }

    // Check if we need to handle background color
    if (ba > 0) 
    {
        if (needSemicolon) 
        {
            ansi << ";";
        }
        // Use RGB mode for background
        ansi << "48;2;" << static_cast<int>(br) << ";"
            << static_cast<int>(bg) << ";"
            << static_cast<int>(bb);
    }

    // End the ANSI sequence
    ansi << "m";

    return ansi.str();
}

#ifdef ENABLE_CLE


namespace CLP
{
    // Styles
    ZAttrib kAttribAppName(GOLD);
    ZAttrib kAttribCaption(BLACK | MAKE_BG(GOLD));
    ZAttrib kAttribSection(CYAN);

    ZAttrib kAttribFolderListBG(MAKE_BG(0xff000044));

    ZAttrib kAttribListBoxBG(MAKE_BG(ORANGE));
    ZAttrib kAttribListBoxEntry(WHITE);
    ZAttrib kAttribListBoxSelectedEntry(BLACK | MAKE_BG(YELLOW));

    ZAttrib kAttribParamListBG(MAKE_BG(0xff4444aa));
    ZAttrib kAttribTopInfoBG(BLACK | MAKE_BG(0xFF333399));

    ZAttrib kAttribHelpBG(0xFF444444FF000000);

    ZAttrib kRawText(WHITE);
    ZAttrib kSelectedText(WHITE | MAKE_BG(0xFF999999));
    ZAttrib kGoodParam(GREEN);
    ZAttrib kBadParam(RED);
    ZAttrib kUnknownParam(YELLOW);

    ZAttrib kAttribError(RED);
    ZAttrib kAttribWarning(ORANGE);

    ZAttrib kAttribScrollbarBG(BLACK|MAKE_BG(0xFF888888));
    ZAttrib kAttribScrollbarThumb(WHITE|MAKE_BG(0xFFBBBBBB));



    HANDLE mhInput;
    HANDLE mhOutput;
    std::vector<CHAR_INFO> originalConsoleBuf;
    CONSOLE_SCREEN_BUFFER_INFO originalScreenInfo;
    CONSOLE_SCREEN_BUFFER_INFO screenInfo;
    bool bScreenInfoInitialized = false;
    bool bScreenInvalid = true;
    COORD gLastCursorPos = { -1, -1 };

    TableWin helpTableWin;

    void SetCursorPosition(COORD coord, bool bForce)
    {
        if (bForce || coord.X != gLastCursorPos.X || coord.Y != gLastCursorPos.Y)
        {
            cout << "\033[" + SH::FromInt(coord.X+1) + "G\033[" + SH::FromInt(coord.Y+1) + "d" << flush;
            gLastCursorPos = coord;
        }
    }

    void InitScreenInfo()
    {
        HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOutput == INVALID_HANDLE_VALUE)
        {
            cerr << "Failed to get standard output handle." << endl;
            return;
        }

        if (!GetConsoleScreenBufferInfo(hOutput, &screenInfo))
        {
            cerr << "Failed to get console info." << endl;
        }

        bScreenInfoInitialized = true;
    }

    void SaveConsoleState()
    {
        originalScreenInfo = screenInfo;
        originalConsoleBuf.resize(originalScreenInfo.dwSize.X * originalScreenInfo.dwSize.Y);
        SMALL_RECT readRegion = { 0, 0, originalScreenInfo.dwSize.X - 1, originalScreenInfo.dwSize.Y - 1 };
        ReadConsoleOutput(mhOutput, &originalConsoleBuf[0], screenInfo.dwSize, { 0, 0 }, &readRegion);
    }

    void RestoreConsoleState()
    {
        SMALL_RECT writeRegion = { 0, 0, originalScreenInfo.dwSize.X - 1, originalScreenInfo.dwSize.Y - 1 };
        WriteConsoleOutput(mhOutput, &originalConsoleBuf[0], originalScreenInfo.dwSize, { 0, 0 }, &writeRegion);
        SetConsoleCursorPosition(mhOutput, originalScreenInfo.dwCursorPosition);
        SetConsoleTextAttribute(mhOutput, originalScreenInfo.wAttributes);
    }

    SHORT ScreenW()
    { 
        if (!bScreenInfoInitialized)
        {
            InitScreenInfo();
        }

        return screenInfo.srWindow.Right - screenInfo.srWindow.Left + 1; 
    }

    SHORT ScreenH() 
    { 
        if (!bScreenInfoInitialized)
        {
            InitScreenInfo();
        }

        return screenInfo.srWindow.Bottom - screenInfo.srWindow.Top + 1;
    }

    void DrawAnsiChar(int64_t x, int64_t y, uint8_t c, ZAttrib ca)
    {
        static ZAttrib lastC;
        SetCursorPosition(COORD((SHORT)x, (SHORT)y));

        string s;
        if (lastC.a != ca.a || lastC.r != ca.r || lastC.g != ca.g || lastC.b != ca.b || lastC.ba != ca.ba || lastC.br != ca.br || lastC.bg != ca.bg || lastC.bb != ca.bb || lastC.dec_line != ca.dec_line)
        {
            s += "\033[38;2;" + SH::FromInt(ca.r) + ";" + SH::FromInt(ca.g) + ";" + SH::FromInt(ca.b) + "m"; //forground color
            s += "\033[48;2;" + SH::FromInt(ca.br) + ";" + SH::FromInt(ca.bg) + ";" + SH::FromInt(ca.bb) + "m"; //background color

            if (lastC.dec_line && !ca.dec_line) // turn dec_line off
                s += DEC_LINE_END;
            else if (!lastC.dec_line && ca.dec_line)    // turn dec_line on
                s += DEC_LINE_START;

            lastC = ca;
        }

        if (c < 32 || lastC.a == 0)
            s += ' ';
        else
            s += c;
        cout << s;
        gLastCursorPos.X++;    // cursor advances after drawing
        if (gLastCursorPos.X > screenInfo.srWindow.Right - screenInfo.srWindow.Left)
        {
            gLastCursorPos.X = 0;
            gLastCursorPos.Y++;
        }
    };


    std::string ExpandEnvVars(const std::string& s)
    {
        DWORD size = ExpandEnvironmentStringsA(s.c_str(), nullptr, 0);
        if (size == 0) 
        {
            return s; // Return original on error
        }

        std::string result(size - 1, '\0'); // size includes null terminator
        DWORD actualSize = ExpandEnvironmentStringsA(s.c_str(), &result[0], size); // actualSize includes null terminator

        if (actualSize == 0 || actualSize > size) 
        {
            return s; // Return original on error
        }

        return result.substr(0, actualSize-1); // strip null
    }

    tKeyValList GetEnvVars()
    {
        tKeyValList keyVals;
        LPCH envStrings = GetEnvironmentStrings();
        if (envStrings)
        {
            LPCH current = envStrings;
            while (*current != '\0')
            {
                std::string envVar(current);

                // Find the '=' separator
                size_t equalPos = envVar.find('=');
                if (equalPos != std::string::npos && equalPos > 0)
                {
                    std::string key = envVar.substr(0, equalPos);
                    std::string value = envVar.substr(equalPos + 1);
                    keyVals.push_back({ key, value });
                }

                current += strlen(current) + 1;
            }

            FreeEnvironmentStrings(envStrings);
        }

        return keyVals;
    }


    bool ConsoleWin::Init(const Rect& r)
    {
        SetArea(r);
        SetVisible();
        mbDone = false;
        mbCanceled = false;
        return true;
    }

    void ConsoleWin::SetArea(const Rect& r)
    {
        assert(r.r > r.l && r.b > r.t);
        mX = r.l;
        mY = r.t;

        if (mWidth != r.w() || mHeight != r.h())
        {
            if (r.w() > 0 && r.h() > 0)
            {
                mBuffer.resize(r.w() * r.h());
                mWidth = r.w();
                mHeight = r.h();
                Clear(mClearAttrib, mbGradient);
            }

        }
    }

    bool ConsoleWin::OnMouse(MOUSE_EVENT_RECORD event)
    {
        return false;
    }


    void ConsoleWin::GetArea(Rect& r)
    {
        r.l = mX;
        r.t = mY;
        r.r = r.l + mWidth;
        r.b = r.t + mHeight;
    }

    void ConsoleWin::GetInnerArea(Rect& r)
    {
        r.l = 0;
        r.t = 0;
        r.r = mWidth;
        r.b = mHeight;

        if (enableFrame[Side::L])
            r.l++;
        if (enableFrame[Side::T])
            r.t++;
        if (enableFrame[Side::R])
            r.r--;
        if (enableFrame[Side::B])
            r.b--;
    }



    void ConsoleWin::DrawClippedAnsiText(const Rect& r, std::string ansitext, bool bWrap, const Rect* pClip)
    {

#ifdef _DEBUG
        validateAnsiSequences(ansitext);
#endif

        if (!pClip)
            pClip = &r;

        int64_t cursorX = r.l;
        int64_t cursorY = r.t;

        ZAttrib attrib(WHITE);

        for (size_t i = 0; i < ansitext.length(); i++)
        {
            size_t skiplength = attrib.FromAnsi((uint8_t*)&ansitext[i]);
            if (skiplength > 0)
            {
                i += skiplength - 1;    // -1 so that the i++ above will be correct
            }
            else
            {
                uint8_t c = ansitext[i];
                if (c == '\n' && bWrap)
                {
                    cursorX = r.l;
                    cursorY++;
                }
                else
                {
                    DrawCharClipped(c, cursorX, cursorY, attrib, pClip);
                    cursorX++;
                }

                if (cursorY > r.b || cursorY > pClip->b) // off the bottom of the viewing area
                    break;
            }
        }
    }


/*    void ConsoleWin::DrawClippedAnsiText(const Rect& r, std::string ansitext, bool bWrap, const Rect* pClip)
    {

#ifdef _DEBUG
        validateAnsiSequences(ansitext);
#endif

        if (!pClip)
            pClip = &r;

        int64_t cursorX = r.l;
        int64_t cursorY = r.t;

        if (cursorX < pClip->l || cursorY < pClip->t || cursorX >= pClip->r || cursorY >= pClip->b)
            return;

        ZAttrib attrib(WHITE);

        for (size_t i = 0; i < ansitext.length(); i++)
        {
            size_t skiplength = attrib.FromAnsi(&ansitext[i]);
            if (skiplength > 0)
            {
                i += skiplength - 1;    // -1 so that the i++ above will be correct
            }
            else
            {
                char c = ansitext[i];
                if (c == '\n' && bWrap)
                {
                    cursorX = r.l;
                    cursorY++;
                }
                else
                {
                    DrawCharClipped(c, cursorX, cursorY, attrib, pClip);
                    cursorX++;
                    if (cursorX >= r.r)
                    {
//                        if (bWrap)
                        {
                            cursorX = (SHORT)r.l;
                            cursorY++;
                        }
                    }
                }

                if (cursorY > r.b || cursorY > pClip->b) // off the bottom of the viewing area
                    break;
            }
        }
    }

    */


    std::string removeANSISequences(const std::string& str)
    {
        std::string result;
        bool inEscapeSequence = false;

        for (uint8_t ch : str)
        {
            if (inEscapeSequence)
            {
                if (ch == 'm')
                {
                    inEscapeSequence = false;
                }
                continue;
            }
            if (ch == '\x1B')
            {
                inEscapeSequence = true;
                continue;
            }
            result.push_back(ch);
        }

        return result;
    }


    Rect ConsoleWin::GetTextOuputRect(std::string text)
    {
        Rect area;
        int64_t x = 0;
        int64_t y = 0;
        ZAttrib attrib;
        for (size_t i = 0; i < text.length(); i++)
        {
            size_t skiplength = attrib.FromAnsi((uint8_t*)&text[i]);
            if (skiplength > 0)
            {
                i += skiplength - 1;    // -1 so that the i++ above will be correct
            }
            else
            {
                uint8_t c = text[i];
                if (c == '\n' /*|| x > mWidth*/)
                {
                    x = 0;
                    y++;
                    area.b = std::max<int64_t>(area.b, y);
                }
                else
                {
                    x++;
                    area.r = std::max<int64_t>(area.r, x);
                }
            }
        }
        return area;
    }

/*    int64_t ConsoleWin::GetTextOutputRows(const std::string& _text, int64_t w)
    {
        string text = StripAnsiSequences(_text);
        if (text.length() == 0)// if no visible text
            return 0;

        int64_t rows = 1;
        int64_t x = 0;
        int64_t y = 1;
        ZAttrib attrib;
        for (size_t i = 0; i < text.length(); i++)
        {
            uint8_t c = text[i];
            if (c == '\n' || x >= w)
            {
                x = 0;
                y++;
                rows = std::max<int64_t>(rows, y);
            }
            else
            {
                x++;
            }
        }
        return rows;
    }
    */

    int64_t ConsoleWin::GetTextOutputRows(const std::string& _text, int64_t w)
    {

        string text = StripAnsiSequences(_text);
        if (text.empty())
            return 0;

        int64_t rows = 1;
        int64_t x = 0;
        for (const auto& c : text)
        {
            if (c == '\n')
            {
                rows++;
                x = 0;
            }
            else
            {
                x++;
                if (x > w)
                {
                    rows++;
                    x = 0;
                }
            }
        }

        return rows;
    }


    void ConsoleWin::GetCaptionPosition(string& caption, ConsoleWin::Position pos, int64_t& x, int64_t& y)
    {
        if (pos == Position::LT || pos == Position::LB) // Left
            x = 0;
        else if (pos == Position::CT || pos == Position::CB) // Center
            x = (mWidth - caption.length()) / 2;
        else
            x = mWidth - caption.length(); // Right

        if (pos == Position::LT || pos == Position::CT || pos == Position::RT)
            y = 0;
        else
            y = mHeight - 1;
    }

    void ConsoleWin::BasePaint()
    {
        // Update display
        if (!mbVisible)
            return;

        Clear(mClearAttrib, mbGradient);

        // Fill 
        Fill(Rect(0, 0, mWidth, mHeight), mClearAttrib, mbGradient);

        if (enableFrame[Side::T])
            Fill(Rect(0, 0, mWidth, 1), kAttribCaption, mbGradient);   // top

        if (enableFrame[Side::L])
            Fill(Rect(0, 0, 1, mHeight), kAttribCaption, mbGradient);   // left

        if (enableFrame[Side::B])
            Fill(Rect(0, mHeight - 1, mWidth, mHeight), kAttribCaption, mbGradient);    // bottom

        if (enableFrame[Side::R])
            Fill(Rect(mWidth - 1, 0, mWidth, mHeight), kAttribCaption, mbGradient);    // right  

        // draw all captions
        for (uint8_t pos = 0; pos < Position::MAX_POSITIONS; pos++)
        {
            string caption = positionCaption[pos];
            if (!caption.empty())
            {
                int64_t x = 0;
                int64_t y = 0;
                GetCaptionPosition(caption, Position(pos), x, y);
                DrawClippedText(Rect(x, y, mWidth, mHeight), caption, kAttribCaption, false);
            }
        }
    }


    void ConsoleWin::RenderToBackBuf(tConsoleBuffer& backBuf)
    {
        int64_t dr = ScreenW();
        int64_t db = ScreenH();

        for (int64_t sy = 0; sy < mHeight; sy++)
        {
            int64_t dy = sy + mY;
            if (dy >= 0 && dy < db) // clip top and bottom
            {
                for (int64_t sx = 0; sx < mWidth; sx++)
                {
                    int64_t dx = sx + mX;
                    if (dx >= 0 && dx < dr)
                    {
                        int64_t sindex = (sy * mWidth) + sx;
                        int64_t dindex = (dy * dr) + dx;
                        backBuf[dindex] = mBuffer[sindex];
                    }
                }
            }

        }
    }


    void ConsoleWin::Clear(ZAttrib attrib, bool bGradient)
    {
        mClearAttrib = attrib;
        mbGradient = bGradient;
        for (size_t i = 0; i < mBuffer.size(); i++)
        {
            mBuffer[i].c = 0;
            mBuffer[i].attrib = mClearAttrib;
        }
    }

    void ConsoleWin::SetEnableFrame(bool _l, bool _t, bool _r, bool _b)
    {
        enableFrame[Side::L] = _l;
        enableFrame[Side::T] = _t;
        enableFrame[Side::R] = _r;
        enableFrame[Side::B] = _b;
    }


    void ConsoleWin::ClearCaptions()
    {
        for (uint8_t pos = 0; pos < ConsoleWin::Position::MAX_POSITIONS; pos++)
            positionCaption[pos].clear();
    }

    void ConsoleWin::Fill(uint8_t c, const Rect& r, ZAttrib attrib)
    {
        for (int64_t y = r.t; y < r.b; y++)
        {
            for (int64_t x = r.l; x < r.r; x++)
            {
                size_t offset = y * mWidth + x;
                mBuffer[offset].c = c;
                mBuffer[offset].attrib = attrib;
            }
        }
    }


    void ConsoleWin::Fill(const Rect& r, ZAttrib attrib, bool bGradient)
    {
        for (int64_t y = r.t; y < r.b; y++)
        {
            // playing with gradient fill
            if (bGradient)
            {
                attrib.br = (uint64_t)(attrib.br * 0.96);
                attrib.bg = (uint64_t)(attrib.bg * 0.96);
                attrib.bb = (uint64_t)(attrib.bb * 0.96);
            }
            for (int64_t x = r.l; x < r.r; x++)
            {
                size_t offset = y * mWidth + x;
                mBuffer[offset].attrib = attrib;
            }
        }
    }

    void ConsoleWin::Fill(ZAttrib attrib, bool bGradient)
    {
        return Fill(Rect(0, 0, mWidth, mHeight), attrib, bGradient);
    }

    void ConsoleWin::DrawCharClipped(uint8_t c, int64_t x, int64_t y, ZAttrib attrib, const Rect* pClip)
    {
        if (pClip)
        {
            if (x < pClip->l || y < pClip->t || x >= pClip->r || y >= pClip->b)
                return;
        }

        if (attrib.FG() == attrib.BG())  // would be invisible
        {
            // invert fg
            attrib.r = ~attrib.r;
            attrib.g = ~attrib.g;
            attrib.b = ~attrib.b;
        }

        size_t offset = y * mWidth + x;
        DrawCharClipped(c, (int64_t)offset, attrib);
    }

    void ConsoleWin::DrawCharClipped(uint8_t c, int64_t offset, ZAttrib attrib)
    {
        if (offset >= 0 && offset < (int64_t)mBuffer.size())    // clip
        {
            mBuffer[offset].c = c;
            if (GET_BA(attrib) > 0)
                mBuffer[offset].attrib = attrib;
            else
                mBuffer[offset].attrib.SetFG(attrib.FG());  // if background is transparent, only set FG color
        }
    }



    int64_t ConsoleWin::DrawFixedColumnStrings(int64_t x, int64_t y, tStringArray& strings, vector<size_t>& colWidths, int64_t padding, tAttribArray attribs, const Rect* pClip)
    {
        assert(strings.size() == colWidths.size() && colWidths.size() == attribs.size());

        // compute how many rows is required to draw all strings in the fixed columns
        int64_t rowsRequired = 0;
        for (int i = 0; i < strings.size(); i++)
        {
            rowsRequired = std::max<int64_t>(rowsRequired, GetTextOutputRows(strings[i], colWidths[i]));
        }

        // If we have a background color for the row, use the clipping rect to fill it in
        if (pClip)
        {
            int64_t rowWidth = 0;
            for (int i = 0; i < colWidths.size(); i++)
                rowWidth = colWidths[i];

            Rect rowArea(pClip->l, y, pClip->r, y + rowsRequired);
            if (rowArea.b > pClip->b)
                rowArea.b = pClip->b;

            if (attribs[0].ba > 0)
                Fill(rowArea, attribs[0]);
        }

        for (int i = 0; i < strings.size(); i++)
        {
            DrawClippedText(Rect(x, y, x+colWidths[i], mHeight), strings[i], attribs[i], true, pClip);
            x += colWidths[i]+padding;
        }

        return rowsRequired;
    }

    void ConsoleWin::DrawClippedText(const Rect& r, std::string text, ZAttrib attributes, bool bWrap, const Rect* pClip)
    {
        COORD cursor((SHORT)r.l, (SHORT)r.t);

        for (size_t textindex = 0; textindex < text.size(); textindex++)
        {
            uint8_t c = text[textindex];
            if (c == '\n' && bWrap)
            {
                cursor.X = (SHORT)r.l;
                cursor.Y++;
            }
            else
            {
                DrawCharClipped(c, cursor.X, cursor.Y, attributes, pClip);
            }

            cursor.X++;
            if (cursor.X >= r.r)
            {
                if (bWrap)
                {
                    cursor.X = (SHORT)r.l;
                    cursor.Y++;
                }
                else
                    break;
            }
        }
    }

    bool InfoWin::Init(const Rect& r)
    {
        bAutoScrollbar = true;
        mTopVisibleRow = 0;
        return ConsoleWin::Init(r);
    }


    void InfoWin::Paint(tConsoleBuffer& backBuf)
    {
        if (!mbVisible)
            return;

        ConsoleWin::BasePaint();

        int64_t firstDrawRow = mTopVisibleRow - 1;

        Rect drawArea;
        GetInnerArea(drawArea);
        Rect textArea = GetTextOuputRect(mText);
        int64_t shiftX = 1;// drawArea.l;
        int64_t shiftY = /*drawArea.t*/ - firstDrawRow;     // shift the text output so that the in-view text starts with the firstdrawrow

        textArea.l += shiftX;
        textArea.r += shiftX;
        textArea.t += shiftY;
        textArea.b += shiftY;


        bool bDrawScrollbar = false;
        if (bAutoScrollbar)
        {
//            int64_t nRows = GetTextOutputRows(mText, drawArea.w());
            int64_t nRows = textArea.h();
            Rect sb(drawArea.r - 1, drawArea.t, drawArea.r, drawArea.b);
            bDrawScrollbar = nRows > drawArea.h();
            DrawScrollbar(sb, 0, nRows - drawArea.h() - 1, mTopVisibleRow, kAttribScrollbarBG, kAttribScrollbarThumb);
            drawArea.r--;   // adjust draw area for text if scrollbar is visible
        }

        DrawClippedAnsiText(textArea, mText, true, &drawArea);

        ConsoleWin::RenderToBackBuf(backBuf);
    }

    void TableWin::SetVisible(bool bVisible)
    {
        mbVisible = bVisible; 
        mRenderedW = 0;
        mRenderedH = 0;
    }


    void TableWin::Paint(tConsoleBuffer& backBuf)
    {
        if (!mbVisible)
            return;

        if (mWidth != mRenderedW || mHeight != mRenderedH)
        {
            mRenderedW = mWidth;
            mRenderedH = mHeight;

            Rect drawArea;
            GetInnerArea(drawArea);
            drawArea.r--;   // allow for scrollbar
            mTable.SetRenderWidth(drawArea.w());
            mText = mTable;
        }

        return InfoWin::Paint(backBuf);
    }


    void ConsoleWin::DrawScrollbar(const Rect& r, int64_t min, int64_t max, int64_t cur, ZAttrib bg, ZAttrib thumb)
    {
        if (!mbVisible)
            return;

        bool bHorizontal = (r.r - r.l) > (r.b - r.t);
        int64_t sbSize = bHorizontal ? (r.r - r.l) : (r.b - r.t);

        assert(sbSize > 0);

        int64_t scrollRange = max - min + 1;  // 31 scroll positions (0 to 30)
        if (scrollRange <= 1) {
            Fill(r, thumb, false);
            return;
        }

        // Total content size = max scroll position + window size + 1
        // If max scroll is 30 and window shows 48 lines, total content is 30 + 48 + 1 = 79
        int64_t totalContentSize = max + sbSize + 1;  // 30 + 48 + 1 = 79

        // Thumb size = (visible_content / total_content) * scrollbar_size
        int64_t thumbSize = (sbSize * sbSize) / totalContentSize;  // (48 * 48) / 79 ≈ 29
        thumbSize = std::max<int64_t>(1, thumbSize);
        thumbSize = std::min<int64_t>(sbSize - 1, thumbSize);

//        Fill(r, bg, false);
        ZAttrib track(bg);
        track.dec_line = true;

        Rect trackRect(r);
        DrawCharClipped('\x77', trackRect.l, trackRect.t, track);
        DrawCharClipped('\x76', trackRect.l, trackRect.b-1,  track);
        trackRect.t++;
        trackRect.b--;

        Fill('\x78', trackRect, track);

        int64_t trackSize = sbSize - thumbSize;  // 48 - 29 = 19
        if (trackSize <= 0) 
        {
            Fill(r, thumb, false);
            return;
        }

        // Position: cur ranges from 0 to 30, so normalize by dividing by 30
        double fNormalizedValue = (double)(cur - min) / (double)(max - min);  // 14/30 ≈ 0.467
        fNormalizedValue = std::clamp(fNormalizedValue, 0.0, 1.0);

        Rect rThumb;
        if (bHorizontal) {
            rThumb.l = r.l + (int64_t)(fNormalizedValue * trackSize);
            rThumb.r = rThumb.l + thumbSize;
            rThumb.t = r.t;
            rThumb.b = r.b;
        }
        else {
            rThumb.t = r.t + (int64_t)(fNormalizedValue * trackSize);  // 0 + (0.467 * 19) ≈ 8
            rThumb.b = rThumb.t + thumbSize;  // 8 + 29 = 37
            rThumb.l = r.l;
            rThumb.r = r.r;
        }

        Fill('\xb1', rThumb, thumb);
    }


    void InfoWin::UpdateCaptions()
    {
        Rect drawArea;
        GetInnerArea(drawArea);
        int64_t drawHeight = drawArea.b - drawArea.t;

        Rect textArea = GetTextOuputRect(mText);

        if (textArea.h() > drawHeight)
        {
            positionCaption[ConsoleWin::Position::RT] = "Lines (" + SH::FromInt(mTopVisibleRow + 1) + "-" + SH::FromInt(mTopVisibleRow + drawHeight) + "/" + SH::FromInt(textArea.h()) + ")";
            positionCaption[ConsoleWin::Position::RB] = "[UP/DOWN][PAGE Up/Down][HOME/END]";
        }
        else
        {
            positionCaption[ConsoleWin::Position::RT].clear();
        }

        bScreenInvalid = true;
    }

    bool InfoWin::OnMouse(MOUSE_EVENT_RECORD event)
    {
        COORD localcoord = event.dwMousePosition;
        localcoord.X -= (SHORT)mX;
        localcoord.Y -= (SHORT)mY;

        if (event.dwEventFlags == MOUSE_WHEELED)
        {
            SHORT wheelDelta = HIWORD(event.dwButtonState);
            if (wheelDelta < 0)
            {
                return OnKey(VK_DOWN, 4);
            }
            else
            {
                return OnKey(VK_UP, 4);
            }
        }

        return ConsoleWin::OnMouse(event);
    }


    bool InfoWin::OnKey(int keycode, char c)
    {
        bool bHandled = false;
        Rect drawArea;
        GetInnerArea(drawArea);
        int64_t drawHeight = drawArea.b - drawArea.t;

        Rect docArea = GetTextOuputRect(mText);

        if ((keycode >= VK_F1 && keycode <= VK_F24) || keycode == VK_ESCAPE)
        {
            mText.clear();
            SetVisible(false);
            mbDone = true;
            return true;
        }

        if (docArea.h() > drawHeight)
        {
            if (keycode == VK_UP)
            {
                mTopVisibleRow = mTopVisibleRow - 1 - c;
                bHandled = true;
            }
            else if (keycode == VK_DOWN)
            {
                mTopVisibleRow = mTopVisibleRow + 1 + c;
                bHandled = true;
            }
            else if (keycode == VK_HOME)
            {
                mTopVisibleRow = 0;
                bHandled = true;
            }
            else if (keycode == VK_PRIOR)
            {
                mTopVisibleRow -= drawHeight;
                bHandled = true;
            }
            else if (keycode == VK_NEXT)
            {
                mTopVisibleRow += drawHeight;
                bHandled = true;
            }
            else if (keycode == VK_END)
            {
                mTopVisibleRow = docArea.h() - drawHeight;
                bHandled = true;
            }

            if (mTopVisibleRow < 0)
                mTopVisibleRow = 0;
            if (mTopVisibleRow > (docArea.h() - drawHeight))
                mTopVisibleRow = (docArea.h() - drawHeight);
        }

        UpdateCaptions();

        return bHandled;
    }

    bool TextEditWin::OnMouse(MOUSE_EVENT_RECORD event)
    {
        COORD localcoord = event.dwMousePosition;
        localcoord.X -= (SHORT)mX;
        localcoord.Y -= (SHORT)(mY+1);

//        cout << "over:" << localcoord.X << "," << localcoord.Y << " ";

        if (event.dwEventFlags == 0)
        {
            if (event.dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED)
            {
                // left button down
                UpdateCursorPos(localcoord);

                selectionstart = (int)CursorToTextIndex(localcoord);
                selectionend = selectionstart;
                bScreenInvalid = true;
                bMouseCapturing = true;
                return true;
            }
            else if (event.dwButtonState == 0 && bMouseCapturing)
            {
                // left button up
                selectionend = (int)CursorToTextIndex(localcoord);

                if (selectionstart == selectionend)
                {
                    ClearSelection();
                }

                bScreenInvalid = true;
                bMouseCapturing = false;
                return true;
            }
        }
        else if (event.dwEventFlags == MOUSE_MOVED)
        {
            // mouse moved
            if (bMouseCapturing)
            {
                selectionend = (int)CursorToTextIndex(localcoord);
                bScreenInvalid = true;
            }
        }

        return false;
    }


    void TextEditWin::UpdateFirstVisibleRow()
    {
        if (!mbVisible)
            return;

        int64_t rowCount = ((int64_t)mText.size() + mWidth - 1) / mWidth;

        if (mLocalCursorPos.Y < 0)
        {
            firstVisibleRow = firstVisibleRow + mLocalCursorPos.Y;
            UpdateCursorPos(COORD(mLocalCursorPos.X, 0));
        }
        else if (mLocalCursorPos.Y >= mHeight)
        {
            firstVisibleRow = firstVisibleRow + mLocalCursorPos.Y - mHeight + 1;
            UpdateCursorPos(COORD(mLocalCursorPos.X, (SHORT)mHeight - 1));
        }
    }


    void TextEditWin::UpdateCursorPos(COORD localPos)
    {
//        if (!mbVisible)
//            return;

        int index = (int)CursorToTextIndex(localPos);
        if (index > (int)mText.length())
            index = (int)mText.length();
        mLocalCursorPos = TextIndexToCursor(index);

//        cout << "mLocalCursorPos:" << mLocalCursorPos.X << ", " << mLocalCursorPos.Y << "\n";

        //        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), LocalCursorToGlobal(mLocalCursorPos));

        COORD global = LocalCursorToGlobal(mLocalCursorPos);
        SetCursorPosition(global, true);
//        cout << "\033[" << global.X + 1 << "G\033[" << global.Y << "d" << std::flush;
    }


    COORD TextEditWin::LocalCursorToGlobal(COORD cursor)
    {
        return COORD(cursor.X + (SHORT)mX, cursor.Y + (SHORT)mY);
    }



    void TextEditWin::FindNextBreak(int nDir)
    {
        int index = (int)CursorToTextIndex(mLocalCursorPos);

        if (nDir > 0)
        {
            index++;
            if (index < (int64_t)mText.length())
            {
                if (isalnum((int)mText[index]))   // index is on an alphanumeric
                {
                    while (index < (int64_t)mText.length() && isalnum((int)mText[index]))   // while an alphanumeric character, skip
                        index++;
                }

                while (index < (int64_t)mText.length() && isblank((int)mText[index]))    // while whitespace, skip
                    index++;
            }
        }
        else
        {
            index--;
            if (index > 0)
            {
                if (isalnum((int)mText[index]))   // index is on an alphanumeric
                {
                    while (index > 0 && isalnum((int)mText[index - 1]))   // while an alphanumeric character, skip
                        index--;
                }
                else
                {
                    while (index > 0 && isblank((int)mText[index - 1]))    // while whitespace, skip
                        index--;

                    while (index > 0 && isalnum((int)mText[index - 1]))   // while an alphanumeric character, skip
                        index--;
                }
            }
        }

        if (index < 0)
            index = 0;
        if (index > (int)mText.length())
            index = (int)mText.length();

        UpdateCursorPos(TextIndexToCursor(index));
    }

    bool TextEditWin::IsIndexInSelection(int64_t i)
    {
        int64_t normalizedStart = selectionstart;
        int64_t normalizedEnd = selectionend;
        if (normalizedEnd < normalizedStart)
        {
            normalizedStart = selectionend;
            normalizedEnd = selectionstart;
        }

        return (i >= normalizedStart && i < normalizedEnd);
    }

    string TextEditWin::GetSelectedText()
    {
        if (!IsTextSelected())
            return "";

        int64_t normalizedStart = selectionstart;
        int64_t normalizedEnd = selectionend;
        if (normalizedEnd < normalizedStart)
        {
            normalizedStart = selectionend;
            normalizedEnd = selectionstart;
        }

        return mText.substr(normalizedStart, normalizedEnd - normalizedStart);
    }

    void TextEditWin::AddUndoEntry()
    {
        undoEntry entry(mText, CursorToTextIndex(mLocalCursorPos), selectionstart, selectionend);
        mUndoEntryList.emplace_back(std::move(entry));
    }

    void TextEditWin::Undo()
    {
        if (mUndoEntryList.empty())
            return;

        undoEntry entry(*mUndoEntryList.rbegin());
        mUndoEntryList.pop_back();

        mText = entry.text;
        selectionstart = entry.selectionstart;
        selectionend = entry.selectionend;
        UpdateCursorPos(TextIndexToCursor(entry.cursorindex));
    }

    TextEditWin::~TextEditWin()
    {
        if (bHotkeyHooked)
        {
            UnhookHotkeys();
        }
    }

    void TextEditWin::HookHotkeys()
    {
        if (bHotkeyHooked)
            return;

        if (!RegisterHotKey(nullptr, CTRL_V_HOTKEY, MOD_CONTROL, 'V'))
        {
            std::cerr << "Error registering hotkey:" << GetLastError() << std::endl;
            return;
        }

        if (!RegisterHotKey(nullptr, SHIFT_INSERT_HOTKEY, MOD_SHIFT, VK_INSERT))
        {
            std::cerr << "Error registering hotkey:" << GetLastError() << std::endl;
            return;
        }

        bHotkeyHooked = true;
    }

    void TextEditWin::UnhookHotkeys()
    {
        if (!bHotkeyHooked)
            return;

        UnregisterHotKey(nullptr, CTRL_V_HOTKEY);
        UnregisterHotKey(nullptr, SHIFT_INSERT_HOTKEY);

        bHotkeyHooked = false;
    }


    void TextEditWin::SetText(const std::string& text)
    {
        mText = text;
        UpdateCursorPos(TextIndexToCursor((int64_t)text.size()));
    }

    void TextEditWin::SetVisible(bool bVisible)
    {
        mbDone = false;
        mbCanceled = false;
        mbVisible = bVisible;

        if (bVisible && !bHotkeyHooked)
        {
            HookHotkeys();
        }

        if (!bVisible && bHotkeyHooked)
        {
            UnhookHotkeys();
        }
        UpdateCursorPos(TextIndexToCursor((int64_t)mText.size()));
    }

    void TextEditWin::SetArea(const Rect& r)
    {
        ConsoleWin::SetArea(r);
        UpdateCursorPos(mLocalCursorPos);
    }

    void TextEditWin::Paint(tConsoleBuffer& backBuf)
    {
        // Update display
        if (!mbVisible)
            return;

        ConsoleWin::BasePaint();

        COORD cursor((SHORT)0, (SHORT)-firstVisibleRow);

        std::vector<ZAttrib> attribs;
        attribs.resize(mText.size());


        for (size_t textindex = 0; textindex < mText.length(); textindex++)
        {
            attribs[textindex] = kRawText;
        }

        for (size_t textindex = 0; textindex < mText.length(); textindex++)
        {
            if (IsIndexInSelection(textindex))
                attribs[textindex] = kSelectedText;
        }


        for (size_t textindex = 0; textindex < mText.size(); textindex++)
        {
            uint8_t c = mText[textindex];
            if (c == '\n' && bMultiline)
            {

                cursor.X = 0;
                cursor.Y++;
            }
            else
            {
                DrawCharClipped(c, cursor.X, cursor.Y, attribs[textindex]);
            }

            cursor.X++;
        }



        ConsoleWin::RenderToBackBuf(backBuf);
    }


    bool TextEditWin::OnKey(int keycode, char c)
    {
        bool bCTRLHeld = GetKeyState(VK_CONTROL) & 0x800;
        bool bSHIFTHeld = GetKeyState(VK_SHIFT) & 0x800;

        bool bHandled = false;

        do
        {
            switch (keycode)
            {
            case VK_RETURN:
                mbDone = true;
                return true;
            case VK_ESCAPE:
            {
                if (IsTextSelected())
                {
                    ClearSelection();
                }
                else
                {
                    mbCanceled = true;
                }
                bHandled = true;
            }
            break;
            case VK_HOME:
            {
                UpdateSelection();
                UpdateCursorPos(TextIndexToCursor(0));
                UpdateSelection();
                bHandled = true;
            }
            break;
            case VK_END:
            {
                UpdateSelection();
                UpdateCursorPos(TextIndexToCursor((int64_t)mText.size()));
                UpdateSelection();
                bHandled = true;
            }
            break;
            case VK_UP:
            {
                if (bMultiline)
                {
                    if (mLocalCursorPos.Y + firstVisibleRow > 0)
                    {
                        mLocalCursorPos.Y--;
                        UpdateFirstVisibleRow();
                        UpdateCursorPos(mLocalCursorPos);
                    }
                    else
                    {
                        UpdateSelection();
                        UpdateCursorPos(COORD(mLocalCursorPos.X, mLocalCursorPos.Y - 1));
                        UpdateSelection();
                    }
                    bHandled = true;
                }
            }
            break;
            case VK_DOWN:
            {
                if (bMultiline)
                {
                    UpdateSelection();
                    UpdateCursorPos(COORD(mLocalCursorPos.X, mLocalCursorPos.Y + 1));
                    UpdateSelection();
                    bHandled = true;
                }
            }
            break;
            case VK_LEFT:
            {
                UpdateSelection();
                // Move cursor left
                int64_t index = CursorToTextIndex(mLocalCursorPos);
                if (index > 0)
                {
                    if (bCTRLHeld)
                        FindNextBreak(-1);
                    else
                        UpdateCursorPos(TextIndexToCursor(index - 1));
                }
                UpdateSelection();
                bHandled = true;
            }
            break;
            case VK_RIGHT:
            {
                UpdateSelection();
                if (bCTRLHeld)
                {
                    FindNextBreak(1);
                }
                else
                {
                    // Move cursor right
                    int64_t index = CursorToTextIndex(mLocalCursorPos);
                    if (index < (int64_t)mText.size())
                    {
                        UpdateCursorPos(TextIndexToCursor(index + 1));
                    }

                }
                UpdateSelection();
                bHandled = true;
            }
            break;
            case VK_BACK:
            {
                if (IsTextSelected())
                {
                    AddUndoEntry();
                    DeleteSelection();
                }
                else
                {
                    // Delete character before cursor
                    int64_t index = CursorToTextIndex(mLocalCursorPos);
                    if (index > 0)
                    {
                        UpdateSelection();
                        mText.erase(index - 1, 1);
                        UpdateCursorPos(TextIndexToCursor(index - 1));
                    }
                }
                bHandled = true;
            }
            break;
            case VK_DELETE:
            {
                if (IsTextSelected())
                {
                    AddUndoEntry();
                    if (bSHIFTHeld) // SHIFT-DELETE is cut
                    {
                        CopyTextToClipboard(GetSelectedText());
                    }

                    DeleteSelection();
                }
                else
                {
                    // Delete character at cursor
                    int64_t index = CursorToTextIndex(mLocalCursorPos);
                    if (index < (int64_t)(mText.size()))
                    {
                        AddUndoEntry();
                        mText.erase(index, 1);
                    }
                }
                UpdateSelection();
                bHandled = true;
            }
            break;
            case 0x58:
            {
                if (bCTRLHeld)  // CTRL-X is cut
                {
                    if (IsTextSelected())
                    {
                        AddUndoEntry();
                        CopyTextToClipboard(GetSelectedText());
                        DeleteSelection();
                    }

                    bHandled = true;
                    break;
                }
            }
            case 0x41:
            {
                if (bCTRLHeld)  // CTRL-A
                {
                    selectionstart = 0;
                    selectionend = mText.length();
                    bHandled = true;
                    break;
                }
            }
            break;
            case VK_INSERT:
            case 0x43:
            {
                if (bCTRLHeld)  // CTRL-C
                {
                    // handle copy
                    CopyTextToClipboard(GetSelectedText());
                    bHandled = true;
                    break;
                }
                if (bSHIFTHeld)  // SHIFT-INSERT is paste
                {
                    AddUndoEntry();
                    HandlePaste(GetTextFromClipboard());
                    bHandled = true;
                    break;
                }
            }
            break;
            case 0x5a:          // CTRL-Z
            {
                if (bCTRLHeld)
                {
                    Undo();
                    bHandled = true;
                    break;
                }
            }
            }
        } while (0); // for breaking

        // nothing handled above....regular text entry
        if (!bHandled && c >= 32)
        {
            AddUndoEntry();
            DeleteSelection();

            // Insert character at cursor position
            int index = (int)CursorToTextIndex(mLocalCursorPos);
            mText.insert(index, 1, c);
            UpdateCursorPos(TextIndexToCursor(index + 1));
//            UpdateSelection();
            bHandled = true;
        }


        UpdateFirstVisibleRow();
        return bHandled;
    }

    int64_t TextEditWin::CursorToTextIndex(COORD coord)
    {
        int64_t i = (coord.Y + firstVisibleRow) * mWidth + coord.X;
        return std::min<size_t>(i, mText.size());
    }

    COORD TextEditWin::TextIndexToCursor(int64_t i)
    {
        if (i > (int64_t)mText.size())
            i = (int64_t)mText.size();

        if (mWidth > 0)
        {
            COORD c;
            c.X = (SHORT)(i) % mWidth;
            c.Y = (SHORT)((i / mWidth) - firstVisibleRow);
            return c;
        }

        return COORD(0, 0);
    }

    void TextEditWin::HandlePaste(string text)
    {
        DeleteSelection();  // delete any selection if needed
        int64_t curindex = CursorToTextIndex(mLocalCursorPos);
        mText.insert(curindex, text);
        curindex += (int)text.length();
        UpdateCursorPos(TextIndexToCursor(curindex));

        /*        static int count = 1;
                char buf[64];
                sprintf(buf, "paste:%d\n", count++);
                OutputDebugString(buf);*/
    }

    void TextEditWin::DeleteSelection()
    {
        if (!IsTextSelected())
            return;

        int64_t normalizedStart = selectionstart;
        int64_t normalizedEnd = selectionend;
        if (normalizedEnd < normalizedStart)
        {
            normalizedStart = selectionend;
            normalizedEnd = selectionstart;
        }

        int64_t selectedChars = normalizedEnd - normalizedStart;

        mText.erase(normalizedStart, selectedChars);

        int curindex = (int)CursorToTextIndex(mLocalCursorPos);
        if (curindex > normalizedStart)
            curindex -= (int)(curindex - normalizedStart);
        UpdateCursorPos(TextIndexToCursor(curindex));

        ClearSelection();
    }

    void TextEditWin::ClearSelection()
    {
        selectionstart = -1;
        selectionend = -1;
        UpdateCursorPos(mLocalCursorPos);
    }

    void TextEditWin::UpdateSelection()
    {
        if (!(GetKeyState(VK_SHIFT) & 0x800))
        {
            ClearSelection();
        }
        else
        {
            if (selectionstart == -1)
            {
                selectionstart = CursorToTextIndex(mLocalCursorPos);
            }
            selectionend = CursorToTextIndex(mLocalCursorPos);
        }
    }



    string GetTextFromClipboard()
    {
        if (!OpenClipboard(NULL))
            return "";

        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData == NULL)
        {
            CloseClipboard();
            return "";
        }

        CHAR* pszText = static_cast<CHAR*>(GlobalLock(hData));
        if (pszText == NULL)
        {
            CloseClipboard();
            return "";
        }

        std::string clipboardText = pszText;

        GlobalUnlock(hData);
        CloseClipboard();

        return clipboardText;
    }

    bool CopyTextToClipboard(const std::string& text)
    {
        if (!OpenClipboard(NULL))
        {
            std::cerr << "Error opening clipboard" << std::endl;
            return false;
        }

        if (!EmptyClipboard())
        {
            CloseClipboard();
            std::cerr << "Error emptying clipboard" << std::endl;
            return false;
        }

        HGLOBAL hClipboardData = GlobalAlloc(GMEM_MOVEABLE, (text.length() + 1) * sizeof(char));
        if (hClipboardData == NULL)
        {
            CloseClipboard();
            std::cerr << "Error allocating memory for clipboard" << std::endl;
            return false;
        }

        char* pBuffer = static_cast<char*>(GlobalLock(hClipboardData));
        if (pBuffer == NULL)
        {
            CloseClipboard();
            std::cerr << "Error locking memory for clipboard" << std::endl;
            return false;
        }

        // Copy the text to the buffer
        strcpy_s(pBuffer, text.length() + 1, text.c_str());

        GlobalUnlock(hClipboardData);

        if (!SetClipboardData(CF_TEXT, hClipboardData))
        {
            CloseClipboard();
            std::cerr << "Error setting clipboard data" << std::endl;
            return false;
        }

        CloseClipboard();
        return true;
    }

    void ShowEnvVars()
    {
        string sText;

        SHORT w = ScreenW();
        SHORT h = ScreenH();

        if (w < 1)
            w = 1;
        if (h < 8)
            h = 8;

        helpTableWin.Init(Rect(0, 1, w, h));
        helpTableWin.Clear(kAttribHelpBG, true);
        helpTableWin.SetEnableFrame();
        helpTableWin.bAutoScrollbar = true;
        bScreenInvalid = true;



        helpTableWin.positionCaption[ConsoleWin::Position::LT] = "Environment Variables";



        Rect drawArea;
        helpTableWin.GetInnerArea(drawArea);
        int64_t drawWidth = drawArea.r - drawArea.l - 2;


        helpTableWin.mTable.Clear();
//        helpTableWin.mTable.SetBorders("|", DEC_LINE_START "q" DEC_LINE_END, "|", DEC_LINE_START "q" DEC_LINE_END, "|");
/*        helpTableWin.mTable.SetBorders(DEC_LINE_START "\x78" DEC_LINE_END,              // LEFT
                                        "\x71",                                         // TOP
                                        DEC_LINE_START "\x78" DEC_LINE_END,             // RIGHT
                                        "\x71",                                         // BOTTOM
                                        DEC_LINE_START "\x78" DEC_LINE_END,             // CENTER
                                        DEC_LINE_START "\x6c",                          // TL
                                        "\x6b" DEC_LINE_END,                            // TR
                                        DEC_LINE_START "\x6d",                          // BL
                                        "\x6a" DEC_LINE_END);                           // BR*/
        Table::SetDecLineBorders(helpTableWin.mTable, COL_BLUE);
                                       
        helpTableWin.mTable.AddRow(SubSectionStyle, "--var--", "--value--");

        Table::Style keyStyle(ParamStyle);
        keyStyle.wrapping = Table::NO_WRAP;

        Table::Style valueStyle(ResetStyle);
        valueStyle.wrapping = Table::CHAR_WRAP;

        tKeyValList keyVals = GetEnvVars();
/*        size_t longestKey = 0;
        size_t longestVal = 0;
        for (const auto& kv : keyVals)
        {
            longestKey = std::max<size_t>(longestKey, kv.first.length());
            longestVal = std::max<size_t>(longestVal, kv.second.length());
        }*/

//        size_t valueColW = drawWidth - longestKey - 6;
        for (const auto& kv : keyVals)
        {
            string sKey(kv.first);
            string sVal(kv.second);

            for (auto c : sKey)
            {
                assert(c != '\0');
            }

            for (auto c : sVal)
            {
                assert(c != '\0');
            }



/*            size_t offset = 0;
            while (offset < sVal.length())
            {
                string sFitVal = sVal.substr(offset, valueColW);
                if (offset == 0) // first row
                    varTable.AddRow(sKey, sFitVal);
                else
                    varTable.AddRow("", sFitVal);
                offset += sFitVal.length();
            }*/
            helpTableWin.mTable.AddRow(Table::Cell(sKey, keyStyle), Table::Cell(sVal, valueStyle));
        }


        helpTableWin.mTable.AlignWidth(drawWidth);

        //helpWin.mText = (string)varTable;
        helpTableWin.UpdateCaptions();
    }


    void ShowLaunchParams()
    {
        string sText;

        SHORT w = ScreenW();
        SHORT h = ScreenH();

        if (w < 1)
            w = 1;
        if (h < 8)
            h = 8;

        helpTableWin.Init(Rect(0, 1, w, h));
        helpTableWin.Clear(kAttribHelpBG, true);
        helpTableWin.SetEnableFrame();
        helpTableWin.bAutoScrollbar = true;
        helpTableWin.positionCaption[ConsoleWin::Position::LT] = "Launch Arguments";
        helpTableWin.mTable.AddRow(SectionStyle, " Launch Arguments ");
        helpTableWin.mTable.AddRow(GetCommandLineA());
        helpTableWin.mTopVisibleRow = 0;
        helpTableWin.UpdateCaptions();
        bScreenInvalid = true;
    }



};  // namespace CLP




#endif // ENABLE_CLE

#ifdef ENABLE_COMMAND_HISTORY
namespace CLP
{
    tStringList commandHistory;

    string HistoryPath()
    {
        string sPath = getenv("LOCALAPPDATA");
        sPath += "\\" + CLP::appName + "_history";
        return sPath;
    }

    bool LoadHistory()
    {
        if (CLP::appName.empty())
            return false;

        string sPath = HistoryPath();
        ifstream inFile(sPath);
        if (inFile)
        {
            stringstream ss;
            ss << inFile.rdbuf();
            commandHistory.clear();
            string sEncoded(ss.str());
            if (!sEncoded.empty())
            {
                SH::ToList(sEncoded, commandHistory);
                while (commandHistory.size() > kCommandHistoryLimit)
                    commandHistory.pop_front();
            }

            bScreenInvalid = true;

            return true;
        }

        return false;
    }

    bool SaveHistory()
    {
        if (CLP::appName.empty() || commandHistory.empty())
            return false;

        while (commandHistory.size() > kCommandHistoryLimit)
            commandHistory.pop_front();

        string sPath = HistoryPath();
        ofstream outFile(sPath, ios_base::trunc);
        if (outFile)
        {
            string sEncoded = SH::FromList(commandHistory);
            outFile << sEncoded;
            return true;
        }
        return false;
    }

    bool AddToHistory(const std::string& sCommandLine)
    {
        for (tStringList::iterator it = commandHistory.begin(); it != commandHistory.end(); it++)
        {
            if (SH::Compare(*it, sCommandLine, false))
            {
                commandHistory.erase(it);
                break;
            }
        }

        commandHistory.emplace_back(sCommandLine);
        bScreenInvalid = true;

        return true;
    }
}
#endif // ENABLE_COMMAND_HISTORY
