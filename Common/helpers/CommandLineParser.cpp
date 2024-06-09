#include "CommandLineParser.h"
#include <algorithm>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <cctype>
#include <assert.h>
#include <stdarg.h> 
#include <sstream>
#include <filesystem>
#include <algorithm>



#ifdef _WIN64
#define NOMINMAX
#include <Windows.h> // For enabling ANSI color output


#ifdef ENABLE_CLE
#include "CommandLineEditor.h"
    #ifdef _DEBUG
    #include <conio.h> 
    #endif
#endif // ENABLE_CLE

#endif // _WIN64

#ifdef _DEBUG
#define PAUSE_FOR_KEY { std::cout << "hit any key..."; _getch(); }
#else
#define PAUSE_FOR_KEY 
#endif



using namespace std;
using namespace SH;
namespace fs = std::filesystem;

//int64_t LOG::gnVerbosityLevel = LVL_DEFAULT;

namespace CLP
{
    string appPath;
    string appName;

    ParamDesc::ParamDesc(const string& sName, string* pString, eBehavior behavior, const string& sUsage, const tStringSet& allowedStrings)
    {
        mValueType = kString;
        msName = sName;
        mpValue = (void*)pString;
        mBehaviorFlags = behavior;
        mbFound = false;
        msUsage = sUsage;
        mAllowedStrings = allowedStrings;

        if (!mAllowedStrings.empty())
            mBehaviorFlags |= kRangeRestricted;
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

    ParamDesc::ParamDesc(const string& sName, int64_t* pInt, eBehavior behavior, const string& sUsage, std::optional<int64_t> nRangeMin, std::optional<int64_t> nRangeMax)
    {
        mValueType = kInt64;
        msName = sName;
        mpValue = (void*)pInt;
        mBehaviorFlags = behavior;
        mnMinInt = nRangeMin;
        mnMaxInt = nRangeMax;

        if (mnMinInt.has_value() || mnMaxInt.has_value())
            mBehaviorFlags |= kRangeRestricted;

        mbFound = false;
        msUsage = sUsage;
    }

    ParamDesc::ParamDesc(const string& sName, float* pFloat, eBehavior behavior, const string& sUsage, std::optional<float> fRangeMin, std::optional<float> fRangeMax)
    {
        mValueType = kFloat;
        msName = sName;
        mpValue = (void*)pFloat;
        mBehaviorFlags = behavior;
        mfMinFloat = fRangeMin;
        mfMaxFloat = fRangeMax;

        if (mfMinFloat.has_value() || mfMaxFloat.has_value())
            mBehaviorFlags |= kRangeRestricted;

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

    bool ParamDesc::Satisfied()
    {
        // basic case of optional unrestricted
        if (IsOptional() && !IsRangeRestricted())
            return true;

        if (IsRangeRestricted())
        {
            if (mpValue)
            {
                switch (mValueType)
                {
                    case ParamDesc::kString: 
                    {
                        string sValue = *(string*)mpValue;
                        return mAllowedStrings.find(sValue) != mAllowedStrings.end();
                    }
                    case ParamDesc::kInt64:
                    {
                        int64_t nValue = *(int64_t*)mpValue;
                        return nValue >= mnMinInt && nValue <= mnMaxInt;
                    }
                    case ParamDesc::kFloat:
                    {
                        float fValue = *(float*)mpValue;
                        return fValue >= mfMaxFloat && fValue <= mfMaxFloat;
                    }
                    case ParamDesc::kBool:
                    {
                        return true;
                    }
                    default:
                        break;
                }
            }

            return false;
        }

        return true;    // not range restricted and optional
    }

    bool ParamDesc::DoesValueSatisfy(const std::string& sValue, std::string& sFailMessage, bool bOutputError)
    {
        // Check path requirements
        if (MustHaveAnExistingPath())
        {
            string sPath(CommandLineParser::StripEnclosure(sValue));
            if (!fs::exists(sPath))
            {
                sFailMessage = "Error: path not found:" + sPath;
                if (bOutputError)
                    cerr << cols[kERROR] << sFailMessage << cols[kRESET] << "\n";
                return false;
            }
        }

        if (MustNotHaveAnExistingPath())
        {
            string sPath(CommandLineParser::StripEnclosure(sValue));
            if (fs::exists(sPath))
            {
                sFailMessage = "Error: " + sPath + " not exist";
                if (bOutputError)
                    cerr << cols[kERROR] << sFailMessage << cols[kRESET] << "\n";
                return false;
            }
        }

        // basic case of optional unrestricted
        if (IsOptional() && !IsRangeRestricted())
            return true;

        if (IsRangeRestricted())
        {
            switch (mValueType)
            {
                case ParamDesc::kString:
                {
                    if (mAllowedStrings.find(sValue) != mAllowedStrings.end())
                        return true;

                    sFailMessage = "Error: Allowed values:{" + FromSet(mAllowedStrings)  + "}";
                    if (bOutputError)
                        cerr << cols[kERROR] + sFailMessage + cols[kRESET] << "\n";
                    return false;
                }
                case ParamDesc::kInt64:
                {
                    int64_t nValue = SH::ToInt(sValue);
                    if (nValue >= mnMinInt && nValue <= mnMaxInt)
                        return true;
                    sFailMessage =  "Error: Allowed range:(" + SH::FromInt(mnMinInt.value()) + "-" + SH::FromInt(mnMaxInt.value()) + ")";
                    if (bOutputError)
                        cerr << cols[kERROR] << sFailMessage << cols[kRESET] << "\n";
                    return false;
                }
                case ParamDesc::kFloat:
                {
                    float fValue = (float)SH::ToDouble(sValue);
                    if (fValue >= mfMaxFloat && fValue <= mfMaxFloat)
                        return true;
                    sFailMessage = "Error: Allowed range:(" + SH::FromDouble(mfMinFloat.value()) + "-" + SH::FromDouble(mfMaxFloat.value()) + ")";
                    if (bOutputError)
                        cerr << cols[kERROR] << sFailMessage << cols[kRESET] << "\n";
                    return false;
                }
                case ParamDesc::kBool:
                {
                    return true;
                }
                default:
                    break;
            }

            return false;
        }

        return true;    // not range restricted and optional
    }



    void ParamDesc::GetExample(std::string& sParameter, std::string& sType, std::string& sDefault, std::string& sUsage)
    {
        // for unrequired params, surround with brackets
        sParameter = cols[kPARAM];
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
                    sType = "(" + std::to_string(mnMinInt.value()) + "-" + std::to_string(mnMaxInt.value()) + ")";
                else
                    sType = "#";
            }
            break;
            case ParamDesc::kFloat:
            {
                sParameter += ":";
                if (IsRangeRestricted())
                    sType = "(" + SH::FromDouble(mfMinFloat.value(), 2) + "-" + SH::FromDouble(mfMaxFloat.value(), 2) + ")";
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
                    sType = "(" + std::to_string(mnMinInt.value()) + "-" + std::to_string(mnMaxInt.value()) + ")";
                else
                    sType = "#";
            }
            break;
            case ParamDesc::kFloat:
            {
                if (IsRangeRestricted())
                    sType = "(" + SH::FromDouble(mfMinFloat.value(), 2) + "-" + SH::FromDouble(mfMaxFloat.value(), 2) + ")";
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

        sParameter += cols[kRESET];

        sUsage = msUsage;
    }

    bool CLModeParser::RegisterParam(ParamDesc param)
    {
        // Assign positional index based on how many have been registered
        if (param.IsPositional())
            param.mnPosition = GetNumPositionalParamsRegistered();
        else
            param.mnPosition = -1;

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
                cout << "Parameter " << cols[kPARAM] << p.msName << cols[kRESET] << " = \"" << p.ValueToString() << "\"\n";
            }
            else
            {
                if (p.IsRequired())
                {
                    cerr << cols[kERROR] << "Error: " << cols[kRESET] << "Required parameter not set:" << cols[kPARAM] << p.msName << "\n" << cols[kRESET];
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
                cerr << cols[kERROR] << "Error: " << cols[kRESET] << "Unknown parameter '" << cols[kERROR] << sKey << cols[kRESET] << "' for mode:" << cols[kPARAM] << msModeDescription<< "\n" << cols[kRESET];
                return false;
            }

            string sFailMessage;
            if (!pDesc->DoesValueSatisfy(sValue, sFailMessage, true))
            {
                return false;
            }

            pDesc->mbFound = true;

            switch (pDesc->mValueType)
            {
                case ParamDesc::kBool:
                    *((bool*)pDesc->mpValue) = SH::ToBool(sValue);    // set the registered bool
                    break;
                case ParamDesc::kInt64:
                    *((int64_t*)pDesc->mpValue) = SH::ToInt(sValue);    // set the registered int
                    break;
                case ParamDesc::kFloat:
                    *((float*)pDesc->mpValue) = (float)SH::ToDouble(sValue);    // set the registered float
                    break;
                default:    // ParamDesc::kString
                    *((string*)pDesc->mpValue) = sValue;     // set the registered string
                    break;
            }
            return true;
        }
        else
        {
            ////////////////////////////////////////////
            // Positional parameter processing
            ParamDesc* pPositionalDesc = nullptr;
            if (!GetDescriptor(GetNumPositionalParamsHandled(), &pPositionalDesc))  // num handled is also the index of the next one
            {
                cerr << cols[kERROR] << "Error: Too many parameters! Max is:" << GetNumPositionalParamsRegistered() << " parameter:" << sArg << "\n" << cols[kRESET];
                return false;
            }

            // Check whether requirements are satisfied
            string sFailMessage;
            if (!pPositionalDesc->DoesValueSatisfy(sArg, sFailMessage, true))
            {
                return false;
            }

            pPositionalDesc->mbFound = true;

            switch (pPositionalDesc->mValueType)
            {
                case ParamDesc::kBool:
                    *((bool*)pPositionalDesc->mpValue) = SH::ToBool(sArg);    // set the registered bool
                break;
                case ParamDesc::kInt64:
                    *((int64_t*)pPositionalDesc->mpValue) =SH::ToInt(sArg);
                break;
                case ParamDesc::kFloat:
                    *((float*)pPositionalDesc->mpValue) = (float)SH::ToDouble(sArg);
                break;
                default:    // ParamDesc::kString
                    *((string*)pPositionalDesc->mpValue) = sArg;     // set the registered string
            }
            return true;
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


    void CommandLineParser::GetCommandLineExample(const std::string& sMode, string& sCommandLineExample)
    {
        sCommandLineExample = appName;

        string sParamsExample;
        for (auto& desc : mGeneralCommandLineParser.mParameterDescriptors)
        {
            if (desc.IsPositional() && desc.IsRequired())
                sParamsExample += " " + desc.msName;
        }
        for (auto& desc : mGeneralCommandLineParser.mParameterDescriptors)
        {
            if (desc.IsNamed() && desc.IsRequired())
                sParamsExample += " " + desc.msName;
        }

        if (IsMultiMode() && IsRegisteredMode(sMode))
        {
            sCommandLineExample += " " + sMode + sParamsExample;
            for (auto& desc : mModeToCommandLineParser[sMode].mParameterDescriptors)
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
        else if (IsMultiMode())
            sCommandLineExample += " COMMAND" + sParamsExample;
        else
            sCommandLineExample += sParamsExample;
    }


    void CLModeParser::GetModeUsageTables(string sMode, TableOutput& modeDescriptionTable, TableOutput& requiredParamTable, TableOutput& optionalParamTable, TableOutput& additionalInfoTable)
    {
        SH::makelower(sMode);

        if (!sMode.empty() && !msModeDescription.empty())
        {
            modeDescriptionTable.AddRow(string("Help for command: ") + cols[kAPP] + sMode.c_str() + cols[kRESET]);


            modeDescriptionTable.AddRow(" ");
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

    CommandLineParser::CommandLineParser(bool bEnableVerbosity, bool bEnableColoredOutput)
    {
#ifdef _WIN64
        // enable color output on windows
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

        DWORD mode = 0;
        GetConsoleMode(hConsole, &mode);
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

        SetConsoleMode(hConsole, mode);
#endif
        if (bEnableColoredOutput)
            ResetCols();

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
        if (sMode.empty())
            return false;

        SH::makelower(sMode);
        for (const auto& pair : mModeToCommandLineParser)
        {
            if (pair.first == sMode)
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
            cerr << cols[kERROR] << "Mode already registered:" << sMode << "\n" << cols[kRESET];
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
            cerr << cols[kERROR] << "Unregistered mode:" << sMode << "\n" << cols[kRESET];
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
            cerr << cols[kERROR] << "Unregistered mode:" << sMode << "\n" << cols[kRESET];
            return false;
        }
        return mModeToCommandLineParser[sMode].AddInfo(sInfo);
    }


    CommandLineParser::eResponse CommandLineParser::TryParse(const tStringArray& params)    // params without app name
    {
        msMode.clear();

        // Handle help
        // 
        // 1) Multi Mode no context
        //      list commands
        // 2) Multi Mode with context
        //      a) registered mode  -> show mode specific help
        //      b) unknown mode     -> "use help message"
        // 3) Single Mode
        //      Show general help

#ifdef ENABLE_CLE
        bool bShowEdit = ContainsArgument("!", params);
        if (bShowEdit)
            return kErrorShowEdit;
#endif        
        bool bMultiMode = !mModeToCommandLineParser.empty();

        if (params.empty())
        {
            if (bMultiMode)
                return kShowAvailableModes;

            if (!bMultiMode && mGeneralCommandLineParser.GetRequiredParameterCount() > 0)
                return kShowHelp;
        }

        string sMode = GetFirstPositionalArgument(params); // mode
        if (bMultiMode && !IsRegisteredMode(sMode))
            return kShowAvailableModes;


        bool bShowHelp = ContainsArgument("?", params);
        bool bDetailedHelp = ContainsArgument("??", params);



        if (bShowHelp || bDetailedHelp)
            return kShowHelp;


        int nErrors = 0;

        if (params.size() > 0)
        {
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
                for (int i = 1; i < params.size(); i++)
                {
                    std::string sParam(params[i]);

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
                        cerr << cols[kERROR] << "Error:" + cols[kRESET] + " Unknown parameter '" << cols[kERROR] << sParam << cols[kRESET] << "'\n";
                        nErrors++;
                    }
                }

                if (nErrors > 0)
                    return kErrorAbort;

                if (nErrors > 0 || !mModeToCommandLineParser[msMode].CheckAllRequirementsMet() || !mGeneralCommandLineParser.CheckAllRequirementsMet())
                {
//                    mModeToCommandLineParser[msMode].ShowFoundParameters();
//                    mGeneralCommandLineParser.ShowFoundParameters();

//                    cout << "\n\"" << cols[kERROR] << "Error:" << appName << " " << cols[kPARAM] << msMode << " -?" << cols[kRESET] << "\" - to see usage.\n";
                    return kErrorAbort;
                }

                return kSuccess;
            }
            else
            {
                // Case 2
                for (int i = 0; i < params.size(); i++)
                {
                    std::string sParam(params[i]);

                    if (mGeneralCommandLineParser.CanHandleArgument(sParam))
                    {
                        if (!mGeneralCommandLineParser.HandleArgument(sParam))
                            nErrors++;
                    }
                    else
                    {
                        cerr << cols[kERROR] << "Error: Unknown parameter '" << sParam << "'\n" << cols[kRESET];
                        nErrors++;
                    }
                }

                if (nErrors > 0 || !mGeneralCommandLineParser.CheckAllRequirementsMet())
                {
//                    mGeneralCommandLineParser.ShowFoundParameters();

//                    cout << "\n\"" << cols[kERROR] << "Error:" << appName << " " << cols[kPARAM] << " -?" << cols[kRESET] << "\" - to see usage.\n";
                    return kErrorAbort;
                }

                return kSuccess;
            }
        }

        // no parameters, and none required
        return kSuccess;
    }


    void string_to_argc_argv(const std::string& input, int& argc, std::vector<std::string>& argv) 
    {
        std::istringstream iss(input);
        std::string token;

        argc = 0;
        argv.clear();
        while (iss >> token) 
        {
            argv.push_back(token);
            argc++;
        }
    }

    tStringArray CommandLineParser::ToArray(int argc, char* argv[])
    {
        tStringArray list;
        list.reserve(argc);
        for (int i = 0; i < argc; i++)
        {
            string sParam(argv[i]);
            list.emplace_back(sParam);
        }
        return list;
    }

    tStringArray CommandLineParser::ToArray(const std::string& sCommandLine)
    {
        tStringArray list;

        size_t length = sCommandLine.length();
        for (size_t i = 0; i < sCommandLine.length(); i++)
        {
            // find start of param
            while (isblank(sCommandLine[i]) && i < length) // skip whitespace
                i++;

            size_t endofparam = i;
            // find end of param
            while (!isblank(sCommandLine[endofparam]) && endofparam < length)
            {
                // if this is an enclosing
                size_t match = SH::FindMatching(sCommandLine, endofparam);
                if (match != string::npos) // if enclosure, skip to endYour location
                {
                    endofparam = match + 1;
                    break;
                }
                else
                    endofparam++;
            }

            // strip surrounding quotes if necessary
            string sParam(sCommandLine.substr(i, endofparam - i));
            list.emplace_back(StripEnclosure(sParam));
            i = endofparam;
        }

        return list;
    }

    std::string CommandLineParser::EncloseWhitespaces(const std::string& value)
    {
        if (SH::ContainsWhitespace(value, true))
            return "\"" + value + "\"";

        return value;
    }

    std::string CommandLineParser::StripEnclosure(const std::string& value)
    {
        size_t len = value.length();
        if (len >= 2)
        {
            if ((value[0] == '\"' && value[len - 1] == '\"') ||
                (value[0] == '\'' && value[len - 1] == '\''))
            {
                return value.substr(1, len - 2);
            }
        }

        return value;
    }


    std::string CommandLineParser::ToString(const tStringArray& stringArray)
    {
        if (stringArray.empty())
            return "";

        string s;
        for (const auto& entry : stringArray)
        {
            s += EncloseWhitespaces(entry) + " ";
        }

        return s.substr(0, s.length() - 1); // strip last ' '
    }

    bool CommandLineParser::Parse(int argc, char* argv[], bool bEditOnParseFail)
    {
#ifdef ENABLE_CLE
        CLP::CommandLineEditor editor;
#endif

        appPath = argv[0];
#ifdef _WIN64
        // Ensure the appPath extension includes ".exe" since it can be launched without
        if (appPath.length() > 4)
        {
            // Ensure the appPath extension includes ".exe" since it can be launched without
            string sExtension(appPath.substr(appPath.length() - 4));
            std::transform(sExtension.begin(), sExtension.end(), sExtension.begin(), [](unsigned char c) { return (unsigned char)std::tolower(c); });
            if (sExtension != ".exe")
                appPath += ".exe";
        }
#endif
        // Extract  application name
        int32_t nLastSlash = (int32_t)appPath.find_last_of('/');
        int32_t nLastBackSlash = (int32_t)appPath.find_last_of('\\');
        nLastSlash = (nLastSlash > nLastBackSlash) ? nLastSlash : nLastBackSlash;

        if (nLastSlash != string::npos)
        {
            appName = appPath.substr(nLastSlash + 1);
            appPath = appPath.substr(0, nLastBackSlash);
        }
        else
            appName = appPath;



        tStringArray argArray(ToArray(argc-1, argv+1));
        bool bSuccess = false;
        while (1)
        {
            CommandLineParser::eResponse response = TryParse(argArray);

            if (response == kSuccess)
            {
                bSuccess = true;
                break;
            }
            else if (response == kCanceled)
            {
                cout << "Canceled\n";
                bSuccess = false;
                break;
            }
            else if (response == kErrorAbort)
            {
                mModeToCommandLineParser[msMode].ShowFoundParameters();
                mGeneralCommandLineParser.ShowFoundParameters();

                string sCommandLineExample;
                TableOutput usageTable;
                GetCommandLineExample(msMode, sCommandLineExample);
                usageTable.AddRow(cols[kSECTION] + "--------Usage---------" + cols[kRESET]);
                usageTable.AddRow(sCommandLineExample);
                cout << usageTable;


//                cout << "\n\"" << cols[kAPP] << appName << " " << cols[kPARAM] << msMode << " ?" << cols[kRESET] << "\" - to see usage.\n";
                bSuccess = false;
                break;
            }
            else if (response == kShowAvailableModes)
            {
                bool bHelp = ContainsArgument("?", argArray);
                bool bDetailedHelp = ContainsArgument("??", argArray);
//                cout << GetHelpString(GetFirstPositionalArgument(argArray), bDetailedHelp);
                if (bHelp|bDetailedHelp)
                {
                    TableOutput clpHelp = GetCLPHelp(bDetailedHelp);
                    TableOutput keyTable = GetKeyTable();
                    if (bDetailedHelp)
                    {
                        clpHelp.AlignWidth(80, clpHelp, keyTable);
                        cout << clpHelp << keyTable;
                    }
                    else
                        cout << clpHelp;
                }
                else
                {
                    cout << GetGeneralHelpString();
                }
                bSuccess = false;
                break;
            }
            else if (response == kShowHelp)
            {
                bool bDetailedHelp = ContainsArgument("??", argArray);
                cout << GetModeHelpString(GetFirstPositionalArgument(argArray), bDetailedHelp);
                bSuccess = false;
                break;
            }
#ifdef ENABLE_CLE
            else if (response == kErrorShowEdit)
            {
                editor.SetConfiguredCLP(this);

                for (tStringArray::iterator it = argArray.begin(); it != argArray.end(); it++)
                {
                    if ((*it) == "!")
                    {
                        argArray.erase(it);
                        break;
                    }
                }
                

                string result = editor.Edit(ToString(argArray));
                if (result.empty())
                {
                    bSuccess = false;
                    break;
                }
                argArray = ToArray(result);
            }
#endif
        }

//        PAUSE_FOR_KEY
        return bSuccess;
    }

    bool CommandLineParser::ContainsArgument(std::string sArgument, const tStringArray& params, bool bCaseSensitive)
    {
        for (const auto& param : params)
            if (SH::Compare(param, sArgument, bCaseSensitive))
                return true;

        return false;
    }

    std::string CommandLineParser::GetFirstPositionalArgument(const tStringArray& params)
    {
        for (int i = 0; i < params.size(); i++)
            if (params[i][0] != '-')
                return params[i];

        return "";
    }


    string CommandLineParser::GetModeHelpString(const std::string& sMode, bool bDetailed)
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

        if (IsRegisteredMode(sMode))
        {
            bHasRequiredParameters = mGeneralCommandLineParser.GetRequiredParameterCount() + mModeToCommandLineParser[sMode].GetRequiredParameterCount() > 0;
            bHasOptionalParameters = mGeneralCommandLineParser.GetOptionalParameterCount() + mModeToCommandLineParser[sMode].GetOptionalParameterCount() > 0;
            bHasAdditionalInfo = mGeneralCommandLineParser.mAdditionalInfo.size() + mModeToCommandLineParser[sMode].mAdditionalInfo.size() > 0;

            if (bHasAdditionalInfo)
            {
                additionalInfoTable.AddRow(" ");
                additionalInfoTable.AddRow(cols[kSECTION] + "---Additional Info----" + cols[kRESET]);

                additionalInfoTable.SetSeparator(' ', 1);
                additionalInfoTable.SetBorders(0, '-', '*', '*');
            }

            if (bHasRequiredParameters)
            {
                requiredParamTable.AddRow(" ");
                requiredParamTable.AddRow(cols[kSECTION] + "-------Required------", "---Type---", "---Default---", "---Description---" + cols[kRESET]);
                requiredParamTable.SetBorders(0, 0, '*', '*');
                requiredParamTable.SetSeparator(' ', 1);
            }

            if (bHasOptionalParameters)
            {
                optionalParamTable.AddRow(" ");
                optionalParamTable.AddRow(cols[kSECTION] + "------[Options]------", "---Type---", "---Default---", "---Description---" + cols[kRESET]);
                optionalParamTable.SetBorders(0, 0, '*', '*');
                optionalParamTable.SetSeparator(' ', 1);
            }


            GetCommandLineExample(sMode, sCommandLineExample);
            mModeToCommandLineParser[sMode].GetModeUsageTables(sMode, descriptionTable, requiredParamTable, optionalParamTable, additionalInfoTable);
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
                additionalInfoTable.AddRow(cols[kSECTION] + "---Additional Info----" + cols[kRESET]);

                additionalInfoTable.SetSeparator(' ', 1);
                additionalInfoTable.SetBorders(0, '-', '*', '*');
            }

            if (bHasRequiredParameters)
            {
                requiredParamTable.AddRow(" ");
                requiredParamTable.AddRow(cols[kSECTION] + "-------Required------", "---Type---", "---Default---", "---Description---" + cols[kRESET]);
                requiredParamTable.SetBorders(0, 0, '*', '*');
                requiredParamTable.SetSeparator(' ', 1);
            }

            if (bHasOptionalParameters)
            {
                optionalParamTable.AddRow(" ");
                optionalParamTable.AddRow(cols[kSECTION] + "------[Options]------", "---Type---", "---Default---", "---Description---" + cols[kRESET]);
                optionalParamTable.SetBorders(0, 0, '*', '*');
                optionalParamTable.SetSeparator(' ', 1);
            }

            GetCommandLineExample("", sCommandLineExample);
            mGeneralCommandLineParser.GetModeUsageTables("", descriptionTable, requiredParamTable, optionalParamTable, additionalInfoTable);
        }

        if (bHasOptionalParameters)
            sCommandLineExample += cols[kPARAM] + " [Options]" + cols[kRESET];

        usageTable.AddRow(cols[kSECTION] + "--------Usage---------" + cols[kRESET]);
        usageTable.AddRow(sCommandLineExample);
        usageTable.SetSeparator(' ', 1);
        usageTable.SetBorders('-', '*', '*', '*');


        TableOutput GeneralHelpTable = GetCLPHelp(bDetailed);


        size_t nMinWidth = 80;
#ifdef _WIN64
        CONSOLE_SCREEN_BUFFER_INFO screenInfo;
        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &screenInfo))
            nMinWidth = screenInfo.dwSize.X-1;
#endif


/*        size_t nMinTableWidth = std::max({ (size_t)nMinWidth,
                                            descriptionTable.GetTableWidth(), 
                                            requiredParamTable.GetTableWidth(), 
                                            optionalParamTable.GetTableWidth(), 
                                            usageTable.GetTableWidth(), 
                                            GeneralHelpTable.GetTableWidth(),
                                            additionalInfoTable.GetTableWidth()});

        GeneralHelpTable.SetMinimumOutputWidth(nMinTableWidth);
        requiredParamTable.SetMinimumOutputWidth(nMinTableWidth);
        optionalParamTable.SetMinimumOutputWidth(nMinTableWidth);
        descriptionTable.SetMinimumOutputWidth(nMinTableWidth);
        additionalInfoTable.SetMinimumOutputWidth(nMinTableWidth);
        usageTable.SetMinimumOutputWidth(nMinTableWidth);*/

        GeneralHelpTable.AlignWidth(nMinWidth, GeneralHelpTable, requiredParamTable, optionalParamTable, descriptionTable, additionalInfoTable, usageTable);


        // Now default/global
        stringstream ss;

        ss << descriptionTable;
        if (bHasAdditionalInfo)
            ss << additionalInfoTable;
        if (bHasRequiredParameters)
            ss << requiredParamTable;
        if (bHasOptionalParameters)   // 1 because "Optional:" added above
            ss << optionalParamTable;
        if (bDetailed)
            ss << GeneralHelpTable;
        ss << usageTable;

        return ss.str();
    }

