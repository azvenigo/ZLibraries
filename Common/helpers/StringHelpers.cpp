// MIT License
// Copyright 2019 Alex Zvenigorodsky
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "helpers/StringHelpers.h"
#include "helpers/LoggingHelpers.h"
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cctype>

using namespace std;

#ifdef _WIN32
#pragma warning(disable : 4244)
#endif

string	SH::wstring2string(const wstring& sVal)
{
    return string(sVal.begin(), sVal.end());
}

wstring SH::string2wstring(const string& sVal)
{
    return wstring(sVal.begin(), sVal.end());
}

string SH::ToHexString(uint32_t nVal)
{
	char buf[64];
	sprintf(buf, "0x%" PRIX32, nVal);
	string sRet;
	sRet.assign(buf);
	return sRet;
}


string SH::ToHexString(uint64_t nVal)
{
	char buf[64];
	sprintf(buf, "0x%" PRIX64, nVal);
	string sRet;
	sRet.assign(buf);
	return sRet;
}

string SH::FromBin(uint8_t* pBuf, int32_t nLength)
{
	string sRet;
	char buf[64];
	for (int32_t i = 0; i < nLength; i++)
	{
		sprintf(buf, "%02" PRIX8, *(pBuf + i));
		sRet.append(buf);
	}

	return sRet;
}

string	SH::FromInt(int64_t nVal)
{
    char buf[32];
    sprintf(buf, "%" PRIi64, nVal);

    return string(buf);
}

double SH::ToDouble(string sVal)
{
    try
    {
        return stod(sVal, NULL);
    }
    catch (...)
    {
    }

    return 0;
}

string	SH::FromDouble(double fVal, int64_t nPrecision)
{
    if (nPrecision == kAuto)
        return std::to_string(fVal);

    std::stringstream ss;
    ss << std::fixed << std::setprecision(nPrecision) << fVal;

    return ss.str();
}

bool SH::ToBool(string sValue)
{
    transform(sValue.begin(), sValue.end(), sValue.begin(), [](unsigned char c) { return (unsigned char)tolower(c); });
    return sValue == "1" ||
        sValue == "t" ||
        sValue == "true" ||
        sValue == "y" ||
        sValue == "yes" ||
        sValue == "on";
}

void SH::SplitToken(string& sBefore, string& sAfter, const string& token)
{
    size_t pos = sAfter.find(token);
    if (pos == string::npos)
    {
        sBefore = sAfter;
        sAfter = "";
        return;
    }

    sBefore = sAfter.substr(0, pos).c_str();
    sAfter = sAfter.substr(pos + token.length()).c_str();
}

string SH::replaceTokens(std::string input, const std::string& token, const std::string& value)
{
    size_t pos = 0;
    while ((pos = input.find(token, pos)) != std::string::npos)
    {
        input.replace(pos, token.length(), value);
        pos += value.length();
    }
    return input;
}

string SH::FormatFriendlyBytes(uint64_t nBytes, int64_t sizeType, bool bIncludeBytes)
{
    char buf[128];

    switch (sizeType)
    {
    case kKiB:
        sprintf(buf, "%" PRId64 "KiB", nBytes / kKiB);
        return string(buf);

    case kMiB:
        sprintf(buf, "%" PRId64 "MiB", nBytes / kMiB);
        return string(buf);

    case kGiB:
        sprintf(buf, "%" PRId64 "GiB", nBytes / kGiB);
        return string(buf);
    default:
        break;
    }

    if (nBytes < kKiB)
    {
        sprintf(buf, "%" PRId64 "B", nBytes);
        bIncludeBytes = false; // already reported in bytes
    }
    else if (nBytes < kMiB)
    {
        double fKiB = (double)nBytes / (double)kKiB;
        sprintf(buf, "%0.1fKiB", fKiB);
    }
    else if (nBytes < kGiB)
    {
        double fMiB = (double)nBytes / (double)kMiB;
        sprintf(buf, "%0.1fMiB", fMiB);
    }
    else if (nBytes < kTiB)
    {
        double fGiB = (double)nBytes / (double)kGiB;
        sprintf(buf, "%0.1fGiB", fGiB);
    }
    else
    {
        double fTiB = (double)nBytes / (double)kTiB;
        sprintf(buf, "%0.1fTiB", fTiB);
    }

    string s(buf);
    if (bIncludeBytes)
    {
        sprintf(buf, " (%" PRId64 "B)", nBytes);
        s += string(buf);
    }

    return s;
}
bool SH::Compare(const string& a, const string& b, bool bCaseSensitive)
{
    if (bCaseSensitive)
        return a.compare(b) == 0;

    return ((a.size() == b.size()) && equal(a.begin(), a.end(), b.begin(), [](auto char1, auto char2) { return toupper(char1) == toupper(char2); }));
}

