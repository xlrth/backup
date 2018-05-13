#include <string>
#include <vector>
#include <experimental/filesystem>
#include <fstream>

#include "picosha2.h"

#include "backupConfig.h"
#include "sqliteWrapper.h"
#include "helpers.h"



bool        verbose     = false;
FILE*       logFile     = nullptr;

long long   errorCount  = 0;
long long   bytesLinked = 0;
long long   bytesCopied = 0;




struct Snapshot
{
    std::experimental::filesystem::path dir;
    std::experimental::filesystem::path sqliteFile;
    sqlite3*                            dbHandle;
};



////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void BackupSingleRecursive(
    const std::experimental::filesystem::path&  source,
    const std::experimental::filesystem::path&  dest,
    std::vector<Snapshot>&                      snapshots,
    const std::vector<std::string>&             excludes)
{
    std::string sourceStr = source.string();
    std::string destStr = dest.string();

    std::string sourceStrUpper = ToUpper(sourceStr);
    for (const auto& exclude : excludes)
    {
        if (std::string::npos != sourceStrUpper.find(exclude))
        {
            Log(logFile, "excluding " + sourceStr);
            return;
        }
    }

    if (std::experimental::filesystem::is_directory(source))
    {
        VERIFY(std::experimental::filesystem::create_directory(dest));

        for (auto& entry : std::experimental::filesystem::directory_iterator(source))
        {
            BackupSingleRecursive(entry.path(), dest / entry.path().filename(), snapshots, excludes);
        }
    }
    else
    {
        // lock file to prevent others from modification
        FILE* sourceFileHandle = fopen(sourceStr.c_str(), "r");
        if (sourceFileHandle == nullptr)
        {
            Log(logFile, "ERROR: Cannot lock " + sourceStr);
            errorCount++;
            return;
        }
        VERIFY(sourceFileHandle != nullptr);

        long long sourceSize = std::experimental::filesystem::file_size(source);
        auto sourceDateTimePoint = std::experimental::filesystem::last_write_time(source);
        long long sourceDate = sourceDateTimePoint.time_since_epoch().count();

        std::string hash;
        std::string archiveStr;
        if (snapshots.size() > 1)
        {
            auto query = StartQuery(snapshots[snapshots.size() - 2].dbHandle, "select SIZE, DATE, HASH, ARCHIVE from HASH indexed by HASH_idx_file where FILE = \"" + source.u8string() + "\"");
            if (HasData(query))
            {
                long long archSize = ReadInt(query, 0);
                long long archDate = ReadInt(query, 1);
                std::string archHash = ReadString(query, 2);
                if (archSize == sourceSize && archDate == sourceDate)
                {
                    // ASSUME FILE IS SAME
                    hash = archHash;
                    archiveStr = ReadString(query, 3);
                    if (verbose)
                    {
                        Log(logFile, "Skipping hashing: " + sourceStr);
                    }
                }
                VERIFY(!HasData(query));
            }
            FinalizeQuery(query);
        }
        if (hash.empty())
        {
            std::ifstream ifs(sourceStr, std::ios::binary);
            hash = picosha2::hash256_hex_string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
        }

        bool tooManyLinks = false;
        bool linkFailed = false;
        bool linkSucceeded = false;
        std::error_code errorCode;
        std::experimental::filesystem::path archPath;
        if (!archiveStr.empty())
        {
            // try linking with archive of last snapshot
            archPath = snapshots[snapshots.size() - 2].dir / std::experimental::filesystem::u8path(archiveStr);
            std::experimental::filesystem::create_hard_link(archPath, dest, errorCode);
            if (!errorCode)
            {
                linkSucceeded = true;
                bytesLinked += sourceSize;
                if (verbose)
                {
                    Log(logFile, "Linked from last snapshot: " + archPath.string() + " to " + destStr);
                }
            }
        }
        if (!linkSucceeded)
        {
            // try to link all other archives with same hash
            for (int i = (int)snapshots.size() - 1; i >= 0; i--)
            {
                auto query = StartQuery(snapshots[i].dbHandle, "select ARCHIVE from HASH where HASH = \"" + hash + "\"");
                while (HasData(query))
                {
                    archPath = snapshots[i].dir / std::experimental::filesystem::u8path(ReadString(query, 0));
                    std::experimental::filesystem::create_hard_link(archPath, dest, errorCode);
                    if (errorCode)
                    {
                        //  TODO: OVERHEAD checking all lilnks for recurring content
                        //  TODO: OVERHEAD checking all lilnks for recurring content
                        //  TODO: OVERHEAD checking all lilnks for recurring content
                        //  TODO: OVERHEAD checking all lilnks for recurring content
                        if (errorCode.value() == 1142)
                        {
                            tooManyLinks = true;
                            if (verbose)
                            {
                                Log(logFile, "Too many links: " + archPath.string() + " to " + destStr);
                            }
                        }
                        else
                        {
                            linkFailed = true;
                            if (verbose)
                            {
                                Log(logFile, "Linking failed: " + archPath.string() + " to " + destStr + " : " + std::to_string(errorCode.value()) + " " + errorCode.message());
                            }
                        }
                    }
                    else
                    {
                        linkSucceeded = true;
                        bytesLinked += sourceSize;
                        if (verbose)
                        {
                            Log(logFile, "Linked " + archPath.string() + " to " + destStr);
                        }
                        break;
                    }
                }
                FinalizeQuery(query);
                if (linkSucceeded)
                {
                    break;
                }
            }
        }
        if (!linkSucceeded && !tooManyLinks && linkFailed)
        {
            Log(logFile, "ERROR: Cannot link " + archPath.string() + " to " + destStr + " : " + std::to_string(errorCode.value()) + " " + errorCode.message());
            errorCount++;
        }
        if (!linkSucceeded)
        {
            if (!tooManyLinks)
            {
                Log(logFile, "adding file, modified: " + TimeAsString(sourceDateTimePoint) + " size: " + std::to_string(sourceSize) + " path: " + sourceStr);
            }
            else if (verbose)
            {
                Log(logFile, "adding second instance (too many links), modified: " + TimeAsString(sourceDateTimePoint) + " size: " + std::to_string(sourceSize) + " path: " + sourceStr);
            }
            if (!std::experimental::filesystem::copy_file(source, dest))
            {
                Log(logFile, "ERROR: Cannot copy file from " + sourceStr + " to " + destStr);
                errorCount++;
                VERIFY(0 == std::fclose(sourceFileHandle));
                return;
            }
            bytesCopied += sourceSize;
            if (!MakeReadOnly(dest))
            {
                Log(logFile, "ERROR: Cannot make read-only: " + dest.string());
                errorCount++;
            }
        }

        VERIFY(0 == std::fclose(sourceFileHandle));
        RunQuery(snapshots.back().dbHandle, "insert or replace into HASH values (\"" + source.u8string() + "\", " + std::to_string(sourceSize) + ", " + std::to_string(sourceDate) + ", \"" + hash + "\", \"" + dest.u8string().substr(snapshots.back().dir.string().length() + 1) + "\")");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void Backup(
    const std::experimental::filesystem::path&              repository,
    const std::vector<std::experimental::filesystem::path>& sources,
    const std::vector<std::string>&                         excludes)
{
    VERIFY(std::experimental::filesystem::exists(repository)
        && std::experimental::filesystem::is_directory(repository));

    std::vector<Snapshot> snapshotsAll;
    for (auto& p : std::experimental::filesystem::directory_iterator(repository))
    {
        if (!std::experimental::filesystem::is_directory(p))
        {
            continue;
        }
        snapshotsAll.push_back({ p, "", 0 });
    }
    std::sort(snapshotsAll.begin(), snapshotsAll.end(), [](const auto& a, const auto& b) {return a.dir > b.dir; });

    std::vector<Snapshot> snapshots;
    for (int snapshotIdx = 0; snapshotIdx < snapshotsAll.size(); snapshotIdx = std::max(1, snapshotIdx << 1))
    {
        snapshots.push_back(snapshotsAll[snapshotIdx]);
    }
    std::reverse(snapshots.begin(), snapshots.end());

    for (auto& snapshot : snapshots)
    {
        if (std::experimental::filesystem::exists(snapshot.dir / "IN_PROGRESS"))
        {
            throw "snapshot " + snapshot.dir.string() + " is incomplete";
        }
        snapshot.sqliteFile = snapshot.dir / "hash.sqlite";
        if (!std::experimental::filesystem::exists(snapshot.sqliteFile))
        {
            throw "snapshot " + snapshot.dir.string() + " does not have a sqlite file";
        }
    }

    auto newRepoDir = repository / CurrentTimeAsString();

    snapshots.push_back({ newRepoDir, newRepoDir / "hash.sqlite", 0 });

    VERIFY(!std::experimental::filesystem::is_directory(snapshots.back().dir));
    VERIFY(std::experimental::filesystem::create_directory(snapshots.back().dir));

    std::experimental::filesystem::path lockFilePath = snapshots.back().dir / "IN_PROGRESS";
    FILE* lockFile = fopen(lockFilePath.string().c_str(), "w");
    VERIFY(lockFile != nullptr);


    std::experimental::filesystem::path logFilePath = snapshots.back().dir / "log.txt";
    logFile = fopen(logFilePath.string().c_str(), "w");
    VERIFY(logFile != nullptr);

    Log(logFile, "backuping to " + snapshots.back().dir.string());

    for (auto& snapshot : snapshots)
    {
        snapshot.dbHandle = OpenDB(snapshot.sqliteFile.string().c_str(), &snapshot != &snapshots.back());
        RunQuery(snapshot.dbHandle, "pragma cache_size = 1000000");
    }

    RunQuery(snapshots.back().dbHandle, "pragma page_size = 4096");
    RunQuery(snapshots.back().dbHandle, "pragma synchronous = off");
    RunQuery(snapshots.back().dbHandle, "pragma secure_delete = off");
    RunQuery(snapshots.back().dbHandle, "pragma journal_mode = off");
    RunQuery(snapshots.back().dbHandle, "create table HASH (FILE text not null primary key, SIZE integer not null, DATE integer not null, HASH text not null, ARCHIVE text not null)");
    RunQuery(snapshots.back().dbHandle, "create index HASH_idx_hash on HASH (HASH, ARCHIVE)");
    RunQuery(snapshots.back().dbHandle, "create unique index HASH_idx_file on HASH (FILE, SIZE, DATE, HASH, ARCHIVE)");

    for (auto& source : sources)
    {
        std::string modified = source.string();
#ifdef _WIN32
        std::replace(modified.begin(), modified.end(), ':', '#');
        std::replace(modified.begin(), modified.end(), '\\', '#');
#else
        std::replace(modified.begin(), modified.end(), '/', '#');
#endif

        BackupSingleRecursive(source, snapshots.back().dir / modified, snapshots, excludes);
    }

    for (auto& snapshot : snapshots)
    {
        CloseDB(snapshot.dbHandle);
    }

    if (!MakeReadOnly(snapshots.back().sqliteFile))
    {
        Log(logFile, "ERROR: Cannot make read-only: " + snapshots.back().sqliteFile.string());
        errorCount++;
    }

    Log(logFile, "DONE.");
    Log(logFile, "Errors: " + std::to_string(errorCount));
    Log(logFile, "Bytes copied: " + std::to_string(bytesCopied));
    Log(logFile, "Bytes linked: " + std::to_string(bytesLinked));

    VERIFY(0 == std::fclose(logFile));
    logFile = nullptr;

    VERIFY(0 == std::fclose(lockFile));
    VERIFY(std::experimental::filesystem::remove(lockFilePath));

    if (!MakeReadOnly(logFilePath))
    {
        Log(logFile, "ERROR: Cannot make read-only: " + logFilePath.string());
        errorCount++;
    }
}




////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
    try
    {
        if (argc == 1 || argc > 3 || (argc == 3 && ToUpper(argv[2]) != "-VERBOSE"))
        {
            Log(logFile, "usage: backup <config-file> [-verbose]");
            return 1;
        }

        if (argc == 3)
        {
            verbose = true;
        }

        std::experimental::filesystem::path                 repository;
        std::vector<std::experimental::filesystem::path>    sources;
        std::vector<std::string>                            excludes;
        readConfig(argv[1], repository, sources, excludes);

        for (auto& exclude : excludes)
        {
            exclude = ToUpper(exclude);
        }

        Backup(repository, sources, excludes);

        return 0;
    }
    catch (const std::string& exception)
    {
        Log(logFile, std::string("exception: ") + exception);
    }
    catch (...)
    {
        Log(logFile, "unknown exception");
    }

    return 1;
}
