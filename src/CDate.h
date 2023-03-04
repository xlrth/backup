#pragma once

#include <chrono>

class CDate
{
public:
    static constexpr long long UNSPECIFIED = -1;

public:
    CDate() = default;
    CDate(CDate&&) = default;
    CDate(const CDate&) = default;

    CDate(long long value);
    CDate(std::chrono::system_clock::time_point value);

    bool IsSpecified() const;

    operator std::chrono::system_clock::time_point() const;
    operator long long() const;

    CDate& operator = (CDate&& other) = default;
    CDate& operator = (const CDate& other) = default;

private: // static
    static long long TimePointToLongLong(std::chrono::system_clock::time_point timePoint);
    static std::chrono::system_clock::time_point LongLongToTimePoint(long long longLong);

private:
    long long mValue = UNSPECIFIED;
};
