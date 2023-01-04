#pragma once
#include <string>
#include <map>
#include <vector>

//#define NDEBUG

#include "json.hpp"

//using json = nlohmann::json;

namespace REG
{
    // behavior flags

    typedef std::vector<std::string>            tStringVector;
    typedef std::map<std::string, std::string>  tStringToStringMap;

    typedef std::map<std::string, tStringToStringMap> tGroupToStringMap;

    class Registry : public nlohmann::json
    {
    public:
        bool    Load(const std::string& sFilename);
        bool    Save(const std::string& sFilename);

/*        template <typename T>
        inline void    Set(const std::string& sGroup, const std::string& sKey, T arg)
        {
            (*this)[sGroup][sKey] = arg;
        }

        template <typename T>
        inline bool    Get(const std::string& sGroup, const std::string& sKey, T& arg)
        {
            if (!(*this).contains(sGroup))
            {
                cerr << "Registry contains no group:" << sGroup << "\n";
                return false;
            }
            if (!(*this)[sGroup].contains(sKey))
            {
                cerr << "Registry group:" << sGroup << " contains no key:" << sKey << "\n";
                return false;
            }

            (*this)[sGroup][sKey].get_to(arg);
            return true;
        }*/

        // GetOrSetDefault will return a value if it is already in the registry.
        // Otherwise will set the registry key to the passed in default and return that
        template <typename T>
        inline void GetOrSetDefault(const std::string& sGroup, const std::string& sKey, T& arg, const T& default)
        {
            if ((*this).contains(sGroup))
            {
                if ((*this)[sGroup].contains(sKey))
                {
                    (*this)[sGroup][sKey].get_to(arg);  // already in the registry, return it in arg
                    return;
                }
            }

            (*this)[sGroup][sKey] = default;
            arg = default;
        }
    protected:
        std::string         msRegistryFilename;
    };
};  // namespace REG
extern REG::Registry gRegistry;

