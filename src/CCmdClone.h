#pragma once

#include <vector>

#include "CCmd.h"
#include "CSnapshot.h"
#include "CRepository.h"

class CCmdClone : public CCmd
{
public:
    virtual std::string GetUsageSpec() override;
    virtual COptions    GetOptionsSpec() override;

    virtual bool Run(const std::vector<CPath>& paths, const COptions& options) override;

private:
    void PrintHelp();
    void CloneFile(const CRepoFile& sourceFile, CRepository& targetRepository, CSnapshot& targetSnapshot, const COptions& options);
};
