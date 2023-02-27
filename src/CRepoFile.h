#pragma once

#include <string>
#include <memory>
#include <fstream>

#include "CDate.h"
#include "CPath.h"

class CRepoFile
{
public:
    static constexpr long long UNSPECIFIED = -1;

public:
    CRepoFile() = default;
    CRepoFile(
        CPath           sourcePath          ,
        CPath           parentPath          ,
        CPath           relativePath        ,
        long long       size                ,
        CDate           date                ,
        std::string     hash                );

    bool IsExisting() const;    
    bool IsLinkable() const;

    CPath               GetSourcePath() const;
    CPath               GetParentPath() const;
    CPath               GetRelativePath() const;
    long long           GetSize() const;
    CDate               GetDate() const;
    const std::string&  GetHash() const;

    void SetSourcePath(const CPath& sourcePath);
    void SetParentPath(const CPath& parentPath);
    void SetRelativePath(const CPath& relativePath);
    void SetSize(long long size);
    void SetDate(CDate date);
    void SetHash(const std::string& hash);

    bool OpenSource();
    void CloseSource();
    bool HashSource();

    CPath GetFullPath() const;

    std::string SourceToString() const;
    std::string ToString() const;

    bool Copy(const CPath& source) const;

    bool Link(const CPath& source) const;

public: // static
    static long long StaticGetBytesCopied();
    static long long StaticGetBytesLinked();

private:
    CPath           mSourcePath;
    CPath           mParentPath;
    CPath           mRelativePath;
    long long       mSize = UNSPECIFIED;
    CDate           mDate = UNSPECIFIED;
    std::string     mHash;

    std::shared_ptr<std::ifstream>   mSourceFileHandle;

private: // static
    static long long   sBytesCopied;
    static long long   sBytesLinked;
};
