#pragma once

#include <vector>
#include <experimental/filesystem>

void ReadConfig(
    const std::experimental::filesystem::path&          configPath,
    std::experimental::filesystem::path&                repository,
    std::vector<std::experimental::filesystem::path>&   sources,
    std::vector<std::string>&                           excludes);
