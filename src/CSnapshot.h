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


public: // methods
    CSnapshot() = default;
    CSnapshot(CSnapshot&&) = default;
    CSnapshot(const CPath& path, bool create);
    ~CSnapshot();

    const CPath& GetPath() const;

    void Open(const CPath& path, bool create);
    void Close();

    void Insert(const CRepoFile& file);
    CIterator Query(const CRepoFile& constraints);

private:
    void InitRead();
    void InitWrite();

    CPath           mPath;
    CPath           mSqliteFile;
    CSqliteWrapper  mDb;
    CPath           mLockFilePath;
    std::ofstream   mLockFileHandle;
};
