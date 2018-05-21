#include "backupConfig.h"
#include "helpers.h"

#include <fstream>

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void readConfig(
    const std::experimental::filesystem::path&          configPath, 
    std::experimental::filesystem::path&                repository, 
    std::vector<std::experimental::filesystem::path>&   sources, 
    std::vector<std::string>&                           excludes)
{
    std::ifstream configFile(configPath);
    VERIFY(configFile.is_open());

    repository.clear();
    sources.clear();
    excludes.clear();

    std::string line;
    std::string currentCategory;
    while (std::getline(configFile, line))
    {
        if (line.empty())
        {
            continue;
        }
        if (line.front() == '[')
        {
            VERIFY(line.back() == ']');
            currentCategory = line.substr(1, line.length() - 2);
        }
        else
        {
            if (ToUpper(currentCategory) == "REPOSITORY")
            {
                VERIFY(repository.empty());
                repository = line;
            }
            else if (ToUpper(currentCategory) == "SOURCES")
            {
                sources.push_back(line);
            }
            else if (ToUpper(currentCategory) == "EXCLUDES")
            {
                excludes.push_back(line);
            }
            else
            {
                VERIFY(false && "config: invalid category");
            }
        }
    }
}
