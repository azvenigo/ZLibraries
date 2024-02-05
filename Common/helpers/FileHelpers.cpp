#include "FileHelpers.h"
#include <filesystem>


using namespace std;
namespace fs=std::filesystem;

bool ScanFolder(string sFolder, list<string>& fileListResults, bool bRecursive)
{
    if (bRecursive)
    {
        for (auto const& dir_entry : fs::recursive_directory_iterator(sFolder))
        {
            if (dir_entry.is_regular_file())
            {
                fs::path p = dir_entry.path();
                p.make_preferred();

                fileListResults.push_back(p.lexically_normal().string());
            }
        }
    }
    else
    {
        for (auto const& dir_entry : fs::directory_iterator(sFolder))
        {
            if (dir_entry.is_regular_file())
            {
                fs::path p = dir_entry.path();
                p.make_preferred();
                fileListResults.push_back(p.lexically_normal().string());
            }
        }
    }

    return true;
}

#ifdef _WIN64
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winver.h>


string GetFileVersion(const string& sFilePath)
{
    DWORD dwHandle;

    DWORD dwSize = GetFileVersionInfoSize(sFilePath.c_str(), &dwHandle);
    uint8_t* buf = new uint8_t[dwSize];

    VS_FIXEDFILEINFO* pvFileInfo = NULL;
    UINT fiLen = 0;

    if ((dwSize > 0) && GetFileVersionInfo(sFilePath.c_str(), dwHandle, dwSize, buf))
    {
        VerQueryValue(buf, "\\", (LPVOID*)&pvFileInfo, &fiLen);
    }

    delete[] buf;

    if (fiLen > 0)
    {
        char buf[25];
        int len = sprintf(buf, "%hu.%hu.%hu.%hu",
            HIWORD(pvFileInfo->dwFileVersionMS),
            LOWORD(pvFileInfo->dwFileVersionMS),
            HIWORD(pvFileInfo->dwFileVersionLS),
            LOWORD(pvFileInfo->dwFileVersionLS)
        );

        return string(buf, len);
    }
    else
    {
        return string("unknown");
    }
}
#endif
