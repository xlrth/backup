#include "CCmdPurge.h"

#include "COptions.h"
#include "CLogger.h"
#include "Helpers.h"
#include "CRepository.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CCmdPurge::GetUsageSpec()
{
    return "<snapshot-dir> ... ";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
COptions CCmdPurge::GetOptionsSpec()
{
    return { { "help", "verbose", "compact_db" }, {} };
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCmdPurge::PrintHelp()
{
    CLogger::GetInstance().Log(
        "                                                                                \n"
        "PURGE                                                                           \n"
        "                                                                                \n"
        "Description:                                                                    \n"
        "                                                                                \n"
        "    Removes all database entries from snapshot databases that reference missing \n"
        "    backup files.                                                               \n"
        "                                                                                \n"
        "Application:                                                                    \n"
        "                                                                                \n"
        "    In case snapshots were modified using manual file deletions, this           \n"
        "    command can remove all database entries referencing missing files.          \n"
        "                                                                                \n"
        "Path arguments:                                                                 \n"
        "                                                                                \n"
        "    <snapshot-dir> ...  Paths to one or multiple snapshots to be purged.        \n"
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
bool CCmdPurge::Run(const std::vector<CPath>& paths, const COptions& options)
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

    for (auto& snapshotPath : snapshotPaths)
    {
        CSnapshot snapshot(snapshotPath, false);
        snapshot.SetInProgress();

        CLogger::GetInstance().Init(snapshot.GetMetaDataPath());
        options.Log();
        CLogger::GetInstance().Log("purging snapshot: " + snapshot.GetAbsolutePath().string());

        std::vector<CRepoFile> repoFiles = snapshot.FindAllFiles({});

        for (auto& repoFile : repoFiles)
        {
            if (!repoFile.IsExisting())
            {
                CLogger::GetInstance().Log("purging: " + repoFile.ToString(), COLOR_PURGE);

                snapshot.DBDelete(repoFile);
            }
            LOG_DEBUG("skipping: " + repoFile.ToString(), COLOR_SKIP);
        }

        if (options.GetBool("compact_db"))
        {
            CLogger::GetInstance().Log("compacting DB");
            snapshot.DBCompact();
        }

        snapshot.ClearInProgress();

        CLogger::GetInstance().Log("finished purging snapshot: " + snapshot.GetAbsolutePath().string());
        CRepoFile::StaticLogStats();
        CLogger::GetInstance().Close();
    }

    if (snapshotPaths.size() > 1)
    {
        CLogger::GetInstance().Log("finished purging " + std::to_string(snapshotPaths.size()) + " snapshots");
        CLogger::GetInstance().LogTotalEventCount();
    }

    return true;
}
