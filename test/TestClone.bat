pushd %~dp0
..\x64\Release\Backup.exe clone %1 %1_cloned --verbose
popd
pause
