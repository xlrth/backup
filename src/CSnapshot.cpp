#include "CSnapshot.h"

#include <experimental/filesystem>

#include "Helpers.h"

static constexpr bool DB_COLUMNS_FILE_SIZE_DATE_HASH_ARCHIVE = true;

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
    static_assert(DB_COLUMNS_FILE_SIZE_DATE_HASH_ARCHIVE, "TODO");

    return
    {
        std::experimental::filesystem::u8path(mStatement.ReadString(0)),
        mParentPath,
        std::experimental::filesystem::u8path(mStatement.ReadString(4)),
        mStatement.ReadInt(1),
        mStatement.ReadInt(2),
        mStatement.ReadString(3)
    };
}


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


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
    if (mDb.IsOpen())
    {
        Close();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
const CPath& CSnapshot::GetPath() const
{
    return mPath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::Open(const CPath& path, bool create)
{
    VERIFY(!mDb.IsOpen());

    mPath = path;

    if (std::experimental::filesystem::exists(mPath / "IN_PROGRESS"))
    {
        throw "snapshot " + mPath.string() + " is incomplete";
    }
    mSqliteFile = mPath / "hash.sqlite";
    if (create)
    {
        if (std::experimental::filesystem::exists(mPath))
        {
            throw "snapshot " + mPath.string() + " already existing";
        }
        if (!std::experimental::filesystem::create_directory(mPath))
        {
            throw "Cannot create directory " + mPath.string();
        }
        mLockFilePath = mPath / "IN_PROGRESS";
        mLockFileHandle.open(mLockFilePath.string());
        if (!mLockFileHandle.is_open())
        {
            throw "Cannot create lock file in " + mPath.string();
        }

        InitWrite();
    }
    else
    {
        if (!std::experimental::filesystem::exists(mSqliteFile))
        {
            throw "snapshot " + mPath.string() + " does not have a sqlite file";
        }

        InitRead();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::Close()
{
    VERIFY(mDb.IsOpen());

    mDb.Close();

    if (mLockFileHandle.is_open())
    {
        MakeReadOnly(mSqliteFile);

        mLockFileHandle.close();
        if (!std::experimental::filesystem::remove(mLockFilePath))
        {
            throw "cannot remove lock file: " + mLockFilePath.string();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::InitRead()
{
    mDb = CSqliteWrapper(mSqliteFile.string().c_str(), true);
    mDb.RunQuery("pragma cache_size = 1000000");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::InitWrite()
{
    static_assert(DB_COLUMNS_FILE_SIZE_DATE_HASH_ARCHIVE, "TODO");

    mDb = CSqliteWrapper(mSqliteFile.string().c_str(), false);
    mDb.RunQuery("pragma cache_size = 1000000");
    mDb.RunQuery("pragma page_size = 4096");
    mDb.RunQuery("pragma synchronous = off");
    mDb.RunQuery("pragma secure_delete = off");
    mDb.RunQuery("pragma journal_mode = off");
    mDb.RunQuery("create table HASH (FILE text not null, SIZE integer not null, DATE integer not null, HASH text not null, ARCHIVE text not null)");
    mDb.RunQuery("create unique index HASH_idx_ARCHIVE on HASH (ARCHIVE, FILE, SIZE, DATE, HASH)");
    mDb.RunQuery("create index HASH_idx_HASH on HASH (HASH, FILE, SIZE, DATE, ARCHIVE)");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::Insert(const CRepoFile& file)
{
    static_assert(DB_COLUMNS_FILE_SIZE_DATE_HASH_ARCHIVE, "TODO");

    mDb.RunQuery(
        "insert into HASH values ("
        "\"" + file.GetSourcePath().u8string() + "\", "
        + std::to_string(file.GetSize()) + ", "
        + std::to_string(file.GetDate()) + ", "
        + "\"" + file.GetHash() + "\", "
        "\"" + file.GetRelativePath().u8string() + "\""
        + ")");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CSnapshot::CIterator CSnapshot::Query(const CRepoFile& constraints) const
{
    static_assert(DB_COLUMNS_FILE_SIZE_DATE_HASH_ARCHIVE, "TODO");

    std::vector<std::string> constraintStrings;
    if (!constraints.GetSourcePath().empty())
    {
        constraintStrings.push_back("FILE=\"" + constraints.GetSourcePath().u8string() + "\"");
    }
    if (constraints.GetSize() != CRepoFile::UNSPECIFIED)
    {
        constraintStrings.push_back("SIZE=" + std::to_string(constraints.GetSize()));
    }
    if (constraints.GetDate() != CRepoFile::UNSPECIFIED)
    {
        constraintStrings.push_back("DATE=" + std::to_string(constraints.GetDate()));
    }
    if (!constraints.GetHash().empty())
    {
        constraintStrings.push_back("HASH=\"" + constraints.GetHash() + "\"");
    }
    if (!constraints.GetRelativePath().empty())
    {
        constraintStrings.push_back("ARCHIVE=\"" + constraints.GetRelativePath().u8string() + "\"");
    }

    std::string query = "select FILE, SIZE, DATE, HASH, ARCHIVE from HASH";
    if (!constraintStrings.empty())
    {
        query += " WHERE ";
        for (auto& constraint : constraintStrings)
        {
            query += constraint;
            if (&constraint != &constraintStrings.back())
            {
                query += " AND ";
            }
        }
    }

    return { mDb.StartQuery(query), mPath };
}
