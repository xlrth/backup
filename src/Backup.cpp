#include <fstream>
#include <chrono>
#include <thread>
#include <map>

#include "Backup.h"
#include "BackupConfig.h"
#include "SqliteWrapper.h"

#include "Helpers.h"
#include "picosha2.h"

#ifdef _WIN32
#   define MAX_PATH_LENGTH  _MAX_FILESYS_NAME
#else
#   include <linux/limits.h>
#   define MAX_PATH_LENGTH  PATH_MAX
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// constants
enum class EEvent
{
    ERROR                   ,
    EXCLUDE                 ,
    ADD                     ,
    ADD_ADDITIONAL          ,
    LINK                    ,
    LINK_UNCHANGED          ,
    HASH_CHANGED            ,
    HASH_NEW                ,
    HASH_ALWAYS,
    HASH_SKIP,
    TOO_MANY_LINKS          ,
};

std::map<EEvent, EColor> COLOR_MAP
{
    { EEvent::ERROR          , EColor::RED          },
    { EEvent::EXCLUDE        , EColor::BLUE         },
    { EEvent::ADD            , EColor::YELLOW       },
    { EEvent::ADD_ADDITIONAL , EColor::MAGENTA      },
    { EEvent::LINK           , EColor::CYAN         },
    { EEvent::LINK_UNCHANGED , EColor::GREEN        },
    { EEvent::HASH_CHANGED   , EColor::DARK_YELLOW  },
    { EEvent::HASH_NEW       , EColor::DARK_GREEN   },
    { EEvent::HASH_ALWAYS    , EColor::DARK_GRAY    },
    { EEvent::HASH_SKIP      , EColor::DARK_GRAY    },
    { EEvent::TOO_MANY_LINKS , EColor::DARK_MAGENTA },
};


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
static bool                        gAlwaysHash  = false;

static long long   gErrorCount  = 0;
static long long   gBytesLinked = 0;
static long long   gBytesCopied = 0;


