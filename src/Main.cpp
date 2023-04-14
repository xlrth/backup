#include <string>
#include <deque>
#include <vector>
#include <map>

#include "COptions.h"
#include "CLogger.h"
#include "CCmdBackup.h"
#include "CCmdVerify.h"
#include "CCmdPurge.h"
#include "CCmdDistill.h"
#include "CCmdClone.h"

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   undef max
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
std::vector<std::pair<std::string, std::shared_ptr<CCmd>>> CreateCommands()
{
    return
    {
        { "backup"  , std::make_shared<CCmdBackup>()   },
        { "verify"  , std::make_shared<CCmdVerify>()   },
        { "purge"   , std::make_shared<CCmdPurge>()    },
        { "distill" , std::make_shared<CCmdDistill>()  },
        { "clone"   , std::make_shared<CCmdClone>()    },
    };
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CPath GetExecutablePath(const char* argv0)
{
#ifdef _WIN32
    WCHAR result[1024] = { '\0' };
    if (0 == ::GetModuleFileName(NULL, result, 1024))
    {
        return argv0;
    }
    return std::wstring(result);
#else
    return argv0;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void PrintUsage(const char* argv0)
{
    int COL1 = 8;
    int COL2 = 37;
    CLogger::GetInstance().Log("usage:");
    for (auto& command : CreateCommands())
    {
        CLogger::GetInstance().Log(
            "  " + GetExecutablePath(argv0).filename().string()
            + " " + command.first + std::string(std::max(0, COL1 - int(command.first.length())), ' ')
            + command.second->GetUsageSpec() + std::string(std::max(0, COL2 - int(command.second->GetUsageSpec().length())), ' ')
            + command.second->GetOptionsSpec().GetUsageString());
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool ParseCmdLine(int argc, char** argv, std::shared_ptr<CCmd>& command, std::vector<CPath>& paths, COptions& options)
{
    if (argc < 2)
    {
        return false;
    }
    
    auto commandVec = CreateCommands();
    std::map<std::string, std::shared_ptr<CCmd>> commandMap(commandVec.begin(), commandVec.end());

    if (commandMap.find(argv[1]) == commandMap.end())
    {
        return false;
    }
    command = commandMap.at(argv[1]);
    options = command->GetOptionsSpec();

    paths.clear();

    std::deque<std::string> arguments(argv + 2, argv + argc);
    while (!arguments.empty())
    {
        if (!options.IsCmdLineOption(arguments.front()))
        {
            paths.push_back(arguments.front());
        }
        else if (!options.ParseCmdLineArg(arguments.front()))
        {
            return false;
        }
        arguments.pop_front();
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
    bool successful = Helpers::TryCatchEval([argc, argv]()
    {
        VERIFY(argc > 0);

        std::shared_ptr<CCmd>   command;
        std::vector<CPath>      paths;
        COptions                options;
        if (!ParseCmdLine(argc, argv, command, paths, options))
        {
            PrintUsage(argv[0]);
            return false;
        }
        if (!command->Run(paths, options))
        {
            PrintUsage(argv[0]);
            return false;
        }
        return true;
    });

    if (successful)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}