    TableOutput CommandLineParser::GetCLPHelp(bool bDetailed)
    {
        TableOutput descriptionTable;
        descriptionTable.SetSeparator(' ', 1);
        descriptionTable.SetBorders('*', '*', '*', '*');

        descriptionTable.AddRow(cols[kSECTION] + CLP::appName + cols[kRESET]);
        descriptionTable.AddRow(" ");

        descriptionTable.AddRow(cols[kSECTION] + "----GENERAL HELP----" + cols[kRESET]);
        descriptionTable.AddRow("Generally the application is given a COMMAND followed by parameters.");
        descriptionTable.AddRow("Run the application with no parameters to get a list of available commands.");
        descriptionTable.AddRow("Parameters can be positional (1st, 2nd, 3rd, etc.) or named key:value pairs.");
        descriptionTable.AddRow("Parameters can be required or optional");
        descriptionTable.AddRow("You can get general help by adding '?' or '??' anywhere on the command line.");

        return descriptionTable;
    }

    TableOutput CommandLineParser::GetCommandsTable()
    {
        TableOutput commandsTable;
        commandsTable.SetBorders(0, 0, '*', '*');
        commandsTable.SetSeparator(' ', 1);

        commandsTable.AddRow(cols[kSECTION] + "-----Commands-----" + cols[kRESET]);
        for (tModeToParser::iterator it = mModeToCommandLineParser.begin(); it != mModeToCommandLineParser.end(); it++)
        {
            string sCommand = (*it).first;
            string sModeDescription = ((*it).second).GetModeDescription();
            if (!sCommand.empty())
                commandsTable.AddRow(cols[kPARAM] + sCommand + cols[kRESET], sModeDescription);
        }
        return commandsTable;
    }


