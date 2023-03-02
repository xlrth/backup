#include "Helpers.h"

#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "CPath.h"

#ifdef _WIN32 
#   include <Windows.h>
HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#   ifdef _DEBUG
#       define WIN32_LEAN_AND_MEAN
        // needed for OutputDebugString
#   endif
#endif

static std::chrono::system_clock::time_point gStartTime;

static CPath            gLogFilePath;
static std::ofstream    gLogFileHandle;

static int  gSessionErrorCount      = 0;
static int  gSessionWarningCount    = 0;
static int  gTotalErrorCount        = 0;
static int  gTotalWarningCount      = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::string NumberAsString(long long number, int minWidth)
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
    std::transform(str.begin(), str.end(), result.begin(), [](char c) { return char(::toupper(c)); });
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void InitLog(const CPath& basePath)
{
    gStartTime = std::chrono::system_clock::now();

    gSessionErrorCount = 0;
    gSessionWarningCount = 0;

    gLogFilePath = basePath / ("log_" + CurrentTimeAsString() + ".txt");
    gLogFileHandle = std::ofstream(gLogFilePath.string());
    VERIFY(gLogFileHandle.is_open());

    if (!::SetConsoleOutputCP(1252))
    {
        LogWarning("SetConsoleCP failed with error code " + ::GetLastError());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CloseLog()
{
    Log("elapsed time: " + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - gStartTime).count()) + "s");
    Log("warnings: " + std::to_string(gSessionWarningCount));
    Log("errors:   " + std::to_string(gSessionErrorCount));

    if (gLogFileHandle.is_open())
    {
        gLogFileHandle.close();
        MakeReadOnly(gLogFilePath);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void LogTotalEventCount()
{
    Log("total warnings: " + std::to_string(gTotalWarningCount));
    Log("total errors:   " + std::to_string(gTotalErrorCount));
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
    SetConsoleTextAttribute(hConsole, static_cast<WORD>(color));
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
void LogWarning(const std::string& str, const std::error_code& errorCode)
{
    Log("WARNING: " + str + (errorCode ? " error: " + errorCode.message() : ""), COLOR_WARNING);
    gSessionWarningCount++;
    gTotalWarningCount++;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void LogError(const std::string& str, const std::error_code& errorCode)
{
    Log("ERROR: " + str + (errorCode ? " error: " + errorCode.message() : ""), COLOR_ERROR);
    gSessionErrorCount++;
    gTotalErrorCount++;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void LogFatal(const std::string& str, const std::error_code& errorCode)
{
    Log("FATAL: " + str + (errorCode ? " error: " + errorCode.message() : ""), COLOR_FATAL);
    gSessionErrorCount++;
    gTotalErrorCount++;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool MakeReadOnly(const CPath& file)
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
        LogWarning("cannot make read-only: " + file.string(), errorCode);
    }
    return !errorCode;
}
