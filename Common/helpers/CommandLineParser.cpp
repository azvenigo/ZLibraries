#include "CommandLineParser.h"
#include <algorithm>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <cctype>
#include <assert.h>
#include <stdarg.h> 
#include <sstream>
#include <algorithm>

using namespace std;

namespace CLP
{

    struct sSizeEntry
    {
        const char* label;
        int64_t     value;
    };

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

    int32_t LongestLine(const string& sText)
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

    void RepeatOut(char c, int32_t nCount, bool bNewLine)
    {
        while (nCount-- > 0)
            cout << c;

        if (bNewLine)
            cout << '\n';
    }

    void OutputFixed(int32_t nWidth, const char* format, ...)
    {
        va_list args;
        va_start(args, format);

        int32_t nRequiredLength = vsnprintf(nullptr, 0, format, args);
        char* pBuf = (char*)malloc(nRequiredLength + 1);
        vsnprintf(pBuf, nRequiredLength + 1, format, args);

        va_end(args);

        int32_t nPadding = nWidth - nRequiredLength - 6;    // "** " and " **" on the ends is six chars

        cout << "** " << pBuf;
        RepeatOut(' ', nPadding, false);
        cout << " **\n";
        free(pBuf);
    }

    void AddSpacesToEnsureWidth(string& sText, int64_t nWidth)
    {
        int64_t nSpaces = nWidth - sText.length();
        while (nSpaces-- > 0)
            sText += ' ';
    }

    void OutputLines(int32_t nWidth, const string& sMultiLineString)
    {
        std::string line;
        std::istringstream stringStream(sMultiLineString);
        while (std::getline(stringStream, line, '\n'))
        {
            OutputFixed(nWidth, line.c_str());
        }

    }




    // many ways to say "true"
    bool StringToBool(string sValue)
    {
        std::transform(sValue.begin(), sValue.end(), sValue.begin(), [](unsigned char c){ return (unsigned char) std::tolower(c); });
        return sValue == "1" ||
            sValue == "t" ||
            sValue == "true" ||
            sValue == "y" ||
            sValue == "yes" ||
            sValue == "on";
    }

