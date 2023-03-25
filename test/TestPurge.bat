pushd %~dp0
..\x64\Release\Backup.exe purge --verbose --compact_db %*
popd
pause
