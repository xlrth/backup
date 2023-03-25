#include "CRepository.h"

#include <set>

#include "CLogger.h"
#include "Helpers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CRepository CRepository::StaticGetParentRepository(const std::vector<CPath>& snapshotPaths)
{
    VERIFY(!snapshotPaths.empty());

    for (auto& snapshotPath : snapshotPaths)
    {
        if (!std::filesystem::absolute(snapshotPath).has_parent_path())
        {
            throw "cannot resolve snapshot parent directory: " + snapshotPath.string();
        }
    }
    for (auto& snapshotPath : snapshotPaths)
    {
        if (!std::filesystem::equivalent(
            std::filesystem::absolute(snapshotPath).parent_path(),
            std::filesystem::absolute(snapshotPaths.back()).parent_path()))
        {
            throw "snapshots from different repositories mixed";
        }
    }

    return CRepository(std::filesystem::weakly_canonical(snapshotPaths.back()).parent_path(), false);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::vector<CPath> CRepository::StaticGetSnapshotPaths(const CPath repositoryPath)
{
    if (!std::filesystem::is_directory(repositoryPath))
    {
        throw "repository path is not a directory: " + repositoryPath.string();
    }

    // std::set for ordering by timestamp, affecting file search order
    std::set<CPath> snapshotPaths;
    for (auto& p : std::filesystem::directory_iterator(repositoryPath))
    {
        if (!std::filesystem::is_directory(p))
        {
            continue;
        }

        CSnapshot::StaticValidate(p.path());
        snapshotPaths.insert(p.path());
    }

    return std::vector<CPath>(snapshotPaths.begin(), snapshotPaths.end());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepository::StaticValidateSnapshotPaths(const std::vector<CPath>& snapshotPaths)
{
    for (auto& snapshotPath : snapshotPaths)
    {
        CSnapshot::StaticValidate(snapshotPath);
        for (auto& otherSnapshotPath : snapshotPaths)
        {
            if (&snapshotPath == &otherSnapshotPath)
            {
                continue;
            }
            if (std::filesystem::equivalent(snapshotPath, otherSnapshotPath))
            {
                throw "a snapshot path is equal to another: " + snapshotPath.string() + " and " + otherSnapshotPath.string();
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CRepository::CRepository(const CPath& path, bool create)
{
    Open(path, create);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
const CPath& CRepository::GetAbsolutePath() const
{
    return mPath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepository::Open(const CPath& path, bool create)
{
    VERIFY(mSnapshots.empty());

    // use normalized path, so we always have the same max path limits
    mPath = std::filesystem::weakly_canonical(path);

    if (create)
    {
        if (!std::filesystem::exists(mPath))
        {
            std::error_code errorCode;
            if (!std::filesystem::create_directories(mPath, errorCode))
            {
                throw "cannot create repository directory: " + mPath.string() + ": " + errorCode.message();
            }

            CLogger::GetInstance().Log("creating repository: " + mPath.string());
        }
    }

    for (auto& snapshotPath : StaticGetSnapshotPaths(mPath))
    {
        mSnapshots.emplace_back(std::make_unique<CSnapshot>(snapshotPath, false));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepository::Close()
{
    mSnapshots.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
const std::vector<std::shared_ptr<CSnapshot>>& CRepository::GetAllSnapshots() const
{
    return mSnapshots;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepository::AttachSnapshot(std::shared_ptr<CSnapshot> snapshot)
{
    VERIFY(!mPath.empty());

    if (!snapshot->GetAbsolutePath().has_parent_path())
    {
        throw "cannot resolve snapshot parent directory: " + snapshot->GetAbsolutePath().string();
    }
    if (!std::filesystem::equivalent(mPath, snapshot->GetAbsolutePath().parent_path()))
    {
        throw "snapshots from different repositories mixed";
    }
    for (auto& otherSnapshot : mSnapshots)
    {
        if (std::filesystem::equivalent(snapshot->GetAbsolutePath(), otherSnapshot->GetAbsolutePath()))
        {
            throw "a snapshot is equal to another: " + snapshot->GetAbsolutePath().string() + " and " + otherSnapshot->GetAbsolutePath().string();
        }
    }
    mSnapshots.emplace_back(snapshot);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<CSnapshot> CRepository::DetachSnapshot(const CPath& snapshotPath)
{
    for (auto it = mSnapshots.begin(); it != mSnapshots.end(); it++)
    {
        if (std::filesystem::equivalent((*it)->GetAbsolutePath(), snapshotPath))
        {
            std::shared_ptr<CSnapshot> result = *it;
            mSnapshots.erase(it);
            return result;
        }
    }
    throw "cannot detach, snapshot not attached: " + snapshotPath.string();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CRepoFile CRepository::FindFile(const CRepoFile& constraints, bool preferLinkable) const
{
    CRepoFile unlinkableFile;
    for (int i = (int)mSnapshots.size() - 1; i >= 0; i--)
    {
        CRepoFile file = mSnapshots[i]->FindFile(constraints, preferLinkable);
        if (!file.HasHash())
        {
            continue;
        }
        if (!preferLinkable || file.IsLinkable())
        {
            return file;
        }
        unlinkableFile = file;
    }

    return unlinkableFile;
}
