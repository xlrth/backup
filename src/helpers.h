#pragma once

#include <string>
#include <experimental/filesystem>

#define VERIFY(expr) if(!(expr)) { throw std::string("\"" #expr "\" failed!"); }

std::string StartProcess(const std::string& cmd);

std::string TimeAsString(std::chrono::system_clock::time_point time);

std::string CurrentTimeAsString();

std::string ToUpper(const std::string& str);

void Log(FILE* logFile, const std::string& str);

bool MakeReadOnly(const std::experimental::filesystem::path& file);
