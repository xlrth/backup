#pragma once

#include <vector>

#include "CPath.h"

class CPurge
{
public:
    static bool Run(const std::vector<CPath>& snapshotPaths);
};
