#include "CVerify.h"

#include <ostream>
#include <iomanip>
#include <map>

#include "COptions.h"
#include "CLogger.h"
#include "CHelpers.h"
#include "CSnapshot.h"
#include "CRepository.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVerify::Run(const std::vector<CPath>& paths)
{
    CLogger::Init("");
    COptions::GetSingleton().Log();

    struct SFileTableEntry
    {
        CRepoFile           repoFile;
        int                 hardLinkCount;
    };

    std::map<unsigned long long, SFileTableEntry> fileTable;

    std::vector<CPath> snapshotPaths = paths;
    if (snapshotPaths.empty())
    {
        snapshotPaths.push_back(std::experimental::filesystem::current_path());
    }
    if (snapshotPaths.size() == 1 && !CSnapshot::IsValidPath(snapshotPaths.back()))
    {
        snapshotPaths = CRepository::GetSnapshotPaths(snapshotPaths.back());
    }

    for (auto& snapshotPath : snapshotPaths)
    {
        CLogger::Log("verifying snapshot " + snapshotPath.string());

        CSnapshot snapshot(snapshotPath, CSnapshot::EOpenMode::READ);

        snapshot.DBVerify();

        std::vector<CRepoFile> repoFiles = snapshot.FindAllFiles({});
        for (auto& repoFile : repoFiles)
        {
            LOG_DEBUG("verifying: " + repoFile.ToString(), EColor::WHITE);

            if (!repoFile.IsExisting())
            {
                CLogger::LogError("missing: " + repoFile.ToString());
                continue;
            }

            unsigned long long fileSystemIndex;
            int hardLinkCount;

            SFileTableEntry* fileTableEntry = nullptr;

            if (repoFile.ReadFileInfo(fileSystemIndex, hardLinkCount))
            {
                fileTableEntry = &fileTable[fileSystemIndex];
                if (!fileTableEntry->repoFile.GetHash().empty())
                {
                    if (fileTableEntry->repoFile.GetHash() != repoFile.GetHash())
                    {
                        CLogger::LogError("inconsistent hash: " + repoFile.ToString() + " DB: " + repoFile.GetHash() + " repo file: " + fileTableEntry->repoFile.GetHash());
                    }
                    else
                    {
                        LOG_DEBUG("repeating link: " + repoFile.ToString(), COLOR_HASH_SKIP);
                    }
                    continue;
                }
            }

            if (!repoFile.Open())
            {
                CLogger::LogError("file not accessible: " + repoFile.ToString());
            }

            if (COptions::GetSingleton().verifyHashes)
            {
                std::string storedHash = repoFile.GetHash();
                LOG_DEBUG("hashing: " + repoFile.ToString(), COLOR_HASH);
                if (!repoFile.Hash())
                {
                    CLogger::LogError("file not hashable: " + repoFile.ToString());
                    repoFile.SetHash("");
                }
                else if (storedHash != repoFile.GetHash())
                {
                    CLogger::LogError("inconsistent hash: " + repoFile.ToString() + " DB: " + storedHash + " repo file: " + repoFile.GetHash());
                }
            }

            repoFile.Close();

            if (fileTableEntry != nullptr)
            {
                fileTableEntry->repoFile = repoFile;
                fileTableEntry->hardLinkCount = hardLinkCount;
            }
        }

        CLogger::Log("finished verifying snapshot " + snapshotPath.string());
    }

    if (COptions::GetSingleton().writeFileTable)
    {
        auto fileName = "fileTable_" + CHelpers::CurrentTimeAsString() + ".csv";
        std::ofstream fileTableFile(fileName);
        if (!fileTableFile.is_open())
        {
            CLogger::LogWarning("cannot open " + fileName);
        }
        else
        {
            CLogger::Log("writing " + fileName);
            for (auto& fileTableEntry : fileTable)
            {
                fileTableFile
                    << std::setw(20) << fileTableEntry.first << ", "
                    << std::setw(6) << fileTableEntry.second.hardLinkCount << ", "
                    << fileTableEntry.second.repoFile.GetHash() << ", "
                    << fileTableEntry.second.repoFile.ToCSV()
                    << std::endl;
            }
        }
    }

    if (snapshotPaths.size() > 1)
    {
        CLogger::Log("finished verifying " + std::to_string(snapshotPaths.size()) + " snapshots");
    }
    CRepoFile::StaticLogStats();
    CLogger::Close();

    return true;
}
