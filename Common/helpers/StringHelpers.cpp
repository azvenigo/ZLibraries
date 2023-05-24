// MIT License
// Copyright 2019 Alex Zvenigorodsky
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "helpers/StringHelpers.h"
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <iostream>
#include <sstream>
#include <iomanip>

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
    return stod(sVal, NULL);
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



string SH::FormatFriendlyBytes(uint64_t nBytes, int64_t sizeType)
{
    if (sizeType == kAuto)
    {
        if (nBytes > kTiB)   // TB show in GB
            sizeType = kGiB;
        if (nBytes > kGiB)	// GB show in MB
            sizeType = kMiB;
        else if (nBytes > kMiB)	// MB show in KB
            sizeType = kKiB;
        else
            sizeType = kBytes;
    }

    char buf[128];

    switch (sizeType)
    {
    case kKiB:
        sprintf(buf, "%" PRId64 "KiB", nBytes / kKiB);
        return string(buf);
        break;
    case kMiB:
        sprintf(buf, "%" PRId64 "MiB", nBytes / kMiB);
        return string(buf);
        break;
    case kGiB:
        sprintf(buf, "%" PRId64 "GiB", nBytes / kGiB);
        return string(buf);
        break;
    }

    if (nBytes > kGiB)       // return in MB
    {
        sprintf(buf, "%" PRId64 "MiB", nBytes / kMiB);
        return string(buf);
    }
    else if (nBytes > kMB)  // return in KB
    {
        sprintf(buf, "%" PRId64 "KiB", nBytes / kKiB);
        return string(buf);
    }

    else sprintf(buf, "%" PRId64 "bytes", nBytes);

    return string(buf);
}

bool SH::Compare(const string& a, const string& b, bool bCaseSensitive)
{
    if (bCaseSensitive)
        return a.compare(b) == 0;

    return ((a.size() == b.size()) && equal(a.begin(), a.end(), b.begin(), [](auto char1, auto char2) { return toupper(char1) == toupper(char2); }));
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
        if (c < 'A' || c > 'Z')
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

    int64_t nOut = strtoll(sReadable.c_str(), NULL, nNumberBase);
    return nOut;
}

// If the number is a power of two, converts to a more readable form
// example 32768   -> 32KiB
//         1048576 -> 1MiB
string SH::ToUserReadable(int64_t nValue)
{
    char buf[128];
    if (nValue % kPiB == 0)
        sprintf(buf, "%" PRId64 "PiB", nValue / kPiB);
    else if (nValue % kPB == 0)
        sprintf(buf, "%" PRId64 "PB", nValue / kPB);

    else if (nValue % kTiB == 0)
        sprintf(buf, "%" PRId64 "TiB", nValue / kTiB);
    else if (nValue % kTB == 0)
        sprintf(buf, "%" PRId64 "TB", nValue / kTB);

    else if (nValue % kGiB == 0)
        sprintf(buf, "%" PRId64 "GiB", nValue / kGiB);
    else if (nValue % kGB == 0)
        sprintf(buf, "%" PRId64 "GB", nValue / kGB);

    else if (nValue % kMiB == 0)
        sprintf(buf, "%" PRId64 "MiB", nValue / kMiB);
    else if (nValue % kMB == 0)
        sprintf(buf, "%" PRId64 "MB", nValue / kMB);

    else if (nValue % kKiB == 0)
        sprintf(buf, "%" PRId64 "KiB", nValue / kKiB);
    else if (nValue % kKB == 0)
        sprintf(buf, "%" PRId64 "KB", nValue / kKB);

    else sprintf(buf, "%" PRId64, nValue);

    return string(buf);
}

string SH::FromVector(vector<string>& stringVector)
{
    string sValue;
    for (uint32_t i = 0; i < stringVector.size(); i++)
    {
        assert(stringVector[i].find(kCharSplitToken) == string::npos);   // cannot encode extended ascii character kSplitToken

        if (!stringVector[i].empty())
        {
            sValue += stringVector[i] + kCharSplitToken;
        }
    }

    if (!sValue.empty())
        sValue = sValue.substr(0,sValue.length() - 1);	// remove the trailing kCharSplitToken

    return sValue;
}

void SH::ToVector(const string& sEncoded, vector<string>& outStringVector)
{
    std::size_t current, previous = 0;

    current = sEncoded.find(kCharSplitToken);
    while (current != std::string::npos)
    {
        outStringVector.push_back(sEncoded.substr(previous, current - previous));
        previous = current + 1;
        current = sEncoded.find(kCharSplitToken, previous);
    }
    outStringVector.push_back(sEncoded.substr(previous, current - previous));
}

string SH::FromSet(tStringSet& stringSet)
{
    string sValue;
    for (auto s : stringSet)
    {
        assert(s.find(kCharSplitToken) == string::npos);   // cannot encode extended ascii character kSplitToken

        if (!s.empty())
        {
            sValue += s + kCharSplitToken;
        }
    }

    if (!sValue.empty())
        sValue = sValue.substr(0, sValue.length() - 1);	// remove the trailing kCharSplitToken

    return sValue;
}

void SH::ToSet(const std::string& sEncoded, tStringSet& outStringSet)
{
    std::size_t current, previous = 0;

    current = sEncoded.find(kCharSplitToken);
    while (current != std::string::npos)
    {
        outStringSet.insert(sEncoded.substr(previous, current - previous));
        previous = current + 1;
        current = sEncoded.find(kCharSplitToken, previous);
    }
    outStringSet.insert(sEncoded.substr(previous, current - previous));
}


string SH::FromMap(const map<string, string>& stringMap)
{
    string sReturn;
    for (map<string, string>::const_iterator it = stringMap.begin(); it != stringMap.end(); it++)
    {
        string sKey = (*it).first;
        string sValue = (*it).second;

        if (sKey.find(kCharSplitToken) != string::npos ||
            sValue.find(kCharSplitToken) != string::npos ||
            sKey.find(kCharEqualityToken) != string::npos ||
            sValue.find(kCharEqualityToken) != string::npos)
        {
            assert(false);
            cerr << "Converting to a string array doesn't support extended ascii characters.\n";
            return "";
        }
        if (!sKey.empty())
            sReturn += sKey + kCharEqualityToken + sValue + kCharSplitToken;
    }

    if (!sReturn.empty())
        sReturn = sReturn.substr(sReturn.length() - 1);	// remove the trailing kCharSplitToken

    return sReturn;
}

void SH::ToMap(const string& sEncoded, map<string, string>& outStringMap)
{
    std::size_t current, previous = 0;

    current = sEncoded.find(kCharSplitToken);
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
        current = sEncoded.find(kCharSplitToken, previous);
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


