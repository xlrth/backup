#pragma once

#include <string>
#include <vector>

#include "CPath.h"
#include "CRepository.h"

class CBackup
{
public:
    void Backup(const std::vector<CPath>& paths);

private: // methods
    void ReadConfig(
        const CPath&                configPath, 
        std::vector<CPath>&         sources,
        std::vector<std::string>&   excludes);

    void BackupSingleRecursive(const CPath& source, const CPath& destLocal);

private: // variables
    CRepository                 mRepository;
    std::vector<std::string>    mExcludes;
};



