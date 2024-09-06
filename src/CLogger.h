#pragma once

#include <vector>
#include <string>
#include <fstream>

#include "CPath.h"

// ansi colors. source: https://ss64.com/nt/syntax-ansi.html
inline static const std::string COLOR_DEFAULT          = "\033[0m";
inline static const std::string COLOR_BLACK            = "\033[30m";
inline static const std::string COLOR_DARK_RED         = "\033[31m";
inline static const std::string COLOR_DARK_GREEN       = "\033[32m";
inline static const std::string COLOR_DARK_YELLOW      = "\033[33m";
inline static const std::string COLOR_DARK_BLUE        = "\033[34m";
inline static const std::string COLOR_DARK_MAGENTA     = "\033[35m";
inline static const std::string COLOR_DARK_CYAN        = "\033[36m";
inline static const std::string COLOR_DARK_WHITE       = "\033[37m";
inline static const std::string COLOR_BRIGHT_BLACK     = "\033[90m";
inline static const std::string COLOR_BRIGHT_RED       = "\033[91m";
inline static const std::string COLOR_BRIGHT_GREEN     = "\033[92m";
inline static const std::string COLOR_BRIGHT_YELLOW    = "\033[93m";
inline static const std::string COLOR_BRIGHT_BLUE      = "\033[94m";
inline static const std::string COLOR_BRIGHT_MAGENTA   = "\033[95m";
inline static const std::string COLOR_BRIGHT_CYAN      = "\033[96m";
inline static const std::string COLOR_BRIGHT_WHITE     = "\033[97m";


// generic
inline static const std::string COLOR_DEBUG                 = COLOR_DARK_WHITE;
inline static const std::string COLOR_WARNING               = COLOR_BRIGHT_YELLOW;
inline static const std::string COLOR_ERROR                 = COLOR_BRIGHT_RED;
inline static const std::string COLOR_FATAL                 = COLOR_BRIGHT_RED;

// verbose low-level
inline static const std::string COLOR_COPY                  = COLOR_DARK_GREEN;
inline static const std::string COLOR_COPY_SMALL            = COLOR_BRIGHT_BLACK;
inline static const std::string COLOR_LINK                  = COLOR_BRIGHT_CYAN;
inline static const std::string COLOR_LINK_LIMIT            = COLOR_DARK_YELLOW;
inline static const std::string COLOR_DELETE                = COLOR_DARK_MAGENTA;

// verbose high-level
inline static const std::string COLOR_HASH                  = COLOR_BRIGHT_MAGENTA;
inline static const std::string COLOR_DUP                   = COLOR_DARK_CYAN;
inline static const std::string COLOR_SKIP                  = COLOR_BRIGHT_BLACK;
inline static const std::string COLOR_VERIFY                = COLOR_DARK_BLUE;

// high-level
inline static const std::string COLOR_IMPORT                = COLOR_BRIGHT_GREEN;
inline static const std::string COLOR_EXCLUDE               = COLOR_BRIGHT_BLACK;
inline static const std::string COLOR_PURGE                 = COLOR_BRIGHT_MAGENTA;
inline static const std::string COLOR_DISTILL               = COLOR_BRIGHT_CYAN;
inline static const std::string COLOR_CLONE                 = COLOR_BRIGHT_BLUE;

// code page Windows-1252
inline static const int WINDOWS_CONSOLE_OUTPUT_CODEPAGE = 1252;

// use macro to avoid evaluation of arguments when verbose disabled
#define LOG_DEBUG(str, color) if (CLogger::GetInstance().IsDebugLogEnabled()) { CLogger::GetInstance().Log((str), (color)); }

class CLogger
{
public: // methods static
    static CLogger&   GetInstance();

public: // methods
    void Init(const CPath& basePath);
    void Close();

    void EnableDebugLog(bool enable);
    bool IsDebugLogEnabled() const;

    void Log(const std::string& str, const std::string& color = COLOR_BRIGHT_WHITE);

    void LogWarning(const std::string& str, const std::error_code& errorCode = {});
    void LogError  (const std::string& str, const std::error_code& errorCode = {});
    void LogFatal  (const std::string& str, const std::error_code& errorCode = {});

    void LogTotalEventCount();

private:
    CPath           mLogFilePath;
    std::ofstream   mLogFileHandle;

    std::vector<std::string>                mMessageBuffer;

    bool    mEnableDebugLog = false;

    std::chrono::system_clock::time_point   mSessionStartTime;
    long long                               mSessionErrorCount      = 0;
    long long                               mSessionWarningCount    = 0;
    std::chrono::system_clock::time_point   mTotalStartTime         = std::chrono::system_clock::now();
    long long                               mTotalErrorCount        = 0;
    long long                               mTotalWarningCount      = 0;
};
