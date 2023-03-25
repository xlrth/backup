#include "CCmdVerify.h"

#include <algorithm>
#include <ostream>
#include <iomanip>
#include <numeric>

#include "CLogger.h"
#include "Helpers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename First>
bool SmallerThan(First first1, First first2)
{
    return first1 < first2;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename First, typename... Rest>
bool SmallerThan(First first1, First first2, Rest... rest)
{
    return first1 < first2 || (first1 == first2 && SmallerThan(rest...));
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CFileTable::CFileTable(const std::vector<CPath>& snapshotPaths)
    :
    mSnapshotPaths(snapshotPaths)
{
    mEntries.reserve(1000);
    mEntryMap.reserve(1000);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
  unsigned long long CFileTable::GetSize() const
{
      return mEntries.size();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CFileTable::CEntry& CFileTable::GetEntry(unsigned long long fileSystemIndex)
{
    auto entryMapIt = mEntryMap.find(fileSystemIndex);
    if (entryMapIt == mEntryMap.end())
    {
        mEntryMap[fileSystemIndex] = mEntries.size();
        mEntries.push_back({ fileSystemIndex, mSnapshotPaths.size() });
        return mEntries.back();
    }

    return mEntries[entryMapIt->second];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFileTable::VerifyFileSignatureUniqueness() const
{
    std::vector<unsigned long long> sortedIndex(mEntries.size());
    std::iota(sortedIndex.begin(), sortedIndex.end(), 0);
    std::sort(sortedIndex.begin(), sortedIndex.end(),
        [this](auto e1, auto e2)
    {
        return SmallerThan(
            mEntries[e1].mRepoFile.GetSize(),       mEntries[e2].mRepoFile.GetSize(),
            mEntries[e1].mRepoFile.GetTime(),       mEntries[e2].mRepoFile.GetTime(),
            mEntries[e1].mRepoFile.GetSourcePath(), mEntries[e2].mRepoFile.GetSourcePath());
    });

    for (long long i = 0; i < static_cast<long long>(sortedIndex.size()) - 1; i++)
    {
        auto& file1 = mEntries[sortedIndex[i]].mRepoFile;
        auto& file2 = mEntries[sortedIndex[i + 1]].mRepoFile;
        if (file1.GetSize() == file2.GetSize()
            && file1.GetTime() == file2.GetTime()
            && file1.GetSourcePath() == file2.GetSourcePath()
            && file1.GetHash() != file2.GetHash())
        {
            CLogger::GetInstance().LogError("files with same signature but different hash: " + file1.SourceToString() + " and " + file2.SourceToString());
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFileTable::WriteToFile() const
{
    auto outputPath = std::filesystem::current_path() / ("fileTable_" + Helpers::CurrentTimeAsString() + ".csv");

    std::ofstream outputFile(outputPath);
    if (!outputFile.is_open())
    {
        CLogger::GetInstance().LogError("cannot open " + outputPath.string());
        return;
    }

    CLogger::GetInstance().Log("writing file table to " + outputPath.string());

    outputFile
        << "file index,";

    for (int snapshotIdx = 0; snapshotIdx < mSnapshotPaths.size(); snapshotIdx++)
    {
        outputFile << mSnapshotPaths[snapshotIdx].filename().string() << ",";
    }

    outputFile
        << "size,"
        << "time,"
        << "hash,"
        << "first path,"
        << "first source,"
        << std::endl;

    for (auto& entry : mEntries)
    {
        outputFile
            << std::setw(20) << entry.mFileSystemIndex << ",";

        for (int snapshotIdx = 0; snapshotIdx < mSnapshotPaths.size(); snapshotIdx++)
        {
            outputFile << entry.mRefCounts[snapshotIdx] << ",";
        }

        outputFile
            << entry.mRepoFile.ToCSV() << ","
            << std::endl;
    }

    outputFile.close();
    Helpers::MakeReadOnly(outputPath);
}