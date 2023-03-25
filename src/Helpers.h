#pragma once

#include <string>
#include <chrono>

#include "CPath.h"

#define VERIFY(expr) if(!(expr)) { throw std::string("\"" #expr "\" failed!"); }

namespace Helpers
{
    std::string NumberAsString(long long number, int minWidth);
    std::string TimeAsString(std::chrono::system_clock::time_point time, bool friendlyFormat = false);
    std::string CurrentTimeAsString(bool friendlyFormat = false);
    std::string ElapsedTimeAsString(std::chrono::system_clock::time_point start, std::chrono::system_clock::time_point stop = std::chrono::system_clock::now());
    std::string ToLower(const std::string& str);

    std::chrono::system_clock::time_point   FileTimeToSystemTime(std::chrono::file_clock::time_point fileTime);

    bool MakeReadOnly(const CPath& file);
    bool MakeWritable(const CPath& file);
    void MakeBackup(const CPath& file);

    void DeleteEmptyDirectories(const CPath& dir);
};