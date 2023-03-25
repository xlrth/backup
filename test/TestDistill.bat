pushd %~dp0
..\x64\Release\Backup.exe distill --verbose --compact_db %*
popd
pause
