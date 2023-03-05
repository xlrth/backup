#include "CRepository.h"

#include "set"

#include "CHelpers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CRepository::CRepository(const CPath& repositoryPath)
    : mRepositoryPath(repositoryPath)
{
    for (auto& snapshotPath : GetSnapshotPaths(repositoryPath))
    {
        mSnapshots.emplace_back(std::make_unique<CSnapshot>(snapshotPath, CSnapshot::EOpenMode::READ));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepository::OpenSnapshot(const CPath& snapshotPath)
{
    for (auto& snapshot : mSnapshots)
    {
        VERIFY(!std::experimental::filesystem::equivalent(snapshot->GetPath(), snapshotPath));
    }
    mSnapshots.emplace_back(std::make_unique<CSnapshot>(snapshotPath, CSnapshot::EOpenMode::READ));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepository::CloseSnapshot(const CPath& snapshotPath)
{
    for (auto it = mSnapshots.begin(); it != mSnapshots.end(); it++)
    {
        if (std::experimental::filesystem::equivalent((*it)->GetPath(), snapshotPath))
        {
            mSnapshots.erase(it);
            return;
        }
    }
    throw "cannot close snapshot, path not found: " + snapshotPath.string();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CSnapshot& CRepository::CreateSnapshot()
{
    mSnapshots.emplace_back(std::make_unique<CSnapshot>(mRepositoryPath / CHelpers::CurrentTimeAsString(), CSnapshot::EOpenMode::CREATE));
    return *mSnapshots.back();
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
void CRepository::ReorderSnapshotsByDistance(const CPath& centerSnapshotPath)
{
    auto centerSnapshotTime = SnapshotPathAsTime(centerSnapshotPath);
    std::sort(mSnapshots.begin(), mSnapshots.end(), [centerSnapshotTime](const auto& s1, const auto& s2)
    {
        return std::abs((SnapshotPathAsTime(s1->GetPath()) - centerSnapshotTime).count())
            >  std::abs((SnapshotPathAsTime(s2->GetPath()) - centerSnapshotTime).count());
    });
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepository::FindFile(const CRepoFile& repoFile, bool verifyAccessible) const
{
    for (int i = (int)mSnapshots.size() - 1; i >= 0; i--)
    {
        auto iterator = mSnapshots[i]->DBSelect({ {}, {}, {}, {}, {}, repoFile.GetHash() });
        while (iterator.HasFile())
        {
            auto found = iterator.GetNextFile();
            VERIFY(found.GetSize() == repoFile.GetSize());

            if (!verifyAccessible || found.Open())
            {
                return true;
            }
        }
    }

    return false;
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
std::vector<CPath> CRepository::GetSnapshotPaths(const CPath repositoryPath)
{
    if (!std::experimental::filesystem::is_directory(repositoryPath))
    {
        throw "repository path invalid: " + repositoryPath.string();
    }

    // set for ordering by timestamp
    std::set<CPath> snapshotPaths;
    for (auto& p : std::experimental::filesystem::directory_iterator(repositoryPath))
    {
        if (!CSnapshot::IsValidPath(p.path()))
        {
            continue;
        }
        if (SnapshotPathAsTime(p.path()) == std::chrono::system_clock::time_point{} || SnapshotPathAsTime(p.path()) > std::chrono::system_clock::now())
        {
            throw "snapshot directory name malformed: " + p.path().string();
        }

        snapshotPaths.insert(p.path());
    }

    return std::vector<CPath>(snapshotPaths.begin(), snapshotPaths.end());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::chrono::system_clock::time_point CRepository::SnapshotPathAsTime(const CPath& snapshotPath)
{
    return CHelpers::StringAsTime(snapshotPath.filename().string());
}
