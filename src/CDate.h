#pragma once

#include <chrono>

class CDate
{
public:
    CDate() = default;
    CDate(CDate&&) = default;
    CDate(const CDate&) = default;

    CDate(long long value)
        :
        mValue(value)
    {}

    CDate(std::chrono::system_clock::time_point value)
        :
        mValue(TimePointToLongLong(value))
    {}

    operator std::chrono::system_clock::time_point() const
    {
        return LongLongToTimePoint(mValue);
    }

    operator long long() const
    {
        return mValue;
    }

    CDate& operator = (const CDate& other) = default;

private: // static
    static long long TimePointToLongLong(std::chrono::system_clock::time_point timePoint)
    {
        return timePoint.time_since_epoch().count();
    }

    static std::chrono::system_clock::time_point LongLongToTimePoint(long long longLong)
    {
        return std::chrono::system_clock::time_point(std::chrono::system_clock::duration(longLong));
    }

private:
    long long mValue = 0;
};