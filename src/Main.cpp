#include <string>
#include <deque>
#include <vector>

#include "COptions.h"
#include "CBackup.h"
#include "CVerify.h"
#include "Helpers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void printUsage()
{
    Log("usage:"                                                                                                                                                                    "\n"
        "  backup.exe backup   <repository-dir> <source-config-file> [-verbose] [-always_hash] [-hard_link_min_bytes=n]"                                                            "\n"
        "  backup.exe verify   <snapshot-dir> [<snapshot-dir> ...] [-verbose] [-create_new_snapshot] [-verify_hashes] [-write_file_table]"                                           "\n"
        "  backup.exe recover  <snapshot-dir> [<snapshot-dir> ...] [-verbose] [-create_new_snapshot]"                                                                               "\n"
        "  backup.exe purge    <snapshot-dir> [<snapshot-dir> ...] [-verbose] [-create_new_snapshot]"                                                                               "\n"
        "  backup.exe add      <target-snapshot-dir> <source-snapshot-dir> [<source-snapshot-dir> ...] [-verbose] [-create_new_snapshot] [-ignore_path] [-hard_link_min_bytes=n]"   "\n"
        "  backup.exe subtract <target-snapshot-dir> <source-snapshot-dir> [<source-snapshot-dir> ...] [-verbose] [-create_new_snapshot] [-ignore_path] [-hard_link_min_bytes=n]"   "\n"
    );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
    try
    {
        if (argc < 3)
        {
            printUsage();
            return 1;
        }

        std::string                                         command = ToUpper(argv[1]);
        std::deque<std::string>                             arguments(argv + 2, argv + argc);
        std::vector<CPath>    paths;

        while (!arguments.empty())
        {
            if (ToUpper(arguments.front()) == "-VERBOSE")
            {
                COptions::GetSingletonNonConst().verbose = true;
            }
            else if (ToUpper(arguments.front()) == "-ALWAYS_HASH")
            {
                COptions::GetSingletonNonConst().alwaysHash = true;
            }
            else if (ToUpper(arguments.front()) == "-VERIFY_HASHES")
            {
                COptions::GetSingletonNonConst().verifyHashes = true;
            }
            else if (ToUpper(arguments.front()) == "-WRITE_FILE_TABLE")
            {
                COptions::GetSingletonNonConst().writeFileTable = true;
            }
            else
            {
                paths.push_back(arguments.front());
            }
            arguments.pop_front();
        }

        if (command == "BACKUP")
        {
            CBackup backup;
            backup.Backup(paths);
        }
        else if (command == "VERIFY")
        {
            CVerify verify;
            verify.Verify(paths);
        }
        
    }
    catch (const char* exception)
    {
        LogFatal(exception);
        return 1;
    }
    catch (const std::string& exception)
    {
        LogFatal(exception);
        return 1;
    }
    catch (const std::exception& exception)
    {
        LogFatal(exception.what());
        return 1;
    }
    catch (...)
    {
        LogFatal("unknown exception");
        return 1;
    }

    return 0;
}
