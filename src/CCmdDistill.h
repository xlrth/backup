#pragma once

#include <vector>

#include "CCmd.h"

class CCmdDistill : public CCmd
{
public:
    virtual std::string GetUsageSpec() override;
    virtual COptions    GetOptionsSpec() override;

    virtual bool Run(const std::vector<CPath>& paths, const COptions& options) override;
private:
    void PrintHelp();
};
