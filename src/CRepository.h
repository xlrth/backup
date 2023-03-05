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
    CRepository(const CPath& repositoryPath);

    void OpenSnapshot(const CPath& snapshotPath);
    void CloseSnapshot(const CPath& snapshotPath);

    CSnapshot& CreateSnapshot();

    const std::vector<std::unique_ptr<CSnapshot>>&   GetAllSnapshots() const;
    std::vector<std::unique_ptr<CSnapshot>>&         GetAllSnapshots();

    void ReorderSnapshotsByDistance(const CPath& centerSnapshotPath);

    bool FindFile(const CRepoFile& repoFile, bool verifyAccessible = true) const;
    bool FindAndDuplicateFile(CRepoFile& target, CSnapshot& targetSnapshot, bool copyOnLinkFail = true) const;

public: // static
    static std::vector<CPath> GetSnapshotPaths(const CPath repositoryPath);

private:
    static std::chrono::system_clock::time_point    SnapshotPathAsTime(const CPath& snapshotPath);

    CPath                                       mRepositoryPath;
    std::vector<std::unique_ptr<CSnapshot>>     mSnapshots;
};
