@echo off
g++ -O3 -o ThemisEngine.exe main.cpp board.cpp init.cpp movegen.cpp attack.cpp makemove.cpp search.cpp eval.cpp uci.cpp tt.cpp
if %errorlevel% neq 0 exit /b %errorlevel%
echo Build Successful!
ThemisEngine.exe
