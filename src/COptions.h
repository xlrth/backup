#pragma once

#include "Helpers.h"

class COptions
{
public:
    bool verbose            = false;
    bool alwaysHash         = false;
    int mHardLinkMinBytes   = 512 + 1;
    int createNewSnapshot   = 0;
    int verifyHashes        = 0;
    int writeFileTable      = 0;
    int matchPath           = 0;

    void Log() const
    {
        if (verbose)
        {
            ::Log("verbose enabled");
        }
        if (alwaysHash)
        {
            ::Log("always hashing enabled");
        }
        if (mHardLinkMinBytes != 512 + 1)
        {
            ::Log("min bytes for hard link = " + std::to_string(mHardLinkMinBytes));
        }
        if (createNewSnapshot)
        {
            ::Log("creating snapshot enabled");
        }
        if (verifyHashes)
        {
            ::Log("verifying hashes enabled");
        }
        if (writeFileTable)
        {
            ::Log("writing file table enabled");
        }
        if (matchPath)
        {
            ::Log("matching paths enabled");
        }
    }

    static COptions& GetSingletonNonConst()
    {
        static COptions sSingleton;
        return sSingleton;
    }

    static const COptions& GetSingleton()
    {
        return GetSingletonNonConst();
    }
};
