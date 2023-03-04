#include "CSnapshot.h"

#include <experimental/filesystem>

#include "CHelpers.h"

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
CSnapshot::CSnapshot(const CPath& path, EOpenMode openMode)
{
    Open(path, openMode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CSnapshot::~CSnapshot()
{
    Close();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSnapshot::IsValidPath(const CPath& path)
{
    auto sqliteFile = path / "hash.sqlite";

    return std::experimental::filesystem::exists(sqliteFile)
        && std::experimental::filesystem::is_regular_file(sqliteFile);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
const CPath& CSnapshot::GetPath() const
{
    return mPath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::Open(const CPath& path, EOpenMode openMode)
{
    VERIFY(!mDb.IsOpen());

    mPath = path;

    if (std::experimental::filesystem::exists(mPath / "IN_PROGRESS"))
    {
        throw "snapshot " + mPath.string() + " is incomplete";
    }

    mSqliteFile = mPath / "hash.sqlite";

    if (openMode == EOpenMode::READ)
    {
        if (!std::experimental::filesystem::exists(mSqliteFile))
        {
            throw "snapshot " + mPath.string() + " does not have a sqlite file";
        }

        DBInitRead();
    }
    else if (openMode == EOpenMode::WRITE)
    {
        if (!std::experimental::filesystem::exists(mSqliteFile))
        {
            throw "snapshot " + mPath.string() + " does not have a sqlite file";
        }
        mLockFilePath = mPath / "IN_PROGRESS";
        mLockFileHandle.open(mLockFilePath.string());
        if (!mLockFileHandle.is_open())
        {
            throw "Cannot create lock file in " + mPath.string();
        }

        CHelpers::MakeBackup(mSqliteFile);
        CHelpers::MakeWritable(mSqliteFile);

        DBInitWrite();
    }
    else if (openMode == EOpenMode::CREATE)
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

        DBInitCreate();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::Close()
{
    if (mDb.IsOpen())
    {
        mDb.Close();

        if (mLockFileHandle.is_open())
        {
            CHelpers::MakeReadOnly(mSqliteFile);
            CHelpers::MakeBackup(mSqliteFile);

            mLockFileHandle.close();
            if (!std::experimental::filesystem::remove(mLockFilePath))
            {
                throw "cannot remove lock file: " + mLockFilePath.string();
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CRepoFile CSnapshot::FindFirstFile(const CRepoFile& constraints) const
{
    auto iterator = DBSelect(constraints);
    if (iterator.HasFile())
    {
        return iterator.GetNextFile();
    }

    return {};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::vector<CRepoFile> CSnapshot::FindAllFiles(const CRepoFile& constraints) const
{
    std::vector<CRepoFile> result;

    auto iterator = DBSelect(constraints);
    while (iterator.HasFile())
    {
        result.emplace_back(iterator.GetNextFile());
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSnapshot::ImportFile(const CPath& source, CRepoFile& target)
{
    VERIFY(!target.GetRelativePath().empty());
    VERIFY(!target.GetHash().empty());
    VERIFY(target.GetSize().IsSpecified());
    VERIFY(target.GetDate().IsSpecified());

    target.SetSourcePath(source);
    target.SetParentPath(GetPath());

    if (!target.Copy(source))
    {
        return false;
    }
    
    DBInsert(target);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSnapshot::DuplicateFile(const CPath& source, CRepoFile& target, bool copyOnLinkFail)
{
    VERIFY(!target.GetSourcePath().empty());
    VERIFY(!target.GetRelativePath().empty());
    VERIFY(!target.GetHash().empty());
    VERIFY(target.GetSize().IsSpecified());
    VERIFY(target.GetDate().IsSpecified());

    target.SetParentPath(GetPath());

    if (!target.Link(source))
    {
        if (!copyOnLinkFail || !target.Copy(source))
        {
            return false;
        }
    }

    DBInsert(target);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CSnapshot::CIterator CSnapshot::DBSelect(const CRepoFile& constraints) const
{
    std::string query = "select * from HASH " + DBFormatConstraints(constraints);

    return { mDb.StartQuery(query), mPath };
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::DBInsert(const CRepoFile& file)
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
void CSnapshot::CSnapshot::DBDelete(const CRepoFile& constraints)
{
    mDb.RunQuery("delete from HASH " + DBFormatConstraints(constraints));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::CSnapshot::DBVerify()
{
    mDb.RunQuery("pragma integrity_check");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::CSnapshot::DBCompact()
{
    mDb.RunQuery("vacuum");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::DBInitRead()
{
    mDb = CSqliteWrapper(mSqliteFile.string().c_str(), true);
    mDb.RunQuery("pragma cache_size = 1000000");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::DBInitWrite()
{
    mDb = CSqliteWrapper(mSqliteFile.string().c_str(), false);
    mDb.RunQuery("pragma cache_size = 1000000");
    mDb.RunQuery("pragma synchronous = off");
    mDb.RunQuery("pragma secure_delete = off");
    mDb.RunQuery("pragma journal_mode = off");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSnapshot::DBInitCreate()
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
std::string CSnapshot::DBFormatConstraints(const CRepoFile& constraints)
{
    static_assert(DB_COLUMNS_FILE_SIZE_DATE_HASH_ARCHIVE, "TODO");

    std::vector<std::string> constraintStrings;
    if (!constraints.GetSourcePath().empty())
    {
        constraintStrings.push_back("FILE=\"" + constraints.GetSourcePath().u8string() + "\"");
    }
    if (constraints.GetSize().IsSpecified())
    {
        constraintStrings.push_back("SIZE=" + std::to_string(constraints.GetSize()));
    }
    if (constraints.GetDate().IsSpecified())
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
