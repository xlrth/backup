#pragma once

#include <vector>
#include <unordered_map>

#include "CRepoFile.h"

class CFileTable
{
public:
    class CEntry
    {
    public:
        CEntry(unsigned long long fileSystemIndex, size_t snapshotsNum)
            :
            mFileSystemIndex(fileSystemIndex),
            mRefCounts(snapshotsNum)
        {}

        unsigned long long  mFileSystemIndex;
        CRepoFile           mRepoFile;
        std::vector<int>    mRefCounts;
    };

public:
    CFileTable(const std::vector<CPath>& snapshotPaths);

    unsigned long long  GetSize() const;
    CEntry&             GetEntry(unsigned long long fileSystemIndex);

    void VerifyFileSignatureUniqueness() const;
    void WriteToFile() const;

private:
    std::vector<CPath>                                              mSnapshotPaths;
    std::vector<CEntry>                                             mEntries;
    std::unordered_map<unsigned long long, unsigned long long>      mEntryMap;
};
