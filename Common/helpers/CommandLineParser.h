#pragma once
#include <string>
#include <map>
#include <vector>
#include <iostream>

using namespace std;

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
//   parser.RegisterMode("print");
//   parser.RegisterParam("print", ParamDesc("PATH",       &path1,        ParamDesc::kPositional | ParamDesc::kRequired, "Prints the specified file."));
//
//   parser.RegisterMode("concat");
//   parser.RegisterParam(ParamDesc("PATH1",    &path1,     ParamDesc::kPositional | ParamDesc::kRequired, "First file"));
//   parser.RegisterParam(ParamDesc("PATH2",    &path2,     ParamDesc::kPositional | ParamDesc::kRequired, "File to append to first file."));
//   parser.RegisterParam(ParamDesc("verbose",  &bVerbose,  ParamDesc::kNamed | ParamDesc::kOptional));
//
//   parser.RegisterAppDescription("Concatenates a file to the end of another file.");
//   if (!parser.Parse(argc, argv))
//      return false;


namespace CLP
{
    // behavior flags
    #define eBehavior uint32_t

    const static uint32_t kPositional = 0;    // default. Defined for readability
    const static uint32_t kNamed = 1;    // if not set, it's positional

    const static uint32_t kOptional = 0;    // default. Defined for readability
    const static uint32_t kRequired = 2;    // if not set, it's optional 

    const static uint32_t kRangeUnrestricted = 0;    // default. Defined for readability
    const static uint32_t kRangeRestricted = 4;    // if not set, no range restriction (default)



    class ParamDesc
    {
    public:
        friend class CLModeParser;
        friend class CommandLineParser;

        // named string
        ParamDesc(const string& sName, string* pString, eBehavior behavior, const string& sUsage = "");
        ParamDesc(const string& sName, bool* pBool, eBehavior behavior, const string& sUsage = "");
        ParamDesc(const string& sName, int64_t* pInt, eBehavior behavior, const string& sUsage = "", int64_t nRangeMin = 0, int64_t nRangeMax = 0);

    private:

        string      GetExampleString();

        // Behavior Accessors
        bool        IsNamed()               const { return mBehaviorFlags & kNamed; }
        bool        IsPositional()          const { return !IsNamed(); }

        bool        IsRequired()            const { return mBehaviorFlags & kRequired; }
        bool        IsOptional()            const { return !IsRequired(); }

        bool        IsRangeRestricted()     const { return mBehaviorFlags & kRangeRestricted; }
        bool        IsRangeUnrestricted()   const { return !IsRangeRestricted(); }

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

        string          msName;         // For named parameters like "-catlives:9" or "-VERBOSE".  Also used for help text for all parameters
        int64_t         mnPosition;     // positional parameter index.  For unnamed (i.e. positional) parameters
        eBehavior       mBehaviorFlags;

        // range restricted parameters
        int64_t         mnMinValue;
        int64_t         mnMaxValue;

        // Parameter usage for help text
        string          msUsage; 

        // Tracking for checking whether all required parameters were found
        bool            mbFound;

    };


    ///////////////////////////////////////////////////////////////////////////////////////////////////
    // Helper class used by CommandLineParser
    class CLModeParser
    {
    public:
        CLModeParser();
        ~CLModeParser();

        // Registration Functions

        bool    RegisterParam(ParamDesc param);

        bool    RegisterModeDescription(const string& sModeDescription) { msModeDescription = sModeDescription; return true; }

        bool    Parse(int argc, char* argv[], bool bVerbose = false);

        string  GetModeDescription() { return msModeDescription; }
        bool    GetParamWasFound(const string& sKey);   // returns true if the parameter was found when parsing
        bool    GetParamWasFound(int64_t nIndex);       // returns true if the parameter was found when parsing

        void    OutputModeUsage(const string& sAppName, const string& sMode = "");

    protected:
        bool    GetDescriptor(const string& sKey, ParamDesc** pDescriptorOut);
        bool    GetDescriptor(int64_t nIndex, ParamDesc** pDescriptorOut);

        string  msModeDescription;

    private:
        // Positional parameters
        int64_t             mnRegisteredPositional;
        vector<ParamDesc>   mParameterDescriptors;

    protected:

        bool        ParseParameter(std::string sParam, uint64_t& nRegisteredPositionalVal, vector<ParamDesc>& paramDescriptors, bool bVerbose = false);
    };


    typedef  map<string, CLModeParser> tModeStringToParserMap;

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
        void    RegisterAppDescription(const string& sDescription);
        bool    Parse(int argc, char* argv[], bool bVerbose = false);
        void    OutputUsage();
        void    OutputHelp();

        // Accessors
        string  GetAppMode() { return msMode; }         // empty string if default mode
        string  GetAppPath() { return msAppPath; }
        string  GetAppName() { return msAppName; }
        bool    GetParamWasFound(const string& sKey);   // returns true if the parameter was found when parsing
        bool    GetParamWasFound(int64_t nIndex);       // returns true if the parameter was found when parsing


        // multi-mode behavior registration
        bool    RegisterMode(const string& sMode, const string& sModeDescription);
        bool    RegisterParam(const string& sMode, ParamDesc param);    // will return false if sMode hasn't been registered yet

        // default mode (i.e. no command specified)
        bool    RegisterParam(ParamDesc param);         // default mode parameter

    protected:

        string  FindMode(const string& sArg);  // checks first parameter against registered modes. empty if none found

        string  msMode;
        string  msAppPath;                      // full path to the app.exe
        string  msAppName;                      // just the app.exe
        string  msAppDescription;

        CLModeParser   mDefaultCommandLineParser;      // if no registered modes, defaults to this one
        tModeStringToParserMap  mModeToCommandLineParser;
    };
};  // namespace CLP