    TableOutput CommandLineParser::GetKeyTable()
    {
        TableOutput table;
        table.SetSeparator(' ', 1);
        table.SetBorders('*', '*', '*', '*');

        table.AddRow(cols[kSECTION] + "--------KEYS---------" + cols[kRESET]);
        table.AddRow(cols[kPARAM] + "         [] " + cols[kRESET], "Optional" );
        table.AddRow(cols[kPARAM] + "          - " + cols[kRESET], "Named '-key:value' pair. (examples: -size:1KB  -verbose:3)");
        table.AddRow(cols[kPARAM] + "            " + cols[kRESET], "Can be anywhere on command line, in any order.");
        table.AddRow(cols[kPARAM] + "          # " + cols[kRESET], "NUMBER");
        table.AddRow(cols[kPARAM] + "            " + cols[kRESET], "Can be hex (0x05) or decimal or floating point.");
        table.AddRow(cols[kPARAM] + "            " + cols[kRESET], "Can include commas (1,000)");
        table.AddRow(cols[kPARAM] + "            " + cols[kRESET], "Can include scale labels (10k, 64KiB, etc.)");
        table.AddRow(cols[kPARAM] + "        BOOL" + cols[kRESET], "Boolean value can be 1/0, t/f, y/n, yes/no. Presence of the flag means true.");

        return table;
    }

