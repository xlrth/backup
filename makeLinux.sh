#!/bin/bash
c++ -o backup -flto=auto -O3 -std=c++20 \
-lsqlite3 -lstdc++fs    \
src/CCmdBackup.cpp      \
src/CCmdClone.cpp       \
src/CCmdDistill.cpp     \
src/CCmdPurge.cpp       \
src/CCmdVerify.cpp      \
src/CFileTable.cpp      \
src/CLogger.cpp         \
src/COptions.cpp        \
src/CPath.cpp           \
src/CRepoFile.cpp       \
src/CRepository.cpp     \
src/CSize.cpp           \
src/CSnapshot.cpp       \
src/CSqliteWrapper.cpp  \
src/CTime.cpp           \
src/Helpers.cpp         \
src/Main.cpp            \
