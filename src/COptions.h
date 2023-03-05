#pragma once

#include "CHelpers.h"

class COptions
{
public:
    bool verbose            = false;
    bool alwaysHash         = false;
    bool skipUnchanged      = false;
    bool verifyHashes       = false;
    bool writeFileTable     = false;
    bool compactDB          = false;
    bool verifyAccessible   = false;

    int mHardLinkMinBytes = 512 + 1;

    // would need automatic sync to usage message
    //bool ParseCmdLineArg(const std::string& cmdLineArg);

    void Log() const;

    static COptions& GetSingletonNonConst();
    static const COptions& GetSingleton();
};
