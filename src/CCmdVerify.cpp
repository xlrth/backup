#include "CCmdVerify.h"

#include "COptions.h"
#include "CLogger.h"
#include "Helpers.h"
#include "CRepository.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CCmdVerify::GetUsageSpec()
{
    return "<repository-dir> | <snapshot-dir> ...";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
COptions CCmdVerify::GetOptionsSpec()
{
    return { { "help", "verbose", "verify_hash", "write_file_table" }, {} };
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCmdVerify::PrintHelp()
{
    CLogger::GetInstance().Log(
        "                                                                                \n"
        "VERIFY                                                                          \n"
        "                                                                                \n"
        "Description:                                                                    \n"
        "                                                                                \n"
        "    Checks consistency of snapshots including database integrity, backup file   \n"
        "    existance and signature uniqueness, optionally verifies content hashes by   \n"
        "    rehashing of all backup files. A CSV file containing a file table with all  \n"
        "    backup files can be generated.                                              \n"
        "                                                                                \n"
        "Path arguments:                                                                 \n"
        "                                                                                \n"
        "    <repository-dir> | <snapshot-dir> ...   Path to a repository or to snapshots\n"
        "                                            to be verified.                     \n"
        "                                                                                \n"
        "Options:                                                                        \n"
        "                                                                                \n"
        "    --help              Displays this help text.                                \n"
        "                                                                                \n"
        "    --verbose           Higher verbosity of command line logging.               \n"
        "                                                                                \n"
        "    --verify_hash       Checks correctness of database hashes by recalculation  \n"
        "                        of hashes of all backup files. This test might increase \n"
        "                        run time significantly. Files which are referenced from \n"
        "                        multiple hard links are checked only once.              \n"
        "                                                                                \n"
        "    --write_file_table  creates a CSV file containing a file table with all     \n"
        "                        backup files and their properties. Files which are      \n"
        "                        referenced with multiple hard links occur only once,    \n"
        "                        and their linkage into snapshots is described.          \n"
    );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCmdVerify::Run(const std::vector<CPath>& paths, const COptions& options)
{
    if (options.GetBool("help"))
    {
        PrintHelp();
        return true;
    }

    if (paths.empty())
    {
        return false;
    }

    CLogger::GetInstance().EnableDebugLog(options.GetBool("verbose"));
    CLogger::GetInstance().Init("");
    options.Log();

    std::vector<CPath> snapshotPaths = paths;
    if (snapshotPaths.size() == 1 && !CSnapshot::StaticIsExsting(snapshotPaths.back()))
    {
        snapshotPaths = CRepository::StaticGetSnapshotPaths(snapshotPaths.back());
    }
    else
    {
        CRepository::StaticValidateSnapshotPaths(snapshotPaths);
    }

    CFileTable fileTable(snapshotPaths);

    for (int snapshotIdx = 0; snapshotIdx < snapshotPaths.size(); snapshotIdx++)
    {
        CSnapshot snapshot(snapshotPaths[snapshotIdx], false);

        CLogger::GetInstance().Log("verifying snapshot: " + snapshot.GetAbsolutePath().string());

        CLogger::GetInstance().Log("verifying database integrity");
        if (!snapshot.DBCheckIntegrity())
        {
            CLogger::GetInstance().LogError("database integrity check failed, skipping file verification");
        }
        else
        {
            CLogger::GetInstance().Log("verifying files");
            VerifyFiles(fileTable, snapshot, snapshotIdx, options);
        }

        CLogger::GetInstance().Log("finished verifying snapshot: " + snapshot.GetAbsolutePath().string());
    }

    CLogger::GetInstance().Log("verifying file signature uniqueness");
    fileTable.VerifyFileSignatureUniqueness();

    if (options.GetBool("write_file_table"))
    {
        fileTable.WriteToFile();
    }

    CLogger::GetInstance().Log("finished verifying " + std::to_string(snapshotPaths.size()) + " snapshots");
    CRepoFile::StaticLogStats();
    CLogger::GetInstance().Close();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCmdVerify::VerifyFiles(CFileTable& fileTable, const CSnapshot& snapshot, int snapshotIdx, const COptions& options)
{
    std::vector<CRepoFile> repoFiles = snapshot.FindAllFiles({});
    for (auto& repoFile : repoFiles)
    {
        LOG_DEBUG("verifying: " + repoFile.ToString(), COLOR_VERIFY);

        CFileTable::CEntry* fileTableEntry = nullptr;

        unsigned long long fileSystemIndex = repoFile.GetFileSystemIndex();
        if (fileSystemIndex == static_cast<unsigned long long>(-1))
        {
            CLogger::GetInstance().LogError("cannot read file system index: " + repoFile.ToString());
        }
        else
        {
            fileTableEntry = &fileTable.GetEntry(fileSystemIndex);
            fileTableEntry->mRefCounts[snapshotIdx]++;
            if (fileTableEntry->mRepoFile.HasHash())
            {
                if (fileTableEntry->mRepoFile.GetHash() != repoFile.GetHash())
                {
                    CLogger::GetInstance().LogError("inconsistent hash: " + repoFile.ToString() + " DB: " + repoFile.GetHash() + " repo file: " + fileTableEntry->mRepoFile.GetHash());
                }
                else
                {
                    LOG_DEBUG("recurring link: " + repoFile.ToString(), COLOR_SKIP);
                }
                continue;
            }
        }

        if (options.GetBool("verify_hash"))
        {
            std::string lastHash = repoFile.GetHash();
            LOG_DEBUG("hashing: " + repoFile.ToString(), COLOR_HASH);
            if (!repoFile.Hash())
            {
                CLogger::GetInstance().LogError("cannot hash: " + repoFile.ToString());
                repoFile.SetHash("ERROR");
            }
            else if (lastHash != repoFile.GetHash())
            {
                CLogger::GetInstance().LogError("inconsistent hash: " + repoFile.ToString() + " DB: " + lastHash + " repo file: " + repoFile.GetHash());
            }
        }

        if (fileTableEntry != nullptr)
        {
            fileTableEntry->mRepoFile = repoFile;
        }
    }
}
