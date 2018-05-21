#include <string>
#include <vector>
#include <experimental/filesystem>

std::ofstream& GetLogFile();

void Backup(
    const std::experimental::filesystem::path&              repository,
    const std::vector<std::experimental::filesystem::path>& sources,
    const std::vector<std::string>&                         excludes,
    bool                                                    verbose);
