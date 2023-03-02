#include "CRepository.h"

#include "CRepoFile.h"
#include "Helpers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepository::OpenSnapshot(const CPath& snapshotPath)
{
    mSnapshots.emplace_back(snapshotPath, false);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepository::OpenAllSnapshots(const CPath& repositoryPath)
{
    if (!std::experimental::filesystem::exists(repositoryPath)
        || !std::experimental::filesystem::is_directory(repositoryPath))
    {
        throw "repository path invalid: " + repositoryPath.string();
    }

    std::vector<CPath> snapshotPaths;
    for (auto& p : std::experimental::filesystem::directory_iterator(repositoryPath))
    {
        if (!std::experimental::filesystem::is_directory(p))
        {
            continue;
        }
        snapshotPaths.emplace_back(p.path());
    }

    std::sort(snapshotPaths.begin(), snapshotPaths.end());

    if (!snapshotPaths.empty() && CurrentTimeAsString() < snapshotPaths.back().filename().string().substr(0, CurrentTimeAsString().length()))
    {
        throw "repository snapshot dir names not consistent, some seem to have a date in the future";
    }

    for (auto& s : snapshotPaths)
    {
        OpenSnapshot(s);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepository::CreateTargetSnapshot(const CPath& repositoryPath)
{
    VERIFY(!mHasTargetSnapshot);
    mHasTargetSnapshot = true;

    mSnapshots.emplace_back(repositoryPath / CurrentTimeAsString(), true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
const CPath& CRepository::GetTargetSnapshotPath() const
{
    VERIFY(mHasTargetSnapshot);

    return mSnapshots.back().GetPath();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRepository::GetTargetSnapshotIndex() const
{
    VERIFY(mHasTargetSnapshot);

    return int(mSnapshots.size()) - 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRepository::GetNewestSourceSnapshotIndex() const
{
    if (mHasTargetSnapshot)
    {
        return int(mSnapshots.size()) - 2;
    }
    else
    {
        return int(mSnapshots.size()) - 1;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRepository::GetSnapshotIndexByPath(const CPath& path) const
{
    for (int snapshotIdx = 0; snapshotIdx < mSnapshots.size(); snapshotIdx++)
    {
        if (std::experimental::filesystem::equivalent(path, mSnapshots[snapshotIdx].GetPath()))
        {
            return snapshotIdx;
        }
    }

    return -1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CRepoFile CRepository::FindRepoFile(const CPath& relativePath, const std::string& hash, int snapshotIdx)
{
    if (snapshotIdx < 0)
    {
        return {};
    }

    auto iterator = mSnapshots[snapshotIdx].Query({ {}, {}, relativePath, CRepoFile::UNSPECIFIED, CRepoFile::UNSPECIFIED, hash });
    if (iterator.HasFile())
    {
        return iterator.GetNextFile();
    }

    return {};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::vector<CRepoFile> CRepository::EnumRepoFiles(int snapshotIdx)
{
    if (snapshotIdx < 0)
    {
        return {};
    }

    auto iterator = mSnapshots[snapshotIdx].Query({ {}, {}, {}, CRepoFile::UNSPECIFIED, CRepoFile::UNSPECIFIED, {} });

    std::vector<CRepoFile> result;
    result.reserve(1000000);
    while (iterator.HasFile())
    {
        result.emplace_back(iterator.GetNextFile());
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepository::Import(const CRepoFile& source)
{
    VERIFY(!source.GetSourcePath().empty());
    VERIFY(!source.GetRelativePath().empty());
    VERIFY(!source.GetHash().empty());
    VERIFY(source.GetSize() != CRepoFile::UNSPECIFIED);
    VERIFY(source.GetDate() != CRepoFile::UNSPECIFIED);

    CRepoFile target = source;
    target.SetParentPath(mSnapshots.back().GetPath());

    if (target.Copy(source.GetSourcePath()))
    {
        mSnapshots.back().Insert(target);
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepository::Duplicate(const CRepoFile& source)
{
    VERIFY(!source.GetSourcePath().empty());
    VERIFY(!source.GetRelativePath().empty());
    VERIFY(!source.GetHash().empty());
    VERIFY(source.GetSize() != CRepoFile::UNSPECIFIED);
    VERIFY(source.GetDate() != CRepoFile::UNSPECIFIED);

    CRepoFile target = source;
    target.SetParentPath(mSnapshots.back().GetPath());

    if (   !source.GetParentPath().empty()
        && source.IsLinkable()
        && target.Link(source.GetFullPath())
        || SearchAndLink(target))
    {
        mSnapshots.back().Insert(target);
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepository::SearchAndLink(const CRepoFile& target)
{
    // try to link archives with same hash. Can fail for deleted files or when exceeding hard link count limits
    // reverse-iterate to improve likelihood of finding archive with hard link count below max
    for (int i = (int)mSnapshots.size() - 1; i >= 0; i--)
    {
        auto iterator = mSnapshots[i].Query(CRepoFile{ {}, {}, {}, CRepoFile::UNSPECIFIED, CRepoFile::UNSPECIFIED, target.GetHash() });
        while (iterator.HasFile())
        {
            auto file = iterator.GetNextFile();
            if (!file.IsLinkable())
            {
                continue;
            }

            VERIFY(target.GetSize() == file.GetSize());

            if (target.Link(file.GetFullPath()))
            {
                return true;
            }
        }
    }

    return false;
}
