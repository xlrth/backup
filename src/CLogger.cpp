#include "CLogger.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#ifdef _WIN32 
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#endif

#include "Helpers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CLogger& CLogger::GetInstance()
{
    static CLogger sSingleton;
    return sSingleton;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLogger::Init(const CPath& basePath)
{
    VERIFY(!mLogFileHandle.is_open());

    mSessionStartTime = std::chrono::system_clock::now();
    mSessionErrorCount = 0;
    mSessionWarningCount = 0;

    mLogFilePath = basePath / "log.txt";

    Helpers::MakeBackup(mLogFilePath);
    Helpers::MakeWritable(mLogFilePath);

    mLogFileHandle = std::ofstream(mLogFilePath.string());
    if (!mLogFileHandle.is_open())
    {
        throw "cannot open log file " + mLogFilePath.string();
    }

    for (auto& message : mMessageBuffer)
    {
        mLogFileHandle << message << std::endl;
    }
    mMessageBuffer.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLogger::Close()
{
    Log("elapsed time: " + Helpers::ElapsedTimeAsString(mSessionStartTime));
    Log("warnings: " + std::to_string(mSessionWarningCount), mSessionWarningCount > 0 ? COLOR_WARNING : COLOR_DEFAULT);
    Log("errors:   " + std::to_string(mSessionErrorCount), mSessionErrorCount > 0 ? COLOR_ERROR : COLOR_DEFAULT);

    if (mLogFileHandle.is_open())
    {
        mLogFileHandle.close();

        Helpers::MakeReadOnly(mLogFilePath);
        Helpers::MakeBackup(mLogFilePath);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLogger::LogTotalEventCount()
{
    Log("total elapsed time: "  + Helpers::ElapsedTimeAsString(mTotalStartTime));
    Log("total warnings: "      + std::to_string(mTotalWarningCount), mTotalWarningCount > 0 ? COLOR_WARNING : COLOR_DEFAULT);
    Log("total errors:   "      + std::to_string(mTotalErrorCount), mTotalErrorCount > 0 ? COLOR_ERROR : COLOR_DEFAULT);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLogger::EnableDebugLog(bool enable)
{
    mEnableDebugLog = enable;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLogger::IsDebugLogEnabled() const
{
    return mEnableDebugLog;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLogger::Log(const std::string& str, const std::string& color)
{
    if (mLogFileHandle.is_open())
    {
        mLogFileHandle << str << std::endl;
    }
    else
    {
        mMessageBuffer.push_back(str);
    }

#ifdef _WIN32 
#   ifdef _DEBUG
    OutputDebugStringA(str.c_str());
    OutputDebugStringA("\n");
#   endif

    ::SetConsoleOutputCP(WINDOWS_CONSOLE_OUTPUT_CODEPAGE);
    DWORD dwMode = 0;
    if (!GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &dwMode))
    {
        return;
    }
    ::SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif

    std::cout << color << str << COLOR_DEFAULT << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLogger::LogWarning(const std::string& str, const std::error_code& errorCode)
{
    Log("WARNING: " + str + (errorCode ? " error: " + errorCode.message() : ""), COLOR_WARNING);
    mSessionWarningCount++;
    mTotalWarningCount++;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLogger::LogError(const std::string& str, const std::error_code& errorCode)
{
    Log("ERROR: " + str + (errorCode ? " error: " + errorCode.message() : ""), COLOR_ERROR);
    mSessionErrorCount++;
    mTotalErrorCount++;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLogger::LogFatal(const std::string& str, const std::error_code& errorCode)
{
    Log("FATAL: " + str + (errorCode ? " error: " + errorCode.message() : ""), COLOR_FATAL);
    mSessionErrorCount++;
    mTotalErrorCount++;
}
