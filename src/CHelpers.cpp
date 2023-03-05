#include "CHelpers.h"

#include <iomanip>
#include <sstream>

#include "CLogger.h"

static std::string TIME_FORMAT = "%Y-%m-%d_%H-%M-%S";

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CHelpers::NumberAsString(long long number, int minWidth)
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
std::string CHelpers::TimeAsString(std::chrono::system_clock::time_point time)
{
    auto itt = std::chrono::system_clock::to_time_t(time);
    std::ostringstream ss;
    ss << std::put_time(localtime(&itt), TIME_FORMAT.c_str());
    return ss.str();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::chrono::system_clock::time_point CHelpers::StringAsTime(const std::string& string)
{
    struct std::tm tm;
    std::memset(&tm, 0, sizeof(tm));
    std::istringstream iss;
    iss.str(string.substr(0, TIME_FORMAT.length()));
    iss >> std::get_time(&tm, TIME_FORMAT.c_str());
    time_t timeT = mktime(&tm);
    if (timeT == -1)
    {
        return {};
    }
    return std::chrono::system_clock::from_time_t(timeT);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CHelpers::CurrentTimeAsString()
{
    return TimeAsString(std::chrono::system_clock::now());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CHelpers::ElapsedTimeAsString(std::chrono::system_clock::time_point start, std::chrono::system_clock::time_point stop)
{
    return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(stop - start).count()) + "s";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CHelpers::ToUpper(const std::string& str)
{
    std::string result = str;
    std::transform(str.begin(), str.end(), result.begin(), [](char c) { return char(::toupper(c)); });
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CHelpers::MakeReadOnly(const CPath& file)
{
    std::error_code errorCode;
    std::experimental::filesystem::permissions(file,
        std::experimental::filesystem::perms::remove_perms
        | std::experimental::filesystem::perms::owner_write
        | std::experimental::filesystem::perms::group_write
        | std::experimental::filesystem::perms::others_write,
        errorCode);
    if (errorCode)
    {
        CLogger::LogWarning("cannot make read-only: " + file.string(), errorCode);
    }
    return !errorCode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CHelpers::MakeWritable(const CPath& file)
{
    std::error_code errorCode;
    std::experimental::filesystem::permissions(file,
        std::experimental::filesystem::perms::add_perms
        | std::experimental::filesystem::perms::owner_write
        | std::experimental::filesystem::perms::group_write
        | std::experimental::filesystem::perms::others_write,
        errorCode);
    if (errorCode)
    {
        //        CLogger::LogWarning("cannot make writable: " + file.string(), errorCode);
    }
    return !errorCode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHelpers::MakeBackup(const CPath& file)
{
    if (!std::experimental::filesystem::exists(file))
    {
        return;
    }

    CPath backupPath = file.parent_path() / (file.stem().string() + "_" + TimeAsString(std::experimental::filesystem::last_write_time(file)) + file.extension().string());

    if (!std::experimental::filesystem::exists(backupPath))
    {
        std::error_code errorCode;
        if (!std::experimental::filesystem::copy_file(file, backupPath, errorCode))
        {
            throw "cannot create backup " + backupPath.string() + ": " + errorCode.message();
        }
    }

    if (!std::experimental::filesystem::exists(backupPath)
        || std::experimental::filesystem::file_size(backupPath) != std::experimental::filesystem::file_size(file)
        || std::experimental::filesystem::last_write_time(backupPath) != std::experimental::filesystem::last_write_time(file))
    {
        throw "backup " + backupPath.string() + " seems invalid";
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHelpers::DeleteEmptyDirectories(const CPath& dir)
{
    if (!std::experimental::filesystem::is_directory(dir))
    {
        return;
    }

    for (auto& p : std::experimental::filesystem::directory_iterator(dir))
    {
        DeleteEmptyDirectories(p.path());
    }

    if (std::experimental::filesystem::is_empty(dir))
    {
        std::error_code errorCode;
        if (!std::experimental::filesystem::remove(dir, errorCode))
        {
            CLogger::LogWarning("cannot delete directory: " + dir.string(), errorCode);
        }
    }
}
