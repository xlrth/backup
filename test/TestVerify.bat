pushd %~dp0
..\x64\Release\Backup.exe verify --verbose --verify_hash --write_file_table %*
popd
pause
