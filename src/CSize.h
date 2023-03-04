#pragma once

class CSize
{
public:
    static constexpr long long UNSPECIFIED = -1;

public:
    CSize() = default;
    CSize(CSize&&) = default;
    CSize(const CSize&) = default;

    CSize(long long value);

    operator long long() const;

    bool IsSpecified() const;

    CSize& operator = (CSize&& other) = default;
    CSize& operator = (const CSize& other) = default;

private:
    long long mValue = UNSPECIFIED;
};
