#pragma once

#include <string>

#include "sqlite3.h"


sqlite3* OpenDB(const std::string& path, bool readOnly);

void CloseDB(sqlite3* db);

void RunQuery(sqlite3* db, const std::string& query);

sqlite3_stmt* StartQuery(sqlite3* db, const std::string& query);

void FinalizeQuery(sqlite3_stmt* st);

bool HasData(sqlite3_stmt* st);

long long ReadInt(sqlite3_stmt* st, int col);
std::string ReadString(sqlite3_stmt* st, int col);
