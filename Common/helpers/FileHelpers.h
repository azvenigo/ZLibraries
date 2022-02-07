#include <string>
#include <iostream>
#include <list>
#include <Windows.h>

using namespace std;

bool FileExists(const string& sPath);
bool EnsurePath(string sPath);
bool CopyFile(string sInputFilename, string sOutputFilename);
bool ScanFolder(string sFolder, list<string>& fileListResults);
