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

