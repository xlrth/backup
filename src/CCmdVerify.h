#pragma once

#include <vector>
#include <unordered_map>

#include "CCmd.h"
#include "CRepoFile.h"
#include "CSnapshot.h"
#include "CFileTable.h"

class CCmdVerify : public CCmd
{
public:
    virtual std::string GetUsageSpec() override;
    virtual COptions    GetOptionsSpec() override;

    virtual bool Run(const std::vector<CPath>& paths, const COptions& options) override;

private:
    void PrintHelp();
    void VerifyFiles(CFileTable& fileTable, const CSnapshot& snapshot, int snapshotIdx, const COptions& options);
};
