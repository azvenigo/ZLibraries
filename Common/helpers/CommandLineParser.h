#pragma once
#include <string>
#include <map>
#include <vector>
#include <set>
#include <iostream>
#include <optional>
#include "LoggingHelpers.h"
#include "StringHelpers.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CommandlineParser 
// Class to make command line parsing as simple (but flexible) as possible.
// 
// Simplest use case example: 
// (parses a filename from a commandline. If one isn't provided it shows that the parameter is missing and returns false.)
// 
//   string fileName;
//   CommandLineParser parser;
//   parser.RegisterParam(ParamDesc("FILENAME", &fileName, ParamDesc::kPositional | ParamDesc::kRequired, "This is a description of the parameter."));
//   if (!parser.Parse(argc, argv))
//      return false;
// 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Two modes of operation:
// 
// "single mode" command line 
// > Example: "app.exe file1.txt"
// 
// "multi mode" command line where first parameter is a command with different parameter schemes
// > Example: "app.exe print file1.txt"
//            "app.exe concat file1.txt file2.txt -verbose:2"
//            "app.exe list file1.txt -maxlines:3k"
// 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Additional Details:
// > Handles required and optional parameters. 
// > Is able to check that numbers fall within specified ranges
// > Shows descriptive error messages if any required parameters are missing, unrecognized or are outside of acceptible ranges
// > Can parse human readable number input such as 12MiB, 1KB, 2GB, etc.
//
// Named parameters start with '-' and map a key to a value separated by a ':'
// simple flags such as "-run" is treated as a boolean.
// > Example: "-threads:4"  "-verbose:1" 
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
//   int64_t nThreads;
//   int64_t nBlockSize;
//   bool    bCaseSensitive;
// 
//   CommandLineParser parser;
//
//   parser.RegisterParam(ParamDesc("SOURCE",       &sourcePath,        ParamDesc::kPositional | ParamDesc::kRequired, "File/folder to index by blocks."));
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
    extern std::string appPath;
    extern std::string appName;

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

    const static uint32_t kPath                 = 16;   // value should be a path
    const static uint32_t kExistingPath         = 32;   // if set, must be able to find an existing file/folder
    const static uint32_t kNoExistingPath       = 64;   // if set, must not have existing file/folder at this location


    // decorations
    const static uint32_t kRESET            = 0;
    const static uint32_t kAPP              = 1;
    const static uint32_t kSECTION          = 2;
    const static uint32_t kPARAM            = 3;
    const static uint32_t kERROR            = 4;

    const static uint32_t kMAX_CATEGORIES   = 5;

    // array of colors
    extern std::string     cols[kMAX_CATEGORIES];

    [[maybe_unused]] static void ResetCols()            // reset colored output
    {
        cols[kRESET]    = COL_RESET;
        cols[kAPP]      = COL_YELLOW;
        cols[kSECTION]  = COL_CYAN;
        cols[kPARAM]    = COL_YELLOW;
        cols[kERROR]    = COL_RED;
    };

    [[maybe_unused]] static void DisableCols()          // disable colored output
    {
        for (uint32_t i = 0; i < kMAX_CATEGORIES; i++)
            cols[i] = "";
    };


    class ParamDesc
    {
    public:
        friend class CLModeParser;
        friend class CommandLineParser;

#ifdef ENABLE_CLE
        friend class CommandLineEditor;
        friend class ParamListWin;
#endif

        // named string
        ParamDesc(const std::string& sName, std::string* pString, eBehavior behavior, const std::string& sUsage = "", const tStringSet& allowedStrings = {});
        ParamDesc(const std::string& sName, bool* pBool, eBehavior behavior, const std::string& sUsage = "");
        ParamDesc(const std::string& sName, int64_t* pInt, eBehavior behavior, const std::string& sUsage = "", std::optional<int64_t> nRangeMin = std::nullopt, std::optional<int64_t> nRangeMax = std::nullopt);
        ParamDesc(const std::string& sName, float* pFloat, eBehavior behavior, const std::string& sUsage = "", std::optional<float> fRangeMin = std::nullopt, std::optional<float> fRangeMax = std::nullopt);

        // Behavior Accessors
        bool        IsNamed()                       const { return mBehaviorFlags & kNamed; }
        bool        IsPositional()                  const { return !IsNamed(); }

        bool        IsRequired()                    const { return mBehaviorFlags & kRequired; }
        bool        IsOptional()                    const { return !IsRequired(); }

        bool        IsRangeRestricted()             const { return mBehaviorFlags & kRangeRestricted; }
        bool        IsRangeUnrestricted()           const { return !IsRangeRestricted(); }

        bool        IsCaseSensitive()               const { return mBehaviorFlags & kCaseSensitive; }
        bool        IsCaseInsensitive()             const { return !IsCaseSensitive(); }

        bool        IsAPath()                       const { return mBehaviorFlags & kPath; }
        bool        MustHaveAnExistingPath()        const { return mBehaviorFlags & kExistingPath; }
        bool        MustNotHaveAnExistingPath()     const { return mBehaviorFlags & kNoExistingPath; }

        bool        Satisfied();                                                                    // If value is required and set and (if appropriate) within required range
        bool        DoesValueSatisfy(const std::string& sValue, std::string& sFailMessage, bool bOutputError = false);        // converts string form of value into eParamValueType and returns whether it satisfies conditions

    private:

        void        GetExample(std::string& sParameter, std::string& sType, std::string& sDefault, std::string& sUsage);
        std::string ValueToString();

        enum eParamValueType
        {
            // kValue Types
            kUnknown    = 0,
            kInt64      = 1,
            kBool       = 2,
            kString     = 3,
            kFloat      = 4
        };

        eParamValueType             mValueType;
        void*                       mpValue;        // Memory location of value to be filled in

        std::string                 msName;         // For named parameters like "-catlives:9" or "-VERBOSE".  Also used for help text for all parameters
        int64_t                     mnPosition;     // positional parameter index.  For unnamed (i.e. positional) parameters
        eBehavior                   mBehaviorFlags;

        // range restricted parameters
        std::optional<int64_t>      mnMinInt;
        std::optional<int64_t>      mnMaxInt;
        std::optional<float>        mfMinFloat;
        std::optional<float>        mfMaxFloat;

        tStringSet                  mAllowedStrings;

        // Parameter usage for help text
        std::string                 msUsage;

        // Tracking for checking whether all required parameters were found
        bool                        mbFound;

    };


    ///////////////////////////////////////////////////////////////////////////////////////////////////
    // Helper class used by CommandLineParser
    class CLModeParser
    {
        friend class CommandLineParser;
#ifdef ENABLE_CLE
        friend class CommandLineEditor;
#endif

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

        void    ShowFoundParameters();
        void    GetModeUsageTables(std::string sMode, TableOutput& modeDescriptionTable, TableOutput& requiredParamTable, TableOutput& optionalParamTable, TableOutput& additionalInfoTable);

    protected:
        bool    CanHandleArgument(const std::string& sArg); // returns true if the key for this argument is registered
        bool    HandleArgument(const std::string& sArg);    // returns false if there's an error

        bool    GetDescriptor(const std::string& sKey, ParamDesc** pDescriptorOut);
        bool    GetDescriptor(int64_t nIndex, ParamDesc** pDescriptorOut);

        std::string             msModeDescription;
        std::list<std::string>  mAdditionalInfo;

    private:
        // Parameters
        std::vector<ParamDesc>  mParameterDescriptors;
    };


    typedef  std::map<std::string, CLModeParser> tModeToParser;

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
#ifdef ENABLE_CLE
        friend class CommandLineEditor;
