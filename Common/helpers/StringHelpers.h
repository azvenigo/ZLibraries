//////////////////////////////////////////////////////////////////////////////////////////////////
// StringHelpers
// Purpose: A collection of String formating and conversion utilities. 
//
// MIT License
// Copyright 2019 Alex Zvenigorodsky
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#pragma once

#include <stdint.h>
#include <string>
#include <cctype>
#include <locale>
#include <codecvt>
#include <algorithm>
#include <list>
#include <vector>
#include <map>
#include <set>

typedef std::set<std::string>   tStringSet;
typedef std::list<std::string>  tStringList;
typedef std::vector<std::string> tStringArray;

namespace SH
{
    const char kCharSplitToken = -77; // extended ascii character |
    const char kCharEqualityToken = -9; // extended ascii character

    //////////////////////////////////////////////////////////////////////////////////////////////
    // Common conversions
    // some commonly used conversions
    inline void     makelower(std::string& rhs) { std::transform(rhs.begin(), rhs.end(), rhs.begin(), [](unsigned char c) { return (unsigned char)std::tolower(c); }); }
    inline void     makeupper(std::string& rhs) { std::transform(rhs.begin(), rhs.end(), rhs.begin(), [](unsigned char c) { return (unsigned char)std::toupper(c); }); }

    //////////////////////////////////////////////////////////////////////////////////////////////
    static const int64_t kAuto = 0LL;
    static const int64_t kBytes = 1LL;
    static const int64_t kK = 1000LL;
    static const int64_t kKB = 1000LL;
    static const int64_t kKiB = 1024LL;

    static const int64_t kM = 1000LL * 1000LL;
    static const int64_t kMB = 1000LL * 1000LL;
    static const int64_t kMiB = 1024LL * 1024LL;

    static const int64_t kG = 1000LL * 1000LL * 1000LL;
    static const int64_t kGB = 1000LL * 1000LL * 1000LL;
    static const int64_t kGiB = 1024LL * 1024LL * 1024LL;

    static const int64_t kT = 1000LL * 1000LL * 1000LL * 1000LL;
    static const int64_t kTB = 1000LL * 1000LL * 1000LL * 1000LL;
    static const int64_t kTiB = 1024LL * 1024LL * 1024LL * 1024LL;

    static const int64_t kP = 1000LL * 1000LL * 1000LL * 1000LL * 1000LL;
    static const int64_t kPB = 1000LL * 1000LL * 1000LL * 1000LL * 1000LL;
    static const int64_t kPiB = 1024LL * 1024LL * 1024LL * 1024LL * 1024LL;


    struct sSizeEntry
    {
        const char* label;
        int64_t     value;
    };

    static const sSizeEntry sizeEntryTable[] =
    {
        { "B"     , 1 },

        { "K"     , kK},
        { "KB"    , kKB},
        { "KIB"   , kKiB},

        { "M"     , kM},
        { "MB"    , kMB},
        { "MIB"   , kMiB},

        { "G"     , kG},
        { "GB"    , kGB},
        { "GIB"   , kGiB},

        { "T"     , kT},
        { "TB"    , kTB},
        { "TIB"   , kTiB},

        { "P"     , kP},
        { "PB"    , kPB},
        { "PIB"   , kPiB}
    };



    static const int sizeEntryTableSize = sizeof(sizeEntryTable) / sizeof(sSizeEntry);
    typedef std::set<std::string>                       tStringSet;
    	
    bool            ToBool(std::string sVal);
    double          ToDouble(std::string sVal);

    std::string     ToHexString(uint32_t nVal);
    std::string	    ToHexString(uint64_t nVal);


    std::string	    FromInt(int64_t nVal);
    std::string	    FromBin(uint8_t* pBuf, int32_t nLength);
    std::string	    FromDouble(double fVal, int64_t nPrecision = kAuto);

    std::string     wstring2string(const std::wstring& sVal);
    std::wstring    string2wstring(const std::string& sVal);

