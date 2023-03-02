#pragma once

#include <vector>

#include "CPath.h"

class CVerify
{
public:
    static void Run(const std::vector<CPath>& snapshotPaths);
};
