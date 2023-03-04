#pragma once

#include <vector>
#include <memory>

#include "CPath.h"
#include "CRepoFile.h"
#include "CSnapshot.h"

class CRepository
{
public:
    CRepository() = default;
    CRepository(
        const CPath&                repositoryPath, 
        CSnapshot::EOpenMode        openMode = CSnapshot::EOpenMode::READ, 
        const std::vector<CPath>&   excludes = {});

    void OpenSnapshot(
        const CPath&                snapshotPath,
        CSnapshot::EOpenMode        openMode = CSnapshot::EOpenMode::READ);

    void OpenAllSnapshots(
        const CPath&                repositoryPath, 
        CSnapshot::EOpenMode        openMode = CSnapshot::EOpenMode::READ, 
        const std::vector<CPath>&   excludes = {});

//    CSnapshot&                      GetSnapshotByPath();
//    const CSnapshot&                GetSnapshotByPath() const;
    const std::vector<std::unique_ptr<CSnapshot>>&   GetAllSnapshots() const;
    std::vector<std::unique_ptr<CSnapshot>>&         GetAllSnapshots();

    bool FindAndDuplicateFile(CRepoFile& target, CSnapshot& targetSnapshot, bool copyOnLinkFail = true) const;

public:
    static std::vector<CPath> GetSnapshotPaths(const CPath repositoryPath, const std::vector<CPath>& excludes = {});

private:
    std::vector<std::unique_ptr<CSnapshot>>  mSnapshots;
};