bool SH::StartsWith(const std::string& a, const std::string& prefix, bool bCaseSensitive)
{
    size_t len = prefix.length();
    if (a.length() < len)
        return false;

    if (bCaseSensitive)
        return prefix.compare(a.substr(0, len)) == 0;

    return (equal(a.begin(), a.begin() + len, prefix.begin(), [](auto char1, auto char2) { return toupper(char1) == toupper(char2); }));
}

bool SH::EndsWith(const std::string& a, const std::string& suffix, bool bCaseSensitive)
{
    size_t len = suffix.size();
    if (a.size() < len)
        return false;

    size_t offset = a.size() - len;
    if (bCaseSensitive)
        return suffix.compare(a.substr(offset)) == 0;

    return (equal(a.begin() + offset, a.end(), suffix.begin(), [](auto char1, auto char2) { return toupper(char1) == toupper(char2); }));
}

bool SH::Contains(const string& a, const string& sub, bool bCaseSensitive)
{
    if (bCaseSensitive)
        return a.find(sub) != string::npos;

    string a2(a);
    string sub2(sub);

    makeupper(a2);
    makeupper(sub2);
    return a2.find(sub2) != string::npos;
}


// Converts user readable numbers into ints
// Supports hex (0x12345)
// Strips commas (1,000,000)
// Supports trailing scaling labels  (k, kb, kib, m, mb, mib, etc.)
int64_t SH::ToInt(string sReadable)
{
    try
    {
        makeupper(sReadable);

        // strip any commas in case human readable string has those
        sReadable.erase(remove(sReadable.begin(), sReadable.end(), ','), sReadable.end());


        // Determine if this is a hex value
        int32_t nNumberBase = 10;
        if (sReadable.substr(0, 2) == "0X")
        {
            nNumberBase = 16;
            sReadable = sReadable.substr(2);
        }


        int32_t nReadableLength = (int32_t)sReadable.length();

        // count how many chars in the trailing label (if any)
        int32_t nLabelChars = 0;
        for (int32_t i = nReadableLength - 1; i >= 0; i--)
        {
            char c = sReadable[i];
            if (nNumberBase == 10 && (c < 'A' || c > 'Z'))
                break;
            if (nNumberBase == 16 && (c < 'G' || c > 'Z'))
                break;

            nLabelChars++;
        }

        for (int i = 0; i < sizeEntryTableSize; i++)
        {
            const sSizeEntry& entry = sizeEntryTable[i];

            if (sReadable.substr(nReadableLength - nLabelChars).compare(entry.label) == 0)
            {
                int64_t nOut = strtoll(sReadable.substr(0, nReadableLength - nLabelChars).c_str(), NULL, nNumberBase);
                return nOut * entry.value;
            }
        }

        return strtoll(sReadable.c_str(), NULL, nNumberBase);
    }
    catch (...)
    {
    }

    return 0;
}

