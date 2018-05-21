#include <string>
#include <experimental/filesystem>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "helpers.h"

#ifdef _WIN32 
#   ifdef _DEBUG
#       define WIN32_LEAN_AND_MEAN
        // needed for OutputDebugString
#       include <Windows.h>
#       undef min
#       undef max
#   endif
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string TimeAsString(std::chrono::system_clock::time_point time)
{
    auto itt = std::chrono::system_clock::to_time_t(time);
    std::ostringstream ss;
    ss << std::put_time(localtime(&itt), "%Y-%m-%d-%H-%M-%S");
    return ss.str();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CurrentTimeAsString()
{
    return TimeAsString(std::chrono::system_clock::now());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string ToUpper(const std::string& str)
{
    std::string result = str;
    std::transform(str.begin(), str.end(), result.begin(), ::toupper);
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void Log(FILE* logFile, const std::string& str)
{
    if (logFile != nullptr)
    {
        fputs(str.c_str(), logFile);
        fputs("\n", logFile);
    }

    std::cout << str << std::endl;

#if (defined _WIN32) && (defined _DEBUG)
    OutputDebugStringA(str.c_str());
    OutputDebugStringA("\n");
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool MakeReadOnly(const std::experimental::filesystem::path& file)
{
    std::error_code errorCode;
    std::experimental::filesystem::permissions(file,
          std::experimental::filesystem::perms::owner_read
        | std::experimental::filesystem::perms::group_read
        | std::experimental::filesystem::perms::others_read,
        errorCode);
    return !errorCode;
}
