#include <fstream>

#include "backup.h"
#include "backupConfig.h"
#include "sqliteWrapper.h"
#include "helpers.h"
#include "picosha2.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// typedefs
struct Snapshot
{
    std::experimental::filesystem::path dir;
    std::experimental::filesystem::path sqliteFile;
    sqlite3*                            dbHandle;
};


////////////////////////////////////////////////////////////////////////////////////////////////////
// globals
static std::vector<Snapshot>       gSnapshots;
static std::vector<std::string>    gExcludes;
static bool                        gVerbose     = false;
static FILE*                       gLogFile     = nullptr;

static long long   gErrorCount  = 0;
static long long   gBytesLinked = 0;
static long long   gBytesCopied = 0;


////////////////////////////////////////////////////////////////////////////////////////////////////
// functions


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
FILE* GetLogFile()
{
    // Hack to use the log file in case of an exception
    return gLogFile;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
static void BackupSingleRecursive(
    const std::experimental::filesystem::path&  source,
    const std::experimental::filesystem::path&  dest)
{
    std::string sourceStr = source.string();
    std::string destStr = dest.string();

    std::string sourceStrUpper = ToUpper(sourceStr);
    for (const auto& exclude : gExcludes)
    {
        if (std::string::npos != sourceStrUpper.find(exclude))
        {
            Log(gLogFile, "excluding " + sourceStr);
            return;
        }
    }

    if (std::experimental::filesystem::is_directory(source))
    {
        VERIFY(std::experimental::filesystem::create_directory(dest));

        for (auto& entry : std::experimental::filesystem::directory_iterator(source))
        {
            BackupSingleRecursive(entry.path(), dest / entry.path().filename());
        }
    }
    else
    {
        // lock file to prevent others from modification
        FILE* sourceFileHandle = fopen(sourceStr.c_str(), "r");
        if (sourceFileHandle == nullptr)
        {
            Log(gLogFile, "ERROR: Cannot lock " + sourceStr);
            gErrorCount++;
            return;
        }
        VERIFY(sourceFileHandle != nullptr);

        long long sourceSize = std::experimental::filesystem::file_size(source);
        auto sourceDateTimePoint = std::experimental::filesystem::last_write_time(source);
        long long sourceDate = sourceDateTimePoint.time_since_epoch().count();

        std::string hash;
        std::string archiveStr;
        if (gSnapshots.size() > 1)
        {
            auto query = StartQuery(gSnapshots[gSnapshots.size() - 2].dbHandle, "select SIZE, DATE, HASH, ARCHIVE from HASH indexed by HASH_idx_file where FILE = \"" + source.u8string() + "\"");
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
                    if (gVerbose)
                    {
                        Log(gLogFile, "Skipping hashing: " + sourceStr);
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
            archPath = gSnapshots[gSnapshots.size() - 2].dir / std::experimental::filesystem::u8path(archiveStr);
            std::experimental::filesystem::create_hard_link(archPath, dest, errorCode);
            if (!errorCode)
            {
                linkSucceeded = true;
                gBytesLinked += sourceSize;
                if (gVerbose)
                {
                    Log(gLogFile, "Linked from last snapshot: " + archPath.string() + " to " + destStr);
                }
            }
        }
        if (!linkSucceeded)
        {
            // try to link all other archives with same hash
            for (int i = (int)gSnapshots.size() - 1; i >= 0; i--)
            {
                auto query = StartQuery(gSnapshots[i].dbHandle, "select ARCHIVE from HASH where HASH = \"" + hash + "\"");
                while (HasData(query))
                {
                    archPath = gSnapshots[i].dir / std::experimental::filesystem::u8path(ReadString(query, 0));
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
                            if (gVerbose)
                            {
                                Log(gLogFile, "Too many links: " + archPath.string() + " to " + destStr);
                            }
                        }
                        else
                        {
                            linkFailed = true;
                            if (gVerbose)
                            {
                                Log(gLogFile, "Linking failed: " + archPath.string() + " to " + destStr + " : " + std::to_string(errorCode.value()) + " " + errorCode.message());
                            }
                        }
                    }
                    else
                    {
                        linkSucceeded = true;
                        gBytesLinked += sourceSize;
                        if (gVerbose)
                        {
                            Log(gLogFile, "Linked " + archPath.string() + " to " + destStr);
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
            Log(gLogFile, "ERROR: Cannot link " + archPath.string() + " to " + destStr + " : " + std::to_string(errorCode.value()) + " " + errorCode.message());
            gErrorCount++;
        }
        if (!linkSucceeded)
        {
            if (!tooManyLinks)
            {
                Log(gLogFile, "adding file, modified: " + TimeAsString(sourceDateTimePoint) + " size: " + std::to_string(sourceSize) + " path: " + sourceStr);
            }
            else if (gVerbose)
            {
                Log(gLogFile, "adding second instance (too many links), modified: " + TimeAsString(sourceDateTimePoint) + " size: " + std::to_string(sourceSize) + " path: " + sourceStr);
            }
            if (!std::experimental::filesystem::copy_file(source, dest))
            {
                Log(gLogFile, "ERROR: Cannot copy file from " + sourceStr + " to " + destStr);
                gErrorCount++;
                VERIFY(0 == std::fclose(sourceFileHandle));
                return;
            }
            gBytesCopied += sourceSize;
            if (!MakeReadOnly(dest))
            {
                Log(gLogFile, "ERROR: Cannot make read-only: " + dest.string());
                gErrorCount++;
            }
        }

        VERIFY(0 == std::fclose(sourceFileHandle));
        RunQuery(gSnapshots.back().dbHandle, "insert or replace into HASH values (\"" + source.u8string() + "\", " + std::to_string(sourceSize) + ", " + std::to_string(sourceDate) + ", \"" + hash + "\", \"" + dest.u8string().substr(gSnapshots.back().dir.string().length() + 1) + "\")");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void Backup(
    const std::experimental::filesystem::path&              repository,
    const std::vector<std::experimental::filesystem::path>& sources,
    const std::vector<std::string>&                         excludes,
    bool                                                    verbose)
{
    gExcludes = excludes;
    gVerbose = verbose;

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

    for (int snapshotIdx = 0; snapshotIdx < snapshotsAll.size(); snapshotIdx = std::max(1, snapshotIdx << 1))
    {
        gSnapshots.push_back(snapshotsAll[snapshotIdx]);
    }
    std::reverse(gSnapshots.begin(), gSnapshots.end());

    for (auto& snapshot : gSnapshots)
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

    gSnapshots.push_back({ newRepoDir, newRepoDir / "hash.sqlite", 0 });

    VERIFY(!std::experimental::filesystem::is_directory(gSnapshots.back().dir));
    VERIFY(std::experimental::filesystem::create_directory(gSnapshots.back().dir));

    std::experimental::filesystem::path lockFilePath = gSnapshots.back().dir / "IN_PROGRESS";
    FILE* lockFile = fopen(lockFilePath.string().c_str(), "w");
    VERIFY(lockFile != nullptr);


    std::experimental::filesystem::path logFilePath = gSnapshots.back().dir / "log.txt";
    gLogFile = fopen(logFilePath.string().c_str(), "w");
    VERIFY(gLogFile != nullptr);

    Log(gLogFile, "backuping to " + gSnapshots.back().dir.string());

    for (auto& snapshot : gSnapshots)
    {
        snapshot.dbHandle = OpenDB(snapshot.sqliteFile.string().c_str(), &snapshot != &gSnapshots.back());
        RunQuery(snapshot.dbHandle, "pragma cache_size = 1000000");
    }

    RunQuery(gSnapshots.back().dbHandle, "pragma page_size = 4096");
    RunQuery(gSnapshots.back().dbHandle, "pragma synchronous = off");
    RunQuery(gSnapshots.back().dbHandle, "pragma secure_delete = off");
    RunQuery(gSnapshots.back().dbHandle, "pragma journal_mode = off");
    RunQuery(gSnapshots.back().dbHandle, "create table HASH (FILE text not null primary key, SIZE integer not null, DATE integer not null, HASH text not null, ARCHIVE text not null)");
    RunQuery(gSnapshots.back().dbHandle, "create index HASH_idx_hash on HASH (HASH, ARCHIVE)");
    RunQuery(gSnapshots.back().dbHandle, "create unique index HASH_idx_file on HASH (FILE, SIZE, DATE, HASH, ARCHIVE)");

    for (auto& source : sources)
    {
        std::string modified = source.string();
#ifdef _WIN32
        std::replace(modified.begin(), modified.end(), ':', '#');
        std::replace(modified.begin(), modified.end(), '\\', '#');
#else
        std::replace(modified.begin(), modified.end(), '/', '#');
#endif

        BackupSingleRecursive(source, gSnapshots.back().dir / modified);
    }

    for (auto& snapshot : gSnapshots)
    {
        CloseDB(snapshot.dbHandle);
    }

    if (!MakeReadOnly(gSnapshots.back().sqliteFile))
    {
        Log(gLogFile, "ERROR: Cannot make read-only: " + gSnapshots.back().sqliteFile.string());
        gErrorCount++;
    }

    Log(gLogFile, "DONE.");
    Log(gLogFile, "Errors: " + std::to_string(gErrorCount));
    Log(gLogFile, "Bytes copied: " + std::to_string(gBytesCopied));
    Log(gLogFile, "Bytes linked: " + std::to_string(gBytesLinked));

    VERIFY(0 == std::fclose(gLogFile));
    gLogFile = nullptr;

    VERIFY(0 == std::fclose(lockFile));
    VERIFY(std::experimental::filesystem::remove(lockFilePath));

    if (!MakeReadOnly(logFilePath))
    {
        Log(gLogFile, "ERROR: Cannot make read-only: " + logFilePath.string());
        gErrorCount++;
    }
}
