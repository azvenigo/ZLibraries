#include "CommandLineParser.h"
#include <algorithm>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <cctype>
#include <assert.h>
#include <stdarg.h> 
#include <sstream>

using namespace std;

struct sPowersEntry
{
    const char* label;
    int64_t     value;
};

static const sPowersEntry powerEntryTable[] =
{
    { "B"     , 1 },
    { "KB"    , 1 * 1000LL},
    { "KIB"   , 1 * 1024LL},
    { "MB"    , 1 * 1000LL * 1000LL},
    { "MIB"   , 1 * 1024LL * 1024LL},
    { "GB"    , 1 * 1000LL * 1000LL * 1000LL},
    { "GIB"   , 1 * 1024LL * 1024LL * 1024LL},
    { "TB"    , 1 * 1000LL * 1000LL * 1000LL * 1000LL},
    { "TIB"   , 1 * 1024LL * 1024LL * 1024LL * 1024LL},
    { "PB"    , 1 * 1000LL * 1000LL * 1000LL * 1000LL * 1000LL},
    { "PIB"   , 1 * 1024LL * 1024LL * 1024LL * 1024LL * 1024LL}
};

static const int powerEntryTableSize = sizeof(powerEntryTable) / sizeof(sPowersEntry);


int64_t IntFromUserReadable(string sReadable)
{
    std::transform(sReadable.begin(), sReadable.end(), sReadable.begin(), std::toupper);

    // strip any commas in case human readable string has those
    sReadable.erase(std::remove(sReadable.begin(), sReadable.end(), ','), sReadable.end());

    int32_t nReadableLength = (int32_t) sReadable.length();

    // count how many chars in the trailing label (if any)
    int32_t nLabelChars = 0;
    for (int32_t i = nReadableLength-1; i >= 0; i--)
    {
        char c = sReadable[i];
        if (c < 'A' || c > 'Z')
            break;

        nLabelChars++;
    }

    for (int i = 0; i < powerEntryTableSize; i++)
    {

        const sPowersEntry& entry = powerEntryTable[i];

        if (sReadable.substr(nReadableLength-nLabelChars).compare(entry.label)==0)
        {
            int64_t nOut = strtoll(sReadable.substr(0, nReadableLength-nLabelChars).c_str(), NULL, 10);
            return nOut*entry.value;
        }
    }

    int64_t nOut = strtoll(sReadable.c_str(), NULL, 10);
    return nOut;
}


CommandLineParser::CommandLineParser()
{
    mnRequiredUnnamed = 0;
    mUnnamedParameterDescriptors.reserve(16);
}

CommandLineParser::~CommandLineParser()
{
}

void CommandLineParser::RegisterDescription(const string& sDescription) 
{ 
    msDescription = sDescription;
}

void CommandLineParser::SetRequiredNumberOfUnnamedParameters(int64_t nRequired) 
{ 
    mnRequiredUnnamed = nRequired; 
}


bool CommandLineParser::RegisterNamedString(const string& sKey, string* pRegisteredString, bool bRequired)
{
    if (GetNamedDescriptor(sKey))
    {
        cerr << "sKey:" << sKey << " is already registered!";
        return false;
    }

    ParameterDescriptor desc;
    desc.msName = sKey;
    desc.mpValue = (void*)pRegisteredString;
    desc.mbRequired = bRequired;
    desc.mType = ParameterDescriptor::kString;
    mNamedParameterDescriptors[sKey] = desc;

    return true;
}

bool CommandLineParser::RegisterNamedInt64(const string& sKey, int64_t* pRegisteredInt64, bool bRequired, bool bRangeRestricted, int64_t nMinValue, int64_t nMaxValue)
{
    if (GetNamedDescriptor(sKey))
    {
        cerr << "sKey:" << sKey << " is already registered!";
        return false;
    }

    ParameterDescriptor desc;
    desc.msName = sKey;
    desc.mpValue = (void*)pRegisteredInt64;
    desc.mbRequired = bRequired;
    desc.mType = ParameterDescriptor::kInt64;
    desc.mbRangeRestricted = bRangeRestricted;
    desc.mnMinValue = nMinValue;
    desc.mnMaxValue = nMaxValue;
    mNamedParameterDescriptors[sKey] = desc;

    return true;
}

