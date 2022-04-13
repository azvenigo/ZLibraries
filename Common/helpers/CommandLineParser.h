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
// 1) Instantiate an instance of ComandLineMap
// 2) Provide an optional description for what this application does.
// 3) Specify the required number of positional parameters
// 4) Register your variables that will receive the results of parsing
// 5) Parse
//
// Usage Example:
// 
//   string  sourcePath;
//   string  destPath;
//   string  searchString;
//   int64_t nThreads;
//   bool    bCaseSensitive;
// 
//   CommandLineParser parser;
//   parser.RegisterDescription("Searches the source file for lines that include searchString and copies those lines to the destination file.");
//   parser.SetRequiredNumberOfPositionalParameters(2);
//   parser.RegisterPositionalString("SOURCE", &sourcePath);
//   parser.RegisterPositionalString("DESTINATION", &destPath);
//   parser.RegisterPositionalString("SEARCH", &searchString);
//   parser.RegisterNamedInt64("threads", &nThreads, false, true, 1, 128);
//   parser.RegisterNamedBool("casesensitive", &bCaseSensitive);
//   
//   parser.Parse(argc, argv, false);


class ParamDesc
{
public:
    friend class CommandLineParser;

    enum eKeyType
    {
        kPositional = 1,
        kNamed = 2
    };

    enum eRequired
    {
        kOptional = 0,
        kRequired = 1
    };


    //ParameterDescriptor() : mnPosition(-1), mKeyType(eParamType::kUnknown), mValueType(eParamType::kUnknown), mbRequired(false), mpValue(nullptr), mbRangeRestricted(false), mnMinValue(0), mnMaxValue(0), mbFound(false) {}

    // named string
    ParamDesc(eKeyType type, eRequired bRequired, const string& sName, string* pString, const string& sUsage = "");
    ParamDesc(eKeyType type, eRequired bRequired, const string& sName, bool* pBool, const string& sUsage = "");
    ParamDesc(eKeyType type, eRequired bRequired, const string& sName, int64_t* pInt, bool bRangeRestricted = false, int64_t nMin = 0, int64_t nMax = 0, const string& sUsage = "");

    string      GetExampleString();

private:

    enum eInternalParamType
    {
        // kValue Types
        kUnknown = 0,
        kInt64 = 1,
        kBool = 2,
        kString = 3,


    };



    string      msName;         // named parameters like "-count:12" or "-verbose"
    int64_t     mnPosition;     // positional parameter index (0 is the app name, etc.)

    void*       mpValue;        // Memory location of value to be filled in

    eKeyType    mKeyType;  // Positional or named
    eInternalParamType  mValueType;
    bool        mbRequired;     // If param is required, parse will return false when missing

    bool        mbRangeRestricted;  // If restricted mnMinValue and mnMaxValue should be set
    int64_t     mnMinValue;
    int64_t     mnMaxValue;

    string      msUsage;        // Optional explanation for help text

    // Tracking
    bool        mbFound;   // For checking whether all required parameters were found

};

class CommandLineParser
{
public:
    CommandLineParser();
    ~CommandLineParser();

    bool    Parse(int argc, char* argv[], bool bVerbose=false);

    void    OutputUsage();  // dynamically built from registered parameters

    // Registration Functions
    void    RegisterAppDescription(const string& sDescription);

    bool    RegisterParam(ParamDesc param);

/*    bool    RegisterNamedString(const string& sKey, string* pRegisteredString, bool bRequired = false, const string& sUsage = "");
    bool    RegisterNamedInt64(const string& sKey, int64_t* pRegisteredInt64, bool bRequired = false, bool bRangeRestricted=false, int64_t nMinValue=0, int64_t nMaxValue=0, const string& sUsage = "");
    bool    RegisterNamedBool(const string& sKey, bool* pRegisteredBool, bool bRequired = false, const string& sUsage = "");

    bool    RegisterPositionalString(const string& sDisplayName, string* pRegisteredString, bool bRequired = false, const string& sUsage = "");
    bool    RegisterPositionalInt64(const string& sDisplayName, int64_t* pRegisteredInt64, bool bRequired = false, bool bRangeRestricted = false, int64_t nMinValue = 0, int64_t nMaxValue = 0, const string& sUsage = "");
    bool    RegisterPositionalBool(const string& sDisplayName, bool* pRegisteredBool, bool bRequired = false, const string& sUsage = "");
*/
protected:
    // Accessors
    bool    GetNamedDescriptor(const string& sKey, ParamDesc** pDescriptorOut = nullptr);     // Gets the descriptor or just returns whether one exists if no pointer is passed in
    bool    GetPositionalDescriptor(int64_t nIndex, ParamDesc** pDescriptorOut = nullptr);       // Gets the descriptor or just returns whether one exists if no pointer is passed in

    string  msAppPath;          // full path to the app.exe
    string  msAppName;          // just the app.exe
    string  msAppDescription;

    // Positional parameters
    int64_t                     mnRegisteredPositional;

    vector<ParamDesc> mParameterDescriptors;

protected:
    // helper funcitons for neat formatting
    void        AddSpacesToEnsureWidth(string& sText, int64_t nWidth);
    int32_t     LongestLine(const string& sText);
    void        OutputFixed(int32_t nWidth, const char* format, ...);
    void        OutputLines(int32_t nWidth, const string& sMultiLineString);
    void        RepeatOut(char c, int32_t nCount, bool bNewLine = false);
};
