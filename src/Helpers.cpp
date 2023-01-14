#include <string>
#include <experimental/filesystem>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "Helpers.h"

#ifdef _WIN32 
#   include <Windows.h>
HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#   ifdef _DEBUG
#       define WIN32_LEAN_AND_MEAN
        // needed for OutputDebugString
#   endif
#endif

static std::experimental::filesystem::path  gLogFilePath;
static std::ofstream                        gLogFileHandle;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string BytesAsString(long long bytes, int digits)
{
    int maxLength = digits + digits / 3 - 1;

    std::string str = std::to_string(bytes);

    for (int idx = static_cast<int>(str.length()) - 3; idx > 0; idx -= 3)
    {
        str.insert(str.begin() + idx, ',');
    }

    if (str.length() < maxLength)
    {
        str = std::string(maxLength - str.length(), ' ') + str;
    }

    return str;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string TimeAsString(std::chrono::system_clock::time_point time)
{
    auto itt = std::chrono::system_clock::to_time_t(time);
    std::ostringstream ss;
    ss << std::put_time(localtime(&itt), "%Y-%m-%d_%H-%M-%S");
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
void InitLog(const std::experimental::filesystem::path& basePath)
{
    gLogFilePath = basePath / "log.txt";

    gLogFileHandle = std::ofstream(gLogFilePath.string());
    VERIFY(gLogFileHandle.is_open());

    if (!::SetConsoleOutputCP(1252))
    {
        Log("ERROR: SetConsoleCP failed with error code " + ::GetLastError(), EColor::RED);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CloseLog()
{
    gLogFileHandle.close();

    if (!MakeReadOnly(gLogFilePath))
    {
        Log("ERROR: Cannot make read-only: " + gLogFilePath.string(), EColor::RED);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void Log(const std::string& str, EColor color)
{
    if (gLogFileHandle.is_open())
    {
        gLogFileHandle << str << std::endl;
    }

#ifdef _WIN32 
    SetConsoleTextAttribute(hConsole, static_cast<int>(color));
// alternate solution:
//    std::cout << "\033[32mThis is green\033[0m" << std::endl;
#endif

    std::cout << str << std::endl;

#ifdef _WIN32 
    SetConsoleTextAttribute(hConsole, 15);

#   ifdef _DEBUG
    OutputDebugStringA(str.c_str());
    OutputDebugStringA("\n");
#   endif
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool MakeReadOnly(const std::experimental::filesystem::path& file)
{
    std::error_code errorCode;
    std::experimental::filesystem::permissions(file,
          std::experimental::filesystem::perms::remove_perms
        | std::experimental::filesystem::perms::owner_write
        | std::experimental::filesystem::perms::group_write
        | std::experimental::filesystem::perms::others_write,
        errorCode);
    return !errorCode;
}
