@echo off
setlocal

call "%~dp0run.bat" -goreabattle -noupdate -noautoupdate
exit /b %ERRORLEVEL%
