#pragma once

#include <vector>

#include "CPath.h"

class CDistill
{
public:
    static bool Run(const std::vector<CPath>& snapshotPaths);
};
