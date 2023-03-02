#pragma once

#include <string>
#include <vector>

#include "CPath.h"
#include "CRepository.h"

class CBackup
{
public:
    static void Run(const std::vector<CPath>& paths);

private: // static methods
    static void ReadConfig(
        const CPath&                configPath, 
        std::vector<CPath>&         sources,
        std::vector<std::string>&   excludes);

    static void BackupSingleRecursive(
        const CPath&                    sourcePath,
        const CPath&                    targetPathRelative,
        const std::vector<std::string>& excludes,
        CRepository&                    repository);
};
