#include "CCmdClone.h"

#include "COptions.h"
#include "CLogger.h"
#include "Helpers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CCmdClone::GetUsageSpec()
{
    return "<source-repo-dir> <target-repo-dir>";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
COptions CCmdClone::GetOptionsSpec()
{
    return { { "help", "verbose", "incremental" }, { "suffix" } };
}


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCmdClone::PrintHelp()
{
    CLogger::GetInstance().Log(
        "                                                                                \n"
        "CLONE                                                                           \n"
        "                                                                                \n"
        "Description:                                                                    \n"
        "                                                                                \n"
        "    Creates a copy of a backup repository. Deduplication is performed the same  \n"
        "    way as the BACKUP command does. Since normal file system copy operations    \n"
        "    often do not preserve hard links, this command can be used to move a        \n"
        "    repository to another partition, or backup the repository itself.           \n"
        "                                                                                \n"
        "Path arguments:                                                                 \n"
        "                                                                                \n"
        "    <source-repo-dir>   Existing repository directory to be copied.             \n"
        "                                                                                \n"
        "    <target-repo-dir>   Target directory on the target backup storage.          \n"
        "                                                                                \n"
        "Options:                                                                        \n"
        "                                                                                \n"
        "    --help          Displays this help text.                                    \n"
        "                                                                                \n"
        "    --verbose       Higher verbosity of command line logging.                   \n"
        "                                                                                \n"
        "    --incremental   Skips cloning files whose signature, i.e. full path name,   \n"
        "                    size and modification time were already cloned into another \n"
        "                    snapshot. The target repository will have a structure as if \n"
        "                    it was created using BACKUP --incremental for all snapshots.\n"
        "                    This option will reduce clone duration.                     \n"
        "                                                                                \n"
        "    --suffix=s      Adds the suffix s to the directory name of all snapshots of \n"
        "                    the target directory.                                       \n"
    );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCmdClone::Run(const std::vector<CPath>& paths, const COptions& options)
{
    if (options.GetBool("help"))
    {
        PrintHelp();
        return true;
    }

    auto repositoryPaths = paths;
    if (repositoryPaths.size() != 2)
    {
        return false;
    }
    CLogger::GetInstance().EnableDebugLog(options.GetBool("verbose"));

    CRepository sourceRepository(repositoryPaths.front(), false);
    CRepository targetRepository(repositoryPaths.back(), true);

    for (auto& sourceSnapshot : sourceRepository.GetAllSnapshots())
    {
        CPath suffix = options.GetString("suffix").empty() ? "" : "_" + options.GetString("suffix");
        CPath targetSnapshotPath = targetRepository.GetAbsolutePath() / (sourceSnapshot->GetAbsolutePath().filename() += suffix);

        std::shared_ptr<CSnapshot> targetSnapshot = std::make_shared<CSnapshot>(targetSnapshotPath, true);
        targetSnapshot->SetInProgress();
        targetRepository.AttachSnapshot(targetSnapshot);

        CLogger::GetInstance().Init(targetSnapshot->GetMetaDataPath());
        options.Log();
        CLogger::GetInstance().Log("cloning to snapshot: " + targetSnapshot->GetAbsolutePath().string());

        std::vector<CRepoFile> sourceFiles = sourceSnapshot->FindAllFiles({});
        for (auto& sourceFile : sourceFiles)
        {
            CloneFile(sourceFile, targetRepository, *targetSnapshot, options);
        }

        targetSnapshot->ClearInProgress();

        CLogger::GetInstance().Log("finished cloning to snapshot: " + targetSnapshot->GetAbsolutePath().string());
        CRepoFile::StaticLogStats();
        CLogger::GetInstance().Close();
    }

    if (sourceRepository.GetAllSnapshots().size() > 1)
    {
        CLogger::GetInstance().Log("finished cloning " + std::to_string(sourceRepository.GetAllSnapshots().size()) + " snapshots");
        CLogger::GetInstance().LogTotalEventCount();
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCmdClone::CloneFile(const CRepoFile& sourceFile, CRepository& targetRepository, CSnapshot& targetSnapshot, const COptions& options)
{
    CRepoFile targetFile
    {
        sourceFile.GetSourcePath(),
        sourceFile.GetSize(),
        sourceFile.GetTime(),
        sourceFile.GetHash(),
        sourceFile.GetRelativePath(),
        targetSnapshot.GetAbsolutePath()
    };

    CRepoFile existingFile = targetRepository.FindFile(
        { targetFile.GetSourcePath(), targetFile.GetSize(), targetFile.GetTime(), {}, {}, {} },
        false);

    if (existingFile.HasHash() && existingFile.GetHash() != targetFile.GetHash())
    {
        CLogger::GetInstance().LogError("file with known signature but hash mismatch. excluding: " + targetFile.ToString());
        return;
    }

    if (existingFile.HasHash() && options.GetBool("incremental"))
    {
        LOG_DEBUG("skipping linking: " + targetFile.ToString(), COLOR_SKIP);
        return;
    }

    if (!existingFile.HasHash() || !existingFile.IsLinkable())
    {
        existingFile = targetRepository.FindFile(
            { {}, {}, {}, targetFile.GetHash(), {}, {} },
            true);
    }

    if (existingFile.HasHash())
    {
        if (targetSnapshot.InsertFile(existingFile.GetFullPath(), targetFile, true))
        {
            LOG_DEBUG("duplicated: " + targetFile.ToString(), COLOR_DUP);
            return;
        }
        else
        {
            CLogger::GetInstance().LogError("cannot duplicate, excluding: " + targetFile.ToString());
            return;
        }
    }

    CLogger::GetInstance().Log("cloning: " + targetFile.ToString(), COLOR_CLONE);

    if (!targetSnapshot.InsertFile(sourceFile.GetFullPath(), targetFile, false))
    {
        CLogger::GetInstance().LogError("cannot clone, excluding: " + targetFile.ToString());
    }
}
