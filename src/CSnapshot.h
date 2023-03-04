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

    // find           by suffix                     in last snapshot                        OFTEN               backup
// find           by suffix, hash               in source snapshot                      OFTEN               subtract
// find           by         hash               in source snapshot                      OFTEN               subtract
// find           by suffix, hash               in target snapshot                      OFTEN               add
// find           by         hash               in target snapshot                      OFTEN               add
// enum all                                     in ref, source snapshot                 ONCE PER SNAP       verify, recover, purge, add

    CRepoFile               FindFirstFile(const CRepoFile& constraints) const;
    std::vector<CRepoFile>  FindAllFiles(const CRepoFile& constraints) const;

    bool ImportFile(const CPath& source, CRepoFile& target);
    bool DuplicateFile(const CPath& source, CRepoFile& target, bool copyOnLinkFail = false);

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

    CPath                   mPath;
    CPath                   mSqliteFile;
    mutable CSqliteWrapper  mDb;
    CPath                   mLockFilePath;
    std::ofstream           mLockFileHandle;
};
