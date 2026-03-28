@echo off
setlocal

echo ===== Build Taskbar Monitor =====

if not exist build mkdir build

set CXX=g++
set RC=windres
set CXXFLAGS=-std=c++17 -DUNICODE -D_UNICODE -Wall -Wextra
set LDFLAGS=-municode -mwindows -lgdi32 -lcomctl32 -lcomdlg32 -lshell32 -lole32 -loleaut32 -luuid -ldwmapi -luxtheme -liphlpapi

%CXX% -c src\main.cpp %CXXFLAGS% -o build\main.o
if errorlevel 1 goto error

%CXX% -c src\App.cpp %CXXFLAGS% -o build\App.o
if errorlevel 1 goto error

%CXX% -c src\Config.cpp %CXXFLAGS% -o build\Config.o
if errorlevel 1 goto error

%CXX% -c src\MonitorService.cpp %CXXFLAGS% -o build\MonitorService.o
if errorlevel 1 goto error

%CXX% -c src\TaskbarWidget.cpp %CXXFLAGS% -o build\TaskbarWidget.o
if errorlevel 1 goto error

%CXX% -c src\OptionsWindow.cpp %CXXFLAGS% -o build\OptionsWindow.o
if errorlevel 1 goto error

%CXX% -c src\UiResources.cpp %CXXFLAGS% -o build\UiResources.o
if errorlevel 1 goto error

%RC% --codepage=65001 res\resource.rc -O coff -o build\resource.o
if errorlevel 1 goto error

echo Linking...
%CXX% build\main.o build\App.o build\Config.o build\MonitorService.o build\TaskbarWidget.o build\OptionsWindow.o build\UiResources.o build\resource.o -o build\TaskbarMonitor.exe %LDFLAGS%
if errorlevel 1 goto error

echo ===== Build succeeded =====
echo Output: build\TaskbarMonitor.exe
goto end

:error
echo ===== Build failed =====
exit /b 1

:end
endlocal
