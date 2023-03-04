#include "CLogger.h"

#include "CHelpers.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#ifdef _WIN32 
#   include <Windows.h>
HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#   ifdef _DEBUG
#       define WIN32_LEAN_AND_MEAN
        // needed for OutputDebugString
#   endif
#endif

static CLogger sSingleton;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLogger::Init(const CPath& basePath)
{
    sSingleton.mSessionStartTime = std::chrono::system_clock::now();
    sSingleton.mSessionErrorCount = 0;
    sSingleton.mSessionWarningCount = 0;

    sSingleton.mLogFilePath = basePath / "log.txt";

    CHelpers::MakeBackup(sSingleton.mLogFilePath);
    CHelpers::MakeWritable(sSingleton.mLogFilePath);

    sSingleton.mLogFileHandle = std::ofstream(sSingleton.mLogFilePath.string());
    if (!sSingleton.mLogFileHandle.is_open())
    {
        throw "cannot open log file " + sSingleton.mLogFilePath.string();
    }

    if (!::SetConsoleOutputCP(1252))
    {
        LogWarning("SetConsoleCP failed with error code " + ::GetLastError());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLogger::Close()
{
    Log("elapsed time: " + CHelpers::ElapsedTimeAsString(sSingleton.mSessionStartTime));
    Log("warnings: " + std::to_string(sSingleton.mSessionWarningCount));
    Log("errors:   " + std::to_string(sSingleton.mSessionErrorCount));

    if (sSingleton.mLogFileHandle.is_open())
    {
        sSingleton.mLogFileHandle.close();

        CHelpers::MakeReadOnly(sSingleton.mLogFilePath);
        CHelpers::MakeBackup(sSingleton.mLogFilePath);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLogger::LogTotalEventCount()
{
    Log("total elapsed time: "  + CHelpers::ElapsedTimeAsString(sSingleton.mTotalStartTime));
    Log("total warnings: "      + std::to_string(sSingleton.mTotalWarningCount));
    Log("total errors:   "      + std::to_string(sSingleton.mTotalErrorCount));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLogger::Log(const std::string& str, EColor color)
{
    if (sSingleton.mLogFileHandle.is_open())
    {
        sSingleton.mLogFileHandle << str << std::endl;
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
void CLogger::LogWarning(const std::string& str, const std::error_code& errorCode)
{
    Log("WARNING: " + str + (errorCode ? " error: " + errorCode.message() : ""), COLOR_WARNING);
    sSingleton.mSessionWarningCount++;
    sSingleton.mTotalWarningCount++;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLogger::LogError(const std::string& str, const std::error_code& errorCode)
{
    Log("ERROR: " + str + (errorCode ? " error: " + errorCode.message() : ""), COLOR_ERROR);
    sSingleton.mSessionErrorCount++;
    sSingleton.mTotalErrorCount++;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLogger::LogFatal(const std::string& str, const std::error_code& errorCode)
{
    Log("FATAL: " + str + (errorCode ? " error: " + errorCode.message() : ""), COLOR_FATAL);
    sSingleton.mSessionErrorCount++;
    sSingleton.mTotalErrorCount++;
}