bool CommandLineParser::RegisterNamedBool(const string& sKey, bool* pRegisteredBool, bool bRequired)
{
    if (GetNamedDescriptor(sKey))
    {
        cerr << "sKey:" << sKey << " is already registered!";
        return false;
    }

    ParameterDescriptor desc;
    desc.msName = sKey;
    desc.mpValue = (void*)pRegisteredBool;
    desc.mbRequired = bRequired;
    desc.mType = ParameterDescriptor::kBool;
    mNamedParameterDescriptors[sKey] = desc;

    return true;
}

bool CommandLineParser::RegisterUnnamedString(const string& sDisplayName, string* pRegisteredString)
{
    int64_t nIndex = mUnnamedParameterDescriptors.size();

    mUnnamedParameterDescriptors.resize(nIndex+1);

    ParameterDescriptor desc;
    desc.msName = sDisplayName;
    desc.mnPosition = nIndex;
    desc.mpValue = (void*)pRegisteredString;
    desc.mType = ParameterDescriptor::kString;
    mUnnamedParameterDescriptors[nIndex] = desc;

    return true;
}

bool CommandLineParser::RegisterUnnamedInt64(const string& sDisplayName, int64_t* pRegisteredInt64, bool bRangeRestricted, int64_t nMinValue, int64_t nMaxValue)
{
    int64_t nIndex = mUnnamedParameterDescriptors.size();
    mUnnamedParameterDescriptors.resize(nIndex + 1);

    ParameterDescriptor desc;
    desc.msName = sDisplayName;
    desc.mnPosition = nIndex;
    desc.mpValue = (void*)pRegisteredInt64;
    desc.mType = ParameterDescriptor::kInt64;
    desc.mbRangeRestricted = bRangeRestricted;
    desc.mnMinValue = nMinValue;
    desc.mnMaxValue = nMaxValue;
    mUnnamedParameterDescriptors[nIndex] = desc;

    return true;
}

bool CommandLineParser::RegisterUnnamedBool(const string& sDisplayName, bool* pRegisteredBool)
{
    int64_t nIndex = mUnnamedParameterDescriptors.size();
    mUnnamedParameterDescriptors.resize(nIndex + 1);

    ParameterDescriptor desc;
    desc.msName = sDisplayName;
    desc.mnPosition = nIndex;
    desc.mpValue = (void*)pRegisteredBool;
    desc.mType = ParameterDescriptor::kBool;
    mUnnamedParameterDescriptors[nIndex] = desc;

    return true;
}




