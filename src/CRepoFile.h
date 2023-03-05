#pragma once

#include <string>
#include <memory>
#include <fstream>

#include "CPath.h"
#include "CSize.h"
#include "CDate.h"

class CRepoFile
{
public:
    CRepoFile() = default;
    CRepoFile(CRepoFile&&) = default;
    CRepoFile(const CRepoFile&) = default;

    CRepoFile(
        CPath           sourcePath          ,
        CPath           parentPath          ,
        CPath           relativePath        ,
        CSize           size                ,
        CDate           date                ,
        std::string     hash                );

    bool IsExisting() const;    
//    bool IsLinkable() const;

    CPath               GetSourcePath() const;
    CPath               GetParentPath() const;
    CPath               GetRelativePath() const;
    CSize               GetSize() const;
    CDate               GetDate() const;
    const std::string&  GetHash() const;

    void SetSourcePath(const CPath& sourcePath);
    void SetParentPath(const CPath& parentPath);
    void SetRelativePath(const CPath& relativePath);
    void SetSize(CSize size);
    void SetDate(CDate date);
    void SetHash(const std::string& hash);

    bool OpenSource();
    void CloseSource();
    bool HashSource();

    bool Open();
    void Close();
    bool Hash();

    CPath GetFullPath() const;

    std::string SourceToString() const;
    std::string ToString() const;
    std::string ToCSV() const;

    bool Copy(const CPath& source) const;
    bool Link(const CPath& source) const;

    bool Delete();

    bool GetFileInfo(unsigned long long& fileSystemIndex, int& hardLinkCount) const;

    CRepoFile& operator = (CRepoFile&& other) = default;
    CRepoFile& operator = (const CRepoFile& other) = default;
//    bool operator != (const CRepoFile& other) const;

public: // static
    static void StaticLogStats();

private:
    CPath           mSourcePath;
    CPath           mParentPath;
    CPath           mRelativePath;
    CSize           mSize;
    CDate           mDate;
    std::string     mHash;

    std::shared_ptr<std::ifstream>   mSourceFileHandle;
    std::shared_ptr<std::ifstream>   mFileHandle;

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
