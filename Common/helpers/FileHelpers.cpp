#include "FileHelpers.h"

#if defined(WIN32) || defined(WIN64)
#include <Windows.h>
#endif

using namespace std;

bool FileExists(const string& sPath)
{
    HANDLE hFile = CreateFileA(sPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    CloseHandle(hFile);
    return true;
}

bool EnsurePath(string sPath)
{
//	cout << "Ensuring path:" << sPath << std::endl;
	
    // If path passed in does not end in '/' or '\\' add it
    
    char sTail = *(sPath.rbegin());
    if (sTail != '\\' && sTail != '/')
    {
        sPath += '/';
    }

	for (string::iterator it = sPath.begin(); it != sPath.end(); it++)
	{
		if (*it == '\\')
			*it = '/';
	}

	size_t nIndex = sPath.find_first_of('/', 1); // skip leading '/'
	do
	{
		string sSubPath = sPath.substr(0, nIndex);

//		cout << "Making path:" << sSubPath << std::endl;

    // Windows implementation

        bool bExists = GetFileAttributesA(sSubPath.c_str()) != INVALID_FILE_ATTRIBUTES;

        if (!bExists)
        {
            BOOL bResult = CreateDirectoryA(sSubPath.c_str(), NULL);
            if (bResult != TRUE)
            {
                cout << "ERROR CreateDirectory(\"" << sSubPath << "\"); nResult:" << std::dec << GetLastError() << "\n";
                return false;
            }
        }

		nIndex = sPath.find_first_of('/', nIndex+1);	// next directory separator
	} while (nIndex != string::npos);

	return true;
}


bool CopyFile(string sInputFilename, string sOutputFilename)
{
    // windows implementation
    return false;
}
