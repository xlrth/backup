#pragma once

#include <vector>

#include "COptions.h"
#include "CPath.h"

class CCmd
{
public:
    virtual ~CCmd() {}

    virtual std::string GetUsageSpec() = 0;
    virtual COptions    GetOptionsSpec() = 0;

    virtual bool Run(const std::vector<CPath>& paths, const COptions& options) = 0;
};
