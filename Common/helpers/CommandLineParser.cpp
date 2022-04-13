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

struct sSizeEntry
{
    const char* label;
    int64_t     value;
};

static const sSizeEntry sizeEntryTable[] =
{
    { "B"     , 1 },

    { "K"     , 1 * 1000LL},
    { "KB"    , 1 * 1000LL},
    { "KIB"   , 1 * 1024LL},

    { "M"     , 1 * 1000LL * 1000LL},
    { "MB"    , 1 * 1000LL * 1000LL},
    { "MIB"   , 1 * 1024LL * 1024LL},

    { "G"     , 1 * 1000LL * 1000LL * 1000LL},
    { "GB"    , 1 * 1000LL * 1000LL * 1000LL},
    { "GIB"   , 1 * 1024LL * 1024LL * 1024LL},

    { "T"     , 1 * 1000LL * 1000LL * 1000LL * 1000LL},
    { "TB"    , 1 * 1000LL * 1000LL * 1000LL * 1000LL},
    { "TIB"   , 1 * 1024LL * 1024LL * 1024LL * 1024LL},

    { "P"     , 1 * 1000LL * 1000LL * 1000LL * 1000LL * 1000LL},
    { "PB"    , 1 * 1000LL * 1000LL * 1000LL * 1000LL * 1000LL},
    { "PIB"   , 1 * 1024LL * 1024LL * 1024LL * 1024LL * 1024LL}
};

static const int sizeEntryTableSize = sizeof(sizeEntryTable) / sizeof(sSizeEntry);


string Int64ToString(int64_t n)
{
    char buf[16];
    sprintf_s(buf, "%lld", n);
    return string(buf);
}


ParamDesc::ParamDesc(eKeyType type, eRequired required, const string& sName, string* pString, const string& sUsage)
{
    mKeyType = type;
    mValueType = kString;
    msName = sName;
    mpValue = (void*)pString;
    mbRequired = (required == kRequired);
    msUsage = sUsage;
}

ParamDesc::ParamDesc(eKeyType type, eRequired required, const string& sName, bool* pBool, const string& sUsage)
{
    mKeyType = type;
    mValueType = kBool;
    msName = sName;
    mpValue = (void*)pBool;
    mbRequired = (required == kRequired);
    msUsage = sUsage;
}

ParamDesc::ParamDesc(eKeyType type, eRequired required, const string& sName, int64_t* pInt, bool bRangeRestricted, int64_t nMin, int64_t nMax, const string& sUsage)
{
    mKeyType = type;
    mValueType = kInt64;
    msName = sName;
    mpValue = (void*)pInt;
    mbRequired = (required == kRequired);
    msUsage = sUsage;
    mbRangeRestricted = bRangeRestricted;
    mnMinValue = nMin;
    mnMaxValue = nMax;
}


string ParamDesc::GetExampleString()
{
    // for unrequired params, surround with brackets
    string sCommandExample = " ";
    if (!mbRequired)
        sCommandExample += "[";

    if (mKeyType == kNamed)
    {
        sCommandExample += "-";
        switch (mValueType)
        {
        case ParamDesc::kString:
            sCommandExample += msName + ":STRING";
            break;
        case ParamDesc::kInt64:
            sCommandExample += msName + ":";
            if (mbRangeRestricted)
                sCommandExample += "(" + Int64ToString(mnMinValue) + "-" + Int64ToString(mnMaxValue) + ")";
            else
                sCommandExample += "NUMBER";

            break;
        case ParamDesc::kBool:
            sCommandExample += msName;
        }
    }
    else // kPositional
    {
        switch (mValueType)
        {
        case ParamDesc::kString:
            sCommandExample += msName + " (STRING)";
            break;
        case ParamDesc::kInt64:
            if (mbRangeRestricted)
                sCommandExample += msName + "(" + Int64ToString(mnMinValue) + "-" + Int64ToString(mnMaxValue) + ")";
            else
                sCommandExample += msName + " (NUMBER)";

            break;
        case ParamDesc::kBool:
            sCommandExample += msName + "(BOOL)";
        }
    }

    if (!mbRequired)
        sCommandExample += "]";

    return sCommandExample;
}


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

    for (int i = 0; i < sizeEntryTableSize; i++)
    {
        const sSizeEntry& entry = sizeEntryTable[i];

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
    mnRegisteredPositional = 0;
}

CommandLineParser::~CommandLineParser()
{
}

