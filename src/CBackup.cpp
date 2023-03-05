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

    CSnapshot& targetSnapshot = repository.CreateSnapshot(COptions::GetSingleton().suffix);

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

        BackupSingleRecursive(sourceMod, destSuffix, excludes, repository, targetSnapshot);
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
            BackupSingleRecursive(entry.path(), targetPathRelative / CPath(entry.path().filename()), excludes, repository, targetSnapshot);
        }
    }
    else
    {
        CRepoFile targetFile;

        targetFile.SetSourcePath(sourcePath);
        targetFile.SetRelativePath(targetPathRelative);

        targetFile.SetSize(std::experimental::filesystem::file_size(sourcePath));
        targetFile.SetDate(std::experimental::filesystem::last_write_time(sourcePath).time_since_epoch().count());

        CRepoFile existingFile = repository.FindFile(
                { {}, {}, targetFile.GetRelativePath(), targetFile.GetSize(), targetFile.GetDate(), {} },
                COptions::GetSingleton().verifyAccessible,
                false);

        if (!existingFile.GetHash().empty())
        {
            targetFile.SetHash(existingFile.GetHash());
        }

        if (!targetFile.GetHash().empty() && !COptions::GetSingleton().alwaysHash)
        {
            LOG_DEBUG("skipping hashing: " + targetFile.SourceToString(), COLOR_HASH_SKIP);
        }
        else
        {
            LOG_DEBUG("hashing: " + targetFile.SourceToString(), COLOR_HASH);

            // lock file to prevent others from modification
            if (!targetFile.OpenSource())
            {
                CLogger::LogError("cannot lock, excluding: " + sourcePath.string());
                return;
            }
            // re-read properties after locking
            targetFile.SetSize(std::experimental::filesystem::file_size(sourcePath));
            targetFile.SetDate(std::experimental::filesystem::last_write_time(sourcePath).time_since_epoch().count());

            std::string lastHash = targetFile.GetHash();
            if (!targetFile.HashSource())
            {
                CLogger::LogError("cannot hash, excluding: " + targetFile.SourceToString());
                return;
            }
            VERIFY(!targetFile.GetHash().empty());

            if (!lastHash.empty() && lastHash != targetFile.GetHash())
            {
                CLogger::LogWarning("supposedly unchanged file with hash mismatch versus last: " + targetFile.SourceToString());
                existingFile = {};
            }
        }

        if (!existingFile.GetHash().empty())
        {
            if (COptions::GetSingleton().skipUnchanged)
            {
                LOG_DEBUG("skip unchanged: " + targetFile.ToString(), COLOR_DUP_SKIP);
                return;
            }
        }
        else
        {
            existingFile = repository.FindFile(
                { {}, {}, {}, {}, {}, targetFile.GetHash() },
                COptions::GetSingleton().verifyAccessible,
                true);
        }

        if (!existingFile.GetHash().empty())
        {
            if (targetSnapshot.DuplicateFile(existingFile.GetFullPath(), targetFile))
            {
                LOG_DEBUG("duplicated by hash: " + targetFile.ToString(), COLOR_DUP);
                return;
            }
            else
            {
                CLogger::LogError("cannot duplicate: " + targetFile.ToString());
                return;
            }
        }

        CLogger::Log("importing: " + targetFile.SourceToString(), COLOR_IMPORT);

        if (!targetSnapshot.ImportFile(sourcePath, targetFile))
        {
            CLogger::LogError("cannot import, excluding: " + targetFile.SourceToString());
        }
    }
}
