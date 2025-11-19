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
#else
#include <sys/ioctl.h>
#include <unistd.h>
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
    ZAttrib kAttribFrame(BLACK | MAKE_BG(GOLD));
    ZAttrib kAttribCaption(WHITE);
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

//    ZAttrib kAttribScrollbarBG(BLACK | MAKE_BG(0xFF888888));
//    ZAttrib kAttribScrollbarThumb(WHITE | MAKE_BG(0xFFBBBBBB));
    ZAttrib kAttribScrollbarBG(BLACK | MAKE_BG(GOLD));
    ZAttrib kAttribScrollbarThumb(WHITE | MAKE_BG(GOLD));


/*    HANDLE mhInput;
    HANDLE mhOutput;
    std::vector<CHAR_INFO> originalConsoleBuf;
    std::vector<CHAR_INFO> workingConsoleBuf;
    CONSOLE_SCREEN_BUFFER_INFO originalScreenInfo;
    CONSOLE_SCREEN_BUFFER_INFO screenInfo;
    bool bScreenInfoInitialized = false;
    bool bScreenInvalid = true;
    COORD gLastCursorPos = { -1, -1 };*/

    NativeConsole gConsole;

    TableWin helpTableWin;

    void NativeConsole::SetCursorPosition(COORD coord, bool bForce)
    {
        string out;
        if (bForce || coord.X != lastCursorPos.X || coord.Y != lastCursorPos.Y)
        {
            out = "\033[" + SH::FromInt(coord.X + 1) + "G\033[" + SH::FromInt(coord.Y + 1) + "d";
            lastCursorPos = coord;
        }

        if (mbCursorVisible)
            //out += "\033[?25h";
            out += "\033[?25h\033[1 q";
        else
            out += "\033[?25l";

        DWORD written;
        WriteConsole(mhOutput, out.c_str(), (DWORD)out.length(), &written, NULL);
    }

    void NativeConsole::SetCursorVisible(bool bVisible)
    {
        if (mbCursorVisible != bVisible)
            mbScreenInvalid = true;
        mbCursorVisible = bVisible;
    }


    bool NativeConsole::ConsoleHasFocus()
    {
        HWND consoleWnd = GetConsoleWindow();
        if (!consoleWnd) return false;

        return (GetForegroundWindow() == consoleWnd);
    }


    bool NativeConsole::ScreenChanged() const
    {
        CONSOLE_SCREEN_BUFFER_INFO newScreenInfo;
        if (!GetConsoleScreenBufferInfo(mhOutput, &newScreenInfo))
        {
            cerr << "Failed to get console info." << endl;
            return false;
        }

        bool bChanged = newScreenInfo.dwSize.X != screenInfo.w() || newScreenInfo.dwSize.Y != screenInfo.h();
        return bChanged;
    }

    bool NativeConsole::UpdateScreenInfo()
    {
        HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOutput == INVALID_HANDLE_VALUE)
            return false;

        CONSOLE_SCREEN_BUFFER_INFO si;
        if (!GetConsoleScreenBufferInfo(hOutput, &si))
        {
            cerr << "Failed to get console info." << endl;
            return false;
        }

        if (si.dwSize.X == screenInfo.w() && si.dwSize.Y == screenInfo.h())
            return false;

        screenInfo.l = si.srWindow.Left;
        screenInfo.t = si.srWindow.Top;
        screenInfo.r = screenInfo.l + si.dwSize.X;
        screenInfo.b = screenInfo.t + si.dwSize.Y;

        assert(si.dwSize.X == screenInfo.w() && si.dwSize.Y == screenInfo.h());
        return true;
    }

    bool NativeConsole::UpdateNativeConsole()
    {
        // TBD.....does this need thread safety?
        if (!UpdateScreenInfo())    // no change
            return false;

        mBackBuffer.clear();
        mDrawStateBuffer.clear();
        size_t screenchars = screenInfo.w() * screenInfo.h();
        mBackBuffer.resize(screenchars);
        mDrawStateBuffer.resize(screenchars);

        msAnsiOut.reserve((size_t)(screenchars * 8)); // rough estimate


        DWORD written;
        COORD origin = { 0, 0 };
        FillConsoleOutputCharacter(mhOutput, ' ', (DWORD)screenchars, origin, &written);
        FillConsoleOutputAttribute(mhOutput, 0x07, (DWORD)screenchars, origin, &written);


        mbScreenInvalid = true;
        return true;
    }

    // Helper function to check if RGB values match one of the 16 standard console colors
    bool IsStandard16Color(uint8_t r, uint8_t g, uint8_t b)
    {
        // Standard console color table
        static const uint8_t standardColors[16][3] =
        {
            {0,   0,   0},   // 0: Black
            {0,   0,   128}, // 1: Dark Blue
            {0,   128, 0},   // 2: Dark Green
            {0,   128, 128}, // 3: Dark Cyan
            {128, 0,   0},   // 4: Dark Red
            {128, 0,   128}, // 5: Dark Magenta
            {128, 128, 0},   // 6: Dark Yellow
            {192, 192, 192}, // 7: Gray
            {128, 128, 128}, // 8: Dark Gray
            {0,   0,   255}, // 9: Blue
            {0,   255, 0},   // 10: Green
            {0,   255, 255}, // 11: Cyan
            {255, 0,   0},   // 12: Red
            {255, 0,   255}, // 13: Magenta
            {255, 255, 0},   // 14: Yellow
            {255, 255, 255}  // 15: White
        };

        for (int i = 0; i < 16; i++)
        {
            if (standardColors[i][0] == r &&
                standardColors[i][1] == g &&
                standardColors[i][2] == b)
            {
                return true;
            }
        }

        return false;
    }

    // Helper to convert RGB to color index (0-15)
    uint8_t RGBToColorIndex(uint8_t r, uint8_t g, uint8_t b)
    {
        static const uint8_t standardColors[16][3] =
        {
            {0,   0,   0},   {0,   0,   128}, {0,   128, 0},   {0,   128, 128},
            {128, 0,   0},   {128, 0,   128}, {128, 128, 0},   {192, 192, 192},
            {128, 128, 128}, {0,   0,   255}, {0,   255, 0},   {0,   255, 255},
            {255, 0,   0},   {255, 0,   255}, {255, 255, 0},   {255, 255, 255}
        };

        for (uint8_t i = 0; i < 16; i++)
        {
            if (standardColors[i][0] == r &&
                standardColors[i][1] == g &&
                standardColors[i][2] == b)
            {
                return i;
            }
        }

        return 7; // Default to gray if not found
    }

    WORD ZAttribToConsoleAttributes(const ZAttrib& attrib)
    {
        // This assumes CanUseWriteConsoleOutput returned true
        uint8_t fgIndex = RGBToColorIndex(attrib.r, attrib.g, attrib.b);
        uint8_t bgIndex = RGBToColorIndex(attrib.br, attrib.bg, attrib.bb);

        return (WORD)((bgIndex << 4) | fgIndex);
    }


    bool CanUseWriteConsoleOutput(const ZChar& c)
    {
        // 1. Check if character is in the valid range
        // WriteConsoleOutput can handle extended ASCII (0-255)
        // but null characters should be avoided
        if (c.c == 0)
        {
            return false;
        }

        // 2. Check if dec_line is set (DEC line drawing mode)
        // WriteConsoleOutput doesn't handle special line drawing the same way as ANSI
        if (c.attrib.dec_line)
        {
            return false;
        }

        // 3. Check if colors are standard 16-color palette
        // WriteConsoleOutput's CHAR_INFO uses 4-bit color indices (0-15)
        // We need to check if the RGB values map cleanly to the standard palette

        // Standard Windows console colors (as RGB):
        // 0=Black(0,0,0), 1=DarkBlue(0,0,128), 2=DarkGreen(0,128,0), 3=DarkCyan(0,128,128)
        // 4=DarkRed(128,0,0), 5=DarkMagenta(128,0,128), 6=DarkYellow(128,128,0), 7=Gray(192,192,192)
        // 8=DarkGray(128,128,128), 9=Blue(0,0,255), 10=Green(0,255,0), 11=Cyan(0,255,255)
        // 12=Red(255,0,0), 13=Magenta(255,0,255), 14=Yellow(255,255,0), 15=White(255,255,255)

        // If alpha is not 255, we can't represent it in WriteConsoleOutput
        if (c.attrib.a != 255 || c.attrib.ba != 255)
        {
            return false;
        }

        // Check if foreground and background colors match the standard 16-color palette
        if (!IsStandard16Color(c.attrib.r, c.attrib.g, c.attrib.b))
        {
            return false;
        }

        if (!IsStandard16Color(c.attrib.br, c.attrib.bg, c.attrib.bb))
        {
            return false;
        }

        return true;
    }


    bool NativeConsole::Init()
    {
        if (mbInitted)
            return true;

        mhOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        if (mhOutput == INVALID_HANDLE_VALUE)
        {
            cerr << "Failed to get standard output handle." << endl;
            return false;
        }

        mhOutput = CreateFile("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (!GetConsoleScreenBufferInfo(mhOutput, &originalScreenInfo))
        {
            cerr << "Failed to get console info." << endl;
            return false;
        }

        // Save console state
        originalConsoleBuf.resize(originalScreenInfo.dwSize.X * originalScreenInfo.dwSize.Y);
        SMALL_RECT readRegion = { 0, 0, originalScreenInfo.dwSize.X - 1, originalScreenInfo.dwSize.Y - 1 };
        ReadConsoleOutput(mhOutput, &originalConsoleBuf[0], originalScreenInfo.dwSize, { 0, 0 }, &readRegion);




        // Set console mode to allow reading mouse and key events
        DWORD mode;
        if (!GetConsoleMode(mhInput, &mode))
        {
            // Piped input...... reopen for interactive input
            CloseHandle(mhInput);
            mhInput = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

            if (mhInput == INVALID_HANDLE_VALUE)
            {
                cerr << "Failed to get console mode." << endl;
                return false;
            }

            SetStdHandle(STD_INPUT_HANDLE, mhInput);
            if (!GetConsoleMode(mhInput, &mode))
            {
                cerr << "Failed to get console mode after creating new stdin" << endl;
                return false;
            }
        }



        mode |= ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT;
        mode &= ~(ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE);

        if (!SetConsoleMode(mhInput, mode))
        {
            cerr << "Failed to set console mode." << endl;
            return false;
        }

        screenInfo = { 0,0,0,0 };
        UpdateNativeConsole();

        mbScreenInvalid = true;
        mbInitted = true;
        return true;
    }

    bool NativeConsole::Shutdown()
    {
        if (!mbInitted)
            return true;

        cout << "\033[?25h" << COL_WHITE << COL_BG_BLACK << DEC_LINE_END << COL_RESET;    // show cursor, reset colors, ensure dec line mode is off

        CONSOLE_SCREEN_BUFFER_INFO si;
        if (GetConsoleScreenBufferInfo(mhOutput, &si))
        {
            if (si.dwSize.X == originalScreenInfo.dwSize.X && si.dwSize.Y == originalScreenInfo.dwSize.Y)
            {
                // Restore console state only if the dimmensions are the same as when captured
                SMALL_RECT writeRegion = { 0, 0, originalScreenInfo.dwSize.X - 1, originalScreenInfo.dwSize.Y - 1 };
                WriteConsoleOutput(mhOutput, &originalConsoleBuf[0], originalScreenInfo.dwSize, { 0, 0 }, &writeRegion);
                SetConsoleCursorPosition(mhOutput, originalScreenInfo.dwCursorPosition);
                SetConsoleTextAttribute(mhOutput, originalScreenInfo.wAttributes);
            }
            else
            {
                // Dimensions have changed, so just clear
                DWORD count = si.dwSize.X * si.dwSize.Y;
                FillConsoleOutputCharacter(mhOutput, (TCHAR)' ', count, { 0,0 }, &count);
                FillConsoleOutputAttribute(mhOutput, si.wAttributes, count, { 0,0 }, &count);
            }
        }

        mbInitted = false;
        return true;
    }


    int64_t NativeConsole::Width()
    { 
        if (screenInfo.w() < 1)
            UpdateScreenInfo();

        return screenInfo.r - screenInfo.l;
    }

    int64_t NativeConsole::Height()
    { 
        if (screenInfo.h() < 1)
            UpdateScreenInfo();

        return screenInfo.b - screenInfo.t;
    }

    bool NativeConsole::Render()
    {
        if (!mbScreenInvalid)
            return false;


        std::string resetDecLine = "\033[?25l\033(B";  // Reset to ASCII mode and hide cursor
        DWORD written;
        WriteConsole(mhOutput, resetDecLine.c_str(), (DWORD)resetDecLine.length(), &written, NULL);


        COORD savePos = lastCursorPos;
        int64_t w = Width();
        int64_t h = Height();

        assert(mDrawStateBuffer.size() == w * h);
        assert(mBackBuffer.size() == w * h);

        // Compatible regions
/*        for (int y = 0; y < h; y++)
        {
            int x = 0;
            while (x < w)
            {
                // Skip unchanged
                while (x < w && mBackBuffer[y * w + x] == mDrawStateBuffer[y * w + x])
                {
                    x++;
                }

                if (x >= w) break;

                // Find run of changed, compatible chars
                int startX = x;
                std::vector<CHAR_INFO> chars;

                while (x < w && mBackBuffer[y * w + x] != mDrawStateBuffer[y * w + x] && CanUseWriteConsoleOutput(mBackBuffer[y * w + x]))
                {
                    CHAR_INFO ci;
                    ZChar c = mBackBuffer[y * w + x];
                    if (c.c < 32 && (c.attrib.a > 0 || c.attrib.ba > 0))
                        c.c = ' ';
                    ci.Char.AsciiChar = c.c;
                    ci.Attributes = ZAttribToConsoleAttributes(mBackBuffer[y * w + x].attrib);
                    chars.push_back(ci);
                    x++;
                }

                // Write this run
                if (!chars.empty())
                {
                    COORD bufSize = { (SHORT)chars.size(), 1 };
                    COORD bufCoord = { 0, 0 };
                    SMALL_RECT region = { (SHORT)startX, (SHORT)y, (SHORT)(startX + chars.size() - 1), (SHORT)y };
                    WriteConsoleOutput(mBufferHandle, chars.data(), bufSize, bufCoord, &region);
                }

                // now skip any incompattible chars
                while (x < w && !CanUseWriteConsoleOutput(mBackBuffer[y * w + x]))
                {
                    x++;
                }
            }
        }
        */

        // ansi regions
        msAnsiOut.clear();

        ZAttrib currentAttrib;
        bool needAttribReset = true;  // Force first attribute write


        for (int64_t y = 0; y < h; y++)
        {
            for (int64_t x = 0; x < w; x++)
            {
                int64_t idx = y * w + x;
                ZChar c = mBackBuffer[idx];
                if (c.c == 0 && (c.attrib.a > 0 || c.attrib.ba > 0))
                    c.c = ' ';

                if (mDrawStateBuffer[idx] == c /*|| CanUseWriteConsoleOutput(c)*/)
                    continue;
                
                // ansi
                msAnsiOut += "\033[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "H";

                if (needAttribReset || c.attrib != currentAttrib)
                {
                    msAnsiOut += c.attrib.ToAnsi();
                    currentAttrib = c.attrib;
                    needAttribReset = false;
                }

                // Write character (handle control chars and a==0 like before)
                if (c.c < 32)
                    msAnsiOut += ' ';
                else
                    msAnsiOut += (char)c.c;
            }
        }

        if (!msAnsiOut.empty())
        {
            DWORD written;
            WriteConsole(mhOutput, msAnsiOut.c_str(), (DWORD)msAnsiOut.length(), &written, NULL);
        }

        SetCursorPosition(savePos, true);
        mDrawStateBuffer = mBackBuffer;
        mbScreenInvalid = false;

        return true;
    }

/*    bool NativeConsole::Render()
    {
        // force for now
        mbScreenInvalid = true;


        if (!mbScreenInvalid)
            return false;


        cout << "\033[?25l";

        COORD savePos = lastCursorPos;
        int32_t w = Width();
        int32_t h = Height();

        assert(mDrawStateBuffer.size() == w * h);
        assert(mBackBuffer.size() == w * h);

        // Compatible regions
        for (int y = 0; y < h; y++)
        {
            int x = 0;
            while (x < w)
            {
                // Skip unchanged
                while (x < w && mBackBuffer[y * w + x] == mDrawStateBuffer[y * w + x])
                {
                    x++;
                }

                if (x >= w) break;

                // Find run of changed, compatible chars
                int startX = x;
                std::vector<CHAR_INFO> chars;

                while (x < w && mBackBuffer[y * w + x] != mDrawStateBuffer[y * w + x] && CanUseWriteConsoleOutput(mDrawStateBuffer[y * w + x]))
                {
                    CHAR_INFO ci;
                    ci.Char.AsciiChar = mDrawStateBuffer[y * w + x].c;
                    ci.Attributes = ZAttribToConsoleAttributes(mDrawStateBuffer[y * w + x].attrib);
                    chars.push_back(ci);
                    x++;
                }

                // Write this run
                if (!chars.empty())
                {
                    COORD bufSize = { (SHORT)chars.size(), 1 };
                    COORD bufCoord = { 0, 0 };
                    SMALL_RECT region = { (SHORT)startX, (SHORT)y, (SHORT)(startX + chars.size() - 1), (SHORT)y };
                    WriteConsoleOutput(mBufferHandle, chars.data(), bufSize, bufCoord, &region);
                }

                // now skip any incompattible chars
                while (x < w && /*frontBuf[y * w + x] != backBuf[y * w + x] &&*//* !CanUseWriteConsoleOutput(mDrawStateBuffer[y * w + x]))
                {
                    x++;
                }
            }
        }

        // Write remaining regions
        std::string ansiOutput;
        ZAttrib currentAttrib;

        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                int idx = y * w + x;

                // Skip if unchanged or already written by WriteConsoleOutput
                if (mDrawStateBuffer[idx] == mBackBuffer[idx] || CanUseWriteConsoleOutput(mBackBuffer[idx]))
                {
                    continue;
                }

                // Position cursor (batch these to minimize escapes)
                ansiOutput += "\033[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "H";

                // Change attributes if needed
                if (mBackBuffer[idx].attrib != currentAttrib)
                {
                    ansiOutput += mBackBuffer[idx].attrib.ToAnsi();
                    currentAttrib = mBackBuffer[idx].attrib;
                }

                // Write character
                ansiOutput += (char)mBackBuffer[idx].c;
            }
        }

        if (!ansiOutput.empty())
        {
            DWORD written;
            WriteConsole(mBufferHandle, ansiOutput.c_str(), (DWORD)ansiOutput.length(), &written, NULL);

            int stophere = 5;
        }

        //SetCursorPosition(savePos);
        mDrawStateBuffer = mBackBuffer;
        mbScreenInvalid = false;
        return true;
    }

*/
/*    void NativeConsole::DrawAnsiChar(int64_t x, int64_t y, uint8_t c, ZAttrib ca)
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
        lastCursorPos.X++;    // cursor advances after drawing
        if (lastCursorPos.X > screenInfo.srWindow.Right - screenInfo.srWindow.Left)
        {
            lastCursorPos.X = 0;
            lastCursorPos.Y++;
        }
    };
    */

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

    std::string ConsoleWin::GetSelectedText()
    {
        Rect inner;
        GetInnerArea(inner);
        string sSelected;
        if (mRowColSelection.Valid())
        {
            mRowColSelection.Normalize();

            for (int64_t y = mRowColSelection.start.y; y <= mRowColSelection.end.y; y++)
            {
                for (int64_t x = inner.l; x < inner.r; x++)
                {
                    int64_t index = y * mWidth + x;
                    if (mRowColSelection.IsInside(x, y))
                    {
                        if (mBuffer[index].attrib.dec_line) // change dec_line drawing 
                            sSelected.push_back('*');
                        else
                            sSelected.push_back(mBuffer[index].c);
                    }
                }


                int64_t i = sSelected.length()-1;
                while (i > 0 && sSelected[i] == ' ')
                    i--;
                sSelected = sSelected.substr(0, i+1);
                sSelected += "\n";
            }
        }
        else if (mRectSelection.Valid())
        {
            Rect clipped(mRectSelection);
            clipped.Normalize();
            clipped.ClipTo(inner);

            for (int64_t y = clipped.t; y < clipped.b; y++)
            {
                for (int64_t x = clipped.l; x < clipped.r; x++)
                {
                    int64_t index = y * mWidth + x;
                    if (mBuffer[index].attrib.dec_line) // change dec_line drawing 
                        sSelected.push_back('*');
                    else
                        sSelected.push_back(mBuffer[index].c);
                }

                int64_t i = sSelected.length() - 1;
                while (i > 0 && sSelected[i] == ' ')
                    i--;
                sSelected = sSelected.substr(0, i + 1);
                sSelected += "\n";
            }
        }

        return sSelected;
    }


    bool ConsoleWin::OnMouse(MOUSE_EVENT_RECORD event)
    {
        if (mbSelectionEnabled)
        {

            bool bALTHeld = GetKeyState(VK_MENU) & 0x800;

            Rect inner;
            GetInnerArea(inner);

            Point local(event.dwMousePosition.X, event.dwMousePosition.Y);
            if (local.x < inner.l)
                local.x = inner.l;
            else if (local.x > inner.r)
                local.x = inner.r;
            if (local.y < inner.t)
                local.y = inner.t;
            else if (local.y > inner.b)
                local.y = inner.b;

//            local.x += mX;
//            local.y += mY;


            //        cout << "over:" << localcoord.X << "," << localcoord.Y << " ";

            if (event.dwEventFlags == 0)
            {
                if (event.dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED)
                {
                    mbRectangularSelection = bALTHeld;
                    if (mbRectangularSelection)
                    {
                        mRowColSelection.Clear();
                        mRectSelection.l = local.x;
                        mRectSelection.t = local.y;
                    }
                    else
                    {
                        mRectSelection.Clear();
                        mRowColSelection.start = local;
                        mRowColSelection.end = local;
                    }

                    mbWindowInvalid = true;
                    gConsole.Invalidate();
                    mbMouseCapturing = true;
                    return true;
                }
                else if (event.dwButtonState == RIGHTMOST_BUTTON_PRESSED)
                {
                    if (mRowColSelection.Valid() || mRectSelection.Valid())
                        CopyTextToClipboard(GetSelectedText());
                    gConsole.Invalidate();
                }
                else if (event.dwButtonState == 0 && mbMouseCapturing)
                {
                    // left button up
                    if (mbRectangularSelection)
                    {
                        mRectSelection.r = local.x;
                        mRectSelection.b = local.y;
                        mRectSelection.Normalize();
                        mRectSelection.ClipTo(inner);
                        if (mRectSelection.l == mRectSelection.r && mRectSelection.t == mRectSelection.b)
                            ClearSelection();
                    }
                    else
                    {
                        mRowColSelection.end = local;
                        if (mRowColSelection.start.x == mRowColSelection.end.x && mRowColSelection.start.y == mRowColSelection.end.y)
                            ClearSelection();
                    }


                    mbWindowInvalid = true;
                    gConsole.Invalidate();
                    mbMouseCapturing = false;
                    return true;
                }

            }
            else if (event.dwEventFlags == DOUBLE_CLICK)
            {
                mRowColSelection.start = local;
                mRowColSelection.end = local;

                int64_t sindex = (mRowColSelection.start.y * mWidth) + mRowColSelection.start.x;
                int64_t eindex = sindex;
                while (mRowColSelection.start.x-1 > inner.l && !IsBreakingChar(mBuffer[sindex-1].c))
                {
                    mRowColSelection.start.x--;
                    sindex--;
                }
                while (mRowColSelection.end.x+1 < inner.r && !IsBreakingChar(mBuffer[eindex+1].c))
                {
                    mRowColSelection.end.x++;
                    eindex++;
                }

                //mbMouseCapturing = true;
                mbWindowInvalid = true;
                gConsole.Invalidate();
                return true;
            }
            else if (event.dwEventFlags == MOUSE_MOVED)
            {
                // mouse moved
                if (mbMouseCapturing)
                {
                    if (mbRectangularSelection)
                    {
                        mRectSelection.r = local.x;
                        mRectSelection.b = local.y;
                    }
                    else
                    {
                        mRowColSelection.end = local;
                    }
                    mbWindowInvalid = true;
                    gConsole.Invalidate();
                }
/*                else
                {
                    mRowColSelection.start.x = local.x;
                    mRowColSelection.start.y = local.y;
                    mRowColSelection.end.x = local.x;
                    mRowColSelection.end.y = local.y;
                    mbWindowInvalid = true;
                    gConsole.Invalidate();
                }*/
            }
        }

        return false;
    }

    void ConsoleWin::ClearSelection()
    {
        mRowColSelection.Clear();
        mRectSelection.Clear();
        gConsole.Invalidate();
    }


    void ConsoleWin::GetArea(Rect& r)
    {
        r.l = mX;
        r.t = mY;
        r.r = r.l + mWidth;
        r.b = r.t + mHeight;
    }

    void ConsoleWin::GetInnerArea(Rect& r) const
    {
        r.l = 0;
        r.t = 0;
        r.r = mWidth;
        r.b = mHeight;
/*        r.l = 0;
        r.t = 0;
        r.r = 23;
        r.b = 22;*/

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
        Rect area(0, 0, 0, 0);
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

        ZAttrib bottomColor = kAttribFrame;
        if (mbGradient)
        {
            double factor = pow(0.96, mHeight);
            bottomColor.br = (uint64_t)(bottomColor.br * factor);
            bottomColor.bg = (uint64_t)(bottomColor.bg * factor);
            bottomColor.bb = (uint64_t)(bottomColor.bb * factor);
        }

        // Fill 
        Fill(Rect(0, 0, mWidth, mHeight), mClearAttrib, mbGradient);

        if (enableFrame[Side::T])
            Fill(Rect(0, 0, mWidth, 1), kAttribFrame, mbGradient);   // top

        if (enableFrame[Side::L])
            Fill(Rect(0, 0, 1, mHeight), kAttribFrame, mbGradient);   // left

        if (enableFrame[Side::B])
            Fill(Rect(0, mHeight - 1, mWidth, mHeight), bottomColor);    // bottom

        if (enableFrame[Side::R])
            Fill(Rect(mWidth - 1, 0, mWidth, mHeight), kAttribFrame, mbGradient);    // right  

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
        int64_t dr = gConsole.Width();
        int64_t db = gConsole.Height();

        if (mbSelectionEnabled)
        {
            Rect inner;
            GetInnerArea(inner);

            if (mRowColSelection.Valid())
            {
                int64_t startX = mRowColSelection.start.x;
                int64_t startY = mRowColSelection.start.y;
                int64_t endX = mRowColSelection.end.x;
                int64_t endY = mRowColSelection.end.y;

                // clip
                if (startY < inner.t)
                    startY = inner.t;
                if (endY > inner.b)
                    endY = inner.b;
                if (startX < inner.l)
                    startX = inner.l;
                if (endX > inner.r)
                    endX = inner.r;

                int64_t startIndex = (startY * mWidth) + startX;
                int64_t endIndex = (endY * mWidth) + endX;
                if (startIndex > endIndex)
                    std::swap(startIndex, endIndex);

                for (int64_t index = startIndex; index <= endIndex; index++)
                {
                    int64_t dy = (index / mWidth);
                    int64_t dx = index % mWidth;
                    if (dx >= inner.l && dx < inner.r && dy >= inner.t && dy < inner.b)
                    {
                        bool bDecLine = mBuffer[index].attrib.dec_line; // preserve declin setting
                        mBuffer[index].attrib = kSelectedText;
                        mBuffer[index].attrib.dec_line = bDecLine;
                    }
                }
            }
            else if (mRectSelection.Valid())
            {
                Rect clipped(mRectSelection);
                clipped.Normalize();
                clipped.ClipTo(inner);

                for (int64_t y = clipped.t; y < clipped.b; y++)
                {
                    for (int64_t x = clipped.l; x < clipped.r; x++)
                    {
                        int64_t index = (y * mWidth) + x;
                        bool bDecLine = mBuffer[index].attrib.dec_line; // preserve declin setting
                        mBuffer[index].attrib = kSelectedText;
                        mBuffer[index].attrib.dec_line = bDecLine; 
                    }
                }
            }
        }





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


        mbWindowInvalid = false;
    }


    void ConsoleWin::Clear(ZAttrib attrib, bool bGradient)
    {
        mClearAttrib = attrib;
        mbGradient = bGradient;
        for (size_t i = 0; i < mBuffer.size(); i++)
        {
            mBuffer[i].c = ' ';
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

    void ConsoleWin::SetEnableSelection(bool bEnable)
    {
        mbSelectionEnabled = bEnable;
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

        int64_t firstDrawRow = mTopVisibleRow;

        Rect drawArea;
        GetInnerArea(drawArea);
        Rect textArea = GetTextOuputRect(mText);
        int64_t shiftX = drawArea.l;
        int64_t shiftY = drawArea.t - firstDrawRow;     // shift the text output so that the in-view text starts with the firstdrawrow

        textArea.l += shiftX;
        textArea.r += shiftX;
        textArea.t += shiftY;
        textArea.b += shiftY;


        bool bDrawScrollbar = false;

        if (bAutoScrollbar)
        {
//            int64_t nRows = GetTextOutputRows(mText, drawArea.w());
            int64_t nRows = textArea.h();
            Rect sb(drawArea.r, drawArea.t, drawArea.r + 1, drawArea.b);    // drawArea is reduced by 1 for scrollbar
            bDrawScrollbar = nRows > drawArea.h();
            DrawScrollbar(sb, 0, nRows - drawArea.h() - 1, mTopVisibleRow, kAttribScrollbarBG, kAttribScrollbarThumb);

//            drawArea.r--;   // adjust draw area for text if scrollbar is visible
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

    bool TableWin::Init(const Rect& r)
    {
        mRenderedW = 0;
        mRenderedH = 0;
        mTable.Clear();

        return InfoWin::Init(r);
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
//            drawArea.r--;   // allow for scrollbar
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

        DrawCharClipped(SCROLLBAR_TRACK_TOP, trackRect.l, trackRect.t, track);
        DrawCharClipped(SCROLLBAR_TRACK_BOTTOM, trackRect.l, trackRect.b-1,  track);
        trackRect.t++;
        trackRect.b--;

        Fill(SCROLLBAR_TRACK_CENTER, trackRect, track);

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

        Fill(SCROLLBAR_THUMB, rThumb, thumb);
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

        gConsole.Invalidate();
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
                return OnKey(VK_DOWN, (char)(mHeight / 4));
            }
            else
            {
                return OnKey(VK_UP, (char)(mHeight / 4));
            }
        }

        return ConsoleWin::OnMouse(event);
    }

    void InfoWin::GetInnerArea(Rect& r) const
    {
        ConsoleWin::GetInnerArea(r);
//        if (bAutoScrollbar)
//            r.r--;
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

                mSelection.start = CursorToTextIndex(localcoord);
                mSelection.end = mSelection.start;
                gConsole.Invalidate();
                mbMouseCapturing = true;
                return true;
            }
            else if (event.dwButtonState == 0 && mbMouseCapturing)
            {
                // left button up
                mSelection.end = CursorToTextIndex(localcoord);

                if (mSelection.start == mSelection.end)
                {
                    ClearSelection();
                }

                gConsole.Invalidate();
                mbMouseCapturing = false;
                return true;
            }
        }
        else if (event.dwEventFlags == MOUSE_MOVED)
        {
            // mouse moved
            if (mbMouseCapturing)
            {
                mSelection.end = CursorToTextIndex(localcoord);
                gConsole.Invalidate();
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
        gConsole.SetCursorPosition(global, true);
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

/*    bool TextEditWin::IsIndexInSelection(int64_t i)
    {
        int64_t normalizedStart = mSelectionStartIndex;
        int64_t normalizedEnd = mSelectionEndIndex;
        if (normalizedEnd < normalizedStart)
        {
            normalizedStart = mSelectionEndIndex;
            normalizedEnd = mSelectionStartIndex;
        }

        return (i >= normalizedStart && i < normalizedEnd);
    }*/

    string TextEditWin::GetSelectedText()
    {
        if (!IsTextSelected())
            return "";

        mSelection.Normalize();
        return mText.substr(mSelection.start, mSelection.end- mSelection.start);
    }

    void TextEditWin::AddUndoEntry()
    {
        undoEntry entry(mText, CursorToTextIndex(mLocalCursorPos), mSelection);
        mUndoEntryList.emplace_back(std::move(entry));
    }

    void TextEditWin::Undo()
    {
        if (mUndoEntryList.empty())
            return;

        undoEntry entry(*mUndoEntryList.rbegin());
        mUndoEntryList.pop_back();

        mText = entry.text;
        mSelection = entry.selection;
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

        Rect drawArea;
        GetInnerArea(drawArea);


        COORD cursor((SHORT)drawArea.l, (SHORT)(drawArea.t-firstVisibleRow));

        std::vector<ZAttrib> attribs;
        attribs.resize(mText.size());


        for (size_t textindex = 0; textindex < mText.length(); textindex++)
        {
            attribs[textindex] = kRawText;
        }

        for (size_t textindex = 0; textindex < mText.length(); textindex++)
        {
            if (mSelection.IsInside(textindex))
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
                    mSelection.start = 0;
                    mSelection.end = mText.length();
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
        Rect drawArea;
        GetInnerArea(drawArea);

        int64_t i = (coord.Y + firstVisibleRow - drawArea.t) * mWidth + coord.X - drawArea.l;
        return std::min<size_t>(i, mText.size());
    }

    COORD TextEditWin::TextIndexToCursor(int64_t i)
    {
        if (i > (int64_t)mText.size())
            i = (int64_t)mText.size();

        if (mWidth > 0)
        {
            Rect drawArea;
            GetInnerArea(drawArea);

            COORD c;
            c.X = (SHORT)(drawArea.l + i % mWidth);
            c.Y = (SHORT)(drawArea.t + (i / mWidth) - firstVisibleRow);
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

        mSelection.Normalize();
        int64_t selectedChars = mSelection.end - mSelection.start;

        mText.erase(mSelection.start, selectedChars);

        int curindex = (int)CursorToTextIndex(mLocalCursorPos);
        if (curindex > mSelection.start)
            curindex -= (int)(curindex - mSelection.start);
        UpdateCursorPos(TextIndexToCursor(curindex));

        ClearSelection();
    }

    void TextEditWin::ClearSelection()
    {
        mSelection.Clear();
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
            if (!mSelection.Valid())
                mSelection.start = CursorToTextIndex(mLocalCursorPos);
            mSelection.end = mSelection.start;
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

        int64_t w = gConsole.Width();
        int64_t h = gConsole.Height();

        if (w < 1)
            w = 1;
        if (h < 8)
            h = 8;

        Rect drawArea;
        helpTableWin.GetInnerArea(drawArea);
        int64_t drawWidth = drawArea.r - drawArea.l - 2;

        Table::SetDecLineBorders(helpTableWin.mTable, COL_BLUE);

        helpTableWin.Init(Rect(0, 0, w, h));
        helpTableWin.Clear(kAttribHelpBG, true);
        helpTableWin.SetEnableFrame();
        helpTableWin.SetEnableSelection();
        helpTableWin.bAutoScrollbar = true;
        helpTableWin.mTopVisibleRow = 0;

        helpTableWin.positionCaption[ConsoleWin::Position::LT] = "Environment";

        helpTableWin.mTable.AddRow(SectionStyle, " Launch Arguments ");
        helpTableWin.mTable.AddRow(GetCommandLineA());
        helpTableWin.mTable.AddSeparator();
                                       
        helpTableWin.mTable.AddRow(SectionStyle, " Environment Variables ");
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
        size_t paircount = keyVals.size();
        size_t count = 0;
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

            helpTableWin.mTable.AddRow(Table::Cell(sKey, keyStyle), Table::Cell(sVal, valueStyle));

            if (count+1 < paircount)
                helpTableWin.mTable.AddSeparator();
            count++;
        }


        helpTableWin.mTable.AlignWidth(drawWidth);

        //helpWin.mText = (string)varTable;
        helpTableWin.UpdateCaptions();
        gConsole.Invalidate();
    }
};  // namespace CLP



#else
    int16_t CLP::ScreenW()
    { 
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
            return 120; // some default for failure
        return w.ws_col;
    }

    int16_t CLP::ScreenH() 
    { 
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
            return 30; // some default for failure
        return w.ws_row;
    }

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

            gConsole.Invalidate();

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
        gConsole.Invalidate();

        return true;
    }
}
#endif // ENABLE_COMMAND_HISTORY