void CommandLineParser::RegisterAppDescription(const string& sDescription) 
{ 
    msAppDescription = sDescription;
}

bool CommandLineParser::RegisterParam(ParamDesc param)
{
    // Assign positional index based on how many have been registered
    if (param.mKeyType == ParamDesc::kPositional)
        param.mnPosition = mnRegisteredPositional++;

    mParameterDescriptors.emplace_back(param);

    return true;
}

/*
bool CommandLineParser::RegisterNamedString(const string& sKey, string* pRegisteredString, bool bRequired, const string& sUsage)
{
    if (GetNamedDescriptor(sKey))
    {
        cerr << "sKey:" << sKey << " is already registered!";
        return false;
    }

    ParameterDescriptor desc;
    desc.mKeyType = ParameterDescriptor::eParamType::kNamed;
    desc.msName = sKey;
    desc.mpValue = (void*)pRegisteredString;
    desc.mbRequired = bRequired;
    desc.mValueType = ParameterDescriptor::kString;
    desc.msUsage = sUsage;
    mParameterDescriptors.emplace_back(desc);

    return true;
}

bool CommandLineParser::RegisterNamedInt64(const string& sKey, int64_t* pRegisteredInt64, bool bRequired, bool bRangeRestricted, int64_t nMinValue, int64_t nMaxValue, const string& sUsage)
{
    if (GetNamedDescriptor(sKey))
    {
        cerr << "sKey:" << sKey << " is already registered!";
        return false;
    }

    ParameterDescriptor desc;
    desc.mKeyType = ParameterDescriptor::eParamType::kNamed;
    desc.msName = sKey;
    desc.mpValue = (void*)pRegisteredInt64;
    desc.mbRequired = bRequired;
    desc.mValueType = ParameterDescriptor::kInt64;
    desc.mbRangeRestricted = bRangeRestricted;
    desc.mnMinValue = nMinValue;
    desc.mnMaxValue = nMaxValue;
    desc.msUsage = sUsage;
    mParameterDescriptors.emplace_back(desc);

    return true;
}

bool CommandLineParser::RegisterNamedBool(const string& sKey, bool* pRegisteredBool, bool bRequired, const string& sUsage)
{
    if (GetNamedDescriptor(sKey))
    {
        cerr << "sKey:" << sKey << " is already registered!";
        return false;
    }

    ParameterDescriptor desc;
    desc.mKeyType = ParameterDescriptor::eParamType::kNamed;
    desc.msName = sKey;
    desc.mpValue = (void*)pRegisteredBool;
    desc.mbRequired = bRequired;
    desc.mValueType = ParameterDescriptor::kBool;
    desc.msUsage = sUsage;
    mParameterDescriptors.emplace_back(desc);

    return true;
}

bool CommandLineParser::RegisterPositionalString(const string& sDisplayName, string* pRegisteredString, bool bRequired, const string& sUsage)
{
    ParameterDescriptor desc;
    desc.msName = sDisplayName;
    desc.mKeyType = ParameterDescriptor::eParamType::kPositional;
    desc.mnPosition = mnRegisteredPositional;
    desc.mbRequired = bRequired;
    desc.mpValue = (void*)pRegisteredString;
    desc.mValueType = ParameterDescriptor::kString;
    desc.msUsage = sUsage;
    mParameterDescriptors.emplace_back(desc);

    mnRegisteredPositional++;

    return true;
}

bool CommandLineParser::RegisterPositionalInt64(const string& sDisplayName, int64_t* pRegisteredInt64, bool bRequired, bool bRangeRestricted, int64_t nMinValue, int64_t nMaxValue, const string& sUsage)
{
    ParameterDescriptor desc;
    desc.msName = sDisplayName;
    desc.mKeyType = ParameterDescriptor::eParamType::kPositional;
    desc.mnPosition = mnRegisteredPositional;
    desc.mbRequired = bRequired;
    desc.mpValue = (void*)pRegisteredInt64;
    desc.mValueType = ParameterDescriptor::kInt64;
    desc.mbRangeRestricted = bRangeRestricted;
    desc.mnMinValue = nMinValue;
    desc.mnMaxValue = nMaxValue;
    desc.msUsage = sUsage;
    mParameterDescriptors.emplace_back(desc);

    mnRegisteredPositional++;

    return true;
}

bool CommandLineParser::RegisterPositionalBool(const string& sDisplayName, bool* pRegisteredBool, bool bRequired, const string& sUsage)
{
    ParameterDescriptor desc;
    desc.msName = sDisplayName;
    desc.mKeyType = ParameterDescriptor::eParamType::kPositional;
    desc.mnPosition = mnRegisteredPositional;
    desc.mbRequired = bRequired;
    desc.mpValue = (void*)pRegisteredBool;
    desc.mValueType = ParameterDescriptor::kBool;
    desc.msUsage = sUsage;
    mParameterDescriptors.emplace_back(desc);

    mnRegisteredPositional++;

    return true;
}
*/



