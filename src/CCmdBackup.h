#pragma once

#include <string>
#include <vector>

#include "CCmd.h"
#include "CRepository.h"

class CCmdBackup : public CCmd
{
public:
    virtual std::string GetUsageSpec() override;
    virtual COptions    GetOptionsSpec() override;

    virtual bool Run(const std::vector<CPath>& paths, const COptions& options) override;

private:
    void PrintHelp();

    void    ReadConfig(const CPath& configPath);
    void    PrepareSources(const CPath& repositoryPath);
    CPath   FormatTargetPath(const CPath& sourcePath);
    bool    IsBlacklisted(const CPath& sourcePath);

    void BackupEntryRecursive(const CPath& sourcePath, const CPath& targetPathRelative);
    void BackupDirectory(const CPath& sourcePath, const CPath& targetPathRelative);
    void BackupFile(const CPath& sourcePath, const CPath& targetPathRelative);
    bool LockAndHash(CRepoFile& targetFile, CRepoFile& existingFile);
    void LogStats();

    COptions                    mOptions;
    std::vector<CPath>          mSources;
    std::vector<CPath>          mExcludes;
    CRepository                 mRepository;
    std::shared_ptr<CSnapshot>  mTargetSnapshot;

    long long mExcludeCountBlacklisted  = 0;
    long long mExcludeCountSymlink      = 0;
    long long mExcludeCountUnknownType  = 0;
};