////////////////////////////////////////////////////////////////////////////////////////////////////
// functions


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
static void BackupSingleRecursive(
    const std::experimental::filesystem::path&  source,
    const std::experimental::filesystem::path&  dest)
{
    // WORKAROUND for filesystem misbehaviour due to long paths
    if (source.native().size() >= MAX_PATH_LENGTH)
    {
        Log("ERROR: Path is too long: " + std::string(source.native().begin(), source.native().end()), COLOR_MAP[EEvent::ERROR]);
        gErrorCount++;
        return;
    }
    if (dest.native().size() >= MAX_PATH_LENGTH)
    {
        Log("ERROR: Path is too long: " + std::string(dest.native().begin(), dest.native().end()), COLOR_MAP[EEvent::ERROR]);
        gErrorCount++;
        return;
    }

    std::string sourceStr = source.string();
    std::string destStr = dest.string();

    std::string sourceStrUpper = ToUpper(sourceStr);
    for (const auto& exclude : gExcludes)
    {
        if (exclude.length() < sourceStrUpper.length() && std::mismatch(exclude.rbegin(), exclude.rend(), sourceStrUpper.rbegin())
            .first == exclude.rend())
        {
            Log("excluding " + sourceStr, COLOR_MAP[EEvent::EXCLUDE]);
            return;
        }
    }

    if (!std::experimental::filesystem::exists(source))
    {
        Log("ERROR: Cannot find " + sourceStr, COLOR_MAP[EEvent::ERROR]);
        gErrorCount++;
        return;
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
            Log("ERROR: Cannot lock " + sourceStr, COLOR_MAP[EEvent::ERROR]);
            gErrorCount++;
            return;
        }

        long long sourceSize = std::experimental::filesystem::file_size(source);
        auto sourceDateTimePoint = std::experimental::filesystem::last_write_time(source);
        long long sourceDate = sourceDateTimePoint.time_since_epoch().count();

        std::string unchangedArchHash;
        std::string unchangedArchSubPath;
        bool fileChanged = false;

        if (gSnapshots.size() > 1)
        {
            auto query = gSnapshots[gSnapshots.size() - 2].db.StartQuery(
                "select SIZE, DATE, HASH, ARCHIVE from HASH indexed by HASH_idx_file where FILE = \"" + source.u8string() + "\"");
            if (query.HasData())
            {
                long long archSize = query.ReadInt(0);
                long long archDate = query.ReadInt(1);
                if (archSize == sourceSize && archDate == sourceDate)
                {
                    // ASSUME FILE IS SAME
                    unchangedArchHash = query.ReadString(2);
                    unchangedArchSubPath = query.ReadString(3);
                }
                else
                {
                    fileChanged = true;
                }
                VERIFY(!query.HasData());
            }
        }

        std::string hash;

        if (unchangedArchHash.empty() || gAlwaysHash)
        {
            if (gVerbose)
            {
                std::string reason;
                EEvent evt;
                if (fileChanged)
                {
                    reason = "File changed";
                    evt = EEvent::HASH_CHANGED;
                }
                else if (unchangedArchHash.empty())
                {
                    reason = "Path is new";
                    evt = EEvent::HASH_NEW;
                }
                else
                {
                    reason = "Always hashing";
                    evt = EEvent::HASH_ALWAYS;
                }
                
                Log("Hashing, reason: " + reason + ", date: " + TimeAsString(sourceDateTimePoint) + " size: " + BytesAsString(sourceSize) + " path: " + sourceStr, COLOR_MAP[evt]);
            }

            hash = picosha2::hash256_hex_string(std::istreambuf_iterator<char>(sourceFileHandle), std::istreambuf_iterator<char>());
            if (!unchangedArchHash.empty() && unchangedArchHash != hash)
            {
                Log("ERROR: Supposedly unchanged file with hash mismatch: " + sourceStr, COLOR_MAP[EEvent::ERROR]);
                gErrorCount++;
                unchangedArchHash.clear();
                unchangedArchSubPath.clear();
            }
        }
        else
        {
            if (gVerbose)
            {
                Log("File seems unchanged, skipping hashing, date: " + TimeAsString(sourceDateTimePoint) + " size: " + BytesAsString(sourceSize) + " path: " + sourceStr, COLOR_MAP[EEvent::HASH_SKIP]);
            }

            hash = unchangedArchHash;
        }

        bool tooManyLinks = false;
        bool linkSucceeded = false;
        std::error_code errorCode;
        
        if (!unchangedArchSubPath.empty())
        {

            // try linking with archive of last snapshot
            std::experimental::filesystem::path archPath = gSnapshots[gSnapshots.size() - 2].dir / std::experimental::filesystem::u8path(unchangedArchSubPath);
            std::experimental::filesystem::create_hard_link(archPath, dest, errorCode);
            if (!errorCode)
            {
                linkSucceeded = true;
                gBytesLinked += sourceSize;
                if (gVerbose)
                {
                    Log("Linked archive in last snapshot, date: " + TimeAsString(sourceDateTimePoint) + " size: " + BytesAsString(sourceSize) + " path: " + sourceStr, COLOR_MAP[EEvent::LINK_UNCHANGED]);
                }
            }
        }
        if (!linkSucceeded)
        {
            // try to link archives with same hash. Can fail for deleted files or when exceeding hard link count limits
            for (int i = (int)gSnapshots.size() - 1; i >= 0; i--)
            {
            	// reverse-iterate to improve likelihood of finding archive with hard link count below max
                auto query = gSnapshots[i].db.StartQuery("select ARCHIVE from HASH where HASH = \"" + hash + "\" ORDER BY rowid DESC");
                while (query.HasData())
                {
                    std::experimental::filesystem::path archPath = gSnapshots[i].dir / std::experimental::filesystem::u8path(query.ReadString(0));
                    std::experimental::filesystem::create_hard_link(archPath, dest, errorCode);
                    if (errorCode)
                    {
                        if (errorCode.value() == 1142)
                        {
                            tooManyLinks = true;
                            if (gVerbose)
                            {
                                Log("Archive has too many links, size: " + BytesAsString(sourceSize) + " path: " + archPath.string(), COLOR_MAP[EEvent::TOO_MANY_LINKS]);
                            }
                        }
                        else
                        {
                            Log("ERROR: Cannot link archive, date: " + TimeAsString(sourceDateTimePoint) + " size: " + BytesAsString(sourceSize) + " path: " + sourceStr 
                                + "    target:    " + archPath.string()
                                + "    error code:    " + std::to_string(errorCode.value()) + " " + errorCode.message(),
                                COLOR_MAP[EEvent::ERROR]);
                            gErrorCount++;
                        }
                    }
                    else
                    {
                        linkSucceeded = true;
                        gBytesLinked += sourceSize;
                        if (gVerbose)
                        {
                            Log("Linked archive, date: " + TimeAsString(sourceDateTimePoint) + " size: " + BytesAsString(sourceSize) + " path: " + sourceStr 
                                + "    target:    " + archPath.string(),
                                COLOR_MAP[EEvent::LINK]);
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

        if (!linkSucceeded)
        {
            if (!tooManyLinks)
            {
                Log("Adding archive, date: " + TimeAsString(sourceDateTimePoint) + " size: " + BytesAsString(sourceSize) + " path: " + sourceStr, COLOR_MAP[EEvent::ADD]);
            }
            else if (gVerbose)
            {
                Log("Adding additional instance (too many links), date: " + TimeAsString(sourceDateTimePoint) + " size: " + BytesAsString(sourceSize) + " path: " + sourceStr, COLOR_MAP[EEvent::ADD_ADDITIONAL]);
            }
            if (!std::experimental::filesystem::copy_file(source, dest, std::experimental::filesystem::copy_options::copy_symlinks))
            {
                Log("ERROR: Cannot copy file from " + sourceStr + " to " + destStr, COLOR_MAP[EEvent::ERROR]);
                gErrorCount++;
                return;
            }
#ifndef _WIN32 // fixes modification date not being copied under linux
            auto lastWriteTime = std::experimental::filesystem::last_write_time(source, errorCode);
            if (errorCode)
            {
                Log("ERROR: Cannot read modification date from " + sourceStr, COLOR_ERROR);
                gErrorCount++;
            }
            else
            {
                std::experimental::filesystem::last_write_time(dest, lastWriteTime, errorCode);
                if (errorCode)
                {
                    Log("ERROR: Cannot set modification date of " + destStr, COLOR_ERROR);
                    gErrorCount++;
                }
            }
#endif
            gBytesCopied += sourceSize;
            if (!MakeReadOnly(dest))
            {
                Log("ERROR: Cannot make read-only: " + dest.string(), COLOR_MAP[EEvent::ERROR]);
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
    bool                                                    verbose,
    bool                                                    alwaysHash)
{
    gExcludes   = excludes;
    gVerbose    = verbose;
    gAlwaysHash = alwaysHash;

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

    InitLog(gSnapshots.back().dir);

    if (gVerbose)
    {
        Log("verbose enabled");
    }
    if (gAlwaysHash)
    {
        Log("always hashing enabled");
    }
    Log("backuping to " + gSnapshots.back().dir.string());

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
    gSnapshots.back().db.RunQuery("create index HASH_idx_hash on HASH (HASH)");
    gSnapshots.back().db.RunQuery("create unique index HASH_idx_file on HASH (FILE, SIZE, DATE, HASH, ARCHIVE)");

    for (auto& source : sources)
    {
        std::string sourceMod = source.string();

#ifdef _WIN32
        if (sourceMod.length() == 2 && sourceMod[1] == ':')
        {
            sourceMod.append({ '\\' });
        }
#endif

        std::string targetMod = sourceMod;

#ifdef _WIN32
        std::replace(targetMod.begin(), targetMod.end(), ':', '#');
        std::replace(targetMod.begin(), targetMod.end(), '\\', '#');
#endif
        std::replace(targetMod.begin(), targetMod.end(), '/', '#');

        BackupSingleRecursive(sourceMod, gSnapshots.back().dir / targetMod);
    }

    for (auto& snapshot : gSnapshots)
    {
        snapshot.db.Close();
    }

    if (!MakeReadOnly(gSnapshots.back().sqliteFile))
    {
        Log("ERROR: Cannot make read-only: " + gSnapshots.back().sqliteFile.string(), COLOR_MAP[EEvent::ERROR]);
        gErrorCount++;
    }

    Log("DONE.");
    Log("Errors: " + std::to_string(gErrorCount));
    Log("Bytes copied: " + BytesAsString(gBytesCopied));
    Log("Bytes linked: " + BytesAsString(gBytesLinked));

    CloseLog();

    lockFileHandle.close();
    VERIFY(std::experimental::filesystem::remove(lockFilePath));
}