#endif

        CommandLineParser(bool bEnableVerbosity = true, bool bEnableColoredOutput = true);

        // Registration Functions
        void            RegisterAppDescription(const std::string& sDescription);
        bool            Parse(int argc, char* argv[], bool bEditOnParseFail = true);
        TableOutput     GetCLPHelp(bool bDetailed = false);
        TableOutput     GetCommandsTable();
        TableOutput     GetKeyTable();
        std::string     GetGeneralHelpString();
        std::string     GetModeHelpString(const std::string& sMode = "", bool bDetailed = false);
        void            GetCommandLineExample(const std::string& sMode, std::string& sCommandLineExample);

        // Accessors
        std::string     GetAppMode() { return msMode; }         // empty string if default mode
        std::string     GetModeDescription(const std::string& sMode);

        bool            IsCurrentMode(std::string sMode);           // true if current mode matches (case insensitive)
        bool            IsMultiMode() const { return !mModeToCommandLineParser.empty(); }
        bool            IsRegisteredMode(std::string sMode);        // true if this mode has been registered

        bool            GetParamWasFound(const std::string& sKey);  // returns true if the parameter was found when parsing
        bool            GetParamWasFound(int64_t nIndex);           // returns true if the parameter was found when parsing

        size_t          GetOptionalParameterCount();
        size_t          GetRequiredParameterCount();


        // multi-mode behavior registration
        bool            RegisterMode(std::string sMode, const std::string& sModeDescription);
        bool            RegisterParam(std::string sMode, ParamDesc param);    // will return false if sMode hasn't been registered yet

        // default mode (i.e. no command specified)
        bool            RegisterParam(ParamDesc param);         // default mode parameter


        bool            AddInfo(const std::string& sInfo);

        // Additional info
        bool            AddInfo(std::string sMode, const std::string& sInfo);

        // utility functions
        static tStringArray     ToArray(int argc, char* argv[]);
        static tStringArray     ToArray(const std::string& sCommandLine);
        static std::string      ToString(const tStringArray& stringList);
        static std::string      EncloseWhitespaces(const std::string& value);     // if value contains whitespaces, surround with quotes
        static std::string      StripEnclosure(const std::string& value);         // remove surrounding quotes

    protected:

        enum eResponse : uint32_t
        {
            kSuccess            = 0,
            kCanceled           = 1,
            kShowAvailableModes = 2,
            kShowHelp           = 3,
            kErrorShowEdit      = 4,
            kErrorAbort         = 5
        };


        eResponse       TryParse(const tStringArray& params);    // params not including app exe in element 0

        bool            ContainsArgument(std::string sArgument, const tStringArray& params, bool bCaseSensitive = false);    // true if included argument is anywhere on the command line
        std::string     GetFirstPositionalArgument(const tStringArray& params);                                                 // first argument that's not a named. (named starts with '-')

        std::string     msMode;
//        std::string     msAppPath;                      // full path to the app.exe
//        std::string     msAppName;                      // just the app.exe
        std::string     msAppDescription;

        CLModeParser    mGeneralCommandLineParser;      // if no registered modes, defaults to this one

        tModeToParser   mModeToCommandLineParser;

    };
};  // namespace CLP
