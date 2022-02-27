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
// Unnamed parameters have to be in order, but not necessarily together
// > Example: "copy c:\sample.txt c:\sample2.txt"
// 
// Named parameters start with '-' and map a key to a value separated by a ':'
// simple flags such as "-run" is treated as a boolean.
// > Example: "-threads:4"  "-verbose" 
//
// Named parameters can be anywhere on the command line, in any order, including before, between or after unnamed parameters
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
// 3) Specify the required number of unnamed parameters
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
//   parser.SetRequiredNumberOfUnnamedParameters(2);
//   parser.RegisterUnnamedString("SOURCE", &sourcePath);
//   parser.RegisterUnnamedString("DESTINATION", &destPath);
//   parser.RegisterUnnamedString("SEARCH", &searchString);
//   parser.RegisterNamedInt64("threads", &nThreads, false, true, 1, 128);
//   parser.RegisterNamedBool("casesensitive", &bCaseSensitive);
//   
//   parser.Parse(argc, argv, false);


class ParameterDescriptor
{
public:
    enum eParamType
    {
        kUnknown = 0,
        kInt64  = 1,
        kBool   = 2,
        kString = 3
    };

    ParameterDescriptor() : mnPosition(-1), mType(kUnknown), mbRequired(false), mpValue(nullptr), mbRangeRestricted(false), mnMinValue(0), mnMaxValue(0), mbFound(false) {}
    
    string      msName;         // named parameters like "-count:12" or "-verbose"
    int64_t     mnPosition;     // unnamed parameter index (0 is the app name, etc.)

    eParamType  mType;
    bool        mbRequired;
    void*       mpValue;

    bool        mbRangeRestricted;
    int64_t     mnMinValue;
    int64_t     mnMaxValue;

    // Tracking
    bool        mbFound;   // For checking whether all required parameters were found

};

typedef map<string, ParameterDescriptor> tKeyToParamDescriptorMap;

class CommandLineParser
{
public:
    CommandLineParser();
    ~CommandLineParser();

    void    SetRequiredNumberOfUnnamedParameters(int64_t nRequired);

    bool    Parse(int argc, char* argv[], bool bVerbose=false);

    // Accessors
    bool    GetNamedDescriptor(const string& sKey, ParameterDescriptor** pDescriptorOut = nullptr);     // Gets the descriptor or just returns whether one exists if no pointer is passed in
    bool    GetUnnamedDescriptor(int64_t nIndex, ParameterDescriptor** pDescriptorOut = nullptr);       // Gets the descriptor or just returns whether one exists if no pointer is passed in
    string  GetUsageString();  // dynamically built from registered parameters

    // Registration Functions
    void    RegisterDescription(const string& sDescription);

    bool    RegisterNamedString(const string& sKey, string* pRegisteredString, bool bRequired = false);
    bool    RegisterNamedInt64(const string& sKey, int64_t* pRegisteredInt64, bool bRequired = false, bool bRangeRestricted=false, int64_t nMinValue=0, int64_t nMaxValue=0);
    bool    RegisterNamedBool(const string& sKey, bool* pRegisteredBool, bool bRequired = false);

    bool    RegisterUnnamedString(const string& sDisplayName, string* pRegisteredString);
    bool    RegisterUnnamedInt64(const string& sDisplayName, int64_t* pRegisteredInt64, bool bRangeRestricted = false, int64_t nMinValue = 0, int64_t nMaxValue = 0);
    bool    RegisterUnnamedBool(const string& sDisplayName, bool* pRegisteredBool);

protected:
    string                      msAppPath;
    string                      msDescription;

    // Named parameters
    tKeyToParamDescriptorMap    mNamedParameterDescriptors;

    // Unnamed parameters
    int64_t                     mnRequiredUnnamed;
    vector<ParameterDescriptor> mUnnamedParameterDescriptors;

protected:
    // helper funcitons for neat formatting
    int32_t     LongestDescriptionLine();
    void        OutputFixed(int32_t nWidth, const char* format, ...);
    void        OutputLines(int32_t nWidth, const string& sMultiLineString);
    string      Int64ToString(int64_t n);
    void        RepeatOut(char c, int32_t nCount, bool bNewLine = false);
};