bool CommandLineParser::Parse(int argc, char* argv[], bool bVerbose)
{
    msAppPath = argv[0];

    // Ensure the msAppPath extension includes ".exe" since it can be launched without
    int32_t nLastDot = (int32_t)msAppPath.find_last_of('.');
    string sExtension(msAppPath.substr(nLastDot));
    std::transform(sExtension.begin(), sExtension.end(), sExtension.begin(), std::tolower);
    if (sExtension != ".exe")
        msAppPath += ".exe";

    // Extract  application name
    int32_t nLastSlash = (int32_t)msAppPath.find_last_of('/');
    int32_t nLastBackSlash = (int32_t)msAppPath.find_last_of('\\');
    nLastSlash = (nLastSlash > nLastBackSlash) ? nLastSlash : nLastBackSlash;

    if (nLastSlash > 0)
        msAppName = msAppPath.substr(nLastSlash + 1);
    else
        msAppName = msAppPath;



    int64_t nPositionalParametersFound = 0;

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

                ParamDesc* pDesc = nullptr;
                if (!GetNamedDescriptor(sKey, &pDesc))
                {
                    cerr << "Error: Unknown parameter \"" << sParam << "\"\n";
                    bError = true;
                    continue;
                }

                switch (pDesc->mValueType)
                {
                case ParamDesc::kBool:
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
                case ParamDesc::kInt64:
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
                case ParamDesc::kString:
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
                // Positional parameter processing
                ParamDesc* pPositionalDesc = nullptr;
                if (!GetPositionalDescriptor(nPositionalParametersFound, &pPositionalDesc))
                {
                    cerr << "Error: Too many parameters! Max is:" << mnRegisteredPositional << "parameter:" << sParam << "\n";
                    bError = true;
                    continue;
                }
                else
                {
                    switch (pPositionalDesc->mValueType)
                    {
                    case ParamDesc::kBool:
                    {
                        pPositionalDesc->mbFound = true;

                        // if sPositionalParameter is any of the following, it is considered TRUE
                        std::transform(sParam.begin(), sParam.end(), sParam.begin(), std::tolower);
                        bool bTrue = sParam == "1" ||
                            sParam == "t" ||
                            sParam == "true" ||
                            sParam == "y" ||
                            sParam == "yes" ||
                            sParam == "on";

                        *((bool*)pPositionalDesc->mpValue) = bTrue;    // set the registered bool
                        if (bVerbose)
                            cout << "Set " << pPositionalDesc->msName << " = " << sParam << "\n";
                    }
                    break;
                    case ParamDesc::kInt64:
                    {
                        pPositionalDesc->mbFound = true;
                        int64_t nValue = IntFromUserReadable(sParam);
                        if (pPositionalDesc->mbRangeRestricted)
                        {
                            if (nValue < pPositionalDesc->mnMinValue || nValue > pPositionalDesc->mnMaxValue)
                            {
                                bError = true;
                                cerr << "Error: Value for parameter \"" << sParam << "\" is:" << nValue << ". Allowed values from min:" << pPositionalDesc->mnMinValue << " to max:" << pPositionalDesc->mnMaxValue << "\n";
                                continue;
                            }
                        }

                        *((int64_t*)pPositionalDesc->mpValue) = nValue;    // set the registered int
                        if (bVerbose)
                            cout << "Set " << pPositionalDesc->msName << " = " << nValue << "\n";
                    }
                    break;
                    case ParamDesc::kString:
                    {
                        pPositionalDesc->mbFound = true;
                        *((string*)pPositionalDesc->mpValue) = sParam;     // set the registered string
                        if (bVerbose)
                            cout << "Set " << pPositionalDesc->msName << " = " << sParam << "\n";
                    }
                    break;
                    default:
                        cerr << "Error: Unknown parameter type at index:" << nPositionalParametersFound << "registered \"" << sParam << "\"\n";
                        bError = true;
                        break;
                    }
                }
                nPositionalParametersFound++;
            }
        }
    }

    for (auto desc : mParameterDescriptors)
    {
        if (desc.mbRequired && !desc.mbFound)
        {
            cerr << "Error: Required parameter not found:" << desc.msName << "\n";
            bError = true;
        }
    }

