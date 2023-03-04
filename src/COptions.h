#pragma once

#include "CHelpers.h"

class COptions
{
public:
    bool verbose            = false;
    bool alwaysHash         = false;
    int mHardLinkMinBytes   = 512 + 1;
    int createNewSnapshot   = 0;
    int verifyHashes        = 0;
    int writeFileTable      = 0;
    int compactDB           = 0;
    int matchPath           = 0;

    // would need automatic sync to usage message
    //bool ParseCmdLineArg(const std::string& cmdLineArg);

    void Log() const;

    static COptions& GetSingletonNonConst();
    static const COptions& GetSingleton();
};