    std::string     FormatFriendlyBytes(uint64_t nBytes, int64_t sizeType = kAuto, bool bIncludeBytes = false);
    std::string     ToUserReadable(double fValue, size_t precision = 2);
    int64_t         ToInt(std::string sReadable);

    std::string     FromVector(tStringArray& stringVector, const char token = kCharSplitToken);
    void            ToVector(const std::string& sEncoded, tStringArray& outStringVector, const char token = kCharSplitToken);

    std::string     FromList(tStringList& stringList, const char token = kCharSplitToken);
    void            ToList(const std::string& sEncoded, tStringList& outStringList, const char token = kCharSplitToken);


    std::string     FromMap(const std::map<std::string, std::string>& stringMap, const char token = kCharSplitToken);
    void            ToMap(const std::string& sEncoded, std::map<std::string, std::string>& outStringMap, const char token = kCharSplitToken);

    std::string     FromSet(tStringSet& stringSet, const char delimiter = kCharSplitToken);
    void            ToSet(const std::string& sEncoded, tStringSet& outStringSet, const char token = kCharSplitToken);

    void            SplitToken(std::string& sBefore, std::string& sAfter, const std::string& token);


    std::string     replaceTokens(std::string input, const std::string& token, const std::string& value);

    size_t          FindMatching(const std::string& s, size_t i);   // given a character in s at i, find the accomanying closure. for example '"' -> '"' or '{' -> '}'  Different pairs (excluding the closure token for current) inside the enclosure are skipped
    bool            ContainsWhitespace(const std::string& s, bool bSkipQuotes = false);   // if bSkipQuotes, it ignores whitespaces in enslosures

    std::string     convertToASCII(const std::string& input);
    bool            Load(const std::string& filename, std::string& s);
    bool            Save(const std::string& filename, const std::string& s);

    

    bool Compare(const std::string& a, const std::string& b, bool bCaseSensitive);
    bool Contains(const std::string& a, const std::string& sub, bool bCaseSensitive);
    bool StartsWith(const std::string& a, const std::string& prefix, bool bCaseSensitive = true);
    bool EndsWith(const std::string& a, const std::string& suffix, bool bCaseSensitive = true);

    template <class T>
    inline bool InContainer(const std::string& s, T container, bool bCaseSensitive = true)
    {
        for (const auto& s2 : container)
        {
            if (SH::Compare(s, s2, bCaseSensitive))
                return true;
        }

        return false;
    }
    
    // URL encoding
    inline bool URL_Safe(char c)
    {
        if (c >= 'A' && c <= 'Z')
            return true;
        if (c >= 'a' && c <= 'z')
            return true;
        if (c >= '0' && c <= '9')
            return true;
        return false;
    }

    inline std::string URL_Encode(const std::string& sRaw)
    {
        char byteToAscii[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

        std::string sResult;
        for (uint8_t c : sRaw)
        {
            if (URL_Safe(c))
                sResult += c;
            else
            {
                sResult += "%";
                sResult += byteToAscii[c >> 4];
                sResult += byteToAscii[c & 0x0F];
            }
        }
        return sResult;
    }

    inline std::string URL_Decode(const std::string& sEncoded)
    {
        std::string sResult;
        for (size_t i = 0; i < sEncoded.length(); i++)
        {
            char c = *(sEncoded.c_str()+i);

            if (c == '%')
            {
                char upper = *(sEncoded.c_str() + i + 1);
                if (upper >= 'A' && upper <= 'F')
                    upper -= 'A';
                else
                    upper -= '0';

                char lower = *(sEncoded.c_str() + i + 2);
                if (lower >= 'A' && lower <= 'F')
                    lower = lower - 'A' + 0xA;
                else
                    lower -= '0';

                char value = (upper << 4) + lower;
                sResult += value;
                i += 2;
            }
            else
                sResult += c;
        }
        return sResult;
    }


};
