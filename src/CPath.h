#pragma once

#include <experimental/filesystem>

class CPath : public std::experimental::filesystem::path
{
public:
    CPath() = default;
    CPath(CPath&&) = default;
    CPath(const CPath&) = default;

    CPath(const std::experimental::filesystem::path& other);
    CPath(std::experimental::filesystem::path&& other);

    CPath(const std::string& s);
    CPath(std::string&& s);

    CPath(const char* s);

    CPath& operator = (CPath&& other) = default;
    CPath& operator = (const CPath& other) = default;
    CPath& operator /= (const CPath& other);

private:
    void FixLongPath();
};

CPath operator / (const CPath& _Left, const CPath& _Right);
