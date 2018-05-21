#include "Backup.h"
#include "BackupConfig.h"
#include "Helpers.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
    try
    {
        if (argc == 1 || argc > 3 || (argc == 3 && ToUpper(argv[2]) != "-VERBOSE"))
        {
            Log(GetLogFile(), "usage: backup <config-file> [-verbose]");
            return 1;
        }

        std::experimental::filesystem::path                 repository;
        std::vector<std::experimental::filesystem::path>    sources;
        std::vector<std::string>                            excludes;

        readConfig(argv[1], repository, sources, excludes);

        for (auto& exclude : excludes)
        {
            exclude = ToUpper(exclude);
        }

        Backup(repository, sources, excludes, argc == 3);

        return 0;
    }
    catch (const std::string& exception)
    {
        Log(GetLogFile(), std::string("exception: ") + exception);
    }
    catch (const std::exception& exception)
    {
        Log(GetLogFile(), std::string("exception: ") + exception.what());
    }
    catch (...)
    {
        Log(GetLogFile(), "unknown exception");
    }

    return 1;
}
