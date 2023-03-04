#include "CPurge.h"

#include "COptions.h"
#include "CLogger.h"
#include "CHelpers.h"
#include "CSnapshot.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPurge::Run(const std::vector<CPath>& snapshotPaths)
{
    for (auto& snapshotPath : snapshotPaths)
    {
        CLogger::Init(snapshotPath);
        CLogger::Log("purging snapshot " + snapshotPath.string());
        COptions::GetSingleton().Log();

        CSnapshot snapshot(snapshotPath, CSnapshot::EOpenMode::WRITE);

        std::vector<CRepoFile> repoFiles = snapshot.FindAllFiles({});

        for (auto& repoFile : repoFiles)
        {
            if (!repoFile.IsExisting())
            {
                CLogger::Log("purging: " + repoFile.ToString(), COLOR_PURGE);

// HACK FOR OLD DBs: 
repoFile.SetDate(CDate::UNSPECIFIED);
// TODO: ALSO ADD MISSING ASSERT IN SQLITEWRAPPER
// TODO: ALSO REMOVE BACKUP BEFORE WRITING?? 
// TODO: BACKWARD COMPATIBILITY SWITCH??? --> WOLFGANG FRAGEN!
                snapshot.DBDelete(repoFile);
            }
        }

        if (COptions::GetSingleton().compactDB)
        {
            CLogger::Log("compacting DB");
            snapshot.DBCompact();
        }

        CLogger::Log("finished purging snapshot " + snapshotPath.string());
        CRepoFile::StaticLogStats();
        CLogger::Close();
    }

    if (snapshotPaths.size() > 1)
    {
        CLogger::Log("finished purging " + std::to_string(snapshotPaths.size()) + " snapshots");
        CLogger::LogTotalEventCount();
    }

    return true;
}
