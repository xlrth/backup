#include <fstream>
#include <chrono>
#include <thread>

#include "Backup.h"
#include "BackupConfig.h"
#include "SqliteWrapper.h"

#include "Helpers.h"
#include "picosha2.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// typedefs
struct Snapshot
{
    std::experimental::filesystem::path dir;
    std::experimental::filesystem::path sqliteFile;
    SqliteWrapper                       db;
};


////////////////////////////////////////////////////////////////////////////////////////////////////
// globals
static std::vector<Snapshot>       gSnapshots;
static std::vector<std::string>    gExcludes;
static bool                        gVerbose     = false;
static std::ofstream               gLogFileHandle;

static long long   gErrorCount  = 0;
static long long   gBytesLinked = 0;
static long long   gBytesCopied = 0;


////////////////////////////////////////////////////////////////////////////////////////////////////
// functions


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::ofstream& GetLogFile()
{
    // Hack to use the log file in case of an exception
    return gLogFileHandle;
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
            Log(gLogFileHandle, "excluding " + sourceStr);
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
        std::ifstream sourceFileHandle;
        // lock file to prevent others from modification, sleep and retry in case of failure
        for (int i = 0; i < 10; i++)
        {
            sourceFileHandle = std::ifstream(sourceStr, std::ios::binary);
            if (sourceFileHandle.is_open())
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!sourceFileHandle.is_open())
        {
            Log(gLogFileHandle, "ERROR: Cannot lock " + sourceStr);
            gErrorCount++;
            return;
        }

        long long sourceSize = std::experimental::filesystem::file_size(source);
        auto sourceDateTimePoint = std::experimental::filesystem::last_write_time(source);
        long long sourceDate = sourceDateTimePoint.time_since_epoch().count();

        std::string hash;
        std::string archiveStr;
        if (gSnapshots.size() > 1)
        {
            auto query = gSnapshots[gSnapshots.size() - 2].db.StartQuery(
                "select SIZE, DATE, HASH, ARCHIVE from HASH indexed by HASH_idx_file where FILE = \"" + source.u8string() + "\"");
            if (query.HasData())
            {
                long long archSize = query.ReadInt(0);
                long long archDate = query.ReadInt(1);
                std::string archHash = query.ReadString(2);
                if (archSize == sourceSize && archDate == sourceDate)
                {
                    // ASSUME FILE IS SAME
                    hash = archHash;
                    archiveStr = query.ReadString(3);
                    if (gVerbose)
                    {
                        Log(gLogFileHandle, "Skipping hashing: " + sourceStr);
                    }
                }
                VERIFY(!query.HasData());
            }
        }
        if (hash.empty())
        {
            hash = picosha2::hash256_hex_string(std::istreambuf_iterator<char>(sourceFileHandle), std::istreambuf_iterator<char>());
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
                    Log(gLogFileHandle, "Linked from last snapshot: " + archPath.string() + " to " + destStr);
                }
            }
        }
        if (!linkSucceeded)
        {
            // try to link all other archives with same hash
            for (int i = (int)gSnapshots.size() - 1; i >= 0; i--)
            {
                auto query = gSnapshots[i].db.StartQuery("select ARCHIVE from HASH where HASH = \"" + hash + "\"");
                while (query.HasData())
                {
                    archPath = gSnapshots[i].dir / std::experimental::filesystem::u8path(query.ReadString(0));
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
                                Log(gLogFileHandle, "Too many links: " + archPath.string() + " to " + destStr);
                            }
                        }
                        else
                        {
                            linkFailed = true;
                            if (gVerbose)
                            {
                                Log(gLogFileHandle, "Linking failed: " + archPath.string() + " to " + destStr + " : " + std::to_string(errorCode.value()) + " " + errorCode.message());
                            }
                        }
                    }
                    else
                    {
                        linkSucceeded = true;
                        gBytesLinked += sourceSize;
                        if (gVerbose)
                        {
                            Log(gLogFileHandle, "Linked " + archPath.string() + " to " + destStr);
                        }
                        break;
                    }
                }
                if (linkSucceeded)
                {
                    break;
                }
            }
        }
        if (!linkSucceeded && !tooManyLinks && linkFailed)
        {
            Log(gLogFileHandle, "ERROR: Cannot link " + archPath.string() + " to " + destStr + " : " + std::to_string(errorCode.value()) + " " + errorCode.message());
            gErrorCount++;
        }
        if (!linkSucceeded)
        {
            if (!tooManyLinks)
            {
                Log(gLogFileHandle, "adding file, modified: " + TimeAsString(sourceDateTimePoint) + " size: " + std::to_string(sourceSize) + " path: " + sourceStr);
            }
            else if (gVerbose)
            {
                Log(gLogFileHandle, "adding second instance (too many links), modified: " + TimeAsString(sourceDateTimePoint) + " size: " + std::to_string(sourceSize) + " path: " + sourceStr);
            }
            if (!std::experimental::filesystem::copy_file(source, dest, std::experimental::filesystem::copy_options::copy_symlinks))
            {
                Log(gLogFileHandle, "ERROR: Cannot copy file from " + sourceStr + " to " + destStr);
                gErrorCount++;
                return;
            }
#ifndef _WIN32 // fixes modification date not being copied under linux
            auto lastWriteTime = std::experimental::filesystem::last_write_time(source, errorCode);
            if (errorCode)
            {
                Log(gLogFileHandle, "ERROR: Cannot read modification date from " + sourceStr);
                gErrorCount++;
            }
            else
            {
                std::experimental::filesystem::last_write_time(dest, lastWriteTime, errorCode);
                if (errorCode)
                {
                    Log(gLogFileHandle, "ERROR: Cannot set modification date of " + destStr);
                    gErrorCount++;
                }
            }
#endif
            gBytesCopied += sourceSize;
            if (!MakeReadOnly(dest))
            {
                Log(gLogFileHandle, "ERROR: Cannot make read-only: " + dest.string());
                gErrorCount++;
            }
        }

        gSnapshots.back().db.RunQuery(
            "insert or replace into HASH values (\""
            + source.u8string() + "\", " + std::to_string(sourceSize) + ", "
            + std::to_string(sourceDate) + ", "
            + "\"" + hash + "\", "
            + "\"" + dest.u8string().substr(gSnapshots.back().dir.string().length() + 1) + "\""
            + ")");
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
        snapshotsAll.push_back({ p });
    }
    std::sort(snapshotsAll.begin(), snapshotsAll.end(), [](const auto& a, const auto& b) {return a.dir > b.dir; });

    for (int snapshotIdx = 0; snapshotIdx < snapshotsAll.size(); snapshotIdx = std::max(1, snapshotIdx << 1))
    {
        gSnapshots.push_back(std::move(snapshotsAll[snapshotIdx]));
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

    gSnapshots.push_back({ newRepoDir, newRepoDir / "hash.sqlite" });

    VERIFY(!std::experimental::filesystem::is_directory(gSnapshots.back().dir));
    VERIFY(std::experimental::filesystem::create_directory(gSnapshots.back().dir));

    std::experimental::filesystem::path lockFilePath = gSnapshots.back().dir / "IN_PROGRESS";
    std::ofstream lockFileHandle(lockFilePath.string());
    VERIFY(lockFileHandle.is_open());


    std::experimental::filesystem::path logFilePath = gSnapshots.back().dir / "log.txt";
    gLogFileHandle = std::ofstream(logFilePath.string());
    VERIFY(gLogFileHandle.is_open());

    Log(gLogFileHandle, "backuping to " + gSnapshots.back().dir.string());

    for (auto& snapshot : gSnapshots)
    {
        snapshot.db = SqliteWrapper(snapshot.sqliteFile.string().c_str(), &snapshot != &gSnapshots.back());
        snapshot.db.RunQuery("pragma cache_size = 1000000");
    }

    gSnapshots.back().db.RunQuery("pragma page_size = 4096");
    gSnapshots.back().db.RunQuery("pragma synchronous = off");
    gSnapshots.back().db.RunQuery("pragma secure_delete = off");
    gSnapshots.back().db.RunQuery("pragma journal_mode = off");
    gSnapshots.back().db.RunQuery("create table HASH (FILE text not null primary key, SIZE integer not null, DATE integer not null, HASH text not null, ARCHIVE text not null)");
    gSnapshots.back().db.RunQuery("create index HASH_idx_hash on HASH (HASH, ARCHIVE)");
    gSnapshots.back().db.RunQuery("create unique index HASH_idx_file on HASH (FILE, SIZE, DATE, HASH, ARCHIVE)");

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
        snapshot.db.Close();
    }

    if (!MakeReadOnly(gSnapshots.back().sqliteFile))
    {
        Log(gLogFileHandle, "ERROR: Cannot make read-only: " + gSnapshots.back().sqliteFile.string());
        gErrorCount++;
    }

    Log(gLogFileHandle, "DONE.");
    Log(gLogFileHandle, "Errors: " + std::to_string(gErrorCount));
    Log(gLogFileHandle, "Bytes copied: " + std::to_string(gBytesCopied));
    Log(gLogFileHandle, "Bytes linked: " + std::to_string(gBytesLinked));

    gLogFileHandle.close();

    lockFileHandle.close();
    VERIFY(std::experimental::filesystem::remove(lockFilePath));

    if (!MakeReadOnly(logFilePath))
    {
        Log(gLogFileHandle, "ERROR: Cannot make read-only: " + logFilePath.string());
        gErrorCount++;
    }
}
