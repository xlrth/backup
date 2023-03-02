#pragma once

#include <string>
#include <vector>
#include <experimental/filesystem>

#include "COptions.h"
#include "CPath.h"
#include "CRepoFile.h"
#include "CSnapshot.h"

class CRepository
{
public:
    void OpenSnapshot(const CPath& snapshotPath);
    void OpenAllSnapshots(const CPath& repositoryPath);

    void CreateTargetSnapshot(const CPath& repositoryPath);

    const CPath&    GetTargetSnapshotPath();
    int             GetTargetSnapshotIndex();
    int             GetNewestSourceSnapshotIndex();
    int             GetSnapshotIndexByPath(const CPath& path);


//    const std::vector<CSnapshot>& GetAllSnapshots() const;
//    const CSnapshot& GetCurrentSnapshot() const;

    // find           by suffix                     in last snapshot                        OFTEN               backup
    // find           by suffix, hash               in source snapshot                      OFTEN               subtract
    // find           by         hash               in source snapshot                      OFTEN               subtract
    // find           by suffix, hash               in target snapshot                      OFTEN               add
    // find           by         hash               in target snapshot                      OFTEN               add
    // enum all                                     in ref, source snapshot                 ONCE PER SNAP       verify, recover, purge, add

    CRepoFile               FindRepoFile(const CPath& relativePath, const std::string& hash, int snapshotIdx);
    std::vector<CRepoFile>  EnumRepoFiles(int snapshotIdx);

    bool Import(const CRepoFile& source);
    bool Duplicate(const CRepoFile& source);

private:
    bool SearchAndLink(const CRepoFile& target);

    std::vector<CSnapshot>  mSnapshots;

    bool mHasTargetSnapshot = false;
};
