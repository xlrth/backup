#include "CSnapshot.h"

#include <filesystem>

#include "CLogger.h"
#include "Helpers.h"

static constexpr bool DB_COLUMNS_SOURCE_SIZE_TIME_HASH_FILE = true;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CSnapshot::CIterator::CIterator(CSqliteWrapper::CStatement&& statement, const CPath& parentPath)
    :
    mParentPath(parentPath),
    mStatement(std::move(statement))
{}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSnapshot::CIterator::HasFile()
{
    return mStatement.HasData();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CRepoFile CSnapshot::CIterator::GetNextFile()
{
    static_assert(DB_COLUMNS_SOURCE_SIZE_TIME_HASH_FILE, "TODO");

    return
    {
        std::filesystem::u8path(mStatement.ReadString(0)),
        mStatement.ReadInt(1),
        mStatement.ReadInt(2),
        mStatement.ReadString(3),
        std::filesystem::u8path(mStatement.ReadString(4)),
        mParentPath
    };
}


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSnapshot::StaticIsExsting(const CPath& path)
{
    return std::filesystem::exists(path / DB_FILE_PATH);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::StaticValidate(const CPath& path)
{
    CPath canonicalPath = std::filesystem::weakly_canonical(path);
    if (!StaticIsExsting(canonicalPath))
    {
        throw "snapshot invalid, sqlite file missing: " + (canonicalPath / DB_FILE_PATH).string();
    }
    if (std::filesystem::exists(canonicalPath / IN_PROGRESS_FILE_PATH))
    {
        throw "snapshot is unfinished, delete either " + IN_PROGRESS_FILE_PATH.string() + " or whole snapshot: " + canonicalPath.string();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CSnapshot::CSnapshot(const CPath& path, bool create)
{
    Open(path, create);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CSnapshot::~CSnapshot()
{
    Helpers::TryCatch([this]() { Close(); });
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
const CPath& CSnapshot::GetAbsolutePath() const
{
    return mPath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CPath CSnapshot::GetMetaDataPath() const
{
    return mPath / META_DATA_PATH;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::Open(const CPath& path, bool create)
{
    VERIFY(!mSqliteDB.IsOpen());

    // use normalized path, so we always have the same max path limits
    mPath = std::filesystem::weakly_canonical(path);

    if (create)
    {
        if (std::filesystem::exists(mPath))
        {
            throw "snapshot directory already exists: " + mPath.string();
        }
        std::error_code errorCode;
        if (!std::filesystem::create_directories(GetMetaDataPath(), errorCode))
        {
            throw "cannot create snapshot directories: " + GetMetaDataPath().string() + ": " + errorCode.message();
        }
    }
    else
    {
        StaticValidate(mPath);

        Helpers::MakeBackup(mPath / DB_FILE_PATH);
        Helpers::MakeWritable(mPath / DB_FILE_PATH);
    }

    DBInit();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::Close()
{
    if (mSqliteDB.IsOpen())
    {
        mSqliteDB.Close();

        Helpers::MakeReadOnly(mPath / DB_FILE_PATH);
        Helpers::MakeBackup(mPath / DB_FILE_PATH);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::SetInProgress()
{
    VERIFY(!IsInProgress());
    std::ofstream file((mPath / IN_PROGRESS_FILE_PATH).string());
    if (!IsInProgress())
    {
        throw "cannot set in progress: " + mPath.string();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::ClearInProgress()
{
    VERIFY(IsInProgress());
    std::filesystem::remove(mPath / IN_PROGRESS_FILE_PATH);
    if (IsInProgress())
    {
        throw "cannot clear in progress: " + mPath.string();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSnapshot::IsInProgress()
{
    return std::filesystem::exists(mPath / IN_PROGRESS_FILE_PATH);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CRepoFile CSnapshot::FindFile(const CRepoFile& constraints, bool preferLinkable) const
{
    CRepoFile unlinkableFile;

    auto iterator = DBSelect(constraints);
    while (iterator.HasFile())
    {
        CRepoFile file = iterator.GetNextFile();
        if (!preferLinkable || file.IsLinkable())
        {
            return file;
        }
        unlinkableFile = file;
    }

    return unlinkableFile;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::vector<CRepoFile> CSnapshot::FindAllFiles(const CRepoFile& constraints) const
{
    std::vector<CRepoFile> result;
    result.reserve(1000);

    auto iterator = DBSelect(constraints);
    while (iterator.HasFile())
    {
        result.emplace_back(iterator.GetNextFile());
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSnapshot::InsertFile(const CPath& source, const CRepoFile& target, bool preferLink)
{
    VERIFY(!target.GetSourcePath().empty());
    VERIFY(target.HasHash());
    VERIFY(target.GetSize().IsSpecified());
    VERIFY(target.GetTime().IsSpecified());
    VERIFY(!target.GetRelativePath().empty());
    VERIFY(target.GetParentPath() == GetAbsolutePath());

    if (!(preferLink && target.Link(source)) && !target.Copy(source))
    {
        return false;
    }

    DBInsert(target);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSnapshot::DeleteFile(CRepoFile& repoFile)
{
    if (!repoFile.Delete())
    {
        return false;
    }

    DBDelete(repoFile);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CSnapshot::CIterator CSnapshot::DBSelect(const CRepoFile& constraints) const
{
    std::string query = "select * from FILES " + DBFormatConstraints(constraints);

    return { mSqliteDB.StartQuery(query), mPath };
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::DBInsert(const CRepoFile& file)
{
    static_assert(DB_COLUMNS_SOURCE_SIZE_TIME_HASH_FILE, "TODO");

    mSqliteDB.RunQuery(
        std::string("insert into FILES values (")
        + "\"" + file.GetSourcePath().u8StringAsString() + "\", "
        + std::to_string(file.GetSize()) + ", "
        + std::to_string(file.GetTime()) + ", "
        + "\"" + file.GetHash() + "\", "
        + "\"" + file.GetRelativePath().u8StringAsString() + "\""
        + ")");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::CSnapshot::DBDelete(const CRepoFile& constraints)
{
    mSqliteDB.RunQuery("delete from FILES " + DBFormatConstraints(constraints));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSnapshot::CSnapshot::DBCheckIntegrity()
{
    auto statement = mSqliteDB.StartQuery("pragma integrity_check");
    VERIFY(statement.HasData());
    return statement.ReadString(0) == "ok";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::CSnapshot::DBCompact()
{
    mSqliteDB.RunQuery("vacuum");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::DBInit()
{
    static_assert(DB_COLUMNS_SOURCE_SIZE_TIME_HASH_FILE, "TODO");

    mSqliteDB = CSqliteWrapper(mPath / DB_FILE_PATH, false);
    mSqliteDB.RunQuery("pragma locking_mode = exclusive");
    mSqliteDB.RunQuery("pragma cache_size = 1000000");
    mSqliteDB.RunQuery("pragma synchronous = off");
    mSqliteDB.RunQuery("pragma secure_delete = off");
    mSqliteDB.RunQuery("pragma journal_mode = off");
    mSqliteDB.RunQuery("create table if not exists FILES (SOURCE text not null, SIZE integer not null, TIME integer not null, HASH text not null, FILE text not null)");
    mSqliteDB.RunQuery("create unique index if not exists FILES_SOURCE_SIZE_TIME_HASH_FILE on FILES (SOURCE, SIZE, TIME, HASH, FILE)");
    mSqliteDB.RunQuery("create index if not exists FILES_HASH on FILES (HASH)");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CSnapshot::DBFormatConstraints(const CRepoFile& constraints)
{
    static_assert(DB_COLUMNS_SOURCE_SIZE_TIME_HASH_FILE, "TODO");

    std::vector<std::string> constraintStrings;
    if (!constraints.GetSourcePath().empty())
    {
        constraintStrings.push_back("SOURCE=\"" + constraints.GetSourcePath().u8StringAsString() + "\"");
    }
    if (constraints.GetSize().IsSpecified())
    {
        constraintStrings.push_back("SIZE=" + std::to_string(constraints.GetSize()));
    }
    if (constraints.GetTime().IsSpecified())
    {
        constraintStrings.push_back("TIME=" + std::to_string(constraints.GetTime()));
    }
    if (constraints.HasHash())
    {
        constraintStrings.push_back("HASH=\"" + constraints.GetHash() + "\"");
    }
    if (!constraints.GetRelativePath().empty())
    {
        constraintStrings.push_back("FILE=\"" + constraints.GetRelativePath().u8StringAsString() + "\"");
    }

    std::string result;
    if (!constraintStrings.empty())
    {
        result += " WHERE ";
        for (auto& constraint : constraintStrings)
        {
            result += constraint;
            if (&constraint != &constraintStrings.back())
            {
                result += " AND ";
            }
        }
    }

    return result;
}
