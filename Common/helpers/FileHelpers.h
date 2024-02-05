#include <string>
#include <list>


bool ScanFolder(std::string sFolder, std::list<std::string>& fileListResults, bool bRecursive = false);

#ifdef _WIN64
std::string GetFileVersion(const std::string& sFilePath);
#endif