errorprompt:

    if (bError)
    {
        OutputUsage();
        return false;
    }

    return true;
}

bool CommandLineParser::GetNamedDescriptor(const string& sKey, ParamDesc** pDescriptorOut)
{
//    cout << "retrieving named desciptor for:" << sKey << "size:" << mNamedParameterDescriptors.size() << "\n";
    for (auto& desc : mParameterDescriptors)
    {
        if (desc.mKeyType == ParamDesc::kNamed && desc.msName == sKey)
        {
            if (pDescriptorOut)
            {
                *pDescriptorOut = &desc;       // copy it out
            }

            return true;
        }
    }

    return false;
}

bool CommandLineParser::GetPositionalDescriptor(int64_t nIndex, ParamDesc** pDescriptorOut)
{
    for (auto& desc : mParameterDescriptors)
    {
        if (desc.mKeyType == ParamDesc::kPositional && desc.mnPosition == nIndex)
        {
            if (pDescriptorOut)
            {
                *pDescriptorOut = &desc;       // copy it out
            }

            return true;
        }
    }

    return false;

    return true;
}


void CommandLineParser::OutputUsage()
{
    string sCommandLineExample = msAppName;

    // create example command line with positional params first followed by named
    for (auto& desc : mParameterDescriptors)
    {
        if (desc.mKeyType == ParamDesc::kPositional)
            sCommandLineExample += " " + desc.GetExampleString();
    }
    for (auto& desc : mParameterDescriptors)
    {
        if (desc.mKeyType == ParamDesc::kNamed)
            sCommandLineExample += " " + desc.GetExampleString();
    }


    // Create table of params and usages
    
    // parameters to be output in even columns
    // example:
    // PATH         - The file to process.
    // PROCESS_SIZE - Number of bytes to process. (Range: 1-1TiB)

    vector<string> leftColumn;
    vector<string> rightColumn;


    for (auto& desc : mParameterDescriptors)
    {
        leftColumn.push_back(desc.GetExampleString());
        rightColumn.push_back(desc.msUsage);
    }

    // Compute column widths
    int32_t nLeftColumnWidth = 0;
    for (auto entry : leftColumn)
    {
        if (entry.length() > nLeftColumnWidth)
            nLeftColumnWidth = (int32_t) entry.length();
    }

    int32_t nRightColumnWidth = 0;
    for (auto entry : rightColumn)
    {
        int32_t nWidth = LongestLine(entry);
        if (nWidth > nRightColumnWidth)
            nRightColumnWidth = nWidth;
    }

    // Compute longest line from app description
    int32_t nWidth = LongestLine(msAppDescription);
    if (sCommandLineExample.length() > nWidth)
        nWidth = (int32_t)sCommandLineExample.length();

    if (nWidth < nLeftColumnWidth + nRightColumnWidth)
        nWidth = nLeftColumnWidth + nRightColumnWidth;

    if (nWidth < 80)
        nWidth = 80;
    nWidth += 6;

    cout << "\n";
    RepeatOut('*', nWidth, true);
    RepeatOut('*', nWidth, true);

    if (!msAppDescription.empty())
    {
        OutputFixed(nWidth, "Description:");
        OutputLines(nWidth, msAppDescription);
        RepeatOut('*', nWidth, true);
    }
    OutputFixed(nWidth, "Usage:     ([] == optional parameters)");
    OutputFixed(nWidth, sCommandLineExample.c_str());
    OutputFixed(nWidth, " ");

    for (int32_t i = 0; i < leftColumn.size(); i++)
    {
        string sLeft = leftColumn[i];
        AddSpacesToEnsureWidth(sLeft, nLeftColumnWidth);

        string sRight = rightColumn[i];

        string sLine = sLeft + " - " + sRight;
        OutputFixed(nWidth, sLine.c_str());
    }

    RepeatOut('*', nWidth, true);
    RepeatOut('*', nWidth, true);
}


int32_t CommandLineParser::LongestLine(const string& sText)
{
    int nLongest = 0;
    int nCount = 0;
    for (auto c : sText)
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

void CommandLineParser::AddSpacesToEnsureWidth(string& sText, int64_t nWidth)
{
    int64_t nSpaces = nWidth - sText.length();
    while (nSpaces-- > 0)
        sText += ' ';
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

