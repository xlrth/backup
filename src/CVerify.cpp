#include "CVerify.h"

#include <iostream>
#include <iomanip>
#include <map>

#include "CPath.h"
#include "CRepoFile.h"
#include "Helpers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVerify::Verify(const std::vector<CPath>& snapshotPaths)
{
    InitLog("");

    COptions::GetSingleton().Log();

    struct SFileTableEntry
    {
        CRepoFile           repoFile;
        int                 hardLinkCount;
    };

    std::map<unsigned long long, SFileTableEntry> fileTable;

    for (auto& snapshotPath : snapshotPaths)
    {
        Log("verifying " + snapshotPath.string());

        CRepository repository;
        repository.OpenSnapshot(snapshotPath);

        std::vector<CRepoFile> repoFiles = repository.EnumRepoFiles(0);

        for (auto& repoFile : repoFiles)
        {
            if (!repoFile.IsExisting())
            {
                LogWarning("missing: " + repoFile.ToString());
                continue;
            }

            unsigned long long fileSystemIndex;
            int hardLinkCount;

            SFileTableEntry* fileTableEntry = nullptr;

            if (repoFile.GetFileInfo(fileSystemIndex, hardLinkCount))
            {
                fileTableEntry = &fileTable[fileSystemIndex];
                if (!fileTableEntry->repoFile.GetHash().empty())
                {
                    if (fileTableEntry->repoFile.GetHash() != repoFile.GetHash())
                    {
                        LogError("inconsistent hash: " + repoFile.ToString());
                    }
                    else
                    {
                        LogDebug("repeating link: " + repoFile.ToString(), COLOR_HASH_SKIP);
                    }
                    continue;
                }
            }

            if (COptions::GetSingleton().verifyHashes)
            {
                std::string storedHash = repoFile.GetHash();
                LogDebug("hashing: " + repoFile.ToString(), COLOR_HASH);
                if (!repoFile.Hash())
                {
                    LogError("cannot hash: " + repoFile.ToString());
                    repoFile.SetHash("");
                }
                else if (storedHash != repoFile.GetHash())
                {
                    LogError("inconsistent hash: " + repoFile.ToString());
                }
            }

            if (fileTableEntry != nullptr)
            {
                fileTableEntry->repoFile = repoFile;
                fileTableEntry->hardLinkCount = hardLinkCount;
            }
        }
    }

    if (COptions::GetSingleton().writeFileTable)
    {
        auto fileName = "fileTable_" + CurrentTimeAsString() + ".csv";
        std::ofstream fileTableFile(fileName);
        if (!fileTableFile.is_open())
        {
            LogWarning("cannot open " + fileName);
        }
        else
        {
            Log("writing " + fileName);
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

    CRepoFile::StaticLogStats();
    CloseLog();
}
