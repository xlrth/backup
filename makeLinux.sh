#!/bin/bash
c++ -o backup -flto -O3 src/Backup.cpp src/BackupConfig.cpp src/Helpers.cpp src/SqliteWrapper.cpp stc/sqlite3.c -lstdc++fs
