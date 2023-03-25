#include "Helpers.h"

#include <iomanip>
#include <sstream>
#include <algorithm>
#include <time.h>

#include "CLogger.h"

static std::string TIME_FORMAT          = "%Y-%m-%d_%H-%M-%S";
static std::string TIME_FORMAT_FRIENDLY = "%Y-%m-%d %H:%M:%S";

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string Helpers::NumberAsString(long long number, int minWidth)
{
    std::string str = std::to_string(number);

    for (int idx = static_cast<int>(str.length()) - 3; idx > 0; idx -= 3)
    {
        str.insert(str.begin() + idx, ',');
    }

    if (str.length() < minWidth)
    {
        str = std::string(minWidth - str.length(), ' ') + str;
    }

    return str;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string Helpers::TimeAsString(std::chrono::system_clock::time_point time, bool friendlyFormat)
{
    auto itt = std::chrono::system_clock::to_time_t(time);
    std::ostringstream ss;
    tm tm;
#ifdef _WIN32
    if (::localtime_s(&tm, &itt) != 0)
    {
        throw "localtime_s failed";
    }
#else
    if (::localtime_r(&itt, &tm) == nullptr)
    {
        throw "localtime_r failed";
    }
#endif
    ss << std::put_time(&tm, (friendlyFormat ? TIME_FORMAT_FRIENDLY : TIME_FORMAT).c_str());
    return ss.str();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string Helpers::CurrentTimeAsString(bool friendlyFormat)
{
    return TimeAsString(std::chrono::system_clock::now(), friendlyFormat);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string Helpers::ElapsedTimeAsString(std::chrono::system_clock::time_point start, std::chrono::system_clock::time_point stop)
{
    return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(stop - start).count()) + "s";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string Helpers::ToLower(const std::string& str)
{
    std::string result = str;
    std::transform(str.begin(), str.end(), result.begin(), [](char c) { return char(::tolower(c)); });
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::chrono::system_clock::time_point Helpers::FileTimeToSystemTime(std::chrono::file_clock::time_point fileTime)
{
#ifdef _WIN32
    return std::chrono::clock_cast<std::chrono::system_clock>(fileTime);
#else
    return std::chrono::file_clock::to_sys(fileTime);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::chrono::file_clock::time_point Helpers::SystemTimeToFileTime(std::chrono::system_clock::time_point systemTime)
{
#ifdef _WIN32
    return std::chrono::clock_cast<std::chrono::file_clock>(systemTime);
#else
    return std::chrono::file_clock::from_sys(systemTime);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool Helpers::MakeReadOnly(const CPath& file)
{
    std::error_code errorCode;
    std::filesystem::permissions(file,
          std::filesystem::perms::owner_write
        | std::filesystem::perms::group_write
        | std::filesystem::perms::others_write,
        std::filesystem::perm_options::remove,
        errorCode);
    if (errorCode)
    {
        CLogger::GetInstance().LogWarning("cannot make read-only: " + file.string(), errorCode);
    }
    return !errorCode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool Helpers::MakeWritable(const CPath& file)
{
    if (!std::filesystem::exists(file))
    {
        return true;
    }

    std::error_code errorCode;
    std::filesystem::permissions(file,
          std::filesystem::perms::owner_write
        | std::filesystem::perms::group_write
        | std::filesystem::perms::others_write,
        std::filesystem::perm_options::add,
        errorCode);
    if (errorCode)
    {
        CLogger::GetInstance().LogWarning("cannot make writable: " + file.string(), errorCode);
    }
    return !errorCode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void Helpers::MakeBackup(const CPath& file)
{
    if (!std::filesystem::exists(file))
    {
        return;
    }

    CPath backupPath = file.parent_path() / (file.stem().string() + "_" 
        + TimeAsString(FileTimeToSystemTime(std::filesystem::last_write_time(file))) + file.extension().string());

    if (!std::filesystem::exists(backupPath))
    {
        std::error_code errorCode;
        if (!std::filesystem::copy_file(file, backupPath, errorCode))
        {
            throw "cannot create backup " + backupPath.string() + ": " + errorCode.message();
        }
#ifndef _WIN32 // fixes modification time not being copied by linux
        auto lastWriteTime = std::filesystem::last_write_time(file, errorCode);
        if (errorCode)
        {
            throw "cannot read modification time from " + file.string() + ": " + errorCode.message();
        }
        else
        {
            std::filesystem::last_write_time(backupPath, lastWriteTime, errorCode);
            if (errorCode)
            {
                throw "cannot set modification time of " + backupPath.string() + ": " + errorCode.message();
            }
        }
#endif
    }

    if (!std::filesystem::exists(backupPath)
        || std::filesystem::file_size(backupPath) != std::filesystem::file_size(file)
        || std::filesystem::last_write_time(backupPath) != std::filesystem::last_write_time(file))
    {
        throw "backup " + backupPath.string() + " seems invalid";
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void Helpers::DeleteEmptyDirectories(const CPath& dir)
{
    if (!std::filesystem::is_directory(dir))
    {
        return;
    }

    for (auto& p : std::filesystem::directory_iterator(dir))
    {
        DeleteEmptyDirectories(p.path());
    }

    if (std::filesystem::is_empty(dir))
    {
        std::error_code errorCode;
        if (!std::filesystem::remove(dir, errorCode))
        {
            CLogger::GetInstance().LogWarning("cannot delete directory: " + dir.string(), errorCode);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool Helpers::IsPrefixOfPath(const CPath& prefix, const CPath& path)
{
    return prefix.native().length() <= path.native().length()
        && std::mismatch(prefix.native().begin(), prefix.native().end(), path.native().begin())
        .first == prefix.native().end();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool Helpers::IsSuffixOfPath(const CPath& suffix, const CPath& path)
{
    return (suffix.native().length() <= path.native().length()
        && std::mismatch(suffix.native().rbegin(), suffix.native().rend(), path.native().rbegin())
        .first == suffix.native().rend());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void Helpers::TryCatch(std::function<void(void)> function)
{
    TryCatchEval([function]() { function(); return true; });
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool Helpers::TryCatchEval(std::function<bool(void)> function)
{
    try
    {
        return function();
    }
    catch (const char* exception)
    {
        CLogger::GetInstance().LogFatal(exception);
        return false;
    }
    catch (const std::string& exception)
    {
        CLogger::GetInstance().LogFatal(exception);
        return false;
    }
    catch (const std::exception& exception)
    {
        CLogger::GetInstance().LogFatal(exception.what());
        return false;
    }
    catch (...)
    {
        CLogger::GetInstance().LogFatal("unknown exception");
        return false;
    }
}
