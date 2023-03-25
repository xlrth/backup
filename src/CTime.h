#pragma once

#include <chrono>

class CTime
{
public:
    static constexpr long long UNSPECIFIED = -1;

public:
    CTime() = default;
    CTime(CTime&&) = default;
    CTime(const CTime&) = default;

    CTime(long long value);
    CTime(std::chrono::system_clock::time_point value);
    CTime(std::chrono::file_clock::time_point value);

    bool IsSpecified() const;

    operator std::chrono::system_clock::time_point() const;
    operator std::chrono::file_clock::time_point() const;
    operator long long() const;

    CTime& operator = (CTime&& other) = default;
    CTime& operator = (const CTime& other) = default;

private: // static
    static long long                                SystemTimePointToLongLong(std::chrono::system_clock::time_point timePoint);
    static std::chrono::system_clock::time_point    LongLongToSystemTimePoint(long long longLong);

private:
    long long mValue = UNSPECIFIED;
};
