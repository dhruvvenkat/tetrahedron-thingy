@echo off
setlocal

if "%CXX%"=="" set "CXX=g++"
if "%CXXFLAGS%"=="" set "CXXFLAGS=-std=c++20 -O2 -Wall -Wextra -pedantic"

%CXX% %CXXFLAGS% -DANIMATION_LIBRARY main.cpp orbits.cpp waves.cpp starfield.cpp rain.cpp -o anim.exe
if errorlevel 1 exit /b %errorlevel%

echo Built anim.exe
