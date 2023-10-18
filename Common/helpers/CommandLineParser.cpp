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


#ifdef _WIN64
#define NOMINMAX
#include <Windows.h> // For enabling ANSI color output
#endif


using namespace std;
using namespace SH;

int64_t LOG::gnVerbosityLevel = LVL_DEFAULT;

namespace CLP
{

// Color settings
#define COL_SECTION COL_CYAN
#define COL_ERROR   COL_RED
#define COL_APP     COL_YELLOW
#define COL_PARAM   COL_YELLOW

    ParamDesc::ParamDesc(const string& sName, string* pString, eBehavior behavior, const string& sUsage, const tStringSet& allowedStrings)
    {
        mValueType = kString;
        msName = sName;
        mpValue = (void*)pString;
        mBehaviorFlags = behavior;
        mbFound = false;
        msUsage = sUsage;
        mAllowedStrings = allowedStrings;
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
        mnMinInt = nRangeMin;
        mnMaxInt = nRangeMax;
        mbFound = false;
        msUsage = sUsage;
    }

    ParamDesc::ParamDesc(const string& sName, float* pFloat, eBehavior behavior, const string& sUsage, float fRangeMin, float fRangeMax)
    {
        mValueType = kFloat;
        msName = sName;
        mpValue = (void*)pFloat;
        mBehaviorFlags = behavior;
        mfMinFloat = fRangeMin;
        mfMaxFloat = fRangeMax;
        mbFound = false;
        msUsage = sUsage;
    }

    std::string ParamDesc::ValueToString()
    {
        switch (mValueType)
        {
        case ParamDesc::kString:    return *((string*)mpValue);
        case ParamDesc::kInt64:     return std::to_string(*(int64_t*)mpValue);
        case ParamDesc::kFloat:     return SH::FromDouble(*(float*)mpValue, 2);
        case ParamDesc::kBool:
            if ((*(bool*)mpValue) == true)
                return "true";
            else
                return "false";
        default:
            break;
        }

        return "unknown_value";
    }

