#include "CBackup.h"

#include "CPath.h"
#include "CRepoFile.h"
#include "Helpers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBackup::ReadConfig(
    const CPath&                configPath, 
    std::vector<CPath>&         sources, 
    std::vector<std::string>&   excludes)
{
    std::ifstream configFileHandle(configPath);
    VERIFY(configFileHandle.is_open());

    sources.clear();
    excludes.clear();

    std::string line;
    std::string currentCategory;
    while (std::getline(configFileHandle, line))
    {
        if (line.empty())
        {
            continue;
        }
        if (line.front() == '*')
        {
            continue;
        }
        if (line.front() == '[')
        {
            if(line.back() != ']')
            {
                throw "config: invalid format";
            }
            currentCategory = line.substr(1, line.length() - 2);
        }
        else
        {
            if (ToUpper(currentCategory) == "SOURCES")
            {
                sources.push_back(line);
            }
            else if (ToUpper(currentCategory) == "EXCLUDES")
            {
                excludes.push_back(line);
            }
            else
            {
                throw "config: invalid category: " + currentCategory;
            }
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBackup::BackupSingleRecursive(const CPath& sourcePath, const CPath& targetPathRelative)
{
    std::string sourceStrUpper = ToUpper(sourcePath.string());
    for (const auto& exclude : mExcludes)
    {
        if (exclude.length() < sourceStrUpper.length() && std::mismatch(exclude.rbegin(), exclude.rend(), sourceStrUpper.rbegin())
            .first == exclude.rend())
        {
            Log("excluding: " + sourcePath.string(), COLOR_EXCLUDE);
            return;
        }
    }

    if (!std::experimental::filesystem::exists(sourcePath))
    {
        LogError("cannot find source, excluding: " + sourcePath.string());
        return;
    }

    if (std::experimental::filesystem::is_directory(sourcePath))
    {
        for (auto& entry : std::experimental::filesystem::directory_iterator(sourcePath))
        {
            BackupSingleRecursive(entry.path(), targetPathRelative / CPath(entry.path().filename()));
        }
    }
    else
    {
        CRepoFile repoFile;

        repoFile.SetSourcePath(sourcePath);
        repoFile.SetRelativePath(targetPathRelative);

        // lock file to prevent others from modification
        if (!repoFile.OpenSource())
        {
            LogError("cannot lock, excluding: " + sourcePath.string());
            return;
        }

        repoFile.SetSize(std::experimental::filesystem::file_size(sourcePath));
        repoFile.SetDate(std::experimental::filesystem::last_write_time(sourcePath).time_since_epoch().count());

        CRepoFile lastBackup = mRepository.Find(targetPathRelative, "", mRepository.GetNewestSnapshotIndex());

        if (   lastBackup.GetSize() == repoFile.GetSize()
            && lastBackup.GetDate() == repoFile.GetDate())
        {
            VERIFY(!lastBackup.GetHash().empty());
            VERIFY(!lastBackup.GetParentPath().empty());
            repoFile.SetHash(lastBackup.GetHash());
            repoFile.SetParentPath(lastBackup.GetParentPath());
        }

        if (!repoFile.GetHash().empty() && !COptions::GetSingleton().alwaysHash)
        {
            LogDebug("skipping hashing: " + repoFile.SourceToString(), COLOR_HASH_SKIP);
        }
        else
        {
            LogDebug("hashing: " + repoFile.SourceToString(), COLOR_HASH);

            std::string lastHash = repoFile.GetHash();
            if (!repoFile.HashSource())
            {
                LogError("cannot hash, excluding: " + repoFile.SourceToString());
                return;
            }

            if (!lastHash.empty() && lastHash != repoFile.GetHash())
            {
                LogWarning("supposedly unchanged file with hash mismatch versus last: " + repoFile.SourceToString());
                repoFile.SetParentPath("");
            }
        }

        if (mRepository.Duplicate(repoFile))
        {
            return;
        }

        Log("importing: " + repoFile.SourceToString(), COLOR_ADD);

        if (!mRepository.Import(repoFile))
        {
            LogError("cannot import new, excluding: " + repoFile.SourceToString());
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBackup::Backup(const std::vector<CPath>& paths)
{
    VERIFY(paths.size() == 2);

    CPath                 repositoryPath = paths[0];
    std::vector<CPath>    sources;

    ReadConfig(paths[1], sources, mExcludes);

    for (auto& exclude : mExcludes)
    {
        exclude = ToUpper(exclude);
    }

    mRepository.Init(repositoryPath);

    InitLog(mRepository.GetTargetSnapshotPath());

    COptions::GetSingleton().Log();

    Log("backuping to " + mRepository.GetTargetSnapshotPath().string());

    for (auto& source : sources)
    {
        std::string sourceStrMod = source.string();

#ifdef _WIN32
        if (sourceStrMod.length() == 2 && sourceStrMod[1] == ':')
        {
            sourceStrMod.append({ '\\' });
        }
#endif

        std::string destStrMod = sourceStrMod;

#ifdef _WIN32
        std::replace(destStrMod.begin(), destStrMod.end(), ':', '#');
        std::replace(destStrMod.begin(), destStrMod.end(), '\\', '#');
#endif
        std::replace(destStrMod.begin(), destStrMod.end(), '/', '#');

        auto sourceMod = CPath(std::experimental::filesystem::u8path(sourceStrMod));
        auto destSuffix = CPath(std::experimental::filesystem::u8path(destStrMod));

        for (int number = 1; number < 100; number++)
        {
            auto dest = mRepository.GetTargetSnapshotPath() / destSuffix;
            if (!std::experimental::filesystem::exists(dest))
            {
                break;
            }
            destSuffix = CPath(std::experimental::filesystem::u8path(destStrMod + "_" + std::to_string(number)));
            LogWarning("destination already exists, adding suffix and trying again: " + destSuffix.string());
        }

        BackupSingleRecursive(sourceMod, destSuffix);
    }

    Log("DONE.");
    Log("bytes copied: " + NumberAsString(CRepoFile::StaticGetBytesCopied(), 19));
    Log("bytes linked: " + NumberAsString(CRepoFile::StaticGetBytesLinked(), 19));

    CloseLog();
}
