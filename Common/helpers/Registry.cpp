#include "Registry.h"
#include <assert.h>
#include <iostream>
#include <fstream>
#include "helpers/StringHelpers.h"
#include "helpers/LoggingHelpers.h"

using namespace std;

namespace REG
{
    void Registry::SetFilename(const std::string& sFilename)
    {
        msRegistryFilename = sFilename;
    }

    bool Registry::Load()
    {
        std::ifstream inFile(msRegistryFilename);
        if (!inFile)
        {
            zout << "WARNING: Cannot open registry file:" << msRegistryFilename.c_str() << "\n";
            return false;
        }

        //mJSON.parse(inFile);
        inFile >> *this;

//        ZDEBUG_OUT(dump());

        return true;
    }

    bool Registry::Save()
    {
        std::ofstream outFile(msRegistryFilename);
        if (!outFile)
        {
            cerr << "ERROR: Cannot open registry file:" << msRegistryFilename.c_str() << "\n";
            return false;
        }

        //outFile << mJSON;
        outFile << *this;

        return true;
    }
    /*
    template <typename T>
    void Registry::Set(const std::string& sGroup, const std::string& sKey, T arg)
    {
        mJSON[sGroup][sKey].insert(arg);
    }

    template <typename T>
    bool Registry::Get(const std::string& sGroup, const std::string& sKey, T& arg)
    {
        if (!mJSON[sGroup].is_null())
        {
            cerr << "ERROR: Group:" << sGroup << " doesn't exist.\n";
            return false;
        }
        if (!mJSON[sGroup].is_null())
        {
            cerr << "ERROR: Group:" << sGroup << " exists. Key:" << sKey << " does NOT.\n";
            return false;
        }

        mJSON[sGroup][sKey].get_to(arg);
        return true;
    }

    template <typename T>
    T Registry::Get(const std::string& sGroup, const std::string& sKey)
    {
        return mJSON[sGroup][sKey].get<T>();
    }

    */
/*    void Registry::Set(const std::string& sGroup, const std::string& sKey, int64_t nValue)
    {
        mGroupToStringMap[sGroup][sKey] = SH::FromInt(nValue);
    }

    void Registry::Set(const std::string& sGroup, const std::string& sKey, double fValue)
    {
        mGroupToStringMap[sGroup][sKey] = SH::FromDouble(fValue);
    }

    void Registry::Set(const std::string& sGroup, const std::string& sKey, bool bValue)
    {
        mGroupToStringMap[sGroup][sKey] = SH::FromInt((int64_t)bValue);
    }

    void Registry::Set(const std::string& sGroup, const std::string& sKey, tStringVector& stringVector)
    {
        mGroupToStringMap[sGroup][sKey] = SH::FromVector(stringVector);
    }

    void Registry::Set(const std::string& sGroup, const std::string& sKey, tStringToStringMap& stringMap)
    {
        mGroupToStringMap[sGroup][sKey] = SH::FromMap(stringMap);
    }

    bool Registry::Get(const std::string& sGroup, const std::string& sKey, string& sValue)
    {
        // Find the group
        tGroupToStringMap::iterator group = mGroupToStringMap.find(sGroup);
        if (group == mGroupToStringMap.end())
            return false;

        // Find the key/value pair
        tStringToStringMap::iterator key = (*group).second.find(sKey);
        if (key == (*group).second.end())
            return false;

        // return the value
        sValue = (*key).second;
        return true;
    }

    bool Registry::Get(const std::string& sGroup, const std::string& sKey, int64_t& nValue)
    {
        string sValue;
        if (!Get(sGroup, sKey, sValue))
            return false;

        nValue = SH::ToInt(sValue);
        return true;
    }

    bool Registry::Get(const std::string& sGroup, const std::string& sKey, double& fValue)
    {
        string sValue;
        if (!Get(sGroup, sKey, sValue))
            return false;

        fValue = SH::ToDouble(sValue);
        return true;
    }

    bool Registry::Get(const std::string& sGroup, const std::string& sKey, bool& bValue)
    {
        string sValue;
        if (!Get(sGroup, sKey, sValue))
            return false;

        bValue = SH::ToBool(sValue);
        return true;
    }

    bool Registry::Get(const std::string& sGroup, const std::string& sKey, tStringVector& stringVector)
    {
        string sValue;
        if (!Get(sGroup, sKey, sValue))
            return false;

        SH::ToVector(sValue, stringVector);
        return true;
    }

    bool Registry::Get(const std::string& sGroup, const std::string& sKey, tStringToStringMap& stringMap)
    {
        string sValue;
        if (!Get(sGroup, sKey, sValue))
            return false;

        SH::ToMap(sValue, stringMap);
        return true;
    }

    bool Registry::Delete(const std::string& sGroup, const std::string& sKey)
    {
    }

    bool Registry::IsRegistered(const std::string& sGroup, const std::string& sKey)
    {
        string sValue;
        if (!Get(sGroup, sKey, sValue))
            return false;

        return true;
    }

    size_t Registry::CountValuesInGroup(const std::string& sGroup)
    {
        // Find the group
        tGroupToStringMap::iterator group = mGroupToStringMap.find(sGroup);
        if (group == mGroupToStringMap.end())
            return 0;

        return (*group).second.size();
    }*/


#ifdef _WIN64

int GetWindowsRegistryString(HKEY key, const string& sPath, const string& sKey, string& sValue)
{
    HKEY hKey;
    LRESULT result = RegOpenKeyEx(key, sPath.c_str(), 0, KEY_READ, &hKey);

    if (result == ERROR_SUCCESS)
    {
        DWORD nDataSize = 2048;
        char buffer[2048];
        DWORD dataType;

        result = RegQueryValueEx(hKey, sKey.c_str(), nullptr, &dataType, (LPBYTE)buffer, &nDataSize);
        RegCloseKey(hKey);

        if (result == ERROR_SUCCESS)
        {
            sValue.assign(buffer);
            return 0;
        }
    }

    return (int)result;
}

int SetWindowsRegistryString(HKEY key, const string& sPath, const string& sKey, const string& sValue)
{
    HKEY hKey;
    LRESULT result = RegCreateKeyEx(key, sPath.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (result == ERROR_SUCCESS)
    {
        result = RegSetValueEx(hKey, sKey.c_str(), 0, REG_SZ, (uint8_t*)sValue.c_str(), (DWORD)sValue.length());
        RegCloseKey(hKey);
        return 0;
    }

    string section;
    if (key == HKEY_CURRENT_USER)
        section = "HKEY_CURRENT_USER";
    else if (key == HKEY_LOCAL_MACHINE)
        section = "HKEY_LOCAL_MACHINE";
    else if (key == HKEY_CLASSES_ROOT)
        section = "HKEY_CLASSES_ROOT";
    else
        section = "unknown section:" + to_string((int64_t)key);

    cerr << "Failed to set registry path:" << section << "\\" << sPath << " key:" << sKey << " value:" << sValue << " error code:" << to_string(result) << "\n";
    return (int)result;
}
#endif






};  // namespace REG
