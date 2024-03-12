#pragma once
#include <string>
#include <map>
#include <vector>

#ifdef _WIN64
#include <Windows.h>
#endif

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
        void    SetFilename(const std::string& sFilename);

        bool    Load();
        bool    Save();

        // GetOrSetDefault will return a value if it is already in the registry.
        // Otherwise will set the registry key to the passed in default and return that
        template <typename T>
        inline bool SetDefault(const std::string& sGroup, const std::string& sKey, const T& _default)
        {
            if (Contains(sGroup, sKey))
            {
                assert(false);
                return false;
            }

            (*this)[sGroup][sKey] = _default;
            return true;
        }

        template <typename T>
        inline void Set(const std::string& sGroup, const std::string& sKey, const T& arg)
        {
            (*this)[sGroup][sKey] = arg;
        }

        template <typename T>
        inline bool Get(const std::string& sGroup, const std::string& sKey, T& arg)
        {
            if ((*this).contains(sGroup))
            {
                if ((*this)[sGroup].contains(sKey))
                {
                    (*this)[sGroup][sKey].get_to(arg);  // already in the registry, return it in arg
                    return true;
                }
            }

            return false;
        }

        inline void Remove(const std::string& sGroup, const std::string& sKey)
        {
            if (Contains(sGroup, sKey))
                (*this)[sGroup].erase(sKey);
        }


        inline std::string GetValue(const std::string& sGroup, const std::string& sKey)
        {
            if ((*this).contains(sGroup))
            {
                if ((*this)[sGroup].contains(sKey))
                {
                    std::string s;
                    (*this)[sGroup][sKey].get_to(s);  // already in the registry, return it in arg
                    return s;
                }
            }

            return "";
        }

        inline bool Contains(const std::string& sGroup, const std::string& sKey)
        {
            return ((*this).contains(sGroup) && (*this)[sGroup].contains(sKey));
        }

    protected:
        std::string         msRegistryFilename;
    };





#ifdef _WIN64
    int GetWindowsRegistryString(HKEY key, const std::string& sPath, const std::string& sKey, std::string& sValue);
    int SetWindowsRegistryString(HKEY key, const std::string& sPath, const std::string& sKey, const std::string& sValue);
#endif

};  // namespace REG
extern REG::Registry gRegistry;

