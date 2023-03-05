pushd %~dp0
..\x64\Release\Backup.exe verify %1 -verbose -verify_hashes -write_file_table
popd
pause
