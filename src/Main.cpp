#include <string>
#include <deque>
#include <vector>

#include "COptions.h"
#include "CLogger.h"
#include "CHelpers.h"
#include "CBackup.h"
#include "CVerify.h"
#include "CPurge.h"
#include "CDistill.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void printUsage()
{
    CLogger::Log("usage:" "\n"
        "  backup.exe backup   <repository-dir> <source-config-file> [-verbose] [-always_hash] [-skip_unchanged] [-verify_accessible] [-suffix=s]" "\n"
        "  backup.exe verify   [<repository-dir>] [<snapshot-dir> ...] [-verbose] [-verify_hashes] [-write_file_table]" "\n"
        "  backup.exe purge    <snapshot-dir> [<snapshot-dir> ...] [-verbose] [-compact_db]" "\n"
        "  backup.exe distill  <snapshot-dir> [<snapshot-dir> ...] [-verbose] [-compact_db] [-verify_accessible]" "\n"
    );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
    try
    {
        std::string                 command = CHelpers::ToUpper(argv[1]);
        std::deque<std::string>     arguments(argv + 2, argv + argc);
        std::vector<CPath>          paths;

        while (!arguments.empty())
        {
            if (CHelpers::ToUpper(arguments.front()) == "-VERBOSE")
            {
                COptions::GetSingletonNonConst().verbose = true;
            }
            else if (CHelpers::ToUpper(arguments.front()) == "-ALWAYS_HASH")
            {
                COptions::GetSingletonNonConst().alwaysHash = true;
            }
            else if (CHelpers::ToUpper(arguments.front()) == "-SKIP_UNCHANGED")
            {
                COptions::GetSingletonNonConst().skipUnchanged = true;
            }
            else if (CHelpers::ToUpper(arguments.front()) == "-VERIFY_HASHES")
            {
                COptions::GetSingletonNonConst().verifyHashes = true;
            }
            else if (CHelpers::ToUpper(arguments.front()) == "-WRITE_FILE_TABLE")
            {
                COptions::GetSingletonNonConst().writeFileTable = true;
            }
            else if (CHelpers::ToUpper(arguments.front()) == "-COMPACT_DB")
            {
                COptions::GetSingletonNonConst().compactDB = true;
            }
            else if (CHelpers::ToUpper(arguments.front()) == "-VERIFY_ACCESSIBLE")
            {
                COptions::GetSingletonNonConst().verifyAccessible = true;
            }
            else if (CHelpers::ToUpper(arguments.front()).substr(0, std::strlen("-SUFFIX=")) == "-SUFFIX=")
            {
                COptions::GetSingletonNonConst().suffix = arguments.front().substr(std::strlen("-SUFFIX="));
            }
            else
            {
                paths.push_back(arguments.front());
            }
            arguments.pop_front();
        }

        if (command == "BACKUP")
        {
            if (!CBackup::Run(paths))
            {
                printUsage();
                return 1;
            }
        }
        else if (command == "VERIFY")
        {
            if (!CVerify::Run(paths))
            {
                printUsage();
                return 1;
            }
        }
        else if (command == "PURGE")
        {
            if (!CPurge::Run(paths))
            {
                printUsage();
                return 1;
            }
        }
        else if (command == "DISTILL")
        {
            if (!CDistill::Run(paths))
            {
                printUsage();
                return 1;
            }
        }

    }
    catch (const char* exception)
    {
        CLogger::LogFatal(exception);
        return 1;
    }
    catch (const std::string& exception)
    {
        CLogger::LogFatal(exception);
        return 1;
    }
    catch (const std::exception& exception)
    {
        CLogger::LogFatal(exception.what());
        return 1;
    }
    catch (...)
    {
        CLogger::LogFatal("unknown exception");
        return 1;
    }

    return 0;
}
