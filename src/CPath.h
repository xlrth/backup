#pragma once

#include <filesystem>

class CPath : public std::filesystem::path
{
public:
    CPath() = default;
    CPath(CPath&&) = default;
    CPath(const CPath&) = default;

    CPath(const std::filesystem::path& other);
    CPath(std::filesystem::path&& other);

    CPath(const std::wstring& s);
    CPath(std::wstring&& s);

    CPath(const std::string& s);
    CPath(std::string&& s);

    CPath(const char* s);

    CPath& operator = (CPath&& other) = default;
    CPath& operator = (const CPath& other) = default;
    CPath& operator /= (const CPath& other);
    CPath& operator += (const CPath& other);

    std::string u8StringAsString() const;

private:
    void FixLongPath();
};

