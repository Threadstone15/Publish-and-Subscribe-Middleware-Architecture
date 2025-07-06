@echo off
echo Compiling Publisher-Subscriber System...

echo Compiling server...
gcc server.c -o server -lws2_32
if %errorlevel% neq 0 (
    echo Failed to compile server
    pause
    exit /b 1
)

echo Compiling client...
gcc client.c -o client -lws2_32
if %errorlevel% neq 0 (
    echo Failed to compile client
    pause
    exit /b 1
)

echo.
echo All files compiled successfully!
echo.
echo Executables created:
echo   - server.exe
echo   - client.exe
echo.
echo To start the system:
echo   1. Start server: server.exe 5000
echo   2. Start client: client.exe 127.0.0.1 5000 
echo                        
echo.
pause
