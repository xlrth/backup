#pragma once

#include <string>

#include "Sqlite3.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
class SqliteWrapper
{
public:
    class Statement
    {
    public:
        Statement(sqlite3_stmt* statement);
        ~Statement() noexcept(false);

        bool        HasData();
        long long   ReadInt(int col);
        std::string ReadString(int col);

    private:
        sqlite3_stmt* mStatement;
    };

    SqliteWrapper();
    SqliteWrapper(const std::string& path, bool readOnly);
    SqliteWrapper(SqliteWrapper&& other);
    ~SqliteWrapper() noexcept(false);

    SqliteWrapper& operator = (SqliteWrapper&& other);

    void        Close();
    void        RunQuery(const std::string& query);
    Statement   StartQuery(const std::string& query);

private:
    sqlite3* mDb;
};