bool CommandLineParser::Parse(int argc, char* argv[], bool bVerbose)
{
    msAppPath = argv[0];
    int64_t nUnnamedParametersFound = 0;        

    bool bError = false;
    if (argc < 2)
    {
        bError = true;
        goto errorprompt;
    }

    for (int i = 1; i < argc; i++)
    {
        std::string sParam(argv[i]);

        if (!sParam.empty())
        {
            if (sParam[0] == '-')
            {
                ////////////////////////////////////////////
                // Named parameter processing
                string sKey;
                string sValue;

                size_t nIndexOfColon = sParam.find(':');
                if (nIndexOfColon != string::npos)
                {
                    sKey = sParam.substr(1, nIndexOfColon-1).c_str();    // everything from first char to colon
                    sValue = sParam.substr(nIndexOfColon + 1).c_str(); // everything after the colon
                }
                else
                {
                    // flag with no value is the same as -flag:true
                    sKey = sParam.substr(1, nIndexOfColon).c_str(); 
                    sValue = "true";
                }

                ParameterDescriptor* pDesc = nullptr;
                if (!GetNamedDescriptor(sKey, &pDesc))
                {
                    cerr << "Error: Unknown parameter \"" << sParam << "\"\n";
                    bError = true;
                    continue;
                }

                switch (pDesc->mType)
                {
                case ParameterDescriptor::kBool:
                {
                    // if sValue is any of the following, it is considered TRUE
                    std::transform(sValue.begin(), sValue.end(), sValue.begin(), std::tolower);
                    bool bTrue = sValue == "1" ||
                        sValue == "t" ||
                        sValue == "true" ||
                        sValue == "y" ||
                        sValue == "yes" ||
                        sValue == "on";

                    *((bool*)pDesc->mpValue) = bTrue;    // set the registered bool
                    pDesc->mbFound = true;
                    if (bVerbose)
                        cout << "Set " << sKey << " = " << sValue << "\n";
                }
                break;
                case ParameterDescriptor::kInt64:
                {
                    int64_t nValue = IntFromUserReadable(sValue);
                    if (pDesc->mbRangeRestricted)
                    {
                        if (nValue < pDesc->mnMinValue || nValue > pDesc->mnMaxValue)
                        {
                            bError = true;
                            cerr << "Error: Value for parameter \"" << sParam << "\" is:" << nValue << ". Allowed range:(" << pDesc->mnMinValue << "-" << pDesc->mnMaxValue << ")\n";
                            continue;
                        }
                    }

                    *((int64_t*)pDesc->mpValue) = nValue;    // set the registered int
                    pDesc->mbFound = true;
                    if (bVerbose)
                        cout << "Set " << sKey << " = " << nValue << "\n";
                }
                break;
                case ParameterDescriptor::kString:
                {
                    *((string*)pDesc->mpValue) = sValue;     // set the registered string
                    pDesc->mbFound = true;
                    if (bVerbose)
                        cout << "Set " << sKey << " = " << sValue << "\n";
                }
                break;
                default:
                    cerr << "Error: Unknown parameter type registered \"" << sParam << "\"\n";
                    bError = true;
                    break;
                }

            }
            else
            {
                ////////////////////////////////////////////
                // Unnamed parameter processing
                ParameterDescriptor* pUnnamedDesc = nullptr;
                if (!GetUnnamedDescriptor(nUnnamedParametersFound, &pUnnamedDesc))
                {
                    cerr << "Error: Too many parameters! Max is:" << mUnnamedParameterDescriptors.size() << "parameter:" << sParam << "\n";
                    bError = true;
                    continue;
                }
                else
                {
                    switch (pUnnamedDesc->mType)
                    {
                    case ParameterDescriptor::kBool:
                    {
                        pUnnamedDesc->mbFound = true;

                        // if sUnnamedParameter is any of the following, it is considered TRUE
                        std::transform(sParam.begin(), sParam.end(), sParam.begin(), std::tolower);
                        bool bTrue = sParam == "1" ||
                            sParam == "t" ||
                            sParam == "true" ||
                            sParam == "y" ||
                            sParam == "yes" ||
                            sParam == "on";

                        *((bool*)pUnnamedDesc->mpValue) = bTrue;    // set the registered bool
                        if (bVerbose)
                            cout << "Set " << pUnnamedDesc->msName << " = " << sParam << "\n";
                    }
                    break;
                    case ParameterDescriptor::kInt64:
                    {
                        pUnnamedDesc->mbFound = true;
                        int64_t nValue = IntFromUserReadable(sParam);
                        if (pUnnamedDesc->mbRangeRestricted)
                        {
                            if (nValue < pUnnamedDesc->mnMinValue || nValue > pUnnamedDesc->mnMaxValue)
                            {
                                bError = true;
                                cerr << "Error: Value for parameter \"" << sParam << "\" is:" << nValue << ". Allowed values from min:" << pUnnamedDesc->mnMinValue << " to max:" << pUnnamedDesc->mnMaxValue << "\n";
                                continue;
                            }
                        }

                        *((int64_t*)pUnnamedDesc->mpValue) = nValue;    // set the registered int
                        if (bVerbose)
                            cout << "Set " << pUnnamedDesc->msName << " = " << nValue << "\n";
                    }
                    break;
                    case ParameterDescriptor::kString:
                    {
                        pUnnamedDesc->mbFound = true;
                        *((string*)pUnnamedDesc->mpValue) = sParam;     // set the registered string
                        if (bVerbose)
                            cout << "Set " << pUnnamedDesc->msName << " = " << sParam << "\n";
                    }
                    break;
                    default:
                        cerr << "Error: Unknown parameter type at index:" << nUnnamedParametersFound << "registered \"" << sParam << "\"\n";
                        bError = true;
                        break;
                    }
                }
                nUnnamedParametersFound++;
            }
        }
    }

    if (mnRequiredUnnamed > 0)
    {
        if (nUnnamedParametersFound < mnRequiredUnnamed)
        {
            cerr << "Error: Missing Parameters! Minimum required:" << mnRequiredUnnamed << "\n";
            bError = true;
        }
    }

    for (auto it = mNamedParameterDescriptors.begin(); it != mNamedParameterDescriptors.end(); it++)
    {
        ParameterDescriptor& desc = (*it).second;
        if (desc.mbRequired && !desc.mbFound)
        {
            cerr << "Error: Required parameter not found:" << desc.msName << "\n";
            bError = true;
        }
    }

errorprompt:

    if (bError)
    {
        string sUsageString = GetUsageString();

        int32_t nWidth = LongestDescriptionLine();
        if (sUsageString.length() > nWidth)
            nWidth = (int32_t) sUsageString.length();
        if (nWidth < 80)
            nWidth = 80;
        nWidth += 6;

        cout << "\n";
        RepeatOut('*', nWidth, true);

        if (!msDescription.empty())
        {
            OutputFixed(nWidth, "Description:");
            OutputLines(nWidth, msDescription);
            RepeatOut('*', nWidth, true);
        }
        OutputFixed(nWidth, "Usage:");
        OutputFixed(nWidth, sUsageString.c_str());
        RepeatOut('*', nWidth, true);
        return false;
    }

    return true;
}

