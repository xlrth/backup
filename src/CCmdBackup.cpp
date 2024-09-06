#include "CCmdBackup.h"

#include <algorithm>

#include "COptions.h"
#include "CLogger.h"
#include "Helpers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CCmdBackup::GetUsageSpec()
{
    return "<source-config-file> <repository-dir>";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
COptions CCmdBackup::GetOptionsSpec()
{
    return { { "help", "verbose", "incremental", "always_hash" }, { "suffix" } };
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCmdBackup::PrintHelp()
{
    CLogger::GetInstance().Log(
        "                                                                                \n"
        "BACKUP                                                                          \n"
        "                                                                                \n"
        "Description:                                                                    \n"
        "                                                                                \n"
        "    Creates copies of files and directories, specified in a configuration file. \n"
        "    Copies are created in a new sub-directory (snapshot) in the specified       \n"
        "    repository directory. All already existing snapshots in the same repository \n"
        "    are used for deduplication to reduce required hard disk space.              \n"
        "                                                                                \n"
        "Methods:                                                                        \n"
        "                                                                                \n"
        "    If the file to be backuped is already part of a snapshot, i.e., an identical\n"
        "    file is found in the repository, the copy operation is replaced with a      \n"
        "    hard link operation. A hard link operation creates a normal file that has   \n"
        "    shared content with one or multiple other files. In this case, the new      \n"
        "    backup file shares content with at least one other backup file.             \n"
        "    The search for an existing backup is a two-step process:                    \n"
        "    1. A file with same full path name, size, and modification time is searched.\n"
        "    2. If no file was found, a SHA256 hash is calculated and searched.          \n"
        "    Both searches are done using a file table in each snapshot in the form of a \n"
        "    sqlite database.                                                            \n"
        "                                                                                \n"
        "    Hash calculation can be enforced by specifying --always_hash. This option   \n"
        "    may increase backup duration significantly, but might also reveal file      \n"
        "    changes that did not affect modification time. Please note that files with  \n"
        "    similar signature (used file properties of search step 1) but different     \n"
        "    hashes can (at the moment) NOT be handled and will be skipped.              \n"
        "    --always_hash is therefore usable only for revealing those files. They can  \n"
        "    be backuped after their modification time was changed.                      \n"
        "                                                                                \n"
        "    Files smaller than a file-system-dependent threshold are never hard-linked, \n"
        "    but added via a copy operation.                                             \n"
        "                                                                                \n"
        "Configuration File Format (Windows example):                                    \n"
        "                                                                                \n"
        "    * lines starting with \"*\" are ignored                                     \n"
        "    [sources]                                                                   \n"
        "    C:\\                                                                        \n"
        "    C:\\Data\\OneSpecificFile.txt                                               \n"
        "    ..\\RelativeDataPath                                                        \n"
        "                                                                                \n"
        "    * the \"excludes\" section specifies PATH SUFFIXES of files and directories \n"
        "    * to be excluded in the backup process                                      \n"
        "    [excludes]                                                                  \n"
        "    C:\\Windows                                                                 \n"
        "    :\\pagefile.sys                                                             \n"
        "    \\thumbs.db                                                                 \n"
        "    .tmp                                                                        \n"
        "    _NO_BACKUP                                                                  \n"
        "                                                                                \n"
        "    The configuration file is interpreted as UTF-8.                             \n"
        "                                                                                \n"
        "Restoring:                                                                      \n"
        "                                                                                \n"
        "    Restoring is done by manually copying files/directories from a snapshot     \n"
        "    directory to the desired target.                                            \n"
        "                                                                                \n"
        "    CAUTION: When restoring files, be sure to make copies (rather than a move   \n"
        "    operation within the same partition) to eliminate all hard links.           \n"
        "    Otherwise modification of restored files may lead to modification of other  \n"
        "    restored files or backuped files.                                           \n"
        "                                                                                \n"
        "Partial or full deletion of snapshots:                                          \n"
        "                                                                                \n"
        "    Full deletion of snapshots can be done by deleting the snapshot directory.  \n"
        "    The DISTILL command can be used for distilling/extracting unique files.     \n"
        "    Partial deletion can be done by manual file/directory deletion and          \n"
        "    subsequent execution of the PURGE command.                                  \n"
        "                                                                                \n"
        "Restrictions:                                                                   \n"
        "                                                                                \n"
        "    Following symbolic links or backing up links themselves is not supported.   \n"
        "    Symbolic links in source directory trees will be excluded from backup.      \n"
        "                                                                                \n"
        "Path arguments:                                                                 \n"
        "                                                                                \n"
        "    <source-config-file>    Path to a configuration file, specifiying files or  \n"
        "                            directories to backup, as well as a blacklist with  \n"
        "                            suffixes of file/directory paths to exclude.        \n"
        "                            See also section \"Configuration File Format\".     \n"
        "                                                                                \n"
        "    <repository-dir>        Target directory on the backup storage. Existing    \n"
        "                            snapshots in the same directory are used for        \n"
        "                            deduplication. The repository directory must not    \n"
        "                            contain any directories other than snapshots.       \n"
        "                                                                                \n"
        "Options:                                                                        \n"
        "                                                                                \n"
        "    --help          Displays this help text.                                    \n"
        "                                                                                \n"
        "    --verbose       Higher verbosity of command line logging.                   \n"
        "                                                                                \n"
        "    --incremental   Skips unchanged files: Files will not be added to the newly \n"
        "                    created snapshot, if their signature, i.e. full path name,  \n"
        "                    size and modification time, can be found in any snapshot in \n"
        "                    the repository. This option will reduce backup duration.    \n"
        "                                                                                \n"
        "    --always_hash   Enables calculcation of hash of all files to be backuped.   \n"
        "                    This option may increase backup duration significantly.     \n"
        "                    See also section \"Methods\".                               \n"
        "                                                                                \n"
        "    --suffix=s      Adds the suffix s to the directory name of the new snapshot.\n"
        "                    This option can be used to mark spapshots, for example to   \n"
        "                    distinguish full snapshots from incremental ones.           \n"
    );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCmdBackup::Run(const std::vector<CPath>& paths, const COptions& options)
{
    if (options.GetBool("help"))
    {
        PrintHelp();
        return true;
    }

    if (paths.size() != 2)
    {
        return false;
    }

    mOptions = options;
    mOptions.Log();

    CPath configPath        = paths[0];
    CPath repositoryPath    = paths[1];

    CPath suffix = mOptions.GetString("suffix").empty() ? "" : "_" + mOptions.GetString("suffix");
    CPath snapshotPath = repositoryPath / (CPath(Helpers::CurrentTimeAsString()) += suffix);

    CLogger::GetInstance().EnableDebugLog(mOptions.GetBool("verbose"));

    ReadConfig(configPath);
    PrepareSources(repositoryPath);

    mRepository.Open(repositoryPath, true);

    mTargetSnapshot = std::make_shared<CSnapshot>(snapshotPath, true);
    mTargetSnapshot->SetInProgress();
    mRepository.AttachSnapshot(mTargetSnapshot);

    CLogger::GetInstance().Init(mTargetSnapshot->GetMetaDataPath());
    CLogger::GetInstance().Log("backing up to snapshot: " + mTargetSnapshot->GetAbsolutePath().string());

    for (auto& sourcePath : mSources)
    {
        LOG_DEBUG("processing source: " + sourcePath.string(), COLOR_DEBUG);
        BackupEntryRecursive(sourcePath, FormatTargetPath(sourcePath));
    }

    mTargetSnapshot->ClearInProgress();

    CLogger::GetInstance().Log("finished backing up to snapshot: " + mTargetSnapshot->GetAbsolutePath().string());
    LogStats();
    CRepoFile::StaticLogStats();
    CLogger::GetInstance().Close();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCmdBackup::ReadConfig(const CPath& configPath)
{
    std::ifstream configFileHandle(configPath);
    if (!configFileHandle.is_open())
    {
        throw "cannot open config: " + configPath.string();
    }

    mSources.clear();
    mExcludes.clear();

    std::string line;
    std::string currentSection;
    while (std::getline(configFileHandle, line))
    {
        // trim whitespaces left side
        line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](char ch) { return !std::isspace(ch); }));
        // trim whitespaces right side
        line.erase(std::find_if(line.rbegin(), line.rend(), [](char ch) { return !std::isspace(ch); }).base(), line.end());

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
            if (line.back() != ']')
            {
                throw "invalid line in config file: " + line;
            }
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }
        if (Helpers::ToLower(currentSection) == "sources")
        {
            mSources.push_back(std::filesystem::u8path(line));
            LOG_DEBUG("source: " + mSources.back().string(), COLOR_DEBUG);
        }
        else if (Helpers::ToLower(currentSection) == "excludes")
        {
            mExcludes.push_back(std::filesystem::u8path(line));
            LOG_DEBUG("exclude: " + mExcludes.back().string(), COLOR_DEBUG);
        }
        else
        {
            throw "invalid section in config file: " + currentSection;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCmdBackup::PrepareSources(const CPath& repositoryPath)
{
    // check for empty sources
    if (mSources.empty())
    {
        CLogger::GetInstance().LogWarning("no sources specified, snapshot will be empty");
    }
#ifdef _WIN32
    // check for ambiguous windows paths
    for (auto& source : mSources)
    {
        if (source.native().size() >= 2 && source.native().substr(1, 1) == L":" && source.native().substr(2, 1) != L"\\")
        {
            CLogger::GetInstance().LogWarning("source path lacks backslash after drive letter, rendering it relative. adding a backslash: " + source.string());
            source = std::wstring(source.native()).insert(2, L"\\");
        }
    }
#endif
    // check for source validity
    for (auto& source : mSources)
    {
        if (IsBlacklisted(source))
        {
            throw "source is blacklisted: " + source.string();
        }
        if (std::filesystem::is_symlink(source))
        {
            throw "source is a symbolic link: " + source.string();
        }
        if (!std::filesystem::exists(source))
        {
            throw "source does not exist: " + source.string();
        }
    }
    // normalize source paths
    for (auto& source : mSources)
    {
        if (source.is_absolute())
        {
            source = std::filesystem::canonical(source);
        }
        else
        {
            source = std::filesystem::relative(std::filesystem::canonical(source));
        }
        LOG_DEBUG("canonical source: " + source.string(), COLOR_DEBUG);
    }
    // check for overlapping sources
    for (auto& source1 : mSources)
    {
        for (auto& source2 : mSources)
        {
            if (&source1 == &source2)
            {
                continue;
            }
            if (Helpers::IsPrefixOfPath(std::filesystem::canonical(source1), std::filesystem::canonical(source2)))
            {
                throw "a source is equal to or part of another: " + source1.string() + " and " + source2.string();
            }
        }
    }
    // check for sources being equal to or part of the repository
    for (auto& source : mSources)
    {
        if (Helpers::IsPrefixOfPath(std::filesystem::weakly_canonical(repositoryPath), std::filesystem::canonical(source)))
        {
            throw "a source is equal to or part of the repository: " + source.string();
        }
    }
    // check for sources containing the repository
    for (auto& source : mSources)
    {
        if (Helpers::IsPrefixOfPath(std::filesystem::canonical(source), std::filesystem::weakly_canonical(repositoryPath)))
        {
            auto repoPathSourceRelative = source / std::filesystem::relative(std::filesystem::weakly_canonical(repositoryPath), source);
            if (!IsBlacklisted(repoPathSourceRelative))
            {
                throw "a source is containing the repository: " + source.string();
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CPath CCmdBackup::FormatTargetPath(const CPath& sourcePath)
{
    std::wstring targetStr = sourcePath.wstring();

#ifdef _WIN32
    std::replace(targetStr.begin(), targetStr.end(), L':', L'#');
    std::replace(targetStr.begin(), targetStr.end(), L'\\', L'#');
#else
    std::replace(targetStr.begin(), targetStr.end(), L'/', L'#');
#endif

    if (targetStr == L".")
    {
        targetStr[0] = L'#';
    }

    CPath targetPathRelative = targetStr;

    if (!std::filesystem::exists(mTargetSnapshot->GetAbsolutePath() / targetPathRelative))
    {
        return targetPathRelative;
    }

    CLogger::GetInstance().LogWarning("multiple sources map to the same target path, adding suffix to target: " + targetPathRelative.string());
    for (int number = 1; number < 100; number++)
    {
        targetPathRelative = targetStr += L"_" + std::to_wstring(number);
        if (!std::filesystem::exists(mTargetSnapshot->GetAbsolutePath() / targetPathRelative))
        {
            return targetPathRelative;
        }
    }

    throw "cannot create target path for " + sourcePath.string();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCmdBackup::IsBlacklisted(const CPath& sourcePath)
{
    for (const auto& exclude : mExcludes)
    {
        if (Helpers::IsSuffixOfPath(exclude, sourcePath))
        {
            return true;
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCmdBackup::BackupEntryRecursive(const CPath& sourcePath, const CPath& targetPathRelative)
{
    if (IsBlacklisted(sourcePath))
    {
        CLogger::GetInstance().Log("excluding (blacklisted):   " + sourcePath.string(), COLOR_EXCLUDE);
        mExcludeCountBlacklisted++;
        return;
    }

    switch (std::filesystem::symlink_status(sourcePath).type())
    {
    case std::filesystem::file_type::none:
    case std::filesystem::file_type::not_found:
        CLogger::GetInstance().LogError("cannot access, excluding: " + sourcePath.string());
        return;

    case std::filesystem::file_type::regular:
        BackupFile(sourcePath, targetPathRelative);
        return;

    case std::filesystem::file_type::directory:
        BackupDirectory(sourcePath, targetPathRelative);
        return;

    case std::filesystem::file_type::symlink:
#ifdef _WIN32
    case std::filesystem::file_type::junction:
#endif
        CLogger::GetInstance().Log("excluding (symbolic link): " + sourcePath.string(), COLOR_EXCLUDE);
        mExcludeCountSymlink++;
        return;

    default:
        CLogger::GetInstance().Log("excluding (unknown type):  " + sourcePath.string(), COLOR_EXCLUDE);
        mExcludeCountUnknownType++;
        return;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCmdBackup::BackupDirectory(const CPath& sourcePath, const CPath& targetPathRelative)
{
    if (!mOptions.GetBool("incremental") && !Helpers::CreateDirectory(mTargetSnapshot->GetAbsolutePath() / targetPathRelative))
    {
        CLogger::GetInstance().LogError("cannot create directory, excluding: " + sourcePath.string());
        return;
    }

    for (auto& entry : std::filesystem::directory_iterator(sourcePath))
    {
        BackupEntryRecursive(entry.path(), targetPathRelative / entry.path().filename());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCmdBackup::BackupFile(const CPath& sourcePath, const CPath& targetPathRelative)
{
    CRepoFile targetFile;

    targetFile.SetSourcePath(sourcePath);
    targetFile.SetRelativePath(targetPathRelative);
    targetFile.SetParentPath(mTargetSnapshot->GetAbsolutePath());

    if (!targetFile.ReadSourceProperties())
    {
        CLogger::GetInstance().LogError("cannot access, excluding: " + targetFile.SourceToString());
        return;
    }

    CRepoFile existingFile = mRepository.FindFile(
        { targetFile.GetSourcePath(), targetFile.GetSize(), targetFile.GetTime(), {}, {}, {} },
        false);

    if (existingFile.HasHash() && !mOptions.GetBool("always_hash"))
    {
        // assume file is unchanged, assume hash is identical
        LOG_DEBUG("skipping hashing: " + targetFile.SourceToString(), COLOR_SKIP);
        targetFile.SetHash(existingFile.GetHash());
    }
    else
    {
        LOG_DEBUG("hashing: " + targetFile.SourceToString(), COLOR_HASH);
        if (!LockAndHash(targetFile, existingFile))
        {
            return;
        }
    }

    if (existingFile.HasHash() && mOptions.GetBool("incremental"))
    {
        LOG_DEBUG("skipping linking: " + targetFile.SourceToString(), COLOR_SKIP);
        return;
    }

    if (!existingFile.HasHash() || !existingFile.IsLinkable())
    {
        existingFile = mRepository.FindFile(
            { {}, {}, {}, targetFile.GetHash(), {}, {} },
            true);
    }

    if (existingFile.HasHash())
    {
        if (mTargetSnapshot->InsertFile(existingFile.GetFullPath(), targetFile, existingFile.IsLinkable()))
        {
            LOG_DEBUG("duplicated: " + targetFile.SourceToString(), COLOR_DUP);
            return;
        }
        else
        {
            CLogger::GetInstance().LogError("cannot duplicate, excluding: " + targetFile.SourceToString());
            return;
        }
    }

    VERIFY(targetFile.IsSourceLocked());

    CLogger::GetInstance().Log("importing: " + targetFile.SourceToString(), COLOR_IMPORT);

    if (!mTargetSnapshot->InsertFile(sourcePath, targetFile, false))
    {
        CLogger::GetInstance().LogError("cannot import, excluding: " + targetFile.SourceToString());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCmdBackup::LockAndHash(CRepoFile& targetFile, CRepoFile& existingFile)
{
    // lock file to prevent others from modification. The lock must be held until import is completed
    if (!targetFile.LockSource())
    {
        CLogger::GetInstance().LogError("cannot lock, excluding: " + targetFile.SourceToString());
        return false;
    }

    CRepoFile preLockTargetFile = targetFile;

    // re-read properties after locking, could have changed since first read
    if (!targetFile.ReadSourceProperties())
    {
        CLogger::GetInstance().LogError("cannot access, excluding: " + targetFile.SourceToString());
        return false;
    }

    if (   targetFile.GetSize() != preLockTargetFile.GetSize()
        || targetFile.GetTime() != preLockTargetFile.GetTime())
    {
        // file changed as we backup, repeat the search of an existing file.
        // it is crucial to do a correct signature uniqueness check later in this method
        existingFile = mRepository.FindFile(
            { targetFile.GetSourcePath(), targetFile.GetSize(), targetFile.GetTime(), {}, {}, {} },
            false);
    }

    if (!targetFile.HashSource())
    {
        CLogger::GetInstance().LogError("cannot hash, excluding: " + targetFile.SourceToString());
        return false;
    }
    VERIFY(targetFile.HasHash());

    // signature required to be unique. Cannot import file if it is not.
    if (existingFile.HasHash() && existingFile.GetHash() != targetFile.GetHash())
    {
        CLogger::GetInstance().LogError("file with known signature but hash mismatch. excluding: " + targetFile.SourceToString());
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCmdBackup::LogStats()
{
    if (mExcludeCountBlacklisted > 0)
    {
        CLogger::GetInstance().Log("excluded (blacklisted):   " + std::to_string(mExcludeCountBlacklisted));
    }
    if (mExcludeCountSymlink > 0)
    {
        CLogger::GetInstance().Log("excluded (symbolic link): " + std::to_string(mExcludeCountSymlink));
    }
    if (mExcludeCountUnknownType > 0)
    {
        CLogger::GetInstance().Log("excluded (unknown type):  " + std::to_string(mExcludeCountUnknownType));
    }
}
