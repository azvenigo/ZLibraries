#include "Registry.h"
#include <assert.h>
#include <iostream>
#include <fstream>
#include <ZXMLNode.h>
#include "helpers/StringHelpers.h"

using namespace std;

namespace REG
{
    bool Registry::ViewImage(const std::string& sFilename)
    {
        std::ifstream inFile(sFilename);
        if (!inFile)
        {
            cerr << "ERROR: Cannot open registry file:" << sFilename.c_str() << "\n";
            return false;
        }

        //mJSON.parse(inFile);
        inFile >> *this;

//        ZDEBUG_OUT(dump());

        msRegistryFilename = sFilename;
        return true;
    }

    bool Registry::Save(const std::string& sFilename)
    {
        std::ofstream outFile(sFilename);
        if (!outFile)
        {
            cerr << "ERROR: Cannot open registry file:" << sFilename.c_str() << "\n";
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
};  // namespace REG
