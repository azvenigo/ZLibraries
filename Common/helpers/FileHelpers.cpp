#include "FileHelpers.h"
#include "StringHelpers.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <system_error>

#ifdef _WIN64
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winver.h>
#endif

using namespace std;
namespace fs=std::filesystem;

namespace FH
{

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

    std::string Canonicalize(const std::string& sPath, bool bEncloseWhitespaces)
    {
        fs::path path;
        if (sPath.length() > 2 && sPath[0] == '\"' && sPath[sPath.length() - 1] == '\"')      // strip enclosures
            path = sPath.substr(1, sPath.length() - 2);
        else if (sPath.length() > 2 && sPath[0] == '\'' && sPath[sPath.length() - 1] == '\'')      // strip enclosures
            path = sPath.substr(1, sPath.length() - 2);
        else
            path = sPath;


        try
        {
            std::string sReturnPath = fs::canonical(path).string();

            if (fs::exists(path) && fs::is_directory(path))
                sReturnPath += '\\';

            if (bEncloseWhitespaces && SH::ContainsWhitespace(sReturnPath))
                return "\"" + sReturnPath + "\"";

            return sReturnPath;
        }
        catch (const std::filesystem::filesystem_error& /*e*/)
        {
//            std::cerr << "Filesystem error: " << e.what() << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }

        return sPath;
    }

    bool HasPermission(std::string sPath, Perms perms)
    {
        fs::path path(sPath);
        if (!fs::exists(path))
            return false;

        try
        {
            std::error_code ec;
            fs::perms permissions = fs::status(path, ec).permissions();

            if (ec) 
            {
                std::cerr << "Error checking status: " << ec.message() << "\n";
                return false;
            }

            if (perms & kRead)
            {
                bool canRead = (permissions & fs::perms::owner_read) != fs::perms::none ||
                    (permissions & fs::perms::group_read) != fs::perms::none ||
                    (permissions & fs::perms::others_read) != fs::perms::none;

                if (!canRead)
                    return false;
            }

            if (perms & kWrite)
            {
                bool canWrite = (permissions & fs::perms::owner_write) != fs::perms::none ||
                    (permissions & fs::perms::group_write) != fs::perms::none ||
                    (permissions & fs::perms::others_write) != fs::perms::none;

                if (!canWrite)
                    return false;
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << "Filesystem error: " << e.what() << std::endl;
            return false;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
            return false;
        }

        return true;
    }

    bool ReadIntoBuffer(const std::filesystem::path filename, std::vector<uint8_t>& outBuffer)
    {
        ifstream inFile(filename, ios::binary);
        if (!inFile.is_open())
        {
            cout << "Unable to open file: " << filename << "\n";
            return false;
        }

        size_t size = fs::file_size(filename);
        outBuffer.resize(size);
        inFile.read((char*)&outBuffer[0], size);

        if (!inFile)
        {
            cout << "Read from:" << filename << " failed. err:" << errno << std::error_code(errno, std::generic_category()).message() << "\n";
            return false;
        }

        return true;
    }


#ifdef _WIN64
#define WIN32_LEAN_AND_MEAN

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
};