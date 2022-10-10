#pragma once
#include <string>
#include <map>
#include <vector>
#include <iostream>
#include "LoggingHelpers.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CommandlineParser 
// Made to simplify command line parsing. 
// Can manage "single mode" command line 
// > Example: "app.exe file1.txt"
// 
// Can manage "multi mode" command line where first parameter is a command with different parameter schemes
// > Example: "app.exe print file1.txt"
//            "app.exe concat file1.txt file2.txt -verbose"
// 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Additional Details:
// > Handles required and optional parameters
// > Is able to check that numbers fall within specified ranges
// > Shows descriptive error messages if any required parameters are missing, unrecognized or are outside of acceptible ranges
// > Can parse human readable number input such as 12MiB, 1KB, 2GB, etc.
//
// Named parameters start with '-' and map a key to a value separated by a ':'
// simple flags such as "-run" is treated as a boolean.
// > Example: "-threads:4"  "-verbose" 
//
// Named parameters can be anywhere on the command line, in any order, including before, between or after positional parameters
// 
// Positional parameters have to be in order, but not necessarily together (note the "-validate" named parameter in the middle)
// > Example: "app.exe c:\sample.txt -validate c:\sample2.txt"
// 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Single Mode Example:
// 1) Instantiate an instance of CommandLineParser
// 2) Provide an optional description for what this application does.
// 3) Register your variables that will receive the results of parsing
// 4) Parse
//
// Usage Example:
// 
//   string  sourcePath;
//   string  destPath;
//   string  searchString;
//   int64_t nThreads;
//   int64_t nBlockSize;
//   bool    bCaseSensitive;
// 
//   CommandLineParser parser;
//
//   parser.RegisterParam(ParamDesc("SOURCE",       &sourcePath,        ParamDesc::kPositional | ParamDesc::kRequired, "File/folder to index by blocks."));
//   parser.RegisterParam(ParamDesc("DESTINATION",  &destPath,          ParamDesc::kPositional | ParamDesc::kRequired, "File/folder to scan at byte granularity."));
//   parser.RegisterParam(ParamDesc("threads",      &nThreads,          ParamDesc::kNamed | ParamDesc::kOptional | ParamDesc::kRangeRestricted, "Number of threads to spawn.", 1, 256));
//   parser.RegisterParam(ParamDesc("blocksize",    &nBlockSize,        ParamDesc::kNamed | ParamDesc::kOptional | ParamDesc::kRangeRestricted, "Granularity of blocks to use for scanning.", 16, 32*1024*1024));
//   parser.RegisterParam(ParamDesc("case",         &bCaseSensitive,    ParamDesc::kNamed | ParamDesc::kOptional));
//
//   parser.RegisterAppDescription("Searches the source file for lines that include searchString and copies those lines to the destination file.");
//   if (!parser.Parse(argc, argv))
//      return false;
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Multi Mode Example:
// 1) Instantiate an instance of CommandLineParser
// 2) Provide an optional description for what this application does.
// 3) Register your modes
// 4) Register variables that will receive the results of parsing for each mode
// 5) Parse
//
// Usage Example:
// 
//   string  path1;
//   string  path2;
//   bool bVerbose;
// 
//   CommandLineParser parser;
//
//   // print mode
//   parser.RegisterMode("print");
//   parser.RegisterParam("print", ParamDesc("PATH",       &path1,        ParamDesc::kPositional | ParamDesc::kRequired, "Prints the specified file."));
//
//   // concat mode
//   parser.RegisterMode("concat");
//   parser.RegisterParam("concat", ParamDesc("PATH1",    &path1,     ParamDesc::kPositional | ParamDesc::kRequired, "First file"));
//   parser.RegisterParam("concat", ParamDesc("PATH2",    &path2,     ParamDesc::kPositional | ParamDesc::kRequired, "File to append to first file."));
// 
//   // common optional parameter
//   parser.RegisterParam(ParamDesc("verbose",  &bVerbose,  ParamDesc::kNamed | ParamDesc::kOptional));
//
//   parser.RegisterAppDescription("This app prints and concatenates files for some reason.");
//   if (!parser.Parse(argc, argv))
//      return false;


namespace CLP
{
    // behavior flags
    #define eBehavior uint32_t

    const static uint32_t kPositional           = 0;    // default
    const static uint32_t kNamed                = 1;    // if set the parameter is named (example -name:value)

    const static uint32_t kOptional             = 0;    // default
    const static uint32_t kRequired             = 2;    // if set the parameter is required or the parser shows an error

    const static uint32_t kRangeUnrestricted    = 0;    // default
    const static uint32_t kRangeRestricted      = 4;    // if set the integer values must fall between some min and max

    const static uint32_t kCaseInsensitive      = 0;    // default
    const static uint32_t kCaseSensitive        = 8;    // if set the named key must match case 


    class ParamDesc
    {
    public:
        friend class CLModeParser;
        friend class CommandLineParser;

        // named string
        ParamDesc(const std::string& sName, std::string* pString, eBehavior behavior, const std::string& sUsage = "");
        ParamDesc(const std::string& sName, bool* pBool, eBehavior behavior, const std::string& sUsage = "");
        ParamDesc(const std::string& sName, int64_t* pInt, eBehavior behavior, const std::string& sUsage = "", int64_t nRangeMin = 0, int64_t nRangeMax = 0);

    private:

        void        GetExample(std::string& sParameter, std::string& sType, std::string& sDefault, std::string& sUsage);

        // Behavior Accessors
        bool        IsNamed()               const { return mBehaviorFlags & kNamed; }
        bool        IsPositional()          const { return !IsNamed(); }

