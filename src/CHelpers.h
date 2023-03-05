#pragma once

#include <string>
#include <chrono>

#include "CPath.h"

#define VERIFY(expr) if(!(expr)) { throw std::string("\"" #expr "\" failed!"); }

class CHelpers
{
public:
    static std::string NumberAsString(long long number, int minWidth);

    static std::string                              TimeAsString(std::chrono::system_clock::time_point time);
    static std::chrono::system_clock::time_point    StringAsTime(const std::string& string);

    static std::string CurrentTimeAsString();
    static std::string ElapsedTimeAsString(std::chrono::system_clock::time_point start, std::chrono::system_clock::time_point stop = std::chrono::system_clock::now());

    static std::string ToUpper(const std::string& str);

    static bool MakeReadOnly(const CPath& file);
    static bool MakeWritable(const CPath& file);
    static void MakeBackup(const CPath& file);
    static void DeleteEmptyDirectories(const CPath& dir);
};