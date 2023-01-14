#pragma once

#include <string>
#include <experimental/filesystem>
#include <fstream>

#define VERIFY(expr) if(!(expr)) { throw std::string("\"" #expr "\" failed!"); }

std::string BytesAsString(long long bytes, int digits = 12);

std::string TimeAsString(std::chrono::system_clock::time_point time);

std::string CurrentTimeAsString();

std::string ToUpper(const std::string& str);

enum class EColor
{
    BLACK           ,
    DARK_BLUE       ,
    DARK_GREEN      ,
    DARK_CYAN       ,
    DARK_RED        ,
    DARK_MAGENTA    ,
    DARK_YELLOW     ,
    LIGHT_GRAY      ,
    DARK_GRAY       ,
    BLUE            ,
    GREEN           ,
    CYAN            ,
    RED             ,
    MAGENTA         ,
    YELLOW          ,
    WHITE           ,
};

void InitLog(const std::experimental::filesystem::path& basePath);
void Log(const std::string& str, EColor color = EColor::WHITE);
void CloseLog();

bool MakeReadOnly(const std::experimental::filesystem::path& file);
