#include "Backup.h"
#include "BackupConfig.h"
#include "Helpers.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
    try
    {
        bool verbose = false;
        bool alwaysHash = false;
        bool usageError = false;

        if (argc < 2)
        {
            usageError = true;
        }

        while (argc > 2)
        {
            if (ToUpper(argv[argc - 1]) == "-VERBOSE")
            {
                verbose = true;
                argc--;
                continue;
            }
            else if (ToUpper(argv[argc - 1]) == "-ALWAYS_HASH")
            {
                alwaysHash = true;
                argc--;
                continue;
            }
            else
            {
                usageError = true;
                break;
            }
        }

        if (usageError)
        {
            Log("usage: backup <config-file> [-verbose] [-always_hash]");
            return 1;
        }

        std::experimental::filesystem::path                 repository;
        std::vector<std::experimental::filesystem::path>    sources;
        std::vector<std::string>                            excludes;

        ReadConfig(argv[1], repository, sources, excludes);

        for (auto& exclude : excludes)
        {
            exclude = ToUpper(exclude);
        }

        Backup(repository, sources, excludes, verbose, alwaysHash);
    }
    catch (const std::string& exception)
    {
        Log(std::string("exception: ") + exception, EColor::RED);
        return 1;
    }
    catch (const std::exception& exception)
    {
        Log(std::string("exception: ") + exception.what(), EColor::RED);
        return 1;
    }
    catch (...)
    {
        Log("unknown exception", EColor::RED);
        return 1;
    }

    return 0;
}
