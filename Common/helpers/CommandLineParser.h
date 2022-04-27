#pragma once
#include <string>
#include <map>
#include <vector>
#include <iostream>

using namespace std;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CommandlineParser 
// Made to simplify command line parsing.
// 
// > Handles required and optional parameters
// > Is able to check that numbers fall within specified ranges
// > Shows descriptive error messages if any required parameters are missing, unrecognized or are outside of acceptible ranges
// > Can parse human readable number input such as 12MiB, 1KB, 2GB, etc.
// 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Details:
// Positional parameters have to be in order, but not necessarily together
// > Example: "copy c:\sample.txt c:\sample2.txt"
// 
// Named parameters start with '-' and map a key to a value separated by a ':'
// simple flags such as "-run" is treated as a boolean.
// > Example: "-threads:4"  "-verbose" 
//
// Named parameters can be anywhere on the command line, in any order, including before, between or after positional parameters
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// > Examples:
// app.exe compare file1.txt file2.txt -nocase -width:16 -P -blocksize:128KiB
// is the same as
// app.exe -blocksize:128KiB -width:16 -nocase compare file1.txt -P file2.txt
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Usage:
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

    class CommandLineParser
    {
    public:
        CommandLineParser();
        ~CommandLineParser();

        // Registration Functions
        void    RegisterAppDescription(const string& sDescription);
        bool    RegisterParam(ParamDesc param);

        bool    Parse(int argc, char* argv[], bool bVerbose = false);
        string  GetAppPath() { return msAppPath; }
        string  GetAppName() { return msAppName; }
        bool    GetParamWasFound(const string& sKey);   // returns true if the parameter was found when parsing
        bool    GetParamWasFound(int64_t nIndex);       // returns true if the parameter was found when parsing

        void    OutputUsage();  // dynamically built from registered parameters


    protected:
        bool    GetDescriptor(const string& sKey, ParamDesc** pDescriptorOut);
        bool    GetDescriptor(int64_t nIndex, ParamDesc** pDescriptorOut);

        string              msAppPath;          // full path to the app.exe
        string              msAppName;          // just the app.exe
        string              msAppDescription;

        // Positional parameters
        int64_t             mnRegisteredPositional;
        vector<ParamDesc>   mParameterDescriptors;

    protected:
        // helper funcitons for neat formatting
        void        AddSpacesToEnsureWidth(string& sText, int64_t nWidth);
        int32_t     LongestLine(const string& sText);
        void        OutputFixed(int32_t nWidth, const char* format, ...);
        void        OutputLines(int32_t nWidth, const string& sMultiLineString);
        void        RepeatOut(char c, int32_t nCount, bool bNewLine = false);
    };
};  // namespace CLP
