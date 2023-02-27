#!/bin/bash
c++ -o backup -flto -O3 src/CBackup.cpp src/CRepoFile.cpp src/CRepository.cpp src/CSnapshot.cpp src/CSqliteWrapper.cpp src/Helpers.cpp src/Main.cpp src/sqlite3.c -lstdc++fs
