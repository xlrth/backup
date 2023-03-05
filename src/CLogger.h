#pragma once

#include <string>
#include <fstream>

#include "CPath.h"

enum class EColor
{
    BLACK,
    DARK_BLUE,
    DARK_GREEN,
    DARK_CYAN,
    DARK_RED,
    DARK_MAGENTA,
    DARK_YELLOW,
    LIGHT_GRAY,
    DARK_GRAY,
    BLUE,
    GREEN,
    CYAN,
    RED,
    MAGENTA,
    YELLOW,
    WHITE,

    INVERSE_RED = int(RED) * 16,
};

static constexpr EColor COLOR_WARNING               = EColor::YELLOW;
static constexpr EColor COLOR_ERROR                 = EColor::RED;
static constexpr EColor COLOR_FATAL                 = EColor::INVERSE_RED;

static constexpr EColor COLOR_COPY                  = EColor::DARK_GREEN;
static constexpr EColor COLOR_COPY_SMALL            = EColor::DARK_GRAY;
static constexpr EColor COLOR_LINK                  = EColor::CYAN;
static constexpr EColor COLOR_DELETE                = EColor::DARK_MAGENTA;

static constexpr EColor COLOR_HASH                  = EColor::MAGENTA;
static constexpr EColor COLOR_HASH_SKIP             = EColor::DARK_GRAY;

static constexpr EColor COLOR_DUP                   = EColor::DARK_CYAN;
static constexpr EColor COLOR_DUP_SKIP              = EColor::DARK_GRAY;

static constexpr EColor COLOR_IMPORT                = EColor::GREEN;
static constexpr EColor COLOR_EXCLUDE               = EColor::BLUE;
static constexpr EColor COLOR_PURGE                 = EColor::MAGENTA;
static constexpr EColor COLOR_DISTILL               = EColor::CYAN;



#define LOG_DEBUG(str, color) if (COptions::GetSingleton().verbose) { CLogger::Log((str), (color)); }

class CLogger
{
public:
    static void Init(const CPath& basePath);
    static void Close();

    static void Log(const std::string& str, EColor color = EColor::WHITE);

    //void LogDebug(const std::string& str, EColor color = EColor::WHITE);
    static void LogWarning(const std::string& str, const std::error_code& errorCode = {});
    static void LogError(const std::string& str, const std::error_code& errorCode = {});
    static void LogFatal(const std::string& str, const std::error_code& errorCode = {});

    static void LogTotalEventCount();

private:
    CPath           mLogFilePath;
    std::ofstream   mLogFileHandle;

    std::chrono::system_clock::time_point   mSessionStartTime;
    int                                     mSessionErrorCount      = 0;
    int                                     mSessionWarningCount    = 0;
    std::chrono::system_clock::time_point   mTotalStartTime         = std::chrono::system_clock::now();
    int                                     mTotalErrorCount        = 0;
    int                                     mTotalWarningCount      = 0;
};
