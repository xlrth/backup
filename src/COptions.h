#pragma once

#include <string>
#include <map>
#include <vector>

class COptions
{
public:
    COptions() = default;
    COptions(const std::vector<std::string> boolNames, const std::vector<std::string> stringNames);

    void SetOptionsSpec(const std::vector<std::string> boolNames, const std::vector<std::string> stringNames);

    std::string GetUsageString() const;

    bool IsCmdLineOption(const std::string& arg);
    bool ParseCmdLineArg(const std::string& arg);

    bool        GetBool(const std::string& name) const;
    std::string GetString(const std::string& name) const;

    void Log() const;

private:
    std::vector<std::string> mBoolNames;
    std::vector<std::string> mStringNames;

    std::map<std::string, bool>         mBoolOpts;
    std::map<std::string, std::string>  mStringOpts;
};
