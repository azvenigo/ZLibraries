#include <stdint.h>
#include <string>
#include <list>

namespace FH
{
    enum Perms : uint32_t
    {
        kRead = 1 << 0,     // 1
        kWrite = 1 << 1     // 2
    };

    bool HasPermission(std::string sPath, Perms perms = kRead);
    bool ScanFolder(std::string sFolder, std::list<std::string>& fileListResults, bool bRecursive = false);
    std::string Canonicalize(const std::string& sPath, bool bEncloseWhitespaces = false);     // converts slashes, resolves relative paths, adds trailing slash to directories, surrounds with quotes if path includes whitespaces

#ifdef _WIN64
    std::string GetFileVersion(const std::string& sFilePath);
#endif

}