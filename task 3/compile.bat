@echo off
echo Compiling Topic-Based Publisher-Subscriber System...

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
echo Example usage:
echo   1. Start server: server.exe 5000
echo   2. Start publisher for SPORTS: client.exe 127.0.0.1 5000 PUBLISHER SPORTS
echo.
echo Now publishers will only send messages to subscribers of the same topic!
echo.
pause