bool CommandLineParser::GetNamedDescriptor(const string& sKey, ParameterDescriptor** pDescriptorOut)
{
//    cout << "retrieving named desciptor for:" << sKey << "size:" << mNamedParameterDescriptors.size() << "\n";
    tKeyToParamDescriptorMap::iterator it = mNamedParameterDescriptors.find(sKey);
    if (it == mNamedParameterDescriptors.end())
    {
        return false;
    }

    if (pDescriptorOut)
    {
        *pDescriptorOut = &(*it).second;       // copy it out
    }

    return true;
}

bool CommandLineParser::GetUnnamedDescriptor(int64_t nIndex, ParameterDescriptor** pDescriptorOut)
{
    if ((int64_t) mUnnamedParameterDescriptors.size() <= nIndex)
        return false;

    if (pDescriptorOut)
        *pDescriptorOut = &mUnnamedParameterDescriptors[nIndex];       // copy it out

    return true;
}


string CommandLineParser::GetUsageString()
{
    string sUsage = msAppPath;
    for (auto unnamed : mUnnamedParameterDescriptors)
    {
        sUsage += "  " + unnamed.msName;
        //            cerr << "  " << unnamed.msName;
    }

    for (auto it = mNamedParameterDescriptors.begin(); it != mNamedParameterDescriptors.end(); it++)
    {
        ParameterDescriptor& desc = (*it).second;

        // for unrequired params, surround with brackets
        sUsage += "  ";
        if (!desc.mbRequired)
            sUsage += "[";

        sUsage += "-";
        switch (desc.mType)
        {
        case ParameterDescriptor::kString:
            sUsage += desc.msName + ":XXX";
            break;
        case ParameterDescriptor::kInt64:
            sUsage += desc.msName + ":";
            if (desc.mbRangeRestricted)
                sUsage += "(" + Int64ToString(desc.mnMinValue) + "-" + Int64ToString(desc.mnMaxValue) + ")";
            else
                sUsage += "###";

            break;
        case ParameterDescriptor::kBool:
            sUsage += desc.msName + ":BOOL";
        }
        if (!desc.mbRequired)
            sUsage += "]";
    }
    return sUsage;
}


int32_t CommandLineParser::LongestDescriptionLine()
{
    int nLongest = 0;
    int nCount = 0;
    for (auto c : msDescription)
    {
        nCount++;
        if (c == '\r' || c == '\n')
            nCount = 0;
        else if (nCount > nLongest)
            nLongest = nCount;
    }
    return nLongest;
}

void CommandLineParser::RepeatOut(char c, int32_t nCount, bool bNewLine)
{
    while (nCount-->0)
        cout << c;

    if (bNewLine)
        cout << '\n';
}

void CommandLineParser::OutputFixed(int32_t nWidth, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    int32_t nRequiredLength = vsnprintf(nullptr, 0, format, args);
    char* pBuf = (char*)malloc(nRequiredLength + 1);
    vsnprintf(pBuf, nRequiredLength + 1, format, args);

    va_end(args);

    int32_t nPadding = nWidth-nRequiredLength-6;    // "** " and " **" on the ends is six chars

    cout << "** " << pBuf;
    RepeatOut(' ', nPadding);
    cout << " **\n";
    free(pBuf);
}

string CommandLineParser::Int64ToString(int64_t n)
{
    char buf[16];
    sprintf_s(buf, "%lld", n);
    return string(buf);
}


void CommandLineParser::OutputLines(int32_t nWidth, const string& sMultiLineString)
{
    std::string line;
    std::istringstream stringStream(sMultiLineString);
    while (std::getline(stringStream, line, '\n'))
    {
        OutputFixed(nWidth, line.c_str());
    }   

}

