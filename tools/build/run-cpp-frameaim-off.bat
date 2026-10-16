@echo off
rem Mouse look turned only at simulation steps (the behaviour before frame aim).
rem Same as run-cpp.bat (same arguments), plus: -frameaim off
call "%~dp0run-cpp.bat" %* -- -frameaim off
exit /b %errorlevel%
