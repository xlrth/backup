#pragma once

#include <vector>

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

public: // static methods
    static bool StaticIsExsting(const CPath& path);
    static void StaticValidate(const CPath& path);

public: // methods
    CSnapshot() = default;
    CSnapshot(CSnapshot&&) = default;
    CSnapshot(const CPath& path, bool create);
    ~CSnapshot();

    const CPath&    GetAbsolutePath() const;
    CPath           GetMetaDataPath() const;

    void Open(const CPath& path, bool create);
    void Close();

    void SetInProgress();
    void ClearInProgress();
    bool IsInProgress();

    CRepoFile               FindFile(const CRepoFile& constraints, bool preferLinkable) const;
    std::vector<CRepoFile>  FindAllFiles(const CRepoFile& constraints) const;

    bool InsertFile(const CPath& source, const CRepoFile& target, bool preferLink);
    bool DeleteFile(CRepoFile& repoFile);

    CIterator   DBSelect(const CRepoFile& constraints) const;
    void        DBInsert(const CRepoFile& repoFile);
    void        DBDelete(const CRepoFile& repoFile);
    bool        DBCheckIntegrity();
    void        DBCompact();

private:
    void DBInit();

    static std::string PathToDBString(const CPath& path);
    static CPath       DBStringToPath(const std::string& path);
    static std::string DBFormatConstraints(const CRepoFile& constraints);

    CPath                       mPath;
    mutable CSqliteWrapper      mSqliteDB;

    inline static const CPath   META_DATA_PATH          = ".backup";
    inline static const CPath   DB_FILE_PATH            = META_DATA_PATH / "db.sqlite";
    inline static const CPath   IN_PROGRESS_FILE_PATH   = META_DATA_PATH / "IN_PROGRESS";
};
