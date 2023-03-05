pushd %~dp0
..\x64\Release\Backup.exe distill %1 -verbose -compact_db -verify_accessible
popd
pause
