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
        std::transform(sValue.begin(), sValue.end(), sValue.begin(), std::tolower);
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
        std::transform(sReadable.begin(), sReadable.end(), sReadable.begin(), std::toupper);

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
            sprintf_s(buf, "%lldPiB", nValue / kPiB);
        else if (nValue % kPB == 0)
            sprintf_s(buf, "%lldPB", nValue / kPB);

        else if (nValue % kTiB == 0)
            sprintf_s(buf, "%lldTiB", nValue / kTiB);
        else if (nValue % kTB == 0)
            sprintf_s(buf, "%lldTB", nValue / kTB);

        else if (nValue % kGiB == 0)
            sprintf_s(buf, "%lldGiB", nValue / kGiB);
        else if (nValue % kGB == 0)
            sprintf_s(buf, "%lldGB", nValue / kGB);

        else if (nValue % kMiB == 0)
            sprintf_s(buf, "%lldMiB", nValue / kMiB);
        else if (nValue % kMB == 0)
            sprintf_s(buf, "%lldMB", nValue / kMB);

        else if (nValue % kKiB == 0)
            sprintf_s(buf, "%lldKiB", nValue / kKiB);
        else if (nValue % kKB == 0)
            sprintf_s(buf, "%lldKB", nValue / kKB);

        else sprintf_s(buf, "%lld", nValue);

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


    string ParamDesc::GetExampleString()
    {
        // for unrequired params, surround with brackets
        string sCommandExample = " ";
        if (IsOptional())
            sCommandExample += "[";

        if (IsNamed())
        {
            sCommandExample += "-";
            switch (mValueType)
            {
            case ParamDesc::kString:
                sCommandExample += msName + ":$$";
                break;
            case ParamDesc::kInt64:
                sCommandExample += msName + ":";
                if (IsRangeRestricted())
                    sCommandExample += "(" + UserReadableFromInt(mnMinValue) + "-" + UserReadableFromInt(mnMaxValue) + ")";
                else
                    sCommandExample += "##";

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
                sCommandExample += msName + " ($$)";
                break;
            case ParamDesc::kInt64:
                if (IsRangeRestricted())
                    sCommandExample += msName + "(" + UserReadableFromInt(mnMinValue) + "-" + UserReadableFromInt(mnMaxValue) + ")";
                else
                    sCommandExample += msName + " (##)";

                break;
            case ParamDesc::kBool:
                sCommandExample += msName + "(bool)";
            }
        }

        if (IsOptional())
            sCommandExample += "]";

        return sCommandExample;
    }

    CLModeParser::CLModeParser()
    {
        mnRegisteredPositional = 0;
    }

    CLModeParser::~CLModeParser()
    {
    }

    bool CLModeParser::RegisterParam(ParamDesc param)
    {
        // Assign positional index based on how many have been registered
        if (param.IsPositional())
            param.mnPosition = mnRegisteredPositional++;

        mParameterDescriptors.emplace_back(param);

        return true;
    }

    // Arg0 and any mode Arg1 specified should be stripped by now
    bool CLModeParser::Parse(int argc, char* argv[], bool bVerbose)
    {
        int64_t nPositionalParametersFound = 0;

        bool bError = false;
        if (argc < 1)
        {
            bError = true;
            goto errorprompt;
        }

        for (int i = 0; i < argc; i++)
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
                        sKey = sParam.substr(1, nIndexOfColon - 1).c_str();    // everything from first char to colon
                        sValue = sParam.substr(nIndexOfColon + 1).c_str(); // everything after the colon
                    }
                    else
                    {
                        // flag with no value is the same as -flag:true
                        sKey = sParam.substr(1, nIndexOfColon).c_str();
                        sValue = "true";
                    }

                    ParamDesc* pDesc = nullptr;
                    if (!GetDescriptor(sKey, &pDesc))
                    {
                        cerr << "Error: Unknown parameter \"" << sParam << "\"\n";
                        bError = true;
                        continue;
                    }

                    switch (pDesc->mValueType)
                    {
                    case ParamDesc::kBool:
                    {
                        *((bool*)pDesc->mpValue) = StringToBool(sValue);    // set the registered bool
                        pDesc->mbFound = true;
                        if (bVerbose)
                            cout << "Set " << sKey << " = " << sValue << "\n";
                    }
                    break;
                    case ParamDesc::kInt64:
                    {
                        int64_t nValue = IntFromUserReadable(sValue);
                        if (pDesc->IsRangeRestricted())
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
                    if (!GetDescriptor(nPositionalParametersFound, &pPositionalDesc))
                    {
                        cerr << "Error: Too many parameters! Max is:" << mnRegisteredPositional << " parameter:" << sParam << "\n";
                        bError = true;
                        continue;
                    }
                    else
                    {
                        switch (pPositionalDesc->mValueType)
                        {
                        case ParamDesc::kBool:
                        {
                            *((bool*)pPositionalDesc->mpValue) = StringToBool(sParam);    // set the registered bool
                            pPositionalDesc->mbFound = true;
                            if (bVerbose)
                                cout << "Set " << pPositionalDesc->msName << " = " << sParam << "\n";
                        }
                        break;
                        case ParamDesc::kInt64:
                        {
                            pPositionalDesc->mbFound = true;
                            int64_t nValue = IntFromUserReadable(sParam);
                            if (pPositionalDesc->IsRangeRestricted())
                            {
                                if (nValue < pPositionalDesc->mnMinValue || nValue > pPositionalDesc->mnMaxValue)
                                {
                                    bError = true;
                                    cerr << "Error: Value for parameter \"" << sParam << "\" is:" << nValue << ". Allowed values from min:" << UserReadableFromInt(pPositionalDesc->mnMinValue) << " to max:" << UserReadableFromInt(pPositionalDesc->mnMaxValue) << "\n";
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

    errorprompt:
        for (auto desc : mParameterDescriptors)
        {
            if (desc.IsRequired() && !desc.mbFound)
            {
                cerr << "Error: Required parameter not found:" << desc.msName << "\n";
                bError = true;
            }
        }

        if (bError)
            return false;

        return true;
    }

    bool CLModeParser::GetDescriptor(const string& sKey, ParamDesc** pDescriptorOut)
    {
        //    cout << "retrieving named desciptor for:" << sKey << "size:" << mNamedParameterDescriptors.size() << "\n";
        for (auto& desc : mParameterDescriptors)
        {
            if (desc.IsNamed() && desc.msName == sKey)
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

    bool CLModeParser::GetParamWasFound(int64_t nIndex)
    {
        ParamDesc* pDesc = nullptr;
        if (GetDescriptor(nIndex, &pDesc))
        {
            return pDesc->mbFound;
        }

        return false;
    }

    void CLModeParser::OutputModeUsage(const string& sAppName, const string& sMode)
    {
        string sCommandLineExample = sAppName + " " + sMode;

        // create example command line with positional params first followed by named
        for (auto& desc : mParameterDescriptors)
        {
            if (desc.IsPositional())
                sCommandLineExample += " " + desc.GetExampleString();
        }
        for (auto& desc : mParameterDescriptors)
        {
            if (desc.IsNamed())
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
                nLeftColumnWidth = (int32_t)entry.length();
        }

        int32_t nRightColumnWidth = 0;
        for (auto entry : rightColumn)
        {
            int32_t nWidth = LongestLine(entry);
            if (nWidth > nRightColumnWidth)
                nRightColumnWidth = nWidth;
        }

        // Compute longest line from app description
        int32_t nWidth = LongestLine(msModeDescription);
        if (sCommandLineExample.length() > nWidth)
            nWidth = (int32_t)sCommandLineExample.length();

        if (nWidth < nLeftColumnWidth + nRightColumnWidth)
            nWidth = nLeftColumnWidth + nRightColumnWidth;

        if (nWidth < 80)
            nWidth = 80;
        nWidth += 6;

        cout << "\n";
        RepeatOut('*', nWidth, true);

        OutputFixed(nWidth, "Keys:");
        OutputFixed(nWidth, "       [] -> optional parameters");
        OutputFixed(nWidth, "       -  -> named key:value pair. (-size:1KB  -verbose)");
        OutputFixed(nWidth, "                    Can be anywhere on command line, in any order.");
        OutputFixed(nWidth, "       ## -> NUMBER Can be hex (0x05) or decimal");
        OutputFixed(nWidth, "                    Can include commas (1,000)");
        OutputFixed(nWidth, "                    Can include scale labels (10k, 64KiB, etc.)");
        OutputFixed(nWidth, "       $$ -> STRING");

        RepeatOut('*', nWidth, true);

        if (!msModeDescription.empty())
        {
            if (!sMode.empty())
                OutputFixed(nWidth, "Command: %s", sMode.c_str());
            OutputLines(nWidth, msModeDescription);
            RepeatOut('*', nWidth, true);
        }


        RepeatOut('*', nWidth, true);
        OutputFixed(nWidth, "Usage:");
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


    void CommandLineParser::RegisterAppDescription(const string& sDescription)
    {
        msAppDescription = sDescription;
    }


    string CommandLineParser::FindMode(const string& sArg)
    {
        for (tModeStringToParserMap::iterator it = mModeToCommandLineParser.begin(); it != mModeToCommandLineParser.end(); it++)
        {
            if ((*it).first == sArg)
                return sArg;
        }

        return "";
    }

    bool CommandLineParser::GetParamWasFound(const string& sKey)
    {
        if (msMode.empty())
            return mDefaultCommandLineParser.GetParamWasFound(sKey);

        return mModeToCommandLineParser[msMode].GetParamWasFound(sKey);
    }

    bool CommandLineParser::GetParamWasFound(int64_t nIndex)
    {
        if (msMode.empty())
            return mDefaultCommandLineParser.GetParamWasFound(nIndex);

        return mModeToCommandLineParser[msMode].GetParamWasFound(nIndex);
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
        return mDefaultCommandLineParser.RegisterParam(param);
    }

    bool CommandLineParser::Parse(int argc, char* argv[], bool bVerbose)
    {
        msAppPath = argv[0];
        // Ensure the msAppPath extension includes ".exe" since it can be launched without
        if (msAppPath.length() > 4)
        {
            // Ensure the msAppPath extension includes ".exe" since it can be launched without
            string sExtension(msAppPath.substr(msAppPath.length() - 4));
            std::transform(sExtension.begin(), sExtension.end(), sExtension.begin(), std::tolower);
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

            msMode = FindMode(sFirst);
        }

        // if multi-mode but no mode specified
        if (msMode.empty() && !mModeToCommandLineParser.empty())
        {
            OutputUsage();
            return false;
        }

        if (msMode.empty()) // if no command mode specified use default parser
        {
            if (!mDefaultCommandLineParser.Parse(argc-1, argv+1, bVerbose)) // strip off app path argv[0]
            {
//                mDefaultCommandLineParser.OutputModeUsage(msAppName);
                return false;
            }
        }
        else
        {
            if (!mModeToCommandLineParser[msMode].Parse(argc - 2, argv + 2, bVerbose))  // "strip" off the app path argv[0] and the mode argv[1]
            {
  //              mModeToCommandLineParser[msMode].OutputModeUsage(msAppName, msMode);
                return false;
            }
        }
        return true;
    }

    void CommandLineParser::OutputHelp()
    {
        if (msMode.empty())
            return mDefaultCommandLineParser.OutputModeUsage(msAppName);

        if (mModeToCommandLineParser.find(msMode) == mModeToCommandLineParser.end())
        {
            cerr << "help: Unknown command:" << msMode << "\n";
            return;
        }

        mModeToCommandLineParser[msMode].OutputModeUsage(msAppName, msMode);
    }

    void CommandLineParser::OutputUsage()
    {
        // First output Application name

        uint32_t nWidth = LongestLine(msAppDescription);
        if (msAppName.length() > nWidth)
            nWidth = (uint32_t) msAppName.length();

        if (nWidth < 80)
            nWidth = 80;
        nWidth += 6;

        RepeatOut('*', nWidth, true);

        OutputFixed(nWidth, msAppName.c_str());
        OutputFixed(nWidth, " ");

        if (!msAppDescription.empty())
        {
            OutputLines(nWidth, msAppDescription);
        }
        RepeatOut('*', nWidth, true);


        bool bMultiMode = !mModeToCommandLineParser.empty();

        if (bMultiMode)
        {
            OutputFixed(nWidth, "Commands:");
            OutputFixed(nWidth, "help COMMAND - Details for the command.");
            for (tModeStringToParserMap::iterator it = mModeToCommandLineParser.begin(); it != mModeToCommandLineParser.end(); it++)
            {
                string sCommand = (*it).first;
                OutputFixed(nWidth, sCommand.c_str());
            }
            RepeatOut('*', nWidth, true);
        }
        else
            mDefaultCommandLineParser.OutputModeUsage(msAppName);
    }




}; // namespace CLP