bool SH::IsANumber(std::string sReadable)
{
    try
    {
        makeupper(sReadable);

        // strip any commas in case human readable string has those
        sReadable.erase(remove(sReadable.begin(), sReadable.end(), ','), sReadable.end());

        if (sReadable.empty())
            return false;

        // strip any size labels
        int32_t nReadableLength = (int32_t)sReadable.length();
        int32_t nLabelChars = 0;
        for (int32_t i = nReadableLength - 1; i >= 0; i--)
        {
            char c = sReadable[i];
            if (c < 'A' || c > 'Z')
                break;
            nLabelChars++;
        }
        if (nLabelChars > 0)
        {
            string sLabel(sReadable.substr(nReadableLength - nLabelChars));

            for (int i = 0; i < sizeEntryTableSize; i++)
            {
                const sSizeEntry& entry = sizeEntryTable[i];
                if (sLabel == entry.label)
                {
                    sReadable = sReadable.substr(0, sReadable.size() - strlen(entry.label));
                    break;
                }
            }
        }

        if (sReadable.empty())
            return false;

        // strip leading negative if there is one
        if (sReadable[0] == '-')
            sReadable = sReadable.substr(1, sReadable.size() - 1);

        // if there is more than one period, that's a no no
        if (std::count(sReadable.begin(), sReadable.end(), '.') > 1)
        {
            return false;
        }

        // is it a hex number? If so, allow only hex digits
        if (sReadable.size() > 2 && SH::StartsWith(sReadable, "0X"))
        {
            sReadable = sReadable.substr(2, sReadable.size() - 2);   // strip the 0x
            if (std::all_of(sReadable.begin(), sReadable.end(), 
                [](unsigned char c) 
                { 
                    return std::isxdigit((int)c); 
                })) // are all characters a hex digit
            {
                return true;    
            }

            return false;   // not a valid hex
        }

        if (std::all_of(sReadable.begin(), sReadable.end(), [](unsigned char c) { return std::isdigit((int)c) || c == '.'; })) // are all characters a digit or a period
        {
            return true;
        }
    }
    catch (...)
    {
    }


    return false;
}


string SH::ToUserReadable(double fValue, size_t precision)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << fValue;
    return out.str();
}

string SH::FromVector(tStringArray& stringVector, const char token)
{
    string sValue;
    for (uint32_t i = 0; i < stringVector.size(); i++)
    {
        assert(stringVector[i].find(token) == string::npos);   // cannot encode extended ascii character token

        if (!stringVector[i].empty())
        {
            sValue += stringVector[i] + token;
        }
    }

    if (!sValue.empty())
        sValue = sValue.substr(0,sValue.length() - 1);	// remove the trailing token

    return sValue;
}

void SH::ToVector(const string& sEncoded, tStringArray& outStringVector, const char token)
{
    std::size_t current, previous = 0;

    current = sEncoded.find(token);
    while (current != std::string::npos)
    {
        outStringVector.push_back(sEncoded.substr(previous, current - previous));
        previous = current + 1;
        current = sEncoded.find(token, previous);
    }
    outStringVector.push_back(sEncoded.substr(previous, current - previous));
}

string SH::FromList(tStringList& stringList, const char token)
{
    string sValue;
    for (auto& s : stringList)
    {
        assert(s.find(token) == string::npos);   // cannot encode extended ascii character token

        if (!s.empty())
        {
            sValue += s + token;
        }
    }

    if (!sValue.empty())
        sValue = sValue.substr(0, sValue.length() - 1);	// remove the trailing token

    return sValue;
}

void SH::ToList(const string& sEncoded, tStringList& stringList, const char token)
{
    std::size_t current, previous = 0;

    current = sEncoded.find(token);
    while (current != std::string::npos)
    {
        stringList.push_back(sEncoded.substr(previous, current - previous));
        previous = current + 1;
        current = sEncoded.find(token, previous);
    }
    stringList.push_back(sEncoded.substr(previous, current - previous));
}


string SH::FromSet(tStringSet& stringSet, const char token)
{
    string sValue;
    for (auto s : stringSet)
    {
        assert(s.find(token) == string::npos);   // cannot encode extended ascii character token

        if (!s.empty())
        {
            sValue += s + token;
        }
    }

    if (!sValue.empty())
        sValue = sValue.substr(0, sValue.length() - 1);	// remove the trailing token

    return sValue;
}

