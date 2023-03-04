#include "COptions.h"

#include "CLogger.h"

void COptions::Log() const
{
    if (verbose)
    {
        CLogger::Log("verbose enabled");
    }
    if (alwaysHash)
    {
        CLogger::Log("always hashing enabled");
    }
    if (mHardLinkMinBytes != 512 + 1)
    {
        CLogger::Log("min bytes for hard link = " + std::to_string(mHardLinkMinBytes));
    }
    if (createNewSnapshot)
    {
        CLogger::Log("create new snapshot enabled");
    }
    if (verifyHashes)
    {
        CLogger::Log("verify hashes enabled");
    }
    if (writeFileTable)
    {
        CLogger::Log("write file table enabled");
    }
    if (compactDB)
    {
        CLogger::Log("compact db enabled");
    }
    if (matchPath)
    {
        CLogger::Log("match paths enabled");
    }
}

COptions& COptions::GetSingletonNonConst()
{
    static COptions sSingleton;
    return sSingleton;
}

const COptions& COptions::GetSingleton()
{
    return GetSingletonNonConst();
}