    string CommandLineParser::GetModeDescription(const std::string& sMode)
    {
        // if no mode specified or if no modes registered, default to general
        if (sMode.empty() || mModeToCommandLineParser.empty())
            return mGeneralCommandLineParser.GetModeDescription();

        if (!IsRegisteredMode(sMode))
            return "";

        return mModeToCommandLineParser[sMode].GetModeDescription();
    }


    string CommandLineParser::GetGeneralHelpString()
    {
        // First output Application name

        TableOutput descriptionTable;
        descriptionTable.SetBorders('*', 0, '*', '*');
        descriptionTable.SetSeparator(' ', 1);

        descriptionTable.AddRow("Application: " + cols[kAPP] + appName + cols[kRESET]);

        descriptionTable.AddRow(" ");
        descriptionTable.AddRow(cols[kSECTION] + "--App Description--" + cols[kRESET]);
        descriptionTable.AddMultilineRow(msAppDescription);
        descriptionTable.AddRow(" ");

        TableOutput commandsTable;

        descriptionTable.AddRow(cols[kSECTION] + "------Usage-------" + cols[kRESET]);

        if (IsMultiMode())
        {

            string sUsageExample(cols[kAPP] + appName + cols[kPARAM] + " COMMAND PARAMS" + cols[kRESET]);

            descriptionTable.AddRow(sUsageExample);
            descriptionTable.AddRow(' ');

            commandsTable = GetCommandsTable();
            commandsTable.AddRow(" ");
        }
        else
        {
            string sUsageExample(cols[kAPP] + appName + cols[kPARAM] + " PARAMS" + cols[kRESET]);
            descriptionTable.AddRow(sUsageExample);
            descriptionTable.AddRow(' ');
        }

        TableOutput helpTable;
        helpTable.SetBorders(0, '*', '*', '*');
        helpTable.SetSeparator(' ', 1);
        helpTable.AddRow(cols[kSECTION] + "-------Help-------" + cols[kRESET]);
        helpTable.AddRow("Add '?' or '??' anywhere on command line for command help.");

        string sExampleCommand;
        if (IsMultiMode())
            sExampleCommand = (*mModeToCommandLineParser.begin()).first;
        helpTable.AddRow("For example: " + cols[kAPP] + appName + " " + cols[kPARAM] + sExampleCommand + " ?" + cols[kRESET]);
#ifdef ENABLE_CLE
        helpTable.AddRow(" ");
        helpTable.AddRow("Add '!' anywhere on command line for interactive command line editing.");
#endif


        size_t nMinWidth = 80;
#ifdef _WIN64
        CONSOLE_SCREEN_BUFFER_INFO screenInfo;
        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &screenInfo))
            nMinWidth = screenInfo.dwSize.X-1;
#endif

        descriptionTable.AlignWidth(nMinWidth, descriptionTable, commandsTable, helpTable);

        stringstream ss;

        ss << descriptionTable;
        ss << commandsTable;
        ss << helpTable;

        return ss.str();
    }

    std::string     cols[kMAX_CATEGORIES] =
    {
        COL_RESET,      // RESET    default
        COL_YELLOW,     // APP      default
        COL_CYAN,       // SECTION  default
        COL_YELLOW,     // PARAM    default
        COL_RED,        // ERROR    default
    };


}; // namespace CLP
