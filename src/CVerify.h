#pragma once

#include <vector>

#include "CPath.h"

class CVerify
{
public:
    static bool Run(const std::vector<CPath>& paths);
};
