#include "CBackup.h"

#include "COptions.h"
#include "CLogger.h"
#include "CHelpers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBackup::Run(const std::vector<CPath>& paths)
{
    if (paths.size() != 2)
    {
        return false;
    }

    CPath                       repositoryPath = paths[0];
    std::vector<CPath>          sources;
    std::vector<std::string>    excludes;

    ReadConfig(paths[1], sources, excludes);

    for (auto& exclude : excludes)
    {
        exclude = CHelpers::ToUpper(exclude);
    }

    CRepository repository(repositoryPath);

    const CSnapshot* lastSnapshot = nullptr;
    if (!repository.GetAllSnapshots().empty())
    {
        lastSnapshot = &*repository.GetAllSnapshots().back();
    }
    CSnapshot& targetSnapshot = repository.CreateSnapshot();

    CLogger::Init(targetSnapshot.GetPath());

    COptions::GetSingleton().Log();

    CLogger::Log("backing up to snapshot " + targetSnapshot.GetPath().string());

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
            auto dest = targetSnapshot.GetPath() / destSuffix;
            if (!std::experimental::filesystem::exists(dest))
            {
                break;
            }
            destSuffix = CPath(std::experimental::filesystem::u8path(destStrMod + "_" + std::to_string(number)));
            CLogger::LogWarning("destination already exists, adding suffix and trying again: " + destSuffix.string());
        }

        BackupSingleRecursive(sourceMod, destSuffix, excludes, repository, lastSnapshot, targetSnapshot);
    }

    CLogger::Log("finished backing up to snapshot " + targetSnapshot.GetPath().string());
    CRepoFile::StaticLogStats();
    CLogger::Close();

    return true;
}

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
            if (CHelpers::ToUpper(currentCategory) == "SOURCES")
            {
                sources.push_back(line);
            }
            else if (CHelpers::ToUpper(currentCategory) == "EXCLUDES")
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
void CBackup::BackupSingleRecursive(
        const CPath&                    sourcePath,
        const CPath&                    targetPathRelative,
        const std::vector<std::string>& excludes,
        const CRepository&              repository,
        const CSnapshot*                lastSnapshot,
        CSnapshot&                      targetSnapshot)
{
    std::string sourceStrUpper = CHelpers::ToUpper(sourcePath.string());
    for (const auto& exclude : excludes)
    {
        if (exclude.length() < sourceStrUpper.length() && std::mismatch(exclude.rbegin(), exclude.rend(), sourceStrUpper.rbegin())
            .first == exclude.rend())
        {
            CLogger::Log("excluding: " + sourcePath.string(), COLOR_EXCLUDE);
            return;
        }
    }

    if (!std::experimental::filesystem::exists(sourcePath))
    {
        CLogger::LogError("cannot find source, excluding: " + sourcePath.string());
        return;
    }

    if (std::experimental::filesystem::is_directory(sourcePath))
    {
        for (auto& entry : std::experimental::filesystem::directory_iterator(sourcePath))
        {
            BackupSingleRecursive(entry.path(), targetPathRelative / CPath(entry.path().filename()), excludes, repository, lastSnapshot, targetSnapshot);
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
            CLogger::LogError("cannot lock, excluding: " + sourcePath.string());
            return;
        }

        repoFile.SetSize(std::experimental::filesystem::file_size(sourcePath));
        repoFile.SetDate(std::experimental::filesystem::last_write_time(sourcePath).time_since_epoch().count());

        CRepoFile lastBackup;
        if (lastSnapshot != nullptr)
        {
            lastBackup = lastSnapshot->FindFirstFile({ {}, {}, targetPathRelative, repoFile.GetSize(), repoFile.GetDate(), {} });
        }

        if (!lastBackup.GetHash().empty())
        {
            repoFile.SetHash(lastBackup.GetHash());
        }

        if (!repoFile.GetHash().empty() && !COptions::GetSingleton().alwaysHash)
        {
            LOG_DEBUG("skipping hashing: " + repoFile.SourceToString(), COLOR_HASH_SKIP);
        }
        else
        {
            LOG_DEBUG("hashing: " + repoFile.SourceToString(), COLOR_HASH);

            std::string lastHash = repoFile.GetHash();
            if (!repoFile.HashSource())
            {
                CLogger::LogError("cannot hash, excluding: " + repoFile.SourceToString());
                return;
            }

            if (!lastHash.empty() && lastHash != repoFile.GetHash())
            {
                CLogger::LogWarning("supposedly unchanged file with hash mismatch versus last: " + repoFile.SourceToString());
                lastBackup = CRepoFile{};
            }
        }

        if (!lastBackup.GetHash().empty() && targetSnapshot.DuplicateFile(lastBackup.GetFullPath(), repoFile))
        {
            LOG_DEBUG("duplicated last backup: " + repoFile.ToString(), COLOR_DUP_LAST);
            return;
        }

        if (repository.FindAndDuplicateFile(repoFile, targetSnapshot))
        {
            LOG_DEBUG("duplicated by hash: " + repoFile.ToString(), COLOR_DUP_HASH);
            return;
        }

        CLogger::Log("importing: " + repoFile.SourceToString(), COLOR_IMPORT);

        if (!targetSnapshot.ImportFile(sourcePath, repoFile))
        {
            CLogger::LogError("cannot import new, excluding: " + repoFile.SourceToString());
        }
    }
}
