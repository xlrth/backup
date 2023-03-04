#include "CRepoFile.h"

#include <thread>
#include <iomanip>

#include "picosha2.h"

#include "COptions.h"
#include "CLogger.h"
#include "CHelpers.h"

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include <io.h> 
#endif

long long CRepoFile::sFilesHashed = 0;
long long CRepoFile::sFilesLinked = 0;
long long CRepoFile::sFilesCopied = 0;

long long CRepoFile::sBytesHashed = 0;
long long CRepoFile::sBytesLinked = 0;
long long CRepoFile::sBytesCopied = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CRepoFile::CRepoFile(
    CPath           sourcePath,
    CPath           parentPath,
    CPath           relativePath,
    CSize           size,
    CDate           date,
    std::string     hash)
    :
    mSourcePath(sourcePath),
    mParentPath(parentPath),
    mRelativePath(relativePath),
    mSize(size),
    mDate(date),
    mHash(hash)
{}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CPath CRepoFile::GetSourcePath() const
{
    return mSourcePath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CPath CRepoFile::GetParentPath() const
{
    return mParentPath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CPath CRepoFile::GetRelativePath() const
{
    return mRelativePath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CSize CRepoFile::GetSize() const
{
    return mSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CDate CRepoFile::GetDate() const
{
    return mDate;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
const std::string& CRepoFile::GetHash() const
{
    return mHash;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepoFile::SetSourcePath(const CPath& sourcePath)
{
    mSourcePath = sourcePath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepoFile::SetParentPath(const CPath& parentPath)
{
    mParentPath = parentPath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepoFile::SetRelativePath(const CPath& relativePath)
{
    mRelativePath = relativePath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepoFile::SetSize(CSize size)
{
    mSize = size;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepoFile::SetDate(CDate date)
{
    mDate = date;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepoFile::SetHash(const std::string& hash)
{
    mHash = hash;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::IsExisting() const
{
    CPath fullPath = GetFullPath();
    return std::experimental::filesystem::exists(fullPath);
}

#if 0
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::IsLinkable() const
{
    CPath fullPath = GetFullPath();
    if (!std::experimental::filesystem::exists(fullPath))
    {
        LogWarning("stored file missing: " + ToString());
        return false;
    }
    if (std::experimental::filesystem::hard_link_count(fullPath) == MAX_HARD_LINK_COUNT)
    {
        LogWarning("stored file reached hard link count limit: " + ToString());
        return false;
    }
    return true;
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::OpenSource()
{
    if (mSourceFileHandle)
    {
        return true;
    }

    mSourceFileHandle = std::make_shared<std::ifstream>();
    for (int i = 0; i < 10; i++)
    {
        mSourceFileHandle->open(mSourcePath.wstring(), std::ios::binary);
        if (mSourceFileHandle->is_open())
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    mSourceFileHandle.reset();

    CLogger::LogWarning("source file not accessible: " + SourceToString());

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepoFile::CloseSource()
{
    mSourceFileHandle.reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::HashSource()
{
    if (!OpenSource())
    {
        CLogger::LogWarning("source file not hashable: " + SourceToString());
        return false;
    }

    mHash = picosha2::hash256_hex_string(std::istreambuf_iterator<char>(*mSourceFileHandle), std::istreambuf_iterator<char>());

    sFilesHashed++;
    sBytesHashed += GetSize();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::Open()
{
    if (mFileHandle)
    {
        return true;
    }

    mFileHandle = std::make_shared<std::ifstream>();
    for (int i = 0; i < 10; i++)
    {
        mFileHandle->open(GetFullPath().wstring(), std::ios::binary);
        if (mFileHandle->is_open())
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    mFileHandle.reset();

    CLogger::LogWarning("file not accessible: " + ToString());

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepoFile::Close()
{
    mFileHandle.reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::Hash()
{
    if (!Open())
    {
        CLogger::LogWarning("file not hashable: " + ToString());
        return false;
    }
        
    mHash = picosha2::hash256_hex_string(std::istreambuf_iterator<char>(*mFileHandle), std::istreambuf_iterator<char>());

    sFilesHashed++;
    sBytesHashed += GetSize();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CPath CRepoFile::GetFullPath() const
{
    return mParentPath / mRelativePath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CRepoFile::SourceToString() const
{
    return CHelpers::NumberAsString(mSize, 15)
        + " " + CHelpers::TimeAsString(mDate)
        + " " + mSourcePath.string();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CRepoFile::ToString() const
{
    return CHelpers::NumberAsString(mSize, 15)
        + " " + CHelpers::TimeAsString(mDate)
        + " " + GetFullPath().string();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CRepoFile::ToCSV() const
{
    std::ostringstream ss;
    ss << std::setw(12) << mSize;

    return ss.str()
        + ", " + CHelpers::TimeAsString(mDate)
        + ", " + GetFullPath().string();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::Copy(const CPath& source) const
{
    std::error_code errorCode;

    CPath directory = GetFullPath().parent_path();
    if (!std::experimental::filesystem::exists(directory) && !std::experimental::filesystem::create_directories(directory, errorCode))
    {
        CLogger::LogWarning("cannot create directories: " + ToString(), errorCode);
        return false;
    }

    if (!std::experimental::filesystem::copy_file(source, GetFullPath(), std::experimental::filesystem::copy_options::copy_symlinks, errorCode))
    {
        CLogger::LogWarning("cannot copy: " + ToString() + " from: " + source.string(), errorCode);
        return false;
    }

    sFilesCopied++;
    sBytesCopied += GetSize();

    LOG_DEBUG("copied: " + ToString() + " from: " + source.string(), GetSize() < COptions::GetSingleton().mHardLinkMinBytes ? COLOR_COPY_SMALL : COLOR_COPY);

    CHelpers::MakeReadOnly(GetFullPath());

#ifndef _WIN32 // fixes modification date not being copied under linux
    auto lastWriteTime = std::experimental::filesystem::last_write_time(source, errorCode);
    if (errorCode)
    {
        LogWarning("cannot read modification date from " + source.string(), errorCode);
    }
    else
    {
        std::experimental::filesystem::last_write_time(GetFullPath(), lastWriteTime, errorCode);
        if (errorCode)
        {
            LogWarning("cannot set modification date of " + ToString(), errorCode);
        }
    }
#endif

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::Link(const CPath& source) const
{
    if (GetSize() < COptions::GetSingleton().mHardLinkMinBytes)
    {
        return Copy(source);
    }

    std::error_code errorCode;

    CPath directory = GetFullPath().parent_path();
    if (!std::experimental::filesystem::exists(directory) && !std::experimental::filesystem::create_directories(directory, errorCode))
    {
        CLogger::LogWarning("cannot create directories: " + ToString(), errorCode);
        return false;
    }

    std::experimental::filesystem::create_hard_link(source, GetFullPath(), errorCode);
    if (errorCode)
    {
        if (errorCode.value() == 1142)
        {
            CLogger::LogWarning("too many links: " + ToString() + " from: " + source.string());
        }
        else
        {
            CLogger::LogWarning("cannot link: " + ToString() + " from: " + source.string(), errorCode);
        }
        return false;
    }

    sFilesLinked++;
    sBytesLinked += GetSize();

    LOG_DEBUG("linked: " + ToString() + " from: " + source.string(), COLOR_LINK);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::GetFileInfo(unsigned long long& fileSystemIndex, int& hardLinkCount) const
{
    fileSystemIndex = 0;
    hardLinkCount = 0;

#ifdef _WIN32

    HANDLE handle = CreateFileW(GetFullPath().wstring().c_str(), GENERIC_READ, FILE_SHARE_READ,         NULL, OPEN_EXISTING, 0, NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
        CLogger::LogWarning("cannot read file information: " + ToString());
        return false;
    }

    BY_HANDLE_FILE_INFORMATION fileInformation;
    if (!GetFileInformationByHandle(handle, &fileInformation))
    {
        CLogger::LogWarning("cannot read file information: " + ToString());
        CloseHandle(handle);
        return false;
    }
    CloseHandle(handle);

    fileSystemIndex = (unsigned long long(fileInformation.nFileIndexHigh) << 32) + unsigned long long(fileInformation.nFileIndexLow);
    hardLinkCount = fileInformation.nNumberOfLinks;
    return true;

#else

    LogWarning("TODO: implement inode access to avoid multiple hashing of linked files");
    return false;

#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepoFile::StaticLogStats()
{
    CLogger::Log("hashed: " + CHelpers::NumberAsString(sFilesHashed, 15) + " files " + CHelpers::NumberAsString(sBytesHashed, 19) + " bytes");
    CLogger::Log("copied: " + CHelpers::NumberAsString(sFilesCopied, 15) + " files " + CHelpers::NumberAsString(sBytesCopied, 19) + " bytes");
    CLogger::Log("linked: " + CHelpers::NumberAsString(sFilesLinked, 15) + " files " + CHelpers::NumberAsString(sBytesLinked, 19) + " bytes");
}
