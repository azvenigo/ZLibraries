// MIT License
// Copyright 2019 Alex Zvenigorodsky
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "FNMatch.h"

using namespace std;

template<class S>
void findAndReplaceAll(S& data, const S& toSearch, const S& replaceStr)
{
    // Get the first occurrence
    size_t pos = data.find(toSearch);
    // Repeat till end is reached
    while (pos != S::npos)
    {
        // Replace this occurrence of Sub String
        data.replace(pos, toSearch.size(), replaceStr);
        // Get the next occurrence from the current position
        pos = data.find(toSearch, pos + replaceStr.size());
    }
}

bool FNMatch(const string& pattern, const string& search)
{
    if ((pattern.empty() || pattern == "*"))  // if we're not matching everything
        return true;

    if (pattern == search)  // exact match
        return true;

    string sPatternToUse(pattern);
    findAndReplaceAll(sPatternToUse, string("\\"), string("/"));
    findAndReplaceAll(sPatternToUse, string("/"), string("./"));
    findAndReplaceAll(sPatternToUse, string("*"), string(".*"));

    std::regex patternRegEx(sPatternToUse, std::regex_constants::ECMAScript | std::regex_constants::icase);
    return regex_match(search, patternRegEx);
}

bool FNMatch(const wstring& pattern, const wstring& search)
{
    if ((pattern.empty() || pattern == L"*"))  // if we're not matching everything
        return true;

    if (pattern == search)  // exact match
        return true;

    wstring sPatternToUse(pattern);
    findAndReplaceAll(sPatternToUse, wstring(L"\\"), wstring(L"/"));
    findAndReplaceAll(sPatternToUse, wstring(L"/"), wstring(L"./"));
    findAndReplaceAll(sPatternToUse, wstring(L"*"), wstring(L".*"));

    std::wregex patternRegEx(sPatternToUse, std::regex_constants::ECMAScript | std::regex_constants::icase);
    return regex_match(search, patternRegEx);
}