void SH::ToSet(const std::string& sEncoded, tStringSet& outStringSet, const char token)
{
    std::size_t current, previous = 0;

    current = sEncoded.find(token);
    while (current != std::string::npos)
    {
        outStringSet.insert(sEncoded.substr(previous, current - previous));
        previous = current + 1;
        current = sEncoded.find(token, previous);
    }
    outStringSet.insert(sEncoded.substr(previous, current - previous));
}


string SH::FromMap(const map<string, string>& stringMap, const char token)
{
    string sReturn;
    for (map<string, string>::const_iterator it = stringMap.begin(); it != stringMap.end(); it++)
    {
        string sKey = (*it).first;
        string sValue = (*it).second;

        if (sKey.find(token) != string::npos ||
            sValue.find(token) != string::npos ||
            sKey.find(kCharEqualityToken) != string::npos ||
            sValue.find(kCharEqualityToken) != string::npos)
        {
            assert(false);
            cerr << "Converting to a string array doesn't support extended ascii characters.\n";
            return "";
        }
        if (!sKey.empty())
            sReturn += sKey + kCharEqualityToken + sValue + token;
    }

    if (!sReturn.empty())
        sReturn = sReturn.substr(sReturn.length() - 1);	// remove the trailing token

    return sReturn;
}

void SH::ToMap(const string& sEncoded, map<string, string>& outStringMap, const char token)
{
    std::size_t current, previous = 0;

    current = sEncoded.find(token);
    while (current != std::string::npos)
    {
        std::size_t equalIndex = sEncoded.find(kCharEqualityToken, previous);
        if (equalIndex != std::string::npos)
        {
            string sKey(sEncoded.substr(previous, equalIndex));
            string sVal(sEncoded.substr(equalIndex + 1, current));
            outStringMap[sKey] = sVal;
        }

        previous = current + 1;
        current = sEncoded.find(token, previous);
    }

    // final value
    std::size_t equalIndex = sEncoded.find(kCharEqualityToken, previous);
    if (equalIndex != std::string::npos)
    {
        string sKey(sEncoded.substr(previous, equalIndex));
        string sVal(sEncoded.substr(equalIndex + 1, current));
        outStringMap[sKey] = sVal;
    }
}

bool SH::ContainsWhitespace(const std::string& s, bool bSkipQuotes)
{
    if (bSkipQuotes)
    {
        for (size_t i = 0; i < s.length(); i++)
        {
            char c = s[i];
            if (std::isspace((uint8_t)c))
                return true;
            if (c == '\'' || c == '\"')
            {
                size_t match = FindMatching(s, i);     // skip quote
                if (match != string::npos)
                    i = match;
            }
        }

        return false;
    }

    return std::any_of(s.begin(), s.end(), [](char c) { return std::isspace((uint8_t)(c)); });
}


size_t SH::FindMatching(const std::string& s, size_t i)
{
    // any enclosing pairs should be accounted for......
    // for example <"blah" [blah] "<<<<" >

    if (i == string::npos || i + 1 > s.length())
        return string::npos;

    char start = s[i];
    char end;
    switch (start)
    {
    case '\"':          // ""
        end = '\"';
        break;
    case '\'':          // ''
        end = '\'';
        break;
    case  '{':          // {}
        end = '}';
        break;
    case '[':           // []
        end = ']';
        break;
    case '<':           // <>
        end = '>';
        break;
    case '(':           // ()
        end = ')';
        break;
    case '`':
        end = '`';      // ``
        break;
    default:
        return string::npos;
    }

    do
    {
        i++;

        if (i < s.length() && s[i] == end)
            return i;

        if (s[i] == '\"' || s[i] == '\'' || s[i] == '{' || s[i] == '[' || s[i] == '[' || s[i] == '<' || s[i] == '(' || s[i] == '`')   // another enclosure?
        {
            size_t j = FindMatching(s, i); // find that enclosure
            if (j != string::npos)
            {
                i = j;  // found another enclosure, advance i
            }
        }
    } while (i < s.length());

    return string::npos;
}

