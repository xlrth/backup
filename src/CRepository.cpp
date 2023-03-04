#include "CRepository.h"

#include "set"

#include "CHelpers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CRepository::CRepository(
    const CPath&                repositoryPath,
    CSnapshot::EOpenMode        openMode,
    const std::vector<CPath>&   excludes)
{
    OpenAllSnapshots(repositoryPath, openMode, excludes);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepository::OpenSnapshot(
    const CPath&                snapshotPath,
    CSnapshot::EOpenMode        openMode)
{
    mSnapshots.emplace_back(std::make_unique<CSnapshot>(snapshotPath, openMode));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepository::OpenAllSnapshots(
    const CPath&                repositoryPath,
    CSnapshot::EOpenMode        openMode,
    const std::vector<CPath>&   excludes)
{
    for (auto& s : GetSnapshotPaths(repositoryPath, excludes))
    {
        OpenSnapshot(s, openMode);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
const std::vector<std::unique_ptr<CSnapshot>>& CRepository::GetAllSnapshots() const
{
    return mSnapshots;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::vector<std::unique_ptr<CSnapshot>>& CRepository::GetAllSnapshots()
{
    return mSnapshots;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepository::FindAndDuplicateFile(CRepoFile& target, CSnapshot& targetSnapshot, bool copyOnLinkFail) const
{
    CRepoFile found;
    for (int i = (int)mSnapshots.size() - 1; i >= 0; i--)
    {
        auto iterator = mSnapshots[i]->DBSelect({ {}, {}, {}, {}, {}, target.GetHash() });
        while (iterator.HasFile())
        {
            found = iterator.GetNextFile();
            VERIFY(found.GetSize() == target.GetSize());

            if (targetSnapshot.DuplicateFile(found.GetFullPath(), target))
            {
                return true;
            }
        }
    }

    if (copyOnLinkFail && !found.GetHash().empty())
    {
        if (targetSnapshot.DuplicateFile(found.GetFullPath(), target, true))
        {
            return true;
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::vector<CPath> CRepository::GetSnapshotPaths(const CPath repositoryPath, const std::vector<CPath>& excludes)
{
    if (!std::experimental::filesystem::exists(repositoryPath)
        || !std::experimental::filesystem::is_directory(repositoryPath))
    {
        throw "repository path invalid: " + repositoryPath.string();
    }

    // set for ordering by timestamp
    std::set<CPath> snapshotPaths;
    for (auto& p : std::experimental::filesystem::directory_iterator(repositoryPath))
    {
        if (!std::experimental::filesystem::is_directory(p))
        {
            continue;
        }
        bool skip = false;
        for (auto& exclude : excludes)
        {
            if (std::experimental::filesystem::equivalent(p.path(), exclude))
            {
                skip = true;
                break;
            }
        }
        if (skip)
        {
            continue;
        }
        if (CSnapshot::IsValidPath(p.path()))
        {
            snapshotPaths.insert(p.path());
        }
    }

    return std::vector<CPath>(snapshotPaths.begin(), snapshotPaths.end());
}
