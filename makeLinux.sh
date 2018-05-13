#!/bin/bash
c++ -o backup -flto -O3 src/backupConfig.cpp src/helpers.cpp src/backup.cpp src/sqliteWrapper.cpp -lsqlite3 -lstdc++fs
