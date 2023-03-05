#pragma once

#include "CSqliteWrapper.h"
#include "CRepoFile.h"

class CSnapshot
{
public: // types
    class CIterator
    {
    public:
        CIterator(CSqliteWrapper::CStatement&& statement, const CPath& parentPath);

        bool HasFile();

        CRepoFile GetNextFile();

    private:
        CPath                       mParentPath;
        CSqliteWrapper::CStatement  mStatement;
    };

    enum class EOpenMode
    {
        READ,
        WRITE,
        CREATE
    };

public: // methods
    CSnapshot() = default;
    CSnapshot(CSnapshot&&) = default;
    CSnapshot(const CPath& path, EOpenMode openMode);
    ~CSnapshot();

    static bool IsValidPath(const CPath& path);

    const CPath& GetPath() const;

    void Open(const CPath& path, EOpenMode openMode);
    void Close();

    CRepoFile               FindFile(const CRepoFile& constraints, bool verifyAccessible, bool preferLinkable) const;
    std::vector<CRepoFile>  FindAllFiles(const CRepoFile& constraints) const;

    bool ImportFile(const CPath& source, CRepoFile& target);
    bool DuplicateFile(const CPath& source, CRepoFile& target);
    bool DeleteFile(CRepoFile& repoFile);

    CIterator DBSelect(const CRepoFile& constraints) const;
    void DBInsert(const CRepoFile& repoFile);
    void DBDelete(const CRepoFile& repoFile);
    void DBVerify();
    void DBCompact();

private:
    void DBInitRead();
    void DBInitWrite();
    void DBInitCreate();

    static std::string DBFormatConstraints(const CRepoFile& constraints);

    CPath                                   mPath;
    CPath                                   mSqliteFile;
    mutable CSqliteWrapper                  mDb;
    CPath                                   mLockFilePath;
    std::ofstream                           mLockFileHandle;
};