#ifdef _DEBUG
class FindMatchingUnitTest
{
public:
    FindMatchingUnitTest()
    {
        // empty
        string s1("");
        assert(SH::FindMatching(s1, 0) == string::npos);
        assert(SH::FindMatching(s1, 6) == string::npos);

        string s2("<>");
        assert(SH::FindMatching(s2, 0) == 1);
        assert(SH::FindMatching(s2, 1) == string::npos);

        string s3("<test>");
        assert(SH::FindMatching(s3, 0) == 5);
        assert(SH::FindMatching(s3, 1) == string::npos);

        string s4("[enclose<>enclose]");
        assert(SH::FindMatching(s4, 0) == 17);

        string s5("[< <<>><<<>>> >]");
        assert(SH::FindMatching(s5, 0) == 15);
        assert(SH::FindMatching(s5, 1) == 14);

        string s6("[Dave's Crabshack]");
        assert(SH::FindMatching(s6, 0) == 17);
        assert(SH::FindMatching(s6, 6) == string::npos);



        // malformed tests
        string m1("<");
        assert(SH::FindMatching(m1, 0) == string::npos);

        string m2("<<blah>");   // malformed
        assert(SH::FindMatching(m2, 0) == string::npos);
        assert(SH::FindMatching(m2, 1) == 6);   // however if we look for enclosure starting with second, that should work

        string m3("{}\"\'blah\'<>");    // no closing '"'
        assert(SH::FindMatching(m3, 1) == string::npos);    // starting on a '}' is not correct
        assert(SH::FindMatching(m3, 2) == string::npos);
    }
};

FindMatchingUnitTest gFindMatchingUnitTestInstance;

#endif


std::string SH::convertToASCII(const std::string& input) 
{
    size_t startIndex = 0;
    // If UTF-8 BOM at the beginning, skip it
    if (input.length() >= 3)
    {
        if (input[0] == 0xef && input[1] == 0xbb && input[2] == 0xbf)
            startIndex = 3;
    }


    std::string output;
    for (size_t i = 0; i < input.length(); ++i) 
    {
        unsigned char c = static_cast<unsigned char>(input[i]);

        // Check for UTF-8 encoded curly quotes and replace them
        if (c == 0xE2 && i + 2 < input.length())
        {
            // Handle double curly quotes
            uint8_t c1 = (uint8_t)input[i + 1];
            uint8_t c2 = (uint8_t)input[i + 2];

            if (c1 == 0x80)
            {
                if (c2 == 0x9C || c2 == 0x9D)
                { // Opening curly double quote 
                    output += '"';
                    i += 2;
                }
                else if (c2 == 0x92 || c2 == 0x98 || c2 == 0x99)
                {
                    output += '\'';
                    i += 2;
                }
            }
        }
        else if (c < 128)
        {
            // If it's a valid ASCII character, append it to output
            output += c;
        }
        else
        {
            // Optionally, handle other non-ASCII characters (e.g., skip or replace them)
            output += '?';  // Replace unsupported characters with '?'
        }
    }
    return output;
}

bool SH::Load(const std::string& filename, std::string& s)
{
    ifstream inFile(filename, std::ios::binary);
    if (!inFile)
    {
        assert(false);
        return false;
    }

    inFile.seekg(0, ios::end);
    size_t size = inFile.tellg();
    inFile.seekg(0, ios::beg);

    s.resize(size);
    inFile.read(&s[0], size);
    return true;
}

bool SH::Save(const std::string& filename, const std::string& s)
{
    ofstream outFile(filename, std::ios::binary|std::ios::trunc);
    if (!outFile)
    {
        assert(false);
        return false;
    }

    outFile.write(&s[0], s.length());
    return true;
}

