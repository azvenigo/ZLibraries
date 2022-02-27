#include <string>
#include <list>
using namespace std;

//bool FileExists(const string& sPath);       // use    fs::exists(sPath);  
// 
//bool EnsurePath(string sPath);              // use    fs::create_directories(sPath);

//bool CopyFile(string sInputFilename, string sOutputFilename);   // use fs::copy_file(sFrom, sTo);


bool ScanFolder(string sFolder, list<string>& fileListResults, bool bRecursive = false);