    void ParamDesc::GetExample(std::string& sParameter, std::string& sType, std::string& sDefault, std::string& sUsage)
    {
        // for unrequired params, surround with brackets
        sParameter = COL_PARAM;
        if (IsOptional())
            sParameter += "[";

        if (IsNamed())
        {
            sParameter += "-";
            sParameter += msName;
            sDefault = ValueToString();
            switch (mValueType)
            {
            case ParamDesc::kString:
                sParameter += ":";
                if (IsRangeRestricted())
                    sType = "{" + FromSet(mAllowedStrings) + "}";
                else
                    sType = "STRING";
                break;
            case ParamDesc::kInt64:
            {
                sParameter += ":";
                if (IsRangeRestricted())
                    sType = "(" + std::to_string(mnMinInt) + "-" + std::to_string(mnMaxInt) + ")";
                else
                    sType = "#";
            }
            break;
            case ParamDesc::kFloat:
            {
                sParameter += ":";
                if (IsRangeRestricted())
                    sType = "(" + SH::FromDouble(mfMinFloat, 2) + "-" + SH::FromDouble(mfMaxFloat, 2) + ")";
                else
                    sType = "#.#";
            }
            break;
            case ParamDesc::kBool:
                sType = "BOOL";
                break;
            default:
                assert(false);
                break;
            }
        }
        else // kPositional
        {
            sParameter += msName;
            switch (mValueType)
            {
            case ParamDesc::kString:
                if (IsRangeRestricted())
                    sType = "{" + FromSet(mAllowedStrings) + "}";
                else
                    sType = "STRING";
                break;
            case ParamDesc::kInt64:
            {
                if (IsRangeRestricted())
                    sType = "(" + std::to_string(mnMinInt) + "-" + std::to_string(mnMaxInt) + ")";
                else
                    sType = "#";
            }
            break;
            case ParamDesc::kFloat:
            {
                if (IsRangeRestricted())
                    sType = "(" + SH::FromDouble(mfMinFloat, 2) + "-" + SH::FromDouble(mfMaxFloat, 2) + ")";
                else
                    sType = "#.#";
            }
            break;
            case ParamDesc::kBool:
                sType = "BOOL";
                break;
            default:
                assert(false);
                break;
            }
        }

        if (IsOptional())
            sParameter += "]";

        sParameter += COL_RESET;

        sUsage = msUsage;
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
                nErrors++;
            }
        }

        return nErrors == 0;
    }

    void CLModeParser::ShowFoundParameters()
    {
        for (auto p : mParameterDescriptors)
        {
            if (p.mbFound)
            {
                cout << "Parameter " << COL_PARAM << p.msName << COL_RESET << " = \"" << p.ValueToString() << "\"\n";
            }
            else
            {
                if (p.IsRequired())
                {
                    cerr << COL_ERROR << "Error: " << COL_RESET << "Required parameter not set:" << COL_PARAM << p.msName << "\n" << COL_RESET;
                }
            }
        }
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

    bool CLModeParser::HandleArgument(const std::string& sArg)
    {
        bool bVerbose = LOG::gnVerbosityLevel > LVL_DEFAULT;
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
                cerr << COL_ERROR << "Error: " << COL_RESET << "Unknown parameter '" << COL_ERROR << sKey << COL_RESET << "' for mode:" << COL_PARAM << msModeDescription<< "\n" << COL_RESET;
                return false;
            }

            switch (pDesc->mValueType)
            {
                case ParamDesc::kBool:
                {
                    *((bool*)pDesc->mpValue) = SH::ToBool(sValue);    // set the registered bool
                    pDesc->mbFound = true;
                    if (bVerbose)
                        cout << "Set " << sKey << " = " << sValue << "\n";
                    return true;
                }
                break;
                case ParamDesc::kInt64:
                {
                    int64_t nValue = SH::ToInt(sValue);
                    if (pDesc->IsRangeRestricted())
                    {
                        if (nValue < pDesc->mnMinInt || nValue > pDesc->mnMaxInt)
                        {
                            cerr << COL_ERROR << "Error: Value for \"" << sArg << "\" OUT OF RANGE." << COL_RESET << " Allowed range:(" << COL_PARAM << pDesc->mnMinInt << "-" << pDesc->mnMaxInt << COL_RESET << ")\n";
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
                case ParamDesc::kFloat:
                {
                    float fValue = (float)SH::ToDouble(sValue);
                    if (pDesc->IsRangeRestricted())
                    {
                        if (fValue < pDesc->mfMinFloat || fValue > pDesc->mfMaxFloat)
                        {
                            cerr << COL_ERROR << "Error: Value for \"" << sArg << "\" OUT OF RANGE." << COL_RESET << " Allowed range:(" << COL_PARAM << pDesc->mfMinFloat << "-" << pDesc->mfMaxFloat << COL_RESET << ")\n";
                            return false;
                        }
                    }

                    *((float*)pDesc->mpValue) = fValue;    // set the registered float
                    pDesc->mbFound = true;
                    if (bVerbose)
                        cout << "Set " << sKey << " = " << fValue << "\n";
                    return true;
                }
                break;
                default:    // ParamDesc::kString
                {
                    if (pDesc->IsRangeRestricted())
                    {
                        if (pDesc->mAllowedStrings.find(sValue) == pDesc->mAllowedStrings.end())
                        {
                            cerr << COL_ERROR << "Error: Value for \"" << sArg << "\" NOT ALLOWED." << COL_RESET << " Allowed values:{" << COL_PARAM << FromSet(pDesc->mAllowedStrings) << COL_RESET << "}\n";
                            return false;
                        }
                    }
                    pDesc->mbFound = true;
                    *((string*)pDesc->mpValue) = sValue;     // set the registered string
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
                cerr << COL_ERROR << "Error: Too many parameters! Max is:" << GetNumPositionalParamsRegistered() << " parameter:" << sArg << "\n" << COL_RESET;
                return false;
            }
            else
            {
                switch (pPositionalDesc->mValueType)
                {
                case ParamDesc::kBool:
                {
                    *((bool*)pPositionalDesc->mpValue) = SH::ToBool(sArg);    // set the registered bool
                    pPositionalDesc->mbFound = true;
                    if (bVerbose)
                        cout << "Set " << pPositionalDesc->msName << " = " << sArg << "\n";
                    return true;
                }
                break;
                case ParamDesc::kInt64:
                {
                    pPositionalDesc->mbFound = true;
                    int64_t nValue = SH::ToInt(sArg);
                    if (pPositionalDesc->IsRangeRestricted())
                    {
                        if (nValue < pPositionalDesc->mnMinInt || nValue > pPositionalDesc->mnMaxInt)
                        {
                            cerr << COL_ERROR << "Error: Value for \"" << sArg << "\" OUT OF RANGE." << COL_RESET << " Allowed range:(" << COL_PARAM << pPositionalDesc->mnMinInt << "-" << pPositionalDesc->mnMaxInt << COL_RESET << ")\n";
                            return false;
                        }
                    }

                    *((int64_t*)pPositionalDesc->mpValue) = nValue;    // set the registered int
                    if (bVerbose)
                        cout << "Set " << pPositionalDesc->msName << " = " << nValue << "\n";
                    return true;
                }
                break;
                case ParamDesc::kFloat:
                {
                    pPositionalDesc->mbFound = true;
                    float fValue = (float)SH::ToDouble(sArg);
                    if (pPositionalDesc->IsRangeRestricted())
                    {
                        if (fValue < pPositionalDesc->mfMinFloat || fValue > pPositionalDesc->mfMaxFloat)
                        {
                            cerr << COL_ERROR << "Error: Value for \"" << sArg << "\" OUT OF RANGE." << COL_RESET << " Allowed range:(" << COL_PARAM << pPositionalDesc->mfMinFloat << "-" << pPositionalDesc->mfMaxFloat << COL_RESET << ")\n";
                            return false;
                        }
                    }

                    *((float*)pPositionalDesc->mpValue) = fValue;    // set the registered float
                    if (bVerbose)
                        cout << "Set " << pPositionalDesc->msName << " = " << fValue << "\n";
                    return true;
                }
                break;
                default:    // ParamDesc::kString
                {
                    if (pPositionalDesc->IsRangeRestricted())
                    {
                        if (pPositionalDesc->mAllowedStrings.find(sArg) == pPositionalDesc->mAllowedStrings.end())
                        {
                            cerr << COL_ERROR << "Error: Value for \"" << sArg << "\" NOT ALLOWED." << COL_RESET << " Allowed values:{" << COL_PARAM << FromSet(pPositionalDesc->mAllowedStrings) << COL_RESET << "}\n";
                            return false;
                        }
                    }

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
            if (desc.IsNamed() && SH::Compare(desc.msName, sKey, desc.IsCaseSensitive()))
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


    void CommandLineParser::GetCommandLineExample(string& sCommandLineExample)
    {
        // create example command line with positional params first followed by named
        sCommandLineExample = COL_APP + msAppName + " " + COL_PARAM + msMode + COL_RESET;
        sCommandLineExample += COL_PARAM;

        for (auto& desc : mGeneralCommandLineParser.mParameterDescriptors)
        {
            if (desc.IsPositional() && desc.IsRequired())
                sCommandLineExample += " " + desc.msName;
        }
        for (auto& desc : mGeneralCommandLineParser.mParameterDescriptors)
        {
            if (desc.IsNamed() && desc.IsRequired())
                sCommandLineExample += " " + desc.msName;
        }

        if (!msMode.empty())
        {
            for (auto& desc : mModeToCommandLineParser[msMode].mParameterDescriptors)
            {
                if (desc.IsPositional() && desc.IsRequired())
                    sCommandLineExample += " " + desc.msName;
            }
            for (auto& desc : mGeneralCommandLineParser.mParameterDescriptors)
            {
                if (desc.IsNamed() && desc.IsRequired())
                    sCommandLineExample += " " + desc.msName;
            }
        }



        sCommandLineExample += COL_RESET;
    }


    void CLModeParser::GetModeUsageTables(string sMode, TableOutput& modeDescriptionTable, TableOutput& requiredParamTable, TableOutput& optionalParamTable, TableOutput& additionalInfoTable)
    {
        SH::makelower(sMode);

        if (!sMode.empty() && !msModeDescription.empty())
        {
            modeDescriptionTable.AddRow(string("Help for command: " COL_APP ) + sMode.c_str() + string(COL_RESET));


            modeDescriptionTable.AddRow(" ");
//            modeDescriptionTable.AddRow(COL_SECTION "-Command Description-" COL_RESET);
            modeDescriptionTable.AddMultilineRow(msModeDescription);
        }


        for (auto info : mAdditionalInfo)
        {
            additionalInfoTable.AddMultilineRow(info);
        }



        // Create table of params and usages

        // parameters to be output in even columns
        // example:
        // PATH         - The file to process.
        // PROCESS_SIZE - Number of bytes to process. (Range: 1-1TiB)

        for (auto& desc : mParameterDescriptors)
        {
            string sName;
            string sType;
            string sDefault;
            string sUsage;

            desc.GetExample(sName, sType, sDefault, sUsage);
             
            if (desc.IsOptional())
                optionalParamTable.AddRow(sName, sType, sDefault, sUsage);
            else
                requiredParamTable.AddRow(sName, sType, sDefault, sUsage);
        }
    }

    CommandLineParser::CommandLineParser(bool bEnableVerbosity)
    {
#ifdef _WIN64
        // enable color output on windows
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

        DWORD mode = 0;
        GetConsoleMode(hConsole, &mode);
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

        SetConsoleMode(hConsole, mode);
#endif

        if (bEnableVerbosity)
            RegisterParam(ParamDesc("verbose", &LOG::gnVerbosityLevel, CLP::kNamed | CLP::kOptional | CLP::kRangeRestricted, "Verbosity level. 0-silent, 1-default, 2-basic, 3-full", 0, 3));
    }


    void CommandLineParser::RegisterAppDescription(const string& sDescription)
    {
        msAppDescription = sDescription;
    }

    bool CommandLineParser::IsCurrentMode(string sMode)
    {
        SH::makelower(sMode);
        return msMode == sMode;
    }

    bool CommandLineParser::IsRegisteredMode(string sMode)
    {
        SH::makelower(sMode);
        for (tModeToParser::iterator it = mModeToCommandLineParser.begin(); it != mModeToCommandLineParser.end(); it++)
        {
            if ((*it).first == sMode)
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


    bool CommandLineParser::RegisterMode(string sMode, const string& sModeDescription)
    {
        SH::makelower(sMode);
        if (mModeToCommandLineParser.find(sMode) != mModeToCommandLineParser.end())
        {
            assert(false);
            cerr << COL_RED << "Mode already registered:" << sMode << "\n" << COL_RESET;
            return false;
        }

        return mModeToCommandLineParser[sMode].RegisterModeDescription(sModeDescription);
    }

    bool CommandLineParser::RegisterParam(string sMode, ParamDesc param)
    {
        SH::makelower(sMode);
        if (mModeToCommandLineParser.find(sMode) == mModeToCommandLineParser.end())
        {
            assert(false);
            cerr << COL_RED << "Unregistered mode:" << sMode << "\n" << COL_RESET;
            return false;
        }
        return mModeToCommandLineParser[sMode].RegisterParam(param);
    }

    bool CommandLineParser::RegisterParam(ParamDesc param)
    {
        return mGeneralCommandLineParser.RegisterParam(param);
    }

    bool CommandLineParser::AddInfo(const std::string& sInfo)
    {
        return mGeneralCommandLineParser.AddInfo(sInfo);
    }


    bool CommandLineParser::AddInfo(std::string sMode, const std::string& sInfo)
    {
        SH::makelower(sMode);
        if (mModeToCommandLineParser.find(sMode) == mModeToCommandLineParser.end())
        {
            assert(false);
            cerr << COL_RED << "Unregistered mode:" << sMode << "\n" << COL_RESET;
            return false;
        }
        return mModeToCommandLineParser[sMode].AddInfo(sInfo);
    }


    bool CommandLineParser::Parse(int argc, char* argv[])
    {
        msMode.clear();
        msAppPath = argv[0];

    #ifdef _WIN64
        // Ensure the msAppPath extension includes ".exe" since it can be launched without
        if (msAppPath.length() > 4)
        {
            // Ensure the msAppPath extension includes ".exe" since it can be launched without
            string sExtension(msAppPath.substr(msAppPath.length() - 4));
            std::transform(sExtension.begin(), sExtension.end(), sExtension.begin(), [](unsigned char c){ return (unsigned char) std::tolower(c); });
            if (sExtension != ".exe")
                msAppPath += ".exe";
        }
    #endif

        // Extract  application name
        int32_t nLastSlash = (int32_t)msAppPath.find_last_of('/');
        int32_t nLastBackSlash = (int32_t)msAppPath.find_last_of('\\');
        nLastSlash = (nLastSlash > nLastBackSlash) ? nLastSlash : nLastBackSlash;

        if (nLastSlash != string::npos)
        {
            msAppName = msAppPath.substr(nLastSlash + 1);
            msAppPath = msAppPath.substr(0, nLastBackSlash);
        }
        else
            msAppName = msAppPath;


        // Handle help
        // 
        // 1) Multi Mode no context
        //      list commands
        // 2) Multi Mode with context
        //      a) registered mode  -> show mode specific help
        //      b) unknown mode     -> "use help message"
        // 3) Single Mode
        //      Show general help

        bool bMultiMode = !mModeToCommandLineParser.empty();

        int nErrors = 0;

        if (argc > 1)
        {
            bool bShowHelp = ContainsArgument("?", argc, argv);
            bool bDetailedHelp = ContainsArgument("??", argc, argv);

            string sMode = GetFirstPositionalArgument(argc, argv); // mode
            if (bMultiMode && !IsRegisteredMode(sMode))
                bShowHelp = true;

            // If "help" requested
            SH::makelower(sMode);
            if (bShowHelp || bDetailedHelp)
            {
                if (bMultiMode)
                {
                    SH::makelower(sMode);
                    if (IsRegisteredMode(sMode))
                    {
                        // case 2a
                        msMode = sMode;
                        OutputHelp(bDetailedHelp);
                        return false;
                    }
                    else
                    {
                        ListModes();
                        return false;

                    }
                }
                else
                {
                    // single mode case 3
                    OutputHelp(bDetailedHelp);
                }

                return false;
            }

            // Handle Command Line

            // 1) Multi Mode for each param
            //      a) mode specific parameter  -> Mode Handle
            //      b) non mode specific        -> General Handle
            //      c) unknown parameter        -> unknown param error
            // 
            //      Check all mode reqs
            //
            // 2) Single mode for each param
            //      general handle
            //      Check all general reqs
            if (IsRegisteredMode(sMode))
            {
                // Case 1
                msMode = sMode;
                for (int i = 2; i < argc; i++)
                {
                    std::string sParam(argv[i]);

                    if (mModeToCommandLineParser[msMode].CanHandleArgument(sParam))
                    {
                        // case 1a
                        if (!mModeToCommandLineParser[msMode].HandleArgument(sParam))
                            nErrors++;
                    }
                    else if (mGeneralCommandLineParser.CanHandleArgument(sParam))
                    {
                        // case 1b
                        if (!mGeneralCommandLineParser.HandleArgument(sParam))
                            nErrors++;
                    }
                    else
                    {
                        // case 1c
                        cerr << COL_ERROR << "Error:" COL_RESET " Unknown parameter '" << COL_RED << sParam << COL_RESET << "'\n";
                        nErrors++;
                    }
                }

                if (nErrors > 0 || !mModeToCommandLineParser[msMode].CheckAllRequirementsMet() || !mGeneralCommandLineParser.CheckAllRequirementsMet())
                {
                    mModeToCommandLineParser[msMode].ShowFoundParameters();
                    mGeneralCommandLineParser.ShowFoundParameters();

                    string sCommandLineExample;
                    TableOutput usageTable;
                    GetCommandLineExample(sCommandLineExample);
                    usageTable.AddRow(COL_SECTION "--------Usage---------" COL_RESET);
                    usageTable.AddRow(sCommandLineExample);
                    cout << usageTable;


                    cout << "\n\"" << COL_APP << msAppName << " " << COL_PARAM << msMode << " ?" << COL_RESET << "\" - to see usage.\n";
                    return false;
                }

                return true;      
            }
            else
            {
                // Case 2
                for (int i = 1; i < argc; i++)
                {
                    std::string sParam(argv[i]);

                    if (mGeneralCommandLineParser.CanHandleArgument(sParam))
                    {
                        if (!mGeneralCommandLineParser.HandleArgument(sParam))
                            nErrors++;
                    }
                    else
                    {
                        cerr << COL_ERROR << "Error: Unknown parameter '" << sParam << "'\n" << COL_RESET;
                        nErrors++;
                    }
                }

                if (!mGeneralCommandLineParser.CheckAllRequirementsMet())
                {
                    mGeneralCommandLineParser.ShowFoundParameters();

                    string sCommandLineExample;
                    TableOutput usageTable;
                    GetCommandLineExample(sCommandLineExample);
                    usageTable.AddRow(COL_SECTION "--------Usage---------" COL_RESET);
                    usageTable.AddRow(sCommandLineExample);
                    cout << usageTable;

                    cout << "\n\"" << COL_APP << msAppName << COL_PARAM << " ?\" - to see usage.\n" << COL_RESET;
                    return false;
                }

                return true;    // all single mode requirements met

            }
        }


        // no parameters but multi-modes are available... list modes
        if (!mModeToCommandLineParser.empty())
        {
            ListModes();
            return false;
        }

        if (!mGeneralCommandLineParser.CheckAllRequirementsMet())   // single mode
        {
            cout << "\n\"" << COL_APP << msAppName << COL_PARAM << " -?\" - to see usage.\n" << COL_RESET;
            return false;
        }


        // no parameters and none required
        return true;
    }

    bool CommandLineParser::ContainsArgument(std::string sArgument, int argc, char* argv[], bool bCaseSensitive)
    {
        for (int i = 1; i < argc; i++)
            if (SH::Compare(argv[i], sArgument, bCaseSensitive))
                return true;

        return false;
    }

    std::string CommandLineParser::GetFirstPositionalArgument(int argc, char* argv[])
    {
        for (int i = 1; i < argc; i++)
            if (argv[i][0] != '-')
                return argv[i];

        return "";
    }


    void CommandLineParser::OutputHelp(bool bDetailed)
    {
        TableOutput usageTable;
        TableOutput descriptionTable;
        TableOutput requiredParamTable;
        TableOutput optionalParamTable;
        TableOutput additionalInfoTable;

        bool bHasRequiredParameters = false;
        bool bHasOptionalParameters = false;
        bool bHasAdditionalInfo = false;


        descriptionTable.SetBorders('*', '-', '*', '*');
        descriptionTable.SetSeparator(' ', 1);


        string sCommandLineExample;

        // if modal
        if (!msMode.empty())
        {
            bHasRequiredParameters = mGeneralCommandLineParser.GetRequiredParameterCount() + mModeToCommandLineParser[msMode].GetRequiredParameterCount() > 0;
            bHasOptionalParameters = mGeneralCommandLineParser.GetOptionalParameterCount() + mModeToCommandLineParser[msMode].GetOptionalParameterCount() > 0;
            bHasAdditionalInfo = mGeneralCommandLineParser.mAdditionalInfo.size() + mModeToCommandLineParser[msMode].mAdditionalInfo.size() > 0;

            if (bHasAdditionalInfo)
            {
                additionalInfoTable.AddRow(" ");
                additionalInfoTable.AddRow(COL_SECTION "---Additional Info----" COL_RESET);

                additionalInfoTable.SetSeparator(' ', 1);
                additionalInfoTable.SetBorders(0, '-', '*', '*');
            }

            if (bHasRequiredParameters)
            {
                requiredParamTable.AddRow(" ");
                requiredParamTable.AddRow(COL_SECTION "-------Required------", "---Type---", "---Default---", "---Description---" COL_RESET);
                requiredParamTable.SetBorders(0, 0, '*', '*');
                requiredParamTable.SetSeparator(' ', 1);
            }

            if (bHasOptionalParameters)
            {
                optionalParamTable.AddRow(" ");
                optionalParamTable.AddRow(COL_SECTION "------[Options]------", "---Type---", "---Default---", "---Description---" COL_RESET);
                optionalParamTable.SetBorders(0, 0, '*', '*');
                optionalParamTable.SetSeparator(' ', 1);
            }


            GetCommandLineExample(sCommandLineExample);
            mModeToCommandLineParser[msMode].GetModeUsageTables(msMode, descriptionTable, requiredParamTable, optionalParamTable, additionalInfoTable);
            mGeneralCommandLineParser.GetModeUsageTables("", descriptionTable, requiredParamTable, optionalParamTable, additionalInfoTable);
        }
        else
        {
            bHasRequiredParameters = mGeneralCommandLineParser.GetRequiredParameterCount();
            bHasOptionalParameters = mGeneralCommandLineParser.GetOptionalParameterCount();
            bHasAdditionalInfo = mGeneralCommandLineParser.mAdditionalInfo.size();


            descriptionTable.AddMultilineRow(msAppDescription);
            descriptionTable.AddRow(" ");


            if (bHasAdditionalInfo)
            {
                additionalInfoTable.AddRow(" ");
                additionalInfoTable.AddRow(COL_SECTION "---Additional Info----" COL_RESET);

                additionalInfoTable.SetSeparator(' ', 1);
                additionalInfoTable.SetBorders(0, '-', '*', '*');
            }

            if (bHasRequiredParameters)
            {
                requiredParamTable.AddRow(" ");
                requiredParamTable.AddRow(COL_SECTION "-------Required------", "---Type---", "---Default---", "---Description---" COL_RESET);
                requiredParamTable.SetBorders(0, 0, '*', '*');
                requiredParamTable.SetSeparator(' ', 1);
            }

            if (bHasOptionalParameters)
            {
                optionalParamTable.AddRow(" ");
                optionalParamTable.AddRow(COL_SECTION "------[Options]------", "---Type---", "---Default---", "---Description---" COL_RESET);
                optionalParamTable.SetBorders(0, 0, '*', '*');
                optionalParamTable.SetSeparator(' ', 1);
            }

            GetCommandLineExample(sCommandLineExample);
            mGeneralCommandLineParser.GetModeUsageTables("", descriptionTable, requiredParamTable, optionalParamTable, additionalInfoTable);
        }

        if (bHasOptionalParameters)
            sCommandLineExample += COL_PARAM " [Options]" COL_RESET;

        usageTable.AddRow(COL_SECTION "--------Usage---------" COL_RESET);
        usageTable.AddRow(sCommandLineExample);
        usageTable.SetSeparator(' ', 1);
        usageTable.SetBorders('-', '*', '*', '*');


        TableOutput keyTable;
        if (bDetailed)
        {
            keyTable.SetSeparator(' ', 1);
            keyTable.SetBorders('-', 0, '*', '*');

            keyTable.AddRow(COL_SECTION "--------Keys---------" COL_RESET);
            keyTable.AddRow(COL_SECTION "         [] ", "Optional" COL_RESET);
            keyTable.AddRow(COL_SECTION "          - ", "Named '-key:value' pair. (examples: -size:1KB  -verbose:3)" COL_RESET);
            keyTable.AddRow(COL_SECTION "            ", "Can be anywhere on command line, in any order." COL_RESET);
            keyTable.AddRow(COL_SECTION "          # ", "NUMBER" COL_RESET);
            keyTable.AddRow(COL_SECTION "            ", "Can be hex (0x05) or decimal or floating point." COL_RESET);
            keyTable.AddRow(COL_SECTION "            ", "Can include commas (1,000)" COL_RESET);
            keyTable.AddRow(COL_SECTION "            ", "Can include scale labels (10k, 64KiB, etc.)" COL_RESET);
            keyTable.AddRow(COL_SECTION "        BOOL", "Boolean value can be 1/0, t/f, y/n, yes/no. Presence of the flag means true." COL_RESET);
        }


        size_t nMinTableWidth = std::max({ (size_t) 120, 
                                            descriptionTable.GetTableWidth(), 
                                            requiredParamTable.GetTableWidth(), 
                                            optionalParamTable.GetTableWidth(), 
                                            usageTable.GetTableWidth(), 
                                            keyTable.GetTableWidth(),
                                            additionalInfoTable.GetTableWidth()});

        keyTable.SetMinimumOutputWidth(nMinTableWidth);
        requiredParamTable.SetMinimumOutputWidth(nMinTableWidth);
        optionalParamTable.SetMinimumOutputWidth(nMinTableWidth);
        descriptionTable.SetMinimumOutputWidth(nMinTableWidth);
        additionalInfoTable.SetMinimumOutputWidth(nMinTableWidth);
        usageTable.SetMinimumOutputWidth(nMinTableWidth);


        // Now default/global
        cout << descriptionTable;
        if (bHasAdditionalInfo)
            cout << additionalInfoTable;
        if (bHasRequiredParameters)
            cout << requiredParamTable;
        if (bHasOptionalParameters)   // 1 because "Optional:" added above
            cout << optionalParamTable;
        if (bDetailed)
            cout << keyTable;
        cout << usageTable;
    }

    void CommandLineParser::ListModes()
    {
        // First output Application name

        TableOutput descriptionTable;
        descriptionTable.SetBorders('*', '*', '*', '*');
        descriptionTable.SetSeparator(' ', 1);

        std::string sModes( string(COL_APP) + msAppName + string(COL_RESET) + " {");
        for (tModeToParser::iterator it = mModeToCommandLineParser.begin(); it != mModeToCommandLineParser.end(); it++)
        {
            sModes += string(COL_PARAM) + (*it).first + string(COL_RESET) + "|";
        }
        sModes[sModes.length() - 1] = '}';  // change last | into }

        descriptionTable.AddRow(sModes);

        descriptionTable.AddRow(" ");
        descriptionTable.AddRow(COL_SECTION "-----Application Description-----" COL_RESET);
        descriptionTable.AddMultilineRow(msAppDescription);
        descriptionTable.AddRow(" ");


        TableOutput commandsTable;
        commandsTable.SetBorders(0, '*', '*', '*');
        commandsTable.SetSeparator(' ', 1);

        commandsTable.AddRow(COL_SECTION "-------Available Commands--------" COL_RESET);
        for (tModeToParser::iterator it = mModeToCommandLineParser.begin(); it != mModeToCommandLineParser.end(); it++)
        {
            string sCommand = (*it).first;
            string sModeDescription = ((*it).second).GetModeDescription();
            if (!sCommand.empty())
                commandsTable.AddRow(COL_PARAM + sCommand + COL_RESET, sModeDescription);
        }

        commandsTable.AddRow(COL_SECTION "--------------Help---------------" COL_RESET);
        commandsTable.AddRow(COL_PARAM "COMMAND ?" COL_RESET, "Add '?' anywhere on command line for list of commands or context specific help.");
        commandsTable.AddRow(COL_PARAM "COMMAND ??" COL_RESET, "Detailed help including parameter keys.");


        size_t nMinTableWidth = std::max({ (size_t) 120, commandsTable.GetTableWidth(), descriptionTable.GetTableWidth() });

        descriptionTable.SetMinimumOutputWidth(nMinTableWidth);
        commandsTable.SetMinimumOutputWidth(nMinTableWidth);

        cout << descriptionTable;
        cout << commandsTable;
    }




}; // namespace CLP
