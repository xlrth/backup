#pragma once

#include <vector>
#include <memory>

#include "Helpers.h"
#include "CPath.h"
#include "CRepoFile.h"
#include "CSnapshot.h"

class CRepository
{
public: // static
    static CRepository          StaticGetParentRepository(const std::vector<CPath>& snapshotPaths);
    static std::vector<CPath>   StaticGetSnapshotPaths(const CPath repositoryPath);
    static void                 StaticValidateSnapshotPaths(const std::vector<CPath>& snapshotPaths);

public:
    CRepository() = default;
    CRepository(const CPath& path, bool create);

    const CPath& GetAbsolutePath() const;

    void Open(const CPath& path, bool create);
    void Close();

    const std::vector<std::shared_ptr<CSnapshot>>&  GetAllSnapshots() const;

    void                        AttachSnapshot(std::shared_ptr<CSnapshot> snapshot);
    std::shared_ptr<CSnapshot>  DetachSnapshot(const CPath& snapshotPath);


    CRepoFile FindFile(const CRepoFile& constraints, bool preferLinkable) const;

private:
    CPath                                       mPath;
    std::vector<std::shared_ptr<CSnapshot>>     mSnapshots;
};
