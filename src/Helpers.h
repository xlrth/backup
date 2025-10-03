#pragma once

#include <string>
#include <chrono>
#include <functional>

#include "CPath.h"

#define VERIFY(expr) if(!(expr)) { throw std::string("\"" #expr "\" failed!"); }

namespace Helpers
{
    std::string     NumberAsString(long long number, int minWidth);
    std::string     TimeAsString(std::chrono::system_clock::time_point time, bool friendlyFormat = false);
    std::string     CurrentTimeAsString(bool friendlyFormat = false);
    std::string     ElapsedTimeAsString(std::chrono::system_clock::time_point start, std::chrono::system_clock::time_point stop = std::chrono::system_clock::now());
    std::string     ToLower(const std::string& str);
    std::u8string   ReinterpretStringAsU8String(const std::string& str);
    std::string     ReinterpretU8StringAsString(const std::u8string& str);

    std::chrono::system_clock::time_point   FileTimeToSystemTime(std::chrono::file_clock::time_point fileTime);
    std::chrono::file_clock::time_point     SystemTimeToFileTime(std::chrono::system_clock::time_point systemTime);

    bool MakeReadOnly(const CPath& file);
    bool MakeWritable(const CPath& file);
    void MakeBackup(const CPath& file);

    bool CreateDirectory(const CPath& dir);
    void DeleteEmptyDirectories(const CPath& dir);

    bool IsPrefixOfPath(const CPath& prefix, const CPath& path);
    bool IsSuffixOfPath(const CPath& suffix, const CPath& path);

    void TryCatch(std::function<void(void)> function);
    bool TryCatchEval(std::function<bool(void)> function);
};