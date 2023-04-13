#pragma once

#include <string>
#include <memory>
#include <fstream>

#include "CPath.h"
#include "CSize.h"
#include "CTime.h"

class CRepoFile
{
public:
    CRepoFile() = default;
    CRepoFile(CRepoFile&&) = default;
    CRepoFile(const CRepoFile&) = default;

    CRepoFile(
        CPath           sourcePath,
        CSize           size,
        CTime           time,
        std::string     hash,
        CPath           relativePath,
        CPath           parentPath);

    CPath               GetSourcePath() const;
    CSize               GetSize() const;
    CTime               GetTime() const;
    const std::string&  GetHash() const;
    bool                HasHash() const;
    CPath               GetRelativePath() const;
    CPath               GetParentPath() const;

    CPath GetFullPath() const;

    void SetSourcePath(const CPath& sourcePath);
    void SetSize(CSize size);
    void SetTime(CTime time);
    void SetHash(const std::string& hash);
    void SetRelativePath(const CPath& relativePath);
    void SetParentPath(const CPath& parentPath);

    bool IsExisting() const;
    bool IsLinkable() const;

    bool ReadSourceProperties();
    bool LockSource();
    void UnlockSource();
    bool HashSource();
    bool IsSourceLocked();

    bool Hash();

    std::string SourceToString() const;
    std::string ToString() const;
    std::string ToCSV() const;

    bool Copy(const CPath& source) const;
    bool Link(const CPath& source) const;

    bool Delete();

    unsigned long long GetFileSystemIndex() const;

    CRepoFile& operator = (CRepoFile&& other) = default;
    CRepoFile& operator = (const CRepoFile& other) = default;

public: // static
    static void StaticLogStats();

private:
    CPath           mSourcePath;
    CSize           mSize;
    CTime           mTime;
    std::string     mHash;
    CPath           mRelativePath;
    CPath           mParentPath;

    std::shared_ptr<std::ifstream>   mSourceFileHandle;

private: // static
    static long long   sFilesHashed;
    static long long   sFilesCopied;
    static long long   sFilesLinked;
    static long long   sFilesDeleted;

    static long long   sBytesHashed;
    static long long   sBytesCopied;
    static long long   sBytesLinked;
    static long long   sBytesDeleted;
};
