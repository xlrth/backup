#include "CRepository.h"

#include "CRepoFile.h"
#include "Helpers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepository::Init(const CPath& repositoryPath)
{
    if (!std::experimental::filesystem::exists(repositoryPath)
        || !std::experimental::filesystem::is_directory(repositoryPath))
    {
        throw "repository path invalid: " + repositoryPath.string();
    }

    std::vector<CPath> snapshotsAll;
    for (auto& p : std::experimental::filesystem::directory_iterator(repositoryPath))
    {
        if (!std::experimental::filesystem::is_directory(p))
        {
            continue;
        }
        snapshotsAll.emplace_back(p.path());
    }
    std::sort(snapshotsAll.begin(), snapshotsAll.end(), [](const auto& a, const auto& b) {return a < b; });

    for (auto& s : snapshotsAll)
    {
        mSnapshots.emplace_back(s, false);
    }

    auto targetSnapshotName = CurrentTimeAsString();

    if (!snapshotsAll.empty() && targetSnapshotName < snapshotsAll.back().filename().string().substr(0, targetSnapshotName.length()))
    {
        throw "repository snapshot dir names not consistent, some seem to have a date in the future";
    }

    mSnapshots.emplace_back(repositoryPath / CurrentTimeAsString(), true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CRepoFile CRepository::Find(const CPath& relativePath, const std::string& hash, int snapshotIdx)
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
std::vector<CRepoFile> CRepository::EnumAll(int snapshotIdx)
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
const CPath& CRepository::GetTargetSnapshotPath()
{
    if (mSnapshots.empty())
    {
        static CPath empty;
        return empty;
    }

    return mSnapshots.back().GetPath();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRepository::GetSnapshotIndexByPath(const CPath& path)
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
int CRepository::GetTargetSnapshotIndex()
{
    return int(mSnapshots.size()) - 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRepository::GetNewestSnapshotIndex()
{
    return int(mSnapshots.size()) - 2;
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
