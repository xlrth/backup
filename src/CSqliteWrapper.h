#pragma once

#include <string>

#include "CPath.h"

#include "sqlite3.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSqliteWrapper
{
public:
    class CStatement
    {
    public:
        CStatement(CStatement&& other);
        CStatement(sqlite3_stmt* statement);
        ~CStatement();

        bool        HasData();

        long long   ReadInt(int col);
        std::string ReadString(int col);

        void        Finalize();

    private:
        sqlite3_stmt* mStatement;
    };

    CSqliteWrapper();
    CSqliteWrapper(const CPath& path, bool readOnly);
    CSqliteWrapper(CSqliteWrapper&& other);
    ~CSqliteWrapper();

    CSqliteWrapper& operator = (CSqliteWrapper&& other);

    bool        IsOpen() const;
    void        Close();
    void        RunQuery(const std::string& query);
    CStatement  StartQuery(const std::string& query);

private:
    sqlite3* mSqliteHandle;
};
