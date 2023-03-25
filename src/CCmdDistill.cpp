#include "CCmdDistill.h"

#include "COptions.h"
#include "CLogger.h"
#include "Helpers.h"
#include "CSnapshot.h"
#include "CRepository.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CCmdDistill::GetUsageSpec()
{
    return "<snapshot-dir> ... ";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
COptions CCmdDistill::GetOptionsSpec()
{
    return { { "help", "verbose", "compact_db" }, {} };
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCmdDistill::PrintHelp()
{
    CLogger::GetInstance().Log(
        "                                                                                \n"
        "DISTILL                                                                         \n"
        "                                                                                \n"
        "Description:                                                                    \n"
        "                                                                                \n"
        "    Deletes all files that are shared with other snapshots of the same          \n"
        "    repository, resulting in an isolation of all files which are unique in the  \n"
        "    target snapshot. File identity is determined via file content only,         \n"
        "    properties like path and name are ignored. Directories will be deleted if   \n"
        "    they become empty, or have already been empty.                              \n"
        "                                                                                \n"
        "Application:                                                                    \n"
        "                                                                                \n"
        "    This command can be used during manual sparsening of a repository. The user \n"
        "    may want to delete entire snapshots, but may want to have the option to     \n"
        "    confirm final deletion of files' last occurrences in the repository.        \n"
        "                                                                                \n"
        "Path arguments:                                                                 \n"
        "                                                                                \n"
        "    <snapshot-dir> ...  Paths to one or multiple snapshots to be distilled.     \n"
        "                        Snapshots are required to be in the same repository.    \n"
        "                                                                                \n"
        "Options:                                                                        \n"
        "                                                                                \n"
        "    --help          Displays this help text.                                    \n"
        "                                                                                \n"
        "    --verbose       Higher verbosity of command line logging.                   \n"
        "                                                                                \n"
        "    --compact_db    Rewrites the database, resulting in minor access time and   \n"
        "                    size reduction, but may take some seconds to run.           \n"
    );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCmdDistill::Run(const std::vector<CPath>& paths, const COptions& options)
{
    if (options.GetBool("help"))
    {
        PrintHelp();
        return true;
    }

    auto snapshotPaths = paths;
    if (snapshotPaths.empty())
    {
        return false;
    }

    CLogger::GetInstance().EnableDebugLog(options.GetBool("verbose"));

    CRepository::StaticValidateSnapshotPaths(snapshotPaths);
    CRepository repository = CRepository::StaticGetParentRepository(snapshotPaths);

    for (auto& snapshotPath : snapshotPaths)
    {
        std::shared_ptr<CSnapshot> snapshot = repository.DetachSnapshot(snapshotPath);
        snapshot->SetInProgress();

        CLogger::GetInstance().Init(snapshot->GetMetaDataPath());
        options.Log();
        CLogger::GetInstance().Log("distilling snapshot: " + snapshot->GetAbsolutePath().string());

        std::vector<CRepoFile> repoFiles = snapshot->FindAllFiles({});
        for (auto& repoFile : repoFiles)
        {
            CRepoFile existingFile = repository.FindFile(
                { {}, {}, {}, repoFile.GetHash(), {}, {} },
                false);
                
            if (!existingFile.HasHash())
            {
                CLogger::GetInstance().Log("distilling: " + repoFile.ToString(), COLOR_DISTILL);
                continue;
            }
            
            if (!snapshot->DeleteFile(repoFile))
            {
                CLogger::GetInstance().LogError("cannot delete: " + repoFile.ToString());
            }
        }

        CLogger::GetInstance().Log("deleting empty directories");
        Helpers::DeleteEmptyDirectories(snapshotPath);

        if (options.GetBool("compact_db"))
        {
            CLogger::GetInstance().Log("compacting DB");
            snapshot->DBCompact();
        }

        snapshot->ClearInProgress();

        CLogger::GetInstance().Log("finished distilling snapshot: " + snapshot->GetAbsolutePath().string());
        CRepoFile::StaticLogStats();
        CLogger::GetInstance().Close();
    }

    if (snapshotPaths.size() > 1)
    {
        CLogger::GetInstance().Log("finished distilling " + std::to_string(snapshotPaths.size()) + " snapshots");
        CLogger::GetInstance().LogTotalEventCount();
    }

    return true;
}