        bool        IsRequired()            const { return mBehaviorFlags & kRequired; }
        bool        IsOptional()            const { return !IsRequired(); }

        bool        IsRangeRestricted()     const { return mBehaviorFlags & kRangeRestricted; }
        bool        IsRangeUnrestricted()   const { return !IsRangeRestricted(); }

        bool        IsCaseSensitive()       const { return mBehaviorFlags & kCaseSensitive; }
        bool        IsCaseInsensitive()     const { return !IsCaseSensitive(); }

        enum eParamValueType
        {
            // kValue Types
            kUnknown    = 0,
            kInt64      = 1,
            kBool       = 2,
            kString     = 3
        };
        eParamValueType mValueType;
        void*           mpValue;        // Memory location of value to be filled in

        std::string     msName;         // For named parameters like "-catlives:9" or "-VERBOSE".  Also used for help text for all parameters
        int64_t         mnPosition;     // positional parameter index.  For unnamed (i.e. positional) parameters
        eBehavior       mBehaviorFlags;

        // range restricted parameters
        int64_t         mnMinValue;
        int64_t         mnMaxValue;

        // Parameter usage for help text
        std::string     msUsage;

        // Tracking for checking whether all required parameters were found
        bool            mbFound;

    };


    ///////////////////////////////////////////////////////////////////////////////////////////////////
    // Helper class used by CommandLineParser
    class CLModeParser
    {
        friend class CommandLineParser;

    public:
        // Registration Functions

        bool    RegisterParam(ParamDesc param);
        bool    AddInfo(const std::string& sInfo) { mAdditionalInfo.push_back(sInfo); return true; }

        bool    RegisterModeDescription(const std::string& sModeDescription) { msModeDescription = sModeDescription; return true; }

        std::string  GetModeDescription() { return msModeDescription; }

        size_t  GetOptionalParameterCount();
        size_t  GetRequiredParameterCount();

        bool    GetParamWasFound(const std::string& sKey);   // returns true if the parameter was found when parsing
        bool    CheckAllRequirementsMet();      // true if all registered parameters that are required were handled
        size_t  GetNumPositionalParamsRegistered();
        size_t  GetNumPositionalParamsHandled();

        void    GetModeUsageTables(std::string sMode, std::string& sCommandLineExample, TableOutput& modeDescriptionTable, TableOutput& requiredParamTable, TableOutput& optionalParamTable, TableOutput& additionalInfoTable);

    protected:
        bool    CanHandleArgument(const std::string& sArg); // returns true if the key for this argument is registered
        bool    HandleArgument(const std::string& sArg, bool bVerbose = false);    // returns false if there's an error

        bool    GetDescriptor(const std::string& sKey, ParamDesc** pDescriptorOut);
        bool    GetDescriptor(int64_t nIndex, ParamDesc** pDescriptorOut);

        std::string             msModeDescription;
        std::list<std::string>  mAdditionalInfo;

    private:
        // Positional parameters
        std::vector<ParamDesc>  mParameterDescriptors;
    };


    typedef  std::map<std::string, CLModeParser> tModeStringToParserMap;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // CommandLineParser 
    // For applications that have different primary commands, the parameters may have different requirements.
    // For example:
    // app.exe list PATH                                      <- "list" is primary command.... 1 positional PATH parameter
    // app.exe copy SOURCE_PATH DEST_PATH [-recursive]        <- "copy" is primary command.... 2 positional PATHs and one optional named parameter
    // app.exe -version                                       <- no primary command uses mDefaultCommandLineParser
    class CommandLineParser
    {
    public:
        // Registration Functions
        void    RegisterAppDescription(const std::string& sDescription);
        bool    Parse(int argc, char* argv[], bool bVerbose = false);
        void    ListModes();
        void    OutputHelp();

        // Accessors
        std::string  GetAppMode() { return msMode; }         // empty string if default mode
        std::string  GetAppPath() { return msAppPath; }
        std::string  GetAppName() { return msAppName; }
        bool    IsCurrentMode(std::string sMode);           // true if current mode matches (case insensitive)
        bool    IsRegisteredMode(std::string sMode);        // true if this mode has been registered

        bool    GetParamWasFound(const std::string& sKey);  // returns true if the parameter was found when parsing
        bool    GetParamWasFound(int64_t nIndex);           // returns true if the parameter was found when parsing

        size_t  GetOptionalParameterCount();
        size_t  GetRequiredParameterCount();


        // multi-mode behavior registration
        bool    RegisterMode(std::string sMode, const std::string& sModeDescription);
        bool    RegisterParam(std::string sMode, ParamDesc param);    // will return false if sMode hasn't been registered yet

        // default mode (i.e. no command specified)
        bool    RegisterParam(ParamDesc param);         // default mode parameter


        bool    AddInfo(const std::string& sInfo);

        // Additional info
        bool    AddInfo(std::string sMode, const std::string& sInfo);

    protected:
        std::string  msMode;
        std::string  msAppPath;                      // full path to the app.exe
        std::string  msAppName;                      // just the app.exe
        std::string  msAppDescription;

        bool         mbVerbose;
        CLModeParser   mGeneralCommandLineParser;      // if no registered modes, defaults to this one
        tModeStringToParserMap  mModeToCommandLineParser;

    };

    // Utility functions
    std::string UserReadableFromInt(int64_t nValue);
    int64_t     IntFromUserReadable(std::string sReadable);
    bool        StringToBool(std::string sValue);
    bool        StringCompare(const std::string& a, const std::string& b, bool bCaseSensitive = true);

};  // namespace CLP
