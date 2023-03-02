#pragma once

#include <experimental/filesystem>

#include "Helpers.h"

#ifdef _WIN32
static constexpr long long MAX_PATH_LENGTH      = _MAX_FILESYS_NAME;
static constexpr long long MAX_HARD_LINK_COUNT  = 1023;
#else
#   include <linux/limits.h>
static constexpr long long MAX_PATH_LENGTH      = PATH_MAX
static constexpr long long MAX_HARD_LINK_COUNT  = TODO: DEFINE FOR NON-WINDOWS;
#endif

class CPath : public std::experimental::filesystem::path
{
public:
    CPath() = default;
    CPath(CPath&&) = default;
    CPath(const CPath&) = default;

    CPath(const std::experimental::filesystem::path& other) 
        :
        std::experimental::filesystem::path(other)
    {
        FixLongPath();
    }

    CPath(std::experimental::filesystem::path&& other)
        :
        std::experimental::filesystem::path(std::move(other))
    {
        FixLongPath();
    }

    CPath(const std::string& s)
        :
        CPath(std::experimental::filesystem::path(s))
    {}

    CPath(std::string&& s)
        :
        CPath(std::experimental::filesystem::path(std::move(s)))
    {}

    CPath(const char* s)
        :
        CPath(std::experimental::filesystem::path(s))
    {}

    CPath& operator = (const CPath& other) = default;

    CPath& operator /= (const CPath& other)
    {
        if (native().size() + other.native().size() + 1 >= MAX_PATH_LENGTH)
        {
            LogWarning("path is too long, truncating: " + string() + "/" + other.string());
            std::experimental::filesystem::path::operator /= (other.native().substr(0, MAX_PATH_LENGTH - native().size() - 2));
        }
        else
        {
            std::experimental::filesystem::path::operator /= (other);
        }
        return *this;
    }

private:
    void FixLongPath()
    {
        if (native().size() >= MAX_PATH_LENGTH)
        {
            LogWarning("path is too long, truncating: " + string());
            *this = std::experimental::filesystem::path(native().substr(0, MAX_PATH_LENGTH - 1));
        }
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CPath operator / (const CPath& _Left, const CPath& _Right)
{
    CPath result = _Left;
    return (result /= _Right);
}