    // Converts user readable numbers into ints
    // Supports hex (0x12345)
    // Strips commas (1,000,000)
    // Supports trailing scaling labels  (k, kb, kib, m, mb, mib, etc.)
    int64_t IntFromUserReadable(string sReadable)
    {
        std::transform(sReadable.begin(), sReadable.end(), sReadable.begin(), [](unsigned char c){ return (unsigned char) std::toupper(c); });

        // strip any commas in case human readable string has those
        sReadable.erase(std::remove(sReadable.begin(), sReadable.end(), ','), sReadable.end());


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
    string UserReadableFromInt(int64_t nValue)
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


    ParamDesc::ParamDesc(const string& sName, string* pString, eBehavior behavior, const string& sUsage)
    {
        mValueType = kString;
        msName = sName;
        mpValue = (void*)pString;
        mBehaviorFlags = behavior;
        mbFound = false;
        msUsage = sUsage;
    }


    ParamDesc::ParamDesc(const string& sName, bool* pBool, eBehavior behavior, const string& sUsage)
    {
        mValueType = kBool;
        msName = sName;
        mpValue = (void*)pBool;
        mBehaviorFlags = behavior;
        mbFound = false;
        msUsage = sUsage;
    }

    ParamDesc::ParamDesc(const string& sName, int64_t* pInt, eBehavior behavior, const string& sUsage, int64_t nRangeMin, int64_t nRangeMax)
    {
        mValueType = kInt64;
        msName = sName;
        mpValue = (void*)pInt;
        mBehaviorFlags = behavior;
        mnMinValue = nRangeMin;
        mnMaxValue = nRangeMax;
        mbFound = false;
        msUsage = sUsage;
    }


    void ParamDesc::GetExample(std::string& sParameter, std::string& sType, std::string& sUsage)
    {
        // for unrequired params, surround with brackets
        if (IsOptional())
            sParameter = "[";

        if (IsNamed())
        {
            sParameter += "-";
            sParameter += msName;
            sParameter += ":";
            switch (mValueType)
            {
            case ParamDesc::kString:
                sType = "$$";
                break;
            case ParamDesc::kInt64:
            {
                if (IsRangeRestricted())
                    sType = "(" + UserReadableFromInt(mnMinValue) + "-" + UserReadableFromInt(mnMaxValue) + ")";
                else
                    sType = "##";
            }
            break;
            case ParamDesc::kBool:
                sType = "BOOL";
            default:
                break;
            }
        }
        else // kPositional
        {
            sParameter += msName;
            switch (mValueType)
            {
            case ParamDesc::kString:
                sType = "$$";
                break;
            case ParamDesc::kInt64:
            {
                if (IsRangeRestricted())
                    sType = "(" + UserReadableFromInt(mnMinValue) + "-" + UserReadableFromInt(mnMaxValue) + ")";
                else
                    sType = "##";
            }
            break;
            case ParamDesc::kBool:
                sType = "BOOL";
            default:
                break;
            }
        }

        if (IsOptional())
            sParameter += "]";

        sUsage = msUsage;
    }

    CLModeParser::CLModeParser()
    {
    }

    CLModeParser::~CLModeParser()
    {
    }

    bool CLModeParser::RegisterParam(ParamDesc param)
    {
        // Assign positional index based on how many have been registered
        if (param.IsPositional())
            param.mnPosition = GetNumPositionalParamsRegistered();

        mParameterDescriptors.emplace_back(param);

        return true;
    }

    size_t CLModeParser::GetNumPositionalParamsRegistered()
    {
        size_t nCount = 0;
        for (auto p : mParameterDescriptors)
        {
            if (p.IsPositional())
                nCount++;
        }

        return nCount;
    }

    size_t CLModeParser::GetNumPositionalParamsHandled()
    {
        size_t nCount = 0;
        for (auto p : mParameterDescriptors)
        {
            if (p.IsPositional() && p.mbFound)
                nCount++;
        }

        return nCount;
    }

    bool CLModeParser::CheckAllRequirementsMet()
    {
        int nErrors = 0;
        for (auto p : mParameterDescriptors)
        {
            if (p.IsRequired() && !p.mbFound)
            {
                cerr << "Error: Required parameter not found:" << p.msName << "\n";
                nErrors++;
            }
        }

        return nErrors == 0;
    }



    bool CLModeParser::CanHandleArgument(const std::string& sArg)
    {
        // named argument
        if (sArg[0] == '-')
        {
            string sKey;

            size_t nIndexOfColon = sArg.find(':');
            if (nIndexOfColon != string::npos)
            {
                sKey = sArg.substr(1, nIndexOfColon - 1).c_str();    // everything from first char to colon
            }
            else
            {
                // flag with no value is the same as -flag:true
                sKey = sArg.substr(1, nIndexOfColon).c_str();
            }

            return GetDescriptor(sKey, nullptr);  // if descriptor is found this parameter is registered
        }
        else
        {
            // positional arguments can be handled is fewer have been handled than total registered
            return GetNumPositionalParamsHandled() < GetNumPositionalParamsRegistered();
        }
    }

    bool CLModeParser::HandleArgument(const std::string& sArg, bool bVerbose)
    {
        // named argument
        if (sArg[0] == '-')
        {
            ////////////////////////////////////////////
            // Named parameter processing
            string sKey;
            string sValue;

            size_t nIndexOfColon = sArg.find(':');
            if (nIndexOfColon != string::npos)
            {
                sKey = sArg.substr(1, nIndexOfColon - 1).c_str();    // everything from first char to colon
                sValue = sArg.substr(nIndexOfColon + 1).c_str(); // everything after the colon
            }
            else
            {
                // flag with no value is the same as -flag:true
                sKey = sArg.substr(1, nIndexOfColon).c_str();
                sValue = "true";
            }

            ParamDesc* pDesc = nullptr;
            if (!GetDescriptor(sKey, &pDesc))
            {
                cerr << "Error: Unknown parameter '" << sKey << "'\n";
                return false;
            }

            switch (pDesc->mValueType)
            {
                case ParamDesc::kBool:
                {
                    *((bool*)pDesc->mpValue) = StringToBool(sValue);    // set the registered bool
                    pDesc->mbFound = true;
                    if (bVerbose)
                        cout << "Set " << sKey << " = " << sValue << "\n";
                    return true;
                }
                break;
                case ParamDesc::kInt64:
                {
                    int64_t nValue = IntFromUserReadable(sValue);
                    if (pDesc->IsRangeRestricted())
                    {
                        if (nValue < pDesc->mnMinValue || nValue > pDesc->mnMaxValue)
                        {
                            cerr << "Error: Value for parameter \"" << sArg << "\" is:" << nValue << ". Allowed range:(" << pDesc->mnMinValue << "-" << pDesc->mnMaxValue << ")\n";
                            return false;
                        }
                    }

                    *((int64_t*)pDesc->mpValue) = nValue;    // set the registered int
                    pDesc->mbFound = true;
                    if (bVerbose)
                        cout << "Set " << sKey << " = " << nValue << "\n";
                    return true;
                }
                break;
                default:    // ParamDesc::kString
                {
                    *((string*)pDesc->mpValue) = sValue;     // set the registered string
                    pDesc->mbFound = true;
                    if (bVerbose)
                        cout << "Set " << sKey << " = " << sValue << "\n";
                    return true;
                }
            }
        }
        else
        {
            ////////////////////////////////////////////
            // Positional parameter processing
            ParamDesc* pPositionalDesc = nullptr;
            if (!GetDescriptor(GetNumPositionalParamsHandled(), &pPositionalDesc))  // num handled is also the index of the next one
            {
                cerr << "Error: Too many parameters! Max is:" << GetNumPositionalParamsRegistered() << " parameter:" << sArg << "\n";
                return false;
            }
            else
            {
                switch (pPositionalDesc->mValueType)
                {
                case ParamDesc::kBool:
                {
                    *((bool*)pPositionalDesc->mpValue) = StringToBool(sArg);    // set the registered bool
                    pPositionalDesc->mbFound = true;
                    if (bVerbose)
                        cout << "Set " << pPositionalDesc->msName << " = " << sArg << "\n";
                    return true;
                }
                break;
                case ParamDesc::kInt64:
                {
                    pPositionalDesc->mbFound = true;
                    int64_t nValue = IntFromUserReadable(sArg);
                    if (pPositionalDesc->IsRangeRestricted())
                    {
                        if (nValue < pPositionalDesc->mnMinValue || nValue > pPositionalDesc->mnMaxValue)
                        {
                            cerr << "Error: Value for parameter \"" << sArg << "\" is:" << nValue << ". Allowed values from min:" << UserReadableFromInt(pPositionalDesc->mnMinValue) << " to max:" << UserReadableFromInt(pPositionalDesc->mnMaxValue) << "\n";
                            return false;
                        }
                    }

                    *((int64_t*)pPositionalDesc->mpValue) = nValue;    // set the registered int
                    if (bVerbose)
                        cout << "Set " << pPositionalDesc->msName << " = " << nValue << "\n";
                    return true;
                }
                break;
                default:    // ParamDesc::kString
                {
                    pPositionalDesc->mbFound = true;
                    *((string*)pPositionalDesc->mpValue) = sArg;     // set the registered string
                    if (bVerbose)
                        cout << "Set " << pPositionalDesc->msName << " = " << sArg << "\n";
                    return true;
                }
                }
            }
        }

        return false;
    }

    bool CLModeParser::GetDescriptor(const string& sKey, ParamDesc** pDescriptorOut)
    {
        for (auto& desc : mParameterDescriptors)
        {
            if (desc.IsNamed() &&  desc.msName.compare(sKey.c_str()) == 0)
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

    bool CLModeParser::GetDescriptor(int64_t nIndex, ParamDesc** pDescriptorOut)
    {
        for (auto& desc : mParameterDescriptors)
        {
            if (desc.IsPositional() && desc.mnPosition == nIndex)
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

    bool CLModeParser::GetParamWasFound(const string& sKey)
    {
        ParamDesc* pDesc = nullptr;
        if (GetDescriptor(sKey, &pDesc))
        {
            return pDesc->mbFound;
        }

        return false;
    }


    size_t  CLModeParser::GetOptionalParameterCount()
    {
        size_t nCount = 0;
        for (auto& desc : mParameterDescriptors)
        {
            if (desc.IsOptional())
                nCount++;
        }

        return nCount;
    }

    size_t  CLModeParser::GetRequiredParameterCount()
    {
        size_t nCount = 0;
        for (auto& desc : mParameterDescriptors)
        {
            if (!desc.IsOptional())
                nCount++;
        }

        return nCount;
    }


    void CLModeParser::GetModeUsageOutput(const string& sAppName, const string& sMode, TableOutput& usageTable, TableOutput& modeDescriptionTable, TableOutput& requiredParamTable, TableOutput& optionalParamTable)
    {
        string sCommandLineExample = sAppName + " " + sMode;

        // create example command line with positional params first followed by named
        for (auto& desc : mParameterDescriptors)
        {
            if (desc.IsPositional() && !desc.IsOptional())
                sCommandLineExample += " " + desc.msName;
        }
        for (auto& desc : mParameterDescriptors)
        {
            if (desc.IsNamed() && !desc.IsOptional())
                sCommandLineExample += " " + desc.msName;
        }

        if (GetOptionalParameterCount() > 0)
            sCommandLineExample += " [options]";


        // Create table of params and usages

        // parameters to be output in even columns
        // example:
        // PATH         - The file to process.
        // PROCESS_SIZE - Number of bytes to process. (Range: 1-1TiB)

        for (auto& desc : mParameterDescriptors)
        {
            string sName;
            string sType;
            string sUsage;

            desc.GetExample(sName, sType, sUsage);
            if (desc.IsOptional())
                optionalParamTable.AddRow(sName, sType, sUsage);
            else
                requiredParamTable.AddRow(sName, sType, sUsage);
        }


        if (!msModeDescription.empty())
        {
            if (!sMode.empty())
                modeDescriptionTable.AddRow(string("Command: " + sMode));
            else
                modeDescriptionTable.AddRow("Temporary Command: sMode string.");
        }

        usageTable.AddRow(sCommandLineExample);
    }


    void CommandLineParser::RegisterAppDescription(const string& sDescription)
    {
        msAppDescription = sDescription;
    }


    bool CommandLineParser::IsRegisteredMode(const string& sArg)
    {
        for (tModeStringToParserMap::iterator it = mModeToCommandLineParser.begin(); it != mModeToCommandLineParser.end(); it++)
        {
            if ((*it).first == sArg)
                return true;
        }

        return false;
    }

    bool CommandLineParser::GetParamWasFound(const string& sKey)
    {
        if (msMode.empty())
            return mGeneralCommandLineParser.GetParamWasFound(sKey);

        return mModeToCommandLineParser[msMode].GetParamWasFound(sKey);
    }

    size_t CommandLineParser::GetOptionalParameterCount()
    {
        size_t nCount = 0;
        if (!msMode.empty())
            nCount = mModeToCommandLineParser[msMode].GetOptionalParameterCount(); 

        nCount += mGeneralCommandLineParser.GetOptionalParameterCount();

        return nCount;
    }

    size_t CommandLineParser::GetRequiredParameterCount()
    {
        if (msMode.empty())
            return mGeneralCommandLineParser.GetRequiredParameterCount();

        return mModeToCommandLineParser[msMode].GetRequiredParameterCount();
    }


    bool CommandLineParser::RegisterMode(const string& sMode, const string& sModeDescription)
    {
        if (mModeToCommandLineParser.find(sMode) != mModeToCommandLineParser.end())
        {
            cerr << "Mode already registered:" << sMode << "\n";
            return false;
        }

        return mModeToCommandLineParser[sMode].RegisterModeDescription(sModeDescription);
    }

    bool CommandLineParser::RegisterParam(const string& sMode, ParamDesc param)
    {
        if (mModeToCommandLineParser.find(sMode) == mModeToCommandLineParser.end())
        {
            cerr << "Unregistered mode:" << sMode << "\n";
            return false;
        }
        return mModeToCommandLineParser[sMode].RegisterParam(param);
    }

    bool CommandLineParser::RegisterParam(ParamDesc param)
    {
        return mGeneralCommandLineParser.RegisterParam(param);
    }

    bool CommandLineParser::Parse(int argc, char* argv[], bool bVerbose)
    {
        msMode.clear();
        msAppPath = argv[0];
        // Ensure the msAppPath extension includes ".exe" since it can be launched without
        if (msAppPath.length() > 4)
        {
            // Ensure the msAppPath extension includes ".exe" since it can be launched without
            string sExtension(msAppPath.substr(msAppPath.length() - 4));
            std::transform(sExtension.begin(), sExtension.end(), sExtension.begin(), [](unsigned char c){ return (unsigned char) std::tolower(c); });
            if (sExtension != ".exe")
                msAppPath += ".exe";
        }

        // Extract  application name
        int32_t nLastSlash = (int32_t)msAppPath.find_last_of('/');
        int32_t nLastBackSlash = (int32_t)msAppPath.find_last_of('\\');
        nLastSlash = (nLastSlash > nLastBackSlash) ? nLastSlash : nLastBackSlash;

        if (nLastSlash > 0)
        {
            msAppName = msAppPath.substr(nLastSlash + 1);
            msAppPath = msAppPath.substr(0, nLastBackSlash);
        }
        else
            msAppName = msAppPath;



        int nErrors = 0;

        if (argc > 1)
        {
            // If "help" requested
            string sFirst(argv[1]);
            if (sFirst == "?" || sFirst == "help" || sFirst == "-h")
            {
                // if specific help for a mode requested
                if (argc > 2)
                {
                    msMode.assign(argv[2]);
                    OutputHelp();
                }
                else
                {
                    OutputUsage();
                }
                return false;
            }

            if (IsRegisteredMode(sFirst))
            {
                msMode = sFirst;
                for (int i = 2; i < argc; i++)
                {
                    std::string sParam(argv[i]);

                    if (mModeToCommandLineParser[msMode].CanHandleArgument(sParam)) // if regestered for specific mode handle it
                    {
                        if (!mModeToCommandLineParser[msMode].HandleArgument(sParam, bVerbose))
                            nErrors++;
                    }
                    else if (mGeneralCommandLineParser.CanHandleArgument(sParam))   // if general parameter handle it
                    {
                        if (!mGeneralCommandLineParser.HandleArgument(sParam, bVerbose))
                            nErrors++;
                    }
                    else
                    {
                        cerr << "Error: Unknown parameter '" << sParam << "'\n";
                        nErrors++;
                    }
                }

                if (!mModeToCommandLineParser[msMode].CheckAllRequirementsMet())
                {
                    cout << "\n\"" << msAppName << " help\" - to see usage.\n";
                    return false;
                }

            }
            else
            {
                // modeless parsing
                for (int i = 1; i < argc; i++)
                {
                    std::string sParam(argv[i]);

                    if (mGeneralCommandLineParser.CanHandleArgument(sParam))
                    {
                        if (!mGeneralCommandLineParser.HandleArgument(sParam, bVerbose))
                            nErrors++;
                    }
                    else
                    {
                        cerr << "Error: Unknown parameter '" << sParam << "'\n";
                        nErrors++;
                    }
                }

                if (!mGeneralCommandLineParser.CheckAllRequirementsMet())
                {
                    cout << "\n\"" << msAppName << " help\" - to see usage.\n";
                    return false;
                }

            }
        }

        if (nErrors > 0)
        {
            OutputUsage();
            return false;
        }

        // no parameters


        // no parameters but multi-modes are available... show usage
        if (!mModeToCommandLineParser.empty())
        {
            OutputUsage();
            return false;
        }

        // no parameters passed, see if general parser has required params 
        if (!mGeneralCommandLineParser.CheckAllRequirementsMet())
        {
            cout << "\n\"" << msAppName << " help\" - to see usage.\n";
            return false;
        }

        // no parameters and none required
        return true;
    }

    void CommandLineParser::OutputHelp()
    {
        if (mModeToCommandLineParser.find(msMode) == mModeToCommandLineParser.end())
        {
            cerr << "help: Unknown command:" << msMode << "\n";
            return;
        }

        TableOutput usageTable;
        TableOutput descriptionTable;
        TableOutput requiredParamTable;
        TableOutput optionalParamTable;

        requiredParamTable.AddRow("*Required Param*", "*Type*", "*Description*");
        optionalParamTable.AddRow("*Optional Param*", "*Type*", "*Description*");
        requiredParamTable.SetBorders(0, 0, '*', '*');
        requiredParamTable.SetSeparator(' ', 1);
        optionalParamTable.SetBorders(0, 0, '*', '*');
        optionalParamTable.SetSeparator(' ', 1);

        descriptionTable.SetBorders('*', 0, '*', '*');
        descriptionTable.SetSeparator(' ', 1);
        descriptionTable.AddRow("*Description*");

        usageTable.AddRow("*Usage*");
        usageTable.SetSeparator(' ', 1);
        usageTable.SetBorders('*', '*', '*', '*');

        // if modal
        if (!msMode.empty())
        {
            TableOutput dummyUsageTable;    // only going to use usageTable from mode
            mModeToCommandLineParser[msMode].GetModeUsageOutput(msAppName, msMode, usageTable, descriptionTable, requiredParamTable, optionalParamTable);
            mGeneralCommandLineParser.GetModeUsageOutput(msAppName, "", dummyUsageTable, descriptionTable, requiredParamTable, optionalParamTable);
        }
        else
        {
            mGeneralCommandLineParser.GetModeUsageOutput(msAppName, "", usageTable, descriptionTable, requiredParamTable, optionalParamTable);
        }


        size_t nMinTableWidth = std::max({ (size_t) 120, descriptionTable.GetTableWidth(), requiredParamTable.GetTableWidth(), optionalParamTable.GetTableWidth(), usageTable.GetTableWidth() });

        usageTable.SetMinimumOutputWidth(nMinTableWidth);
        requiredParamTable.SetMinimumOutputWidth(nMinTableWidth);
        optionalParamTable.SetMinimumOutputWidth(nMinTableWidth);
        descriptionTable.SetMinimumOutputWidth(nMinTableWidth);

        optionalParamTable.SetBorders(0, 0, '*', '*');
        optionalParamTable.SetSeparator(' ', 1);
        if (GetOptionalParameterCount() > 0)
        {
            optionalParamTable.AddRow("*Optional Param*", "*Type*", "*Description*");
            optionalParamTable.SetBorders(0, '*', '*', '*');        // if there are optional parameters, draw a bottom border after optional table
        }
        else
            requiredParamTable.SetBorders(0, '*', '*', '*');        // if no optional parameters, draw a bottom border after required table


        // Now default/global
        cout << descriptionTable;
        cout << usageTable;
        if (requiredParamTable.GetRowCount() > 1)   // 1 because "Required:" added above
            cout << requiredParamTable;
        if (optionalParamTable.GetRowCount() > 1)   // 1 because "Optional:" added above
            cout << optionalParamTable;
    }

    void CommandLineParser::OutputUsage()
    {
        // First output Application name

        TableOutput keyTable;
        keyTable.SetBorders('*', '*', '*', '*');
        keyTable.SetSeparator(' ', 1);
        keyTable.AddRow("Keys:");
        keyTable.AddRow(" []","Optional");
        keyTable.AddRow("  -", "Named '-key:value' pair. (examples: -size:1KB  -verbose)");
        keyTable.AddRow(" ", "Can be anywhere on command line, in any order.");
        keyTable.AddRow(" ##", "NUMBER");
        keyTable.AddRow(" ", "Can be hex (0x05) or decimal");
        keyTable.AddRow(" ", "Can include commas (1,000)");
        keyTable.AddRow(" ", "Can include scale labels (10k, 64KiB, etc.)");
        keyTable.AddRow(" $$", "STRING");

        TableOutput descriptionTable;
        descriptionTable.SetBorders('*', '*', '*', '*');
        descriptionTable.SetSeparator(' ', 1);
        descriptionTable.AddRow("*Application*");
        descriptionTable.AddMultilineRow(msAppDescription);
        descriptionTable.AddRow(" ");

        TableOutput requiredParamTable;
        requiredParamTable.SetBorders(0, 0, '*', '*');
        requiredParamTable.SetSeparator(' ', 1);
        if (GetRequiredParameterCount() > 0)
            requiredParamTable.AddRow("*Required Param*", "*Type*", "*Description*");

        TableOutput optionalParamTable;
        optionalParamTable.SetBorders(0, 0, '*', '*');
        optionalParamTable.SetSeparator(' ', 1);
        if (GetOptionalParameterCount() > 0)
        {
            optionalParamTable.AddRow("*Optional Param*", "*Type*", "*Description*");
            optionalParamTable.SetBorders(0, '*', '*', '*');        // if there are optional parameters, draw a bottom border after optional table
        }
        else
            requiredParamTable.SetBorders(0, '*', '*', '*');        // if no optional parameters, draw a bottom border after required table

        bool bMultiMode = !mModeToCommandLineParser.empty();

        TableOutput usageTable;
        usageTable.SetBorders(0, 0, '*', '*');
        usageTable.SetSeparator(' ', 1);

        if (bMultiMode)
        {
            descriptionTable.AddRow("*Commands*");
            descriptionTable.AddRow("help COMMAND");

            for (tModeStringToParserMap::iterator it = mModeToCommandLineParser.begin(); it != mModeToCommandLineParser.end(); it++)
            {
                string sCommand = (*it).first;
                if (!sCommand.empty())
                    descriptionTable.AddRow(sCommand);
            }


/*            usageTable.AddRow("*Usage*");
            TableOutput dummyUsageTable;    // only going to use usageTable from mode
            mModeToCommandLineParser[msMode].GetModeUsageOutput(msAppName, msMode, usageTable, descriptionTable, requiredParamTable, optionalParamTable);
            mGeneralCommandLineParser.GetModeUsageOutput(msAppName, "", dummyUsageTable, descriptionTable, requiredParamTable, optionalParamTable);
            usageTable.AddRow(" ");
            requiredParamTable.AddRow(" ");
            */

        }
        else
        {
            usageTable.AddRow("*Usage*");
            mGeneralCommandLineParser.GetModeUsageOutput(msAppName, "", usageTable, descriptionTable, requiredParamTable, optionalParamTable);
            usageTable.AddRow(" ");
            requiredParamTable.AddRow(" ");
        }

        size_t nMinTableWidth = std::max({ (size_t) 120, keyTable.GetTableWidth(), descriptionTable.GetTableWidth(), requiredParamTable.GetTableWidth(), optionalParamTable.GetTableWidth(), usageTable.GetTableWidth() });

        usageTable.SetMinimumOutputWidth(nMinTableWidth);
        keyTable.SetMinimumOutputWidth(nMinTableWidth);
        descriptionTable.SetMinimumOutputWidth(nMinTableWidth);
        requiredParamTable.SetMinimumOutputWidth(nMinTableWidth);
        optionalParamTable.SetMinimumOutputWidth(nMinTableWidth);

        cout << descriptionTable;
        cout << usageTable;
        //        cout << keyTable;
        if (GetRequiredParameterCount() > 0)
            cout << requiredParamTable;
        if (GetOptionalParameterCount() > 0)
            cout << optionalParamTable;
    }




}; // namespace CLP
