#include "CPurge.h"

#include "COptions.h"
#include "CLogger.h"
#include "CHelpers.h"
#include "CSnapshot.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPurge::Run(const std::vector<CPath>& snapshotPaths)
{
    if (snapshotPaths.empty())
    {
        return false;
    }

    for (auto& snapshotPath : snapshotPaths)
    {
        if (!CSnapshot::IsValidPath(snapshotPath))
        {
            throw "snapshot path invalid: " + snapshotPath.string();
        }

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