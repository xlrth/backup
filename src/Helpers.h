#pragma once

#include <string>
#include <chrono>

#define VERIFY(expr) if(!(expr)) { throw std::string("\"" #expr "\" failed!"); }

class CPath;

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

    INVERSE_RED = int(RED) * 256,
};

static constexpr EColor COLOR_WARNING               = EColor::YELLOW;
static constexpr EColor COLOR_ERROR                 = EColor::RED;
static constexpr EColor COLOR_FATAL                 = EColor::INVERSE_RED;

static constexpr EColor COLOR_COPY                  = EColor::DARK_GREEN;
static constexpr EColor COLOR_COPY_SMALL            = EColor::LIGHT_GRAY;
static constexpr EColor COLOR_LINK                  = EColor::CYAN;

static constexpr EColor COLOR_HASH                  = EColor::MAGENTA;
static constexpr EColor COLOR_HASH_SKIP             = EColor::DARK_GRAY;

static constexpr EColor COLOR_ADD                   = EColor::GREEN;
static constexpr EColor COLOR_EXCLUDE               = EColor::BLUE;



std::string NumberAsString(long long number, int minWidth);
std::string TimeAsString(std::chrono::system_clock::time_point time);
std::string CurrentTimeAsString();

std::string ToUpper(const std::string& str);

void InitLog(const CPath& basePath);
void CloseLog();

void LogTotalEventCount();

void Log(const std::string& str, EColor color = EColor::WHITE);

#define LogDebug(str, color) if (COptions::GetSingleton().verbose) { Log((str), (color)); }
//void LogDebug(const std::string& str, EColor color = EColor::WHITE);
void LogWarning(const std::string& str, const std::error_code& errorCode = {});
void LogError(const std::string& str, const std::error_code& errorCode = {});
void LogFatal(const std::string& str, const std::error_code& errorCode = {});

bool MakeReadOnly(const CPath& file);
