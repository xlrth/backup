#include "CRepoFile.h"

#include <thread>
#include <iomanip>
#include <regex>

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include <io.h> 
#   undef CreateDirectory
#else
#   include <sys/stat.h>
#   include <fcntl.h>
#   include <ext/stdio_filebuf.h>
#endif

#include "picosha2.h"

#include "COptions.h"
#include "CLogger.h"
#include "Helpers.h"

#ifdef _WIN32
static constexpr long long  MAX_HARD_LINK_COUNT = 1023;
static constexpr long long  HARD_LINK_MIN_BYTES = 513;
#else
#   include <linux/limits.h>
static constexpr long long  MAX_HARD_LINK_COUNT = 65000;
static constexpr long long  HARD_LINK_MIN_BYTES = 0;
#endif


long long CRepoFile::sFilesHashed   = 0;
long long CRepoFile::sFilesLinked   = 0;
long long CRepoFile::sFilesCopied   = 0;
long long CRepoFile::sFilesDeleted  = 0;

long long CRepoFile::sBytesHashed   = 0;
long long CRepoFile::sBytesLinked   = 0;
long long CRepoFile::sBytesCopied   = 0;
long long CRepoFile::sBytesDeleted  = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CRepoFile::CRepoFile(
    CPath           sourcePath,
    CSize           size,
    CTime           time,
    std::string     hash,
    CPath           relativePath,
    CPath           parentPath)
    :
    mSourcePath(sourcePath),
    mSize(size),
    mTime(time),
    mHash(hash),
    mRelativePath(relativePath),
    mParentPath(parentPath)
{}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CPath CRepoFile::GetSourcePath() const
{
    return mSourcePath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CSize CRepoFile::GetSize() const
{
    return mSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CTime CRepoFile::GetTime() const
{
    return mTime;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
const std::string& CRepoFile::GetHash() const
{
    return mHash;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::HasHash() const
{
    return !mHash.empty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CPath CRepoFile::GetRelativePath() const
{
    return mRelativePath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CPath CRepoFile::GetParentPath() const
{
    return mParentPath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CPath CRepoFile::GetFullPath() const
{
    return mParentPath / mRelativePath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepoFile::SetSourcePath(const CPath& sourcePath)
{
    mSourcePath = sourcePath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepoFile::SetSize(CSize size)
{
    mSize = size;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepoFile::SetTime(CTime time)
{
    mTime = time;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepoFile::SetHash(const std::string& hash)
{
    mHash = hash;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepoFile::SetRelativePath(const CPath& relativePath)
{
    mRelativePath = relativePath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepoFile::SetParentPath(const CPath& parentPath)
{
    mParentPath = parentPath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::IsExisting() const
{
    return std::filesystem::exists(GetFullPath());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::IsLinkable() const
{
    if (GetSize() < HARD_LINK_MIN_BYTES)
    {
        return true;
    }

    std::error_code errorCode;
    auto hardLinkCount = std::filesystem::hard_link_count(GetFullPath(), errorCode);
    if (errorCode)
    {
        CLogger::GetInstance().LogWarning("cannot get hard link count: " + ToString(), errorCode);
        return false;
    }

    if (hardLinkCount >= MAX_HARD_LINK_COUNT)
    {
        LOG_DEBUG("hard link limit reached: " + ToString(), COLOR_DARK_YELLOW);
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::ReadSourceProperties()
{
    std::error_code errorCode;

    auto size = std::filesystem::file_size(GetSourcePath(), errorCode);
    if (errorCode)
    {
        CLogger::GetInstance().LogWarning("cannot get file size: " + ToString(), errorCode);
        return false;
    }
    SetSize(size);

    auto time = std::filesystem::last_write_time(GetSourcePath(), errorCode);
    if (errorCode)
    {
        CLogger::GetInstance().LogWarning("cannot get write time: " + ToString(), errorCode);
        return false;
    }
    SetTime(time);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::LockSource()
{
    if (mSourceFileHandle)
    {
        return true;
    }

    mSourceFileHandle = std::make_shared<std::ifstream>();
    for (int i = 0; i < 10; i++)
    {
#ifdef _WIN32
        mSourceFileHandle->open(mSourcePath.wstring(), std::ios::binary, _SH_DENYWR);
        if (mSourceFileHandle->is_open())
        {
            return true;
        }
#else
        mSourceFileHandle->open(mSourcePath.string(), std::ios::binary);
        if (mSourceFileHandle->is_open())
        {
            flock fl = { F_RDLCK, SEEK_SET, 0, 0, 0 };
            int fd = static_cast<__gnu_cxx::stdio_filebuf<char> *const>(mSourceFileHandle->rdbuf())->fd();
            if (::fcntl(fd, F_SETFD, &fl) != -1)
            {
                return true;
            }
            mSourceFileHandle->close();
        }
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    mSourceFileHandle.reset();

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepoFile::UnlockSource()
{
    mSourceFileHandle.reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::IsSourceLocked()
{
    return mSourceFileHandle->is_open();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::HashSource()
{
    if (!LockSource())
    {
        return false;
    }

    mHash = picosha2::hash256_hex_string(std::istreambuf_iterator<char>(*mSourceFileHandle), std::istreambuf_iterator<char>());

    sFilesHashed++;
    sBytesHashed += GetSize();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::Hash()
{
    std::ifstream fileHandle(GetFullPath().string(), std::ios::binary);
    if (!fileHandle.is_open())
    {
        return false;
    }

    mHash = picosha2::hash256_hex_string(std::istreambuf_iterator<char>(fileHandle), std::istreambuf_iterator<char>());

    sFilesHashed++;
    sBytesHashed += GetSize();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::Copy(const CPath& source) const
{
    std::error_code errorCode;

    if (!Helpers::CreateDirectory(GetFullPath().parent_path()))
    {
        return false;
    }

    if (!std::filesystem::copy_file(source, GetFullPath(), errorCode))
    {
        CLogger::GetInstance().LogWarning("cannot copy: " + ToString() + " from: " + source.string(), errorCode);
        return false;
    }

    sFilesCopied++;
    sBytesCopied += GetSize();

    if (GetSize() < HARD_LINK_MIN_BYTES)
    {
        LOG_DEBUG("copied (small): " + ToString() + " from: " + source.string(), COLOR_COPY_SMALL);
    }
    else
    {
        LOG_DEBUG("copied: " + ToString() + " from: " + source.string(), COLOR_COPY);
    }

#ifndef _WIN32 // fixes modification time not being copied by linux
    std::filesystem::last_write_time(GetFullPath(), mTime, errorCode);
    if (errorCode)
    {
        CLogger::GetInstance().LogWarning("cannot set modification time of " + ToString(), errorCode);
    }
#endif

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRepoFile::Link(const CPath& source) const
{
    if (GetSize() < HARD_LINK_MIN_BYTES)
    {
        return Copy(source);
    }

    std::error_code errorCode;

    if (!Helpers::CreateDirectory(GetFullPath().parent_path()))
    {
        return false;
    }

    std::filesystem::create_hard_link(source, GetFullPath(), errorCode);
    if (errorCode)
    {
        {
            CLogger::GetInstance().LogWarning("cannot link: " + ToString() + " from: " + source.string(), errorCode);
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
bool CRepoFile::Delete()
{
    Helpers::MakeWritable(GetFullPath());

    std::error_code errorCode;

    std::filesystem::remove(GetFullPath(), errorCode);
    if (errorCode)
    {
        CLogger::GetInstance().LogWarning("cannot delete: " + ToString(), errorCode);
        return false;
    }

    sFilesDeleted++;
    sBytesDeleted += GetSize();

    LOG_DEBUG("deleted: " + ToString(), COLOR_DELETE);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned long long CRepoFile::GetFileSystemIndex() const
{
#ifdef _WIN32
    HANDLE handle = CreateFileW(GetFullPath().wstring().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return static_cast<unsigned long long>(-1);
    }

    BY_HANDLE_FILE_INFORMATION fileInformation;
    if (!GetFileInformationByHandle(handle, &fileInformation))
    {
        CloseHandle(handle);
        return static_cast<unsigned long long>(-1);
    }
    CloseHandle(handle);

    return (static_cast<unsigned long long>(fileInformation.nFileIndexHigh) << 32)
          + static_cast<unsigned long long>(fileInformation.nFileIndexLow);

#else
    struct stat file_stat;
    if (::stat(GetFullPath().string().c_str(), &file_stat) < 0)
    {
        return static_cast<unsigned long long>(-1);
    }

    return file_stat.st_ino;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CRepoFile::SourceToString() const
{
    return      (mSize.IsSpecified() ? Helpers::NumberAsString(mSize, 15) : std::string(15, ' '))
        + " " + (mTime.IsSpecified() ? Helpers::TimeAsString(mTime, true) : std::string(19, ' '))
        + " " + mSourcePath.string();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CRepoFile::ToString() const
{
    return      (mSize.IsSpecified() ? Helpers::NumberAsString(mSize, 15) : std::string(15, ' '))
        + " " + (mTime.IsSpecified() ? Helpers::TimeAsString(mTime, true) : std::string(19, ' '))
        + " " + GetFullPath().string();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CRepoFile::ToCSV() const
{
    std::regex regex("\"");
    std::ostringstream ss;
    ss << std::setw(12) << mSize
        << ","      << Helpers::TimeAsString(mTime, true)
        << ","      << mHash
        << ",\""    << std::regex_replace(GetFullPath().string(), regex, "\"\"") << "\""
        << ",\""    << std::regex_replace(GetSourcePath().string(), regex, "\"\"") << "\"";
    return ss.str();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRepoFile::StaticLogStats()
{
    CLogger::GetInstance().Log("hashed:  " + Helpers::NumberAsString(sFilesHashed, 11)   + " files " + Helpers::NumberAsString(sBytesHashed, 19)    + " bytes");
    CLogger::GetInstance().Log("copied:  " + Helpers::NumberAsString(sFilesCopied, 11)   + " files " + Helpers::NumberAsString(sBytesCopied, 19)    + " bytes");
    CLogger::GetInstance().Log("linked:  " + Helpers::NumberAsString(sFilesLinked, 11)   + " files " + Helpers::NumberAsString(sBytesLinked, 19)    + " bytes");
    CLogger::GetInstance().Log("deleted: " + Helpers::NumberAsString(sFilesDeleted, 11)  + " files " + Helpers::NumberAsString(sBytesDeleted, 19)   + " bytes");

    sFilesHashed  = 0;
    sFilesCopied  = 0;
    sFilesLinked  = 0;
    sFilesDeleted = 0;
    sBytesHashed  = 0;
    sBytesCopied  = 0;
    sBytesLinked  = 0;
    sBytesDeleted = 0;
}
