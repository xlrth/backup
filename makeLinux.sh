#!/bin/bash
c++ -o backup -flto -O3 src/Main.cpp src/Backup.cpp src/BackupConfig.cpp src/Helpers.cpp src/SqliteWrapper.cpp src/sqlite3.c -lstdc++fs
