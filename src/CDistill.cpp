#include "CDistill.h"

#include "COptions.h"
#include "CLogger.h"
#include "CHelpers.h"
#include "CSnapshot.h"
#include "CRepository.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDistill::Run(const std::vector<CPath>& snapshotPaths)
{
    if (snapshotPaths.empty())
    {
        return false;
    }

    CRepository repository(snapshotPaths.back().parent_path());

    for (auto& snapshotPath : snapshotPaths)
    {
        if (!CSnapshot::IsValidPath(snapshotPath))
        {
            throw "snapshot path invalid: " + snapshotPath.string();
        }
        repository.CloseSnapshot(snapshotPath);
        repository.ReorderSnapshotsByDistance(snapshotPath);

        CLogger::Init(snapshotPath);
        CLogger::Log("distilling snapshot " + snapshotPath.string());
        COptions::GetSingleton().Log();

        CSnapshot snapshot(snapshotPath, CSnapshot::EOpenMode::WRITE);

        std::vector<CRepoFile> repoFiles = snapshot.FindAllFiles({});
        for (auto& repoFile : repoFiles)
        {
            CRepoFile existingFile = repository.FindFile(
                { {}, {}, {}, {}, {}, repoFile.GetHash() },
                COptions::GetSingleton().verifyAccessible,
                false);
                
            if (existingFile.GetHash().empty())
            {
                CLogger::Log("distilled: " + repoFile.ToString(), COLOR_DISTILL);
                continue;
            }
            
            if (!snapshot.DeleteFile(repoFile))
            {
                CLogger::LogError("cannot delete: " + repoFile.ToString());
            }
        }

        CLogger::Log("deleting empty directories");
        CHelpers::DeleteEmptyDirectories(snapshotPath);

        if (COptions::GetSingleton().compactDB)
        {
            CLogger::Log("compacting DB");
            snapshot.DBCompact();
        }

        CLogger::Log("finished distilling snapshot " + snapshotPath.string());
        CRepoFile::StaticLogStats();
        CLogger::Close();

        snapshot.Close();
        repository.OpenSnapshot(snapshotPath);
    }

    if (snapshotPaths.size() > 1)
    {
        CLogger::Log("finished distilling " + std::to_string(snapshotPaths.size()) + " snapshots");
        CLogger::LogTotalEventCount();
    }

    return true;
}
